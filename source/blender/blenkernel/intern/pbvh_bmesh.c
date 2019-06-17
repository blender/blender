/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_buffer.h"
#include "BLI_ghash.h"
#include "BLI_heap_simple.h"
#include "BLI_math.h"
#include "BLI_memarena.h"

#include "BKE_ccg.h"
#include "BKE_DerivedMesh.h"
#include "BKE_pbvh.h"

#include "GPU_buffers.h"

#include "bmesh.h"
#include "pbvh_intern.h"

#include <assert.h>

/* Avoid skinny faces */
#define USE_EDGEQUEUE_EVEN_SUBDIV
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
#  include "BKE_global.h"
#endif

/* Support for only operating on front-faces */
#define USE_EDGEQUEUE_FRONTFACE

/* don't add edges into the queue multiple times */
#define USE_EDGEQUEUE_TAG
/**
 * Ensure we don't have dirty tags for the edge queue, and that they are left cleared.
 * (slow, even for debug mode, so leave disabled for now).
 */
#if defined(USE_EDGEQUEUE_TAG) && 0
#  if !defined(NDEBUG)
#    define USE_EDGEQUEUE_TAG_VERIFY
#  endif
#endif

// #define USE_VERIFY

#ifdef USE_VERIFY
static void pbvh_bmesh_verify(PBVH *bvh);
#endif

/** \name BMesh Utility API
 *
 * Use some local functions which assume triangles.
 * \{ */

/**
 * Typically using BM_LOOPS_OF_VERT and BM_FACES_OF_VERT iterators are fine,
 * however this is an area where performance matters so do it in-line.
 *
 * Take care since 'break' won't works as expected within these macros!
 */

#define BM_LOOPS_OF_VERT_ITER_BEGIN(l_iter_radial_, v_) \
  { \
    struct { \
      BMVert *v; \
      BMEdge *e_iter, *e_first; \
      BMLoop *l_iter_radial; \
    } _iter; \
    _iter.v = v_; \
    if (_iter.v->e) { \
      _iter.e_iter = _iter.e_first = _iter.v->e; \
      do { \
        if (_iter.e_iter->l) { \
          _iter.l_iter_radial = _iter.e_iter->l; \
          do { \
            if (_iter.l_iter_radial->v == _iter.v) { \
              l_iter_radial_ = _iter.l_iter_radial;

#define BM_LOOPS_OF_VERT_ITER_END \
  } \
  } \
  while ((_iter.l_iter_radial = _iter.l_iter_radial->radial_next) != _iter.e_iter->l) \
    ; \
  } \
  } \
  while ((_iter.e_iter = BM_DISK_EDGE_NEXT(_iter.e_iter, _iter.v)) != _iter.e_first) \
    ; \
  } \
  } \
  ((void)0)

#define BM_FACES_OF_VERT_ITER_BEGIN(f_iter_, v_) \
  { \
    BMLoop *l_iter_radial_; \
    BM_LOOPS_OF_VERT_ITER_BEGIN (l_iter_radial_, v_) { \
      f_iter_ = l_iter_radial_->f;

#define BM_FACES_OF_VERT_ITER_END \
  } \
  BM_LOOPS_OF_VERT_ITER_END; \
  } \
  ((void)0)

static void bm_edges_from_tri(BMesh *bm, BMVert *v_tri[3], BMEdge *e_tri[3])
{
  e_tri[0] = BM_edge_create(bm, v_tri[0], v_tri[1], NULL, BM_CREATE_NO_DOUBLE);
  e_tri[1] = BM_edge_create(bm, v_tri[1], v_tri[2], NULL, BM_CREATE_NO_DOUBLE);
  e_tri[2] = BM_edge_create(bm, v_tri[2], v_tri[0], NULL, BM_CREATE_NO_DOUBLE);
}

BLI_INLINE void bm_face_as_array_index_tri(BMFace *f, int r_index[3])
{
  BMLoop *l = BM_FACE_FIRST_LOOP(f);

  BLI_assert(f->len == 3);

  r_index[0] = BM_elem_index_get(l->v);
  l = l->next;
  r_index[1] = BM_elem_index_get(l->v);
  l = l->next;
  r_index[2] = BM_elem_index_get(l->v);
}

/**
 * A version of #BM_face_exists, optimized for triangles
 * when we know the loop and the opposite vertex.
 *
 * Check if any triangle is formed by (l_radial_first->v, l_radial_first->next->v, v_opposite),
 * at either winding (since its a triangle no special checks are needed).
 *
 * <pre>
 * l_radial_first->v & l_radial_first->next->v
 * +---+
 * |  /
 * | /
 * + v_opposite
 * </pre>
 *
 * Its assumed that \a l_radial_first is never forming the target face.
 */
static BMFace *bm_face_exists_tri_from_loop_vert(BMLoop *l_radial_first, BMVert *v_opposite)
{
  BLI_assert(
      !ELEM(v_opposite, l_radial_first->v, l_radial_first->next->v, l_radial_first->prev->v));
  if (l_radial_first->radial_next != l_radial_first) {
    BMLoop *l_radial_iter = l_radial_first->radial_next;
    do {
      BLI_assert(l_radial_iter->f->len == 3);
      if (l_radial_iter->prev->v == v_opposite) {
        return l_radial_iter->f;
      }
    } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
  }
  return NULL;
}

/**
 * Uses a map of vertices to lookup the final target.
 * References can't point to previous items (would cause infinite loop).
 */
static BMVert *bm_vert_hash_lookup_chain(GHash *deleted_verts, BMVert *v)
{
  while (true) {
    BMVert **v_next_p = (BMVert **)BLI_ghash_lookup_p(deleted_verts, v);
    if (v_next_p == NULL) {
      /* not remapped*/
      return v;
    }
    else if (*v_next_p == NULL) {
      /* removed and not remapped */
      return NULL;
    }
    else {
      /* remapped */
      v = *v_next_p;
    }
  }
}

/** \} */

/****************************** Building ******************************/

/* Update node data after splitting */
static void pbvh_bmesh_node_finalize(PBVH *bvh,
                                     const int node_index,
                                     const int cd_vert_node_offset,
                                     const int cd_face_node_offset)
{
  GSetIterator gs_iter;
  PBVHNode *n = &bvh->nodes[node_index];
  bool has_visible = false;

  /* Create vert hash sets */
  n->bm_unique_verts = BLI_gset_ptr_new("bm_unique_verts");
  n->bm_other_verts = BLI_gset_ptr_new("bm_other_verts");

  BB_reset(&n->vb);

  GSET_ITER (gs_iter, n->bm_faces) {
    BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

    /* Update ownership of faces */
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    do {
      BMVert *v = l_iter->v;
      if (!BLI_gset_haskey(n->bm_unique_verts, v)) {
        if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
          BLI_gset_add(n->bm_other_verts, v);
        }
        else {
          BLI_gset_insert(n->bm_unique_verts, v);
          BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, node_index);
        }
      }
      /* Update node bounding box */
      BB_expand(&n->vb, v->co);
    } while ((l_iter = l_iter->next) != l_first);

    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      has_visible = true;
    }
  }

  BLI_assert(n->vb.bmin[0] <= n->vb.bmax[0] && n->vb.bmin[1] <= n->vb.bmax[1] &&
             n->vb.bmin[2] <= n->vb.bmax[2]);

  n->orig_vb = n->vb;

  /* Build GPU buffers for new node and update vertex normals */
  BKE_pbvh_node_mark_rebuild_draw(n);

  BKE_pbvh_node_fully_hidden_set(n, !has_visible);
  n->flag |= PBVH_UpdateNormals;
}

/* Recursively split the node if it exceeds the leaf_limit */
static void pbvh_bmesh_node_split(PBVH *bvh, const BBC *bbc_array, int node_index)
{
  const int cd_vert_node_offset = bvh->cd_vert_node_offset;
  const int cd_face_node_offset = bvh->cd_face_node_offset;
  PBVHNode *n = &bvh->nodes[node_index];

  if (BLI_gset_len(n->bm_faces) <= bvh->leaf_limit) {
    /* Node limit not exceeded */
    pbvh_bmesh_node_finalize(bvh, node_index, cd_vert_node_offset, cd_face_node_offset);
    return;
  }

  /* Calculate bounding box around primitive centroids */
  BB cb;
  BB_reset(&cb);
  GSetIterator gs_iter;
  GSET_ITER (gs_iter, n->bm_faces) {
    const BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
    const BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    BB_expand(&cb, bbc->bcentroid);
  }

  /* Find widest axis and its midpoint */
  const int axis = BB_widest_axis(&cb);
  const float mid = (cb.bmax[axis] + cb.bmin[axis]) * 0.5f;

  /* Add two new child nodes */
  const int children = bvh->totnode;
  n->children_offset = children;
  pbvh_grow_nodes(bvh, bvh->totnode + 2);

  /* Array reallocated, update current node pointer */
  n = &bvh->nodes[node_index];

  /* Initialize children */
  PBVHNode *c1 = &bvh->nodes[children], *c2 = &bvh->nodes[children + 1];
  c1->flag |= PBVH_Leaf;
  c2->flag |= PBVH_Leaf;
  c1->bm_faces = BLI_gset_ptr_new_ex("bm_faces", BLI_gset_len(n->bm_faces) / 2);
  c2->bm_faces = BLI_gset_ptr_new_ex("bm_faces", BLI_gset_len(n->bm_faces) / 2);

  /* Partition the parent node's faces between the two children */
  GSET_ITER (gs_iter, n->bm_faces) {
    BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
    const BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    if (bbc->bcentroid[axis] < mid) {
      BLI_gset_insert(c1->bm_faces, f);
    }
    else {
      BLI_gset_insert(c2->bm_faces, f);
    }
  }

  /* Enforce at least one primitive in each node */
  GSet *empty = NULL, *other;
  if (BLI_gset_len(c1->bm_faces) == 0) {
    empty = c1->bm_faces;
    other = c2->bm_faces;
  }
  else if (BLI_gset_len(c2->bm_faces) == 0) {
    empty = c2->bm_faces;
    other = c1->bm_faces;
  }
  if (empty) {
    GSET_ITER (gs_iter, other) {
      void *key = BLI_gsetIterator_getKey(&gs_iter);
      BLI_gset_insert(empty, key);
      BLI_gset_remove(other, key, NULL);
      break;
    }
  }

  /* Clear this node */

  /* Mark this node's unique verts as unclaimed */
  if (n->bm_unique_verts) {
    GSET_ITER (gs_iter, n->bm_unique_verts) {
      BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
      BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, DYNTOPO_NODE_NONE);
    }
    BLI_gset_free(n->bm_unique_verts, NULL);
  }

  /* Unclaim faces */
  GSET_ITER (gs_iter, n->bm_faces) {
    BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
  }
  BLI_gset_free(n->bm_faces, NULL);

  if (n->bm_other_verts) {
    BLI_gset_free(n->bm_other_verts, NULL);
  }

  if (n->layer_disp) {
    MEM_freeN(n->layer_disp);
  }

  n->bm_faces = NULL;
  n->bm_unique_verts = NULL;
  n->bm_other_verts = NULL;
  n->layer_disp = NULL;

  if (n->draw_buffers) {
    GPU_pbvh_buffers_free(n->draw_buffers);
    n->draw_buffers = NULL;
  }
  n->flag &= ~PBVH_Leaf;

  /* Recurse */
  pbvh_bmesh_node_split(bvh, bbc_array, children);
  pbvh_bmesh_node_split(bvh, bbc_array, children + 1);

  /* Array maybe reallocated, update current node pointer */
  n = &bvh->nodes[node_index];

  /* Update bounding box */
  BB_reset(&n->vb);
  BB_expand_with_bb(&n->vb, &bvh->nodes[n->children_offset].vb);
  BB_expand_with_bb(&n->vb, &bvh->nodes[n->children_offset + 1].vb);
  n->orig_vb = n->vb;
}

/* Recursively split the node if it exceeds the leaf_limit */
static bool pbvh_bmesh_node_limit_ensure(PBVH *bvh, int node_index)
{
  GSet *bm_faces = bvh->nodes[node_index].bm_faces;
  const int bm_faces_size = BLI_gset_len(bm_faces);
  if (bm_faces_size <= bvh->leaf_limit) {
    /* Node limit not exceeded */
    return false;
  }

  /* For each BMFace, store the AABB and AABB centroid */
  BBC *bbc_array = MEM_mallocN(sizeof(BBC) * bm_faces_size, "BBC");

  GSetIterator gs_iter;
  int i;
  GSET_ITER_INDEX (gs_iter, bm_faces, i) {
    BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
    BBC *bbc = &bbc_array[i];

    BB_reset((BB *)bbc);
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BB_expand((BB *)bbc, l_iter->v->co);
    } while ((l_iter = l_iter->next) != l_first);
    BBC_update_centroid(bbc);

    /* so we can do direct lookups on 'bbc_array' */
    BM_elem_index_set(f, i); /* set_dirty! */
  }
  /* likely this is already dirty */
  bvh->bm->elem_index_dirty |= BM_FACE;

  pbvh_bmesh_node_split(bvh, bbc_array, node_index);

  MEM_freeN(bbc_array);

  return true;
}

/**********************************************************************/

#if 0
static int pbvh_bmesh_node_offset_from_elem(PBVH *bvh, BMElem *ele)
{
  switch (ele->head.htype) {
    case BM_VERT:
      return bvh->cd_vert_node_offset;
    default:
      BLI_assert(ele->head.htype == BM_FACE);
      return bvh->cd_face_node_offset;
  }
}

static int pbvh_bmesh_node_index_from_elem(PBVH *bvh, void *key)
{
  const int cd_node_offset = pbvh_bmesh_node_offset_from_elem(bvh, key);
  const int node_index = BM_ELEM_CD_GET_INT((BMElem *)key, cd_node_offset);

  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  BLI_assert(node_index < bvh->totnode);
  (void)bvh;

  return node_index;
}

static PBVHNode *pbvh_bmesh_node_from_elem(PBVH *bvh, void *key)
{
  return &bvh->nodes[pbvh_bmesh_node_index_from_elem(bvh, key)];
}

/* typecheck */
#  define pbvh_bmesh_node_index_from_elem(bvh, key) \
    (CHECK_TYPE_ANY(key, BMFace *, BMVert *), pbvh_bmesh_node_index_from_elem(bvh, key))
#  define pbvh_bmesh_node_from_elem(bvh, key) \
    (CHECK_TYPE_ANY(key, BMFace *, BMVert *), pbvh_bmesh_node_from_elem(bvh, key))
#endif

BLI_INLINE int pbvh_bmesh_node_index_from_vert(PBVH *bvh, const BMVert *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, bvh->cd_vert_node_offset);
  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  BLI_assert(node_index < bvh->totnode);
  return node_index;
}

BLI_INLINE int pbvh_bmesh_node_index_from_face(PBVH *bvh, const BMFace *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, bvh->cd_face_node_offset);
  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  BLI_assert(node_index < bvh->totnode);
  return node_index;
}

BLI_INLINE PBVHNode *pbvh_bmesh_node_from_vert(PBVH *bvh, const BMVert *key)
{
  return &bvh->nodes[pbvh_bmesh_node_index_from_vert(bvh, key)];
}

BLI_INLINE PBVHNode *pbvh_bmesh_node_from_face(PBVH *bvh, const BMFace *key)
{
  return &bvh->nodes[pbvh_bmesh_node_index_from_face(bvh, key)];
}

static BMVert *pbvh_bmesh_vert_create(
    PBVH *bvh, int node_index, const float co[3], const float no[3], const int cd_vert_mask_offset)
{
  PBVHNode *node = &bvh->nodes[node_index];

  BLI_assert((bvh->totnode == 1 || node_index) && node_index <= bvh->totnode);

  /* avoid initializing customdata because its quite involved */
  BMVert *v = BM_vert_create(bvh->bm, co, NULL, BM_CREATE_SKIP_CD);
  CustomData_bmesh_set_default(&bvh->bm->vdata, &v->head.data);

  /* This value is logged below */
  copy_v3_v3(v->no, no);

  BLI_gset_insert(node->bm_unique_verts, v);
  BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, node_index);

  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;

  /* Log the new vertex */
  BM_log_vert_added(bvh->bm_log, v, cd_vert_mask_offset);

  return v;
}

/**
 * \note Callers are responsible for checking if the face exists before adding.
 */
static BMFace *pbvh_bmesh_face_create(
    PBVH *bvh, int node_index, BMVert *v_tri[3], BMEdge *e_tri[3], const BMFace *f_example)
{
  PBVHNode *node = &bvh->nodes[node_index];

  /* ensure we never add existing face */
  BLI_assert(!BM_face_exists(v_tri, 3));

  BMFace *f = BM_face_create(bvh->bm, v_tri, e_tri, 3, f_example, BM_CREATE_NOP);
  f->head.hflag = f_example->head.hflag;

  BLI_gset_insert(node->bm_faces, f);
  BM_ELEM_CD_SET_INT(f, bvh->cd_face_node_offset, node_index);

  /* mark node for update */
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals;
  node->flag &= ~PBVH_FullyHidden;

  /* Log the new face */
  BM_log_face_added(bvh->bm_log, f);

  return f;
}

/* Return the number of faces in 'node' that use vertex 'v' */
#if 0
static int pbvh_bmesh_node_vert_use_count(PBVH *bvh, PBVHNode *node, BMVert *v)
{
  BMFace *f;
  int count = 0;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(bvh, f);
    if (f_node == node) {
      count++;
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return count;
}
#endif

#define pbvh_bmesh_node_vert_use_count_is_equal(bvh, node, v, n) \
  (pbvh_bmesh_node_vert_use_count_at_most(bvh, node, v, (n) + 1) == n)

static int pbvh_bmesh_node_vert_use_count_at_most(PBVH *bvh,
                                                  PBVHNode *node,
                                                  BMVert *v,
                                                  const int count_max)
{
  int count = 0;
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(bvh, f);
    if (f_node == node) {
      count++;
      if (count == count_max) {
        return count;
      }
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return count;
}

/* Return a node that uses vertex 'v' other than its current owner */
static PBVHNode *pbvh_bmesh_vert_other_node_find(PBVH *bvh, BMVert *v)
{
  PBVHNode *current_node = pbvh_bmesh_node_from_vert(bvh, v);
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(bvh, f);

    if (f_node != current_node) {
      return f_node;
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return NULL;
}

static void pbvh_bmesh_vert_ownership_transfer(PBVH *bvh, PBVHNode *new_owner, BMVert *v)
{
  PBVHNode *current_owner = pbvh_bmesh_node_from_vert(bvh, v);
  /* mark node for update */
  current_owner->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;

  BLI_assert(current_owner != new_owner);

  /* Remove current ownership */
  BLI_gset_remove(current_owner->bm_unique_verts, v, NULL);

  /* Set new ownership */
  BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, new_owner - bvh->nodes);
  BLI_gset_insert(new_owner->bm_unique_verts, v);
  BLI_gset_remove(new_owner->bm_other_verts, v, NULL);
  BLI_assert(!BLI_gset_haskey(new_owner->bm_other_verts, v));

  /* mark node for update */
  new_owner->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;
}

static void pbvh_bmesh_vert_remove(PBVH *bvh, BMVert *v)
{
  /* never match for first time */
  int f_node_index_prev = DYNTOPO_NODE_NONE;

  PBVHNode *v_node = pbvh_bmesh_node_from_vert(bvh, v);
  BLI_gset_remove(v_node->bm_unique_verts, v, NULL);
  BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

  /* Have to check each neighboring face's node */
  BMFace *f;
  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    const int f_node_index = pbvh_bmesh_node_index_from_face(bvh, f);

    /* faces often share the same node,
     * quick check to avoid redundant #BLI_gset_remove calls */
    if (f_node_index_prev != f_node_index) {
      f_node_index_prev = f_node_index;

      PBVHNode *f_node = &bvh->nodes[f_node_index];
      f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;

      /* Remove current ownership */
      BLI_gset_remove(f_node->bm_other_verts, v, NULL);

      BLI_assert(!BLI_gset_haskey(f_node->bm_unique_verts, v));
      BLI_assert(!BLI_gset_haskey(f_node->bm_other_verts, v));
    }
  }
  BM_FACES_OF_VERT_ITER_END;
}

static void pbvh_bmesh_face_remove(PBVH *bvh, BMFace *f)
{
  PBVHNode *f_node = pbvh_bmesh_node_from_face(bvh, f);

  /* Check if any of this face's vertices need to be removed
   * from the node */
  BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *l_iter = l_first;
  do {
    BMVert *v = l_iter->v;
    if (pbvh_bmesh_node_vert_use_count_is_equal(bvh, f_node, v, 1)) {
      if (BLI_gset_haskey(f_node->bm_unique_verts, v)) {
        /* Find a different node that uses 'v' */
        PBVHNode *new_node;

        new_node = pbvh_bmesh_vert_other_node_find(bvh, v);
        BLI_assert(new_node || BM_vert_face_count_is_equal(v, 1));

        if (new_node) {
          pbvh_bmesh_vert_ownership_transfer(bvh, new_node, v);
        }
      }
      else {
        /* Remove from other verts */
        BLI_gset_remove(f_node->bm_other_verts, v, NULL);
      }
    }
  } while ((l_iter = l_iter->next) != l_first);

  /* Remove face from node and top level */
  BLI_gset_remove(f_node->bm_faces, f, NULL);
  BM_ELEM_CD_SET_INT(f, bvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

  /* Log removed face */
  BM_log_face_removed(bvh->bm_log, f);

  /* mark node for update */
  f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals;
}

static void pbvh_bmesh_edge_loops(BLI_Buffer *buf, BMEdge *e)
{
  /* fast-path for most common case where an edge has 2 faces,
   * no need to iterate twice.
   * This assumes that the buffer */
  BMLoop **data = buf->data;
  BLI_assert(buf->alloc_count >= 2);
  if (LIKELY(BM_edge_loop_pair(e, &data[0], &data[1]))) {
    buf->count = 2;
  }
  else {
    BLI_buffer_reinit(buf, BM_edge_face_count(e));
    BM_iter_as_array(NULL, BM_LOOPS_OF_EDGE, e, buf->data, buf->count);
  }
}

static void pbvh_bmesh_node_drop_orig(PBVHNode *node)
{
  if (node->bm_orco) {
    MEM_freeN(node->bm_orco);
  }
  if (node->bm_ortri) {
    MEM_freeN(node->bm_ortri);
  }
  node->bm_orco = NULL;
  node->bm_ortri = NULL;
  node->bm_tot_ortri = 0;
}

/****************************** EdgeQueue *****************************/

struct EdgeQueue;

typedef struct EdgeQueue {
  HeapSimple *heap;
  const float *center;
  float center_proj[3]; /* for when we use projected coords. */
  float radius_squared;
  float limit_len_squared;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  float limit_len;
#endif

  bool (*edge_queue_tri_in_range)(const struct EdgeQueue *q, BMFace *f);

  const float *view_normal;
#ifdef USE_EDGEQUEUE_FRONTFACE
  unsigned int use_view_normal : 1;
#endif
} EdgeQueue;

typedef struct {
  EdgeQueue *q;
  BLI_mempool *pool;
  BMesh *bm;
  int cd_vert_mask_offset;
  int cd_vert_node_offset;
  int cd_face_node_offset;
} EdgeQueueContext;

/* only tag'd edges are in the queue */
#ifdef USE_EDGEQUEUE_TAG
#  define EDGE_QUEUE_TEST(e) (BM_elem_flag_test((CHECK_TYPE_INLINE(e, BMEdge *), e), BM_ELEM_TAG))
#  define EDGE_QUEUE_ENABLE(e) \
    BM_elem_flag_enable((CHECK_TYPE_INLINE(e, BMEdge *), e), BM_ELEM_TAG)
#  define EDGE_QUEUE_DISABLE(e) \
    BM_elem_flag_disable((CHECK_TYPE_INLINE(e, BMEdge *), e), BM_ELEM_TAG)
#endif

#ifdef USE_EDGEQUEUE_TAG_VERIFY
/* simply check no edges are tagged
 * (it's a requirement that edges enter and leave a clean tag state) */
static void pbvh_bmesh_edge_tag_verify(PBVH *bvh)
{
  for (int n = 0; n < bvh->totnode; n++) {
    PBVHNode *node = &bvh->nodes[n];
    if (node->bm_faces) {
      GSetIterator gs_iter;
      GSET_ITER (gs_iter, node->bm_faces) {
        BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
        BMEdge *e_tri[3];
        BMLoop *l_iter;

        BLI_assert(f->len == 3);
        l_iter = BM_FACE_FIRST_LOOP(f);
        e_tri[0] = l_iter->e;
        l_iter = l_iter->next;
        e_tri[1] = l_iter->e;
        l_iter = l_iter->next;
        e_tri[2] = l_iter->e;

        BLI_assert((EDGE_QUEUE_TEST(e_tri[0]) == false) && (EDGE_QUEUE_TEST(e_tri[1]) == false) &&
                   (EDGE_QUEUE_TEST(e_tri[2]) == false));
      }
    }
  }
}
#endif

static bool edge_queue_tri_in_sphere(const EdgeQueue *q, BMFace *f)
{
  BMVert *v_tri[3];
  float c[3];

  /* Get closest point in triangle to sphere center */
  BM_face_as_array_vert_tri(f, v_tri);

  closest_on_tri_to_point_v3(c, q->center, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co);

  /* Check if triangle intersects the sphere */
  return len_squared_v3v3(q->center, c) <= q->radius_squared;
}

static bool edge_queue_tri_in_circle(const EdgeQueue *q, BMFace *f)
{
  BMVert *v_tri[3];
  float c[3];
  float tri_proj[3][3];

  /* Get closest point in triangle to sphere center */
  BM_face_as_array_vert_tri(f, v_tri);

  project_plane_normalized_v3_v3v3(tri_proj[0], v_tri[0]->co, q->view_normal);
  project_plane_normalized_v3_v3v3(tri_proj[1], v_tri[1]->co, q->view_normal);
  project_plane_normalized_v3_v3v3(tri_proj[2], v_tri[2]->co, q->view_normal);

  closest_on_tri_to_point_v3(c, q->center_proj, tri_proj[0], tri_proj[1], tri_proj[2]);

  /* Check if triangle intersects the sphere */
  return len_squared_v3v3(q->center_proj, c) <= q->radius_squared;
}

/* Return true if the vertex mask is less than 1.0, false otherwise */
static bool check_mask(EdgeQueueContext *eq_ctx, BMVert *v)
{
  return BM_ELEM_CD_GET_FLOAT(v, eq_ctx->cd_vert_mask_offset) < 1.0f;
}

static void edge_queue_insert(EdgeQueueContext *eq_ctx, BMEdge *e, float priority)
{
  /* Don't let topology update affect fully masked vertices. This used to
   * have a 50% mask cutoff, with the reasoning that you can't do a 50%
   * topology update. But this gives an ugly border in the mesh. The mask
   * should already make the brush move the vertices only 50%, which means
   * that topology updates will also happen less frequent, that should be
   * enough. */
  if (((eq_ctx->cd_vert_mask_offset == -1) ||
       (check_mask(eq_ctx, e->v1) || check_mask(eq_ctx, e->v2))) &&
      !(BM_elem_flag_test_bool(e->v1, BM_ELEM_HIDDEN) ||
        BM_elem_flag_test_bool(e->v2, BM_ELEM_HIDDEN))) {
    BMVert **pair = BLI_mempool_alloc(eq_ctx->pool);
    pair[0] = e->v1;
    pair[1] = e->v2;
    BLI_heapsimple_insert(eq_ctx->q->heap, priority, pair);
#ifdef USE_EDGEQUEUE_TAG
    BLI_assert(EDGE_QUEUE_TEST(e) == false);
    EDGE_QUEUE_ENABLE(e);
#endif
  }
}

static void long_edge_queue_edge_add(EdgeQueueContext *eq_ctx, BMEdge *e)
{
#ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(e) == false)
#endif
  {
    const float len_sq = BM_edge_calc_length_squared(e);
    if (len_sq > eq_ctx->q->limit_len_squared) {
      edge_queue_insert(eq_ctx, e, -len_sq);
    }
  }
}

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
static void long_edge_queue_edge_add_recursive(
    EdgeQueueContext *eq_ctx, BMLoop *l_edge, BMLoop *l_end, const float len_sq, float limit_len)
{
  BLI_assert(len_sq > SQUARE(limit_len));

#  ifdef USE_EDGEQUEUE_FRONTFACE
  if (eq_ctx->q->use_view_normal) {
    if (dot_v3v3(l_edge->f->no, eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#  endif

#  ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(l_edge->e) == false)
#  endif
  {
    edge_queue_insert(eq_ctx, l_edge->e, -len_sq);
  }

  /* temp support previous behavior! */
  if (UNLIKELY(G.debug_value == 1234)) {
    return;
  }

  if ((l_edge->radial_next != l_edge)) {
    /* how much longer we need to be to consider for subdividing
     * (avoids subdividing faces which are only *slightly* skinny) */
#  define EVEN_EDGELEN_THRESHOLD 1.2f
    /* how much the limit increases per recursion
     * (avoids performing subdvisions too far away) */
#  define EVEN_GENERATION_SCALE 1.6f

    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = SQUARE(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < ARRAY_SIZE(l_adjacent); i++) {
        float len_sq_other = BM_edge_calc_length_squared(l_adjacent[i]->e);
        if (len_sq_other > max_ff(len_sq_cmp, limit_len_sq)) {
          //                  edge_queue_insert(eq_ctx, l_adjacent[i]->e, -len_sq_other);
          long_edge_queue_edge_add_recursive(
              eq_ctx, l_adjacent[i]->radial_next, l_adjacent[i], len_sq_other, limit_len);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_end);

#  undef EVEN_EDGELEN_THRESHOLD
#  undef EVEN_GENERATION_SCALE
  }
}
#endif /* USE_EDGEQUEUE_EVEN_SUBDIV */

static void short_edge_queue_edge_add(EdgeQueueContext *eq_ctx, BMEdge *e)
{
#ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(e) == false)
#endif
  {
    const float len_sq = BM_edge_calc_length_squared(e);
    if (len_sq < eq_ctx->q->limit_len_squared) {
      edge_queue_insert(eq_ctx, e, len_sq);
    }
  }
}

static void long_edge_queue_face_add(EdgeQueueContext *eq_ctx, BMFace *f)
{
#ifdef USE_EDGEQUEUE_FRONTFACE
  if (eq_ctx->q->use_view_normal) {
    if (dot_v3v3(f->no, eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  if (eq_ctx->q->edge_queue_tri_in_range(eq_ctx->q, f)) {
    /* Check each edge of the face */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
      const float len_sq = BM_edge_calc_length_squared(l_iter->e);
      if (len_sq > eq_ctx->q->limit_len_squared) {
        long_edge_queue_edge_add_recursive(
            eq_ctx, l_iter->radial_next, l_iter, len_sq, eq_ctx->q->limit_len);
      }
#else
      long_edge_queue_edge_add(eq_ctx, l_iter->e);
#endif
    } while ((l_iter = l_iter->next) != l_first);
  }
}

static void short_edge_queue_face_add(EdgeQueueContext *eq_ctx, BMFace *f)
{
#ifdef USE_EDGEQUEUE_FRONTFACE
  if (eq_ctx->q->use_view_normal) {
    if (dot_v3v3(f->no, eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  if (eq_ctx->q->edge_queue_tri_in_range(eq_ctx->q, f)) {
    BMLoop *l_iter;
    BMLoop *l_first;

    /* Check each edge of the face */
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      short_edge_queue_edge_add(eq_ctx, l_iter->e);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

/* Create a priority queue containing vertex pairs connected by a long
 * edge as defined by PBVH.bm_max_edge_len.
 *
 * Only nodes marked for topology update are checked, and in those
 * nodes only edges used by a face intersecting the (center, radius)
 * sphere are checked.
 *
 * The highest priority (lowest number) is given to the longest edge.
 */
static void long_edge_queue_create(EdgeQueueContext *eq_ctx,
                                   PBVH *bvh,
                                   const float center[3],
                                   const float view_normal[3],
                                   float radius,
                                   const bool use_frontface,
                                   const bool use_projected)
{
  eq_ctx->q->heap = BLI_heapsimple_new();
  eq_ctx->q->center = center;
  eq_ctx->q->radius_squared = radius * radius;
  eq_ctx->q->limit_len_squared = bvh->bm_max_edge_len * bvh->bm_max_edge_len;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  eq_ctx->q->limit_len = bvh->bm_max_edge_len;
#endif

  eq_ctx->q->view_normal = view_normal;

#ifdef USE_EDGEQUEUE_FRONTFACE
  eq_ctx->q->use_view_normal = use_frontface;
#else
  UNUSED_VARS(use_frontface);
#endif

  if (use_projected) {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_circle;
    project_plane_normalized_v3_v3v3(eq_ctx->q->center_proj, center, view_normal);
  }
  else {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_sphere;
  }

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(bvh);
#endif

  for (int n = 0; n < bvh->totnode; n++) {
    PBVHNode *node = &bvh->nodes[n];

    /* Check leaf nodes marked for topology update */
    if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
        !(node->flag & PBVH_FullyHidden)) {
      GSetIterator gs_iter;

      /* Check each face */
      GSET_ITER (gs_iter, node->bm_faces) {
        BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

        long_edge_queue_face_add(eq_ctx, f);
      }
    }
  }
}

/* Create a priority queue containing vertex pairs connected by a
 * short edge as defined by PBVH.bm_min_edge_len.
 *
 * Only nodes marked for topology update are checked, and in those
 * nodes only edges used by a face intersecting the (center, radius)
 * sphere are checked.
 *
 * The highest priority (lowest number) is given to the shortest edge.
 */
static void short_edge_queue_create(EdgeQueueContext *eq_ctx,
                                    PBVH *bvh,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    const bool use_frontface,
                                    const bool use_projected)
{
  eq_ctx->q->heap = BLI_heapsimple_new();
  eq_ctx->q->center = center;
  eq_ctx->q->radius_squared = radius * radius;
  eq_ctx->q->limit_len_squared = bvh->bm_min_edge_len * bvh->bm_min_edge_len;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  eq_ctx->q->limit_len = bvh->bm_min_edge_len;
#endif

  eq_ctx->q->view_normal = view_normal;

#ifdef USE_EDGEQUEUE_FRONTFACE
  eq_ctx->q->use_view_normal = use_frontface;
#else
  UNUSED_VARS(use_frontface);
#endif

  if (use_projected) {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_circle;
    project_plane_normalized_v3_v3v3(eq_ctx->q->center_proj, center, view_normal);
  }
  else {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_sphere;
  }

  for (int n = 0; n < bvh->totnode; n++) {
    PBVHNode *node = &bvh->nodes[n];

    /* Check leaf nodes marked for topology update */
    if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
        !(node->flag & PBVH_FullyHidden)) {
      GSetIterator gs_iter;

      /* Check each face */
      GSET_ITER (gs_iter, node->bm_faces) {
        BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

        short_edge_queue_face_add(eq_ctx, f);
      }
    }
  }
}

/*************************** Topology update **************************/

static void pbvh_bmesh_split_edge(EdgeQueueContext *eq_ctx,
                                  PBVH *bvh,
                                  BMEdge *e,
                                  BLI_Buffer *edge_loops)
{
  float co_mid[3], no_mid[3];

  /* Get all faces adjacent to the edge */
  pbvh_bmesh_edge_loops(edge_loops, e);

  /* Create a new vertex in current node at the edge's midpoint */
  mid_v3_v3v3(co_mid, e->v1->co, e->v2->co);
  mid_v3_v3v3(no_mid, e->v1->no, e->v2->no);
  normalize_v3(no_mid);

  int node_index = BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset);
  BMVert *v_new = pbvh_bmesh_vert_create(
      bvh, node_index, co_mid, no_mid, eq_ctx->cd_vert_mask_offset);

  /* update paint mask */
  if (eq_ctx->cd_vert_mask_offset != -1) {
    float mask_v1 = BM_ELEM_CD_GET_FLOAT(e->v1, eq_ctx->cd_vert_mask_offset);
    float mask_v2 = BM_ELEM_CD_GET_FLOAT(e->v2, eq_ctx->cd_vert_mask_offset);
    float mask_v_new = 0.5f * (mask_v1 + mask_v2);

    BM_ELEM_CD_SET_FLOAT(v_new, eq_ctx->cd_vert_mask_offset, mask_v_new);
  }

  /* For each face, add two new triangles and delete the original */
  for (int i = 0; i < edge_loops->count; i++) {
    BMLoop *l_adj = BLI_buffer_at(edge_loops, BMLoop *, i);
    BMFace *f_adj = l_adj->f;
    BMFace *f_new;
    BMVert *v_opp, *v1, *v2;
    BMVert *v_tri[3];
    BMEdge *e_tri[3];

    BLI_assert(f_adj->len == 3);
    int ni = BM_ELEM_CD_GET_INT(f_adj, eq_ctx->cd_face_node_offset);

    /* Find the vertex not in the edge */
    v_opp = l_adj->prev->v;

    /* Get e->v1 and e->v2 in the order they appear in the
     * existing face so that the new faces' winding orders
     * match */
    v1 = l_adj->v;
    v2 = l_adj->next->v;

    if (ni != node_index && i == 0) {
      pbvh_bmesh_vert_ownership_transfer(bvh, &bvh->nodes[ni], v_new);
    }

    /**
     * The 2 new faces created and assigned to ``f_new`` have their
     * verts & edges shuffled around.
     *
     * - faces wind anticlockwise in this example.
     * - original edge is ``(v1, v2)``
     * - original face is ``(v1, v2, v3)``
     *
     * <pre>
     *         + v3(v_opp)
     *        /|\
     *       / | \
     *      /  |  \
     *   e4/   |   \ e3
     *    /    |e5  \
     *   /     |     \
     *  /  e1  |  e2  \
     * +-------+-------+
     * v1      v4(v_new) v2
     *  (first) (second)
     * </pre>
     *
     * - f_new (first):  ``v_tri=(v1, v4, v3), e_tri=(e1, e5, e4)``
     * - f_new (second): ``v_tri=(v4, v2, v3), e_tri=(e2, e3, e5)``
     */

    /* Create two new faces */
    v_tri[0] = v1;
    v_tri[1] = v_new;
    v_tri[2] = v_opp;
    bm_edges_from_tri(bvh->bm, v_tri, e_tri);
    f_new = pbvh_bmesh_face_create(bvh, ni, v_tri, e_tri, f_adj);
    long_edge_queue_face_add(eq_ctx, f_new);

    v_tri[0] = v_new;
    v_tri[1] = v2;
    /* v_tri[2] = v_opp; */ /* unchanged */
    e_tri[0] = BM_edge_create(bvh->bm, v_tri[0], v_tri[1], NULL, BM_CREATE_NO_DOUBLE);
    e_tri[2] = e_tri[1]; /* switched */
    e_tri[1] = BM_edge_create(bvh->bm, v_tri[1], v_tri[2], NULL, BM_CREATE_NO_DOUBLE);
    f_new = pbvh_bmesh_face_create(bvh, ni, v_tri, e_tri, f_adj);
    long_edge_queue_face_add(eq_ctx, f_new);

    /* Delete original */
    pbvh_bmesh_face_remove(bvh, f_adj);
    BM_face_kill(bvh->bm, f_adj);

    /* Ensure new vertex is in the node */
    if (!BLI_gset_haskey(bvh->nodes[ni].bm_unique_verts, v_new)) {
      BLI_gset_add(bvh->nodes[ni].bm_other_verts, v_new);
    }

    if (BM_vert_edge_count_is_over(v_opp, 8)) {
      BMIter bm_iter;
      BMEdge *e2;

      BM_ITER_ELEM (e2, &bm_iter, v_opp, BM_EDGES_OF_VERT) {
        long_edge_queue_edge_add(eq_ctx, e2);
      }
    }
  }

  BM_edge_kill(bvh->bm, e);
}

static bool pbvh_bmesh_subdivide_long_edges(EdgeQueueContext *eq_ctx,
                                            PBVH *bvh,
                                            BLI_Buffer *edge_loops)
{
  bool any_subdivided = false;

  while (!BLI_heapsimple_is_empty(eq_ctx->q->heap)) {
    BMVert **pair = BLI_heapsimple_pop_min(eq_ctx->q->heap);
    BMVert *v1 = pair[0], *v2 = pair[1];
    BMEdge *e;

    BLI_mempool_free(eq_ctx->pool, pair);
    pair = NULL;

    /* Check that the edge still exists */
    if (!(e = BM_edge_exists(v1, v2))) {
      continue;
    }
#ifdef USE_EDGEQUEUE_TAG
    EDGE_QUEUE_DISABLE(e);
#endif

    /* At the moment edges never get shorter (subdiv will make new edges)
     * unlike collapse where edges can become longer. */
#if 0
    if (len_squared_v3v3(v1->co, v2->co) <= eq_ctx->q->limit_len_squared) {
      continue;
    }
#else
    BLI_assert(len_squared_v3v3(v1->co, v2->co) > eq_ctx->q->limit_len_squared);
#endif

    /* Check that the edge's vertices are still in the PBVH. It's
     * possible that an edge collapse has deleted adjacent faces
     * and the node has been split, thus leaving wire edges and
     * associated vertices. */
    if ((BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE) ||
        (BM_ELEM_CD_GET_INT(e->v2, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE)) {
      continue;
    }

    any_subdivided = true;

    pbvh_bmesh_split_edge(eq_ctx, bvh, e, edge_loops);
  }

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(bvh);
#endif

  return any_subdivided;
}

static void pbvh_bmesh_collapse_edge(PBVH *bvh,
                                     BMEdge *e,
                                     BMVert *v1,
                                     BMVert *v2,
                                     GHash *deleted_verts,
                                     BLI_Buffer *deleted_faces,
                                     EdgeQueueContext *eq_ctx)
{
  BMVert *v_del, *v_conn;

  /* one of the two vertices may be masked, select the correct one for deletion */
  if (BM_ELEM_CD_GET_FLOAT(v1, eq_ctx->cd_vert_mask_offset) <
      BM_ELEM_CD_GET_FLOAT(v2, eq_ctx->cd_vert_mask_offset)) {
    v_del = v1;
    v_conn = v2;
  }
  else {
    v_del = v2;
    v_conn = v1;
  }

  /* Remove the merge vertex from the PBVH */
  pbvh_bmesh_vert_remove(bvh, v_del);

  /* Remove all faces adjacent to the edge */
  BMLoop *l_adj;
  while ((l_adj = e->l)) {
    BMFace *f_adj = l_adj->f;

    pbvh_bmesh_face_remove(bvh, f_adj);
    BM_face_kill(bvh->bm, f_adj);
  }

  /* Kill the edge */
  BLI_assert(BM_edge_is_wire(e));
  BM_edge_kill(bvh->bm, e);

  /* For all remaining faces of v_del, create a new face that is the
   * same except it uses v_conn instead of v_del */
  /* Note: this could be done with BM_vert_splice(), but that
   * requires handling other issues like duplicate edges, so doesn't
   * really buy anything. */
  BLI_buffer_clear(deleted_faces);

  BMLoop *l;

  BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_del) {
    BMFace *existing_face;

    /* Get vertices, replace use of v_del with v_conn */
    // BM_iter_as_array(NULL, BM_VERTS_OF_FACE, f, (void **)v_tri, 3);
    BMFace *f = l->f;
#if 0
    BMVert *v_tri[3];
    BM_face_as_array_vert_tri(f, v_tri);
    for (int i = 0; i < 3; i++) {
      if (v_tri[i] == v_del) {
        v_tri[i] = v_conn;
      }
    }
#endif

    /* Check if a face using these vertices already exists. If so,
     * skip adding this face and mark the existing one for
     * deletion as well. Prevents extraneous "flaps" from being
     * created. */
#if 0
    if (UNLIKELY(existing_face = BM_face_exists(v_tri, 3)))
#else
    if (UNLIKELY(existing_face = bm_face_exists_tri_from_loop_vert(l->next, v_conn)))
#endif
    {
      BLI_buffer_append(deleted_faces, BMFace *, existing_face);
    }
    else
    {
      BMVert *v_tri[3] = {v_conn, l->next->v, l->prev->v};

      BLI_assert(!BM_face_exists(v_tri, 3));
      BMEdge *e_tri[3];
      PBVHNode *n = pbvh_bmesh_node_from_face(bvh, f);
      int ni = n - bvh->nodes;
      bm_edges_from_tri(bvh->bm, v_tri, e_tri);
      pbvh_bmesh_face_create(bvh, ni, v_tri, e_tri, f);

      /* Ensure that v_conn is in the new face's node */
      if (!BLI_gset_haskey(n->bm_unique_verts, v_conn)) {
        BLI_gset_add(n->bm_other_verts, v_conn);
      }
    }

    BLI_buffer_append(deleted_faces, BMFace *, f);
  }
  BM_LOOPS_OF_VERT_ITER_END;

  /* Delete the tagged faces */
  for (int i = 0; i < deleted_faces->count; i++) {
    BMFace *f_del = BLI_buffer_at(deleted_faces, BMFace *, i);

    /* Get vertices and edges of face */
    BLI_assert(f_del->len == 3);
    BMLoop *l_iter = BM_FACE_FIRST_LOOP(f_del);
    BMVert *v_tri[3];
    BMEdge *e_tri[3];
    v_tri[0] = l_iter->v;
    e_tri[0] = l_iter->e;
    l_iter = l_iter->next;
    v_tri[1] = l_iter->v;
    e_tri[1] = l_iter->e;
    l_iter = l_iter->next;
    v_tri[2] = l_iter->v;
    e_tri[2] = l_iter->e;

    /* Remove the face */
    pbvh_bmesh_face_remove(bvh, f_del);
    BM_face_kill(bvh->bm, f_del);

    /* Check if any of the face's edges are now unused by any
     * face, if so delete them */
    for (int j = 0; j < 3; j++) {
      if (BM_edge_is_wire(e_tri[j])) {
        BM_edge_kill(bvh->bm, e_tri[j]);
      }
    }

    /* Check if any of the face's vertices are now unused, if so
     * remove them from the PBVH */
    for (int j = 0; j < 3; j++) {
      if ((v_tri[j] != v_del) && (v_tri[j]->e == NULL)) {
        pbvh_bmesh_vert_remove(bvh, v_tri[j]);

        BM_log_vert_removed(bvh->bm_log, v_tri[j], eq_ctx->cd_vert_mask_offset);

        if (v_tri[j] == v_conn) {
          v_conn = NULL;
        }
        BLI_ghash_insert(deleted_verts, v_tri[j], NULL);
        BM_vert_kill(bvh->bm, v_tri[j]);
      }
    }
  }

  /* Move v_conn to the midpoint of v_conn and v_del (if v_conn still exists, it
   * may have been deleted above) */
  if (v_conn != NULL) {
    BM_log_vert_before_modified(bvh->bm_log, v_conn, eq_ctx->cd_vert_mask_offset);
    mid_v3_v3v3(v_conn->co, v_conn->co, v_del->co);
    add_v3_v3(v_conn->no, v_del->no);
    normalize_v3(v_conn->no);

    /* update boundboxes attached to the connected vertex
     * note that we can often get-away without this but causes T48779 */
    BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_conn) {
      PBVHNode *f_node = pbvh_bmesh_node_from_face(bvh, l->f);
      f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateBB;
    }
    BM_LOOPS_OF_VERT_ITER_END;
  }

  /* Delete v_del */
  BLI_assert(!BM_vert_face_check(v_del));
  BM_log_vert_removed(bvh->bm_log, v_del, eq_ctx->cd_vert_mask_offset);
  /* v_conn == NULL is OK */
  BLI_ghash_insert(deleted_verts, v_del, v_conn);
  BM_vert_kill(bvh->bm, v_del);
}

static bool pbvh_bmesh_collapse_short_edges(EdgeQueueContext *eq_ctx,
                                            PBVH *bvh,
                                            BLI_Buffer *deleted_faces)
{
  const float min_len_squared = bvh->bm_min_edge_len * bvh->bm_min_edge_len;
  bool any_collapsed = false;
  /* deleted verts point to vertices they were merged into, or NULL when removed. */
  GHash *deleted_verts = BLI_ghash_ptr_new("deleted_verts");

  while (!BLI_heapsimple_is_empty(eq_ctx->q->heap)) {
    BMVert **pair = BLI_heapsimple_pop_min(eq_ctx->q->heap);
    BMVert *v1 = pair[0], *v2 = pair[1];
    BLI_mempool_free(eq_ctx->pool, pair);
    pair = NULL;

    /* Check the verts still exist */
    if (!(v1 = bm_vert_hash_lookup_chain(deleted_verts, v1)) ||
        !(v2 = bm_vert_hash_lookup_chain(deleted_verts, v2)) || (v1 == v2)) {
      continue;
    }

    /* Check that the edge still exists */
    BMEdge *e;
    if (!(e = BM_edge_exists(v1, v2))) {
      continue;
    }
#ifdef USE_EDGEQUEUE_TAG
    EDGE_QUEUE_DISABLE(e);
#endif

    if (len_squared_v3v3(v1->co, v2->co) >= min_len_squared) {
      continue;
    }

    /* Check that the edge's vertices are still in the PBVH. It's
     * possible that an edge collapse has deleted adjacent faces
     * and the node has been split, thus leaving wire edges and
     * associated vertices. */
    if ((BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE) ||
        (BM_ELEM_CD_GET_INT(e->v2, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE)) {
      continue;
    }

    any_collapsed = true;

    pbvh_bmesh_collapse_edge(bvh, e, v1, v2, deleted_verts, deleted_faces, eq_ctx);
  }

  BLI_ghash_free(deleted_verts, NULL, NULL);

  return any_collapsed;
}

/************************* Called from pbvh.c *************************/

bool pbvh_bmesh_node_raycast(PBVHNode *node,
                             const float ray_start[3],
                             struct IsectRayPrecalc *isect_precalc,
                             float *depth,
                             bool use_original)
{
  bool hit = false;

  if (use_original && node->bm_tot_ortri) {
    for (int i = 0; i < node->bm_tot_ortri; i++) {
      const int *t = node->bm_ortri[i];
      hit |= ray_face_intersection_tri(ray_start,
                                       isect_precalc,
                                       node->bm_orco[t[0]],
                                       node->bm_orco[t[1]],
                                       node->bm_orco[t[2]],
                                       depth);
    }
  }
  else {
    GSetIterator gs_iter;

    GSET_ITER (gs_iter, node->bm_faces) {
      BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

      BLI_assert(f->len == 3);
      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        BMVert *v_tri[3];

        BM_face_as_array_vert_tri(f, v_tri);
        hit |= ray_face_intersection_tri(
            ray_start, isect_precalc, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co, depth);
      }
    }
  }

  return hit;
}

bool BKE_pbvh_bmesh_node_raycast_detail(PBVHNode *node,
                                        const float ray_start[3],
                                        struct IsectRayPrecalc *isect_precalc,
                                        float *depth,
                                        float *r_edge_length)
{
  if (node->flag & PBVH_FullyHidden) {
    return 0;
  }

  GSetIterator gs_iter;
  bool hit = false;
  BMFace *f_hit = NULL;

  GSET_ITER (gs_iter, node->bm_faces) {
    BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

    BLI_assert(f->len == 3);
    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      BMVert *v_tri[3];
      bool hit_local;
      BM_face_as_array_vert_tri(f, v_tri);
      hit_local = ray_face_intersection_tri(
          ray_start, isect_precalc, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co, depth);

      if (hit_local) {
        f_hit = f;
        hit = true;
      }
    }
  }

  if (hit) {
    BMVert *v_tri[3];
    BM_face_as_array_vert_tri(f_hit, v_tri);
    float len1 = len_squared_v3v3(v_tri[0]->co, v_tri[1]->co);
    float len2 = len_squared_v3v3(v_tri[1]->co, v_tri[2]->co);
    float len3 = len_squared_v3v3(v_tri[2]->co, v_tri[0]->co);

    /* detail returned will be set to the maximum allowed size, so take max here */
    *r_edge_length = sqrtf(max_fff(len1, len2, len3));
  }

  return hit;
}

bool pbvh_bmesh_node_nearest_to_ray(PBVHNode *node,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth,
                                    float *dist_sq,
                                    bool use_original)
{
  bool hit = false;

  if (use_original && node->bm_tot_ortri) {
    for (int i = 0; i < node->bm_tot_ortri; i++) {
      const int *t = node->bm_ortri[i];
      hit |= ray_face_nearest_tri(ray_start,
                                  ray_normal,
                                  node->bm_orco[t[0]],
                                  node->bm_orco[t[1]],
                                  node->bm_orco[t[2]],
                                  depth,
                                  dist_sq);
    }
  }
  else {
    GSetIterator gs_iter;

    GSET_ITER (gs_iter, node->bm_faces) {
      BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

      BLI_assert(f->len == 3);
      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        BMVert *v_tri[3];

        BM_face_as_array_vert_tri(f, v_tri);
        hit |= ray_face_nearest_tri(
            ray_start, ray_normal, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co, depth, dist_sq);
      }
    }
  }

  return hit;
}

void pbvh_bmesh_normals_update(PBVHNode **nodes, int totnode)
{
  for (int n = 0; n < totnode; n++) {
    PBVHNode *node = nodes[n];

    if (node->flag & PBVH_UpdateNormals) {
      GSetIterator gs_iter;

      GSET_ITER (gs_iter, node->bm_faces) {
        BM_face_normal_update(BLI_gsetIterator_getKey(&gs_iter));
      }
      GSET_ITER (gs_iter, node->bm_unique_verts) {
        BM_vert_normal_update(BLI_gsetIterator_getKey(&gs_iter));
      }
      /* This should be unneeded normally */
      GSET_ITER (gs_iter, node->bm_other_verts) {
        BM_vert_normal_update(BLI_gsetIterator_getKey(&gs_iter));
      }
      node->flag &= ~PBVH_UpdateNormals;
    }
  }
}

struct FastNodeBuildInfo {
  int totface; /* number of faces */
  int start;   /* start of faces in array */
  struct FastNodeBuildInfo *child1;
  struct FastNodeBuildInfo *child2;
};

/**
 * Recursively split the node if it exceeds the leaf_limit.
 * This function is multi-threadabe since each invocation applies
 * to a sub part of the arrays.
 */
static void pbvh_bmesh_node_limit_ensure_fast(
    PBVH *bvh, BMFace **nodeinfo, BBC *bbc_array, struct FastNodeBuildInfo *node, MemArena *arena)
{
  struct FastNodeBuildInfo *child1, *child2;

  if (node->totface <= bvh->leaf_limit) {
    return;
  }

  /* Calculate bounding box around primitive centroids */
  BB cb;
  BB_reset(&cb);
  for (int i = 0; i < node->totface; i++) {
    BMFace *f = nodeinfo[i + node->start];
    BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    BB_expand(&cb, bbc->bcentroid);
  }

  /* initialize the children */

  /* Find widest axis and its midpoint */
  const int axis = BB_widest_axis(&cb);
  const float mid = (cb.bmax[axis] + cb.bmin[axis]) * 0.5f;

  int num_child1 = 0, num_child2 = 0;

  /* split vertices along the middle line */
  const int end = node->start + node->totface;
  for (int i = node->start; i < end - num_child2; i++) {
    BMFace *f = nodeinfo[i];
    BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    if (bbc->bcentroid[axis] > mid) {
      int i_iter = end - num_child2 - 1;
      int candidate = -1;
      /* found a face that should be part of another node, look for a face to substitute with */

      for (; i_iter > i; i_iter--) {
        BMFace *f_iter = nodeinfo[i_iter];
        const BBC *bbc_iter = &bbc_array[BM_elem_index_get(f_iter)];
        if (bbc_iter->bcentroid[axis] <= mid) {
          candidate = i_iter;
          break;
        }
        else {
          num_child2++;
        }
      }

      if (candidate != -1) {
        BMFace *tmp = nodeinfo[i];
        nodeinfo[i] = nodeinfo[candidate];
        nodeinfo[candidate] = tmp;
        /* increase both counts */
        num_child1++;
        num_child2++;
      }
      else {
        /* not finding candidate means second half of array part is full of
         * second node parts, just increase the number of child nodes for it */
        num_child2++;
      }
    }
    else {
      num_child1++;
    }
  }

  /* ensure at least one child in each node */
  if (num_child2 == 0) {
    num_child2++;
    num_child1--;
  }
  else if (num_child1 == 0) {
    num_child1++;
    num_child2--;
  }

  /* at this point, faces should have been split along the array range sequentially,
   * each sequential part belonging to one node only */
  BLI_assert((num_child1 + num_child2) == node->totface);

  node->child1 = child1 = BLI_memarena_alloc(arena, sizeof(struct FastNodeBuildInfo));
  node->child2 = child2 = BLI_memarena_alloc(arena, sizeof(struct FastNodeBuildInfo));

  child1->totface = num_child1;
  child1->start = node->start;
  child2->totface = num_child2;
  child2->start = node->start + num_child1;
  child1->child1 = child1->child2 = child2->child1 = child2->child2 = NULL;

  pbvh_bmesh_node_limit_ensure_fast(bvh, nodeinfo, bbc_array, child1, arena);
  pbvh_bmesh_node_limit_ensure_fast(bvh, nodeinfo, bbc_array, child2, arena);
}

static void pbvh_bmesh_create_nodes_fast_recursive(
    PBVH *bvh, BMFace **nodeinfo, BBC *bbc_array, struct FastNodeBuildInfo *node, int node_index)
{
  PBVHNode *n = bvh->nodes + node_index;
  /* two cases, node does not have children or does have children */
  if (node->child1) {
    int children_offset = bvh->totnode;

    n->children_offset = children_offset;
    pbvh_grow_nodes(bvh, bvh->totnode + 2);
    pbvh_bmesh_create_nodes_fast_recursive(
        bvh, nodeinfo, bbc_array, node->child1, children_offset);
    pbvh_bmesh_create_nodes_fast_recursive(
        bvh, nodeinfo, bbc_array, node->child2, children_offset + 1);

    n = &bvh->nodes[node_index];

    /* Update bounding box */
    BB_reset(&n->vb);
    BB_expand_with_bb(&n->vb, &bvh->nodes[n->children_offset].vb);
    BB_expand_with_bb(&n->vb, &bvh->nodes[n->children_offset + 1].vb);
    n->orig_vb = n->vb;
  }
  else {
    /* node does not have children so it's a leaf node, populate with faces and tag accordingly
     * this is an expensive part but it's not so easily threadable due to vertex node indices */
    const int cd_vert_node_offset = bvh->cd_vert_node_offset;
    const int cd_face_node_offset = bvh->cd_face_node_offset;

    bool has_visible = false;

    n->flag = PBVH_Leaf;
    n->bm_faces = BLI_gset_ptr_new_ex("bm_faces", node->totface);

    /* Create vert hash sets */
    n->bm_unique_verts = BLI_gset_ptr_new("bm_unique_verts");
    n->bm_other_verts = BLI_gset_ptr_new("bm_other_verts");

    BB_reset(&n->vb);

    const int end = node->start + node->totface;

    for (int i = node->start; i < end; i++) {
      BMFace *f = nodeinfo[i];
      BBC *bbc = &bbc_array[BM_elem_index_get(f)];

      /* Update ownership of faces */
      BLI_gset_insert(n->bm_faces, f);
      BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

      /* Update vertices */
      BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
      BMLoop *l_iter = l_first;
      do {
        BMVert *v = l_iter->v;
        if (!BLI_gset_haskey(n->bm_unique_verts, v)) {
          if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
            BLI_gset_add(n->bm_other_verts, v);
          }
          else {
            BLI_gset_insert(n->bm_unique_verts, v);
            BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, node_index);
          }
        }
        /* Update node bounding box */
      } while ((l_iter = l_iter->next) != l_first);

      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        has_visible = true;
      }

      BB_expand_with_bb(&n->vb, (BB *)bbc);
    }

    BLI_assert(n->vb.bmin[0] <= n->vb.bmax[0] && n->vb.bmin[1] <= n->vb.bmax[1] &&
               n->vb.bmin[2] <= n->vb.bmax[2]);

    n->orig_vb = n->vb;

    /* Build GPU buffers for new node and update vertex normals */
    BKE_pbvh_node_mark_rebuild_draw(n);

    BKE_pbvh_node_fully_hidden_set(n, !has_visible);
    n->flag |= PBVH_UpdateNormals;
  }
}

/***************************** Public API *****************************/

/* Build a PBVH from a BMesh */
void BKE_pbvh_build_bmesh(PBVH *bvh,
                          BMesh *bm,
                          bool smooth_shading,
                          BMLog *log,
                          const int cd_vert_node_offset,
                          const int cd_face_node_offset)
{
  bvh->cd_vert_node_offset = cd_vert_node_offset;
  bvh->cd_face_node_offset = cd_face_node_offset;
  bvh->bm = bm;

  BKE_pbvh_bmesh_detail_size_set(bvh, 0.75);

  bvh->type = PBVH_BMESH;
  bvh->bm_log = log;

  /* TODO: choose leaf limit better */
  bvh->leaf_limit = 100;

  if (smooth_shading) {
    bvh->flags |= PBVH_DYNTOPO_SMOOTH_SHADING;
  }

  /* bounding box array of all faces, no need to recalculate every time */
  BBC *bbc_array = MEM_mallocN(sizeof(BBC) * bm->totface, "BBC");
  BMFace **nodeinfo = MEM_mallocN(sizeof(*nodeinfo) * bm->totface, "nodeinfo");
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "fast PBVH node storage");

  BMIter iter;
  BMFace *f;
  int i;
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    BBC *bbc = &bbc_array[i];
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    BB_reset((BB *)bbc);
    do {
      BB_expand((BB *)bbc, l_iter->v->co);
    } while ((l_iter = l_iter->next) != l_first);
    BBC_update_centroid(bbc);

    /* so we can do direct lookups on 'bbc_array' */
    BM_elem_index_set(f, i); /* set_dirty! */
    nodeinfo[i] = f;
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
  }

  BMVert *v;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, DYNTOPO_NODE_NONE);
  }

  /* likely this is already dirty */
  bm->elem_index_dirty |= BM_FACE;

  /* setup root node */
  struct FastNodeBuildInfo rootnode = {0};
  rootnode.totface = bm->totface;

  /* start recursion, assign faces to nodes accordingly */
  pbvh_bmesh_node_limit_ensure_fast(bvh, nodeinfo, bbc_array, &rootnode, arena);

  /* We now have all faces assigned to a node,
   * next we need to assign those to the gsets of the nodes. */

  /* Start with all faces in the root node */
  bvh->nodes = MEM_callocN(sizeof(PBVHNode), "PBVHNode");
  bvh->totnode = 1;

  /* take root node and visit and populate children recursively */
  pbvh_bmesh_create_nodes_fast_recursive(bvh, nodeinfo, bbc_array, &rootnode, 0);

  BLI_memarena_free(arena);
  MEM_freeN(bbc_array);
  MEM_freeN(nodeinfo);
}

/* Collapse short edges, subdivide long edges */
bool BKE_pbvh_bmesh_update_topology(PBVH *bvh,
                                    PBVHTopologyUpdateMode mode,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    const bool use_frontface,
                                    const bool use_projected)
{
  /* 2 is enough for edge faces - manifold edge */
  BLI_buffer_declare_static(BMLoop *, edge_loops, BLI_BUFFER_NOP, 2);
  BLI_buffer_declare_static(BMFace *, deleted_faces, BLI_BUFFER_NOP, 32);
  const int cd_vert_mask_offset = CustomData_get_offset(&bvh->bm->vdata, CD_PAINT_MASK);
  const int cd_vert_node_offset = bvh->cd_vert_node_offset;
  const int cd_face_node_offset = bvh->cd_face_node_offset;

  bool modified = false;

  if (view_normal) {
    BLI_assert(len_squared_v3(view_normal) != 0.0f);
  }

  if (mode & PBVH_Collapse) {
    EdgeQueue q;
    BLI_mempool *queue_pool = BLI_mempool_create(sizeof(BMVert * [2]), 0, 128, BLI_MEMPOOL_NOP);
    EdgeQueueContext eq_ctx = {
        &q,
        queue_pool,
        bvh->bm,
        cd_vert_mask_offset,
        cd_vert_node_offset,
        cd_face_node_offset,
    };

    short_edge_queue_create(
        &eq_ctx, bvh, center, view_normal, radius, use_frontface, use_projected);
    modified |= pbvh_bmesh_collapse_short_edges(&eq_ctx, bvh, &deleted_faces);
    BLI_heapsimple_free(q.heap, NULL);
    BLI_mempool_destroy(queue_pool);
  }

  if (mode & PBVH_Subdivide) {
    EdgeQueue q;
    BLI_mempool *queue_pool = BLI_mempool_create(sizeof(BMVert * [2]), 0, 128, BLI_MEMPOOL_NOP);
    EdgeQueueContext eq_ctx = {
        &q,
        queue_pool,
        bvh->bm,
        cd_vert_mask_offset,
        cd_vert_node_offset,
        cd_face_node_offset,
    };

    long_edge_queue_create(
        &eq_ctx, bvh, center, view_normal, radius, use_frontface, use_projected);
    modified |= pbvh_bmesh_subdivide_long_edges(&eq_ctx, bvh, &edge_loops);
    BLI_heapsimple_free(q.heap, NULL);
    BLI_mempool_destroy(queue_pool);
  }

  /* Unmark nodes */
  for (int n = 0; n < bvh->totnode; n++) {
    PBVHNode *node = &bvh->nodes[n];

    if (node->flag & PBVH_Leaf && node->flag & PBVH_UpdateTopology) {
      node->flag &= ~PBVH_UpdateTopology;
    }
  }
  BLI_buffer_free(&edge_loops);
  BLI_buffer_free(&deleted_faces);

#ifdef USE_VERIFY
  pbvh_bmesh_verify(bvh);
#endif

  return modified;
}

/* In order to perform operations on the original node coordinates
 * (currently just raycast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden. */
void BKE_pbvh_bmesh_node_save_orig(PBVHNode *node)
{
  /* Skip if original coords/triangles are already saved */
  if (node->bm_orco) {
    return;
  }

  const int totvert = BLI_gset_len(node->bm_unique_verts) + BLI_gset_len(node->bm_other_verts);

  const int tottri = BLI_gset_len(node->bm_faces);

  node->bm_orco = MEM_mallocN(sizeof(*node->bm_orco) * totvert, __func__);
  node->bm_ortri = MEM_mallocN(sizeof(*node->bm_ortri) * tottri, __func__);

  /* Copy out the vertices and assign a temporary index */
  int i = 0;
  GSetIterator gs_iter;
  GSET_ITER (gs_iter, node->bm_unique_verts) {
    BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
    copy_v3_v3(node->bm_orco[i], v->co);
    BM_elem_index_set(v, i); /* set_dirty! */
    i++;
  }
  GSET_ITER (gs_iter, node->bm_other_verts) {
    BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
    copy_v3_v3(node->bm_orco[i], v->co);
    BM_elem_index_set(v, i); /* set_dirty! */
    i++;
  }

  /* Copy the triangles */
  i = 0;
  GSET_ITER (gs_iter, node->bm_faces) {
    BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

#if 0
    BMIter bm_iter;
    BMVert *v;
    int j = 0;
    BM_ITER_ELEM (v, &bm_iter, f, BM_VERTS_OF_FACE) {
      node->bm_ortri[i][j] = BM_elem_index_get(v);
      j++;
    }
#else
    bm_face_as_array_index_tri(f, node->bm_ortri[i]);
#endif
    i++;
  }
  node->bm_tot_ortri = i;
}

void BKE_pbvh_bmesh_after_stroke(PBVH *bvh)
{
  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = &bvh->nodes[i];
    if (n->flag & PBVH_Leaf) {
      /* Free orco/ortri data */
      pbvh_bmesh_node_drop_orig(n);

      /* Recursively split nodes that have gotten too many
       * elements */
      pbvh_bmesh_node_limit_ensure(bvh, i);
    }
  }
}

void BKE_pbvh_bmesh_detail_size_set(PBVH *bvh, float detail_size)
{
  bvh->bm_max_edge_len = detail_size;
  bvh->bm_min_edge_len = bvh->bm_max_edge_len * 0.4f;
}

void BKE_pbvh_node_mark_topology_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateTopology;
}

GSet *BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node)
{
  return node->bm_unique_verts;
}

GSet *BKE_pbvh_bmesh_node_other_verts(PBVHNode *node)
{
  return node->bm_other_verts;
}

struct GSet *BKE_pbvh_bmesh_node_faces(PBVHNode *node)
{
  return node->bm_faces;
}

/****************************** Debugging *****************************/

#if 0

static void pbvh_bmesh_print(PBVH *bvh)
{
  fprintf(stderr, "\npbvh=%p\n", bvh);
  fprintf(stderr, "bm_face_to_node:\n");

  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bvh->bm, BM_FACES_OF_MESH) {
    fprintf(stderr, "  %d -> %d\n", BM_elem_index_get(f), pbvh_bmesh_node_index_from_face(bvh, f));
  }

  fprintf(stderr, "bm_vert_to_node:\n");
  BMVert *v;
  BM_ITER_MESH (v, &iter, bvh->bm, BM_FACES_OF_MESH) {
    fprintf(stderr, "  %d -> %d\n", BM_elem_index_get(v), pbvh_bmesh_node_index_from_vert(bvh, v));
  }

  for (int n = 0; n < bvh->totnode; n++) {
    PBVHNode *node = &bvh->nodes[n];
    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    GSetIterator gs_iter;
    fprintf(stderr, "node %d\n  faces:\n", n);
    GSET_ITER (gs_iter, node->bm_faces)
      fprintf(stderr, "    %d\n", BM_elem_index_get((BMFace *)BLI_gsetIterator_getKey(&gs_iter)));
    fprintf(stderr, "  unique verts:\n");
    GSET_ITER (gs_iter, node->bm_unique_verts)
      fprintf(stderr, "    %d\n", BM_elem_index_get((BMVert *)BLI_gsetIterator_getKey(&gs_iter)));
    fprintf(stderr, "  other verts:\n");
    GSET_ITER (gs_iter, node->bm_other_verts)
      fprintf(stderr, "    %d\n", BM_elem_index_get((BMVert *)BLI_gsetIterator_getKey(&gs_iter)));
  }
}

static void print_flag_factors(int flag)
{
  printf("flag=0x%x:\n", flag);
  for (int i = 0; i < 32; i++) {
    if (flag & (1 << i)) {
      printf("  %d (1 << %d)\n", 1 << i, i);
    }
  }
}
#endif

#ifdef USE_VERIFY

static void pbvh_bmesh_verify(PBVH *bvh)
{
  /* build list of faces & verts to lookup */
  GSet *faces_all = BLI_gset_ptr_new_ex(__func__, bvh->bm->totface);
  BMIter iter;

  {
    BMFace *f;
    BM_ITER_MESH (f, &iter, bvh->bm, BM_FACES_OF_MESH) {
      BLI_assert(BM_ELEM_CD_GET_INT(f, bvh->cd_face_node_offset) != DYNTOPO_NODE_NONE);
      BLI_gset_insert(faces_all, f);
    }
  }

  GSet *verts_all = BLI_gset_ptr_new_ex(__func__, bvh->bm->totvert);
  {
    BMVert *v;
    BM_ITER_MESH (v, &iter, bvh->bm, BM_VERTS_OF_MESH) {
      if (BM_ELEM_CD_GET_INT(v, bvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
        BLI_gset_insert(verts_all, v);
      }
    }
  }

  /* Check vert/face counts */
  {
    int totface = 0, totvert = 0;
    for (int i = 0; i < bvh->totnode; i++) {
      PBVHNode *n = &bvh->nodes[i];
      totface += n->bm_faces ? BLI_gset_len(n->bm_faces) : 0;
      totvert += n->bm_unique_verts ? BLI_gset_len(n->bm_unique_verts) : 0;
    }

    BLI_assert(totface == BLI_gset_len(faces_all));
    BLI_assert(totvert == BLI_gset_len(verts_all));
  }

  {
    BMFace *f;
    BM_ITER_MESH (f, &iter, bvh->bm, BM_FACES_OF_MESH) {
      BMIter bm_iter;
      BMVert *v;
      PBVHNode *n = pbvh_bmesh_node_lookup(bvh, f);

      /* Check that the face's node is a leaf */
      BLI_assert(n->flag & PBVH_Leaf);

      /* Check that the face's node knows it owns the face */
      BLI_assert(BLI_gset_haskey(n->bm_faces, f));

      /* Check the face's vertices... */
      BM_ITER_ELEM (v, &bm_iter, f, BM_VERTS_OF_FACE) {
        PBVHNode *nv;

        /* Check that the vertex is in the node */
        BLI_assert(BLI_gset_haskey(n->bm_unique_verts, v) ^ BLI_gset_haskey(n->bm_other_verts, v));

        /* Check that the vertex has a node owner */
        nv = pbvh_bmesh_node_lookup(bvh, v);

        /* Check that the vertex's node knows it owns the vert */
        BLI_assert(BLI_gset_haskey(nv->bm_unique_verts, v));

        /* Check that the vertex isn't duplicated as an 'other' vert */
        BLI_assert(!BLI_gset_haskey(nv->bm_other_verts, v));
      }
    }
  }

  /* Check verts */
  {
    BMVert *v;
    BM_ITER_MESH (v, &iter, bvh->bm, BM_VERTS_OF_MESH) {
      /* vertex isn't tracked */
      if (BM_ELEM_CD_GET_INT(v, bvh->cd_vert_node_offset) == DYNTOPO_NODE_NONE) {
        continue;
      }

      PBVHNode *n = pbvh_bmesh_node_lookup(bvh, v);

      /* Check that the vert's node is a leaf */
      BLI_assert(n->flag & PBVH_Leaf);

      /* Check that the vert's node knows it owns the vert */
      BLI_assert(BLI_gset_haskey(n->bm_unique_verts, v));

      /* Check that the vertex isn't duplicated as an 'other' vert */
      BLI_assert(!BLI_gset_haskey(n->bm_other_verts, v));

      /* Check that the vert's node also contains one of the vert's
       * adjacent faces */
      bool found = false;
      BMIter bm_iter;
      BMFace *f = NULL;
      BM_ITER_ELEM (f, &bm_iter, v, BM_FACES_OF_VERT) {
        if (pbvh_bmesh_node_lookup(bvh, f) == n) {
          found = true;
          break;
        }
      }
      BLI_assert(found || f == NULL);

#  if 1
      /* total freak stuff, check if node exists somewhere else */
      /* Slow */
      for (int i = 0; i < bvh->totnode; i++) {
        PBVHNode *n_other = &bvh->nodes[i];
        if ((n != n_other) && (n_other->bm_unique_verts)) {
          BLI_assert(!BLI_gset_haskey(n_other->bm_unique_verts, v));
        }
      }
#  endif
    }
  }

#  if 0
  /* check that every vert belongs somewhere */
  /* Slow */
  BM_ITER_MESH (vi, &iter, bvh->bm, BM_VERTS_OF_MESH) {
    bool has_unique = false;
    for (int i = 0; i < bvh->totnode; i++) {
      PBVHNode *n = &bvh->nodes[i];
      if ((n->bm_unique_verts != NULL) && BLI_gset_haskey(n->bm_unique_verts, vi)) {
        has_unique = true;
      }
    }
    BLI_assert(has_unique);
    vert_count++;
  }

  /* if totvert differs from number of verts inside the hash. hash-totvert is checked above  */
  BLI_assert(vert_count == bvh->bm->totvert);
#  endif

  /* Check that node elements are recorded in the top level */
  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = &bvh->nodes[i];
    if (n->flag & PBVH_Leaf) {
      GSetIterator gs_iter;

      GSET_ITER (gs_iter, n->bm_faces) {
        BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
        PBVHNode *n_other = pbvh_bmesh_node_lookup(bvh, f);
        BLI_assert(n == n_other);
        BLI_assert(BLI_gset_haskey(faces_all, f));
      }

      GSET_ITER (gs_iter, n->bm_unique_verts) {
        BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
        PBVHNode *n_other = pbvh_bmesh_node_lookup(bvh, v);
        BLI_assert(!BLI_gset_haskey(n->bm_other_verts, v));
        BLI_assert(n == n_other);
        BLI_assert(BLI_gset_haskey(verts_all, v));
      }

      GSET_ITER (gs_iter, n->bm_other_verts) {
        BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
        /* this happens sometimes and seems harmless */
        // BLI_assert(!BM_vert_face_check(v));
        BLI_assert(BLI_gset_haskey(verts_all, v));
      }
    }
  }

  BLI_gset_free(faces_all, NULL);
  BLI_gset_free(verts_all, NULL);
}

#endif

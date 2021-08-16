#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_heap.h"
#include "BLI_heap_simple.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "PIL_time.h"
#include "atomic_ops.h"

#include "BKE_customdata.h"
#include "BKE_dyntopo.h"
#include "BKE_pbvh.h"

#include "bmesh.h"
#include "pbvh_intern.h"

#include <stdio.h>

#define DYNVERT_ALL_BOUNDARY (DYNVERT_BOUNDARY | DYNVERT_FSET_BOUNDARY)

#define DYNTOPO_MAX_ITER 4096

#define DYNTOPO_USE_HEAP

#ifndef DYNTOPO_USE_HEAP
/* don't add edges into the queue multiple times */
#  define USE_EDGEQUEUE_TAG
#endif

/* Avoid skinny faces */
#define USE_EDGEQUEUE_EVEN_SUBDIV

/* How much longer we need to be to consider for subdividing
 * (avoids subdividing faces which are only *slightly* skinny) */
#define EVEN_EDGELEN_THRESHOLD 1.2f
/* How much the limit increases per recursion
 * (avoids performing subdivisions too far away). */
#define EVEN_GENERATION_SCALE 1.1f

// recursion depth to start applying front face test
#define DEPTH_START_LIMIT 5

//#define FANCY_EDGE_WEIGHTS
#define SKINNY_EDGE_FIX

// slightly relax geometry by this factor along surface tangents
// to improve convergence of remesher
#define DYNTOPO_SAFE_SMOOTH_FAC 0.05f

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
#  include "BKE_global.h"
#endif

/* Support for only operating on front-faces */
#define USE_EDGEQUEUE_FRONTFACE

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

#define DYNTOPO_MASK(cd_mask_offset, v) BM_ELEM_CD_GET_FLOAT(v, cd_mask_offset)

#ifdef USE_VERIFY
static void pbvh_bmesh_verify(PBVH *pbvh);
#endif

/* -------------------------------------------------------------------- */
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

static bool check_face_is_tri(PBVH *pbvh, BMFace *f);
static bool check_vert_fan_are_tris(PBVH *pbvh, BMVert *v);

BLI_INLINE void surface_smooth_v_safe(PBVH *pbvh, BMVert *v)
{
  float co[3];
  float tan[3];
  float tot = 0.0;

  zero_v3(co);

  // this is a manual edge walk

  BMEdge *e = v->e;
  if (!e) {
    return;
  }
  pbvh_check_vert_boundary(pbvh, v);

  const int cd_dyn_vert = pbvh->cd_dyn_vert;

  MDynTopoVert *mv1 = BKE_PBVH_DYNVERT(cd_dyn_vert, v);
  const bool bound1 = mv1->flag & DYNVERT_ALL_BOUNDARY;

  do {
    BMVert *v2 = e->v1 == v ? e->v2 : e->v1;

    pbvh_check_vert_boundary(pbvh, v2);

    MDynTopoVert *mv2 = BKE_PBVH_DYNVERT(cd_dyn_vert, v2);
    const bool bound2 = mv2->flag & DYNVERT_ALL_BOUNDARY;

    if (bound1 != bound2) {
      e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
      continue;
    }

    sub_v3_v3v3(tan, v2->co, v->co);
    float d = dot_v3v3(tan, v->no);

    madd_v3_v3fl(tan, v->no, -d * 0.99f);
    add_v3_v3(co, tan);
    tot += 1.0f;

    e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
  } while (e != v->e);

  if (tot == 0.0f) {
    return;
  }

  mul_v3_fl(co, 1.0f / tot);
  float x = v->co[0], y = v->co[1], z = v->co[2];

  // conflicts here should be pretty rare.
  atomic_cas_float(&v->co[0], x, x + co[0] * DYNTOPO_SAFE_SMOOTH_FAC);
  atomic_cas_float(&v->co[1], y, y + co[1] * DYNTOPO_SAFE_SMOOTH_FAC);
  atomic_cas_float(&v->co[2], z, z + co[2] * DYNTOPO_SAFE_SMOOTH_FAC);
}

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
      /* Not remapped. */
      return v;
    }
    if (*v_next_p == NULL) {
      /* removed and not remapped */
      return NULL;
    }

    /* remapped */
    v = *v_next_p;
  }
}

static void pbvh_bmesh_copy_facedata(BMesh *bm, BMFace *dest, BMFace *src)
{
  dest->head.hflag = src->head.hflag;
  dest->mat_nr = src->mat_nr;
  CustomData_bmesh_copy_data(&bm->pdata, &bm->pdata, src->head.data, &dest->head.data);
}

static BMVert *pbvh_bmesh_vert_create(PBVH *pbvh,
                                      int node_index,
                                      const float co[3],
                                      const float no[3],
                                      BMVert *v_example,
                                      const int cd_vert_mask_offset)
{
  PBVHNode *node = &pbvh->nodes[node_index];

  BLI_assert((pbvh->totnode == 1 || node_index) && node_index <= pbvh->totnode);

  /* avoid initializing customdata because its quite involved */
  BMVert *v = BM_vert_create(pbvh->bm, co, NULL, BM_CREATE_NOP);

  if (v_example) {
    v->head.hflag = v_example->head.hflag;

    CustomData_bmesh_copy_data(
        &pbvh->bm->vdata, &pbvh->bm->vdata, v_example->head.data, &v->head.data);

    /* This value is logged below */
    copy_v3_v3(v->no, no);

    // keep MDynTopoVert copied from v_example as-is
  }
  else {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);

    copy_v3_v3(mv->origco, co);
    copy_v3_v3(mv->origno, no);
    mv->origmask = 0.0f;
    mv->flag = 0;

    /* This value is logged below */
    copy_v3_v3(v->no, no);
  }

  BLI_table_gset_insert(node->bm_unique_verts, v);
  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, node_index);

  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris;

  /* Log the new vertex */
  BM_log_vert_added(pbvh->bm_log, v, cd_vert_mask_offset);
  v->head.index = pbvh->bm->totvert;  // set provisional index

  return v;
}

/**
 * \note Callers are responsible for checking if the face exists before adding.
 */
static BMFace *pbvh_bmesh_face_create(PBVH *pbvh,
                                      int node_index,
                                      BMVert *v_tri[3],
                                      BMEdge *e_tri[3],
                                      const BMFace *f_example,
                                      bool ensure_verts,
                                      bool log_face)
{
  PBVHNode *node = &pbvh->nodes[node_index];

  /* ensure we never add existing face */
  BLI_assert(!BM_face_exists(v_tri, 3));

  BMFace *f;

  if (!e_tri) {
    f = BM_face_create_verts(pbvh->bm, v_tri, 3, f_example, BM_CREATE_NOP, true);
  }
  else {
    f = BM_face_create(pbvh->bm, v_tri, e_tri, 3, f_example, BM_CREATE_NOP);
  }

  if (f_example) {
    f->head.hflag = f_example->head.hflag;
  }

  BLI_table_gset_insert(node->bm_faces, f);
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, node_index);

  /* mark node for update */
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateTris;
  node->flag &= ~PBVH_FullyHidden;

  /* Log the new face */
  if (log_face) {
    BM_log_face_added(pbvh->bm_log, f);
  }

  int cd_vert_node = pbvh->cd_vert_node_offset;

  if (ensure_verts) {
    BMLoop *l = f->l_first;
    do {
      int ni = BM_ELEM_CD_GET_INT(l->v, cd_vert_node);

      if (ni == DYNTOPO_NODE_NONE) {
        BLI_table_gset_add(node->bm_unique_verts, l->v);
        BM_ELEM_CD_SET_INT(l->v, cd_vert_node, node_index);

        node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris;
      }
      else if (ni != node_index) {
        BLI_table_gset_add(node->bm_other_verts, l->v);
      }

      l = l->next;
    } while (l != f->l_first);
  }

  return f;
}

BMVert *BKE_pbvh_vert_create_bmesh(
    PBVH *pbvh, float co[3], float no[3], PBVHNode *node, BMVert *v_example)
{
  if (!node) {
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node2 = pbvh->nodes + i;

      if (!(node2->flag & PBVH_Leaf)) {
        continue;
      }

      // ensure we have at least some node somewhere picked
      node = node2;

      bool ok = true;

      for (int j = 0; j < 3; j++) {
        if (co[j] < node2->vb.bmin[j] || co[j] >= node2->vb.bmax[j]) {
          continue;
        }
      }

      if (ok) {
        break;
      }
    }
  }

  BMVert *v;

  if (!node) {
    printf("possible pbvh error\n");
    v = BM_vert_create(pbvh->bm, co, v_example, BM_CREATE_NOP);
    BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

    MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, pbvh->cd_dyn_vert);

    copy_v3_v3(mv->origco, co);

    return v;
  }

  return pbvh_bmesh_vert_create(
      pbvh, node - pbvh->nodes, co, no, v_example, pbvh->cd_vert_mask_offset);
}

PBVHNode *BKE_pbvh_node_from_face_bmesh(PBVH *pbvh, BMFace *f)
{
  return pbvh->nodes + BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);
}

BMFace *BKE_pbvh_face_create_bmesh(PBVH *pbvh,
                                   BMVert *v_tri[3],
                                   BMEdge *e_tri[3],
                                   const BMFace *f_example)
{
  int ni = DYNTOPO_NODE_NONE;

  for (int i = 0; i < 3; i++) {
    BMVert *v = v_tri[i];
    BMLoop *l;
    BMIter iter;

    BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
      int ni2 = BM_ELEM_CD_GET_INT(l->f, pbvh->cd_face_node_offset);
      if (ni2 != DYNTOPO_NODE_NONE) {
        ni = ni2;
        break;
      }
    }
  }

  if (ni == DYNTOPO_NODE_NONE) {
    BMFace *f;

    // no existing nodes? find one
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if (!(node->flag & PBVH_Leaf)) {
        continue;
      }

      for (int j = 0; j < 3; j++) {
        BMVert *v = v_tri[j];

        bool ok = true;

        for (int k = 0; k < 3; k++) {
          if (v->co[k] < node->vb.bmin[k] || v->co[k] >= node->vb.bmax[k]) {
            ok = false;
          }
        }

        if (ok &&
            (ni == DYNTOPO_NODE_NONE || BLI_table_gset_len(node->bm_faces) < pbvh->leaf_limit)) {
          ni = i;
          break;
        }
      }

      if (ni != DYNTOPO_NODE_NONE) {
        break;
      }
    }

    if (ni == DYNTOPO_NODE_NONE) {
      // empty pbvh?
      printf("possibly pbvh error\n");

      if (e_tri) {
        f = BM_face_create_verts(pbvh->bm, v_tri, 3, f_example, BM_CREATE_NOP, true);
      }
      else {
        f = BM_face_create(pbvh->bm, v_tri, e_tri, 3, f_example, BM_CREATE_NOP);
      }

      if (f_example) {
        f->head.hflag = f_example->head.hflag;
      }

      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

      return f;
    }
  }

  return pbvh_bmesh_face_create(pbvh, ni, v_tri, e_tri, f_example, true, true);
}

#define pbvh_bmesh_node_vert_use_count_is_equal(pbvh, node, v, n) \
  (pbvh_bmesh_node_vert_use_count_at_most(pbvh, node, v, (n) + 1) == n)

static int pbvh_bmesh_node_vert_use_count_at_most(PBVH *pbvh,
                                                  PBVHNode *node,
                                                  BMVert *v,
                                                  const int count_max)
{
  int count = 0;
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);
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
static PBVHNode *pbvh_bmesh_vert_other_node_find(PBVH *pbvh, BMVert *v)
{
  PBVHNode *current_node = pbvh_bmesh_node_from_vert(pbvh, v);
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);

    if (f_node != current_node) {
      return f_node;
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return NULL;
}

static void pbvh_bmesh_vert_ownership_transfer(PBVH *pbvh, PBVHNode *new_owner, BMVert *v)
{
  PBVHNode *current_owner = pbvh_bmesh_node_from_vert(pbvh, v);
  /* mark node for update */

  if (current_owner) {
    current_owner->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;

    BLI_assert(current_owner != new_owner);

    /* Remove current ownership */
    BLI_table_gset_remove(current_owner->bm_unique_verts, v, NULL);
  }

  /* Set new ownership */
  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, new_owner - pbvh->nodes);
  BLI_table_gset_insert(new_owner->bm_unique_verts, v);
  BLI_table_gset_remove(new_owner->bm_other_verts, v, NULL);
  BLI_assert(!BLI_table_gset_haskey(new_owner->bm_other_verts, v));

  /* mark node for update */
  new_owner->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;
}

static bool pbvh_bmesh_vert_relink(PBVH *pbvh, BMVert *v)
{
  const int cd_vert_node = pbvh->cd_vert_node_offset;
  const int cd_face_node = pbvh->cd_face_node_offset;

  BMFace *f;
  BLI_assert(BM_ELEM_CD_GET_INT(v, cd_vert_node) == DYNTOPO_NODE_NONE);

  bool added = false;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    const int ni = BM_ELEM_CD_GET_INT(f, cd_face_node);

    if (ni == DYNTOPO_NODE_NONE) {
      continue;
    }

    PBVHNode *node = pbvh->nodes + ni;

    if (BM_ELEM_CD_GET_INT(v, cd_vert_node) == DYNTOPO_NODE_NONE) {
      BLI_table_gset_add(node->bm_unique_verts, v);
      BM_ELEM_CD_SET_INT(v, cd_vert_node, ni);
    }
    else if (BM_ELEM_CD_GET_INT(v, cd_vert_node) != ni) {
      BLI_table_gset_add(node->bm_other_verts, v);
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return added;
}

static void pbvh_bmesh_vert_remove(PBVH *pbvh, BMVert *v)
{
  /* never match for first time */
  int f_node_index_prev = DYNTOPO_NODE_NONE;
  const int updateflag = PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris |
                         PBVH_UpdateNormals;

  PBVHNode *v_node = pbvh_bmesh_node_from_vert(pbvh, v);

  if (v_node) {
    BLI_table_gset_remove(v_node->bm_unique_verts, v, NULL);
    v_node->flag |= updateflag;
  }

  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

  /* Have to check each neighboring face's node */
  BMFace *f;
  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    const int f_node_index = pbvh_bmesh_node_index_from_face(pbvh, f);

    if (f_node_index == DYNTOPO_NODE_NONE) {
      continue;
    }

    /* faces often share the same node,
     * quick check to avoid redundant #BLI_table_gset_remove calls */
    if (f_node_index_prev != f_node_index) {
      f_node_index_prev = f_node_index;

      PBVHNode *f_node = &pbvh->nodes[f_node_index];
      f_node->flag |= updateflag;

      /* Remove current ownership */
      BLI_table_gset_remove(f_node->bm_other_verts, v, NULL);

      BLI_assert(!BLI_table_gset_haskey(f_node->bm_unique_verts, v));
      BLI_assert(!BLI_table_gset_haskey(f_node->bm_other_verts, v));
    }
  }
  BM_FACES_OF_VERT_ITER_END;
}

static void pbvh_bmesh_face_remove(
    PBVH *pbvh, BMFace *f, bool log_face, bool check_verts, bool ensure_ownership_transfer)
{
  PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);

  if (!f_node || !(f_node->flag & PBVH_Leaf)) {
    printf("pbvh corruption\n");
    fflush(stdout);
    return;
  }

  /* Check if any of this face's vertices need to be removed
   * from the node */
  if (check_verts) {
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BMVert *v = l_iter->v;
      if (pbvh_bmesh_node_vert_use_count_is_equal(pbvh, f_node, v, 1)) {
        if (BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset) == f_node - pbvh->nodes) {
          // if (BLI_table_gset_haskey(f_node->bm_unique_verts, v)) {
          /* Find a different node that uses 'v' */
          PBVHNode *new_node;

          new_node = pbvh_bmesh_vert_other_node_find(pbvh, v);
          // BLI_assert(new_node || BM_vert_face_count_is_equal(v, 1));

          if (new_node) {
            pbvh_bmesh_vert_ownership_transfer(pbvh, new_node, v);
          }
          else if (ensure_ownership_transfer && !BM_vert_face_count_is_equal(v, 1)) {
            pbvh_bmesh_vert_remove(pbvh, v);

            f_node->flag |= PBVH_RebuildNodeVerts;
            // printf("failed to find new_node\n");
          }
        }
        else {
          /* Remove from other verts */
          BLI_table_gset_remove(f_node->bm_other_verts, v, NULL);
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  /* Remove face from node and top level */
  BLI_table_gset_remove(f_node->bm_faces, f, NULL);
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

  /* Log removed face */
  if (log_face) {
    BM_log_face_removed(pbvh->bm_log, f);
  }

  /* mark node for update */
  f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateTris;
}

void BKE_pbvh_bmesh_remove_face(PBVH *pbvh, BMFace *f, bool log_face)
{
  pbvh_bmesh_face_remove(pbvh, f, log_face, true, true);
}

void BKE_pbvh_bmesh_remove_vertex(PBVH *pbvh, BMVert *v, bool log_vert)
{
  pbvh_bmesh_vert_remove(pbvh, v);

  if (log_vert) {
    BM_log_vert_removed(pbvh->bm_log, v, pbvh->cd_vert_mask_offset);
  }
}

void BKE_pbvh_bmesh_add_face(PBVH *pbvh, struct BMFace *f, bool log_face, bool force_tree_walk)
{
  int ni = -1;

  // look for node in surrounding geometry
  BMLoop *l = f->l_first;
  do {
    ni = BM_ELEM_CD_GET_INT(l->radial_next->f, pbvh->cd_face_node_offset);

    if (ni >= 0 && (!(pbvh->nodes[ni].flag & PBVH_Leaf) || ni >= pbvh->totnode)) {
      printf("EEK! ni: %d totnode: %d\n", ni, pbvh->totnode);
      l = l->next;
      continue;
    }

    if (ni >= 0 && (pbvh->nodes[ni].flag & PBVH_Leaf)) {
      break;
    }

    l = l->next;
  } while (l != f->l_first);

  if (ni < 0 || force_tree_walk) {
    bke_pbvh_insert_face(pbvh, f);
  }
  else {
    BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, ni);
    bke_pbvh_insert_face_finalize(pbvh, f, ni);
  }

  if (log_face) {
    BM_log_face_added(pbvh->bm_log, f);
  }
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

/****************************** EdgeQueue *****************************/

struct EdgeQueue;

typedef struct EdgeQueue {
  HeapSimple *heap;

  void **elems;
  int totelems;

  const float *center;
  float center_proj[3]; /* for when we use projected coords. */
  float radius_squared;
  float limit_len_squared;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  float limit_len;
#endif

  bool (*edge_queue_tri_in_range)(const struct EdgeQueue *q, BMFace *f);
  bool (*edge_queue_vert_in_range)(const struct EdgeQueue *q, BMVert *v);

  const float *view_normal;
#ifdef USE_EDGEQUEUE_FRONTFACE
  unsigned int use_view_normal : 1;
#endif
} EdgeQueue;

typedef struct {
  EdgeQueue *q;
  BLI_mempool *pool;
  BMesh *bm;
  DyntopoMaskCB mask_cb;
  void *mask_cb_data;
  int cd_dyn_vert;
  int cd_vert_mask_offset;
  int cd_vert_node_offset;
  int cd_face_node_offset;
  float avg_elen;
  float max_elen;
  float min_elen;
  float totedge;
} EdgeQueueContext;

BLI_INLINE float maskcb_get(EdgeQueueContext *eq_ctx, BMEdge *e)
{
  if (eq_ctx->mask_cb) {
    SculptVertRef sv1 = {(intptr_t)e->v1};
    SculptVertRef sv2 = {(intptr_t)e->v2};

    float w1 = eq_ctx->mask_cb(sv1, eq_ctx->mask_cb_data);
    float w2 = eq_ctx->mask_cb(sv2, eq_ctx->mask_cb_data);

    return (w1 + w2) * 0.5f;
  }

  return 1.0f;
}

BLI_INLINE float calc_weighted_edge_split(EdgeQueueContext *eq_ctx, BMVert *v1, BMVert *v2)
{
#ifdef FANCY_EDGE_WEIGHTS
  float l = len_squared_v3v3(v1->co, v2->co);
  float val = (float)BM_vert_edge_count(v1) + (float)BM_vert_edge_count(v2);
  val = MAX2(val * 0.5 - 6.0f, 1.0f);
  val = powf(val, 0.5);
  l *= val;

  return l;
#elif 0  // penalize 4-valence verts
  float l = len_squared_v3v3(v1->co, v2->co);
  if (BM_vert_edge_count(v1) == 4 || BM_vert_edge_count(v2) == 4) {
    l *= 0.25f;
  }

  return l;
#else
  return len_squared_v3v3(v1->co, v2->co);
#endif
}

BLI_INLINE float calc_weighted_edge_collapse(EdgeQueueContext *eq_ctx, BMVert *v1, BMVert *v2)
{
#ifdef FANCY_EDGE_WEIGHTS
  float l = len_squared_v3v3(v1->co, v2->co);
  float val = (float)BM_vert_edge_count(v1) + (float)BM_vert_edge_count(v2);
  val = MAX2(val * 0.5 - 6.0f, 1.0f);
  val = powf(val, 0.5);
  l /= val;

  // if (BM_vert_edge_count(v1) == 4 || BM_vert_edge_count(v2) == 4) {
  //  l *= 0.25f;
  //}

  return l;
#else
  return len_squared_v3v3(v1->co, v2->co);
#endif
}

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
static void pbvh_bmesh_edge_tag_verify(PBVH *pbvh)
{
  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];
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

static bool edge_queue_vert_in_sphere(const EdgeQueue *q, BMVert *v)
{
  /* Check if triangle intersects the sphere */
  return len_squared_v3v3(q->center, v->co) <= q->radius_squared;
}

/*
  Profiling revealed the accurate distance to tri in blenlib was too slow,
  so we use a simpler version here
  */
static float dist_to_tri_sphere_simple(
    float p[3], float v1[3], float v2[3], float v3[3], float n[3])
{
  float co[3];

  float dis = len_squared_v3v3(p, v1);
  dis = fmin(dis, len_squared_v3v3(p, v2));
  dis = fmin(dis, len_squared_v3v3(p, v3));

  add_v3_v3v3(co, v1, v2);
  mul_v3_fl(co, 0.5f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  add_v3_v3v3(co, v2, v3);
  mul_v3_fl(co, 0.5f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  add_v3_v3v3(co, v3, v1);
  mul_v3_fl(co, 0.5f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  add_v3_v3v3(co, v1, v2);
  add_v3_v3(co, v3);
  mul_v3_fl(co, 1.0f / 3.0f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  return dis;
}

static bool edge_queue_tri_in_sphere(const EdgeQueue *q, BMFace *f)
{
  BMLoop *l = f->l_first;

  /* Check if triangle intersects the sphere */
  float dis = dist_to_tri_sphere_simple((float *)q->center,
                                        (float *)l->v->co,
                                        (float *)l->next->v->co,
                                        (float *)l->prev->v->co,
                                        (float *)f->no);

  return dis <= q->radius_squared;
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

typedef struct EdgeQueueThreadData {
  PBVH *pbvh;
  PBVHNode *node;
  BMEdge **edges;
  EdgeQueueContext *eq_ctx;
  int totedge;
  int size;
} EdgeQueueThreadData;

static void edge_thread_data_insert(EdgeQueueThreadData *tdata, BMEdge *e)
{
  if (tdata->size <= tdata->totedge) {
    tdata->size = (tdata->totedge + 1) << 1;
    if (!tdata->edges) {
      tdata->edges = MEM_mallocN(sizeof(void *) * tdata->size, "edge_thread_data_insert");
    }
    else {
      tdata->edges = MEM_reallocN(tdata->edges, sizeof(void *) * tdata->size);
    }
  }

  e->head.hflag |= BM_ELEM_TAG;

  tdata->edges[tdata->totedge] = e;
  tdata->totedge++;
}

static bool edge_queue_vert_in_circle(const EdgeQueue *q, BMVert *v)
{
  float c[3];

  project_plane_normalized_v3_v3v3(c, v->co, q->view_normal);

  return len_squared_v3v3(q->center_proj, c) <= q->radius_squared;
}

static void edge_queue_insert(EdgeQueueContext *eq_ctx, BMEdge *e, float priority)
{
  void **elems = eq_ctx->q->elems;
  BLI_array_declare(elems);
  BLI_array_len_set(elems, eq_ctx->q->totelems);

  if (eq_ctx->cd_vert_mask_offset == -1 ||
      !((e->v1->head.hflag | e->v2->head.hflag) & BM_ELEM_HIDDEN)) {
    float dis = len_v3v3(e->v1->co, e->v2->co);
    eq_ctx->avg_elen += dis;
    eq_ctx->max_elen = MAX2(eq_ctx->max_elen, dis);
    eq_ctx->min_elen = MIN2(eq_ctx->min_elen, dis);
    eq_ctx->totedge += 1.0f;

    BMVert **pair = BLI_mempool_alloc(eq_ctx->pool);
    pair[0] = e->v1;
    pair[1] = e->v2;
#ifdef DYNTOPO_USE_HEAP
    BLI_heapsimple_insert(eq_ctx->q->heap, priority, pair);
#endif

    BLI_array_append(elems, pair);
    eq_ctx->q->elems = elems;
    eq_ctx->q->totelems = BLI_array_len(elems);

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
    const float w = maskcb_get(eq_ctx, e);
    const float len_sq = BM_edge_calc_length_squared(e) * w * w;

    if (len_sq > eq_ctx->q->limit_len_squared) {
      edge_queue_insert(eq_ctx, e, -len_sq);
    }
  }
}

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
static void long_edge_queue_edge_add_recursive(EdgeQueueContext *eq_ctx,
                                               BMLoop *l_edge,
                                               BMLoop *l_end,
                                               const float len_sq,
                                               float limit_len,
                                               int depth)
{
  BLI_assert(len_sq > square_f(limit_len));

#  ifdef USE_EDGEQUEUE_FRONTFACE
  if (depth > DEPTH_START_LIMIT && eq_ctx->q->use_view_normal) {
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
    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = square_f(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < (int)ARRAY_SIZE(l_adjacent); i++) {
        float len_sq_other = BM_edge_calc_length_squared(l_adjacent[i]->e);
        float w = maskcb_get(eq_ctx, l_adjacent[i]->e);

        len_sq_other *= w * w;

        if (len_sq_other > max_ff(len_sq_cmp, limit_len_sq)) {
          //                  edge_queue_insert(eq_ctx, l_adjacent[i]->e, -len_sq_other);
          long_edge_queue_edge_add_recursive(eq_ctx,
                                             l_adjacent[i]->radial_next,
                                             l_adjacent[i],
                                             len_sq_other,
                                             limit_len,
                                             depth + 1);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_end);
  }
}
#endif /* USE_EDGEQUEUE_EVEN_SUBDIV */

static void short_edge_queue_edge_add(EdgeQueueContext *eq_ctx, BMEdge *e)
{
#ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(e) == false)
#endif
  {
    const float len_sq = calc_weighted_edge_collapse(eq_ctx, e->v1, e->v2);
    if (len_sq < eq_ctx->q->limit_len_squared) {
      edge_queue_insert(eq_ctx, e, len_sq);
    }
  }
}

static void long_edge_queue_face_add(EdgeQueueContext *eq_ctx, BMFace *f, bool ignore_frontface)
{
#ifdef USE_EDGEQUEUE_FRONTFACE
  if (!ignore_frontface && eq_ctx->q->use_view_normal) {
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
      float len_sq = BM_edge_calc_length_squared(l_iter->e);
      float w = maskcb_get(eq_ctx, l_iter->e);

      len_sq *= w * w;

      if (len_sq > eq_ctx->q->limit_len_squared) {
        long_edge_queue_edge_add_recursive(eq_ctx,
                                           l_iter->radial_next,
                                           l_iter,
                                           len_sq,
                                           eq_ctx->q->limit_len,
                                           DEPTH_START_LIMIT +
                                               1);  // ignore_frontface ? 0 : DEPTH_START_LIMIT+1);
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

static void short_edge_queue_edge_add_recursive_2(EdgeQueueThreadData *tdata,
                                                  BMLoop *l_edge,
                                                  BMLoop *l_end,
                                                  const float len_sq,
                                                  float limit_len,
                                                  int depth)
{
  BLI_assert(len_sq > square_f(limit_len));

  if (l_edge->e->head.hflag & BM_ELEM_TAG) {
    return;
  }

#ifdef USE_EDGEQUEUE_FRONTFACE
  if (depth > DEPTH_START_LIMIT && tdata->eq_ctx->q->use_view_normal) {
    if (dot_v3v3(l_edge->f->no, tdata->eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  edge_thread_data_insert(tdata, l_edge->e);

  /* temp support previous behavior! */
  if (UNLIKELY(G.debug_value == 1234)) {
    return;
  }

  if ((l_edge->radial_next != l_edge)) {
    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = square_f(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < (int)ARRAY_SIZE(l_adjacent); i++) {

        float len_sq_other = calc_weighted_edge_collapse(
            tdata->eq_ctx, l_adjacent[i]->e->v1, l_adjacent[i]->e->v2);

        if (len_sq_other > max_ff(len_sq_cmp, limit_len_sq)) {
          //                  edge_queue_insert(eq_ctx, l_adjacent[i]->e, -len_sq_other);
          short_edge_queue_edge_add_recursive_2(tdata,
                                                l_adjacent[i]->radial_next,
                                                l_adjacent[i],
                                                len_sq_other,
                                                limit_len,
                                                depth + 1);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_end);
  }
}

static void long_edge_queue_edge_add_recursive_2(EdgeQueueThreadData *tdata,
                                                 BMLoop *l_edge,
                                                 BMLoop *l_end,
                                                 const float len_sq,
                                                 float limit_len,
                                                 int depth)
{
  BLI_assert(len_sq > square_f(limit_len));

  if (l_edge->e->head.hflag & BM_ELEM_TAG) {
    return;
  }

#ifdef USE_EDGEQUEUE_FRONTFACE
  if (depth > DEPTH_START_LIMIT && tdata->eq_ctx->q->use_view_normal) {
    if (dot_v3v3(l_edge->f->no, tdata->eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  edge_thread_data_insert(tdata, l_edge->e);

  /* temp support previous behavior! */
  if (UNLIKELY(G.debug_value == 1234)) {
    return;
  }

  if ((l_edge->radial_next != l_edge)) {
    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = square_f(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < (int)ARRAY_SIZE(l_adjacent); i++) {
        BMEdge *e = l_adjacent[i]->e;

        float len_sq_other = calc_weighted_edge_split(
            tdata->eq_ctx, l_adjacent[i]->e->v1, l_adjacent[i]->e->v2);

        float w = maskcb_get(tdata->eq_ctx, e);

        len_sq_other *= w * w;

        if (len_sq_other > max_ff(len_sq_cmp, limit_len_sq)) {
          long_edge_queue_edge_add_recursive_2(tdata,
                                               l_adjacent[i]->radial_next,
                                               l_adjacent[i],
                                               len_sq_other,
                                               limit_len,
                                               depth + 1);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_end);
  }
}

static void long_edge_queue_task_cb(void *__restrict userdata,
                                    const int n,
                                    const TaskParallelTLS *__restrict tls)
{
  EdgeQueueThreadData *tdata = ((EdgeQueueThreadData *)userdata) + n;
  PBVHNode *node = tdata->node;
  EdgeQueueContext *eq_ctx = tdata->eq_ctx;

  BMFace *f;
  const int cd_dyn_vert = tdata->pbvh->cd_dyn_vert;

  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;

    do {
      l->e->head.hflag &= ~BM_ELEM_TAG;
      l = l->next;
    } while (l != f->l_first);
  }
  TGSET_ITER_END

  TGSET_ITER (f, node->bm_faces) {
#ifdef USE_EDGEQUEUE_FRONTFACE
    if (eq_ctx->q->use_view_normal) {
      if (dot_v3v3(f->no, eq_ctx->q->view_normal) < 0.0f) {
        continue;
      }
    }
#endif

    if (eq_ctx->q->edge_queue_tri_in_range(eq_ctx->q, f)) {
      /* Check each edge of the face */
      BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
      BMLoop *l_iter = l_first;
      do {
        // try to improve convergence by applying a small amount of smoothing to topology,
        // but tangentially to surface.
        surface_smooth_v_safe(tdata->pbvh, l_iter->v);

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
        float w = maskcb_get(eq_ctx, l_iter->e);
        float len_sq = BM_edge_calc_length_squared(l_iter->e);

        len_sq *= w * w;

        if (len_sq > eq_ctx->q->limit_len_squared) {
          long_edge_queue_edge_add_recursive_2(
              tdata, l_iter->radial_next, l_iter, len_sq, eq_ctx->q->limit_len, 0);
        }
#else
        const float len_sq = BM_edge_calc_length_squared(l_iter->e);
        if (len_sq > eq_ctx->q->limit_len_squared) {
          edge_thread_data_insert(tdata, l_iter->e);
        }
#endif
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  TGSET_ITER_END
}

static void short_edge_queue_task_cb(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  EdgeQueueThreadData *tdata = ((EdgeQueueThreadData *)userdata) + n;
  PBVHNode *node = tdata->node;
  EdgeQueueContext *eq_ctx = tdata->eq_ctx;

  BMFace *f;

  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;

    do {
      l->e->head.hflag &= ~BM_ELEM_TAG;
      l = l->next;
    } while (l != f->l_first);
  }
  TGSET_ITER_END

  TGSET_ITER (f, node->bm_faces) {
#ifdef USE_EDGEQUEUE_FRONTFACE
    if (eq_ctx->q->use_view_normal) {
      if (dot_v3v3(f->no, eq_ctx->q->view_normal) < 0.0f) {
        continue;
      }
    }
#endif

    if (eq_ctx->q->edge_queue_tri_in_range(eq_ctx->q, f)) {
      /* Check each edge of the face */
      BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
      BMLoop *l_iter = l_first;
      do {
        float w = maskcb_get(eq_ctx, l_iter->e);

        if (w == 0.0f) {
          continue;
        }

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
        float len_sq = calc_weighted_edge_collapse(eq_ctx, l_iter->e->v1, l_iter->e->v2);
        len_sq /= w * w;

        if (len_sq < eq_ctx->q->limit_len_squared) {
          short_edge_queue_edge_add_recursive_2(
              tdata, l_iter->radial_next, l_iter, len_sq, eq_ctx->q->limit_len, 0);
        }
#else
        const float len_sq = calc_weighted_edge_split(eq_ctx, l_iter->e->v1, l_iter->e->v2);
        if (len_sq > eq_ctx->q->limit_len_squared) {
          edge_thread_data_insert(tdata, l_iter->e);
        }
#endif
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  TGSET_ITER_END
}

static bool check_face_is_tri(PBVH *pbvh, BMFace *f)
{
  if (f->len == 3) {
    return true;
  }

  if (f->len < 3) {
    printf("pbvh had < 3 vert face!\n");
    BKE_pbvh_bmesh_remove_face(pbvh, f, false);
    return false;
  }

  BMFace **fs = NULL;
  BMEdge **es = NULL;
  LinkNode *dbl = NULL;
  BLI_array_staticdeclare(fs, 32);
  BLI_array_staticdeclare(es, 32);

  // BKE_pbvh_bmesh_remove_face(pbvh, f, true);
  pbvh_bmesh_face_remove(pbvh, f, true, true, true);

  int len = (f->len - 2) * 3;

  BLI_array_grow_items(fs, len);
  BLI_array_grow_items(es, len);

  int totface = 0;
  int totedge = 0;
  MemArena *arena = NULL;
  struct Heap *heap = NULL;

  if (f->len > 4) {
    arena = BLI_memarena_new(512, "ngon arena");
    heap = BLI_heap_new();
  }

  BM_face_triangulate(pbvh->bm,
                      f,
                      fs,
                      &totface,
                      es,
                      &totedge,
                      &dbl,
                      MOD_TRIANGULATE_QUAD_FIXED,
                      MOD_TRIANGULATE_NGON_BEAUTY,
                      false,
                      arena,
                      heap);

  while (totface && dbl) {
    BMFace *f2 = dbl->link;
    LinkNode *next = dbl->next;

    for (int i = 0; i < totface; i++) {
      if (fs[i] == f2) {
        fs[i] = NULL;
      }
    }

    BM_face_kill(pbvh->bm, dbl->link);
    MEM_freeN(dbl);
    dbl = next;
  }

  for (int i = 0; i < totface; i++) {
    BMFace *f2 = fs[i];

    if (!f2) {
      continue;
    }

    if (f == f2) {
      printf("eek!\n");
      continue;
    }

    BKE_pbvh_bmesh_add_face(pbvh, f2, true, true);
  }

  BKE_pbvh_bmesh_add_face(pbvh, f, true, true);

  BLI_array_free(fs);
  BLI_array_free(es);

  if (arena) {
    BLI_memarena_free(arena);
  }

  if (heap) {
    BLI_heap_free(heap, NULL);
  }

  return false;
}

static bool check_vert_fan_are_tris(PBVH *pbvh, BMVert *v)
{
  BMFace **fs = NULL;
  BLI_array_staticdeclare(fs, 32);

  MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
  if (!(mv->flag & DYNVERT_NEED_TRIANGULATE)) {
    return true;
  }

  BMIter iter;
  BMFace *f;
  BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
    BLI_array_append(fs, f);
  }

  mv->flag &= ~DYNVERT_NEED_TRIANGULATE;

  for (int i = 0; i < BLI_array_len(fs); i++) {
    check_face_is_tri(pbvh, fs[i]);
  }

  BLI_array_free(fs);

  return false;
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
                                   PBVH *pbvh,
                                   const float center[3],
                                   const float view_normal[3],
                                   float radius,
                                   const bool use_frontface,
                                   const bool use_projected)
{
  eq_ctx->q->heap = BLI_heapsimple_new();
  eq_ctx->q->elems = NULL;
  eq_ctx->q->totelems = 0;
  eq_ctx->q->center = center;
  eq_ctx->q->radius_squared = radius * radius;
  eq_ctx->q->limit_len_squared = pbvh->bm_max_edge_len * pbvh->bm_max_edge_len;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  eq_ctx->q->limit_len = pbvh->bm_max_edge_len;
#endif

  eq_ctx->q->view_normal = view_normal;

#ifdef USE_EDGEQUEUE_FRONTFACE
  eq_ctx->q->use_view_normal = use_frontface;
#else
  UNUSED_VARS(use_frontface);
#endif

  if (use_projected) {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_circle;
    eq_ctx->q->edge_queue_vert_in_range = edge_queue_vert_in_circle;
    project_plane_normalized_v3_v3v3(eq_ctx->q->center_proj, center, view_normal);
  }
  else {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_sphere;
    eq_ctx->q->edge_queue_vert_in_range = edge_queue_vert_in_sphere;
  }

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(pbvh);
#endif

  EdgeQueueThreadData *tdata = NULL;
  BLI_array_declare(tdata);

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];

    /* Check leaf nodes marked for topology update */
    if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
        !(node->flag & PBVH_FullyHidden)) {
      EdgeQueueThreadData td;

      memset(&td, 0, sizeof(td));

      td.pbvh = pbvh;
      td.node = node;
      td.eq_ctx = eq_ctx;

      BLI_array_append(tdata, td);
      /* Check each face */
      /*
      BMFace *f;
      TGSET_ITER (f, node->bm_faces) {
        long_edge_queue_face_add(eq_ctx, f);
      }
      TGSET_ITER_END
      */
    }
  }

  int count = BLI_array_len(tdata);

  TaskParallelSettings settings;

  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, count, tdata, long_edge_queue_task_cb, &settings);

  for (int i = 0; i < count; i++) {
    EdgeQueueThreadData *td = tdata + i;

    BMEdge **edges = td->edges;
    for (int j = 0; j < td->totedge; j++) {
      BMEdge *e = edges[j];
      e->head.hflag &= ~BM_ELEM_TAG;

      float w = -calc_weighted_edge_split(eq_ctx, e->v1, e->v2);
      float w2 = maskcb_get(eq_ctx, e);

      w *= w2 * w2;

      edge_queue_insert(eq_ctx, e, w);
    }

    if (td->edges) {
      MEM_freeN(td->edges);
    }
  }
  BLI_array_free(tdata);
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
                                    PBVH *pbvh,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    const bool use_frontface,
                                    const bool use_projected)
{
  eq_ctx->q->heap = BLI_heapsimple_new();
  eq_ctx->q->elems = NULL;
  eq_ctx->q->totelems = 0;
  eq_ctx->q->center = center;
  eq_ctx->q->radius_squared = radius * radius;
  eq_ctx->q->limit_len_squared = pbvh->bm_min_edge_len * pbvh->bm_min_edge_len;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  eq_ctx->q->limit_len = pbvh->bm_min_edge_len;
#endif

  eq_ctx->q->view_normal = view_normal;

#ifdef USE_EDGEQUEUE_FRONTFACE
  eq_ctx->q->use_view_normal = use_frontface;
#else
  UNUSED_VARS(use_frontface);
#endif

  if (use_projected) {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_circle;
    eq_ctx->q->edge_queue_vert_in_range = edge_queue_vert_in_circle;
    project_plane_normalized_v3_v3v3(eq_ctx->q->center_proj, center, view_normal);
  }
  else {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_sphere;
    eq_ctx->q->edge_queue_vert_in_range = edge_queue_vert_in_sphere;
  }

  EdgeQueueThreadData *tdata = NULL;
  BLI_array_declare(tdata);

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];
    EdgeQueueThreadData td;

    if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
        !(node->flag & PBVH_FullyHidden)) {
      memset(&td, 0, sizeof(td));
      td.pbvh = pbvh;
      td.node = node;
      td.eq_ctx = eq_ctx;

      BLI_array_append(tdata, td);
    }

#if 0
    /* Check leaf nodes marked for topology update */
      BMFace *f;

      /* Check each face */
      TGSET_ITER (f, node->bm_faces) {
        short_edge_queue_face_add(eq_ctx, f);
      }
      TGSET_ITER_END
    }
#endif
  }

  int count = BLI_array_len(tdata);

  TaskParallelSettings settings;

  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, count, tdata, short_edge_queue_task_cb, &settings);

  const int cd_dyn_vert = pbvh->cd_dyn_vert;

  for (int i = 0; i < count; i++) {
    EdgeQueueThreadData *td = tdata + i;

    BMEdge **edges = td->edges;
    for (int j = 0; j < td->totedge; j++) {
      BMEdge *e = edges[j];
      MDynTopoVert *mv1, *mv2;

      mv1 = BKE_PBVH_DYNVERT(cd_dyn_vert, e->v1);
      mv2 = BKE_PBVH_DYNVERT(cd_dyn_vert, e->v2);

      pbvh_check_vert_boundary(pbvh, e->v1);
      pbvh_check_vert_boundary(pbvh, e->v2);

      if ((mv1->flag & DYNVERT_ALL_BOUNDARY) != (mv2->flag & DYNVERT_ALL_BOUNDARY)) {
        continue;
      }

      float w = calc_weighted_edge_collapse(eq_ctx, e->v1, e->v2);
      float w2 = maskcb_get(eq_ctx, e);

      if (w2 > 0.0f) {
        w /= w2 * w2;
      }
      else {
        w = 100000.0f;
      }

      e->head.hflag &= ~BM_ELEM_TAG;
      edge_queue_insert(eq_ctx, e, w);
    }

    if (td->edges) {
      MEM_freeN(td->edges);
    }
  }

  BLI_array_free(tdata);
}

/*************************** Topology update **************************/

static void pbvh_bmesh_split_edge(EdgeQueueContext *eq_ctx,
                                  PBVH *pbvh,
                                  BMEdge *e,
                                  BLI_Buffer *edge_loops)
{
  BMesh *bm = pbvh->bm;

  // pbvh_bmesh_check_nodes(pbvh);

  check_vert_fan_are_tris(pbvh, e->v1);
  check_vert_fan_are_tris(pbvh, e->v2);

  // pbvh_bmesh_check_nodes(pbvh);

  float co_mid[3], no_mid[3];
  MDynTopoVert *mv1 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v1);
  MDynTopoVert *mv2 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v2);

  pbvh_check_vert_boundary(pbvh, e->v1);
  pbvh_check_vert_boundary(pbvh, e->v2);

  bool boundary = (mv1->flag & DYNVERT_ALL_BOUNDARY) && (mv2->flag & DYNVERT_ALL_BOUNDARY);

  /* Get all faces adjacent to the edge */
  pbvh_bmesh_edge_loops(edge_loops, e);

  /* Create a new vertex in current node at the edge's midpoint */
  mid_v3_v3v3(co_mid, e->v1->co, e->v2->co);
  mid_v3_v3v3(no_mid, e->v1->no, e->v2->no);
  normalize_v3(no_mid);

  int node_index = BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset);
  BMVert *v_new = pbvh_bmesh_vert_create(
      pbvh, node_index, co_mid, no_mid, NULL, eq_ctx->cd_vert_mask_offset);
  // transfer edge flags

  BMEdge *e1 = BM_edge_create(pbvh->bm, e->v1, v_new, e, BM_CREATE_NOP);
  BMEdge *e2 = BM_edge_create(pbvh->bm, v_new, e->v2, e, BM_CREATE_NOP);

  int eflag = e->head.hflag & ~BM_ELEM_HIDDEN;
  int vflag = (e->v1->head.hflag | e->v2->head.hflag) & ~BM_ELEM_HIDDEN;

  e1->head.hflag = e2->head.hflag = eflag;
  v_new->head.hflag = vflag;

  /*TODO: is it worth interpolating edge customdata?*/

  int ni_new = BM_ELEM_CD_GET_INT(v_new, pbvh->cd_vert_node_offset);

  void *vsrcs[2] = {e->v1->head.data, e->v2->head.data};
  float vws[2] = {0.5f, 0.5f};
  CustomData_bmesh_interp(
      &pbvh->bm->vdata, (const void **)vsrcs, (float *)vws, NULL, 2, v_new->head.data);

  int ni_new2 = BM_ELEM_CD_GET_INT(v_new, pbvh->cd_vert_node_offset);
  if (ni_new2 != ni_new) {
    // printf("error!\n");
    BM_ELEM_CD_SET_INT(v_new, pbvh->cd_vert_node_offset, ni_new);
  }

  /* For each face, add two new triangles and delete the original */
  for (int i = 0; i < (int)edge_loops->count; i++) {
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
      pbvh_bmesh_vert_ownership_transfer(pbvh, &pbvh->nodes[ni], v_new);
    }

    /**
     * The 2 new faces created and assigned to `f_new` have their
     * verts & edges shuffled around.
     *
     * - faces wind anticlockwise in this example.
     * - original edge is `(v1, v2)`
     * - original face is `(v1, v2, v3)`
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
     * - f_new (first):  `v_tri=(v1, v4, v3), e_tri=(e1, e5, e4)`
     * - f_new (second): `v_tri=(v4, v2, v3), e_tri=(e2, e3, e5)`
     */

    /* Create two new faces */
    v_tri[0] = v1;
    v_tri[1] = v_new;
    v_tri[2] = v_opp;
    bm_edges_from_tri(pbvh->bm, v_tri, e_tri);
    f_new = pbvh_bmesh_face_create(pbvh, ni, v_tri, e_tri, f_adj, false, true);
    long_edge_queue_face_add(eq_ctx, f_new, true);

    pbvh_bmesh_copy_facedata(bm, f_new, f_adj);

    // customdata interpolation
    BMLoop *lfirst = f_adj->l_first;
    while (lfirst->v != v1) {
      lfirst = lfirst->next;

      // paranoia check
      if (lfirst == f_adj->l_first) {
        break;
      }
    }

    BMLoop *l1 = lfirst;
    BMLoop *l2 = lfirst->next;
    BMLoop *l3 = lfirst->next->next;

    void *lsrcs[2] = {l1->head.data, l2->head.data};
    float lws[2] = {0.5f, 0.5f};

    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)lsrcs, lws, lws, 2, f_new->l_first->next->head.data);

    lsrcs[0] = l1->head.data;
    lws[0] = 1.0f;

    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)lsrcs, lws, lws, 1, f_new->l_first->head.data);

    lsrcs[0] = l3->head.data;
    lws[0] = 1.0f;

    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)lsrcs, lws, lws, 1, f_new->l_first->prev->head.data);

    v_tri[0] = v_new;
    v_tri[1] = v2;
    /* v_tri[2] = v_opp; */ /* unchanged */
    e_tri[0] = BM_edge_create(pbvh->bm, v_tri[0], v_tri[1], NULL, BM_CREATE_NO_DOUBLE);
    e_tri[2] = e_tri[1]; /* switched */
    e_tri[1] = BM_edge_create(pbvh->bm, v_tri[1], v_tri[2], NULL, BM_CREATE_NO_DOUBLE);

    f_new = pbvh_bmesh_face_create(pbvh, ni, v_tri, e_tri, f_adj, false, true);
    long_edge_queue_face_add(eq_ctx, f_new, true);

    pbvh_bmesh_copy_facedata(bm, f_new, f_adj);

    // customdata interpolation
    lsrcs[0] = lfirst->head.data;
    lsrcs[1] = lfirst->next->head.data;
    lws[0] = lws[1] = 0.5f;

    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)lsrcs, lws, lws, 2, f_new->l_first->head.data);

    lsrcs[0] = lfirst->next->head.data;
    ;
    lws[0] = 1.0f;

    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)lsrcs, lws, lws, 1, f_new->l_first->next->head.data);

    lsrcs[0] = lfirst->prev->head.data;
    lws[0] = 1.0f;

    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)lsrcs, lws, lws, 1, f_new->l_first->prev->head.data);

    /* Delete original */
    pbvh_bmesh_face_remove(pbvh, f_adj, true, true, true);
    BM_face_kill(pbvh->bm, f_adj);

    /* Ensure new vertex is in the node */
    if (!BLI_table_gset_haskey(pbvh->nodes[ni].bm_unique_verts, v_new)) {
      BLI_table_gset_add(pbvh->nodes[ni].bm_other_verts, v_new);
    }
  }

  BM_edge_kill(pbvh->bm, e);

  // pbvh_bmesh_check_nodes(pbvh);

  MDynTopoVert *mv_new = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v_new);
  bke_pbvh_update_vert_boundary(pbvh->cd_dyn_vert, pbvh->cd_faceset_offset, v_new);
  mv_new->flag |= DYNVERT_NEED_DISK_SORT;
}

static bool pbvh_bmesh_subdivide_long_edges(EdgeQueueContext *eq_ctx,
                                            PBVH *pbvh,
                                            BLI_Buffer *edge_loops,
                                            int max_steps)
{
  bool any_subdivided = false;
  double time = PIL_check_seconds_timer();

  RNG *rng = BLI_rng_new((int)(time * 1000.0f));
  int step = 0;

  while (!BLI_heapsimple_is_empty(eq_ctx->q->heap)) {
    if (step++ > max_steps) {
      break;
    }

#ifdef DYNTOPO_TIME_LIMIT
    if (PIL_check_seconds_timer() - time > DYNTOPO_TIME_LIMIT) {
      break;
    }
#endif

#ifndef DYNTOPO_USE_HEAP
    if (eq_ctx->q->totelems == 0) {
      break;
    }

    int ri = BLI_rng_get_int(rng) % eq_ctx->q->totelems;

    BMVert **pair = eq_ctx->q->elems[ri];
    eq_ctx->q->elems[ri] = eq_ctx->q->elems[eq_ctx->q->totelems - 1];
    eq_ctx->q->totelems--;
#else
    BMVert **pair = BLI_heapsimple_pop_min(eq_ctx->q->heap);
#endif
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
    // BLI_assert(calc_weighted_edge_split(eq_ctx, v1->co, v2->co) > eq_ctx->q->limit_len_squared);
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

    pbvh_bmesh_split_edge(eq_ctx, pbvh, e, edge_loops);
  }

#if !defined(DYNTOPO_USE_HEAP) && defined(USE_EDGEQUEUE_TAG)
  for (int i = 0; i < eq_ctx->q->totelems; i++) {
    BMVert **pair = eq_ctx->q->elems[i];
    BMVert *v1 = pair[0], *v2 = pair[1];

    BMEdge *e = BM_edge_exists(v1, v2);

    if (e) {
      EDGE_QUEUE_DISABLE(e);
    }
  }
#endif

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(pbvh);
#endif

  BLI_rng_free(rng);

  return any_subdivided;
}

static void pbvh_bmesh_collapse_edge(PBVH *pbvh,
                                     BMEdge *e,
                                     BMVert *v1,
                                     BMVert *v2,
                                     GHash *deleted_verts,
                                     BLI_Buffer *deleted_faces,
                                     EdgeQueueContext *eq_ctx)
{
  BMVert *v_del, *v_conn;

  check_vert_fan_are_tris(pbvh, e->v1);
  check_vert_fan_are_tris(pbvh, e->v2);

  // customdata interpolation
  if (BM_elem_flag_test(e, BM_ELEM_SEAM)) {
    for (int step = 0; step < 2; step++) {
      int count = 0;
      BMVert *v = step ? v2 : v1;
      BMIter iter;
      BMEdge *e2;

      BM_ITER_ELEM (e2, &iter, v, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(e2, BM_ELEM_SEAM)) {
          count++;
        }
      }

      if (count < 2) {
        return;
      }
    }
  }

  /* one of the two vertices may be masked, select the correct one for deletion */
  if (DYNTOPO_MASK(eq_ctx->cd_vert_mask_offset, v1) <
      DYNTOPO_MASK(eq_ctx->cd_vert_mask_offset, v2)) {
    v_del = v1;
    v_conn = v2;
  }
  else {
    v_del = v2;
    v_conn = v1;
  }

  /* Remove the merge vertex from the PBVH */
  pbvh_bmesh_vert_remove(pbvh, v_del);

  /* Remove all faces adjacent to the edge */
  BMLoop *l_adj;
  while ((l_adj = e->l)) {
    BMFace *f_adj = l_adj->f;

    int eflag = 0;

    // propegate flags to merged edges
    BMLoop *l = f_adj->l_first;
    do {
      BMEdge *e2 = l->e;

      if (e2 != e) {
        eflag |= e2->head.hflag & ~BM_ELEM_HIDDEN;
      }

      l = l->next;
    } while (l != f_adj->l_first);

    do {
      BMEdge *e2 = l->e;
      e2->head.hflag |= eflag;

      l = l->next;
    } while (l != f_adj->l_first);

    pbvh_bmesh_face_remove(pbvh, f_adj, true, true, true);
    BM_face_kill(pbvh->bm, f_adj);
  }

  /* Kill the edge */
  BLI_assert(BM_edge_is_wire(e));
  BM_edge_kill(pbvh->bm, e);

  /* For all remaining faces of v_del, create a new face that is the
   * same except it uses v_conn instead of v_del */
  /* NOTE: this could be done with BM_vert_splice(), but that
   * requires handling other issues like duplicate edges, so doesn't
   * really buy anything. */
  BLI_buffer_clear(deleted_faces);

  BMLoop *l = NULL;
  BMLoop **ls = NULL;
  void **blocks = NULL;
  float *ws = NULL;

  BLI_array_staticdeclare(ls, 64);
  BLI_array_staticdeclare(blocks, 64);
  BLI_array_staticdeclare(ws, 64);

  int totl = 0;

  BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_del) {
    BLI_array_append(ls, l);
    totl++;
  }
  BM_LOOPS_OF_VERT_ITER_END;

  BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_conn) {
    BLI_array_append(ls, l);
    totl++;
  }
  BM_LOOPS_OF_VERT_ITER_END;

  float w = totl > 0 ? 1.0f / (float)(totl) : 1.0f;

  for (int i = 0; i < totl; i++) {
    BLI_array_append(blocks, ls[i]->head.data);
    BLI_array_append(ws, w);
  }

  // snap customdata
  if (totl > 0) {
    CustomData_bmesh_interp(
        &pbvh->bm->ldata, (const void **)blocks, ws, NULL, totl, ls[0]->head.data);
    //*
    BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_del) {
      BMLoop *l2 = l->v != v_del ? l->next : l;

      if (l2 == ls[0]) {
        continue;
      }

      CustomData_bmesh_copy_data(
          &pbvh->bm->ldata, &pbvh->bm->ldata, ls[0]->head.data, &l2->head.data);
    }
    BM_LOOPS_OF_VERT_ITER_END;

    BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_conn) {
      BMLoop *l2 = l->v != v_conn ? l->next : l;

      if (l2 == ls[0]) {
        continue;
      }

      CustomData_bmesh_copy_data(
          &pbvh->bm->ldata, &pbvh->bm->ldata, ls[0]->head.data, &l2->head.data);
    }
    BM_LOOPS_OF_VERT_ITER_END;
    //*/
  }

  BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_del) {
    BMFace *existing_face;

    /* Get vertices, replace use of v_del with v_conn */
    // BM_iter_as_array(NULL, BM_VERTS_OF_FACE, f, (void **)v_tri, 3);
    BMFace *f = l->f;

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
      bool ok = true;

      // check we're not already in deleted_faces
      for (int i = 0; i < (int)deleted_faces->count; i++) {
        if (BLI_buffer_at(deleted_faces, BMFace *, i) == existing_face) {
          ok = false;
          break;
        }
      }

      if (ok) {
        BLI_buffer_append(deleted_faces, BMFace *, existing_face);
      }
    }
    else
    {
      BMVert *v_tri[3] = {v_conn, l->next->v, l->prev->v};

      BLI_assert(!BM_face_exists(v_tri, 3));
      BMEdge *e_tri[3];
      PBVHNode *n = pbvh_bmesh_node_from_face(pbvh, f);
      int ni = n - pbvh->nodes;
      bm_edges_from_tri(pbvh->bm, v_tri, e_tri);
      BMFace *f2 = pbvh_bmesh_face_create(pbvh, ni, v_tri, e_tri, f, false, true);

      BMLoop *l2 = f2->l_first;

      // sync edge flags
      l2->e->head.hflag |= (l->e->head.hflag & ~BM_ELEM_HIDDEN);
      // l2->prev->e->head.hflag |= (l->prev->e->head.hflag & ~BM_ELEM_HIDDEN);

      pbvh_bmesh_copy_facedata(pbvh->bm, f2, f);

      CustomData_bmesh_copy_data(&pbvh->bm->ldata, &pbvh->bm->ldata, l->head.data, &l2->head.data);
      CustomData_bmesh_copy_data(
          &pbvh->bm->ldata, &pbvh->bm->ldata, l->next->head.data, &l2->next->head.data);
      CustomData_bmesh_copy_data(
          &pbvh->bm->ldata, &pbvh->bm->ldata, l->prev->head.data, &l2->prev->head.data);

      /* Ensure that v_conn is in the new face's node */
      if (!BLI_table_gset_haskey(n->bm_unique_verts, v_conn)) {
        BLI_table_gset_add(n->bm_other_verts, v_conn);
      }
    }

    BLI_buffer_append(deleted_faces, BMFace *, f);
  }
  BM_LOOPS_OF_VERT_ITER_END;

  /* Delete the tagged faces */
  for (int i = 0; i < (int)deleted_faces->count; i++) {
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

    BMLoop *l1 = f_del->l_first;
    do {
      if (!l1->e) {
        printf("bmesh error!\n");
        l1->e = BM_edge_exists(l1->v, l1->next->v);
        if (!l1->e) {
          // create
          l1->e = BM_edge_create(pbvh->bm, l1->v, l1->next->v, NULL, 0);
        }
      }
      l1 = l1->next;
    } while (l1 != f_del->l_first);

    /* Remove the face */
    pbvh_bmesh_face_remove(pbvh, f_del, true, true, true);
    BM_face_kill(pbvh->bm, f_del);

    /* Check if any of the face's edges are now unused by any
     * face, if so delete them */
    for (int j = 0; j < 3; j++) {
      if (BM_edge_is_wire(e_tri[j])) {
        BM_edge_kill(pbvh->bm, e_tri[j]);
      }
    }

    /* Check if any of the face's vertices are now unused, if so
     * remove them from the PBVH */
    for (int j = 0; j < 3; j++) {
      if ((v_tri[j] != v_del) && (v_tri[j]->e == NULL)) {
        pbvh_bmesh_vert_remove(pbvh, v_tri[j]);

        BM_log_vert_removed(pbvh->bm_log, v_tri[j], eq_ctx->cd_vert_mask_offset);

        if (v_tri[j] == v_conn) {
          v_conn = NULL;
        }
        BLI_ghash_insert(deleted_verts, v_tri[j], NULL);
        BM_vert_kill(pbvh->bm, v_tri[j]);
      }
    }
  }

  /* Move v_conn to the midpoint of v_conn and v_del (if v_conn still exists, it
   * may have been deleted above) */
  if (v_conn != NULL) {
    // log vert in bmlog, but don't update original customata layers, we want them to be
    // interpolated
    BM_log_vert_before_modified(pbvh->bm_log, v_conn, eq_ctx->cd_vert_mask_offset, false);

    mid_v3_v3v3(v_conn->co, v_conn->co, v_del->co);
    add_v3_v3(v_conn->no, v_del->no);
    normalize_v3(v_conn->no);

    /* update boundboxes attached to the connected vertex
     * note that we can often get-away without this but causes T48779 */
    BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_conn) {
      PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, l->f);
      f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateBB |
                      PBVH_UpdateTris;
    }
    BM_LOOPS_OF_VERT_ITER_END;

    MDynTopoVert *mv_conn = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v_conn);

    bke_pbvh_update_vert_boundary(pbvh->cd_dyn_vert, pbvh->cd_faceset_offset, v_conn);
    mv_conn->flag |= DYNVERT_NEED_DISK_SORT;
  }

  /* Delete v_del */
  BLI_assert(!BM_vert_face_check(v_del));
  BM_log_vert_removed(pbvh->bm_log, v_del, eq_ctx->cd_vert_mask_offset);
  /* v_conn == NULL is OK */
  BLI_ghash_insert(deleted_verts, v_del, v_conn);
  BM_vert_kill(pbvh->bm, v_del);

  BLI_array_free(ws);
  BLI_array_free(blocks);
  BLI_array_free(ls);
}

static bool pbvh_bmesh_collapse_short_edges(EdgeQueueContext *eq_ctx,
                                            PBVH *pbvh,
                                            BLI_Buffer *deleted_faces,
                                            int max_steps)
{
  const float min_len_squared = pbvh->bm_min_edge_len * pbvh->bm_min_edge_len;
  bool any_collapsed = false;
  /* deleted verts point to vertices they were merged into, or NULL when removed. */
  GHash *deleted_verts = BLI_ghash_ptr_new("deleted_verts");

  double time = PIL_check_seconds_timer();
  RNG *rng = BLI_rng_new((unsigned int)(time * 1000.0f));

//#define TEST_COLLAPSE
#ifdef TEST_COLLAPSE
  int _i = 0;
#endif

  int step = 0;

  while (!BLI_heapsimple_is_empty(eq_ctx->q->heap)) {
    if (step++ > max_steps) {
      break;
    }
#ifdef DYNTOPO_TIME_LIMIT
    if (PIL_check_seconds_timer() - time > DYNTOPO_TIME_LIMIT) {
      break;
    }
#endif

#ifndef DYNTOPO_USE_HEAP
    if (eq_ctx->q->totelems == 0) {
      break;
    }

    int ri = BLI_rng_get_int(rng) % eq_ctx->q->totelems;

    BMVert **pair = eq_ctx->q->elems[ri];
    eq_ctx->q->elems[ri] = eq_ctx->q->elems[eq_ctx->q->totelems - 1];
    eq_ctx->q->totelems--;
#else
    BMVert **pair = BLI_heapsimple_pop_min(eq_ctx->q->heap);
#endif
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

    /* Also ignore non-manifold edges */
    if (e->l && e->l != e->l->radial_next->radial_next) {
      continue;
    }

#ifdef USE_EDGEQUEUE_TAG
    EDGE_QUEUE_DISABLE(e);
#endif

    if (calc_weighted_edge_collapse(eq_ctx, v1, v2) >= min_len_squared) {
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

    pbvh_bmesh_collapse_edge(pbvh, e, v1, v2, deleted_verts, deleted_faces, eq_ctx);

#ifdef TEST_COLLAPSE
    if (_i++ > 10) {
      break;
    }
#endif
  }

#if !defined(DYNTOPO_USE_HEAP) && defined(USE_EDGEQUEUE_TAG)
  for (int i = 0; i < eq_ctx->q->totelems; i++) {
    BMVert **pair = eq_ctx->q->elems[i];
    BMVert *v1 = pair[0], *v2 = pair[1];

    /* Check the verts still exist */
    if (!(v1 = bm_vert_hash_lookup_chain(deleted_verts, v1)) ||
        !(v2 = bm_vert_hash_lookup_chain(deleted_verts, v2)) || (v1 == v2)) {
      continue;
    }

    BMEdge *e = BM_edge_exists(v1, v2);
    if (e) {
      EDGE_QUEUE_DISABLE(e);
    }
  }
#endif
  BLI_rng_free(rng);
  BLI_ghash_free(deleted_verts, NULL, NULL);

  return any_collapsed;
}

// need to file a CLANG bug, getting weird behavior here
#ifdef __clang__
__attribute__((optnone))
#endif

static bool
cleanup_valence_3_4(PBVH *pbvh,
                    const float center[3],
                    const float view_normal[3],
                    float radius,
                    const bool use_frontface,
                    const bool use_projected)
{
  bool modified = false;

  float radius2 = radius * 1.25;
  float rsqr = radius2 * radius2;

  GSet *vset = BLI_gset_ptr_new("vset");

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = pbvh->nodes + n;

    /* Check leaf nodes marked for topology update */
    bool ok = (node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology);
    ok = ok && !(node->flag & (PBVH_FullyHidden | PBVH_Delete));

    if (!ok) {
      continue;
    }

    BMVert *v;

    TGSET_ITER (v, node->bm_unique_verts) {
      if (len_squared_v3v3(v->co, center) >= rsqr || !v->e) {
        continue;
      }

      check_vert_fan_are_tris(pbvh, v);

      const int val = BM_vert_edge_count(v);
      if (val != 4 && val != 3) {
        continue;
      }

      MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);

      pbvh_check_vert_boundary(pbvh, v);

      if (mv->flag & DYNVERT_ALL_BOUNDARY) {
        continue;
      }

      BMIter iter;
      BMLoop *l;
      BMLoop *ls[4];
      BMVert *vs[4];

      l = v->e->l;

      if (!l) {
        continue;
      }

      if (l->v != v) {
        l = l->next;
      }

      bool bad = false;
      int i = 0;

      for (int j = 0; j < val; j++) {
        ls[i++] = l->v == v ? l->next : l;

        l = l->prev->radial_next;

        if (l->v != v) {
          l = l->next;
        }

        /*ignore non-manifold edges*/

        if (l->radial_next == l || l->radial_next->radial_next != l) {
          bad = true;
          break;
        }

        for (int k = 0; k < j; k++) {
          if (ls[k]->v == ls[j]->v) {
            if (ls[j]->next->v != v) {
              ls[j] = ls[j]->next;
            }
            else {
              bad = true;
              break;
            }
          }

          // check for non-manifold edges
          if (ls[k] != ls[k]->radial_next->radial_next) {
            bad = true;
            break;
          }

          if (ls[k]->f == ls[j]->f) {
            bad = true;
            break;
          }
        }
      }

      if (bad) {
        continue;
      }

      int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

      if (ni >= 0 && BLI_table_gset_haskey(pbvh->nodes[ni].bm_other_verts, v)) {
        printf("error! %d\n", (int)BLI_table_gset_haskey(pbvh->nodes[ni].bm_unique_verts, v));
        BLI_table_gset_remove(pbvh->nodes[ni].bm_other_verts, v, NULL);
      }
      else if (ni < 0) {
        printf("error!\n");

        // attempt to recover

        BMFace *f;
        BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
          int ni2 = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);

          if (ni2 != DYNTOPO_NODE_NONE) {
            PBVHNode *node2 = pbvh->nodes + ni2;

            BLI_table_gset_remove(node2->bm_unique_verts, v, NULL);
            BLI_table_gset_remove(node2->bm_other_verts, v, NULL);
          }
        }
      }

      BM_log_vert_removed(pbvh->bm_log, v, pbvh->cd_vert_mask_offset);
      pbvh_bmesh_vert_remove(pbvh, v);

      BMFace *f;
      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        int ni2 = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);

        if (ni2 != DYNTOPO_NODE_NONE) {
          PBVHNode *node2 = pbvh->nodes + ni2;

          // BLI_table_gset_remove(node2->bm_unique_verts, v, NULL);
          // BLI_table_gset_remove(node2->bm_other_verts, v, NULL);

          pbvh_bmesh_face_remove(pbvh, f, true, true, true);
        }
      }

      modified = true;

      if (!v->e) {
        printf("mesh error!\n");
        continue;
      }

      l = v->e->l;

      bool flipped = false;

      if (val == 4) {
        // check which quad diagonal to use to split quad
        // try to preserve hard edges

        float n1[3], n2[3], th1, th2;
        normal_tri_v3(n1, ls[0]->v->co, ls[1]->v->co, ls[2]->v->co);
        normal_tri_v3(n2, ls[0]->v->co, ls[2]->v->co, ls[3]->v->co);

        th1 = dot_v3v3(n1, n2);

        normal_tri_v3(n1, ls[1]->v->co, ls[2]->v->co, ls[3]->v->co);
        normal_tri_v3(n2, ls[1]->v->co, ls[3]->v->co, ls[0]->v->co);

        th2 = dot_v3v3(n1, n2);

        if (th1 > th2) {
          flipped = true;
          BMLoop *ls2[4] = {ls[0], ls[1], ls[2], ls[3]};

          for (int j = 0; j < 4; j++) {
            ls[j] = ls2[(j + 1) % 4];
          }
        }
      }

      vs[0] = ls[0]->v;
      vs[1] = ls[1]->v;
      vs[2] = ls[2]->v;

      BMFace *f1 = NULL;
      if (vs[0] != vs[1] && vs[1] != vs[2] && vs[0] != vs[2]) {
        f1 = pbvh_bmesh_face_create(pbvh, n, vs, NULL, l->f, true, false);
        normal_tri_v3(
            f1->no, f1->l_first->v->co, f1->l_first->next->v->co, f1->l_first->prev->v->co);
      }
      else {
        // printf("eek1!\n");
      }

      if (val == 4 && vs[0] != vs[2] && vs[2] != vs[3] && vs[0] != vs[3]) {
        vs[0] = ls[0]->v;
        vs[1] = ls[2]->v;
        vs[2] = ls[3]->v;

        BMFace *example = NULL;
        if (v->e && v->e->l) {
          example = v->e->l->f;
        }

        BMFace *f2 = pbvh_bmesh_face_create(pbvh, n, vs, NULL, example, true, false);

        CustomData_bmesh_swap_data_simple(
            &pbvh->bm->ldata, &f2->l_first->prev->head.data, &ls[3]->head.data);
        CustomData_bmesh_copy_data(
            &pbvh->bm->ldata, &pbvh->bm->ldata, ls[0]->head.data, &f2->l_first->head.data);
        CustomData_bmesh_copy_data(
            &pbvh->bm->ldata, &pbvh->bm->ldata, ls[2]->head.data, &f2->l_first->next->head.data);

        normal_tri_v3(
            f2->no, f2->l_first->v->co, f2->l_first->next->v->co, f2->l_first->prev->v->co);
        BM_log_face_added(pbvh->bm_log, f2);
      }

      if (f1) {
        CustomData_bmesh_swap_data_simple(
            &pbvh->bm->ldata, &f1->l_first->head.data, &ls[0]->head.data);
        CustomData_bmesh_swap_data_simple(
            &pbvh->bm->ldata, &f1->l_first->next->head.data, &ls[1]->head.data);
        CustomData_bmesh_swap_data_simple(
            &pbvh->bm->ldata, &f1->l_first->prev->head.data, &ls[2]->head.data);

        BM_log_face_added(pbvh->bm_log, f1);
      }

      BM_vert_kill(pbvh->bm, v);
    }
    TGSET_ITER_END
  }

  BLI_gset_free(vset, NULL);

  if (modified) {
    pbvh->bm->elem_index_dirty |= BM_VERT | BM_FACE | BM_EDGE;
    pbvh->bm->elem_table_dirty |= BM_VERT | BM_FACE | BM_EDGE;
  }

  return modified;
}

/* Collapse short edges, subdivide long edges */
bool BKE_pbvh_bmesh_update_topology(PBVH *pbvh,
                                    PBVHTopologyUpdateMode mode,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    const bool use_frontface,
                                    const bool use_projected,
                                    int sym_axis,
                                    bool updatePBVH,
                                    DyntopoMaskCB mask_cb,
                                    void *mask_cb_data)
{
  /*
  if (sym_axis >= 0 &&
      PIL_check_seconds_timer() - last_update_time[sym_axis] < DYNTOPO_RUN_INTERVAL) {
    return false;
  }

  if (sym_axis >= 0) {
    last_update_time[sym_axis] = PIL_check_seconds_timer();
  }*/

  /* 2 is enough for edge faces - manifold edge */
  BLI_buffer_declare_static(BMLoop *, edge_loops, BLI_BUFFER_NOP, 2);
  BLI_buffer_declare_static(BMFace *, deleted_faces, BLI_BUFFER_NOP, 32);
  const int cd_vert_mask_offset = CustomData_get_offset(&pbvh->bm->vdata, CD_PAINT_MASK);
  const int cd_vert_node_offset = pbvh->cd_vert_node_offset;
  const int cd_face_node_offset = pbvh->cd_face_node_offset;
  const int cd_dyn_vert = pbvh->cd_dyn_vert;
  float ratio = 1.0f;

  bool modified = false;

  if (view_normal) {
    BLI_assert(len_squared_v3(view_normal) != 0.0f);
  }

  EdgeQueueContext eq_ctx = {
      NULL,
      NULL,
      pbvh->bm,
      mask_cb,
      mask_cb_data,

      cd_dyn_vert,
      cd_vert_mask_offset,
      cd_vert_node_offset,
      cd_face_node_offset,
      .avg_elen = 0.0f,
      .max_elen = -1e17,
      .min_elen = 1e17,
      .totedge = 0.0f,
  };

#if 1
  if (mode & PBVH_Collapse) {
    EdgeQueue q;
    BLI_mempool *queue_pool = BLI_mempool_create(sizeof(BMVert *) * 2, 0, 128, BLI_MEMPOOL_NOP);

    eq_ctx.q = &q;
    eq_ctx.pool = queue_pool;

    short_edge_queue_create(
        &eq_ctx, pbvh, center, view_normal, radius, use_frontface, use_projected);

#  ifdef SKINNY_EDGE_FIX
    // prevent remesher thrashing by throttling edge splitting in pathological case of skinny edges
    float avg_elen = eq_ctx.avg_elen;
    if (eq_ctx.totedge > 0.0f) {
      avg_elen /= eq_ctx.totedge;

      float emax = eq_ctx.max_elen;
      if (emax == 0.0f) {
        emax = 0.0001f;
      }

      if (pbvh->bm_min_edge_len > 0.0f && avg_elen > 0.0f) {
        ratio = avg_elen / (pbvh->bm_min_edge_len * 0.5 + emax * 0.5);
        ratio = MAX2(ratio, 0.25f);
        ratio = MIN2(ratio, 5.0f);
      }
    }
#  endif

    int max_steps = (int)((float)DYNTOPO_MAX_ITER * ratio);

    pbvh_bmesh_check_nodes(pbvh);
    modified |= pbvh_bmesh_collapse_short_edges(&eq_ctx, pbvh, &deleted_faces, max_steps);
    pbvh_bmesh_check_nodes(pbvh);

    BLI_heapsimple_free(q.heap, NULL);
    if (q.elems) {
      MEM_freeN(q.elems);
    }
    BLI_mempool_destroy(queue_pool);
  }

  if (mode & PBVH_Subdivide) {
    EdgeQueue q;
    BLI_mempool *queue_pool = BLI_mempool_create(sizeof(BMVert *) * 2, 0, 128, BLI_MEMPOOL_NOP);

    eq_ctx.q = &q;
    eq_ctx.pool = queue_pool;

    long_edge_queue_create(
        &eq_ctx, pbvh, center, view_normal, radius, use_frontface, use_projected);

#  ifdef SKINNY_EDGE_FIX
    // prevent remesher thrashing by throttling edge splitting in pathological case of skinny edges
    float avg_elen = eq_ctx.avg_elen;
    if (eq_ctx.totedge > 0.0f) {
      avg_elen /= eq_ctx.totedge;

      float emin = eq_ctx.min_elen;
      if (emin == 0.0f) {
        emin = 0.0001f;
      }

      if (avg_elen > 0.0f) {
        ratio = (pbvh->bm_max_edge_len * 0.5 + emin * 0.5) / avg_elen;
        ratio = MAX2(ratio, 0.05f);
        ratio = MIN2(ratio, 1.0f);
      }
    }
#  endif

    int max_steps = (int)((float)DYNTOPO_MAX_ITER * ratio);

    pbvh_bmesh_check_nodes(pbvh);
    modified |= pbvh_bmesh_subdivide_long_edges(&eq_ctx, pbvh, &edge_loops, max_steps);
    pbvh_bmesh_check_nodes(pbvh);

    if (q.elems) {
      MEM_freeN(q.elems);
    }
    BLI_heapsimple_free(q.heap, NULL);
    BLI_mempool_destroy(queue_pool);
  }

#endif
  if (mode & PBVH_Cleanup) {
    pbvh_bmesh_check_nodes(pbvh);
    modified |= cleanup_valence_3_4(
        pbvh, center, view_normal, radius, use_frontface, use_projected);
    pbvh_bmesh_check_nodes(pbvh);
  }

  if (modified) {

#ifdef PROXY_ADVANCED
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      // ensure proxyvert arrays are rebuilt
      if (node->flag & PBVH_Leaf) {
        BKE_pbvh_free_proxyarray(pbvh, node);
      }
    }
#endif

    // avoid potential infinite loops
    const int totnode = pbvh->totnode;

    for (int i = 0; i < totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
          !(node->flag & PBVH_FullyHidden)) {

        node->flag &= ~PBVH_UpdateTopology;

        /* Recursively split nodes that have gotten too many
         * elements */
        if (updatePBVH) {
          pbvh_bmesh_node_limit_ensure(pbvh, i);
        }
      }
    }
  }
  else {  // still unmark nodes
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology)) {
        node->flag &= ~PBVH_UpdateTopology;
      }
    }
  }

  BLI_buffer_free(&edge_loops);
  BLI_buffer_free(&deleted_faces);

#ifdef USE_VERIFY
  pbvh_bmesh_verify(pbvh);
#endif

  return modified;
}

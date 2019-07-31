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
 * \ingroup bmesh
 *
 * Core BMesh functions for adding, removing BMesh elements.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_linklist_stack.h"
#include "BLI_utildefines_stack.h"

#include "BLT_translation.h"

#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* use so valgrinds memcheck alerts us when undefined index is used.
 * TESTING ONLY! */
// #define USE_DEBUG_INDEX_MEMCHECK

#ifdef USE_DEBUG_INDEX_MEMCHECK
#  define DEBUG_MEMCHECK_INDEX_INVALIDATE(ele) \
    { \
      int undef_idx; \
      BM_elem_index_set(ele, undef_idx); /* set_ok_invalid */ \
    } \
    (void)0

#endif

/**
 * \brief Main function for creating a new vertex.
 */
BMVert *BM_vert_create(BMesh *bm,
                       const float co[3],
                       const BMVert *v_example,
                       const eBMCreateFlag create_flag)
{
  BMVert *v = BLI_mempool_alloc(bm->vpool);

  BLI_assert((v_example == NULL) || (v_example->head.htype == BM_VERT));
  BLI_assert(!(create_flag & 1));

  /* --- assign all members --- */
  v->head.data = NULL;

#ifdef USE_DEBUG_INDEX_MEMCHECK
  DEBUG_MEMCHECK_INDEX_INVALIDATE(v);
#else
  BM_elem_index_set(v, -1); /* set_ok_invalid */
#endif

  v->head.htype = BM_VERT;
  v->head.hflag = 0;
  v->head.api_flag = 0;

  /* allocate flags */
  if (bm->use_toolflags) {
    ((BMVert_OFlag *)v)->oflags = bm->vtoolflagpool ? BLI_mempool_calloc(bm->vtoolflagpool) : NULL;
  }

  /* 'v->no' is handled by BM_elem_attrs_copy */
  if (co) {
    copy_v3_v3(v->co, co);
  }
  else {
    zero_v3(v->co);
  }
  /* 'v->no' set below */

  v->e = NULL;
  /* --- done --- */

  /* disallow this flag for verts - its meaningless */
  BLI_assert((create_flag & BM_CREATE_NO_DOUBLE) == 0);

  /* may add to middle of the pool */
  bm->elem_index_dirty |= BM_VERT;
  bm->elem_table_dirty |= BM_VERT;

  bm->totvert++;

  if (!(create_flag & BM_CREATE_SKIP_CD)) {
    if (v_example) {
      int *keyi;

      /* handles 'v->no' too */
      BM_elem_attrs_copy(bm, bm, v_example, v);

      /* exception: don't copy the original shapekey index */
      keyi = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_SHAPE_KEYINDEX);
      if (keyi) {
        *keyi = ORIGINDEX_NONE;
      }
    }
    else {
      CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
      zero_v3(v->no);
    }
  }
  else {
    if (v_example) {
      copy_v3_v3(v->no, v_example->no);
    }
    else {
      zero_v3(v->no);
    }
  }

  BM_CHECK_ELEMENT(v);

  return v;
}

/**
 * \brief Main function for creating a new edge.
 *
 * \note Duplicate edges are supported by the API however users should _never_ see them.
 * so unless you need a unique edge or know the edge won't exist,
 * you should call with \a no_double = true.
 */
BMEdge *BM_edge_create(
    BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *e_example, const eBMCreateFlag create_flag)
{
  BMEdge *e;

  BLI_assert(v1 != v2);
  BLI_assert(v1->head.htype == BM_VERT && v2->head.htype == BM_VERT);
  BLI_assert((e_example == NULL) || (e_example->head.htype == BM_EDGE));
  BLI_assert(!(create_flag & 1));

  if ((create_flag & BM_CREATE_NO_DOUBLE) && (e = BM_edge_exists(v1, v2))) {
    return e;
  }

  e = BLI_mempool_alloc(bm->epool);

  /* --- assign all members --- */
  e->head.data = NULL;

#ifdef USE_DEBUG_INDEX_MEMCHECK
  DEBUG_MEMCHECK_INDEX_INVALIDATE(e);
#else
  BM_elem_index_set(e, -1); /* set_ok_invalid */
#endif

  e->head.htype = BM_EDGE;
  e->head.hflag = BM_ELEM_SMOOTH | BM_ELEM_DRAW;
  e->head.api_flag = 0;

  /* allocate flags */
  if (bm->use_toolflags) {
    ((BMEdge_OFlag *)e)->oflags = bm->etoolflagpool ? BLI_mempool_calloc(bm->etoolflagpool) : NULL;
  }

  e->v1 = v1;
  e->v2 = v2;
  e->l = NULL;

  memset(&e->v1_disk_link, 0, sizeof(BMDiskLink) * 2);
  /* --- done --- */

  bmesh_disk_edge_append(e, e->v1);
  bmesh_disk_edge_append(e, e->v2);

  /* may add to middle of the pool */
  bm->elem_index_dirty |= BM_EDGE;
  bm->elem_table_dirty |= BM_EDGE;

  bm->totedge++;

  if (!(create_flag & BM_CREATE_SKIP_CD)) {
    if (e_example) {
      BM_elem_attrs_copy(bm, bm, e_example, e);
    }
    else {
      CustomData_bmesh_set_default(&bm->edata, &e->head.data);
    }
  }

  BM_CHECK_ELEMENT(e);

  return e;
}

/**
 * \note In most cases a \a l_example should be NULL,
 * since this is a low level API and we shouldn't attempt to be clever and guess what's intended.
 * In cases where copying adjacent loop-data is useful, see #BM_face_copy_shared.
 */
static BMLoop *bm_loop_create(BMesh *bm,
                              BMVert *v,
                              BMEdge *e,
                              BMFace *f,
                              const BMLoop *l_example,
                              const eBMCreateFlag create_flag)
{
  BMLoop *l = NULL;

  l = BLI_mempool_alloc(bm->lpool);

  BLI_assert((l_example == NULL) || (l_example->head.htype == BM_LOOP));
  BLI_assert(!(create_flag & 1));

#ifndef NDEBUG
  if (l_example) {
    /* ensure passing a loop is either sharing the same vertex, or entirely disconnected
     * use to catch mistake passing in loop offset-by-one. */
    BLI_assert((v == l_example->v) || !ELEM(v, l_example->prev->v, l_example->next->v));
  }
#endif

  /* --- assign all members --- */
  l->head.data = NULL;

#ifdef USE_DEBUG_INDEX_MEMCHECK
  DEBUG_MEMCHECK_INDEX_INVALIDATE(l);
#else
  BM_elem_index_set(l, -1); /* set_ok_invalid */
#endif

  l->head.htype = BM_LOOP;
  l->head.hflag = 0;
  l->head.api_flag = 0;

  l->v = v;
  l->e = e;
  l->f = f;

  l->radial_next = NULL;
  l->radial_prev = NULL;
  l->next = NULL;
  l->prev = NULL;
  /* --- done --- */

  /* may add to middle of the pool */
  bm->elem_index_dirty |= BM_LOOP;

  bm->totloop++;

  if (!(create_flag & BM_CREATE_SKIP_CD)) {
    if (l_example) {
      /* no need to copy attrs, just handle customdata */
      // BM_elem_attrs_copy(bm, bm, l_example, l);
      CustomData_bmesh_free_block_data(&bm->ldata, l->head.data);
      CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, l_example->head.data, &l->head.data);
    }
    else {
      CustomData_bmesh_set_default(&bm->ldata, &l->head.data);
    }
  }

  return l;
}

static BMLoop *bm_face_boundary_add(
    BMesh *bm, BMFace *f, BMVert *startv, BMEdge *starte, const eBMCreateFlag create_flag)
{
#ifdef USE_BMESH_HOLES
  BMLoopList *lst = BLI_mempool_calloc(bm->looplistpool);
#endif
  BMLoop *l = bm_loop_create(bm, startv, starte, f, NULL /* starte->l */, create_flag);

  bmesh_radial_loop_append(starte, l);

#ifdef USE_BMESH_HOLES
  lst->first = lst->last = l;
  BLI_addtail(&f->loops, lst);
#else
  f->l_first = l;
#endif

  return l;
}

BMFace *BM_face_copy(
    BMesh *bm_dst, BMesh *bm_src, BMFace *f, const bool copy_verts, const bool copy_edges)
{
  BMVert **verts = BLI_array_alloca(verts, f->len);
  BMEdge **edges = BLI_array_alloca(edges, f->len);
  BMLoop *l_iter;
  BMLoop *l_first;
  BMLoop *l_copy;
  BMFace *f_copy;
  int i;

  BLI_assert((bm_dst == bm_src) || (copy_verts && copy_edges));

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  i = 0;
  do {
    if (copy_verts) {
      verts[i] = BM_vert_create(bm_dst, l_iter->v->co, l_iter->v, BM_CREATE_NOP);
    }
    else {
      verts[i] = l_iter->v;
    }
    i++;
  } while ((l_iter = l_iter->next) != l_first);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  i = 0;
  do {
    if (copy_edges) {
      BMVert *v1, *v2;

      if (l_iter->e->v1 == verts[i]) {
        v1 = verts[i];
        v2 = verts[(i + 1) % f->len];
      }
      else {
        v2 = verts[i];
        v1 = verts[(i + 1) % f->len];
      }

      edges[i] = BM_edge_create(bm_dst, v1, v2, l_iter->e, BM_CREATE_NOP);
    }
    else {
      edges[i] = l_iter->e;
    }
    i++;
  } while ((l_iter = l_iter->next) != l_first);

  f_copy = BM_face_create(bm_dst, verts, edges, f->len, NULL, BM_CREATE_SKIP_CD);

  BM_elem_attrs_copy(bm_src, bm_dst, f, f_copy);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  l_copy = BM_FACE_FIRST_LOOP(f_copy);
  do {
    BM_elem_attrs_copy(bm_src, bm_dst, l_iter, l_copy);
    l_copy = l_copy->next;
  } while ((l_iter = l_iter->next) != l_first);

  return f_copy;
}

/**
 * only create the face, since this calloc's the length is initialized to 0,
 * leave adding loops to the caller.
 *
 * \note, caller needs to handle customdata.
 */
BLI_INLINE BMFace *bm_face_create__internal(BMesh *bm)
{
  BMFace *f;

  f = BLI_mempool_alloc(bm->fpool);

  /* --- assign all members --- */
  f->head.data = NULL;
#ifdef USE_DEBUG_INDEX_MEMCHECK
  DEBUG_MEMCHECK_INDEX_INVALIDATE(f);
#else
  BM_elem_index_set(f, -1); /* set_ok_invalid */
#endif

  f->head.htype = BM_FACE;
  f->head.hflag = 0;
  f->head.api_flag = 0;

  /* allocate flags */
  if (bm->use_toolflags) {
    ((BMFace_OFlag *)f)->oflags = bm->ftoolflagpool ? BLI_mempool_calloc(bm->ftoolflagpool) : NULL;
  }

#ifdef USE_BMESH_HOLES
  BLI_listbase_clear(&f->loops);
#else
  f->l_first = NULL;
#endif
  f->len = 0;
  /* caller must initialize */
  // zero_v3(f->no);
  f->mat_nr = 0;
  /* --- done --- */

  /* may add to middle of the pool */
  bm->elem_index_dirty |= BM_FACE;
  bm->elem_table_dirty |= BM_FACE;

  bm->totface++;

#ifdef USE_BMESH_HOLES
  f->totbounds = 0;
#endif

  return f;
}

/**
 * Main face creation function
 *
 * \param bm: The mesh
 * \param verts: A sorted array of verts size of len
 * \param edges: A sorted array of edges size of len
 * \param len: Length of the face
 * \param create_flag: Options for creating the face
 */
BMFace *BM_face_create(BMesh *bm,
                       BMVert **verts,
                       BMEdge **edges,
                       const int len,
                       const BMFace *f_example,
                       const eBMCreateFlag create_flag)
{
  BMFace *f = NULL;
  BMLoop *l, *startl, *lastl;
  int i;

  BLI_assert((f_example == NULL) || (f_example->head.htype == BM_FACE));
  BLI_assert(!(create_flag & 1));

  if (len == 0) {
    /* just return NULL for now */
    return NULL;
  }

  if (create_flag & BM_CREATE_NO_DOUBLE) {
    /* Check if face already exists */
    f = BM_face_exists(verts, len);
    if (f != NULL) {
      return f;
    }
  }

  f = bm_face_create__internal(bm);

  startl = lastl = bm_face_boundary_add(bm, f, verts[0], edges[0], create_flag);

  for (i = 1; i < len; i++) {
    l = bm_loop_create(bm, verts[i], edges[i], f, NULL /* edges[i]->l */, create_flag);

    bmesh_radial_loop_append(edges[i], l);

    l->prev = lastl;
    lastl->next = l;
    lastl = l;
  }

  startl->prev = lastl;
  lastl->next = startl;

  f->len = len;

  if (!(create_flag & BM_CREATE_SKIP_CD)) {
    if (f_example) {
      BM_elem_attrs_copy(bm, bm, f_example, f);
    }
    else {
      CustomData_bmesh_set_default(&bm->pdata, &f->head.data);
      zero_v3(f->no);
    }
  }
  else {
    if (f_example) {
      copy_v3_v3(f->no, f_example->no);
    }
    else {
      zero_v3(f->no);
    }
  }

  BM_CHECK_ELEMENT(f);

  return f;
}

/**
 * Wrapper for #BM_face_create when you don't have an edge array
 */
BMFace *BM_face_create_verts(BMesh *bm,
                             BMVert **vert_arr,
                             const int len,
                             const BMFace *f_example,
                             const eBMCreateFlag create_flag,
                             const bool create_edges)
{
  BMEdge **edge_arr = BLI_array_alloca(edge_arr, len);

  if (create_edges) {
    BM_edges_from_verts_ensure(bm, edge_arr, vert_arr, len);
  }
  else {
    if (BM_edges_from_verts(edge_arr, vert_arr, len) == false) {
      return NULL;
    }
  }

  return BM_face_create(bm, vert_arr, edge_arr, len, f_example, create_flag);
}

#ifndef NDEBUG

/**
 * Check the element is valid.
 *
 * BMESH_TODO, when this raises an error the output is incredibly confusing.
 * need to have some nice way to print/debug what the heck's going on.
 */
int bmesh_elem_check(void *element, const char htype)
{
  BMHeader *head = element;
  enum {
    IS_NULL = (1 << 0),
    IS_WRONG_TYPE = (1 << 1),

    IS_VERT_WRONG_EDGE_TYPE = (1 << 2),

    IS_EDGE_NULL_DISK_LINK = (1 << 3),
    IS_EDGE_WRONG_LOOP_TYPE = (1 << 4),
    IS_EDGE_WRONG_FACE_TYPE = (1 << 5),
    IS_EDGE_NULL_RADIAL_LINK = (1 << 6),
    IS_EDGE_ZERO_FACE_LENGTH = (1 << 7),

    IS_LOOP_WRONG_FACE_TYPE = (1 << 8),
    IS_LOOP_WRONG_EDGE_TYPE = (1 << 9),
    IS_LOOP_WRONG_VERT_TYPE = (1 << 10),
    IS_LOOP_VERT_NOT_IN_EDGE = (1 << 11),
    IS_LOOP_NULL_CYCLE_LINK = (1 << 12),
    IS_LOOP_ZERO_FACE_LENGTH = (1 << 13),
    IS_LOOP_WRONG_FACE_LENGTH = (1 << 14),
    IS_LOOP_WRONG_RADIAL_LENGTH = (1 << 15),

    IS_FACE_NULL_LOOP = (1 << 16),
    IS_FACE_WRONG_LOOP_FACE = (1 << 17),
    IS_FACE_NULL_EDGE = (1 << 18),
    IS_FACE_NULL_VERT = (1 << 19),
    IS_FACE_LOOP_VERT_NOT_IN_EDGE = (1 << 20),
    IS_FACE_LOOP_WRONG_RADIAL_LENGTH = (1 << 21),
    IS_FACE_LOOP_WRONG_DISK_LENGTH = (1 << 22),
    IS_FACE_LOOP_DUPE_LOOP = (1 << 23),
    IS_FACE_LOOP_DUPE_VERT = (1 << 24),
    IS_FACE_LOOP_DUPE_EDGE = (1 << 25),
    IS_FACE_WRONG_LENGTH = (1 << 26),
  } err = 0;

  if (!element) {
    return IS_NULL;
  }

  if (head->htype != htype) {
    return IS_WRONG_TYPE;
  }

  switch (htype) {
    case BM_VERT: {
      BMVert *v = element;
      if (v->e && v->e->head.htype != BM_EDGE) {
        err |= IS_VERT_WRONG_EDGE_TYPE;
      }
      break;
    }
    case BM_EDGE: {
      BMEdge *e = element;
      if (e->v1_disk_link.prev == NULL || e->v2_disk_link.prev == NULL ||
          e->v1_disk_link.next == NULL || e->v2_disk_link.next == NULL) {
        err |= IS_EDGE_NULL_DISK_LINK;
      }

      if (e->l && e->l->head.htype != BM_LOOP) {
        err |= IS_EDGE_WRONG_LOOP_TYPE;
      }
      if (e->l && e->l->f->head.htype != BM_FACE) {
        err |= IS_EDGE_WRONG_FACE_TYPE;
      }
      if (e->l && (e->l->radial_next == NULL || e->l->radial_prev == NULL)) {
        err |= IS_EDGE_NULL_RADIAL_LINK;
      }
      if (e->l && e->l->f->len <= 0) {
        err |= IS_EDGE_ZERO_FACE_LENGTH;
      }
      break;
    }
    case BM_LOOP: {
      BMLoop *l = element, *l2;
      int i;

      if (l->f->head.htype != BM_FACE) {
        err |= IS_LOOP_WRONG_FACE_TYPE;
      }
      if (l->e->head.htype != BM_EDGE) {
        err |= IS_LOOP_WRONG_EDGE_TYPE;
      }
      if (l->v->head.htype != BM_VERT) {
        err |= IS_LOOP_WRONG_VERT_TYPE;
      }
      if (!BM_vert_in_edge(l->e, l->v)) {
        fprintf(stderr,
                "%s: fatal bmesh error (vert not in edge)! (bmesh internal error)\n",
                __func__);
        err |= IS_LOOP_VERT_NOT_IN_EDGE;
      }

      if (l->radial_next == NULL || l->radial_prev == NULL) {
        err |= IS_LOOP_NULL_CYCLE_LINK;
      }
      if (l->f->len <= 0) {
        err |= IS_LOOP_ZERO_FACE_LENGTH;
      }

      /* validate boundary loop -- invalid for hole loops, of course,
       * but we won't be allowing those for a while yet */
      l2 = l;
      i = 0;
      do {
        if (i >= BM_NGON_MAX) {
          break;
        }

        i++;
      } while ((l2 = l2->next) != l);

      if (i != l->f->len || l2 != l) {
        err |= IS_LOOP_WRONG_FACE_LENGTH;
      }

      if (!bmesh_radial_validate(bmesh_radial_length(l), l)) {
        err |= IS_LOOP_WRONG_RADIAL_LENGTH;
      }

      break;
    }
    case BM_FACE: {
      BMFace *f = element;
      BMLoop *l_iter;
      BMLoop *l_first;
      int len = 0;

#  ifdef USE_BMESH_HOLES
      if (!f->loops.first)
#  else
      if (!f->l_first)
#  endif
      {
        err |= IS_FACE_NULL_LOOP;
      }
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (l_iter->f != f) {
          fprintf(stderr,
                  "%s: loop inside one face points to another! (bmesh internal error)\n",
                  __func__);
          err |= IS_FACE_WRONG_LOOP_FACE;
        }

        if (!l_iter->e) {
          err |= IS_FACE_NULL_EDGE;
        }
        if (!l_iter->v) {
          err |= IS_FACE_NULL_VERT;
        }
        if (l_iter->e && l_iter->v) {
          if (!BM_vert_in_edge(l_iter->e, l_iter->v) ||
              !BM_vert_in_edge(l_iter->e, l_iter->next->v)) {
            err |= IS_FACE_LOOP_VERT_NOT_IN_EDGE;
          }

          if (!bmesh_radial_validate(bmesh_radial_length(l_iter), l_iter)) {
            err |= IS_FACE_LOOP_WRONG_RADIAL_LENGTH;
          }

          if (bmesh_disk_count_at_most(l_iter->v, 2) < 2) {
            err |= IS_FACE_LOOP_WRONG_DISK_LENGTH;
          }
        }

        /* check for duplicates */
        if (BM_ELEM_API_FLAG_TEST(l_iter, _FLAG_ELEM_CHECK)) {
          err |= IS_FACE_LOOP_DUPE_LOOP;
        }
        BM_ELEM_API_FLAG_ENABLE(l_iter, _FLAG_ELEM_CHECK);
        if (l_iter->v) {
          if (BM_ELEM_API_FLAG_TEST(l_iter->v, _FLAG_ELEM_CHECK)) {
            err |= IS_FACE_LOOP_DUPE_VERT;
          }
          BM_ELEM_API_FLAG_ENABLE(l_iter->v, _FLAG_ELEM_CHECK);
        }
        if (l_iter->e) {
          if (BM_ELEM_API_FLAG_TEST(l_iter->e, _FLAG_ELEM_CHECK)) {
            err |= IS_FACE_LOOP_DUPE_EDGE;
          }
          BM_ELEM_API_FLAG_ENABLE(l_iter->e, _FLAG_ELEM_CHECK);
        }

        len++;
      } while ((l_iter = l_iter->next) != l_first);

      /* cleanup duplicates flag */
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BM_ELEM_API_FLAG_DISABLE(l_iter, _FLAG_ELEM_CHECK);
        if (l_iter->v) {
          BM_ELEM_API_FLAG_DISABLE(l_iter->v, _FLAG_ELEM_CHECK);
        }
        if (l_iter->e) {
          BM_ELEM_API_FLAG_DISABLE(l_iter->e, _FLAG_ELEM_CHECK);
        }
      } while ((l_iter = l_iter->next) != l_first);

      if (len != f->len) {
        err |= IS_FACE_WRONG_LENGTH;
      }
      break;
    }
    default:
      BLI_assert(0);
      break;
  }

  BMESH_ASSERT(err == 0);

  return err;
}

#endif /* NDEBUG */

/**
 * low level function, only frees the vert,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_vert(BMesh *bm, BMVert *v)
{
  bm->totvert--;
  bm->elem_index_dirty |= BM_VERT;
  bm->elem_table_dirty |= BM_VERT;

  BM_select_history_remove(bm, v);

  if (v->head.data) {
    CustomData_bmesh_free_block(&bm->vdata, &v->head.data);
  }

  if (bm->vtoolflagpool) {
    BLI_mempool_free(bm->vtoolflagpool, ((BMVert_OFlag *)v)->oflags);
  }
  BLI_mempool_free(bm->vpool, v);
}

/**
 * low level function, only frees the edge,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_edge(BMesh *bm, BMEdge *e)
{
  bm->totedge--;
  bm->elem_index_dirty |= BM_EDGE;
  bm->elem_table_dirty |= BM_EDGE;

  BM_select_history_remove(bm, (BMElem *)e);

  if (e->head.data) {
    CustomData_bmesh_free_block(&bm->edata, &e->head.data);
  }

  if (bm->etoolflagpool) {
    BLI_mempool_free(bm->etoolflagpool, ((BMEdge_OFlag *)e)->oflags);
  }
  BLI_mempool_free(bm->epool, e);
}

/**
 * low level function, only frees the face,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_face(BMesh *bm, BMFace *f)
{
  if (bm->act_face == f) {
    bm->act_face = NULL;
  }

  bm->totface--;
  bm->elem_index_dirty |= BM_FACE;
  bm->elem_table_dirty |= BM_FACE;

  BM_select_history_remove(bm, (BMElem *)f);

  if (f->head.data) {
    CustomData_bmesh_free_block(&bm->pdata, &f->head.data);
  }

  if (bm->ftoolflagpool) {
    BLI_mempool_free(bm->ftoolflagpool, ((BMFace_OFlag *)f)->oflags);
  }
  BLI_mempool_free(bm->fpool, f);
}

/**
 * low level function, only frees the loop,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_loop(BMesh *bm, BMLoop *l)
{
  bm->totloop--;
  bm->elem_index_dirty |= BM_LOOP;
  if (l->head.data) {
    CustomData_bmesh_free_block(&bm->ldata, &l->head.data);
  }

  BLI_mempool_free(bm->lpool, l);
}

/**
 * kills all edges associated with \a f, along with any other faces containing
 * those edges
 */
void BM_face_edges_kill(BMesh *bm, BMFace *f)
{
  BMEdge **edges = BLI_array_alloca(edges, f->len);
  BMLoop *l_iter;
  BMLoop *l_first;
  int i = 0;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    edges[i++] = l_iter->e;
  } while ((l_iter = l_iter->next) != l_first);

  for (i = 0; i < f->len; i++) {
    BM_edge_kill(bm, edges[i]);
  }
}

/**
 * kills all verts associated with \a f, along with any other faces containing
 * those vertices
 */
void BM_face_verts_kill(BMesh *bm, BMFace *f)
{
  BMVert **verts = BLI_array_alloca(verts, f->len);
  BMLoop *l_iter;
  BMLoop *l_first;
  int i = 0;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    verts[i++] = l_iter->v;
  } while ((l_iter = l_iter->next) != l_first);

  for (i = 0; i < f->len; i++) {
    BM_vert_kill(bm, verts[i]);
  }
}

/**
 * Kills \a f and its loops.
 */
void BM_face_kill(BMesh *bm, BMFace *f)
{
#ifdef USE_BMESH_HOLES
  BMLoopList *ls, *ls_next;
#endif

#ifdef NDEBUG
  /* check length since we may be removing degenerate faces */
  if (f->len >= 3) {
    BM_CHECK_ELEMENT(f);
  }
#endif

#ifdef USE_BMESH_HOLES
  for (ls = f->loops.first; ls; ls = ls_next)
#else
  if (f->l_first)
#endif
  {
    BMLoop *l_iter, *l_next, *l_first;

#ifdef USE_BMESH_HOLES
    ls_next = ls->next;
    l_iter = l_first = ls->first;
#else
    l_iter = l_first = f->l_first;
#endif

    do {
      l_next = l_iter->next;

      bmesh_radial_loop_remove(l_iter->e, l_iter);
      bm_kill_only_loop(bm, l_iter);

    } while ((l_iter = l_next) != l_first);

#ifdef USE_BMESH_HOLES
    BLI_mempool_free(bm->looplistpool, ls);
#endif
  }

  bm_kill_only_face(bm, f);
}

/**
 * A version of #BM_face_kill which removes edges and verts
 * which have no remaining connected geometry.
 */
void BM_face_kill_loose(BMesh *bm, BMFace *f)
{
#ifdef USE_BMESH_HOLES
  BMLoopList *ls, *ls_next;
#endif

  BM_CHECK_ELEMENT(f);

#ifdef USE_BMESH_HOLES
  for (ls = f->loops.first; ls; ls = ls_next)
#else
  if (f->l_first)
#endif
  {
    BMLoop *l_iter, *l_next, *l_first;

#ifdef USE_BMESH_HOLES
    ls_next = ls->next;
    l_iter = l_first = ls->first;
#else
    l_iter = l_first = f->l_first;
#endif

    do {
      BMEdge *e;
      l_next = l_iter->next;

      e = l_iter->e;
      bmesh_radial_loop_remove(e, l_iter);
      bm_kill_only_loop(bm, l_iter);

      if (e->l == NULL) {
        BMVert *v1 = e->v1, *v2 = e->v2;

        bmesh_disk_edge_remove(e, e->v1);
        bmesh_disk_edge_remove(e, e->v2);
        bm_kill_only_edge(bm, e);

        if (v1->e == NULL) {
          bm_kill_only_vert(bm, v1);
        }
        if (v2->e == NULL) {
          bm_kill_only_vert(bm, v2);
        }
      }
    } while ((l_iter = l_next) != l_first);

#ifdef USE_BMESH_HOLES
    BLI_mempool_free(bm->looplistpool, ls);
#endif
  }

  bm_kill_only_face(bm, f);
}

/**
 * kills \a e and all faces that use it.
 */
void BM_edge_kill(BMesh *bm, BMEdge *e)
{
  while (e->l) {
    BM_face_kill(bm, e->l->f);
  }

  bmesh_disk_edge_remove(e, e->v1);
  bmesh_disk_edge_remove(e, e->v2);

  bm_kill_only_edge(bm, e);
}

/**
 * kills \a v and all edges that use it.
 */
void BM_vert_kill(BMesh *bm, BMVert *v)
{
  while (v->e) {
    BM_edge_kill(bm, v->e);
  }

  bm_kill_only_vert(bm, v);
}

/********** private disk and radial cycle functions ********** */

/**
 * return the length of the face, should always equal \a l->f->len
 */
static int UNUSED_FUNCTION(bm_loop_length)(BMLoop *l)
{
  BMLoop *l_first = l;
  int i = 0;

  do {
    i++;
  } while ((l = l->next) != l_first);

  return i;
}

/**
 * \brief Loop Reverse
 *
 * Changes the winding order of a face from CW to CCW or vice versa.
 *
 * \param cd_loop_mdisp_offset: Cached result of `CustomData_get_offset(&bm->ldata, CD_MDISPS)`.
 * \param use_loop_mdisp_flip: When set, flip the Z-depth of the mdisp,
 * (use when flipping normals, disable when mirroring, eg: symmetrize).
 */
void bmesh_kernel_loop_reverse(BMesh *bm,
                               BMFace *f,
                               const int cd_loop_mdisp_offset,
                               const bool use_loop_mdisp_flip)
{
  BMLoop *l_first = f->l_first;

  /* track previous cycles radial state */
  BMEdge *e_prev = l_first->prev->e;
  BMLoop *l_prev_radial_next = l_first->prev->radial_next;
  BMLoop *l_prev_radial_prev = l_first->prev->radial_prev;
  bool is_prev_boundary = l_prev_radial_next == l_prev_radial_next->radial_next;

  BMLoop *l_iter = l_first;
  do {
    BMEdge *e_iter = l_iter->e;
    BMLoop *l_iter_radial_next = l_iter->radial_next;
    BMLoop *l_iter_radial_prev = l_iter->radial_prev;
    bool is_iter_boundary = l_iter_radial_next == l_iter_radial_next->radial_next;

#if 0
    bmesh_radial_loop_remove(e_iter, l_iter);
    bmesh_radial_loop_append(e_prev, l_iter);
#else
    /* inline loop reversal */
    if (is_prev_boundary) {
      /* boundary */
      l_iter->radial_next = l_iter;
      l_iter->radial_prev = l_iter;
    }
    else {
      /* non-boundary, replace radial links */
      l_iter->radial_next = l_prev_radial_next;
      l_iter->radial_prev = l_prev_radial_prev;
      l_prev_radial_next->radial_prev = l_iter;
      l_prev_radial_prev->radial_next = l_iter;
    }

    if (e_iter->l == l_iter) {
      e_iter->l = l_iter->next;
    }
    l_iter->e = e_prev;
#endif

    SWAP(BMLoop *, l_iter->next, l_iter->prev);

    if (cd_loop_mdisp_offset != -1) {
      MDisps *md = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_mdisp_offset);
      BKE_mesh_mdisp_flip(md, use_loop_mdisp_flip);
    }

    e_prev = e_iter;
    l_prev_radial_next = l_iter_radial_next;
    l_prev_radial_prev = l_iter_radial_prev;
    is_prev_boundary = is_iter_boundary;

    /* step to next (now swapped) */
  } while ((l_iter = l_iter->prev) != l_first);

#ifndef NDEBUG
  /* validate radial */
  int i;
  for (i = 0, l_iter = l_first; i < f->len; i++, l_iter = l_iter->next) {
    BM_CHECK_ELEMENT(l_iter);
    BM_CHECK_ELEMENT(l_iter->e);
    BM_CHECK_ELEMENT(l_iter->v);
    BM_CHECK_ELEMENT(l_iter->f);
  }

  BM_CHECK_ELEMENT(f);
#endif

  /* Loop indices are no more valid! */
  bm->elem_index_dirty |= BM_LOOP;
}

static void bm_elements_systag_enable(void *veles, int tot, const char api_flag)
{
  BMHeader **eles = veles;
  int i;

  for (i = 0; i < tot; i++) {
    BM_ELEM_API_FLAG_ENABLE((BMElemF *)eles[i], api_flag);
  }
}

static void bm_elements_systag_disable(void *veles, int tot, const char api_flag)
{
  BMHeader **eles = veles;
  int i;

  for (i = 0; i < tot; i++) {
    BM_ELEM_API_FLAG_DISABLE((BMElemF *)eles[i], api_flag);
  }
}

static int bm_loop_systag_count_radial(BMLoop *l, const char api_flag)
{
  BMLoop *l_iter = l;
  int i = 0;
  do {
    i += BM_ELEM_API_FLAG_TEST(l_iter->f, api_flag) ? 1 : 0;
  } while ((l_iter = l_iter->radial_next) != l);

  return i;
}

static int UNUSED_FUNCTION(bm_vert_systag_count_disk)(BMVert *v, const char api_flag)
{
  BMEdge *e = v->e;
  int i = 0;

  if (!e) {
    return 0;
  }

  do {
    i += BM_ELEM_API_FLAG_TEST(e, api_flag) ? 1 : 0;
  } while ((e = bmesh_disk_edge_next(e, v)) != v->e);

  return i;
}

/**
 * Return true when the vertex is manifold,
 * attached to faces which are all flagged.
 */
static bool bm_vert_is_manifold_flagged(BMVert *v, const char api_flag)
{
  BMEdge *e = v->e;

  if (!e) {
    return false;
  }

  do {
    BMLoop *l = e->l;

    if (!l) {
      return false;
    }

    if (BM_edge_is_boundary(l->e)) {
      return false;
    }

    do {
      if (!BM_ELEM_API_FLAG_TEST(l->f, api_flag)) {
        return false;
      }
    } while ((l = l->radial_next) != e->l);
  } while ((e = bmesh_disk_edge_next(e, v)) != v->e);

  return true;
}

/* Mid-level Topology Manipulation Functions */

/**
 * \brief Join Connected Faces
 *
 * Joins a collected group of faces into one. Only restriction on
 * the input data is that the faces must be connected to each other.
 *
 * \return The newly created combine BMFace.
 *
 * \note If a pair of faces share multiple edges,
 * the pair of faces will be joined at every edge.
 *
 * \note this is a generic, flexible join faces function,
 * almost everything uses this, including #BM_faces_join_pair
 */
BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface, const bool do_del)
{
  BMFace *f, *f_new;
#ifdef USE_BMESH_HOLES
  BMLoopList *lst;
  ListBase holes = {NULL, NULL};
#endif
  BMLoop *l_iter;
  BMLoop *l_first;
  BMEdge **edges = NULL;
  BMEdge **deledges = NULL;
  BMVert **delverts = NULL;
  BLI_array_staticdeclare(edges, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(deledges, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(delverts, BM_DEFAULT_NGON_STACK_SIZE);
  BMVert *v1 = NULL, *v2 = NULL;
  int i;
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);

  if (UNLIKELY(!totface)) {
    BMESH_ASSERT(0);
    return NULL;
  }

  if (totface == 1) {
    return faces[0];
  }

  bm_elements_systag_enable(faces, totface, _FLAG_JF);

  for (i = 0; i < totface; i++) {
    f = faces[i];
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      int rlen = bm_loop_systag_count_radial(l_iter, _FLAG_JF);

      if (rlen > 2) {
        /* Input faces do not form a contiguous manifold region */
        goto error;
      }
      else if (rlen == 1) {
        BLI_array_append(edges, l_iter->e);

        if (!v1) {
          v1 = l_iter->v;
          v2 = BM_edge_other_vert(l_iter->e, l_iter->v);
        }
      }
      else if (rlen == 2) {
        const bool d1 = bm_vert_is_manifold_flagged(l_iter->e->v1, _FLAG_JF);
        const bool d2 = bm_vert_is_manifold_flagged(l_iter->e->v2, _FLAG_JF);

        if (!d1 && !d2 && !BM_ELEM_API_FLAG_TEST(l_iter->e, _FLAG_JF)) {
          /* don't remove an edge it makes up the side of another face
           * else this will remove the face as well - campbell */
          if (!BM_edge_face_count_is_over(l_iter->e, 2)) {
            if (do_del) {
              BLI_array_append(deledges, l_iter->e);
            }
            BM_ELEM_API_FLAG_ENABLE(l_iter->e, _FLAG_JF);
          }
        }
        else {
          if (d1 && !BM_ELEM_API_FLAG_TEST(l_iter->e->v1, _FLAG_JF)) {
            if (do_del) {
              BLI_array_append(delverts, l_iter->e->v1);
            }
            BM_ELEM_API_FLAG_ENABLE(l_iter->e->v1, _FLAG_JF);
          }

          if (d2 && !BM_ELEM_API_FLAG_TEST(l_iter->e->v2, _FLAG_JF)) {
            if (do_del) {
              BLI_array_append(delverts, l_iter->e->v2);
            }
            BM_ELEM_API_FLAG_ENABLE(l_iter->e->v2, _FLAG_JF);
          }
        }
      }
    } while ((l_iter = l_iter->next) != l_first);

#ifdef USE_BMESH_HOLES
    for (lst = f->loops.first; lst; lst = lst->next) {
      if (lst == f->loops.first) {
        continue;
      }

      BLI_remlink(&f->loops, lst);
      BLI_addtail(&holes, lst);
    }
#endif
  }

  /* create region face */
  f_new = BLI_array_len(edges) ?
              BM_face_create_ngon(
                  bm, v1, v2, edges, BLI_array_len(edges), faces[0], BM_CREATE_NOP) :
              NULL;
  if (UNLIKELY(f_new == NULL)) {
    /* Invalid boundary region to join faces */
    goto error;
  }

  /* copy over loop data */
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
  do {
    BMLoop *l2 = l_iter->radial_next;

    do {
      if (BM_ELEM_API_FLAG_TEST(l2->f, _FLAG_JF)) {
        break;
      }
      l2 = l2->radial_next;
    } while (l2 != l_iter);

    if (l2 != l_iter) {
      /* loops share an edge, shared vert depends on winding */
      if (l2->v != l_iter->v) {
        l2 = l2->next;
      }
      BLI_assert(l_iter->v == l2->v);

      BM_elem_attrs_copy(bm, bm, l2, l_iter);
    }
  } while ((l_iter = l_iter->next) != l_first);

#ifdef USE_BMESH_HOLES
  /* add holes */
  BLI_movelisttolist(&f_new->loops, &holes);

  /* update loop face pointer */
  for (lst = f_new->loops.first; lst; lst = lst->next) {
    l_iter = l_first = lst->first;
    do {
      l_iter->f = f_new;
    } while ((l_iter = l_iter->next) != l_first);
  }
#endif

  bm_elements_systag_disable(faces, totface, _FLAG_JF);
  BM_ELEM_API_FLAG_DISABLE(f_new, _FLAG_JF);

  /* handle multi-res data */
  if (cd_loop_mdisp_offset != -1) {
    float f_center[3];
    float(*faces_center)[3] = BLI_array_alloca(faces_center, totface);

    BM_face_calc_center_median(f_new, f_center);
    for (i = 0; i < totface; i++) {
      BM_face_calc_center_median(faces[i], faces_center[i]);
    }

    l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
    do {
      for (i = 0; i < totface; i++) {
        BM_loop_interp_multires_ex(
            bm, l_iter, faces[i], f_center, faces_center[i], cd_loop_mdisp_offset);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  /* delete old geometry */
  if (do_del) {
    for (i = 0; i < BLI_array_len(deledges); i++) {
      BM_edge_kill(bm, deledges[i]);
    }

    for (i = 0; i < BLI_array_len(delverts); i++) {
      BM_vert_kill(bm, delverts[i]);
    }
  }
  else {
    /* otherwise we get both old and new faces */
    for (i = 0; i < totface; i++) {
      BM_face_kill(bm, faces[i]);
    }
  }

  BLI_array_free(edges);
  BLI_array_free(deledges);
  BLI_array_free(delverts);

  BM_CHECK_ELEMENT(f_new);
  return f_new;

error:
  bm_elements_systag_disable(faces, totface, _FLAG_JF);
  BLI_array_free(edges);
  BLI_array_free(deledges);
  BLI_array_free(delverts);

  return NULL;
}

static BMFace *bm_face_create__sfme(BMesh *bm, BMFace *f_example)
{
  BMFace *f;
#ifdef USE_BMESH_HOLES
  BMLoopList *lst;
#endif

  f = bm_face_create__internal(bm);

#ifdef USE_BMESH_HOLES
  lst = BLI_mempool_calloc(bm->looplistpool);
  BLI_addtail(&f->loops, lst);
#endif

#ifdef USE_BMESH_HOLES
  f->totbounds = 1;
#endif

  BM_elem_attrs_copy(bm, bm, f_example, f);

  return f;
}

/**
 * \brief Split Face Make Edge (SFME)
 *
 * \warning this is a low level function, most likely you want to use #BM_face_split()
 *
 * Takes as input two vertices in a single face.
 * An edge is created which divides the original face into two distinct regions.
 * One of the regions is assigned to the original face and it is closed off.
 * The second region has a new face assigned to it.
 *
 * \par Examples:
 * <pre>
 *     Before:               After:
 *      +--------+           +--------+
 *      |        |           |        |
 *      |        |           |   f1   |
 *     v1   f1   v2          v1======v2
 *      |        |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 * </pre>
 *
 * \note the input vertices can be part of the same edge. This will
 * result in a two edged face. This is desirable for advanced construction
 * tools and particularly essential for edge bevel. Because of this it is
 * up to the caller to decide what to do with the extra edge.
 *
 * \note If \a holes is NULL, then both faces will lose
 * all holes from the original face.  Also, you cannot split between
 * a hole vert and a boundary vert; that case is handled by higher-
 * level wrapping functions (when holes are fully implemented, anyway).
 *
 * \note that holes represents which holes goes to the new face, and of
 * course this requires removing them from the existing face first, since
 * you cannot have linked list links inside multiple lists.
 *
 * \return A BMFace pointer
 */
BMFace *bmesh_kernel_split_face_make_edge(BMesh *bm,
                                          BMFace *f,
                                          BMLoop *l_v1,
                                          BMLoop *l_v2,
                                          BMLoop **r_l,
#ifdef USE_BMESH_HOLES
                                          ListBase *holes,
#endif
                                          BMEdge *e_example,
                                          const bool no_double)
{
#ifdef USE_BMESH_HOLES
  BMLoopList *lst, *lst2;
#else
  int first_loop_f1;
#endif

  BMFace *f2;
  BMLoop *l_iter, *l_first;
  BMLoop *l_f1 = NULL, *l_f2 = NULL;
  BMEdge *e;
  BMVert *v1 = l_v1->v, *v2 = l_v2->v;
  int f1len, f2len;

  BLI_assert(f == l_v1->f && f == l_v2->f);

  /* allocate new edge between v1 and v2 */
  e = BM_edge_create(bm, v1, v2, e_example, no_double ? BM_CREATE_NO_DOUBLE : BM_CREATE_NOP);

  f2 = bm_face_create__sfme(bm, f);
  l_f1 = bm_loop_create(bm, v2, e, f, l_v2, 0);
  l_f2 = bm_loop_create(bm, v1, e, f2, l_v1, 0);

  l_f1->prev = l_v2->prev;
  l_f2->prev = l_v1->prev;
  l_v2->prev->next = l_f1;
  l_v1->prev->next = l_f2;

  l_f1->next = l_v1;
  l_f2->next = l_v2;
  l_v1->prev = l_f1;
  l_v2->prev = l_f2;

#ifdef USE_BMESH_HOLES
  lst = f->loops.first;
  lst2 = f2->loops.first;

  lst2->first = lst2->last = l_f2;
  lst->first = lst->last = l_f1;
#else
  /* find which of the faces the original first loop is in */
  l_iter = l_first = l_f1;
  first_loop_f1 = 0;
  do {
    if (l_iter == f->l_first) {
      first_loop_f1 = 1;
    }
  } while ((l_iter = l_iter->next) != l_first);

  if (first_loop_f1) {
    /* original first loop was in f1, find a suitable first loop for f2
     * which is as similar as possible to f1. the order matters for tools
     * such as duplifaces. */
    if (f->l_first->prev == l_f1) {
      f2->l_first = l_f2->prev;
    }
    else if (f->l_first->next == l_f1) {
      f2->l_first = l_f2->next;
    }
    else {
      f2->l_first = l_f2;
    }
  }
  else {
    /* original first loop was in f2, further do same as above */
    f2->l_first = f->l_first;

    if (f->l_first->prev == l_f2) {
      f->l_first = l_f1->prev;
    }
    else if (f->l_first->next == l_f2) {
      f->l_first = l_f1->next;
    }
    else {
      f->l_first = l_f1;
    }
  }
#endif

  /* validate both loop */
  /* I don't know how many loops are supposed to be in each face at this point! FIXME */

  /* go through all of f2's loops and make sure they point to it properly */
  l_iter = l_first = BM_FACE_FIRST_LOOP(f2);
  f2len = 0;
  do {
    l_iter->f = f2;
    f2len++;
  } while ((l_iter = l_iter->next) != l_first);

  /* link up the new loops into the new edges radial */
  bmesh_radial_loop_append(e, l_f1);
  bmesh_radial_loop_append(e, l_f2);

  f2->len = f2len;

  f1len = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    f1len++;
  } while ((l_iter = l_iter->next) != l_first);

  f->len = f1len;

  if (r_l) {
    *r_l = l_f2;
  }

#ifdef USE_BMESH_HOLES
  if (holes) {
    BLI_movelisttolist(&f2->loops, holes);
  }
  else {
    /* this code is not significant until holes actually work */
    // printf("warning: call to split face euler without holes argument; holes will be tossed.\n");
    for (lst = f->loops.last; lst != f->loops.first; lst = lst2) {
      lst2 = lst->prev;
      BLI_mempool_free(bm->looplistpool, lst);
    }
  }
#endif

  BM_CHECK_ELEMENT(e);
  BM_CHECK_ELEMENT(f);
  BM_CHECK_ELEMENT(f2);

  return f2;
}

/**
 * \brief Split Edge Make Vert (SEMV)
 *
 * Takes \a e edge and splits it into two, creating a new vert.
 * \a tv should be one end of \a e : the newly created edge
 * will be attached to that end and is returned in \a r_e.
 *
 * \par Examples:
 *
 * <pre>
 *                     E
 *     Before: OV-------------TV
 *                 E       RE
 *     After:  OV------NV-----TV
 * </pre>
 *
 * \return The newly created BMVert pointer.
 */
BMVert *bmesh_kernel_split_edge_make_vert(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e)
{
  BMLoop *l_next;
  BMEdge *e_new;
  BMVert *v_new, *v_old;
#ifndef NDEBUG
  int valence1, valence2;
  bool edok;
  int i;
#endif

  BLI_assert(BM_vert_in_edge(e, tv) != false);

  v_old = BM_edge_other_vert(e, tv);

#ifndef NDEBUG
  valence1 = bmesh_disk_count(v_old);
  valence2 = bmesh_disk_count(tv);
#endif

  /* order of 'e_new' verts should match 'e'
   * (so extruded faces don't flip) */
  v_new = BM_vert_create(bm, tv->co, tv, BM_CREATE_NOP);
  e_new = BM_edge_create(bm, tv, v_new, e, BM_CREATE_NOP);

  bmesh_disk_edge_remove(e_new, tv);
  bmesh_disk_edge_remove(e_new, v_new);

  bmesh_disk_vert_replace(e, v_new, tv);

  /* add e_new to v_new's disk cycle */
  bmesh_disk_edge_append(e_new, v_new);

  /* add e_new to tv's disk cycle */
  bmesh_disk_edge_append(e_new, tv);

#ifndef NDEBUG
  /* verify disk cycles */
  edok = bmesh_disk_validate(valence1, v_old->e, v_old);
  BMESH_ASSERT(edok != false);
  edok = bmesh_disk_validate(valence2, tv->e, tv);
  BMESH_ASSERT(edok != false);
  edok = bmesh_disk_validate(2, v_new->e, v_new);
  BMESH_ASSERT(edok != false);
#endif

  /* Split the radial cycle if present */
  l_next = e->l;
  e->l = NULL;
  if (l_next) {
    BMLoop *l_new, *l;
#ifndef NDEBUG
    int radlen = bmesh_radial_length(l_next);
#endif
    bool is_first = true;

    /* Take the next loop. Remove it from radial. Split it. Append to appropriate radials */
    while (l_next) {
      l = l_next;
      l->f->len++;
      l_next = l_next != l_next->radial_next ? l_next->radial_next : NULL;
      bmesh_radial_loop_unlink(l);

      l_new = bm_loop_create(bm, NULL, NULL, l->f, l, 0);
      l_new->prev = l;
      l_new->next = l->next;
      l_new->prev->next = l_new;
      l_new->next->prev = l_new;
      l_new->v = v_new;

      /* assign the correct edge to the correct loop */
      if (BM_verts_in_edge(l_new->v, l_new->next->v, e)) {
        l_new->e = e;
        l->e = e_new;

        /* append l into e_new's rad cycle */
        if (is_first) {
          is_first = false;
          l->radial_next = l->radial_prev = NULL;
        }

        bmesh_radial_loop_append(l_new->e, l_new);
        bmesh_radial_loop_append(l->e, l);
      }
      else if (BM_verts_in_edge(l_new->v, l_new->next->v, e_new)) {
        l_new->e = e_new;
        l->e = e;

        /* append l into e_new's rad cycle */
        if (is_first) {
          is_first = false;
          l->radial_next = l->radial_prev = NULL;
        }

        bmesh_radial_loop_append(l_new->e, l_new);
        bmesh_radial_loop_append(l->e, l);
      }
    }

#ifndef NDEBUG
    /* verify length of radial cycle */
    edok = bmesh_radial_validate(radlen, e->l);
    BMESH_ASSERT(edok != false);
    edok = bmesh_radial_validate(radlen, e_new->l);
    BMESH_ASSERT(edok != false);

    /* verify loop->v and loop->next->v pointers for e */
    for (i = 0, l = e->l; i < radlen; i++, l = l->radial_next) {
      BMESH_ASSERT(l->e == e);
      // BMESH_ASSERT(l->radial_next == l);
      BMESH_ASSERT(!(l->prev->e != e_new && l->next->e != e_new));

      edok = BM_verts_in_edge(l->v, l->next->v, e);
      BMESH_ASSERT(edok != false);
      BMESH_ASSERT(l->v != l->next->v);
      BMESH_ASSERT(l->e != l->next->e);

      /* verify loop cycle for kloop->f */
      BM_CHECK_ELEMENT(l);
      BM_CHECK_ELEMENT(l->v);
      BM_CHECK_ELEMENT(l->e);
      BM_CHECK_ELEMENT(l->f);
    }
    /* verify loop->v and loop->next->v pointers for e_new */
    for (i = 0, l = e_new->l; i < radlen; i++, l = l->radial_next) {
      BMESH_ASSERT(l->e == e_new);
      // BMESH_ASSERT(l->radial_next == l);
      BMESH_ASSERT(!(l->prev->e != e && l->next->e != e));
      edok = BM_verts_in_edge(l->v, l->next->v, e_new);
      BMESH_ASSERT(edok != false);
      BMESH_ASSERT(l->v != l->next->v);
      BMESH_ASSERT(l->e != l->next->e);

      BM_CHECK_ELEMENT(l);
      BM_CHECK_ELEMENT(l->v);
      BM_CHECK_ELEMENT(l->e);
      BM_CHECK_ELEMENT(l->f);
    }
#endif
  }

  BM_CHECK_ELEMENT(e_new);
  BM_CHECK_ELEMENT(v_new);
  BM_CHECK_ELEMENT(v_old);
  BM_CHECK_ELEMENT(e);
  BM_CHECK_ELEMENT(tv);

  if (r_e) {
    *r_e = e_new;
  }
  return v_new;
}

/**
 * \brief Join Edge Kill Vert (JEKV)
 *
 * Takes an edge \a e_kill and pointer to one of its vertices \a v_kill
 * and collapses the edge on that vertex.
 *
 * \par Examples:
 *
 * <pre>
 *     Before:    e_old  e_kill
 *              +-------+-------+
 *              |       |       |
 *              v_old   v_kill  v_target
 *
 *     After:           e_old
 *              +---------------+
 *              |               |
 *              v_old           v_target
 * </pre>
 *
 * \par Restrictions:
 * KV is a vertex that must have a valance of exactly two. Furthermore
 * both edges in KV's disk cycle (OE and KE) must be unique (no double edges).
 *
 * \return The resulting edge, NULL for failure.
 *
 * \note This euler has the possibility of creating
 * faces with just 2 edges. It is up to the caller to decide what to do with
 * these faces.
 */
BMEdge *bmesh_kernel_join_edge_kill_vert(BMesh *bm,
                                         BMEdge *e_kill,
                                         BMVert *v_kill,
                                         const bool do_del,
                                         const bool check_edge_double,
                                         const bool kill_degenerate_faces)
{
  BMEdge *e_old;
  BMVert *v_old, *v_target;
  BMLoop *l_kill;
#ifndef NDEBUG
  int radlen, i;
  bool edok;
#endif

  BLI_assert(BM_vert_in_edge(e_kill, v_kill));

  if (BM_vert_in_edge(e_kill, v_kill) == 0) {
    return NULL;
  }

  if (bmesh_disk_count_at_most(v_kill, 3) == 2) {
#ifndef NDEBUG
    int valence1, valence2;
    BMLoop *l;
#endif

    e_old = bmesh_disk_edge_next(e_kill, v_kill);
    v_target = BM_edge_other_vert(e_kill, v_kill);
    v_old = BM_edge_other_vert(e_old, v_kill);

    /* check for double edges */
    if (BM_verts_in_edge(v_kill, v_target, e_old)) {
      return NULL;
    }
    else {
      BMEdge *e_splice;
      BLI_SMALLSTACK_DECLARE(faces_degenerate, BMFace *);
      BMLoop *l_kill_next;

#ifndef NDEBUG
      /* For verification later, count valence of 'v_old' and 'v_target' */
      valence1 = bmesh_disk_count(v_old);
      valence2 = bmesh_disk_count(v_target);
#endif

      if (check_edge_double) {
        e_splice = BM_edge_exists(v_target, v_old);
      }

      bmesh_disk_vert_replace(e_old, v_target, v_kill);

      /* remove e_kill from 'v_target's disk cycle */
      bmesh_disk_edge_remove(e_kill, v_target);

#ifndef NDEBUG
      /* deal with radial cycle of e_kill */
      radlen = bmesh_radial_length(e_kill->l);
#endif
      if (e_kill->l) {

        /* fix the neighboring loops of all loops in e_kill's radial cycle */
        l_kill = e_kill->l;
        do {
          /* relink loops and fix vertex pointer */
          if (l_kill->next->v == v_kill) {
            l_kill->next->v = v_target;
          }

          l_kill->next->prev = l_kill->prev;
          l_kill->prev->next = l_kill->next;
          if (BM_FACE_FIRST_LOOP(l_kill->f) == l_kill) {
            BM_FACE_FIRST_LOOP(l_kill->f) = l_kill->next;
          }

          /* fix len attribute of face */
          l_kill->f->len--;
          if (kill_degenerate_faces) {
            if (l_kill->f->len < 3) {
              BLI_SMALLSTACK_PUSH(faces_degenerate, l_kill->f);
            }
          }
          l_kill_next = l_kill->radial_next;

          bm_kill_only_loop(bm, l_kill);

        } while ((l_kill = l_kill_next) != e_kill->l);
        /* `e_kill->l` is invalid but the edge is freed next. */
#ifndef NDEBUG
        /* Validate radial cycle of e_old */
        edok = bmesh_radial_validate(radlen, e_old->l);
        BMESH_ASSERT(edok != false);
#endif
      }
      /* deallocate edge */
      bm_kill_only_edge(bm, e_kill);

      /* deallocate vertex */
      if (do_del) {
        bm_kill_only_vert(bm, v_kill);
      }
      else {
        v_kill->e = NULL;
      }

#ifndef NDEBUG
      /* Validate disk cycle lengths of 'v_old', 'v_target' are unchanged */
      edok = bmesh_disk_validate(valence1, v_old->e, v_old);
      BMESH_ASSERT(edok != false);
      edok = bmesh_disk_validate(valence2, v_target->e, v_target);
      BMESH_ASSERT(edok != false);

      /* Validate loop cycle of all faces attached to 'e_old' */
      for (i = 0, l = e_old->l; i < radlen; i++, l = l->radial_next) {
        BMESH_ASSERT(l->e == e_old);
        edok = BM_verts_in_edge(l->v, l->next->v, e_old);
        BMESH_ASSERT(edok != false);
        edok = bmesh_loop_validate(l->f);
        BMESH_ASSERT(edok != false);

        BM_CHECK_ELEMENT(l);
        BM_CHECK_ELEMENT(l->v);
        BM_CHECK_ELEMENT(l->e);
        BM_CHECK_ELEMENT(l->f);
      }
#endif
      if (check_edge_double) {
        if (e_splice) {
          /* removes e_splice */
          BM_edge_splice(bm, e_old, e_splice);
        }
      }

      if (kill_degenerate_faces) {
        BMFace *f_kill;
        while ((f_kill = BLI_SMALLSTACK_POP(faces_degenerate))) {
          BM_face_kill(bm, f_kill);
        }
      }

      BM_CHECK_ELEMENT(v_old);
      BM_CHECK_ELEMENT(v_target);
      BM_CHECK_ELEMENT(e_old);

      return e_old;
    }
  }
  return NULL;
}

/**
 * \brief Join Vert Kill Edge (JVKE)
 *
 * Collapse an edge, merging surrounding data.
 *
 * Unlike #BM_vert_collapse_edge & #bmesh_kernel_join_edge_kill_vert
 * which only handle 2 valence verts,
 * this can handle any number of connected edges/faces.
 *
 * <pre>
 * Before: -> After:
 * +-+-+-+    +-+-+-+
 * | | | |    | \ / |
 * +-+-+-+    +--+--+
 * | | | |    | / \ |
 * +-+-+-+    +-+-+-+
 * </pre>
 */
BMVert *bmesh_kernel_join_vert_kill_edge(BMesh *bm,
                                         BMEdge *e_kill,
                                         BMVert *v_kill,
                                         const bool do_del,
                                         const bool check_edge_double,
                                         const bool kill_degenerate_faces)
{
  BLI_SMALLSTACK_DECLARE(faces_degenerate, BMFace *);
  BMVert *v_target = BM_edge_other_vert(e_kill, v_kill);

  BLI_assert(BM_vert_in_edge(e_kill, v_kill));

  if (e_kill->l) {
    BMLoop *l_kill, *l_first, *l_kill_next;
    l_kill = l_first = e_kill->l;
    do {
      /* relink loops and fix vertex pointer */
      if (l_kill->next->v == v_kill) {
        l_kill->next->v = v_target;
      }

      l_kill->next->prev = l_kill->prev;
      l_kill->prev->next = l_kill->next;
      if (BM_FACE_FIRST_LOOP(l_kill->f) == l_kill) {
        BM_FACE_FIRST_LOOP(l_kill->f) = l_kill->next;
      }

      /* fix len attribute of face */
      l_kill->f->len--;
      if (kill_degenerate_faces) {
        if (l_kill->f->len < 3) {
          BLI_SMALLSTACK_PUSH(faces_degenerate, l_kill->f);
        }
      }
      l_kill_next = l_kill->radial_next;

      bm_kill_only_loop(bm, l_kill);

    } while ((l_kill = l_kill_next) != l_first);

    e_kill->l = NULL;
  }

  BM_edge_kill(bm, e_kill);
  BM_CHECK_ELEMENT(v_kill);
  BM_CHECK_ELEMENT(v_target);

  if (v_target->e && v_kill->e) {
    /* inline BM_vert_splice(bm, v_target, v_kill); */
    BMEdge *e;
    while ((e = v_kill->e)) {
      BMEdge *e_target;

      if (check_edge_double) {
        e_target = BM_edge_exists(v_target, BM_edge_other_vert(e, v_kill));
      }

      bmesh_edge_vert_swap(e, v_target, v_kill);
      BLI_assert(e->v1 != e->v2);

      if (check_edge_double) {
        if (e_target) {
          BM_edge_splice(bm, e_target, e);
        }
      }
    }
  }

  if (kill_degenerate_faces) {
    BMFace *f_kill;
    while ((f_kill = BLI_SMALLSTACK_POP(faces_degenerate))) {
      BM_face_kill(bm, f_kill);
    }
  }

  if (do_del) {
    BLI_assert(v_kill->e == NULL);
    bm_kill_only_vert(bm, v_kill);
  }

  return v_target;
}

/**
 * \brief Join Face Kill Edge (JFKE)
 *
 * Takes two faces joined by a single 2-manifold edge and fuses them together.
 * The edge shared by the faces must not be connected to any other edges which have
 * Both faces in its radial cycle
 *
 * \par Examples:
 * <pre>
 *           A                   B
 *      +--------+           +--------+
 *      |        |           |        |
 *      |   f1   |           |   f1   |
 *     v1========v2 = Ok!    v1==V2==v3 == Wrong!
 *      |   f2   |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 * </pre>
 *
 * In the example A, faces \a f1 and \a f2 are joined by a single edge,
 * and the euler can safely be used.
 * In example B however, \a f1 and \a f2 are joined by multiple edges and will produce an error.
 * The caller in this case should call #bmesh_kernel_join_edge_kill_vert on the extra edges
 * before attempting to fuse \a f1 and \a f2.
 *
 * \note The order of arguments decides whether or not certain per-face attributes are present
 * in the resultant face. For instance vertex winding, material index, smooth flags,
 * etc are inherited from \a f1, not \a f2.
 *
 * \return A BMFace pointer
 */
BMFace *bmesh_kernel_join_face_kill_edge(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
  BMLoop *l_iter, *l_f1 = NULL, *l_f2 = NULL;
  int newlen = 0, i, f1len = 0, f2len = 0;
  bool edok;
  /* can't join a face to itself */
  if (f1 == f2) {
    return NULL;
  }

  /* validate that edge is 2-manifold edge */
  if (!BM_edge_is_manifold(e)) {
    return NULL;
  }

  /* verify that e is in both f1 and f2 */
  f1len = f1->len;
  f2len = f2->len;

  if (!((l_f1 = BM_face_edge_share_loop(f1, e)) && (l_f2 = BM_face_edge_share_loop(f2, e)))) {
    return NULL;
  }

  /* validate direction of f2's loop cycle is compatible */
  if (l_f1->v == l_f2->v) {
    return NULL;
  }

  /* validate that for each face, each vertex has another edge in its disk cycle that is
   * not e, and not shared. */
  if (BM_edge_in_face(l_f1->next->e, f2) || BM_edge_in_face(l_f1->prev->e, f2) ||
      BM_edge_in_face(l_f2->next->e, f1) || BM_edge_in_face(l_f2->prev->e, f1)) {
    return NULL;
  }

  /* validate only one shared edge */
  if (BM_face_share_edge_count(f1, f2) > 1) {
    return NULL;
  }

  /* validate no internal join */
  {
    bool is_dupe = false;

    /* TODO: skip clearing once this is ensured. */
    for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
      BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
    }

    for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < f1len; i++, l_iter = l_iter->next) {
      BM_elem_flag_set(l_iter->v, BM_ELEM_INTERNAL_TAG, l_iter != l_f1);
    }
    for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
      if (l_iter != l_f2) {
        /* as soon as a duplicate is found, bail out */
        if (BM_elem_flag_test(l_iter->v, BM_ELEM_INTERNAL_TAG)) {
          is_dupe = true;
          break;
        }
      }
    }
    /* Cleanup tags. */
    for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < f1len; i++, l_iter = l_iter->next) {
      BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
    }
    if (is_dupe) {
      return NULL;
    }
  }

  /* join the two loop */
  l_f1->prev->next = l_f2->next;
  l_f2->next->prev = l_f1->prev;

  l_f1->next->prev = l_f2->prev;
  l_f2->prev->next = l_f1->next;

  /* if l_f1 was baseloop, make l_f1->next the base. */
  if (BM_FACE_FIRST_LOOP(f1) == l_f1) {
    BM_FACE_FIRST_LOOP(f1) = l_f1->next;
  }

  /* increase length of f1 */
  f1->len += (f2->len - 2);

  /* make sure each loop points to the proper face */
  newlen = f1->len;
  for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < newlen; i++, l_iter = l_iter->next) {
    l_iter->f = f1;
  }

  /* remove edge from the disk cycle of its two vertices */
  bmesh_disk_edge_remove(l_f1->e, l_f1->e->v1);
  bmesh_disk_edge_remove(l_f1->e, l_f1->e->v2);

  /* deallocate edge and its two loops as well as f2 */
  if (bm->etoolflagpool) {
    BLI_mempool_free(bm->etoolflagpool, ((BMEdge_OFlag *)l_f1->e)->oflags);
  }
  BLI_mempool_free(bm->epool, l_f1->e);
  bm->totedge--;
  BLI_mempool_free(bm->lpool, l_f1);
  bm->totloop--;
  BLI_mempool_free(bm->lpool, l_f2);
  bm->totloop--;
  if (bm->ftoolflagpool) {
    BLI_mempool_free(bm->ftoolflagpool, ((BMFace_OFlag *)f2)->oflags);
  }
  BLI_mempool_free(bm->fpool, f2);
  bm->totface--;
  /* account for both above */
  bm->elem_index_dirty |= BM_EDGE | BM_LOOP | BM_FACE;

  BM_CHECK_ELEMENT(f1);

  /* validate the new loop cycle */
  edok = bmesh_loop_validate(f1);
  BMESH_ASSERT(edok != false);

  return f1;
}

/**
 * Check if splicing vertices would create any double edges.
 *
 * \note assume caller will handle case where verts share an edge.
 */
bool BM_vert_splice_check_double(BMVert *v_a, BMVert *v_b)
{
  bool is_double = false;

  BLI_assert(BM_edge_exists(v_a, v_b) == false);

  if (v_a->e && v_b->e) {
    BMEdge *e, *e_first;

#define VERT_VISIT _FLAG_WALK

    /* tag 'v_a' */
    e = e_first = v_a->e;
    do {
      BMVert *v_other = BM_edge_other_vert(e, v_a);
      BLI_assert(!BM_ELEM_API_FLAG_TEST(v_other, VERT_VISIT));
      BM_ELEM_API_FLAG_ENABLE(v_other, VERT_VISIT);
    } while ((e = BM_DISK_EDGE_NEXT(e, v_a)) != e_first);

    /* check 'v_b' connects to 'v_a' edges */
    e = e_first = v_b->e;
    do {
      BMVert *v_other = BM_edge_other_vert(e, v_b);
      if (BM_ELEM_API_FLAG_TEST(v_other, VERT_VISIT)) {
        is_double = true;
        break;
      }
    } while ((e = BM_DISK_EDGE_NEXT(e, v_b)) != e_first);

    /* cleanup */
    e = e_first = v_a->e;
    do {
      BMVert *v_other = BM_edge_other_vert(e, v_a);
      BLI_assert(BM_ELEM_API_FLAG_TEST(v_other, VERT_VISIT));
      BM_ELEM_API_FLAG_DISABLE(v_other, VERT_VISIT);
    } while ((e = BM_DISK_EDGE_NEXT(e, v_a)) != e_first);

#undef VERT_VISIT
  }

  return is_double;
}

/**
 * \brief Splice Vert
 *
 * Merges two verts into one
 * (\a v_src into \a v_dst, removing \a v_src).
 *
 * \return Success
 *
 * \warning This doesn't work for collapsing edges,
 * where \a v and \a vtarget are connected by an edge
 * (assert checks for this case).
 */
bool BM_vert_splice(BMesh *bm, BMVert *v_dst, BMVert *v_src)
{
  BMEdge *e;

  /* verts already spliced */
  if (v_src == v_dst) {
    return false;
  }

  BLI_assert(BM_vert_pair_share_face_check(v_src, v_dst) == false);

  /* move all the edges from 'v_src' disk to 'v_dst' */
  while ((e = v_src->e)) {
    bmesh_edge_vert_swap(e, v_dst, v_src);
    BLI_assert(e->v1 != e->v2);
  }

  BM_CHECK_ELEMENT(v_src);
  BM_CHECK_ELEMENT(v_dst);

  /* 'v_src' is unused now, and can be killed */
  BM_vert_kill(bm, v_src);

  return true;
}

/** \name BM_vert_separate, bmesh_kernel_vert_separate and friends
 * \{ */

/* BM_edge_face_count(e) >= 1 */
BLI_INLINE bool bm_edge_supports_separate(const BMEdge *e)
{
  return (e->l && e->l->radial_next != e->l);
}

/**
 * \brief Separate Vert
 *
 * Separates all disjoint fans that meet at a vertex, making a unique
 * vertex for each region. returns an array of all resulting vertices.
 *
 * \note this is a low level function, bm_edge_separate needs to run on edges first
 * or, the faces sharing verts must not be sharing edges for them to split at least.
 *
 * \return Success
 */
void bmesh_kernel_vert_separate(
    BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len, const bool copy_select)
{
  int v_edges_num = 0;

  /* Detailed notes on array use since this is stack memory, we have to be careful */

  /* newly created vertices, only use when 'r_vout' is set
   * (total size will be number of fans) */
  BLI_SMALLSTACK_DECLARE(verts_new, BMVert *);
  /* fill with edges from the face-fan, clearing on completion
   * (total size will be max fan edge count) */
  BLI_SMALLSTACK_DECLARE(edges, BMEdge *);
  /* temp store edges to walk over when filling 'edges',
   * (total size will be max radial edges of any edge) */
  BLI_SMALLSTACK_DECLARE(edges_search, BMEdge *);

  /* number of resulting verts, include self */
  int verts_num = 1;
  /* track the total number of edges handled, so we know when we've found the last fan */
  int edges_found = 0;

#define EDGE_VISIT _FLAG_WALK

  /* count and flag at once */
  if (v->e) {
    BMEdge *e_first, *e_iter;
    e_iter = e_first = v->e;
    do {
      v_edges_num += 1;

      BLI_assert(!BM_ELEM_API_FLAG_TEST(e_iter, EDGE_VISIT));
      BM_ELEM_API_FLAG_ENABLE(e_iter, EDGE_VISIT);
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }

  while (true) {
    /* Considering only edges and faces incident on vertex v, walk
     * the edges & collect in the 'edges' list for splitting */

    BMEdge *e = v->e;
    BM_ELEM_API_FLAG_DISABLE(e, EDGE_VISIT);

    do {
      BLI_assert(!BM_ELEM_API_FLAG_TEST(e, EDGE_VISIT));
      BLI_SMALLSTACK_PUSH(edges, e);
      edges_found += 1;

      if (e->l) {
        BMLoop *l_iter, *l_first;
        l_iter = l_first = e->l;
        do {
          BMLoop *l_adjacent = (l_iter->v == v) ? l_iter->prev : l_iter->next;
          BLI_assert(BM_vert_in_edge(l_adjacent->e, v));
          if (BM_ELEM_API_FLAG_TEST(l_adjacent->e, EDGE_VISIT)) {
            BM_ELEM_API_FLAG_DISABLE(l_adjacent->e, EDGE_VISIT);
            BLI_SMALLSTACK_PUSH(edges_search, l_adjacent->e);
          }
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
    } while ((e = BLI_SMALLSTACK_POP(edges_search)));

    /* now we have all edges connected to 'v->e' */

    BLI_assert(edges_found <= v_edges_num);

    if (edges_found == v_edges_num) {
      /* We're done! The remaining edges in 'edges' form the last fan,
       * which can be left as is.
       * if 'edges' were alloc'd it'd be freed here. */
      break;
    }
    else {
      BMVert *v_new;

      v_new = BM_vert_create(bm, v->co, v, BM_CREATE_NOP);
      if (copy_select) {
        BM_elem_select_copy(bm, v_new, v);
      }

      while ((e = BLI_SMALLSTACK_POP(edges))) {
        bmesh_edge_vert_swap(e, v_new, v);
      }

      if (r_vout) {
        BLI_SMALLSTACK_PUSH(verts_new, v_new);
      }
      verts_num += 1;
    }
  }

#undef EDGE_VISIT

  /* flags are clean now, handle return values */

  if (r_vout_len != NULL) {
    *r_vout_len = verts_num;
  }

  if (r_vout != NULL) {
    BMVert **verts;

    verts = MEM_mallocN(sizeof(BMVert *) * verts_num, __func__);
    *r_vout = verts;

    verts[0] = v;
    BLI_SMALLSTACK_AS_TABLE(verts_new, &verts[1]);
  }
}

/**
 * Utility function for #BM_vert_separate
 *
 * Takes a list of edges, which have been split from their original.
 *
 * Any edges which failed to split off in #bmesh_kernel_vert_separate
 * will be merged back into the original edge.
 *
 * \param edges_separate:
 * A list-of-lists, each list is from a single original edge (the first edge is the original),
 * Check for duplicates (not just with the first) but between all.
 * This is O(n2) but radial edges are very rarely >2 and almost never >~10.
 *
 * \note typically its best to avoid creating the data in the first place,
 * but inspecting all loops connectivity is quite involved.
 *
 * \note this function looks like it could become slow,
 * but in common cases its only going to iterate a few times.
 */
static void bmesh_kernel_vert_separate__cleanup(BMesh *bm, LinkNode *edges_separate)
{
  do {
    LinkNode *n_orig = edges_separate->link;
    do {
      LinkNode *n_prev = n_orig;
      LinkNode *n_step = n_orig->next;
      BMEdge *e_orig = n_orig->link;
      do {
        BMEdge *e = n_step->link;
        BLI_assert(e != e_orig);
        if ((e->v1 == e_orig->v1) && (e->v2 == e_orig->v2) && BM_edge_splice(bm, e_orig, e)) {
          /* don't visit again */
          n_prev->next = n_step->next;
        }
        else {
          n_prev = n_step;
        }
      } while ((n_step = n_step->next));

    } while ((n_orig = n_orig->next) && n_orig->next);
  } while ((edges_separate = edges_separate->next));
}

/**
 * High level function which wraps both #bmesh_kernel_vert_separate and #bmesh_kernel_edge_separate
 */
void BM_vert_separate(BMesh *bm,
                      BMVert *v,
                      BMEdge **e_in,
                      int e_in_len,
                      const bool copy_select,
                      BMVert ***r_vout,
                      int *r_vout_len)
{
  LinkNode *edges_separate = NULL;
  int i;

  for (i = 0; i < e_in_len; i++) {
    BMEdge *e = e_in[i];
    if (bm_edge_supports_separate(e)) {
      LinkNode *edges_orig = NULL;
      do {
        BMLoop *l_sep = e->l;
        bmesh_kernel_edge_separate(bm, e, l_sep, copy_select);
        BLI_linklist_prepend_alloca(&edges_orig, l_sep->e);
        BLI_assert(e != l_sep->e);
      } while (bm_edge_supports_separate(e));
      BLI_linklist_prepend_alloca(&edges_orig, e);
      BLI_linklist_prepend_alloca(&edges_separate, edges_orig);
    }
  }

  bmesh_kernel_vert_separate(bm, v, r_vout, r_vout_len, copy_select);

  if (edges_separate) {
    bmesh_kernel_vert_separate__cleanup(bm, edges_separate);
  }
}

/**
 * A version of #BM_vert_separate which takes a flag.
 */
void BM_vert_separate_hflag(BMesh *bm,
                            BMVert *v,
                            const char hflag,
                            const bool copy_select,
                            BMVert ***r_vout,
                            int *r_vout_len)
{
  LinkNode *edges_separate = NULL;
  BMEdge *e_iter, *e_first;

  e_iter = e_first = v->e;
  do {
    if (BM_elem_flag_test(e_iter, hflag)) {
      BMEdge *e = e_iter;
      if (bm_edge_supports_separate(e)) {
        LinkNode *edges_orig = NULL;
        do {
          BMLoop *l_sep = e->l;
          bmesh_kernel_edge_separate(bm, e, l_sep, copy_select);
          /* trick to avoid looping over separated edges */
          if (edges_separate == NULL && edges_orig == NULL) {
            e_first = l_sep->e;
          }
          BLI_linklist_prepend_alloca(&edges_orig, l_sep->e);
          BLI_assert(e != l_sep->e);
        } while (bm_edge_supports_separate(e));
        BLI_linklist_prepend_alloca(&edges_orig, e);
        BLI_linklist_prepend_alloca(&edges_separate, edges_orig);
      }
    }
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);

  bmesh_kernel_vert_separate(bm, v, r_vout, r_vout_len, copy_select);

  if (edges_separate) {
    bmesh_kernel_vert_separate__cleanup(bm, edges_separate);
  }
}

void BM_vert_separate_tested_edges(BMesh *UNUSED(bm),
                                   BMVert *v_dst,
                                   BMVert *v_src,
                                   bool (*testfn)(BMEdge *, void *arg),
                                   void *arg)
{
  LinkNode *edges_hflag = NULL;
  BMEdge *e_iter, *e_first;

  e_iter = e_first = v_src->e;
  do {
    if (testfn(e_iter, arg)) {
      BLI_linklist_prepend_alloca(&edges_hflag, e_iter);
    }
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v_src)) != e_first);

  if (edges_hflag) {
    do {
      e_iter = edges_hflag->link;
      bmesh_disk_vert_replace(e_iter, v_dst, v_src);
    } while ((edges_hflag = edges_hflag->next));
  }
}

/** \} */

/**
 * \brief Splice Edge
 *
 * Splice two unique edges which share the same two vertices into one edge.
 *  (\a e_src into \a e_dst, removing e_src).
 *
 * \return Success
 *
 * \note Edges must already have the same vertices.
 */
bool BM_edge_splice(BMesh *bm, BMEdge *e_dst, BMEdge *e_src)
{
  BMLoop *l;

  if (!BM_vert_in_edge(e_src, e_dst->v1) || !BM_vert_in_edge(e_src, e_dst->v2)) {
    /* not the same vertices can't splice */

    /* the caller should really make sure this doesn't happen ever
     * so assert on release builds */
    BLI_assert(0);

    return false;
  }

  while (e_src->l) {
    l = e_src->l;
    BLI_assert(BM_vert_in_edge(e_dst, l->v));
    BLI_assert(BM_vert_in_edge(e_dst, l->next->v));
    bmesh_radial_loop_remove(e_src, l);
    bmesh_radial_loop_append(e_dst, l);
  }

  BLI_assert(bmesh_radial_length(e_src->l) == 0);

  BM_CHECK_ELEMENT(e_src);
  BM_CHECK_ELEMENT(e_dst);

  /* removes from disks too */
  BM_edge_kill(bm, e_src);

  return true;
}

/**
 * \brief Separate Edge
 *
 * Separates a single edge into two edge: the original edge and
 * a new edge that has only \a l_sep in its radial.
 *
 * \return Success
 *
 * \note Does nothing if \a l_sep is already the only loop in the
 * edge radial.
 */
void bmesh_kernel_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep, const bool copy_select)
{
  BMEdge *e_new;
#ifndef NDEBUG
  const int radlen = bmesh_radial_length(e->l);
#endif

  BLI_assert(l_sep->e == e);
  BLI_assert(e->l);

  if (BM_edge_is_boundary(e)) {
    BLI_assert(0); /* no cut required */
    return;
  }

  if (l_sep == e->l) {
    e->l = l_sep->radial_next;
  }

  e_new = BM_edge_create(bm, e->v1, e->v2, e, BM_CREATE_NOP);
  bmesh_radial_loop_remove(e, l_sep);
  bmesh_radial_loop_append(e_new, l_sep);
  l_sep->e = e_new;

  if (copy_select) {
    BM_elem_select_copy(bm, e_new, e);
  }

  BLI_assert(bmesh_radial_length(e->l) == radlen - 1);
  BLI_assert(bmesh_radial_length(e_new->l) == 1);

  BM_CHECK_ELEMENT(e_new);
  BM_CHECK_ELEMENT(e);
}

/**
 * \brief Un-glue Region Make Vert (URMV)
 *
 * Disconnects a face from its vertex fan at loop \a l_sep
 *
 * \return The newly created BMVert
 *
 * \note Will be a no-op and return original vertex if only two edges at that vertex.
 */
BMVert *bmesh_kernel_unglue_region_make_vert(BMesh *bm, BMLoop *l_sep)
{
  BMVert *v_new = NULL;
  BMVert *v_sep = l_sep->v;
  BMEdge *e_iter;
  BMEdge *edges[2];
  int i;

  /* peel the face from the edge radials on both sides of the
   * loop vert, disconnecting the face from its fan */
  if (!BM_edge_is_boundary(l_sep->e)) {
    bmesh_kernel_edge_separate(bm, l_sep->e, l_sep, false);
  }
  if (!BM_edge_is_boundary(l_sep->prev->e)) {
    bmesh_kernel_edge_separate(bm, l_sep->prev->e, l_sep->prev, false);
  }

  /* do inline, below */
#if 0
  if (BM_vert_edge_count_is_equal(v_sep, 2)) {
    return v_sep;
  }
#endif

  /* Search for an edge unattached to this loop */
  e_iter = v_sep->e;
  while (!ELEM(e_iter, l_sep->e, l_sep->prev->e)) {
    e_iter = bmesh_disk_edge_next(e_iter, v_sep);

    /* We've come back around to the initial edge, all touch this loop.
     * If there are still only two edges out of v_sep,
     * then this whole URMV was just a no-op, so exit now. */
    if (e_iter == v_sep->e) {
      BLI_assert(BM_vert_edge_count_is_equal(v_sep, 2));
      return v_sep;
    }
  }

  v_sep->e = l_sep->e;

  v_new = BM_vert_create(bm, v_sep->co, v_sep, BM_CREATE_NOP);

  edges[0] = l_sep->e;
  edges[1] = l_sep->prev->e;

  for (i = 0; i < ARRAY_SIZE(edges); i++) {
    BMEdge *e = edges[i];
    bmesh_edge_vert_swap(e, v_new, v_sep);
  }

  BLI_assert(v_sep != l_sep->v);
  BLI_assert(v_sep->e != l_sep->v->e);

  BM_CHECK_ELEMENT(l_sep);
  BM_CHECK_ELEMENT(v_sep);
  BM_CHECK_ELEMENT(edges[0]);
  BM_CHECK_ELEMENT(edges[1]);
  BM_CHECK_ELEMENT(v_new);

  return v_new;
}

/**
 * A version of #bmesh_kernel_unglue_region_make_vert that disconnects multiple loops at once.
 * The loops must all share the same vertex, can be in any order
 * and are all moved to use a single new vertex - which is returned.
 *
 * This function handles the details of finding fans boundaries.
 */
BMVert *bmesh_kernel_unglue_region_make_vert_multi(BMesh *bm, BMLoop **larr, int larr_len)
{
  BMVert *v_sep = larr[0]->v;
  BMVert *v_new;
  int edges_len = 0;
  int i;
  /* any edges not owned by 'larr' loops connected to 'v_sep'? */
  bool is_mixed_edge_any = false;
  /* any loops not owned by 'larr' radially connected to 'larr' loop edges? */
  bool is_mixed_loop_any = false;

#define LOOP_VISIT _FLAG_WALK
#define EDGE_VISIT _FLAG_WALK

  for (i = 0; i < larr_len; i++) {
    BMLoop *l_sep = larr[i];

    /* all must be from the same vert! */
    BLI_assert(v_sep == l_sep->v);

    BLI_assert(!BM_ELEM_API_FLAG_TEST(l_sep, LOOP_VISIT));
    BM_ELEM_API_FLAG_ENABLE(l_sep, LOOP_VISIT);

    /* weak! but it makes it simpler to check for edges to split
     * while doing a radial loop (where loops may be adjacent) */
    BM_ELEM_API_FLAG_ENABLE(l_sep->next, LOOP_VISIT);
    BM_ELEM_API_FLAG_ENABLE(l_sep->prev, LOOP_VISIT);

    BMLoop *loop_pair[2] = {l_sep, l_sep->prev};
    for (int j = 0; j < ARRAY_SIZE(loop_pair); j++) {
      BMEdge *e = loop_pair[j]->e;
      if (!BM_ELEM_API_FLAG_TEST(e, EDGE_VISIT)) {
        BM_ELEM_API_FLAG_ENABLE(e, EDGE_VISIT);
        edges_len += 1;
      }
    }
  }

  BMEdge **edges = BLI_array_alloca(edges, edges_len);
  STACK_DECLARE(edges);

  STACK_INIT(edges, edges_len);

  {
    BMEdge *e_first, *e_iter;
    e_iter = e_first = v_sep->e;
    do {
      if (BM_ELEM_API_FLAG_TEST(e_iter, EDGE_VISIT)) {
        BMLoop *l_iter, *l_first;
        bool is_mixed_loop = false;

        l_iter = l_first = e_iter->l;
        do {
          if (!BM_ELEM_API_FLAG_TEST(l_iter, LOOP_VISIT)) {
            is_mixed_loop = true;
            break;
          }
        } while ((l_iter = l_iter->radial_next) != l_first);

        if (is_mixed_loop) {
          /* ensure the first loop is one we don't own so we can do a quick check below
           * on the edge's loop-flag to see if the edge is mixed or not. */
          e_iter->l = l_iter;

          is_mixed_loop_any = true;
        }

        STACK_PUSH(edges, e_iter);
      }
      else {
        /* at least one edge attached isn't connected to our loops */
        is_mixed_edge_any = true;
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v_sep)) != e_first);
  }

  BLI_assert(edges_len == STACK_SIZE(edges));

  if (is_mixed_loop_any == false && is_mixed_edge_any == false) {
    /* all loops in 'larr' are the sole owners of their edges.
     * nothing to split away from, this is a no-op */
    v_new = v_sep;
  }
  else {
    v_new = BM_vert_create(bm, v_sep->co, v_sep, BM_CREATE_NOP);

    for (i = 0; i < STACK_SIZE(edges); i++) {
      BMEdge *e = edges[i];
      BMLoop *l_iter, *l_first, *l_next;
      BMEdge *e_new;

      /* disable so copied edge isn't left dirty (loop edges are cleared last too) */
      BM_ELEM_API_FLAG_DISABLE(e, EDGE_VISIT);

      /* will always be false when (is_mixed_loop_any == false) */
      if (!BM_ELEM_API_FLAG_TEST(e->l, LOOP_VISIT)) {
        /* edge has some loops owned by us, some owned by other loops */
        BMVert *e_new_v_pair[2];

        if (e->v1 == v_sep) {
          e_new_v_pair[0] = v_new;
          e_new_v_pair[1] = e->v2;
        }
        else {
          BLI_assert(v_sep == e->v2);
          e_new_v_pair[0] = e->v1;
          e_new_v_pair[1] = v_new;
        }

        e_new = BM_edge_create(bm, UNPACK2(e_new_v_pair), e, BM_CREATE_NOP);

        /* now moved all loops from 'larr' to this newly created edge */
        l_iter = l_first = e->l;
        do {
          l_next = l_iter->radial_next;
          if (BM_ELEM_API_FLAG_TEST(l_iter, LOOP_VISIT)) {
            bmesh_radial_loop_remove(e, l_iter);
            bmesh_radial_loop_append(e_new, l_iter);
            l_iter->e = e_new;
          }
        } while ((l_iter = l_next) != l_first);
      }
      else {
        /* we own the edge entirely, replace the vert */
        bmesh_disk_vert_replace(e, v_new, v_sep);
      }

      /* loop vert is handled last! */
    }
  }

  for (i = 0; i < larr_len; i++) {
    BMLoop *l_sep = larr[i];

    l_sep->v = v_new;

    BLI_assert(BM_ELEM_API_FLAG_TEST(l_sep, LOOP_VISIT));
    BLI_assert(BM_ELEM_API_FLAG_TEST(l_sep->prev, LOOP_VISIT));
    BLI_assert(BM_ELEM_API_FLAG_TEST(l_sep->next, LOOP_VISIT));
    BM_ELEM_API_FLAG_DISABLE(l_sep, LOOP_VISIT);
    BM_ELEM_API_FLAG_DISABLE(l_sep->prev, LOOP_VISIT);
    BM_ELEM_API_FLAG_DISABLE(l_sep->next, LOOP_VISIT);

    BM_ELEM_API_FLAG_DISABLE(l_sep->prev->e, EDGE_VISIT);
    BM_ELEM_API_FLAG_DISABLE(l_sep->e, EDGE_VISIT);
  }

#undef LOOP_VISIT
#undef EDGE_VISIT

  return v_new;
}

static void bmesh_edge_vert_swap__recursive(BMEdge *e, BMVert *v_dst, BMVert *v_src)
{
  BMLoop *l_iter, *l_first;

  BLI_assert(ELEM(v_src, e->v1, e->v2));
  bmesh_disk_vert_replace(e, v_dst, v_src);

  l_iter = l_first = e->l;
  do {
    if (l_iter->v == v_src) {
      l_iter->v = v_dst;
      if (BM_vert_in_edge(l_iter->prev->e, v_src)) {
        bmesh_edge_vert_swap__recursive(l_iter->prev->e, v_dst, v_src);
      }
    }
    else if (l_iter->next->v == v_src) {
      l_iter->next->v = v_dst;
      if (BM_vert_in_edge(l_iter->next->e, v_src)) {
        bmesh_edge_vert_swap__recursive(l_iter->next->e, v_dst, v_src);
      }
    }
    else {
      BLI_assert(l_iter->prev->v != v_src);
    }
  } while ((l_iter = l_iter->radial_next) != l_first);
}

/**
 * This function assumes l_sep is apart of a larger fan which has already been
 * isolated by calling #bmesh_kernel_edge_separate to segregate it radially.
 */
BMVert *bmesh_kernel_unglue_region_make_vert_multi_isolated(BMesh *bm, BMLoop *l_sep)
{
  BMVert *v_new = BM_vert_create(bm, l_sep->v->co, l_sep->v, BM_CREATE_NOP);
  /* passing either 'l_sep->e', 'l_sep->prev->e' will work */
  bmesh_edge_vert_swap__recursive(l_sep->e, v_new, l_sep->v);
  BLI_assert(l_sep->v == v_new);
  return v_new;
}

/**
 * Avoid calling this where possible,
 * low level function so both face pointers remain intact but point to swapped data.
 * \note must be from the same bmesh.
 */
void bmesh_face_swap_data(BMFace *f_a, BMFace *f_b)
{
  BMLoop *l_iter, *l_first;

  BLI_assert(f_a != f_b);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
  do {
    l_iter->f = f_b;
  } while ((l_iter = l_iter->next) != l_first);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f_b);
  do {
    l_iter->f = f_a;
  } while ((l_iter = l_iter->next) != l_first);

  SWAP(BMFace, (*f_a), (*f_b));

  /* swap back */
  SWAP(void *, f_a->head.data, f_b->head.data);
  SWAP(int, f_a->head.index, f_b->head.index);
}

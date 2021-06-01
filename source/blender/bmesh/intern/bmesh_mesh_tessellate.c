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
 * This file contains code for polygon tessellation
 * (creating triangles from polygons).
 */

#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_heap.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"

#include "bmesh.h"
#include "bmesh_tools.h"

/**
 * \brief BM_mesh_calc_tessellation get the looptris and its number from a certain bmesh
 * \param looptris:
 *
 * \note \a looptris Must be pre-allocated to at least the size of given by: poly_to_tri_count
 */
void BM_mesh_calc_tessellation(BMesh *bm, BMLoop *(*looptris)[3])
{
  /* Avoid polygon filling logic for 3-4 sided faces. */
#define USE_TESSFACE_SPEEDUP

#ifndef NDEBUG
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
#endif

  BMIter iter;
  BMFace *efa;
  int i = 0;

  MemArena *arena = NULL;

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    BLI_assert(efa->len >= 3);
#ifdef USE_TESSFACE_SPEEDUP
    if (efa->len == 3) {
      /* `0 1 2` -> `0 1 2` */
      BMLoop *l;
      BMLoop **l_ptr = looptris[i++];
      l_ptr[0] = l = BM_FACE_FIRST_LOOP(efa);
      l_ptr[1] = l = l->next;
      l_ptr[2] = l->next;
    }
    else if (efa->len == 4) {
      /* `0 1 2 3` -> (`0 1 2`, `0 2 3`) */
      BMLoop *l;
      BMLoop **l_ptr_a = looptris[i++];
      BMLoop **l_ptr_b = looptris[i++];
      (l_ptr_a[0] = l_ptr_b[0] = l = BM_FACE_FIRST_LOOP(efa));
      (l_ptr_a[1] = l = l->next);
      (l_ptr_a[2] = l_ptr_b[1] = l = l->next);
      (l_ptr_b[2] = l->next);

      if (UNLIKELY(is_quad_flip_v3_first_third_fast(
              l_ptr_a[0]->v->co, l_ptr_a[1]->v->co, l_ptr_a[2]->v->co, l_ptr_b[2]->v->co))) {
        /* flip out of degenerate 0-2 state. */
        l_ptr_a[2] = l_ptr_b[2];
        l_ptr_b[0] = l_ptr_a[1];
      }
    }
    else
#endif /* USE_TESSFACE_SPEEDUP */
    {
      int j;

      BMLoop *l_iter;
      BMLoop *l_first;
      BMLoop **l_arr;

      float axis_mat[3][3];
      float(*projverts)[2];
      uint(*tris)[3];

      const int totfilltri = efa->len - 2;

      if (UNLIKELY(arena == NULL)) {
        arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = BLI_memarena_alloc(arena, sizeof(*tris) * totfilltri);
      l_arr = BLI_memarena_alloc(arena, sizeof(*l_arr) * efa->len);
      projverts = BLI_memarena_alloc(arena, sizeof(*projverts) * efa->len);

      axis_dominant_v3_to_m3_negate(axis_mat, efa->no);

      j = 0;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        l_arr[j] = l_iter;
        mul_v2_m3v3(projverts[j], axis_mat, l_iter->v->co);
        j++;
      } while ((l_iter = l_iter->next) != l_first);

      BLI_polyfill_calc_arena(projverts, efa->len, 1, tris, arena);

      for (j = 0; j < totfilltri; j++) {
        BMLoop **l_ptr = looptris[i++];
        uint *tri = tris[j];

        l_ptr[0] = l_arr[tri[0]];
        l_ptr[1] = l_arr[tri[1]];
        l_ptr[2] = l_arr[tri[2]];
      }

      BLI_memarena_clear(arena);
    }
  }

  if (arena) {
    BLI_memarena_free(arena);
    arena = NULL;
  }

  BLI_assert(i <= looptris_tot);

#undef USE_TESSFACE_SPEEDUP
}

/**
 * A version of #BM_mesh_calc_tessellation that avoids degenerate triangles.
 */
void BM_mesh_calc_tessellation_beauty(BMesh *bm, BMLoop *(*looptris)[3])
{
#ifndef NDEBUG
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
#endif

  BMIter iter;
  BMFace *efa;
  int i = 0;

  MemArena *pf_arena = NULL;

  /* use_beauty */
  Heap *pf_heap = NULL;

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    BLI_assert(efa->len >= 3);

    if (efa->len == 3) {
      BMLoop *l;
      BMLoop **l_ptr = looptris[i++];
      l_ptr[0] = l = BM_FACE_FIRST_LOOP(efa);
      l_ptr[1] = l = l->next;
      l_ptr[2] = l->next;
    }
    else if (efa->len == 4) {
      BMLoop *l_v1 = BM_FACE_FIRST_LOOP(efa);
      BMLoop *l_v2 = l_v1->next;
      BMLoop *l_v3 = l_v2->next;
      BMLoop *l_v4 = l_v1->prev;

      /* #BM_verts_calc_rotate_beauty performs excessive checks we don't need!
       * It's meant for rotating edges, it also calculates a new normal.
       *
       * Use #BLI_polyfill_beautify_quad_rotate_calc since we have the normal.
       */
#if 0
      const bool split_13 = (BM_verts_calc_rotate_beauty(
                                 l_v1->v, l_v2->v, l_v3->v, l_v4->v, 0, 0) < 0.0f);
#else
      float axis_mat[3][3], v_quad[4][2];
      axis_dominant_v3_to_m3(axis_mat, efa->no);
      mul_v2_m3v3(v_quad[0], axis_mat, l_v1->v->co);
      mul_v2_m3v3(v_quad[1], axis_mat, l_v2->v->co);
      mul_v2_m3v3(v_quad[2], axis_mat, l_v3->v->co);
      mul_v2_m3v3(v_quad[3], axis_mat, l_v4->v->co);

      const bool split_13 = BLI_polyfill_beautify_quad_rotate_calc(
                                v_quad[0], v_quad[1], v_quad[2], v_quad[3]) < 0.0f;
#endif

      BMLoop **l_ptr_a = looptris[i++];
      BMLoop **l_ptr_b = looptris[i++];
      if (split_13) {
        l_ptr_a[0] = l_v1;
        l_ptr_a[1] = l_v2;
        l_ptr_a[2] = l_v3;

        l_ptr_b[0] = l_v1;
        l_ptr_b[1] = l_v3;
        l_ptr_b[2] = l_v4;
      }
      else {
        l_ptr_a[0] = l_v1;
        l_ptr_a[1] = l_v2;
        l_ptr_a[2] = l_v4;

        l_ptr_b[0] = l_v2;
        l_ptr_b[1] = l_v3;
        l_ptr_b[2] = l_v4;
      }
    }
    else {
      int j;

      BMLoop *l_iter;
      BMLoop *l_first;
      BMLoop **l_arr;

      float axis_mat[3][3];
      float(*projverts)[2];
      unsigned int(*tris)[3];

      const int totfilltri = efa->len - 2;

      if (UNLIKELY(pf_arena == NULL)) {
        pf_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
        pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
      }

      tris = BLI_memarena_alloc(pf_arena, sizeof(*tris) * totfilltri);
      l_arr = BLI_memarena_alloc(pf_arena, sizeof(*l_arr) * efa->len);
      projverts = BLI_memarena_alloc(pf_arena, sizeof(*projverts) * efa->len);

      axis_dominant_v3_to_m3_negate(axis_mat, efa->no);

      j = 0;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        l_arr[j] = l_iter;
        mul_v2_m3v3(projverts[j], axis_mat, l_iter->v->co);
        j++;
      } while ((l_iter = l_iter->next) != l_first);

      BLI_polyfill_calc_arena(projverts, efa->len, 1, tris, pf_arena);

      BLI_polyfill_beautify(projverts, efa->len, tris, pf_arena, pf_heap);

      for (j = 0; j < totfilltri; j++) {
        BMLoop **l_ptr = looptris[i++];
        unsigned int *tri = tris[j];

        l_ptr[0] = l_arr[tri[0]];
        l_ptr[1] = l_arr[tri[1]];
        l_ptr[2] = l_arr[tri[2]];
      }

      BLI_memarena_clear(pf_arena);
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);

    BLI_heap_free(pf_heap, NULL);
  }

  BLI_assert(i <= looptris_tot);
}

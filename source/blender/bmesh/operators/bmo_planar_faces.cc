/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Iteratively flatten 4+ sided faces.
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

#define ELE_VERT_ADJUST (1 << 0)
#define ELE_FACE_ADJUST (1 << 1)

struct VertAccum {
  float co[3];
  int co_tot;
};

void bmo_planar_faces_exec(BMesh *bm, BMOperator *op)
{
  const float fac = BMO_slot_float_get(op->slots_in, "factor");
  const int iterations = BMO_slot_int_get(op->slots_in, "iterations");
  const int faces_num = BMO_slot_buffer_len(op->slots_in, "faces");

  const float eps = 0.00001f;
  const float eps_sq = square_f(eps);

  BMOIter oiter;
  BMFace *f;
  BLI_mempool *vert_accum_pool;
  GHash *vaccum_map;
  float (*faces_center)[3];
  int i, iter_step, shared_vert_num;

  faces_center = static_cast<float (*)[3]>(
      MEM_mallocN(sizeof(*faces_center) * faces_num, __func__));

  shared_vert_num = 0;
  BMO_ITER_INDEX (f, &oiter, op->slots_in, "faces", BM_FACE, i) {
    BMLoop *l_iter, *l_first;

    if (f->len == 3) {
      continue;
    }

    BM_face_calc_center_median_weighted(f, faces_center[i]);

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (!BMO_vert_flag_test(bm, l_iter->v, ELE_VERT_ADJUST)) {
        BMO_vert_flag_enable(bm, l_iter->v, ELE_VERT_ADJUST);
        shared_vert_num += 1;
      }
    } while ((l_iter = l_iter->next) != l_first);

    BMO_face_flag_enable(bm, f, ELE_FACE_ADJUST);
  }

  vert_accum_pool = BLI_mempool_create(sizeof(VertAccum), 0, 512, BLI_MEMPOOL_NOP);
  vaccum_map = BLI_ghash_ptr_new_ex(__func__, shared_vert_num);

  for (iter_step = 0; iter_step < iterations; iter_step++) {
    GHashIterator gh_iter;
    bool changed = false;

    BMO_ITER_INDEX (f, &oiter, op->slots_in, "faces", BM_FACE, i) {
      BMLoop *l_iter, *l_first;
      float plane[4];

      if (!BMO_face_flag_test(bm, f, ELE_FACE_ADJUST)) {
        continue;
      }
      BMO_face_flag_disable(bm, f, ELE_FACE_ADJUST);

      BLI_assert(f->len != 3);

/* keep original face data (else we 'move' the face) */
#if 0
      BM_face_normal_update(f);
      BM_face_calc_center_median_weighted(f, f_center);
#endif

      plane_from_point_normal_v3(plane, faces_center[i], f->no);

      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        VertAccum *va;
        void **va_p;
        float co[3];

        if (!BLI_ghash_ensure_p(vaccum_map, l_iter->v, &va_p)) {
          *va_p = BLI_mempool_calloc(vert_accum_pool);
        }
        va = static_cast<VertAccum *>(*va_p);

        closest_to_plane_normalized_v3(co, plane, l_iter->v->co);
        va->co_tot += 1;

        interp_v3_v3v3(va->co, va->co, co, 1.0f / float(va->co_tot));
      } while ((l_iter = l_iter->next) != l_first);
    }

    GHASH_ITER (gh_iter, vaccum_map) {
      BMVert *v = static_cast<BMVert *>(BLI_ghashIterator_getKey(&gh_iter));
      VertAccum *va = static_cast<VertAccum *>(BLI_ghashIterator_getValue(&gh_iter));
      BMIter iter;

      if (len_squared_v3v3(v->co, va->co) > eps_sq) {
        BMO_vert_flag_enable(bm, v, ELE_VERT_ADJUST);
        interp_v3_v3v3(v->co, v->co, va->co, fac);
        changed = true;
      }

      /* tag for re-calculation */
      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        if (f->len != 3) {
          BMO_face_flag_enable(bm, f, ELE_FACE_ADJUST);
        }
      }
    }

    /* if nothing changed, break out early */
    if (changed == false) {
      break;
    }

    BLI_ghash_clear(vaccum_map, nullptr, nullptr);
    BLI_mempool_clear(vert_accum_pool);
  }

  MEM_freeN(faces_center);
  BLI_ghash_free(vaccum_map, nullptr, nullptr);
  BLI_mempool_destroy(vert_accum_pool);
}

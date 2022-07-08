/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 *
 * This file contains code for polygon tessellation
 * (creating triangles from polygons).
 *
 * \see bmesh_mesh_tessellate.c for the #BMesh equivalent of this file.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h" /* Own include. */

#include "BLI_strict_flags.h"

/** Compared against total loops. */
#define MESH_FACE_TESSELLATE_THREADED_LIMIT 4096

/* -------------------------------------------------------------------- */
/** \name Loop Tessellation
 *
 * Fill in #MLoopTri data-structure.
 * \{ */

/**
 * \param face_normal: This will be optimized out as a constant.
 */
BLI_INLINE void mesh_calc_tessellation_for_face_impl(const MLoop *mloop,
                                                     const MPoly *mpoly,
                                                     const MVert *mvert,
                                                     uint poly_index,
                                                     MLoopTri *mlt,
                                                     MemArena **pf_arena_p,
                                                     const bool face_normal,
                                                     const float normal_precalc[3])
{
  const uint mp_loopstart = (uint)mpoly[poly_index].loopstart;
  const uint mp_totloop = (uint)mpoly[poly_index].totloop;

#define ML_TO_MLT(i1, i2, i3) \
  { \
    ARRAY_SET_ITEMS(mlt->tri, mp_loopstart + i1, mp_loopstart + i2, mp_loopstart + i3); \
    mlt->poly = poly_index; \
  } \
  ((void)0)

  switch (mp_totloop) {
    case 3: {
      ML_TO_MLT(0, 1, 2);
      break;
    }
    case 4: {
      ML_TO_MLT(0, 1, 2);
      MLoopTri *mlt_a = mlt++;
      ML_TO_MLT(0, 2, 3);
      MLoopTri *mlt_b = mlt;

      if (UNLIKELY(face_normal ? is_quad_flip_v3_first_third_fast_with_normal(
                                     /* Simpler calculation (using the normal). */
                                     mvert[mloop[mlt_a->tri[0]].v].co,
                                     mvert[mloop[mlt_a->tri[1]].v].co,
                                     mvert[mloop[mlt_a->tri[2]].v].co,
                                     mvert[mloop[mlt_b->tri[2]].v].co,
                                     normal_precalc) :
                                 is_quad_flip_v3_first_third_fast(
                                     /* Expensive calculation (no normal). */
                                     mvert[mloop[mlt_a->tri[0]].v].co,
                                     mvert[mloop[mlt_a->tri[1]].v].co,
                                     mvert[mloop[mlt_a->tri[2]].v].co,
                                     mvert[mloop[mlt_b->tri[2]].v].co))) {
        /* Flip out of degenerate 0-2 state. */
        mlt_a->tri[2] = mlt_b->tri[2];
        mlt_b->tri[0] = mlt_a->tri[1];
      }
      break;
    }
    default: {
      const MLoop *ml;
      float axis_mat[3][3];

      /* Calculate `axis_mat` to project verts to 2D. */
      if (face_normal == false) {
        float normal[3];
        const float *co_curr, *co_prev;

        zero_v3(normal);

        /* Calc normal, flipped: to get a positive 2D cross product. */
        ml = mloop + mp_loopstart;
        co_prev = mvert[ml[mp_totloop - 1].v].co;
        for (uint j = 0; j < mp_totloop; j++, ml++) {
          co_curr = mvert[ml->v].co;
          add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
          co_prev = co_curr;
        }
        if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
          normal[2] = 1.0f;
        }
        axis_dominant_v3_to_m3_negate(axis_mat, normal);
      }
      else {
        axis_dominant_v3_to_m3_negate(axis_mat, normal_precalc);
      }

      const uint totfilltri = mp_totloop - 2;

      MemArena *pf_arena = *pf_arena_p;
      if (UNLIKELY(pf_arena == NULL)) {
        pf_arena = *pf_arena_p = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      uint(*tris)[3] = tris = BLI_memarena_alloc(pf_arena, sizeof(*tris) * (size_t)totfilltri);
      float(*projverts)[2] = projverts = BLI_memarena_alloc(
          pf_arena, sizeof(*projverts) * (size_t)mp_totloop);

      ml = mloop + mp_loopstart;
      for (uint j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, pf_arena);

      /* Apply fill. */
      for (uint j = 0; j < totfilltri; j++, mlt++) {
        const uint *tri = tris[j];
        ML_TO_MLT(tri[0], tri[1], tri[2]);
      }

      BLI_memarena_clear(pf_arena);

      break;
    }
  }
#undef ML_TO_MLT
}

static void mesh_calc_tessellation_for_face(const MLoop *mloop,
                                            const MPoly *mpoly,
                                            const MVert *mvert,
                                            uint poly_index,
                                            MLoopTri *mlt,
                                            MemArena **pf_arena_p)
{
  mesh_calc_tessellation_for_face_impl(
      mloop, mpoly, mvert, poly_index, mlt, pf_arena_p, false, NULL);
}

static void mesh_calc_tessellation_for_face_with_normal(const MLoop *mloop,
                                                        const MPoly *mpoly,
                                                        const MVert *mvert,
                                                        uint poly_index,
                                                        MLoopTri *mlt,
                                                        MemArena **pf_arena_p,
                                                        const float normal_precalc[3])
{
  mesh_calc_tessellation_for_face_impl(
      mloop, mpoly, mvert, poly_index, mlt, pf_arena_p, true, normal_precalc);
}

static void mesh_recalc_looptri__single_threaded(const MLoop *mloop,
                                                 const MPoly *mpoly,
                                                 const MVert *mvert,
                                                 int totloop,
                                                 int totpoly,
                                                 MLoopTri *mlooptri,
                                                 const float (*poly_normals)[3])
{
  MemArena *pf_arena = NULL;
  const MPoly *mp = mpoly;
  uint tri_index = 0;

  if (poly_normals != NULL) {
    for (uint poly_index = 0; poly_index < (uint)totpoly; poly_index++, mp++) {
      mesh_calc_tessellation_for_face_with_normal(mloop,
                                                  mpoly,
                                                  mvert,
                                                  poly_index,
                                                  &mlooptri[tri_index],
                                                  &pf_arena,
                                                  poly_normals[poly_index]);
      tri_index += (uint)(mp->totloop - 2);
    }
  }
  else {
    for (uint poly_index = 0; poly_index < (uint)totpoly; poly_index++, mp++) {
      mesh_calc_tessellation_for_face(
          mloop, mpoly, mvert, poly_index, &mlooptri[tri_index], &pf_arena);
      tri_index += (uint)(mp->totloop - 2);
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
    pf_arena = NULL;
  }
  BLI_assert(tri_index == (uint)poly_to_tri_count(totpoly, totloop));
  UNUSED_VARS_NDEBUG(totloop);
}

struct TessellationUserData {
  const MLoop *mloop;
  const MPoly *mpoly;
  const MVert *mvert;

  /** Output array. */
  MLoopTri *mlooptri;

  /** Optional pre-calculated polygon normals array. */
  const float (*poly_normals)[3];
};

struct TessellationUserTLS {
  MemArena *pf_arena;
};

static void mesh_calc_tessellation_for_face_fn(void *__restrict userdata,
                                               const int index,
                                               const TaskParallelTLS *__restrict tls)
{
  const struct TessellationUserData *data = userdata;
  struct TessellationUserTLS *tls_data = tls->userdata_chunk;
  const int tri_index = poly_to_tri_count(index, data->mpoly[index].loopstart);
  mesh_calc_tessellation_for_face_impl(data->mloop,
                                       data->mpoly,
                                       data->mvert,
                                       (uint)index,
                                       &data->mlooptri[tri_index],
                                       &tls_data->pf_arena,
                                       false,
                                       NULL);
}

static void mesh_calc_tessellation_for_face_with_normal_fn(void *__restrict userdata,
                                                           const int index,
                                                           const TaskParallelTLS *__restrict tls)
{
  const struct TessellationUserData *data = userdata;
  struct TessellationUserTLS *tls_data = tls->userdata_chunk;
  const int tri_index = poly_to_tri_count(index, data->mpoly[index].loopstart);
  mesh_calc_tessellation_for_face_impl(data->mloop,
                                       data->mpoly,
                                       data->mvert,
                                       (uint)index,
                                       &data->mlooptri[tri_index],
                                       &tls_data->pf_arena,
                                       true,
                                       data->poly_normals[index]);
}

static void mesh_calc_tessellation_for_face_free_fn(const void *__restrict UNUSED(userdata),
                                                    void *__restrict tls_v)
{
  struct TessellationUserTLS *tls_data = tls_v;
  if (tls_data->pf_arena) {
    BLI_memarena_free(tls_data->pf_arena);
  }
}

static void mesh_recalc_looptri__multi_threaded(const MLoop *mloop,
                                                const MPoly *mpoly,
                                                const MVert *mvert,
                                                int UNUSED(totloop),
                                                int totpoly,
                                                MLoopTri *mlooptri,
                                                const float (*poly_normals)[3])
{
  struct TessellationUserTLS tls_data_dummy = {NULL};

  struct TessellationUserData data = {
      .mloop = mloop,
      .mpoly = mpoly,
      .mvert = mvert,
      .mlooptri = mlooptri,
      .poly_normals = poly_normals,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  settings.userdata_chunk = &tls_data_dummy;
  settings.userdata_chunk_size = sizeof(tls_data_dummy);

  settings.func_free = mesh_calc_tessellation_for_face_free_fn;

  BLI_task_parallel_range(0,
                          totpoly,
                          &data,
                          poly_normals ? mesh_calc_tessellation_for_face_with_normal_fn :
                                         mesh_calc_tessellation_for_face_fn,
                          &settings);
}

void BKE_mesh_recalc_looptri(const MLoop *mloop,
                             const MPoly *mpoly,
                             const MVert *mvert,
                             int totloop,
                             int totpoly,
                             MLoopTri *mlooptri)
{
  if (totloop < MESH_FACE_TESSELLATE_THREADED_LIMIT) {
    mesh_recalc_looptri__single_threaded(mloop, mpoly, mvert, totloop, totpoly, mlooptri, NULL);
  }
  else {
    mesh_recalc_looptri__multi_threaded(mloop, mpoly, mvert, totloop, totpoly, mlooptri, NULL);
  }
}

void BKE_mesh_recalc_looptri_with_normals(const MLoop *mloop,
                                          const MPoly *mpoly,
                                          const MVert *mvert,
                                          int totloop,
                                          int totpoly,
                                          MLoopTri *mlooptri,
                                          const float (*poly_normals)[3])
{
  BLI_assert(poly_normals != NULL);
  if (totloop < MESH_FACE_TESSELLATE_THREADED_LIMIT) {
    mesh_recalc_looptri__single_threaded(
        mloop, mpoly, mvert, totloop, totpoly, mlooptri, poly_normals);
  }
  else {
    mesh_recalc_looptri__multi_threaded(
        mloop, mpoly, mvert, totloop, totpoly, mlooptri, poly_normals);
  }
}

/** \} */

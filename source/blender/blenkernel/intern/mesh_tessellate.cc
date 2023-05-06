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

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "BKE_mesh.hh"

#include "BLI_strict_flags.h"

namespace blender::bke::mesh {

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
BLI_INLINE void mesh_calc_tessellation_for_face_impl(const Span<int> corner_verts,
                                                     const blender::OffsetIndices<int> polys,
                                                     const Span<float3> positions,
                                                     uint poly_index,
                                                     MLoopTri *mlt,
                                                     MemArena **pf_arena_p,
                                                     const bool face_normal,
                                                     const float normal_precalc[3])
{
  const uint mp_loopstart = uint(polys[poly_index].start());
  const uint mp_totloop = uint(polys[poly_index].size());

  auto create_tri = [&](uint i1, uint i2, uint i3) {
    mlt->tri[0] = mp_loopstart + i1;
    mlt->tri[1] = mp_loopstart + i2;
    mlt->tri[2] = mp_loopstart + i3;
  };

  switch (mp_totloop) {
    case 3: {
      create_tri(0, 1, 2);
      break;
    }
    case 4: {
      create_tri(0, 1, 2);
      MLoopTri *mlt_a = mlt++;
      create_tri(0, 2, 3);
      MLoopTri *mlt_b = mlt;
      if (UNLIKELY(is_quad_flip_v3_first_third_fast(positions[corner_verts[mlt_a->tri[0]]],
                                                    positions[corner_verts[mlt_a->tri[1]]],
                                                    positions[corner_verts[mlt_a->tri[2]]],
                                                    positions[corner_verts[mlt_b->tri[2]]])))
      {
        /* Flip out of degenerate 0-2 state. */
        mlt_a->tri[2] = mlt_b->tri[2];
        mlt_b->tri[0] = mlt_a->tri[1];
      }
      break;
    }
    default: {
      float axis_mat[3][3];

      /* Calculate `axis_mat` to project verts to 2D. */
      if (face_normal == false) {
        float normal[3];
        const float *co_curr, *co_prev;

        zero_v3(normal);

        /* Calc normal, flipped: to get a positive 2D cross product. */
        co_prev = positions[corner_verts[mp_loopstart + mp_totloop - 1]];
        for (uint j = 0; j < mp_totloop; j++) {
          co_curr = positions[corner_verts[mp_loopstart + j]];
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
      if (UNLIKELY(pf_arena == nullptr)) {
        pf_arena = *pf_arena_p = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      uint(*tris)[3] = static_cast<uint(*)[3]>(
          BLI_memarena_alloc(pf_arena, sizeof(*tris) * size_t(totfilltri)));
      float(*projverts)[2] = static_cast<float(*)[2]>(
          BLI_memarena_alloc(pf_arena, sizeof(*projverts) * size_t(mp_totloop)));

      for (uint j = 0; j < mp_totloop; j++) {
        mul_v2_m3v3(projverts[j], axis_mat, positions[corner_verts[mp_loopstart + j]]);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, pf_arena);

      /* Apply fill. */
      for (uint j = 0; j < totfilltri; j++, mlt++) {
        const uint *tri = tris[j];
        create_tri(tri[0], tri[1], tri[2]);
      }

      BLI_memarena_clear(pf_arena);

      break;
    }
  }
#undef ML_TO_MLT
}

static void mesh_calc_tessellation_for_face(const Span<int> corner_verts,
                                            const blender::OffsetIndices<int> polys,
                                            const Span<float3> positions,
                                            uint poly_index,
                                            MLoopTri *mlt,
                                            MemArena **pf_arena_p)
{
  mesh_calc_tessellation_for_face_impl(
      corner_verts, polys, positions, poly_index, mlt, pf_arena_p, false, nullptr);
}

static void mesh_calc_tessellation_for_face_with_normal(const Span<int> corner_verts,
                                                        const blender::OffsetIndices<int> polys,
                                                        const Span<float3> positions,
                                                        uint poly_index,
                                                        MLoopTri *mlt,
                                                        MemArena **pf_arena_p,
                                                        const float normal_precalc[3])
{
  mesh_calc_tessellation_for_face_impl(
      corner_verts, polys, positions, poly_index, mlt, pf_arena_p, true, normal_precalc);
}

static void mesh_recalc_looptri__single_threaded(const Span<int> corner_verts,
                                                 const blender::OffsetIndices<int> polys,
                                                 const Span<float3> positions,
                                                 MLoopTri *mlooptri,
                                                 const float (*poly_normals)[3])
{
  MemArena *pf_arena = nullptr;
  uint tri_index = 0;

  if (poly_normals != nullptr) {
    for (const int64_t i : polys.index_range()) {
      mesh_calc_tessellation_for_face_with_normal(corner_verts,
                                                  polys,
                                                  positions,
                                                  uint(i),
                                                  &mlooptri[tri_index],
                                                  &pf_arena,
                                                  poly_normals[i]);
      tri_index += uint(polys[i].size() - 2);
    }
  }
  else {
    for (const int64_t i : polys.index_range()) {
      mesh_calc_tessellation_for_face(
          corner_verts, polys, positions, uint(i), &mlooptri[tri_index], &pf_arena);
      tri_index += uint(polys[i].size() - 2);
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
    pf_arena = nullptr;
  }
  BLI_assert(tri_index == uint(poly_to_tri_count(int(polys.size()), int(corner_verts.size()))));
}

struct TessellationUserData {
  Span<int> corner_verts;
  blender::OffsetIndices<int> polys;
  Span<float3> positions;

  /** Output array. */
  MutableSpan<MLoopTri> mlooptri;

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
  const TessellationUserData *data = static_cast<const TessellationUserData *>(userdata);
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls->userdata_chunk);
  const int tri_index = poly_to_tri_count(index, int(data->polys[index].start()));
  mesh_calc_tessellation_for_face_impl(data->corner_verts,
                                       data->polys,
                                       data->positions,
                                       uint(index),
                                       &data->mlooptri[tri_index],
                                       &tls_data->pf_arena,
                                       false,
                                       nullptr);
}

static void mesh_calc_tessellation_for_face_with_normal_fn(void *__restrict userdata,
                                                           const int index,
                                                           const TaskParallelTLS *__restrict tls)
{
  const TessellationUserData *data = static_cast<const TessellationUserData *>(userdata);
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls->userdata_chunk);
  const int tri_index = poly_to_tri_count(index, int(data->polys[index].start()));
  mesh_calc_tessellation_for_face_impl(data->corner_verts,
                                       data->polys,
                                       data->positions,
                                       uint(index),
                                       &data->mlooptri[tri_index],
                                       &tls_data->pf_arena,
                                       true,
                                       data->poly_normals[index]);
}

static void mesh_calc_tessellation_for_face_free_fn(const void *__restrict /*userdata*/,
                                                    void *__restrict tls_v)
{
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls_v);
  if (tls_data->pf_arena) {
    BLI_memarena_free(tls_data->pf_arena);
  }
}

static void looptris_calc_all(const Span<float3> positions,
                              const blender::OffsetIndices<int> polys,
                              const Span<int> corner_verts,
                              const Span<float3> poly_normals,
                              MutableSpan<MLoopTri> looptris)
{
  if (corner_verts.size() < MESH_FACE_TESSELLATE_THREADED_LIMIT) {
    mesh_recalc_looptri__single_threaded(corner_verts,
                                         polys,
                                         positions,
                                         looptris.data(),
                                         reinterpret_cast<const float(*)[3]>(poly_normals.data()));
    return;
  }
  struct TessellationUserTLS tls_data_dummy = {nullptr};

  struct TessellationUserData data {
  };
  data.corner_verts = corner_verts;
  data.polys = polys;
  data.positions = positions;
  data.mlooptri = looptris;
  data.poly_normals = reinterpret_cast<const float(*)[3]>(poly_normals.data());

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  settings.userdata_chunk = &tls_data_dummy;
  settings.userdata_chunk_size = sizeof(tls_data_dummy);

  settings.func_free = mesh_calc_tessellation_for_face_free_fn;

  BLI_task_parallel_range(0,
                          int(polys.size()),
                          &data,
                          data.poly_normals ? mesh_calc_tessellation_for_face_with_normal_fn :
                                              mesh_calc_tessellation_for_face_fn,
                          &settings);
}

void looptris_calc(const Span<float3> vert_positions,
                   const OffsetIndices<int> polys,
                   const Span<int> corner_verts,
                   MutableSpan<MLoopTri> looptris)
{
  looptris_calc_all(vert_positions, polys, corner_verts, {}, looptris);
}

void looptris_calc_poly_indices(const OffsetIndices<int> polys, MutableSpan<int> looptri_polys)
{
  threading::parallel_for(polys.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const IndexRange poly = polys[i];
      const int start = poly_to_tri_count(int(i), int(poly.start()));
      const int num = ME_POLY_TRI_TOT(int(poly.size()));
      looptri_polys.slice(start, num).fill(int(i));
    }
  });
}

void looptris_calc_with_normals(const Span<float3> vert_positions,
                                const OffsetIndices<int> polys,
                                const Span<int> corner_verts,
                                const Span<float3> poly_normals,
                                MutableSpan<MLoopTri> looptris)
{
  BLI_assert(!poly_normals.is_empty() || polys.size() == 0);
  looptris_calc_all(vert_positions, polys, corner_verts, poly_normals, looptris);
}

/** \} */

}  // namespace blender::bke::mesh

void BKE_mesh_recalc_looptri(const int *corner_verts,
                             const int *poly_offsets,
                             const float (*vert_positions)[3],
                             int totvert,
                             int totloop,
                             int totpoly,
                             MLoopTri *mlooptri)
{
  blender::bke::mesh::looptris_calc(
      {reinterpret_cast<const blender::float3 *>(vert_positions), totvert},
      blender::Span(poly_offsets, totpoly + 1),
      {corner_verts, totloop},
      {mlooptri, poly_to_tri_count(totpoly, totloop)});
}

/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file contains code for face tessellation
 * (creating triangles from polygons).
 *
 * \see `bmesh_mesh_tessellate.cc` for the #BMesh equivalent of this file.
 */

#include "BLI_array_utils.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "BKE_mesh.hh"

#include "BLI_strict_flags.h" /* Keep last. */

namespace blender::bke::mesh {

/** Compared against total loops. */
#define MESH_FACE_TESSELLATE_THREADED_LIMIT 4096

/* -------------------------------------------------------------------- */
/** \name Loop Tessellation
 *
 * Fill in Corner Triangle data-structure.
 * \{ */

/**
 * \param face_normal: This will be optimized out as a constant.
 */
BLI_INLINE void mesh_calc_tessellation_for_face_impl(const Span<int> corner_verts,
                                                     const blender::OffsetIndices<int> faces,
                                                     const Span<float3> positions,
                                                     uint face_index,
                                                     int3 *tri,
                                                     MemArena **pf_arena_p,
                                                     const bool face_normal,
                                                     const float normal_precalc[3])
{
  const uint mp_loopstart = uint(faces[face_index].start());
  const uint mp_totloop = uint(faces[face_index].size());

  auto create_tri = [&](uint i1, uint i2, uint i3) {
    (*tri)[0] = int(mp_loopstart + i1);
    (*tri)[1] = int(mp_loopstart + i2);
    (*tri)[2] = int(mp_loopstart + i3);
  };

  switch (mp_totloop) {
    case 3: {
      create_tri(0, 1, 2);
      break;
    }
    case 4: {
      create_tri(0, 1, 2);
      int3 *tri_a = tri++;
      create_tri(0, 2, 3);
      int3 *tri_b = tri;
      if (UNLIKELY(is_quad_flip_v3_first_third_fast(positions[corner_verts[(*tri_a)[0]]],
                                                    positions[corner_verts[(*tri_a)[1]]],
                                                    positions[corner_verts[(*tri_a)[2]]],
                                                    positions[corner_verts[(*tri_b)[2]]])))
      {
        /* Flip out of degenerate 0-2 state. */
        (*tri_a)[2] = (*tri_b)[2];
        (*tri_b)[0] = (*tri_a)[1];
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
      for (uint j = 0; j < totfilltri; j++, tri++) {
        create_tri(tris[j][0], tris[j][1], tris[j][2]);
      }

      BLI_memarena_clear(pf_arena);

      break;
    }
  }
#undef ML_TO_MLT
}

static void mesh_calc_tessellation_for_face(const Span<int> corner_verts,
                                            const blender::OffsetIndices<int> faces,
                                            const Span<float3> positions,
                                            uint face_index,
                                            int3 *tri,
                                            MemArena **pf_arena_p)
{
  mesh_calc_tessellation_for_face_impl(
      corner_verts, faces, positions, face_index, tri, pf_arena_p, false, nullptr);
}

static void mesh_calc_tessellation_for_face_with_normal(const Span<int> corner_verts,
                                                        const blender::OffsetIndices<int> faces,
                                                        const Span<float3> positions,
                                                        uint face_index,
                                                        int3 *tri,
                                                        MemArena **pf_arena_p,
                                                        const float normal_precalc[3])
{
  mesh_calc_tessellation_for_face_impl(
      corner_verts, faces, positions, face_index, tri, pf_arena_p, true, normal_precalc);
}

static void mesh_recalc_corner_tris__single_threaded(const Span<int> corner_verts,
                                                     const blender::OffsetIndices<int> faces,
                                                     const Span<float3> positions,
                                                     int3 *corner_tris,
                                                     const float (*face_normals)[3])
{
  MemArena *pf_arena = nullptr;
  uint corner_tri_i = 0;

  if (face_normals != nullptr) {
    for (const int64_t i : faces.index_range()) {
      mesh_calc_tessellation_for_face_with_normal(corner_verts,
                                                  faces,
                                                  positions,
                                                  uint(i),
                                                  &corner_tris[corner_tri_i],
                                                  &pf_arena,
                                                  face_normals[i]);
      corner_tri_i += uint(faces[i].size() - 2);
    }
  }
  else {
    for (const int64_t i : faces.index_range()) {
      mesh_calc_tessellation_for_face(
          corner_verts, faces, positions, uint(i), &corner_tris[corner_tri_i], &pf_arena);
      corner_tri_i += uint(faces[i].size() - 2);
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
    pf_arena = nullptr;
  }
  BLI_assert(corner_tri_i == uint(poly_to_tri_count(int(faces.size()), int(corner_verts.size()))));
}

struct TessellationUserData {
  Span<int> corner_verts;
  blender::OffsetIndices<int> faces;
  Span<float3> positions;

  /** Output array. */
  MutableSpan<int3> corner_tris;

  /** Optional pre-calculated face normals array. */
  const float (*face_normals)[3];
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
  const int corner_tri_i = poly_to_tri_count(index, int(data->faces[index].start()));
  mesh_calc_tessellation_for_face_impl(data->corner_verts,
                                       data->faces,
                                       data->positions,
                                       uint(index),
                                       &data->corner_tris[corner_tri_i],
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
  const int corner_tri_i = poly_to_tri_count(index, int(data->faces[index].start()));
  mesh_calc_tessellation_for_face_impl(data->corner_verts,
                                       data->faces,
                                       data->positions,
                                       uint(index),
                                       &data->corner_tris[corner_tri_i],
                                       &tls_data->pf_arena,
                                       true,
                                       data->face_normals[index]);
}

static void mesh_calc_tessellation_for_face_free_fn(const void *__restrict /*userdata*/,
                                                    void *__restrict tls_v)
{
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls_v);
  if (tls_data->pf_arena) {
    BLI_memarena_free(tls_data->pf_arena);
  }
}

static void corner_tris_calc_all(const Span<float3> positions,
                                 const blender::OffsetIndices<int> faces,
                                 const Span<int> corner_verts,
                                 const Span<float3> face_normals,
                                 MutableSpan<int3> corner_tris)
{
  if (corner_verts.size() < MESH_FACE_TESSELLATE_THREADED_LIMIT) {
    mesh_recalc_corner_tris__single_threaded(
        corner_verts,
        faces,
        positions,
        corner_tris.data(),
        reinterpret_cast<const float(*)[3]>(face_normals.data()));
    return;
  }
  TessellationUserTLS tls_data_dummy = {nullptr};

  TessellationUserData data{};
  data.corner_verts = corner_verts;
  data.faces = faces;
  data.positions = positions;
  data.corner_tris = corner_tris;
  data.face_normals = reinterpret_cast<const float(*)[3]>(face_normals.data());

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  settings.userdata_chunk = &tls_data_dummy;
  settings.userdata_chunk_size = sizeof(tls_data_dummy);

  settings.func_free = mesh_calc_tessellation_for_face_free_fn;

  BLI_task_parallel_range(0,
                          int(faces.size()),
                          &data,
                          data.face_normals ? mesh_calc_tessellation_for_face_with_normal_fn :
                                              mesh_calc_tessellation_for_face_fn,
                          &settings);
}

void corner_tris_calc(const Span<float3> vert_positions,
                      const OffsetIndices<int> faces,
                      const Span<int> corner_verts,
                      MutableSpan<int3> corner_tris)
{
  corner_tris_calc_all(vert_positions, faces, corner_verts, {}, corner_tris);
}

void corner_tris_calc_face_indices(const OffsetIndices<int> faces, MutableSpan<int> tri_faces)
{
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const IndexRange face = faces[i];
      const int start = poly_to_tri_count(int(i), int(face.start()));
      const int num = face_triangles_num(int(face.size()));
      tri_faces.slice(start, num).fill(int(i));
    }
  });
}

void corner_tris_calc_with_normals(const Span<float3> vert_positions,
                                   const OffsetIndices<int> faces,
                                   const Span<int> corner_verts,
                                   const Span<float3> face_normals,
                                   MutableSpan<int3> corner_tris)
{
  BLI_assert(!face_normals.is_empty() || faces.is_empty());
  corner_tris_calc_all(vert_positions, faces, corner_verts, face_normals, corner_tris);
}

/** \} */

void vert_tris_from_corner_tris(const Span<int> corner_verts,
                                const Span<int3> corner_tris,
                                MutableSpan<int3> vert_tris)
{
  array_utils::gather(corner_verts, corner_tris.cast<int>(), vert_tris.cast<int>());
}

int3 corner_tri_get_real_edges(const Span<int2> edges,
                               const Span<int> corner_verts,
                               const Span<int> corner_edges,
                               const int3 &corner_tri)
{
  int3 real_edges;
  for (int i = 2, i_next = 0; i_next < 3; i = i_next++) {
    const int corner_1 = int(corner_tri[i]);
    const int corner_2 = int(corner_tri[i_next]);
    const int vert_1 = corner_verts[corner_1];
    const int vert_2 = corner_verts[corner_2];
    const int edge_i = corner_edges[corner_1];
    const int2 edge = edges[edge_i];

    const bool is_real = (vert_1 == edge[0] && vert_2 == edge[1]) ||
                         (vert_1 == edge[1] && vert_2 == edge[0]);

    real_edges[i] = is_real ? edge_i : -1;
  }
  return real_edges;
}

}  // namespace blender::bke::mesh

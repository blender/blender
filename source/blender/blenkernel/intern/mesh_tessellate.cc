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
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.hh"

#include "BKE_mesh.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender::bke::mesh {

/* -------------------------------------------------------------------- */
/** \name Face Tessellation
 *
 * Fill in Corner Triangle Array
 * \{ */

/**
 * \param face_normal: This will be optimized out as a constant.
 */
BLI_INLINE void mesh_calc_tessellation_for_face_impl(const Span<int> corner_verts,
                                                     const Span<float3> positions,
                                                     const int face_start,
                                                     const int face_size,
                                                     int3 *tri,
                                                     MemArena **pf_arena_p,
                                                     const bool face_normal,
                                                     const float normal_precalc[3])
{
  auto create_tri = [&](int i1, int i2, int i3) {
    (*tri)[0] = face_start + i1;
    (*tri)[1] = face_start + i2;
    (*tri)[2] = face_start + i3;
  };

  switch (face_size) {
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
        co_prev = positions[corner_verts[face_start + face_size - 1]];
        for (int j = 0; j < face_size; j++) {
          co_curr = positions[corner_verts[face_start + j]];
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

      const int totfilltri = face_size - 2;

      MemArena *pf_arena = *pf_arena_p;
      if (UNLIKELY(pf_arena == nullptr)) {
        pf_arena = *pf_arena_p = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      uint(*tris)[3] = static_cast<uint(*)[3]>(
          BLI_memarena_alloc(pf_arena, sizeof(*tris) * size_t(totfilltri)));
      float (*projverts)[2] = static_cast<float (*)[2]>(
          BLI_memarena_alloc(pf_arena, sizeof(*projverts) * size_t(face_size)));

      for (int j = 0; j < face_size; j++) {
        mul_v2_m3v3(projverts[j], axis_mat, positions[corner_verts[face_start + j]]);
      }

      BLI_polyfill_calc_arena(projverts, uint(face_size), 1, tris, pf_arena);

      /* Apply fill. */
      for (int j = 0; j < totfilltri; j++, tri++) {
        create_tri(int(tris[j][0]), int(tris[j][1]), int(tris[j][2]));
      }

      BLI_memarena_clear(pf_arena);

      break;
    }
  }
}

static void mesh_calc_tessellation_for_face(const Span<int> corner_verts,
                                            const Span<float3> positions,
                                            const int face_start,
                                            const int face_size,
                                            int3 *tri,
                                            MemArena **pf_arena_p)
{
  mesh_calc_tessellation_for_face_impl(
      corner_verts, positions, face_start, face_size, tri, pf_arena_p, false, nullptr);
}

static void mesh_calc_tessellation_for_face_with_normal(const Span<int> corner_verts,
                                                        const Span<float3> positions,
                                                        const int face_start,
                                                        const int face_size,
                                                        int3 *tri,
                                                        MemArena **pf_arena_p,
                                                        const float normal_precalc[3])
{
  mesh_calc_tessellation_for_face_impl(
      corner_verts, positions, face_start, face_size, tri, pf_arena_p, true, normal_precalc);
}

struct LocalData {
  MemArena *pf_arena = nullptr;

  ~LocalData()
  {
    if (pf_arena) {
      BLI_memarena_free(pf_arena);
    }
  }
};

static void corner_tris_calc_impl(const Span<float3> positions,
                                  const OffsetIndices<int> faces,
                                  const Span<int> corner_verts,
                                  const Span<float3> face_normals,
                                  MutableSpan<int3> corner_tris)
{
  threading::EnumerableThreadSpecific<LocalData> all_local_data;
  if (face_normals.is_empty()) {
    threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
      LocalData &local_data = all_local_data.local();
      for (const int64_t i : range) {
        const int face_start = int(faces[i].start());
        const int face_size = int(faces[i].size());
        const int tris_start = poly_to_tri_count(int(i), face_start);
        mesh_calc_tessellation_for_face(corner_verts,
                                        positions,
                                        face_start,
                                        face_size,
                                        &corner_tris[tris_start],
                                        &local_data.pf_arena);
      }
    });
  }
  else {
    threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
      LocalData &local_data = all_local_data.local();
      for (const int64_t i : range) {
        const int face_start = int(faces[i].start());
        const int face_size = int(faces[i].size());
        const int tris_start = poly_to_tri_count(int(i), face_start);
        mesh_calc_tessellation_for_face_with_normal(corner_verts,
                                                    positions,
                                                    face_start,
                                                    face_size,
                                                    &corner_tris[tris_start],
                                                    &local_data.pf_arena,
                                                    face_normals[i]);
      }
    });
  }
}

void corner_tris_calc(const Span<float3> vert_positions,
                      const OffsetIndices<int> faces,
                      const Span<int> corner_verts,
                      MutableSpan<int3> corner_tris)
{
  corner_tris_calc_impl(vert_positions, faces, corner_verts, {}, corner_tris);
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
  corner_tris_calc_impl(vert_positions, faces, corner_verts, face_normals, corner_tris);
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

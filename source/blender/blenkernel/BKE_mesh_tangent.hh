/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"

struct ReportList;
struct Mesh;

/**
 * Compute simplified tangent space normals, i.e.
 * tangent vector + sign of bi-tangent one, which combined with
 * custom normals can be used to recreate the full tangent space.
 *
 * \note The mesh should be made of only triangles and quads!
 */
void BKE_mesh_calc_loop_tangent_single_ex(const float (*vert_positions)[3],
                                          int numVerts,
                                          const int *corner_verts,
                                          float (*r_looptangent)[4],
                                          const float (*corner_normals)[3],
                                          const float (*loop_uvs)[2],
                                          int numLoops,
                                          blender::OffsetIndices<int> faces,
                                          ReportList *reports);

/**
 * Wrapper around BKE_mesh_calc_loop_tangent_single_ex, which takes care of most boilerplate code.
 * \note
 * - There must be a valid loop's CD_NORMALS available.
 * - The mesh should be made of only triangles and quads!
 */
void BKE_mesh_calc_loop_tangent_single(Mesh *mesh,
                                       const char *uvmap,
                                       float (*r_looptangents)[4],
                                       ReportList *reports);

namespace blender::bke::mesh {

/**
 * See: #BKE_editmesh_uv_tangents_calc (matching logic).
 */
Array<Array<float4>> calc_uv_tangents(Span<float3> vert_positions,
                                      OffsetIndices<int> faces,
                                      Span<int> corner_verts,
                                      Span<int3> corner_tris,
                                      Span<int> corner_tri_faces,
                                      Span<bool> sharp_faces,
                                      Span<float3> vert_normals,
                                      Span<float3> face_normals,
                                      Span<float3> corner_normals,
                                      Span<Span<float2>> uv_maps);

Array<float4> calc_orco_tangents(Span<float3> vert_positions,
                                 OffsetIndices<int> faces,
                                 Span<int> corner_verts,
                                 Span<int3> corner_tris,
                                 Span<int> corner_tri_faces,
                                 Span<bool> sharp_faces,
                                 Span<float3> vert_normals,
                                 Span<float3> face_normals,
                                 Span<float3> corner_normals,
                                 Span<float3> vert_orco);

}  // namespace blender::bke::mesh

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

namespace blender::bke::mesh {

/**
 * Compute simplified tangent space normals, i.e.
 * tangent vector + sign of bi-tangent one, which combined with
 * custom normals can be used to recreate the full tangent space.
 *
 * \note The mesh should be made of only triangles and quads!
 */
void calc_uv_tangent_tris_quads(Span<float3> vert_positions,
                                OffsetIndices<int> faces,
                                Span<int> corner_verts,
                                Span<float3> corner_normals,
                                Span<float2> uv_map,
                                MutableSpan<float4> results,
                                ReportList *reports);

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

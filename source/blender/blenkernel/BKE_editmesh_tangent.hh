/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

namespace blender {

struct BMEditMesh;

/**
 * \see #BKE_mesh_calc_loop_tangent, same logic but used arrays instead of #BMesh data.
 */
Array<Array<float4>> BKE_editmesh_uv_tangents_calc(BMEditMesh *em,
                                                   Span<float3> face_normals,
                                                   Span<float3> corner_normals,
                                                   Span<StringRef> uv_names);

Array<float4> BKE_editmesh_orco_tangents_calc(BMEditMesh *em,
                                              Span<float3> face_normals,
                                              Span<float3> corner_normals,
                                              Span<float3> vert_orco);

}  // namespace blender

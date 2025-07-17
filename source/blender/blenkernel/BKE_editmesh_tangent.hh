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

struct BMEditMesh;

/**
 * \see #BKE_mesh_calc_loop_tangent, same logic but used arrays instead of #BMesh data.
 */
blender::Array<blender::Array<blender::float4>> BKE_editmesh_uv_tangents_calc(
    BMEditMesh *em,
    blender::Span<blender::float3> face_normals,
    blender::Span<blender::float3> corner_normals,
    blender::Span<blender::StringRef> uv_names);

blender::Array<blender::float4> BKE_editmesh_orco_tangents_calc(
    BMEditMesh *em,
    blender::Span<blender::float3> face_normals,
    blender::Span<blender::float3> corner_normals,
    blender::Span<blender::float3> vert_orco);

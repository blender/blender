/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

struct BMEditMesh;

namespace blender::bke {

struct EditMeshData {
  /** when set, \a vertexNos, faceNos are lazy initialized */
  Array<float3> vert_positions;

  /** lazy initialize (when \a vert_positions is set) */
  Array<float3> vert_normals;
  Array<float3> face_normals;
  /** also lazy init but don't depend on \a vert_positions */
  Array<float3> face_centers;
};

}  // namespace blender::bke

blender::Span<blender::float3> BKE_editmesh_cache_ensure_face_normals(
    BMEditMesh &em, blender::bke::EditMeshData &emd);
blender::Span<blender::float3> BKE_editmesh_cache_ensure_vert_normals(
    BMEditMesh &em, blender::bke::EditMeshData &emd);

blender::Span<blender::float3> BKE_editmesh_cache_ensure_face_centers(
    BMEditMesh &em, blender::bke::EditMeshData &emd);

std::optional<blender::Bounds<blender::float3>> BKE_editmesh_cache_calc_minmax(
    const BMEditMesh &em, const blender::bke::EditMeshData &emd);

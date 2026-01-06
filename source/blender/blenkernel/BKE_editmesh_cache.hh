/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender {

struct BMEditMesh;

namespace bke {

struct EditMeshData {
  /**
   * Deformed positions calculated by modifiers in the modifier stack that can process an
   * edit mesh input. When this is not empty, the other arrays will depend on the values.
   */
  Array<float3> vert_positions;

  /**
   * Lazily initialized vertex normal cache (used when `vert_positions` is set.
   * Access via #BKE_editmesh_cache_ensure_vert_normals instead of directly.
   */
  Array<float3> vert_normals;
  /**
   * Lazily initialized face normal cache (used when `vert_positions` is set.
   * Access via #BKE_editmesh_cache_ensure_face_normals instead of directly.
   */
  Array<float3> face_normals;
  /**
   * Cache of face centers, also depends on `vert_positions` when it is not empty.
   * Access via #BKE_editmesh_cache_ensure_face_centers instead of directly.
   */
  Array<float3> face_centers;
};

}  // namespace bke

Span<float3> BKE_editmesh_cache_ensure_face_normals(BMEditMesh &em, bke::EditMeshData &emd);
Span<float3> BKE_editmesh_cache_ensure_vert_normals(BMEditMesh &em, bke::EditMeshData &emd);

Span<float3> BKE_editmesh_cache_ensure_face_centers(BMEditMesh &em, bke::EditMeshData &emd);

std::optional<Bounds<float3>> BKE_editmesh_cache_calc_minmax(const BMEditMesh &em,
                                                             const bke::EditMeshData &emd);

}  // namespace blender

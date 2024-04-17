/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_bounds.hh"
#include "BLI_function_ref.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "DNA_modifier_types.h"

#include "BKE_volume_grid_fwd.hh"

struct Depsgraph;
struct Mesh;
struct Volume;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

struct MeshToVolumeResolution {
  MeshToVolumeModifierResolutionMode mode;
  union {
    float voxel_size;
    float voxel_amount;
  } settings;
};

#ifdef WITH_OPENVDB

/**
 * \param bounds_fn: Return the bounds of the mesh positions,
 * used for deciding the voxel size in "Amount" mode.
 */
float volume_compute_voxel_size(const Depsgraph *depsgraph,
                                FunctionRef<Bounds<float3>()> bounds_fn,
                                MeshToVolumeResolution resolution,
                                float exterior_band_width,
                                const float4x4 &transform);
/**
 * Add a new fog VolumeGrid to the Volume by converting the supplied mesh.
 */
bke::VolumeGridData *fog_volume_grid_add_from_mesh(Volume *volume,
                                                   StringRefNull name,
                                                   Span<float3> positions,
                                                   Span<int> corner_verts,
                                                   Span<int3> corner_tris,
                                                   const float4x4 &mesh_to_volume_space_transform,
                                                   float voxel_size,
                                                   float interior_band_width,
                                                   float density);

bke::VolumeGrid<float> mesh_to_density_grid(const Span<float3> positions,
                                            const Span<int> corner_verts,
                                            const Span<int3> corner_tris,
                                            const float voxel_size,
                                            const float interior_band_width,
                                            const float density);

bke::VolumeGrid<float> mesh_to_sdf_grid(Span<float3> positions,
                                        Span<int> corner_verts,
                                        Span<int3> corner_tris,
                                        float voxel_size,
                                        float half_band_width);

#endif
}  // namespace blender::geometry

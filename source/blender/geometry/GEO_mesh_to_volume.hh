/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_function_ref.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_modifier_types.h"

#include "BKE_volume_grid_fwd.hh"

#pragma once

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
                                                   const Mesh *mesh,
                                                   const float4x4 &mesh_to_volume_space_transform,
                                                   float voxel_size,
                                                   float interior_band_width,
                                                   float density);
/**
 * Add a new SDF VolumeGrid to the Volume by converting the supplied mesh.
 */
bke::VolumeGridData *sdf_volume_grid_add_from_mesh(
    Volume *volume, StringRefNull name, const Mesh &mesh, float voxel_size, float half_band_width);
#endif
}  // namespace blender::geometry

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "BKE_volume_grid_fwd.hh"

#pragma once

struct Volume;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

#ifdef WITH_OPENVDB

/**
 * Add a new fog VolumeGrid to the Volume by converting the supplied points.
 */
bke::VolumeGridData *fog_volume_grid_add_from_points(Volume *volume,
                                                     StringRefNull name,
                                                     Span<float3> positions,
                                                     Span<float> radii,
                                                     float voxel_size,
                                                     float density);

bke::VolumeGrid<float> points_to_sdf_grid(Span<float3> positions,
                                          Span<float> radii,
                                          float voxel_size);

#endif
}  // namespace blender::geometry

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_OPENVDB

#  include <openvdb_fwd.hh>

#  include "BKE_volume_grid_fwd.hh"

namespace blender::geometry {

openvdb::FloatGrid &resample_sdf_grid_if_necessary(bke::VolumeGrid<float> &volume_grid,
                                                   bke::VolumeTreeAccessToken &tree_token,
                                                   const openvdb::math::Transform &transform,
                                                   std::shared_ptr<openvdb::FloatGrid> &storage);

}  // namespace blender::geometry

#endif

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB

#  include "BKE_volume_grid.hh"

#  include "GEO_volume_grid_resample.hh"

#  include <openvdb/openvdb.h>
#  include <openvdb/tools/GridTransformer.h>

namespace blender::geometry {

openvdb::FloatGrid &resample_sdf_grid_if_necessary(bke::VolumeGrid<float> &volume_grid,
                                                   bke::VolumeTreeAccessToken &tree_token,
                                                   const openvdb::math::Transform &transform,
                                                   std::shared_ptr<openvdb::FloatGrid> &storage)
{
  const openvdb::FloatGrid &grid = volume_grid.grid(tree_token);
  if (grid.transform() == transform) {
    return volume_grid.grid_for_write(tree_token);
  }

  storage = openvdb::FloatGrid::create();
  storage->setTransform(transform.copy());

  /* TODO: Using #doResampleToMatch when the transform is affine and non-scaled may be faster. */
  openvdb::tools::resampleToMatch<openvdb::tools::BoxSampler>(grid, *storage);
  /* Ensure valid background value for level set grids, otherwise pruning will throw an exception.
   */
  if (storage->background() < 0.0f) {
    storage->tree().root().setBackground(0.0f, true);
  }
  openvdb::tools::pruneLevelSet(storage->tree());

  return *storage;
}

}  // namespace blender::geometry

#endif

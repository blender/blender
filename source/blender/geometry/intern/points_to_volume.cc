/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "BKE_volume.hh"
#include "BKE_volume_openvdb.hh"

#include "GEO_points_to_volume.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>

namespace blender::geometry {

/* Implements the interface required by #openvdb::tools::ParticlesToLevelSet. */
struct OpenVDBParticleList {
  using PosType = openvdb::Vec3R;

  Span<float3> positions;
  Span<float> radii;

  size_t size() const
  {
    return size_t(positions.size());
  }

  void getPos(size_t n, openvdb::Vec3R &xyz) const
  {
    xyz = &positions[n].x;
  }

  void getPosRad(size_t n, openvdb::Vec3R &xyz, openvdb::Real &radius) const
  {
    xyz = &positions[n].x;
    radius = radii[n];
  }
};

static openvdb::FloatGrid::Ptr points_to_sdf_grid(const Span<float3> positions,
                                                  const Span<float> radii)
{
  /* Create a new grid that will be filled. #ParticlesToLevelSet requires
   * the background value to be positive */
  openvdb::FloatGrid::Ptr new_grid = openvdb::FloatGrid::create(1.0f);

  /* Create a narrow-band level set grid based on the positions and radii. */
  openvdb::tools::ParticlesToLevelSet op{*new_grid};
  /* Don't ignore particles based on their radius. */
  op.setRmin(0.0f);
  op.setRmax(std::numeric_limits<float>::max());
  OpenVDBParticleList particles{positions, radii};
  op.rasterizeSpheres(particles);
  op.finalize();

  return new_grid;
}

bke::VolumeGridData *fog_volume_grid_add_from_points(Volume *volume,
                                                     const StringRefNull name,
                                                     const Span<float3> positions,
                                                     const Span<float> radii,
                                                     const float voxel_size,
                                                     const float density)
{
  openvdb::FloatGrid::Ptr new_grid = points_to_sdf_grid(positions, radii);
  new_grid->transform().postScale(voxel_size);
  new_grid->setGridClass(openvdb::GRID_FOG_VOLUME);

  /* Convert the level set to a fog volume. This also sets the background value to zero. Inside the
   * fog there will be a density of 1. */
  openvdb::tools::sdfToFogVolume(*new_grid);

  /* Take the desired density into account. */
  openvdb::tools::foreach (new_grid->beginValueOn(),
                           [&](const openvdb::FloatGrid::ValueOnIter &iter) {
                             iter.modifyValue([&](float &value) { value *= density; });
                           });

  return BKE_volume_grid_add_vdb(*volume, name, std::move(new_grid));
}

bke::VolumeGridData *sdf_volume_grid_add_from_points(Volume *volume,
                                                     const StringRefNull name,
                                                     const Span<float3> positions,
                                                     const Span<float> radii,
                                                     const float voxel_size)
{
  openvdb::FloatGrid::Ptr new_grid = points_to_sdf_grid(positions, radii);
  new_grid->transform().postScale(voxel_size);
  new_grid->setGridClass(openvdb::GRID_LEVEL_SET);

  return BKE_volume_grid_add_vdb(*volume, name, std::move(new_grid));
}
}  // namespace blender::geometry
#endif

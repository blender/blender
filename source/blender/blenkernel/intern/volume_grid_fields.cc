/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"
#include "BKE_volume_grid_fields.hh"

#ifdef WITH_OPENVDB

#  include <openvdb/math/Transform.h>

namespace blender::bke {

VoxelFieldContext::VoxelFieldContext(const openvdb::math::Transform &transform,
                                     const Span<openvdb::Coord> voxels)
    : transform_(transform), voxels_(voxels)
{
}

GVArray VoxelFieldContext::get_varray_for_input(const fn::FieldInput &field_input,
                                                const IndexMask & /*mask*/,
                                                ResourceScope & /*scope*/) const
{
  const bke::AttributeFieldInput *attribute_field_input =
      dynamic_cast<const bke::AttributeFieldInput *>(&field_input);
  if (!attribute_field_input) {
    return {};
  }
  if (attribute_field_input->attribute_name() != "position") {
    return {};
  }

  /* Support retrieving voxel positions. */
  Array<float3> positions(voxels_.size());
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const openvdb::Coord &voxel = voxels_[i];
      const openvdb::Vec3d position = transform_.indexToWorld(voxel);
      positions[i] = float3(position.x(), position.y(), position.z());
    }
  });
  return VArray<float3>::from_container(std::move(positions));
}

TilesFieldContext::TilesFieldContext(const openvdb::math::Transform &transform,
                                     const Span<openvdb::CoordBBox> tiles)
    : transform_(transform), tiles_(tiles)
{
}

GVArray TilesFieldContext::get_varray_for_input(const fn::FieldInput &field_input,
                                                const IndexMask & /*mask*/,
                                                ResourceScope & /*scope*/) const
{
  const bke::AttributeFieldInput *attribute_field_input =
      dynamic_cast<const bke::AttributeFieldInput *>(&field_input);
  if (attribute_field_input == nullptr) {
    return {};
  }
  if (attribute_field_input->attribute_name() != "position") {
    return {};
  }

  /* Support retrieving tile positions. */
  Array<float3> positions(tiles_.size());
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const openvdb::CoordBBox &tile = tiles_[i];
      const openvdb::Vec3d position = transform_.indexToWorld(tile.getCenter());
      positions[i] = float3(position.x(), position.y(), position.z());
    }
  });
  return VArray<float3>::from_container(std::move(positions));
}

}  // namespace blender::bke

#endif

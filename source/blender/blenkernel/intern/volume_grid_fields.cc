/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"
#include "BKE_volume_grid_fields.hh"

#include "BLT_translation.hh"

#ifdef WITH_OPENVDB

#  include <openvdb/math/Transform.h>

namespace blender::bke {

VoxelFieldContext::VoxelFieldContext(const openvdb::math::Transform &transform,
                                     const Span<openvdb::Coord> voxels)
    : transform_(transform), voxels_(voxels)
{
}

GVArray VoxelFieldContext::get_varray_for_input(const fn::FieldInput &field_input,
                                                const IndexMask &mask,
                                                ResourceScope &scope) const
{
  if (const auto *attribute_field = dynamic_cast<const bke::AttributeFieldInput *>(&field_input)) {
    if (attribute_field->attribute_name() == "position") {
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
  }
  if (dynamic_cast<const fn::IndexFieldInput *>(&field_input)) {
    return {};
  }
  return field_input.get_varray_for_context(*this, mask, scope);
}

TilesFieldContext::TilesFieldContext(const openvdb::math::Transform &transform,
                                     const Span<openvdb::CoordBBox> tiles)
    : transform_(transform), tiles_(tiles)
{
}

GVArray TilesFieldContext::get_varray_for_input(const fn::FieldInput &field_input,
                                                const IndexMask &mask,
                                                ResourceScope &scope) const
{
  if (const auto *attribute_field = dynamic_cast<const bke::AttributeFieldInput *>(&field_input)) {
    if (attribute_field->attribute_name() == "position") {
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
  }
  if (dynamic_cast<const fn::IndexFieldInput *>(&field_input)) {
    return {};
  }
  return field_input.get_varray_for_context(*this, mask, scope);
}

VoxelCoordinateFieldInput::VoxelCoordinateFieldInput(const math::Axis axis)
    : fn::FieldInput(CPPType::get<int>(), TIP_("Voxel Coordinate")), axis_(axis)
{
}

GVArray VoxelCoordinateFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                          const IndexMask &mask,
                                                          ResourceScope & /*scope*/) const
{
  if (const auto *voxel_context = dynamic_cast<const VoxelFieldContext *>(&context)) {
    const Span<openvdb::Coord> voxels = voxel_context->voxels();
    Array<int> result(mask.min_array_size());
    mask.foreach_index_optimized<int>([&](const int i) { result[i] = voxels[i][axis_.as_int()]; });
    return VArray<int>::from_container(std::move(result));
  }
  if (const auto *tiles_context = dynamic_cast<const TilesFieldContext *>(&context)) {
    const Span<openvdb::CoordBBox> tiles = tiles_context->tiles();
    Array<int> result(mask.min_array_size());
    mask.foreach_index_optimized<int>(
        [&](const int i) { result[i] = tiles[i].min()[axis_.as_int()]; });
    return VArray<int>::from_container(std::move(result));
  }
  return {};
}

VoxelExtentFieldInput::VoxelExtentFieldInput(const math::Axis axis)
    : fn::FieldInput(CPPType::get<int>(), TIP_("Voxel Extent")), axis_(axis)
{
}

GVArray VoxelExtentFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                      const IndexMask &mask,
                                                      ResourceScope & /*scope*/) const
{
  if (dynamic_cast<const VoxelFieldContext *>(&context)) {
    return VArray<int>::from_single(1, mask.min_array_size());
  }
  if (const auto *tiles_context = dynamic_cast<const TilesFieldContext *>(&context)) {
    const Span<openvdb::CoordBBox> tiles = tiles_context->tiles();
    Array<int> result(mask.min_array_size());
    mask.foreach_index_optimized<int>(
        [&](const int i) { result[i] = tiles[i].dim()[axis_.as_int()]; });
    return VArray<int>::from_container(std::move(result));
  }
  return {};
}

IsTileFieldInput::IsTileFieldInput() : fn::FieldInput(CPPType::get<bool>(), TIP_("Is Tile")) {}

GVArray IsTileFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                 const IndexMask &mask,
                                                 ResourceScope & /*scope*/) const
{
  if (dynamic_cast<const VoxelFieldContext *>(&context)) {
    return VArray<bool>::from_single(false, mask.min_array_size());
  }
  if (dynamic_cast<const TilesFieldContext *>(&context)) {
    return VArray<bool>::from_single(true, mask.min_array_size());
  }
  return {};
}

}  // namespace blender::bke

#endif

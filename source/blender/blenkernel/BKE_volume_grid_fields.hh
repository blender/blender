/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"

#ifdef WITH_OPENVDB

#  include "openvdb_fwd.hh"

namespace blender::bke {

/**
 * A field context that allows computing a value per voxel. Each voxel is defined by a 3D integer
 * coordinate and a transform matrix.
 */
class VoxelFieldContext : public fn::FieldContext {
 private:
  const openvdb::math::Transform &transform_;
  Span<openvdb::Coord> voxels_;

 public:
  VoxelFieldContext(const openvdb::math::Transform &transform, Span<openvdb::Coord> voxels);

  GVArray get_varray_for_input(const fn::FieldInput &field_input,
                               const IndexMask &mask,
                               ResourceScope &scope) const override;
};

/**
 * Similar to #VoxelFieldContext, but allows computing values for tiles. A tile contains multiple
 * voxels.
 */
class TilesFieldContext : public fn::FieldContext {
 private:
  const openvdb::math::Transform &transform_;
  Span<openvdb::CoordBBox> tiles_;

 public:
  TilesFieldContext(const openvdb::math::Transform &transform,
                    const Span<openvdb::CoordBBox> tiles);

  GVArray get_varray_for_input(const fn::FieldInput &field_input,
                               const IndexMask &mask,
                               ResourceScope &scope) const override;
};

}  // namespace blender::bke

#endif

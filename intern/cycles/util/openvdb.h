/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_OPENVDB_H__
#define __UTIL_OPENVDB_H__

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>

namespace openvdb {

using Vec4fTree = tree::Tree4<Vec4f, 5, 4, 3>::Type;
using Vec4fGrid = Grid<Vec4fTree>;

/* Apply operation to known grid types. */
template<typename OpType>
bool grid_type_operation(const openvdb::GridBase::ConstPtr &grid, OpType &&op)
{
  if (grid->isType<openvdb::FloatGrid>()) {
    return op.template operator()<openvdb::FloatGrid, openvdb::FloatGrid, float, 1>(grid);
  }
  else if (grid->isType<openvdb::Vec3fGrid>()) {
    return op.template operator()<openvdb::Vec3fGrid, openvdb::Vec3fGrid, openvdb::Vec3f, 3>(grid);
  }
  else if (grid->isType<openvdb::BoolGrid>()) {
    return op.template operator()<openvdb::BoolGrid, openvdb::FloatGrid, float, 1>(grid);
  }
  else if (grid->isType<openvdb::DoubleGrid>()) {
    return op.template operator()<openvdb::DoubleGrid, openvdb::FloatGrid, float, 1>(grid);
  }
  else if (grid->isType<openvdb::Int32Grid>()) {
    return op.template operator()<openvdb::Int32Grid, openvdb::FloatGrid, float, 1>(grid);
  }
  else if (grid->isType<openvdb::Int64Grid>()) {
    return op.template operator()<openvdb::Int64Grid, openvdb::FloatGrid, float, 1>(grid);
  }
  else if (grid->isType<openvdb::Vec3IGrid>()) {
    return op.template operator()<openvdb::Vec3IGrid, openvdb::Vec3fGrid, openvdb::Vec3f, 3>(grid);
  }
  else if (grid->isType<openvdb::Vec3dGrid>()) {
    return op.template operator()<openvdb::Vec3dGrid, openvdb::Vec3fGrid, openvdb::Vec3f, 3>(grid);
  }
  else if (grid->isType<openvdb::MaskGrid>()) {
    return op.template operator()<openvdb::MaskGrid, openvdb::FloatGrid, float, 1>(grid);
  }
  else {
    return false;
  }
}

};  // namespace openvdb

#endif

#endif /* __UTIL_OPENVDB_H__ */

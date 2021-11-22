/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>

namespace openvdb {

using Vec4fTree = tree::Tree4<Vec4f, 5, 4, 3>::Type;
using Vec4fGrid = Grid<Vec4fTree>;

}  // namespace openvdb

CCL_NAMESPACE_BEGIN

/* Convert to the few grid types we can render natively. */
openvdb::GridBase::ConstPtr openvdb_convert_to_known_type(const openvdb::GridBase::ConstPtr &grid);

/* Apply operation to known grid types. */
template<typename OpType>
bool openvdb_grid_type_operation(const openvdb::GridBase::ConstPtr &grid, OpType &&op)
{
  if (grid->isType<openvdb::FloatGrid>()) {
    return op.template operator()<openvdb::FloatGrid, float, 1>(
        openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid));
  }
  if (grid->isType<openvdb::Vec3fGrid>()) {
    return op.template operator()<openvdb::Vec3fGrid, openvdb::Vec3f, 3>(
        openvdb::gridConstPtrCast<openvdb::Vec3fGrid>(grid));
  }
  if (grid->isType<openvdb::Vec4fGrid>()) {
    return op.template operator()<openvdb::Vec4fGrid, openvdb::Vec4f, 4>(
        openvdb::gridConstPtrCast<openvdb::Vec4fGrid>(grid));
  }
  assert(0);
  return false;
}

/* Count number of channels in known types. */
int openvdb_num_channels(const openvdb::GridBase::ConstPtr &grid);

CCL_NAMESPACE_END

#endif

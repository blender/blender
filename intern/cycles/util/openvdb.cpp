/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPENVDB

#  include "util/openvdb.h"

#  include <openvdb/tools/Activate.h>
#  include <openvdb/tools/Dense.h>

CCL_NAMESPACE_BEGIN

/* Convert to the few data types we native can render. */
openvdb::GridBase::ConstPtr openvdb_convert_to_known_type(const openvdb::GridBase::ConstPtr &grid)
{
  if (grid->isType<openvdb::FloatGrid>()) {
    return grid;
  }
  if (grid->isType<openvdb::Vec3fGrid>()) {
    return grid;
  }
  if (grid->isType<openvdb::Vec4fGrid>()) {
    return grid;
  }
  if (grid->isType<openvdb::BoolGrid>()) {
    const openvdb::FloatGrid floatgrid(*openvdb::gridConstPtrCast<openvdb::BoolGrid>(grid));
    return std::make_shared<openvdb::FloatGrid>(std::move(floatgrid));
  }
  if (grid->isType<openvdb::DoubleGrid>()) {
    const openvdb::FloatGrid floatgrid(*openvdb::gridConstPtrCast<openvdb::DoubleGrid>(grid));
    return std::make_shared<openvdb::FloatGrid>(std::move(floatgrid));
  }
  if (grid->isType<openvdb::Int32Grid>()) {
    const openvdb::FloatGrid floatgrid(*openvdb::gridConstPtrCast<openvdb::Int32Grid>(grid));
    return std::make_shared<openvdb::FloatGrid>(std::move(floatgrid));
  }
  if (grid->isType<openvdb::Int64Grid>()) {
    const openvdb::FloatGrid floatgrid(*openvdb::gridConstPtrCast<openvdb::Int64Grid>(grid));
    return std::make_shared<openvdb::FloatGrid>(std::move(floatgrid));
  }
  if (grid->isType<openvdb::Vec3IGrid>()) {
    const openvdb::Vec3fGrid floatgrid(*openvdb::gridConstPtrCast<openvdb::Vec3IGrid>(grid));
    return std::make_shared<openvdb::Vec3fGrid>(std::move(floatgrid));
  }
  if (grid->isType<openvdb::Vec3dGrid>()) {
    const openvdb::Vec3fGrid floatgrid(*openvdb::gridConstPtrCast<openvdb::Vec3dGrid>(grid));
    return std::make_shared<openvdb::Vec3fGrid>(std::move(floatgrid));
  }
  return nullptr;
}

/* Counter number of channels. */
struct NumChannelsOp {
  int num_channels = 0;

  template<typename GridType, typename FloatDataType, const int channels>
  bool operator()(const typename GridType::ConstPtr & /*unused*/)
  {
    num_channels = channels;
    return true;
  }
};

int openvdb_num_channels(const openvdb::GridBase::ConstPtr &grid)
{
  NumChannelsOp op;
  if (!openvdb_grid_type_operation(grid, op)) {
    return 0;
  }
  return op.num_channels;
}

/* Convert OpenVDB to NanoVDB. */
#  ifdef WITH_NANOVDB
#  endif

CCL_NAMESPACE_END

#endif

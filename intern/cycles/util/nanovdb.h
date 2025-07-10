/* SPDX-FileCopyrightText: Contributors to the OpenVDB Project
 * SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_NANOVDB

#  include <openvdb/openvdb.h>

#  define NANOVDB_USE_OPENVDB
#  define NANOVDB_USE_TBB

#  include <nanovdb/NanoVDB.h>  // manages and streams the raw memory buffer of a NanoVDB grid.

#  if NANOVDB_MAJOR_VERSION_NUMBER > 32 || \
      (NANOVDB_MAJOR_VERSION_NUMBER == 32 && NANOVDB_MINOR_VERSION_NUMBER >= 7)
#    include <nanovdb/GridHandle.h>
#  else
#    include <nanovdb/util/GridHandle.h>
#  endif

CCL_NAMESPACE_BEGIN

/* Convert NanoVDB to OpenVDB mask grid that represents just the topology. */
openvdb::MaskGrid::Ptr nanovdb_to_openvdb_mask(const nanovdb::GridHandle<> &handle);

/* Convert OpenVDB to NanoVDB grid. */
nanovdb::GridHandle<> openvdb_to_nanovdb(const openvdb::GridBase::ConstPtr &grid,
                                         const int precision,
                                         const float clipping);

CCL_NAMESPACE_END

#endif

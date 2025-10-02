/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef WITH_OPENVDB

#  include <openvdb/openvdb.h>

#  include "BKE_volume_grid.hh"
#  include "BKE_volume_openvdb.hh"

#  include "BLI_function_ref.hh"
#  include "BLI_generic_pointer.hh"
#  include "BLI_generic_span.hh"
#  include "BLI_index_mask_fwd.hh"

namespace blender::bke::volume_grid {

using LeafNodeMask = openvdb::util::NodeMask<3u>;

using GetVoxelsFn = FunctionRef<void(MutableSpan<openvdb::Coord> r_voxels)>;
/**
 * Process voxels within a single leaf node. #get_voxels_fn is a mechanism to lazily create the
 * actual voxel coordinates (sometimes that isn't necessary).
 */
using ProcessLeafFn = FunctionRef<void(const LeafNodeMask &leaf_node_mask,
                                       const openvdb::CoordBBox &leaf_bbox,
                                       GetVoxelsFn get_voxels_fn)>;
/**
 * Process multiple sparse tiles.
 */
using ProcessTilesFn = FunctionRef<void(Span<openvdb::CoordBBox> tiles)>;
/**
 * Process voxels from potentially multiple leaf nodes. This is use to efficiently process voxels
 * across multiple leaf nodes with fewer active voxels.
 */
using ProcessVoxelsFn = FunctionRef<void(Span<openvdb::Coord> voxels)>;

/**
 * Splits up the work of processing different parts of the topology into multiple tasks, with
 * callbacks for each type of task called in parallel.
 */
void parallel_grid_topology_tasks(const openvdb::MaskTree &mask_tree,
                                  ProcessLeafFn process_leaf_fn,
                                  ProcessVoxelsFn process_voxels_fn,
                                  ProcessTilesFn process_tiles_fn);

template<typename GridT>
constexpr bool is_supported_grid_type = is_same_any_v<GridT,
                                                      openvdb::FloatGrid,
                                                      openvdb::Vec3fGrid,
                                                      openvdb::BoolGrid,
                                                      openvdb::Int32Grid,
                                                      openvdb::Vec4fGrid>;

template<typename Fn> inline void to_typed_grid(const openvdb::GridBase &grid_base, Fn &&fn)
{
  const VolumeGridType grid_type = get_type(grid_base);
  BKE_volume_grid_type_to_static_type(grid_type, [&](auto type_tag) {
    using GridT = typename decltype(type_tag)::type;
    if constexpr (is_supported_grid_type<GridT>) {
      fn(static_cast<const GridT &>(grid_base));
    }
    else {
      BLI_assert_unreachable();
    }
  });
}

template<typename Fn> inline void to_typed_grid(openvdb::GridBase &grid_base, Fn &&fn)
{
  const VolumeGridType grid_type = get_type(grid_base);
  BKE_volume_grid_type_to_static_type(grid_type, [&](auto type_tag) {
    using GridT = typename decltype(type_tag)::type;
    if constexpr (is_supported_grid_type<GridT>) {
      fn(static_cast<GridT &>(grid_base));
    }
    else {
      BLI_assert_unreachable();
    }
  });
}

/** Create a grid with the same activated voxels and internal nodes as the given grid. */
openvdb::GridBase::Ptr create_grid_with_topology(const openvdb::MaskTree &topology,
                                                 const openvdb::math::Transform &transform,
                                                 const VolumeGridType grid_type);

/** Set values for the given voxels in the grid. They must already be activated. */
void set_grid_values(openvdb::GridBase &grid_base, GSpan values, Span<openvdb::Coord> voxels);

/**
 * Set values for the given tiles in the grid. The tiles must be activated, but not to deeper
 * levels beyond the tile.
 */
void set_tile_values(openvdb::GridBase &grid_base, GSpan values, Span<openvdb::CoordBBox> tiles);

/**
 * Boolean grids are stored as bitmaps, but we often have to process arrays of booleans. This
 * utility sets the bitmap values based on the boolean array.
 */
void set_mask_leaf_buffer_from_bools(openvdb::BoolGrid &grid,
                                     Span<bool> values,
                                     const IndexMask &index_mask,
                                     Span<openvdb::Coord> voxels);

void set_grid_background(openvdb::GridBase &grid_base, const GPointer value);

/** See #openvdb::tools::pruneInactive. */
void prune_inactive(openvdb::GridBase &grid_base);

}  // namespace blender::bke::volume_grid

/** \} */

#endif /* WITH_OPENVDB */

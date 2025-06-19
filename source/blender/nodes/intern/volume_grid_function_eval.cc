/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_customdata.hh"
#include "BLT_translation.hh"
#include "FN_multi_function.hh"

#include "BKE_anonymous_attribute_make.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_fields.hh"
#include "BKE_volume_openvdb.hh"

#include <fmt/format.h>

#ifdef WITH_OPENVDB

#  include <openvdb/Grid.h>
#  include <openvdb/math/Transform.h>
#  include <openvdb/tools/Merge.h>

#endif

#include "volume_grid_function_eval.hh"

namespace blender::nodes {

#ifdef WITH_OPENVDB

template<typename GridT>
static constexpr bool is_supported_grid_type = is_same_any_v<GridT,
                                                             openvdb::FloatGrid,
                                                             openvdb::Vec3fGrid,
                                                             openvdb::BoolGrid,
                                                             openvdb::Int32Grid,
                                                             openvdb::Vec4fGrid>;

template<typename Fn> static void to_typed_grid(const openvdb::GridBase &grid_base, Fn &&fn)
{
  const VolumeGridType grid_type = bke::volume_grid::get_type(grid_base);
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

template<typename Fn> static void to_typed_grid(openvdb::GridBase &grid_base, Fn &&fn)
{
  const VolumeGridType grid_type = bke::volume_grid::get_type(grid_base);
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

static std::optional<VolumeGridType> cpp_type_to_grid_type(const CPPType &cpp_type)
{
  const std::optional<eCustomDataType> cd_type = bke::cpp_type_to_custom_data_type(cpp_type);
  if (!cd_type) {
    return std::nullopt;
  }
  return bke::custom_data_type_to_volume_grid_type(*cd_type);
}

using LeafNodeMask = openvdb::util::NodeMask<3u>;
using GetVoxelsFn = FunctionRef<void(MutableSpan<openvdb::Coord> r_voxels)>;
using ProcessLeafFn = FunctionRef<void(const LeafNodeMask &leaf_node_mask,
                                       const openvdb::CoordBBox &leaf_bbox,
                                       GetVoxelsFn get_voxels_fn)>;
using ProcessTilesFn = FunctionRef<void(Span<openvdb::CoordBBox> tiles)>;
using ProcessVoxelsFn = FunctionRef<void(Span<openvdb::Coord> voxels)>;

/**
 * Call #process_leaf_fn on the leaf node if it has a certain minimum number of active voxels. If
 * there are only a few active voxels, gather those in #r_coords for later batch processing.
 */
template<typename LeafNodeT>
static void parallel_grid_topology_tasks_leaf_node(const LeafNodeT &node,
                                                   const ProcessLeafFn process_leaf_fn,
                                                   Vector<openvdb::Coord, 1024> &r_coords)
{
  using NodeMaskT = typename LeafNodeT::NodeMaskType;

  const int on_count = node.onVoxelCount();
  /* This number is somewhat arbitrary. 64 is a 1/8th of the number of voxels in a standard leaf
   * which is 8x8x8. It's a trade-off between benefiting from the better performance of
   * leaf-processing vs. processing more voxels in a batch. */
  const int on_count_threshold = 64;
  if (on_count <= on_count_threshold) {
    /* The leaf contains only a few active voxels. It's beneficial to process them in a batch with
     * active voxels from other leafs. So only gather them here for later processing. */
    for (auto value_iter = node.cbeginValueOn(); value_iter.test(); ++value_iter) {
      const openvdb::Coord coord = value_iter.getCoord();
      r_coords.append(coord);
    }
    return;
  }
  /* Process entire leaf at once. This is especially beneficial when very many of the voxels in
   * the leaf are active. In that case, one can work on the openvdb arrays stored in the leafs
   * directly. */
  const NodeMaskT &value_mask = node.getValueMask();
  const openvdb::CoordBBox bbox = node.getNodeBoundingBox();
  process_leaf_fn(value_mask, bbox, [&](MutableSpan<openvdb::Coord> r_voxels) {
    for (auto value_iter = node.cbeginValueOn(); value_iter.test(); ++value_iter) {
      r_voxels[value_iter.pos()] = value_iter.getCoord();
    }
  });
}

/**
 * Calls the process functions on all the active tiles and voxels within the given internal node.
 */
template<typename InternalNodeT>
static void parallel_grid_topology_tasks_internal_node(const InternalNodeT &node,
                                                       const ProcessLeafFn process_leaf_fn,
                                                       const ProcessVoxelsFn process_voxels_fn,
                                                       const ProcessTilesFn process_tiles_fn)
{
  using ChildNodeT = typename InternalNodeT::ChildNodeType;
  using LeafNodeT = typename InternalNodeT::LeafNodeType;
  using NodeMaskT = typename InternalNodeT::NodeMaskType;
  using UnionT = typename InternalNodeT::UnionType;

  /* Gather the active sub-nodes first, to be able to parallelize over them more easily. */
  const NodeMaskT &child_mask = node.getChildMask();
  const UnionT *table = node.getTable();
  Vector<int, 512> child_indices;
  for (auto child_mask_iter = child_mask.beginOn(); child_mask_iter.test(); ++child_mask_iter) {
    child_indices.append(child_mask_iter.pos());
  }

  threading::parallel_for(child_indices.index_range(), 8, [&](const IndexRange range) {
    /* Voxels collected from potentially multiple leaf nodes to be processed in one batch. This
     * inline buffer size is sufficient to avoid an allocation in all cases (a single standard leaf
     * has 512 voxels). */
    Vector<openvdb::Coord, 1024> gathered_voxels;
    for (const int child_index : child_indices.as_span().slice(range)) {
      const ChildNodeT &child = *table[child_index].getChild();
      if constexpr (std::is_same_v<ChildNodeT, LeafNodeT>) {
        parallel_grid_topology_tasks_leaf_node(child, process_leaf_fn, gathered_voxels);
        /* If enough voxels have been gathered, process them in one batch. */
        if (gathered_voxels.size() >= 512) {
          process_voxels_fn(gathered_voxels);
          gathered_voxels.clear();
        }
      }
      else {
        /* Recurse into lower-level internal nodes. */
        parallel_grid_topology_tasks_internal_node(
            child, process_leaf_fn, process_voxels_fn, process_tiles_fn);
      }
    }
    /* Process any remaining voxels. */
    if (!gathered_voxels.is_empty()) {
      process_voxels_fn(gathered_voxels);
      gathered_voxels.clear();
    }
  });

  /* Process the active tiles within the internal node. Note that these are not processed above
   * already because there only sub-nodes are handled, but tiles are "inlined" into internal nodes.
   * All tiles are first gathered and then processed in one batch. */
  const NodeMaskT &value_mask = node.getValueMask();
  Vector<openvdb::CoordBBox> tile_bboxes;
  for (auto value_mask_iter = value_mask.beginOn(); value_mask_iter.test(); ++value_mask_iter) {
    const openvdb::Index32 index = value_mask_iter.pos();
    const openvdb::Coord tile_origin = node.offsetToGlobalCoord(index);
    const openvdb::CoordBBox tile_bbox = openvdb::CoordBBox::createCube(tile_origin,
                                                                        ChildNodeT::DIM);
    tile_bboxes.append(tile_bbox);
  }
  if (!tile_bboxes.is_empty()) {
    process_tiles_fn(tile_bboxes);
  }
}

/* Call the process functions on all active tiles and voxels in the given tree. */
static void parallel_grid_topology_tasks(const openvdb::MaskTree &mask_tree,
                                         const ProcessLeafFn process_leaf_fn,
                                         const ProcessVoxelsFn process_voxels_fn,
                                         const ProcessTilesFn process_tiles_fn)
{
  /* Iterate over the root internal nodes. */
  for (auto root_child_iter = mask_tree.cbeginRootChildren(); root_child_iter.test();
       ++root_child_iter)
  {
    const auto &internal_node = *root_child_iter;
    parallel_grid_topology_tasks_internal_node(
        internal_node, process_leaf_fn, process_voxels_fn, process_tiles_fn);
  }
}

/**
 * Call the multi-function in a batch on all active voxels in a leaf node.
 *
 * \param fn: The multi-function to call.
 * \param input_values: All input values which may be grids, fields or single values.
 * \param input_grids: The input grids already extracted from #input_values.
 * \param output_grids: The output grids to be filled with the results of the multi-function. The
 *   topology of these grids is initialized already.
 * \param transform: The transform of all input and output grids.
 * \param leaf_node_mask: Indicates which voxels in the leaf should be computed.
 * \param leaf_bbox: The bounding box of the leaf node.
 * \param get_voxels_fn: A function that extracts the active voxels from the leaf node. This
 *   function knows the order of voxels in the leaf.
 */
BLI_NOINLINE static void process_leaf_node(const mf::MultiFunction &fn,
                                           const Span<bke::SocketValueVariant *> input_values,
                                           const Span<const openvdb::GridBase *> input_grids,
                                           MutableSpan<openvdb::GridBase::Ptr> output_grids,
                                           const openvdb::math::Transform &transform,
                                           const LeafNodeMask &leaf_node_mask,
                                           const openvdb::CoordBBox &leaf_bbox,
                                           const GetVoxelsFn get_voxels_fn)
{
  /* Create an index mask for all the active voxels in the leaf. */
  IndexMaskMemory memory;
  const IndexMask index_mask = IndexMask::from_predicate(
      IndexRange(LeafNodeMask::SIZE), GrainSize(LeafNodeMask::SIZE), memory, [&](const int64_t i) {
        return leaf_node_mask.isOn(i);
      });

  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);
  mf::ParamsBuilder params{fn, &index_mask};
  mf::ContextBuilder context;

  /* We need to find the corresponding leaf nodes in all the input and output grids. That's done by
   * finding the leaf that contains this voxel. */
  const openvdb::Coord any_voxel_in_leaf = leaf_bbox.min();

  std::optional<MutableSpan<openvdb::Coord>> voxel_coords_opt;
  auto ensure_voxel_coords = [&]() {
    if (!voxel_coords_opt.has_value()) {
      voxel_coords_opt = scope.allocator().allocate_array<openvdb::Coord>(
          index_mask.min_array_size());
      get_voxels_fn(voxel_coords_opt.value());
    }
    return *voxel_coords_opt;
  };

  for (const int input_i : input_values.index_range()) {
    const bke::SocketValueVariant &value_variant = *input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *grid_base = input_grids[input_i]) {
      /* The input is a grid, so we can attempt to reference the grid values directly. */
      to_typed_grid(*grid_base, [&](const auto &grid) {
        using GridT = typename std::decay_t<decltype(grid)>;
        using ValueT = typename GridT::ValueType;
        BLI_assert(param_cpp_type.size == sizeof(ValueT));
        const auto &tree = grid.tree();

        if (const auto *leaf_node = tree.probeLeaf(any_voxel_in_leaf)) {
          /* Boolean grids are special because they encode the values as bitmask. So create a
           * temporary buffer for the inputs. */
          if constexpr (std::is_same_v<ValueT, bool>) {
            const Span<openvdb::Coord> voxels = ensure_voxel_coords();
            MutableSpan<bool> values = scope.allocator().allocate_array<bool>(
                index_mask.min_array_size());
            index_mask.foreach_index([&](const int64_t i) {
              const openvdb::Coord &coord = voxels[i];
              values[i] = tree.getValue(coord);
            });
            params.add_readonly_single_input(values);
          }
          else {
            const Span<ValueT> values(leaf_node->buffer().data(), LeafNodeMask::SIZE);
            const LeafNodeMask &input_leaf_mask = leaf_node->valueMask();
            const LeafNodeMask missing_mask = leaf_node_mask & !input_leaf_mask;
            if (missing_mask.isOff()) {
              /* All values available, so reference the data directly. */
              params.add_readonly_single_input(
                  GSpan(param_cpp_type, values.data(), values.size()));
            }
            else {
              /* Fill in the missing values with the background value. */
              MutableSpan copied_values = scope.allocator().construct_array_copy(values);
              const auto &background = tree.background();
              for (auto missing_it = missing_mask.beginOn(); missing_it.test(); ++missing_it) {
                const int index = missing_it.pos();
                copied_values[index] = background;
              }
              params.add_readonly_single_input(
                  GSpan(param_cpp_type, copied_values.data(), copied_values.size()));
            }
          }
        }
        else {
          /* The input does not have this leaf node, so just get the value that's used for the
           * entire leaf. The leaf may be in a tile or is inactive in which case the background
           * value is used. */
          const auto &single_value = tree.getValue(any_voxel_in_leaf);
          params.add_readonly_single_input(GPointer(param_cpp_type, &single_value));
        }
      });
    }
    else if (value_variant.is_context_dependent_field()) {
      /* Compute the field on all active voxels in the leaf and pass the result to the
       * multi-function. */
      const fn::GField field = value_variant.get<fn::GField>();
      const CPPType &type = field.cpp_type();
      const Span<openvdb::Coord> voxels = ensure_voxel_coords();
      bke::VoxelFieldContext field_context{transform, voxels};
      fn::FieldEvaluator evaluator{field_context, &index_mask};
      GMutableSpan values{
          type, scope.allocator().allocate_array(type, voxels.size()), voxels.size()};
      evaluator.add_with_destination(field, values);
      evaluator.evaluate();
      params.add_readonly_single_input(values);
    }
    else {
      /* Pass the single value directly to the multi-function. */
      params.add_readonly_single_input(value_variant.get_single_ptr());
    }
  }

  for (const int output_i : output_grids.index_range()) {
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    openvdb::GridBase &grid_base = *output_grids[output_i];
    to_typed_grid(grid_base, [&](auto &grid) {
      using GridT = typename std::decay_t<decltype(grid)>;
      using ValueT = typename GridT::ValueType;

      auto &tree = grid.tree();
      auto *leaf_node = tree.probeLeaf(any_voxel_in_leaf);
      /* Should have been added before. */
      BLI_assert(leaf_node);

      /* Boolean grids are special because they encode the values as bitmask. */
      if constexpr (std::is_same_v<ValueT, bool>) {
        MutableSpan<bool> values = scope.allocator().allocate_array<bool>(
            index_mask.min_array_size());
        params.add_uninitialized_single_output(values);
      }
      else {
        /* Write directly into the buffer of the output leaf node. */
        ValueT *values = leaf_node->buffer().data();
        params.add_uninitialized_single_output(
            GMutableSpan(param_cpp_type, values, LeafNodeMask::SIZE));
      }
    });
  }

  /* Actually call the multi-function which will write the results into the output grids (except
   * for boolean grids). */
  fn.call_auto(index_mask, params, context);

  for (const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &param_cpp_type = param_type.data_type().single_type();
    if (!param_cpp_type.is<bool>()) {
      continue;
    }
    openvdb::BoolGrid &grid = static_cast<openvdb::BoolGrid &>(*output_grids[output_i]);
    const Span<bool> values = params.computed_array(param_index).typed<bool>();
    auto accessor = grid.getUnsafeAccessor();
    const Span<openvdb::Coord> voxels = ensure_voxel_coords();
    index_mask.foreach_index([&](const int64_t i) {
      const openvdb::Coord &coord = voxels[i];
      accessor.setValue(coord, values[i]);
    });
  }
}

/**
 * Call the multi-function in a batch on all the given voxels.
 *
 * \param fn: The multi-function to call.
 * \param input_values: All input values which may be grids, fields or single values.
 * \param input_grids: The input grids already extracted from #input_values.
 * \param output_grids: The output grids to be filled with the results of the multi-function. The
 *   topology of these grids is initialized already.
 * \param transform: The transform of all input and output grids.
 * \param voxels: The voxels to process.
 */
BLI_NOINLINE static void process_voxels(const mf::MultiFunction &fn,
                                        const Span<bke::SocketValueVariant *> input_values,
                                        const Span<const openvdb::GridBase *> input_grids,
                                        MutableSpan<openvdb::GridBase::Ptr> output_grids,
                                        const openvdb::math::Transform &transform,
                                        const Span<openvdb::Coord> voxels)
{
  const int64_t voxels_num = voxels.size();
  const IndexMask index_mask{voxels_num};
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);
  mf::ParamsBuilder params{fn, &index_mask};
  mf::ContextBuilder context;

  for (const int input_i : input_values.index_range()) {
    const bke::SocketValueVariant &value_variant = *input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *grid_base = input_grids[input_i]) {
      /* Retrieve all voxel values from the input grid. */
      to_typed_grid(*grid_base, [&](const auto &grid) {
        using ValueType = typename std::decay_t<decltype(grid)>::ValueType;
        const auto &tree = grid.tree();
        /* Could try to cache the accessor across batches, but it's not straight forward since its
         * type depends on the grid type and thread-safety has to be maintained. It's likely not
         * worth it because the cost is already negligible since we are processing a full batch. */
        auto accessor = grid.getConstUnsafeAccessor();

        MutableSpan<ValueType> values = scope.allocator().allocate_array<ValueType>(voxels_num);
        for (const int64_t i : IndexRange(voxels_num)) {
          const openvdb::Coord &coord = voxels[i];
          values[i] = tree.getValue(coord, accessor);
        }
        BLI_assert(param_cpp_type.size == sizeof(ValueType));
        params.add_readonly_single_input(GSpan(param_cpp_type, values.data(), voxels_num));
      });
    }
    else if (value_variant.is_context_dependent_field()) {
      /* Evaluate the field on all voxels. */
      const fn::GField field = value_variant.get<fn::GField>();
      const CPPType &type = field.cpp_type();
      bke::VoxelFieldContext field_context{transform, voxels};
      fn::FieldEvaluator evaluator{field_context, voxels_num};
      GMutableSpan values{type, scope.allocator().allocate_array(type, voxels_num), voxels_num};
      evaluator.add_with_destination(field, values);
      evaluator.evaluate();
      params.add_readonly_single_input(values);
    }
    else {
      /* Pass the single value directly to the multi-function. */
      params.add_readonly_single_input(value_variant.get_single_ptr());
    }
  }

  /* Prepare temporary output buffers for the field evaluation. Those will later be copied into the
   * output grids. */
  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &type = param_type.data_type().single_type();
    void *buffer = scope.allocator().allocate_array(type, voxels_num);
    params.add_uninitialized_single_output(GMutableSpan{type, buffer, voxels_num});
  }

  /* Actually call the multi-function which will fill the temporary output buffers. */
  fn.call_auto(index_mask, params, context);

  /* Copy the values from the temporary buffers into the output grids. */
  for (const int output_i : output_grids.index_range()) {
    openvdb::GridBase &grid_base = *output_grids[output_i];
    to_typed_grid(grid_base, [&](auto &grid) {
      using GridT = std::decay_t<decltype(grid)>;
      using ValueType = typename GridT::ValueType;
      const int param_index = input_values.size() + output_i;
      const ValueType *computed_values = static_cast<const ValueType *>(
          params.computed_array(param_index).data());

      auto accessor = grid.getUnsafeAccessor();
      for (const int64_t i : IndexRange(voxels_num)) {
        const openvdb::Coord &coord = voxels[i];
        const ValueType &value = computed_values[i];
        accessor.setValue(coord, value);
      }
    });
  }
}

/**
 * Call the multi-function in a batch on all the given tiles. It is assumed that all input grids
 * are constant within the given tiles.
 *
 * \param fn: The multi-function to call.
 * \param input_values: All input values which may be grids, fields or single values.
 * \param input_grids: The input grids already extracted from #input_values.
 * \param output_grids: The output grids to be filled with the results of the multi-function. The
 *   topology of these grids is initialized already.
 * \param transform: The transform of all input and output grids.
 * \param tiles: The tiles to process.
 */
BLI_NOINLINE static void process_tiles(const mf::MultiFunction &fn,
                                       const Span<bke::SocketValueVariant *> input_values,
                                       const Span<const openvdb::GridBase *> input_grids,
                                       MutableSpan<openvdb::GridBase::Ptr> output_grids,
                                       const openvdb::math::Transform &transform,
                                       const Span<openvdb::CoordBBox> tiles)
{
  const int64_t tiles_num = tiles.size();
  const IndexMask index_mask{tiles_num};

  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);
  mf::ParamsBuilder params{fn, &index_mask};
  mf::ContextBuilder context;

  for (const int input_i : input_values.index_range()) {
    const bke::SocketValueVariant &value_variant = *input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *grid_base = input_grids[input_i]) {
      /* Sample the tile values from the input grid. */
      to_typed_grid(*grid_base, [&](const auto &grid) {
        using GridT = std::decay_t<decltype(grid)>;
        using ValueType = typename GridT::ValueType;
        const auto &tree = grid.tree();
        auto accessor = grid.getConstUnsafeAccessor();

        MutableSpan<ValueType> values = scope.allocator().allocate_array<ValueType>(tiles_num);
        for (const int64_t i : IndexRange(tiles_num)) {
          const openvdb::CoordBBox &tile = tiles[i];
          /* The tile is assumed to have a single constant value. Therefore, we can get the value
           * from any voxel in that tile as representative. */
          const openvdb::Coord any_coord_in_tile = tile.min();
          values[i] = tree.getValue(any_coord_in_tile, accessor);
        }
        BLI_assert(param_cpp_type.size == sizeof(ValueType));
        params.add_readonly_single_input(GSpan(param_cpp_type, values.data(), tiles_num));
      });
    }
    else if (value_variant.is_context_dependent_field()) {
      /* Evaluate the field on all tiles. */
      const fn::GField field = value_variant.get<fn::GField>();
      const CPPType &type = field.cpp_type();
      bke::TilesFieldContext field_context{transform, tiles};
      fn::FieldEvaluator evaluator{field_context, tiles_num};
      GMutableSpan values{type, scope.allocator().allocate_array(type, tiles_num), tiles_num};
      evaluator.add_with_destination(field, values);
      evaluator.evaluate();
      params.add_readonly_single_input(values);
    }
    else {
      /* Pass the single value directly to the multi-function. */
      params.add_readonly_single_input(value_variant.get_single_ptr());
    }
  }

  /* Prepare temporary output buffers for the field evaluation. Those will later be copied into the
   * output grids. */
  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &type = param_type.data_type().single_type();
    void *buffer = scope.allocator().allocate_array(type, tiles_num);
    params.add_uninitialized_single_output(GMutableSpan{type, buffer, tiles_num});
  }

  /* Actually call the multi-function which will fill the temporary output buffers. */
  fn.call_auto(index_mask, params, context);

  /* Copy the values from the temporary buffers into the output grids. */
  for (const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    openvdb::GridBase &grid_base = *output_grids[output_i];
    to_typed_grid(grid_base, [&](auto &grid) {
      using GridT = typename std::decay_t<decltype(grid)>;
      using TreeT = typename GridT::TreeType;
      using ValueType = typename GridT::ValueType;
      auto &tree = grid.tree();

      const ValueType *computed_values = static_cast<const ValueType *>(
          params.computed_array(param_index).data());

      const auto set_tile_value =
          [&](auto &node, const openvdb::Coord &coord_in_tile, auto value) {
            const openvdb::Index n = node.coordToOffset(coord_in_tile);
            BLI_assert(node.isChildMaskOff(n));
            /* TODO: Figure out how to do this without const_cast, although the same is done in
             * `openvdb_ax/openvdb_ax/compiler/VolumeExecutable.cc` which has a similar purpose.
             * It seems like OpenVDB generally allows that, but it does not have a proper public
             * API for this yet. */
            using UnionType = typename std::decay_t<decltype(node)>::UnionType;
            auto *table = const_cast<UnionType *>(node.getTable());
            table[n].setValue(value);
          };

      for (const int i : IndexRange(tiles_num)) {
        const openvdb::CoordBBox tile = tiles[i];
        const openvdb::Coord coord_in_tile = tile.min();
        const auto &computed_value = computed_values[i];
        using InternalNode1 = typename TreeT::RootNodeType::ChildNodeType;
        using InternalNode2 = typename InternalNode1::ChildNodeType;
        /* Find the internal node that contains the tile and update the value in there. */
        if (auto *node = tree.template probeNode<InternalNode2>(coord_in_tile)) {
          set_tile_value(*node, coord_in_tile, computed_value);
        }
        else if (auto *node = tree.template probeNode<InternalNode1>(coord_in_tile)) {
          set_tile_value(*node, coord_in_tile, computed_value);
        }
        else {
          BLI_assert_unreachable();
        }
      }
    });
  }
}

BLI_NOINLINE static void process_background(const mf::MultiFunction &fn,
                                            const Span<bke::SocketValueVariant *> input_values,
                                            const Span<const openvdb::GridBase *> input_grids,
                                            const openvdb::math::Transform &transform,
                                            MutableSpan<openvdb::GridBase::Ptr> output_grids)
{
  AlignedBuffer<160, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);

  const IndexMask mask(1);
  mf::ParamsBuilder params(fn, &mask);
  mf::ContextBuilder context;

  for (const int input_i : input_values.index_range()) {
    const bke::SocketValueVariant &value_variant = *input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *grid_base = input_grids[input_i]) {
      to_typed_grid(*grid_base, [&](const auto &grid) {
#  ifndef NDEBUG
        using GridT = std::decay_t<decltype(grid)>;
        using ValueType = typename GridT::ValueType;
        BLI_assert(param_cpp_type.size == sizeof(ValueType));
#  endif
        const auto &tree = grid.tree();
        params.add_readonly_single_input(GPointer(param_cpp_type, &tree.background()));
      });
      continue;
    }

    if (value_variant.is_context_dependent_field()) {
      const fn::GField field = value_variant.get<fn::GField>();
      const CPPType &type = field.cpp_type();
      static const openvdb::CoordBBox background_space = openvdb::CoordBBox::inf();
      bke::TilesFieldContext field_context(transform,
                                           Span<openvdb::CoordBBox>(&background_space, 1));
      fn::FieldEvaluator evaluator(field_context, 1);
      GMutableSpan value(type, scope.allocator().allocate(type), 1);
      evaluator.add_with_destination(field, value);
      evaluator.evaluate();
      params.add_readonly_single_input(GPointer(type, value.data()));
      continue;
    }

    params.add_readonly_single_input(value_variant.get_single_ptr());
  }

  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &type = param_type.data_type().single_type();

    GMutableSpan value_buffer(type, scope.allocator().allocate(type), 1);
    params.add_uninitialized_single_output(value_buffer);
  }

  fn.call_auto(mask, params, context);

  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    const GSpan value = params.computed_array(param_index);

    openvdb::GridBase &grid_base = *output_grids[output_i];
    to_typed_grid(grid_base, [&](auto &grid) {
      using GridT = std::decay_t<decltype(grid)>;
      using ValueType = typename GridT::ValueType;
      auto &tree = grid.tree();

      BLI_assert(value.type().size == sizeof(ValueType));
      tree.root().setBackground(*static_cast<const ValueType *>(value.data()), true);
    });
  }
}

bool execute_multi_function_on_value_variant__volume_grid(
    const mf::MultiFunction &fn,
    const Span<bke::SocketValueVariant *> input_values,
    const Span<bke::SocketValueVariant *> output_values,
    std::string &r_error_message)
{
  const int inputs_num = input_values.size();
  Array<bke::VolumeTreeAccessToken> input_volume_tokens(inputs_num);
  Array<const openvdb::GridBase *> input_grids(inputs_num, nullptr);

  for (const int input_i : IndexRange(inputs_num)) {
    bke::SocketValueVariant &value_variant = *input_values[input_i];
    if (value_variant.is_volume_grid()) {
      const bke::GVolumeGrid g_volume_grid = value_variant.get<bke::GVolumeGrid>();
      input_grids[input_i] = &g_volume_grid->grid(input_volume_tokens[input_i]);
    }
    else if (value_variant.is_context_dependent_field()) {
      /* Nothing to do here. The field is evaluated later. */
    }
    else {
      value_variant.convert_to_single();
    }
  }

  const openvdb::math::Transform *transform = nullptr;
  for (const openvdb::GridBase *grid : input_grids) {
    if (!grid) {
      continue;
    }
    const openvdb::math::Transform &other_transform = grid->transform();
    if (!transform) {
      transform = &other_transform;
      continue;
    }
    if (*transform != other_transform) {
      r_error_message = TIP_("Input grids have incompatible transforms");
      return false;
    }
  }
  if (transform == nullptr) {
    r_error_message = TIP_("No input grid found that can determine the topology");
    return false;
  }

  openvdb::MaskTree mask_tree;
  for (const openvdb::GridBase *grid : input_grids) {
    if (!grid) {
      continue;
    }
    to_typed_grid(*grid, [&](const auto &grid) { mask_tree.topologyUnion(grid.tree()); });
  }

  Array<openvdb::GridBase::Ptr> output_grids(output_values.size());
  for (const int i : output_values.index_range()) {
    const int param_index = input_values.size() + i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &cpp_type = param_type.data_type().single_type();
    const std::optional<VolumeGridType> grid_type = cpp_type_to_grid_type(cpp_type);
    if (!grid_type) {
      r_error_message = TIP_("Grid type not supported");
      return false;
    }

    openvdb::GridBase::Ptr grid;
    BKE_volume_grid_type_to_static_type(*grid_type, [&](auto type_tag) {
      using GridT = typename decltype(type_tag)::type;
      using TreeT = typename GridT::TreeType;
      using ValueType = typename TreeT::ValueType;
      const ValueType background{};
      auto tree = std::make_shared<TreeT>(mask_tree, background, openvdb::TopologyCopy());
      grid = openvdb::createGrid(std::move(tree));
    });

    grid->setTransform(transform->copy());
    output_grids[i] = std::move(grid);
  }

  parallel_grid_topology_tasks(
      mask_tree,
      [&](const LeafNodeMask &leaf_node_mask,
          const openvdb::CoordBBox &leaf_bbox,
          const GetVoxelsFn get_voxels_fn) {
        process_leaf_node(fn,
                          input_values,
                          input_grids,
                          output_grids,
                          *transform,
                          leaf_node_mask,
                          leaf_bbox,
                          get_voxels_fn);
      },
      [&](const Span<openvdb::Coord> voxels) {
        process_voxels(fn, input_values, input_grids, output_grids, *transform, voxels);
      },
      [&](const Span<openvdb::CoordBBox> tiles) {
        process_tiles(fn, input_values, input_grids, output_grids, *transform, tiles);
      });

  process_background(fn, input_values, input_grids, *transform, output_grids);

  for (const int i : output_values.index_range()) {
    if (bke::SocketValueVariant *output_value = output_values[i]) {
      output_value->set(bke::GVolumeGrid(std::move(output_grids[i])));
    }
  }

  return true;
}

#else

bool execute_multi_function_on_value_variant__volume_grid(
    const mf::MultiFunction & /*fn*/,
    const Span<bke::SocketValueVariant *> /*input_values*/,
    const Span<bke::SocketValueVariant *> /*output_values*/,
    std::string &r_error_message)
{
  r_error_message = TIP_("Compiled without OpenVDB");
  return false;
}

#endif

}  // namespace blender::nodes

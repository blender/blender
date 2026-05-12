/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB

#  include "DNA_customdata_types.h"

#  include "BKE_attribute_legacy_convert.hh"
#  include "BKE_customdata.hh"
#  include "BKE_volume_enums.hh"
#  include "BKE_volume_grid_fields.hh"
#  include "BKE_volume_grid_multi_function_eval.hh"
#  include "BKE_volume_grid_process.hh"

#  include "BLT_translation.hh"

#  include "FN_field_evaluation.hh"

namespace blender::bke::volume_grid::multi_function_eval {

static std::optional<VolumeGridType> cpp_type_to_grid_type(const CPPType &cpp_type)
{
  const std::optional<eCustomDataType> cd_type = bke::cpp_type_to_custom_data_type(cpp_type);
  if (!cd_type) {
    return std::nullopt;
  }
  return bke::custom_data_type_to_volume_grid_type(*cd_type);
}

/**
 * Call the multi-function in a batch on all active voxels in a leaf node.
 *
 * \param fn: The multi-function to call.
 * \param input_values: All input values which may be grids, fields or single values.
 * \param output_grids: The output grids to be filled with the results of the multi-function. The
 *   topology of these grids is initialized already. May be null if the output is not needed.
 * \param transform: The transform of all input and output grids.
 * \param leaf_node_mask: Indicates which voxels in the leaf should be computed.
 * \param leaf_bbox: The bounding box of the leaf node.
 * \param get_voxels_fn: A function that extracts the active voxels from the leaf node. This
 *   function knows the order of voxels in the leaf.
 */
BLI_NOINLINE static void process_leaf_node(const mf::MultiFunction &fn,
                                           const Span<InputVariant> input_values,
                                           MutableSpan<openvdb::GridBase::Ptr> output_grids,
                                           const openvdb::math::Transform &transform,
                                           const LeafNodeMask &leaf_node_mask,
                                           const openvdb::CoordBBox &leaf_bbox,
                                           const GetVoxelsFn get_voxels_fn)
{
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);

  /* Create an index mask for all the active voxels in the leaf. */
  const IndexMask index_mask = IndexMask::from_predicate(
      IndexRange(LeafNodeMask::SIZE),
      scope.allocator(),
      [&](const int64_t i) { return leaf_node_mask.isOn(i); },
      exec_mode::serial);

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
    const InputVariant &value_variant = input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *const *grid_base = std::get_if<const openvdb::GridBase *>(
            &value_variant))
    {
      /* The input is a grid, so we can attempt to reference the grid values directly. */
      to_typed_grid(**grid_base, [&](const auto &grid) {
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
            index_mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
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
    else if (const fn::GField *const *field = std::get_if<const fn::GField *>(&value_variant)) {
      /* Compute the field on all active voxels in the leaf and pass the result to the
       * multi-function. */
      const CPPType &type = (*field)->cpp_type();
      const Span<openvdb::Coord> voxels = ensure_voxel_coords();
      bke::VoxelFieldContext field_context{transform, voxels};
      fn::FieldEvaluator evaluator{field_context, &index_mask};
      GMutableSpan values{
          type, scope.allocator().allocate_array(type, voxels.size()), voxels.size()};
      evaluator.add_with_destination(**field, values);
      evaluator.evaluate();
      params.add_readonly_single_input(values);
    }
    else {
      /* Pass the single value directly to the multi-function. */
      params.add_readonly_single_input(std::get<GPointer>(value_variant));
    }
  }

  for (const int output_i : output_grids.index_range()) {
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();
    if (!output_grids[output_i]) {
      params.add_ignored_single_output();
      continue;
    }

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
  fn.prepare_for_execution();
  fn.call_auto(index_mask, params, context);

  for (const int output_i : output_grids.index_range()) {
    const int param_index = input_values.size() + output_i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &param_cpp_type = param_type.data_type().single_type();
    if (!param_cpp_type.is<bool>()) {
      continue;
    }
    set_mask_leaf_buffer_from_bools(static_cast<openvdb::BoolGrid &>(*output_grids[output_i]),
                                    params.computed_array(param_index).typed<bool>(),
                                    index_mask,
                                    ensure_voxel_coords());
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
                                        const Span<InputVariant> input_values,
                                        MutableSpan<openvdb::GridBase::Ptr> output_grids,
                                        const openvdb::math::Transform &transform,
                                        const Span<openvdb::Coord> voxels)
{
  const int64_t voxels_num = voxels.size();
  const IndexMask index_mask{voxels_num};
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);
  mf::ParamsBuilder params{fn, &index_mask};
  mf::ContextBuilder context;

  for (const int input_i : input_values.index_range()) {
    const InputVariant &value_variant = input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *const *grid_base = std::get_if<const openvdb::GridBase *>(
            &value_variant))
    {
      /* Retrieve all voxel values from the input grid. */
      to_typed_grid(**grid_base, [&](const auto &grid) {
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
    else if (const fn::GField *const *field = std::get_if<const fn::GField *>(&value_variant)) {
      /* Evaluate the field on all voxels.
       * TODO: Collect fields from all inputs to evaluate together. */
      const CPPType &type = (*field)->cpp_type();
      bke::VoxelFieldContext field_context{transform, voxels};
      fn::FieldEvaluator evaluator{field_context, voxels_num};
      GMutableSpan values{type, scope.allocator().allocate_array(type, voxels_num), voxels_num};
      evaluator.add_with_destination(**field, values);
      evaluator.evaluate();
      params.add_readonly_single_input(values);
    }
    else {
      /* Pass the single value directly to the multi-function. */
      params.add_readonly_single_input(std::get<GPointer>(value_variant));
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
    if (!output_grids[output_i]) {
      continue;
    }
    const int param_index = input_values.size() + output_i;
    set_grid_values(*output_grids[output_i], params.computed_array(param_index), voxels);
  }
}

/**
 * Call the multi-function in a batch on all the given tiles. It is assumed that all input grids
 * are constant within the given tiles.
 *
 * \param fn: The multi-function to call.
 * \param input_values: All input values which may be grids, fields or single values.
 * \param output_grids: The output grids to be filled with the results of the multi-function. The
 *   topology of these grids is initialized already.
 * \param transform: The transform of all input and output grids.
 * \param tiles: The tiles to process.
 */
BLI_NOINLINE static void process_tiles(const mf::MultiFunction &fn,
                                       const Span<InputVariant> input_values,
                                       MutableSpan<openvdb::GridBase::Ptr> output_grids,
                                       const openvdb::math::Transform &transform,
                                       const Span<openvdb::CoordBBox> tiles)
{
  const int64_t tiles_num = tiles.size();
  const IndexMask index_mask{tiles_num};

  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);
  mf::ParamsBuilder params{fn, &index_mask};
  mf::ContextBuilder context;

  for (const int input_i : input_values.index_range()) {
    const InputVariant &value_variant = input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *const *grid_base = std::get_if<const openvdb::GridBase *>(
            &value_variant))
    {
      /* Sample the tile values from the input grid. */
      to_typed_grid(**grid_base, [&](const auto &grid) {
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
    else if (const fn::GField *const *field = std::get_if<const fn::GField *>(&value_variant)) {
      /* Evaluate the field on all tiles.
       * TODO: Gather fields from all inputs to evaluate together. */
      const CPPType &type = (*field)->cpp_type();
      bke::TilesFieldContext field_context{transform, tiles};
      fn::FieldEvaluator evaluator{field_context, tiles_num};
      GMutableSpan values{type, scope.allocator().allocate_array(type, tiles_num), tiles_num};
      evaluator.add_with_destination(**field, values);
      evaluator.evaluate();
      params.add_readonly_single_input(values);
    }
    else {
      /* Pass the single value directly to the multi-function. */
      params.add_readonly_single_input(std::get<GPointer>(value_variant));
    }
  }

  /* Prepare temporary output buffers for the field evaluation. Those will later be copied into the
   * output grids. */
  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    if (!output_grids[output_i]) {
      params.add_ignored_single_output();
      continue;
    }
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
    if (!output_grids[output_i]) {
      continue;
    }
    const int param_index = input_values.size() + output_i;
    set_tile_values(*output_grids[output_i], params.computed_array(param_index), tiles);
  }
}

BLI_NOINLINE static void process_background(const mf::MultiFunction &fn,
                                            const Span<InputVariant> input_values,
                                            const openvdb::math::Transform &transform,
                                            MutableSpan<openvdb::GridBase::Ptr> output_grids)
{
  AlignedBuffer<160, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);

  const IndexMask mask(1);
  mf::ParamsBuilder params(fn, &mask);
  mf::ContextBuilder context;

  for (const int input_i : input_values.index_range()) {
    const InputVariant &value_variant = input_values[input_i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &param_cpp_type = param_type.data_type().single_type();

    if (const openvdb::GridBase *const *grid_base = std::get_if<const openvdb::GridBase *>(
            &value_variant))
    {
      to_typed_grid(**grid_base, [&](const auto &grid) {
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

    if (const fn::GField *const *field = std::get_if<const fn::GField *>(&value_variant)) {
      const CPPType &type = (*field)->cpp_type();
      static const openvdb::CoordBBox background_space = openvdb::CoordBBox::inf();
      bke::TilesFieldContext field_context(transform,
                                           Span<openvdb::CoordBBox>(&background_space, 1));
      fn::FieldEvaluator evaluator(field_context, 1);
      GMutableSpan value(type, scope.allocator().allocate(type), 1);
      evaluator.add_with_destination(**field, value);
      evaluator.evaluate();
      params.add_readonly_single_input(GPointer(type, value.data()));
      continue;
    }

    params.add_readonly_single_input(std::get<GPointer>(value_variant));
  }

  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    if (!output_grids[output_i]) {
      params.add_ignored_single_output();
      continue;
    }
    const int param_index = input_values.size() + output_i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &type = param_type.data_type().single_type();

    GMutableSpan value_buffer(type, scope.allocator().allocate(type), 1);
    params.add_uninitialized_single_output(value_buffer);
  }

  fn.call_auto(mask, params, context);

  for ([[maybe_unused]] const int output_i : output_grids.index_range()) {
    if (!output_grids[output_i]) {
      continue;
    }
    const int param_index = input_values.size() + output_i;
    const GSpan value = params.computed_array(param_index);
    set_grid_background(*output_grids[output_i], GPointer(value.type(), value.data()));
  }
}

EvalResult evaluate_multi_function_on_grid(const mf::MultiFunction &fn,
                                           const Span<InputVariant> input_values,
                                           const Span<bool> output_usages)
{
  int inputs_num = 0;
  int outputs_num = 0;
  for (const int param_i : fn.param_indices()) {
    const mf::ParamType param_type = fn.param_type(param_i);
    if (param_type.interface_type() == mf::ParamType::Input) {
      inputs_num++;
    }
    else if (param_type.interface_type() == mf::ParamType::Output) {
      outputs_num++;
    }
    else {
      BLI_assert_unreachable();
    }
  }

  BLI_assert(input_values.size() == inputs_num);
  BLI_assert(output_usages.size() == outputs_num);
  Array<bke::VolumeTreeAccessToken> input_volume_tokens(inputs_num);

  Vector<const openvdb::GridBase *> input_grids;
  for (const int input_i : IndexRange(inputs_num)) {
    const InputVariant &value_variant = input_values[input_i];
    if (const openvdb::GridBase *const *grid = std::get_if<const openvdb::GridBase *>(
            &value_variant))
    {
      input_grids.append(*grid);
    }
  }

  const openvdb::math::Transform *transform = nullptr;
  for (const openvdb::GridBase *grid : input_grids) {
    const openvdb::math::Transform &other_transform = grid->transform();
    if (!transform) {
      transform = &other_transform;
      continue;
    }
    if (*transform != other_transform) {
      return {EvalResult::Failure{TIP_("Input grids have incompatible transforms")}};
    }
  }
  if (transform == nullptr) {
    return {EvalResult::Failure{TIP_("No input grid found that can determine the topology")}};
  }

  openvdb::MaskTree mask_tree;
  for (const openvdb::GridBase *grid : input_grids) {
    to_typed_grid(*grid, [&](const auto &grid) { mask_tree.topologyUnion(grid.tree()); });
  }

  Array<openvdb::GridBase::Ptr> output_grids(outputs_num);
  for (const int i : IndexRange(outputs_num)) {
    if (!output_usages[i]) {
      continue;
    }
    const int param_index = input_values.size() + i;
    const mf::ParamType param_type = fn.param_type(param_index);
    const CPPType &cpp_type = param_type.data_type().single_type();
    const std::optional<VolumeGridType> grid_type = cpp_type_to_grid_type(cpp_type);
    if (!grid_type) {
      return {EvalResult::Failure{TIP_("Grid type not supported")}};
    }
    output_grids[i] = create_grid_with_topology(mask_tree, *transform, *grid_type);
  }

  fn.prepare_for_execution();

  parallel_grid_topology_tasks(
      mask_tree,
      [&](const LeafNodeMask &leaf_node_mask,
          const openvdb::CoordBBox &leaf_bbox,
          const GetVoxelsFn get_voxels_fn) {
        process_leaf_node(
            fn, input_values, output_grids, *transform, leaf_node_mask, leaf_bbox, get_voxels_fn);
      },
      [&](const Span<openvdb::Coord> voxels) {
        process_voxels(fn, input_values, output_grids, *transform, voxels);
      },
      [&](const Span<openvdb::CoordBBox> tiles) {
        process_tiles(fn, input_values, output_grids, *transform, tiles);
      });

  process_background(fn, input_values, *transform, output_grids);

  return {EvalResult::Success{std::move(output_grids)}};
}

}  // namespace blender::bke::volume_grid::multi_function_eval

#endif

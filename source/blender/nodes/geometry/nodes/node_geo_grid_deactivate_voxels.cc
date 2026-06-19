/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_volume_grid.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#ifdef WITH_OPENVDB
#  include "BKE_volume_grid_fields.hh"
#  include "BKE_volume_grid_process.hh"
#endif

namespace blender::nodes::node_geo_grid_deactivate_voxels {

namespace volume_grid = bke::volume_grid;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();

  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (!node || !tree) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  b.add_input(data_type, "Grid"_ustr).hide_value().structure_type(StructureType::Grid);
  b.add_output(data_type, "Grid"_ustr)
      .structure_type(StructureType::Grid)
      .align_with_previous()
      .propagate_references({1});

  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .structure_type(StructureType::Field);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static std::optional<eNodeSocketDatatype> node_type_for_socket_type(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return SOCK_FLOAT;
    case SOCK_BOOLEAN:
      return SOCK_BOOLEAN;
    case SOCK_INT:
      return SOCK_INT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
      return SOCK_VECTOR;
    default:
      return std::nullopt;
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  const StructureType structure_type = other_socket.runtime->inferred_structure_type;
  const eNodeSocketDatatype other_type = other_socket.type;

  if (params.in_out() == SOCK_IN) {
    if (ELEM(structure_type, StructureType::Grid, StructureType::Dynamic)) {
      const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
      if (data_type) {
        params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeGridDeactivateVoxels"_ustr);
          node.custom1 = *data_type;
          params.update_and_connect_available_socket(node, "Grid"_ustr);
        });
      }
    }
    if (params.node_tree().typeinfo->validate_link(other_type, SOCK_BOOLEAN)) {
      params.add_item(IFACE_("Selection"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridDeactivateVoxels"_ustr);
        params.connect_available_socket(node, "Selection"_ustr);
      });
    }
  }
  else {
    if (ELEM(structure_type, StructureType::Grid, StructureType::Dynamic)) {
      const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
      if (data_type) {
        params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeGridDeactivateVoxels"_ustr);
          node.custom1 = *data_type;
          params.update_and_connect_available_socket(node, "Grid"_ustr);
        });
      }
    }
  }
}

#ifdef WITH_OPENVDB

BLI_NOINLINE static void process_leaf_node(const fn::Field<bool> &selection_field,
                                           const openvdb::math::Transform &transform,
                                           const volume_grid::LeafNodeMask &leaf_node_mask,
                                           const openvdb::CoordBBox &leaf_bbox,
                                           const volume_grid::GetVoxelsFn get_voxels_fn,
                                           openvdb::GridBase &output_grid)
{
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);

  const IndexMask index_mask = IndexMask::from_predicate(
      IndexRange(volume_grid::LeafNodeMask::SIZE),
      scope.allocator(),
      [&](const int64_t i) { return leaf_node_mask.isOn(i); },
      exec_mode::serial);

  const openvdb::Coord any_voxel_in_leaf = leaf_bbox.min();
  MutableSpan<openvdb::Coord> voxels = scope.allocator().allocate_array<openvdb::Coord>(
      index_mask.min_array_size());
  get_voxels_fn(voxels);

  bke::VoxelFieldContext field_context{transform, voxels};
  fn::FieldEvaluator evaluator{field_context, &index_mask};

  MutableSpan<bool> selection = scope.allocator().allocate_array<bool>(
      index_mask.min_array_size());
  evaluator.add_with_destination(selection_field, selection);

  evaluator.evaluate();

  volume_grid::set_leaf_values_off(output_grid, any_voxel_in_leaf, selection);
}

BLI_NOINLINE static void process_voxels(const fn::Field<bool> &selection_field,
                                        const openvdb::math::Transform &transform,
                                        const Span<openvdb::Coord> voxels,
                                        openvdb::GridBase &output_grid)
{
  const int64_t voxels_num = voxels.size();
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);

  bke::VoxelFieldContext field_context{transform, voxels};
  fn::FieldEvaluator evaluator{field_context, voxels_num};

  MutableSpan<bool> selection = scope.allocator().allocate_array<bool>(voxels_num);
  evaluator.add_with_destination(selection_field, selection);
  evaluator.evaluate();

  volume_grid::set_grid_values_off(output_grid, selection, voxels);
}

BLI_NOINLINE static void process_tiles(const fn::Field<bool> &selection_field,
                                       const openvdb::math::Transform &transform,
                                       const Span<openvdb::CoordBBox> tiles,
                                       openvdb::GridBase &output_grid)
{
  const int64_t tiles_num = tiles.size();
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope(allocation_buffer);

  bke::TilesFieldContext field_context{transform, tiles};
  fn::FieldEvaluator evaluator{field_context, tiles_num};

  MutableSpan<bool> selection;
  selection = scope.allocator().allocate_array<bool>(tiles_num);
  evaluator.add_with_destination(selection_field, selection);
  evaluator.evaluate();

  volume_grid::set_tile_values_off(output_grid, selection, tiles);
}

#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid"_ustr);
  if (!grid) {
    params.error_message_add(NodeWarningType::Error, "The grid input is required");
    params.set_default_remaining_outputs();
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  openvdb::GridBase &grid_base = grid.get_for_write().grid_for_write(tree_token);

  fn::Field<bool> selection_field = params.extract_input<fn::Field<bool>>("Selection"_ustr);
  if (!selection_field.depends_on_input()) {
    if (fn::evaluate_constant_field(selection_field)) {
      /* Deactivate everything. */
      grid_base.clear();
    }
    /* If selection_field evaluates to false keep the grid unmodified. */
  }
  else {
    const openvdb::math::Transform &transform = grid_base.transform();
    openvdb::MaskTree mask_tree;
    volume_grid::to_typed_grid(grid_base,
                               [&](const auto &grid) { mask_tree.topologyUnion(grid.tree()); });

    volume_grid::parallel_grid_topology_tasks(
        mask_tree,
        [&](const volume_grid::LeafNodeMask &leaf_node_mask,
            const openvdb::CoordBBox &leaf_bbox,
            const volume_grid::GetVoxelsFn get_voxels_fn) {
          process_leaf_node(
              selection_field, transform, leaf_node_mask, leaf_bbox, get_voxels_fn, grid_base);
        },
        [&](const Span<openvdb::Coord> voxels) {
          process_voxels(selection_field, transform, voxels, grid_base);
        },
        [&](const Span<openvdb::CoordBBox> tiles) {
          process_tiles(selection_field, transform, tiles, grid_base);
        });
  }

  params.set_output("Grid"_ustr, std::move(grid));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Node socket data type",
                    rna_enum_node_socket_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT,
                    grid_socket_type_items_filter_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGridDeactivateVoxels"_ustr);
  ntype.ui_name = "Deactivate Voxels";
  ntype.ui_description = "Deactivate selected voxels and tiles";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_deactivate_voxels

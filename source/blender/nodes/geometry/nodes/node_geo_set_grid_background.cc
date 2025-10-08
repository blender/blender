/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_volume_grid_process.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

namespace blender::nodes::node_geo_set_grid_background {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input(data_type, "Grid")
      .hide_value()
      .structure_type(StructureType::Grid)
      .is_default_link_socket();
  b.add_output(data_type, "Grid").structure_type(StructureType::Grid).align_with_previous();
  b.add_input(data_type, "Background").structure_type(StructureType::Single);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
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
  const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(
      params.other_socket());
  if (!data_type) {
    return;
  }
  const bNodeSocket &other_socket = params.other_socket();
  const StructureType structure_type = other_socket.runtime->inferred_structure_type;
  const bool is_grid = structure_type == StructureType::Grid;
  const bool is_dynamic = structure_type == StructureType::Dynamic;

  if (params.in_out() == SOCK_IN) {
    if (is_grid || is_dynamic) {
      params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSetGridBackground");
        node.custom1 = *data_type;
        params.update_and_connect_available_socket(node, "Grid");
      });
    }
    if (!is_grid || is_dynamic) {
      params.add_item(IFACE_("Background"), [data_type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSetGridBackground");
        node.custom1 = *data_type;
        params.update_and_connect_available_socket(node, "Background");
      });
    }
  }
  else {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSetGridBackground");
      node.custom1 = *data_type;
      params.update_and_connect_available_socket(node, "Grid");
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const auto background = params.extract_input<bke::SocketValueVariant>("Background");

  bke::VolumeTreeAccessToken tree_token;
  openvdb::GridBase &grid_base = grid.get_for_write().grid_for_write(tree_token);
  bke::volume_grid::set_grid_background(grid_base, background.get_single_ptr());

  params.set_output("Grid", std::move(grid));
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

  geo_node_type_base(&ntype, "GeometryNodeSetGridBackground");
  ntype.ui_name = "Set Grid Background";
  ntype.ui_description = "Set the background value used for inactive voxels and tiles";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_grid_background

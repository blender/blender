/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_attribute_math.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

namespace blender::nodes::node_geo_set_grid_transform {

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
  b.add_output<decl::Bool>("Is Valid")
      .description("The new transform is valid and was successfully applied to the grid.");
  b.add_input(data_type, "Grid")
      .hide_value()
      .structure_type(StructureType::Grid)
      .is_default_link_socket();
  b.add_output(data_type, "Grid").structure_type(StructureType::Grid).align_with_previous();
  b.add_input<decl::Matrix>("Transform")
      .description("The new transform from grid index space to object space.");
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
  const bNodeSocket &other_socket = params.other_socket();
  const StructureType structure_type = other_socket.runtime->inferred_structure_type;
  const bool is_grid = structure_type == StructureType::Grid;
  const bool is_dynamic = structure_type == StructureType::Dynamic;
  const eNodeSocketDatatype other_type = eNodeSocketDatatype(other_socket.type);

  if (params.in_out() == SOCK_IN) {
    if (is_grid || is_dynamic) {
      const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
      if (data_type) {
        params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeSetGridTransform");
          node.custom1 = *data_type;
          params.update_and_connect_available_socket(node, "Grid");
        });
      }
    }
    if (!is_grid && params.node_tree().typeinfo->validate_link(other_type, SOCK_MATRIX)) {
      params.add_item(IFACE_("Transform"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSetGridTransform");
        params.update_and_connect_available_socket(node, "Transform");
      });
    }
  }
  else {
    const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
    if (data_type) {
      params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSetGridTransform");
        node.custom1 = *data_type;
        params.update_and_connect_available_socket(node, "Grid");
      });
    }
    if (params.node_tree().typeinfo->validate_link(SOCK_BOOLEAN, other_type)) {
      params.add_item(IFACE_("Is Valid"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSetGridTransform");
        params.update_and_connect_available_socket(node, "Is Valid");
      });
    }
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

  const float4x4 transform = params.extract_input<float4x4>("Transform");

  try {
    bke::VolumeGridData &grid_data = grid.get_for_write();
    bke::volume_grid::set_transform_matrix(grid_data, transform);
    params.set_output("Is Valid", true);
  }
  catch (const openvdb::ArithmeticError & /*error*/) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Failed to set the new grid transform."));
    params.set_output("Is Valid", false);
  }

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

  geo_node_type_base(&ntype, "GeometryNodeSetGridTransform");
  ntype.ui_name = "Set Grid Transform";
  ntype.ui_description = "Set the transform for the grid from index space into object space.";
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

}  // namespace blender::nodes::node_geo_set_grid_transform

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_attribute_math.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

namespace blender::nodes::node_geo_grid_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  b.add_input(data_type, "Grid").hide_value().structure_type(StructureType::Grid);

  b.add_output<decl::Matrix>("Transform")
      .description("Transform from grid index space to object space");
  b.add_output(data_type, "Background Value").description("Default value outside of grid voxels");
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
          bNode &node = params.add_node("GeometryNodeGridInfo");
          node.custom1 = *data_type;
          params.update_and_connect_available_socket(node, "Grid");
        });
      }
    }
  }
  else {
    if (params.node_tree().typeinfo->validate_link(SOCK_MATRIX, other_type)) {
      params.add_item(IFACE_("Transform"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridInfo");
        params.update_and_connect_available_socket(node, "Transform");
      });
    }
    const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
    if (data_type) {
      params.add_item(IFACE_("Background Value"), [data_type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridInfo");
        node.custom1 = *data_type;
        params.update_and_connect_available_socket(node, "Background Value");
      });
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(params.node().custom1);

  const auto grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  const std::shared_ptr<const openvdb::GridBase> vdb_grid = grid->grid_ptr(tree_token);
  params.set_output("Transform", BKE_volume_transform_to_blender(vdb_grid->transform()));

  bke::attribute_math::convert_to_static_type(
      *bke::socket_type_to_geo_nodes_base_cpp_type(data_type), [&](auto type_tag) {
        using ValueT = decltype(type_tag);
        using type_traits = typename bke::VolumeGridTraits<ValueT>;
        using TreeType = typename type_traits::TreeType;
        using GridType = openvdb::Grid<TreeType>;

        if constexpr (!std::is_same_v<typename type_traits::BlenderType, void>) {
          const std::shared_ptr<const GridType> vdb_typed_grid = openvdb::GridBase::grid<GridType>(
              vdb_grid);
          params.set_output("Background Value",
                            type_traits::to_blender(vdb_typed_grid->background()));
        }
      });
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

  geo_node_type_base(&ntype, "GeometryNodeGridInfo");
  ntype.ui_name = "Grid Info";
  ntype.ui_description = "Retrieve information about a volume grid";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_info_cc

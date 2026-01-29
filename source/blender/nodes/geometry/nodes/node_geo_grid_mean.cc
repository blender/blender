/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_process.hh"
#include "BKE_volume_openvdb.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/Filter.h"
#endif

namespace blender::nodes::node_geo_grid_mean_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
  b.add_input(data_type, "Grid").hide_value().structure_type(StructureType::Grid);
  b.add_output(data_type, "Grid").structure_type(StructureType::Grid).align_with_previous();

  b.add_input<decl::Int>("Width")
      .default_value(1)
      .min(0)
      .max(10)
      .structure_type(StructureType::Single)
      .description("Filter kernel radius in voxels");

  b.add_input<decl::Int>("Iterations")
      .default_value(1)
      .min(0)
      .max(100)
      .structure_type(StructureType::Single)
      .description("Number of iterations to apply the filter");
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static std::optional<eNodeSocketDatatype> node_type_for_socket_type(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return SOCK_FLOAT;
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

  const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
  if (!data_type) {
    return;
  }

  if (params.in_out() == SOCK_IN && (is_grid || is_dynamic)) {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeGridMean");
      node.custom1 = *data_type;
      params.update_and_connect_available_socket(node, "Grid");
    });
  }
  else if (params.in_out() == SOCK_OUT) {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeGridMean");
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

  const int width = params.extract_input<int>("Width");
  const int iterations = params.extract_input<int>("Iterations");
  if (width <= 0 || iterations <= 0) {
    params.set_output("Grid", std::move(grid));
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  openvdb::GridBase &grid_base = grid.get_for_write().grid_for_write(tree_token);

  bke::volume_grid::to_typed_grid(grid_base, [&](auto &typed_grid) {
    openvdb::tools::Filter filter(typed_grid);
    filter.mean(width, iterations);
  });

  params.set_output("Grid", std::move(grid));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static const EnumPropertyItem *grid_mean_socket_type_items_filter_fn(bContext * /*C*/,
                                                                     PointerRNA * /*ptr*/,
                                                                     PropertyRNA * /*prop*/,
                                                                     bool *r_free)
{
  *r_free = true;
  return enum_items_filter(rna_enum_node_socket_data_type_items,
                           [](const EnumPropertyItem &item) -> bool {
                             return socket_type_supports_grids(eNodeSocketDatatype(item.value)) &&
                                    item.value != SOCK_BOOLEAN;
                           });
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
                    grid_mean_socket_type_items_filter_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGridMean");
  ntype.ui_name = "Grid Mean";
  ntype.ui_description =
      "Apply mean (box) filter smoothing to a voxel. The mean value from surrounding voxels in a "
      "box-shape defined by the radius replaces the voxel value.";
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

}  // namespace blender::nodes::node_geo_grid_mean_cc

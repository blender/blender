/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_process.hh"
#include "BKE_volume_openvdb.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/Clip.h>
#endif

namespace blender::nodes::node_geo_grid_clip_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();

  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
  b.add_input(data_type, "Grid").hide_value().structure_type(StructureType::Grid);
  b.add_output(data_type, "Grid").structure_type(StructureType::Grid).align_with_previous();

  b.add_input<decl::Int>("Min X")
      .default_value(0)
      .structure_type(StructureType::Single)
      .description("Minimum X index of the clipping bounding box");
  b.add_input<decl::Int>("Min Y")
      .default_value(0)
      .structure_type(StructureType::Single)
      .description("Minimum Y index of the clipping bounding box");
  b.add_input<decl::Int>("Min Z")
      .default_value(0)
      .structure_type(StructureType::Single)
      .description("Minimum Z index of the clipping bounding box");
  b.add_input<decl::Int>("Max X")
      .default_value(32)
      .structure_type(StructureType::Single)
      .description("Maximum X index of the clipping bounding box");
  b.add_input<decl::Int>("Max Y")
      .default_value(32)
      .structure_type(StructureType::Single)
      .description("Maximum Y index of the clipping bounding box");
  b.add_input<decl::Int>("Max Z")
      .default_value(32)
      .structure_type(StructureType::Single)
      .description("Maximum Z index of the clipping bounding box");
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
  params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
    bNode &node = params.add_node("GeometryNodeGridClip");
    node.custom1 = *data_type;
    params.update_and_connect_available_socket(node, "Grid");
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const int3 min_index = int3(params.extract_input<int>("Min X"),
                              params.extract_input<int>("Min Y"),
                              params.extract_input<int>("Min Z"));
  const int3 max_index = int3(params.extract_input<int>("Max X"),
                              params.extract_input<int>("Max Y"),
                              params.extract_input<int>("Max Z"));

  bke::VolumeTreeAccessToken tree_token;
  openvdb::GridBase &grid_base = grid.get_for_write().grid_for_write(tree_token);

  const openvdb::CoordBBox coord_bbox(openvdb::Coord(min_index.x, min_index.y, min_index.z),
                                      openvdb::Coord(max_index.x, max_index.y, max_index.z));

  bke::volume_grid::to_typed_grid(grid_base, [&](auto &typed_grid) {
    auto active_bbox = typed_grid.evalActiveVoxelBoundingBox();
    if (active_bbox.empty()) {
      return;
    }
    active_bbox.intersect(coord_bbox);
    typed_grid.clip(active_bbox);
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
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGridClip");
  ntype.ui_name = "Clip Grid";
  ntype.ui_description =
      "Deactivate grid voxels outside minimum and maximum coordinates, setting them to the "
      "background value.";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_clip_cc

/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_process.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/Morphology.h"
#endif

namespace blender::nodes::node_geo_grid_dilate_erode_cc {

enum class Connectivity : int8_t {
  Face = 0,
  FaceEdge = 1,
  FaceEdgeVertex = 2,
};

enum class TilePolicy : int8_t {
  Ignore = 0,
  Expand = 1,
  Preserve = 2,
};

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

  static EnumPropertyItem connectivity_items[] = {
      {int(Connectivity::Face),
       "FACE",
       0,
       "Face",
       "6-connectivity: affect voxels connected by faces only"},
      {int(Connectivity::FaceEdge),
       "FACE_EDGE",
       0,
       "Edge",
       "18-connectivity: affect voxels connected by faces or edges only"},
      {int(Connectivity::FaceEdgeVertex),
       "FACE_EDGE_VERTEX",
       0,
       "Vertex",
       "26-connectivity: affect voxels connected by faces, edges, or vertices"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem tile_policy_items[] = {
      {int(TilePolicy::Ignore),
       "IGNORE",
       0,
       "Ignore",
       "Ignore active tiles; they are neither dilated/eroded nor contribute to the operation"},
      {int(TilePolicy::Expand),
       "EXPAND",
       0,
       "Expand",
       "Voxelize active tiles, apply operation, and leave in voxelized state"},
      {int(TilePolicy::Preserve),
       "PRESERVE",
       0,
       "Preserve",
       "Keep tiles unchanged when possible, only voxelizing if necessary. More memory efficient"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  b.add_input<decl::Menu>("Connectivity")
      .static_items(connectivity_items)
      .default_value(MenuValue(Connectivity::Face))
      .structure_type(StructureType::Single)
      .optional_label();

  b.add_input<decl::Menu>("Tiles")
      .static_items(tile_policy_items)
      .default_value(MenuValue(TilePolicy::Preserve))
      .structure_type(StructureType::Single)
      .optional_label();

  b.add_input<decl::Int>("Steps")
      .default_value(1)
      .min(-100)
      .max(100)
      .structure_type(StructureType::Single)
      .description("Number of times to dilate or erode the active voxels");
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
  const bNodeSocket &other_socket = params.other_socket();
  const StructureType structure_type = other_socket.runtime->inferred_structure_type;
  const bool supports_grid = ELEM(structure_type, StructureType::Grid, StructureType::Dynamic);

  const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
  if (!data_type) {
    return;
  }

  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                   data_type.value()))
    {
      params.add_item(IFACE_("Grid"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridDilateAndErode");
        params.update_and_connect_available_socket(node, "Steps");
      });
    }
  }
  else if (params.in_out() == SOCK_IN && supports_grid) {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeGridDilateAndErode");
      node.custom1 = *data_type;
      params.update_and_connect_available_socket(node, "Grid");
    });
  }
  else if (params.in_out() == SOCK_OUT) {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeGridDilateAndErode");
      node.custom1 = *data_type;
      params.update_and_connect_available_socket(node, "Grid");
    });
  }
}

#ifdef WITH_OPENVDB
static openvdb::tools::NearestNeighbors connectivity_to_openvdb(const Connectivity connectivity)
{
  switch (connectivity) {
    case Connectivity::Face:
      return openvdb::tools::NN_FACE;
    case Connectivity::FaceEdge:
      return openvdb::tools::NN_FACE_EDGE;
    case Connectivity::FaceEdgeVertex:
      return openvdb::tools::NN_FACE_EDGE_VERTEX;
  }
  BLI_assert_unreachable();
  return openvdb::tools::NN_FACE;
}

static openvdb::tools::TilePolicy tile_policy_to_openvdb(const TilePolicy policy)
{
  switch (policy) {
    case TilePolicy::Ignore:
      return openvdb::tools::IGNORE_TILES;
    case TilePolicy::Expand:
      return openvdb::tools::EXPAND_TILES;
    case TilePolicy::Preserve:
      return openvdb::tools::PRESERVE_TILES;
  }
  BLI_assert_unreachable();
  return openvdb::tools::PRESERVE_TILES;
}
#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const Connectivity connectivity = params.extract_input<Connectivity>("Connectivity");
  const TilePolicy tile_policy = params.extract_input<TilePolicy>("Tiles");
  const int steps = params.extract_input<int>("Steps");
  if (steps == 0) {
    params.set_output("Grid", std::move(grid));
    return;
  }

  const openvdb::tools::NearestNeighbors nn = connectivity_to_openvdb(connectivity);
  const openvdb::tools::TilePolicy policy = tile_policy_to_openvdb(tile_policy);

  bke::VolumeTreeAccessToken tree_token;
  openvdb::GridBase &grid_base = grid.get_for_write().grid_for_write(tree_token);
  bke::volume_grid::to_typed_grid(grid_base, [&](auto &grid) {
    if (steps > 0) {
      openvdb::tools::dilateActiveValues(grid.tree(), steps, nn, policy);
    }
    else {
      openvdb::tools::erodeActiveValues(grid.tree(), -steps, nn, policy);
    }
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
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGridDilateAndErode");
  ntype.ui_name = "Grid Dilate & Erode";
  ntype.ui_description =
      "Dilate or erode the active regions of a grid. This changes which voxels are "
      "active but does not change their values.";
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

}  // namespace blender::nodes::node_geo_grid_dilate_erode_cc

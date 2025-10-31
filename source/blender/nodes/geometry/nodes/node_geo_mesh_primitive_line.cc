/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "BKE_material.hh"

#include "NOD_rna_define.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_mesh_primitive_line.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_line_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshLine)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Count").default_value(10).min(1).max(10000).description(
      "Number of vertices on the line");
  b.add_input<decl::Float>("Resolution")
      .default_value(1.0f)
      .min(0.1f)
      .subtype(PROP_DISTANCE)
      .description("Length of each individual edge");
  b.add_input<decl::Vector>("Start Location")
      .subtype(PROP_TRANSLATION)
      .description("Position of the first vertex");
  b.add_input<decl::Vector>("Offset")
      .default_value({0.0f, 0.0f, 1.0f})
      .subtype(PROP_TRANSLATION)
      .description(
          "In offset mode, the distance between each socket on each axis. In end points mode, the "
          "position of the final vertex")
      .label_fn([](bNode node) {
        return (node_storage(node).mode == GEO_NODE_MESH_LINE_MODE_END_POINTS) ?
                   IFACE_("End Location") :
                   IFACE_("Offset");
      });
  ;
  b.add_output<decl::Geometry>("Mesh");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
  if (RNA_enum_get(ptr, "mode") == GEO_NODE_MESH_LINE_MODE_END_POINTS) {
    layout->prop(ptr, "count_mode", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryMeshLine *node_storage = MEM_callocN<NodeGeometryMeshLine>(__func__);

  node_storage->mode = GEO_NODE_MESH_LINE_MODE_OFFSET;
  node_storage->count_mode = GEO_NODE_MESH_LINE_COUNT_TOTAL;

  node->storage = node_storage;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *count_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *resolution_socket = count_socket->next;

  const NodeGeometryMeshLine &storage = node_storage(*node);
  const GeometryNodeMeshLineMode mode = (GeometryNodeMeshLineMode)storage.mode;
  const GeometryNodeMeshLineCountMode count_mode = (GeometryNodeMeshLineCountMode)
                                                       storage.count_mode;

  bke::node_set_socket_availability(*ntree,
                                    *resolution_socket,
                                    mode == GEO_NODE_MESH_LINE_MODE_END_POINTS &&
                                        count_mode == GEO_NODE_MESH_LINE_COUNT_RESOLUTION);
  bke::node_set_socket_availability(*ntree,
                                    *count_socket,
                                    mode == GEO_NODE_MESH_LINE_MODE_OFFSET ||
                                        count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  if (params.in_out() == SOCK_OUT) {
    search_link_ops_for_declarations(params, declaration.outputs);
    return;
  }
  if (params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                 SOCK_FLOAT))
  {
    params.add_item(IFACE_("Count"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeMeshLine");
      node_storage(node).mode = GEO_NODE_MESH_LINE_MODE_OFFSET;
      params.connect_available_socket(node, "Count");
    });
    params.add_item(IFACE_("Resolution"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeMeshLine");
      node_storage(node).mode = GEO_NODE_MESH_LINE_MODE_OFFSET;
      node_storage(node).count_mode = GEO_NODE_MESH_LINE_COUNT_RESOLUTION;
      params.connect_available_socket(node, "Resolution");
    });
    params.add_item(IFACE_("Start Location"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeMeshLine");
      params.connect_available_socket(node, "Start Location");
    });
    params.add_item(IFACE_("Offset"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeMeshLine");
      params.connect_available_socket(node, "Offset");
    });
    /* The last socket is reused in end points mode. */
    params.add_item(IFACE_("End Location"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeMeshLine");
      node_storage(node).mode = GEO_NODE_MESH_LINE_MODE_END_POINTS;
      params.connect_available_socket(node, "Offset");
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryMeshLine &storage = node_storage(params.node());
  const GeometryNodeMeshLineMode mode = (GeometryNodeMeshLineMode)storage.mode;
  const GeometryNodeMeshLineCountMode count_mode = (GeometryNodeMeshLineCountMode)
                                                       storage.count_mode;

  Mesh *mesh = nullptr;
  const float3 start = params.extract_input<float3>("Start Location");
  if (mode == GEO_NODE_MESH_LINE_MODE_END_POINTS) {
    /* The label switches to "End Location", but the same socket is used. */
    const float3 end = params.extract_input<float3>("Offset");
    const float3 total_delta = end - start;

    if (count_mode == GEO_NODE_MESH_LINE_COUNT_RESOLUTION) {
      /* Don't allow asymptotic count increase for low resolution values. */
      const float resolution = std::max(params.extract_input<float>("Resolution"), 0.0001f);
      const int count = math::length(total_delta) / resolution + 1;
      const float3 delta = math::normalize(total_delta) * resolution;
      mesh = geometry::create_line_mesh(start, delta, count);
    }
    else if (count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL) {
      const int count = params.extract_input<int>("Count");
      if (count == 1) {
        mesh = geometry::create_line_mesh(start, float3(0), count);
      }
      else {
        const float3 delta = total_delta / float(count - 1);
        mesh = geometry::create_line_mesh(start, delta, count);
      }
    }
  }
  else if (mode == GEO_NODE_MESH_LINE_MODE_OFFSET) {
    const float3 delta = params.extract_input<float3>("Offset");
    const int count = params.extract_input<int>("Count");
    mesh = geometry::create_line_mesh(start, delta, count);
  }

  if (mesh) {
    BKE_id_material_eval_ensure_default_slot(reinterpret_cast<ID *>(mesh));
  }

  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
}

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem mode_items[] = {
      {GEO_NODE_MESH_LINE_MODE_OFFSET,
       "OFFSET",
       0,
       "Offset",
       "Specify the offset from one vertex to the next"},
      {GEO_NODE_MESH_LINE_MODE_END_POINTS,
       "END_POINTS",
       0,
       "End Points",
       "Specify the line's start and end points"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem count_mode_items[] = {
      {GEO_NODE_MESH_LINE_COUNT_TOTAL,
       "TOTAL",
       0,
       "Count",
       "Specify the total number of vertices"},
      {GEO_NODE_MESH_LINE_COUNT_RESOLUTION,
       "RESOLUTION",
       0,
       "Resolution",
       "Specify the distance between vertices"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "",
                    mode_items,
                    NOD_storage_enum_accessors(mode),
                    GEO_NODE_MESH_LINE_MODE_OFFSET);

  RNA_def_node_enum(srna,
                    "count_mode",
                    "Count Mode",
                    "",
                    count_mode_items,
                    NOD_storage_enum_accessors(count_mode),
                    GEO_NODE_MESH_LINE_COUNT_TOTAL);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshLine", GEO_NODE_MESH_PRIMITIVE_LINE);
  ntype.ui_name = "Mesh Line";
  ntype.ui_description = "Generate vertices in a line and connect them with edges";
  ntype.enum_name_legacy = "MESH_PRIMITIVE_LINE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryMeshLine", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.updatefunc = node_update;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_primitive_line_cc

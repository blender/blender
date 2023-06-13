/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "BKE_material.h"
#include "BKE_mesh.hh"

#include "UI_interface.h"
#include "UI_resources.h"

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
          "position of the final vertex");
  b.add_output<decl::Geometry>("Mesh");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
  if (RNA_enum_get(ptr, "mode") == GEO_NODE_MESH_LINE_MODE_END_POINTS) {
    uiItemR(layout, ptr, "count_mode", 0, "", ICON_NONE);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryMeshLine *node_storage = MEM_cnew<NodeGeometryMeshLine>(__func__);

  node_storage->mode = GEO_NODE_MESH_LINE_MODE_OFFSET;
  node_storage->count_mode = GEO_NODE_MESH_LINE_COUNT_TOTAL;

  node->storage = node_storage;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *count_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *resolution_socket = count_socket->next;
  bNodeSocket *start_socket = resolution_socket->next;
  bNodeSocket *end_and_offset_socket = start_socket->next;

  const NodeGeometryMeshLine &storage = node_storage(*node);
  const GeometryNodeMeshLineMode mode = (GeometryNodeMeshLineMode)storage.mode;
  const GeometryNodeMeshLineCountMode count_mode = (GeometryNodeMeshLineCountMode)
                                                       storage.count_mode;

  node_sock_label(end_and_offset_socket,
                  (mode == GEO_NODE_MESH_LINE_MODE_END_POINTS) ? N_("End Location") :
                                                                 N_("Offset"));

  bke::nodeSetSocketAvailability(ntree,
                                 resolution_socket,
                                 mode == GEO_NODE_MESH_LINE_MODE_END_POINTS &&
                                     count_mode == GEO_NODE_MESH_LINE_COUNT_RESOLUTION);
  bke::nodeSetSocketAvailability(ntree,
                                 count_socket,
                                 mode == GEO_NODE_MESH_LINE_MODE_OFFSET ||
                                     count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  if (params.in_out() == SOCK_OUT) {
    search_link_ops_for_declarations(params, declaration.outputs);
    return;
  }
  else if (params.node_tree().typeinfo->validate_link(
               eNodeSocketDatatype(params.other_socket().type), SOCK_FLOAT))
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
      mesh = create_line_mesh(start, delta, count);
    }
    else if (count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL) {
      const int count = params.extract_input<int>("Count");
      if (count == 1) {
        mesh = create_line_mesh(start, float3(0), count);
      }
      else {
        const float3 delta = total_delta / float(count - 1);
        mesh = create_line_mesh(start, delta, count);
      }
    }
  }
  else if (mode == GEO_NODE_MESH_LINE_MODE_OFFSET) {
    const float3 delta = params.extract_input<float3>("Offset");
    const int count = params.extract_input<int>("Count");
    mesh = create_line_mesh(start, delta, count);
  }

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes::node_geo_mesh_primitive_line_cc

namespace blender::nodes {

Mesh *create_line_mesh(const float3 start, const float3 delta, const int count)
{
  if (count < 1) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(count, count - 1, 0, 0);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int2> edges = mesh->edges_for_write();

  threading::parallel_invoke(
      1024 < count,
      [&]() {
        threading::parallel_for(positions.index_range(), 4096, [&](IndexRange range) {
          for (const int i : range) {
            positions[i] = start + delta * i;
          }
        });
      },
      [&]() {
        threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
          for (const int i : range) {
            edges[i][0] = i;
            edges[i][1] = i + 1;
          }
        });
      });

  return mesh;
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_line()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_line_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_LINE, "Mesh Line", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  node_type_storage(
      &ntype, "NodeGeometryMeshLine", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}

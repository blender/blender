/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"
#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_line_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshLine)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Count"))
      .default_value(10)
      .min(1)
      .max(10000)
      .description(N_("Number of vertices on the line"));
  b.add_input<decl::Float>(N_("Resolution"))
      .default_value(1.0f)
      .min(0.1f)
      .subtype(PROP_DISTANCE)
      .description(N_("Length of each individual edge"));
  b.add_input<decl::Vector>(N_("Start Location"))
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the first vertex"));
  b.add_input<decl::Vector>(N_("Offset"))
      .default_value({0.0f, 0.0f, 1.0f})
      .subtype(PROP_TRANSLATION)
      .description(N_(
          "In offset mode, the distance between each socket on each axis. In end points mode, the "
          "position of the final vertex"));
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
  if (RNA_enum_get(ptr, "mode") == GEO_NODE_MESH_LINE_MODE_END_POINTS) {
    uiItemR(layout, ptr, "count_mode", 0, "", ICON_NONE);
  }
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshLine *node_storage = MEM_cnew<NodeGeometryMeshLine>(__func__);

  node_storage->mode = GEO_NODE_MESH_LINE_MODE_OFFSET;
  node_storage->count_mode = GEO_NODE_MESH_LINE_COUNT_TOTAL;

  node->storage = node_storage;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *count_socket = (bNodeSocket *)node->inputs.first;
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

  nodeSetSocketAvailability(ntree,
                            resolution_socket,
                            mode == GEO_NODE_MESH_LINE_MODE_END_POINTS &&
                                count_mode == GEO_NODE_MESH_LINE_COUNT_RESOLUTION);
  nodeSetSocketAvailability(ntree,
                            count_socket,
                            mode == GEO_NODE_MESH_LINE_MODE_OFFSET ||
                                count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  if (params.in_out() == SOCK_OUT) {
    search_link_ops_for_declarations(params, declaration.outputs());
    return;
  }
  else if (params.node_tree().typeinfo->validate_link(
               static_cast<eNodeSocketDatatype>(params.other_socket().type), SOCK_FLOAT)) {
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
        const float3 delta = total_delta / (float)(count - 1);
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

static void fill_edge_data(MutableSpan<MEdge> edges)
{
  for (const int i : edges.index_range()) {
    edges[i].v1 = i;
    edges[i].v2 = i + 1;
    edges[i].flag |= ME_LOOSEEDGE;
  }
}

Mesh *create_line_mesh(const float3 start, const float3 delta, const int count)
{
  if (count < 1) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(count, count - 1, 0, 0, 0);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};

  for (const int i : verts.index_range()) {
    copy_v3_v3(verts[i].co, start + delta * i);
  }

  fill_edge_data(edges);

  return mesh;
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_line()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_line_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_LINE, "Mesh Line", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_storage(
      &ntype, "NodeGeometryMeshLine", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}

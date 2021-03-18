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

#include "BLI_map.hh"
#include "BLI_math_matrix.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_mesh_primitive_line_in[] = {
    {SOCK_INT, N_("Count"), 10, 0.0f, 0.0f, 0.0f, 1, 10000},
    {SOCK_FLOAT, N_("Resolution"), 1.0f, 0.0f, 0.0f, 0.0f, 0.01f, FLT_MAX, PROP_DISTANCE},
    {SOCK_VECTOR,
     N_("Start Location"),
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     -FLT_MAX,
     FLT_MAX,
     PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Offset"), 0.0f, 0.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_line_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_mesh_primitive_line_layout(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
  if (RNA_enum_get(ptr, "mode") == GEO_NODE_MESH_LINE_MODE_END_POINTS) {
    uiItemR(layout, ptr, "count_mode", 0, "", ICON_NONE);
  }
}

static void geo_node_mesh_primitive_line_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshLine *node_storage = (NodeGeometryMeshLine *)MEM_callocN(
      sizeof(NodeGeometryMeshLine), __func__);

  node_storage->mode = GEO_NODE_MESH_LINE_MODE_OFFSET;
  node_storage->count_mode = GEO_NODE_MESH_LINE_COUNT_TOTAL;

  node->storage = node_storage;
}

static void geo_node_mesh_primitive_line_update(bNodeTree *UNUSED(tree), bNode *node)
{
  bNodeSocket *count_socket = (bNodeSocket *)node->inputs.first;
  bNodeSocket *resolution_socket = count_socket->next;
  bNodeSocket *start_socket = resolution_socket->next;
  bNodeSocket *end_and_offset_socket = start_socket->next;

  const NodeGeometryMeshLine &storage = *(const NodeGeometryMeshLine *)node->storage;
  const GeometryNodeMeshLineMode mode = (const GeometryNodeMeshLineMode)storage.mode;
  const GeometryNodeMeshLineCountMode count_mode = (const GeometryNodeMeshLineCountMode)
                                                       storage.count_mode;

  node_sock_label(end_and_offset_socket,
                  (mode == GEO_NODE_MESH_LINE_MODE_END_POINTS) ? N_("End Location") :
                                                                 N_("Offset"));

  nodeSetSocketAvailability(resolution_socket,
                            mode == GEO_NODE_MESH_LINE_MODE_END_POINTS &&
                                count_mode == GEO_NODE_MESH_LINE_COUNT_RESOLUTION);
  nodeSetSocketAvailability(count_socket,
                            mode == GEO_NODE_MESH_LINE_MODE_OFFSET ||
                                count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL);
}

namespace blender::nodes {

static void fill_edge_data(MutableSpan<MEdge> edges)
{
  for (const int i : edges.index_range()) {
    edges[i].v1 = i;
    edges[i].v2 = i + 1;
    edges[i].flag |= ME_LOOSEEDGE;
  }
}

static Mesh *create_line_mesh(const float3 start, const float3 delta, const int count)
{
  if (count < 1) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(count, count - 1, 0, 0, 0);
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};

  short normal[3];
  normal_float_to_short_v3(normal, delta.normalized());

  float3 co = start;
  for (const int i : verts.index_range()) {
    copy_v3_v3(verts[i].co, co);
    copy_v3_v3_short(verts[i].no, normal);
    co += delta;
  }

  fill_edge_data(edges);

  return mesh;
}

static void geo_node_mesh_primitive_line_exec(GeoNodeExecParams params)
{
  const NodeGeometryMeshLine &storage = *(const NodeGeometryMeshLine *)params.node().storage;
  const GeometryNodeMeshLineMode mode = (const GeometryNodeMeshLineMode)storage.mode;
  const GeometryNodeMeshLineCountMode count_mode = (const GeometryNodeMeshLineCountMode)
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
      const int count = total_delta.length() / resolution + 1;
      const float3 delta = total_delta.normalized() * resolution;
      mesh = create_line_mesh(start, delta, count);
    }
    else if (count_mode == GEO_NODE_MESH_LINE_COUNT_TOTAL) {
      const int count = params.extract_input<int>("Count");
      if (count > 1) {
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

  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_line()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_LINE, "Line", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_line_in, geo_node_mesh_primitive_line_out);
  node_type_init(&ntype, geo_node_mesh_primitive_line_init);
  node_type_update(&ntype, geo_node_mesh_primitive_line_update);
  node_type_storage(
      &ntype, "NodeGeometryMeshLine", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_line_exec;
  ntype.draw_buttons = geo_node_mesh_primitive_line_layout;
  nodeRegisterType(&ntype);
}

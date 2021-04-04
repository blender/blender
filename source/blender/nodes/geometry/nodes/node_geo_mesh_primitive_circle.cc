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

#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_mesh_primitive_circle_in[] = {
    {SOCK_INT, N_("Vertices"), 32, 0.0f, 0.0f, 0.0f, 3, 4096},
    {SOCK_FLOAT, N_("Radius"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_circle_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_mesh_primitive_circle_layout(uiLayout *layout,
                                                  bContext *UNUSED(C),
                                                  PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

static void geo_node_mesh_primitive_circle_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshCircle *node_storage = (NodeGeometryMeshCircle *)MEM_callocN(
      sizeof(NodeGeometryMeshCircle), __func__);

  node_storage->fill_type = GEO_NODE_MESH_CIRCLE_FILL_NONE;

  node->storage = node_storage;
}

namespace blender::nodes {

static int circle_vert_total(const GeometryNodeMeshCircleFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_CIRCLE_FILL_NONE:
    case GEO_NODE_MESH_CIRCLE_FILL_NGON:
      return verts_num;
    case GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN:
      return verts_num + 1;
  }
  BLI_assert_unreachable();
  return 0;
}

static int circle_edge_total(const GeometryNodeMeshCircleFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_CIRCLE_FILL_NONE:
    case GEO_NODE_MESH_CIRCLE_FILL_NGON:
      return verts_num;
    case GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN:
      return verts_num * 2;
  }
  BLI_assert_unreachable();
  return 0;
}

static int circle_corner_total(const GeometryNodeMeshCircleFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_CIRCLE_FILL_NONE:
      return 0;
    case GEO_NODE_MESH_CIRCLE_FILL_NGON:
      return verts_num;
    case GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN:
      return verts_num * 3;
  }
  BLI_assert_unreachable();
  return 0;
}

static int circle_face_total(const GeometryNodeMeshCircleFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_CIRCLE_FILL_NONE:
      return 0;
    case GEO_NODE_MESH_CIRCLE_FILL_NGON:
      return 1;
    case GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN:
      return verts_num;
  }
  BLI_assert_unreachable();
  return 0;
}

static Mesh *create_circle_mesh(const float radius,
                                const int verts_num,
                                const GeometryNodeMeshCircleFillType fill_type)
{
  Mesh *mesh = BKE_mesh_new_nomain(circle_vert_total(fill_type, verts_num),
                                   circle_edge_total(fill_type, verts_num),
                                   0,
                                   circle_corner_total(fill_type, verts_num),
                                   circle_face_total(fill_type, verts_num));
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  float angle = 0.0f;
  const float angle_delta = 2.0f * M_PI / static_cast<float>(verts_num);
  for (MVert &vert : verts) {
    copy_v3_v3(vert.co, float3(std::cos(angle) * radius, std::sin(angle) * radius, 0.0f));
    angle += angle_delta;
  }
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    copy_v3_v3(verts.last().co, float3(0));
  }

  /* Point all vertex normals in the up direction. */
  const short up_normal[3] = {0, 0, SHRT_MAX};
  for (MVert &vert : verts) {
    copy_v3_v3_short(vert.no, up_normal);
  }

  /* Create outer edges. */
  for (const int i : IndexRange(verts_num)) {
    MEdge &edge = edges[i];
    edge.v1 = i;
    edge.v2 = (i + 1) % verts_num;
  }

  /* Set loose edge flags. */
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NONE) {
    for (const int i : IndexRange(verts_num)) {
      MEdge &edge = edges[i];
      edge.flag |= ME_LOOSEEDGE;
    }
  }

  /* Create triangle fan edges. */
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    for (const int i : IndexRange(verts_num)) {
      MEdge &edge = edges[verts_num + i];
      edge.v1 = verts_num;
      edge.v2 = i;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Create corners and faces. */
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    MPoly &poly = polys[0];
    poly.loopstart = 0;
    poly.totloop = loops.size();

    for (const int i : IndexRange(verts_num)) {
      MLoop &loop = loops[i];
      loop.e = i;
      loop.v = i;
    }
  }
  else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    for (const int i : IndexRange(verts_num)) {
      MPoly &poly = polys[i];
      poly.loopstart = 3 * i;
      poly.totloop = 3;

      MLoop &loop_a = loops[3 * i];
      loop_a.e = i;
      loop_a.v = i;
      MLoop &loop_b = loops[3 * i + 1];
      loop_b.e = verts_num + ((i + 1) % verts_num);
      loop_b.v = (i + 1) % verts_num;
      MLoop &loop_c = loops[3 * i + 2];
      loop_c.e = verts_num + i;
      loop_c.v = verts_num;
    }
  }

  return mesh;
}

static void geo_node_mesh_primitive_circle_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const NodeGeometryMeshCircle &storage = *(const NodeGeometryMeshCircle *)node.storage;

  const GeometryNodeMeshCircleFillType fill_type = (const GeometryNodeMeshCircleFillType)
                                                       storage.fill_type;

  const float radius = params.extract_input<float>("Radius");
  const int verts_num = params.extract_input<int>("Vertices");
  if (verts_num < 3) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  Mesh *mesh = create_circle_mesh(radius, verts_num, fill_type);

  BLI_assert(BKE_mesh_is_valid(mesh));

  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_circle()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CIRCLE, "Circle", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_circle_in, geo_node_mesh_primitive_circle_out);
  node_type_init(&ntype, geo_node_mesh_primitive_circle_init);
  node_type_storage(
      &ntype, "NodeGeometryMeshCircle", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_circle_exec;
  ntype.draw_buttons = geo_node_mesh_primitive_circle_layout;
  nodeRegisterType(&ntype);
}

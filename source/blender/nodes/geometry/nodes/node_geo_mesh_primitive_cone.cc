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

static bNodeSocketTemplate geo_node_mesh_primitive_cone_in[] = {
    {SOCK_INT, N_("Vertices"), 32, 0.0f, 0.0f, 0.0f, 3, 4096},
    {SOCK_FLOAT, N_("Radius Top"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("Radius Bottom"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("Depth"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_cone_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_mesh_primitive_cone_layout(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

static void geo_node_mesh_primitive_cone_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshCone *node_storage = (NodeGeometryMeshCone *)MEM_callocN(
      sizeof(NodeGeometryMeshCone), __func__);

  node_storage->fill_type = GEO_NODE_MESH_CIRCLE_FILL_NGON;

  node->storage = node_storage;
}

namespace blender::nodes {

static int vert_total(const GeometryNodeMeshCircleFillType fill_type,
                      const int verts_num,
                      const bool top_is_point,
                      const bool bottom_is_point)
{
  int vert_total = 0;
  if (!top_is_point) {
    vert_total += verts_num;
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      vert_total++;
    }
  }
  else {
    vert_total++;
  }
  if (!bottom_is_point) {
    vert_total += verts_num;
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      vert_total++;
    }
  }
  else {
    vert_total++;
  }

  return vert_total;
}

static int edge_total(const GeometryNodeMeshCircleFillType fill_type,
                      const int verts_num,
                      const bool top_is_point,
                      const bool bottom_is_point)
{
  if (top_is_point && bottom_is_point) {
    return 1;
  }

  int edge_total = 0;
  if (!top_is_point) {
    edge_total += verts_num;
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      edge_total += verts_num;
    }
  }

  edge_total += verts_num;

  if (!bottom_is_point) {
    edge_total += verts_num;
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      edge_total += verts_num;
    }
  }

  return edge_total;
}

static int corner_total(const GeometryNodeMeshCircleFillType fill_type,
                        const int verts_num,
                        const bool top_is_point,
                        const bool bottom_is_point)
{
  if (top_is_point && bottom_is_point) {
    return 0;
  }

  int corner_total = 0;
  if (!top_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      corner_total += verts_num;
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      corner_total += verts_num * 3;
    }
  }

  if (!top_is_point && !bottom_is_point) {
    corner_total += verts_num * 4;
  }
  else {
    corner_total += verts_num * 3;
  }

  if (!bottom_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      corner_total += verts_num;
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      corner_total += verts_num * 3;
    }
  }

  return corner_total;
}

static int face_total(const GeometryNodeMeshCircleFillType fill_type,
                      const int verts_num,
                      const bool top_is_point,
                      const bool bottom_is_point)
{
  if (top_is_point && bottom_is_point) {
    return 0;
  }

  int face_total = 0;
  if (!top_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      face_total++;
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      face_total += verts_num;
    }
  }

  face_total += verts_num;

  if (!bottom_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      face_total++;
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      face_total += verts_num;
    }
  }

  return face_total;
}

static void calculate_uvs(Mesh *mesh,
                          const bool top_is_point,
                          const bool bottom_is_point,
                          const int verts_num,
                          const GeometryNodeMeshCircleFillType fill_type)
{
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  OutputAttributePtr uv_attribute = mesh_component.attribute_try_get_for_output(
      "uv_map", ATTR_DOMAIN_CORNER, CD_PROP_FLOAT2, nullptr);
  MutableSpan<float2> uvs = uv_attribute->get_span_for_write_only<float2>();

  Array<float2> circle(verts_num);
  float angle = 0.0f;
  const float angle_delta = 2.0f * M_PI / static_cast<float>(verts_num);
  for (const int i : IndexRange(verts_num)) {
    circle[i].x = std::cos(angle) * 0.225f + 0.25f;
    circle[i].y = std::sin(angle) * 0.225f + 0.25f;
    angle += angle_delta;
  }

  int loop_index = 0;
  if (!top_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      for (const int i : IndexRange(verts_num)) {
        uvs[loop_index++] = circle[i];
      }
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      for (const int i : IndexRange(verts_num)) {
        uvs[loop_index++] = circle[i];
        uvs[loop_index++] = circle[(i + 1) % verts_num];
        uvs[loop_index++] = float2(0.25f, 0.25f);
      }
    }
  }

  /* Create side corners and faces. */
  if (!top_is_point && !bottom_is_point) {
    const float bottom = (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NONE) ? 0.0f : 0.5f;
    /* Quads connect the top and bottom. */
    for (const int i : IndexRange(verts_num)) {
      const float vert = static_cast<float>(i);
      uvs[loop_index++] = float2(vert / verts_num, bottom);
      uvs[loop_index++] = float2(vert / verts_num, 1.0f);
      uvs[loop_index++] = float2((vert + 1.0f) / verts_num, 1.0f);
      uvs[loop_index++] = float2((vert + 1.0f) / verts_num, bottom);
    }
  }
  else {
    /* Triangles connect the top and bottom section. */
    if (!top_is_point) {
      for (const int i : IndexRange(verts_num)) {
        uvs[loop_index++] = circle[i] + float2(0.5f, 0.0f);
        uvs[loop_index++] = float2(0.75f, 0.25f);
        uvs[loop_index++] = circle[(i + 1) % verts_num] + float2(0.5f, 0.0f);
      }
    }
    else {
      BLI_assert(!bottom_is_point);
      for (const int i : IndexRange(verts_num)) {
        uvs[loop_index++] = circle[i];
        uvs[loop_index++] = circle[(i + 1) % verts_num];
        uvs[loop_index++] = float2(0.25f, 0.25f);
      }
    }
  }

  /* Create bottom corners and faces. */
  if (!bottom_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      for (const int i : IndexRange(verts_num)) {
        /* Go backwards because of reversed face normal. */
        uvs[loop_index++] = circle[verts_num - 1 - i] + float2(0.5f, 0.0f);
      }
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      for (const int i : IndexRange(verts_num)) {
        uvs[loop_index++] = circle[i] + float2(0.5f, 0.0f);
        uvs[loop_index++] = float2(0.75f, 0.25f);
        uvs[loop_index++] = circle[(i + 1) % verts_num] + float2(0.5f, 0.0f);
      }
    }
  }

  uv_attribute.apply_span_and_save();
}

Mesh *create_cylinder_or_cone_mesh(const float radius_top,
                                   const float radius_bottom,
                                   const float depth,
                                   const int verts_num,
                                   const GeometryNodeMeshCircleFillType fill_type)
{
  const bool top_is_point = radius_top == 0.0f;
  const bool bottom_is_point = radius_bottom == 0.0f;
  const float height = depth * 0.5f;
  /* Handle the case of a line / single point before everything else to avoid
   * the need to check for it later. */
  if (top_is_point && bottom_is_point) {
    const bool single_vertex = height == 0.0f;
    Mesh *mesh = BKE_mesh_new_nomain(single_vertex ? 1 : 2, single_vertex ? 0 : 1, 0, 0, 0);
    copy_v3_v3(mesh->mvert[0].co, float3(0.0f, 0.0f, height));
    if (single_vertex) {
      const short up[3] = {0, 0, SHRT_MAX};
      copy_v3_v3_short(mesh->mvert[0].no, up);
      return mesh;
    }
    copy_v3_v3(mesh->mvert[1].co, float3(0.0f, 0.0f, -height));
    mesh->medge[0].v1 = 0;
    mesh->medge[0].v2 = 1;
    mesh->medge[0].flag |= ME_LOOSEEDGE;
    BKE_mesh_calc_normals(mesh);
    return mesh;
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      vert_total(fill_type, verts_num, top_is_point, bottom_is_point),
      edge_total(fill_type, verts_num, top_is_point, bottom_is_point),
      0,
      corner_total(fill_type, verts_num, top_is_point, bottom_is_point),
      face_total(fill_type, verts_num, top_is_point, bottom_is_point));
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  /* Calculate vertex positions. */
  const int top_verts_start = 0;
  const int bottom_verts_start = top_verts_start + (!top_is_point ? verts_num : 1);
  float angle = 0.0f;
  const float angle_delta = 2.0f * M_PI / static_cast<float>(verts_num);
  for (const int i : IndexRange(verts_num)) {
    const float x = std::cos(angle);
    const float y = std::sin(angle);
    if (!top_is_point) {
      copy_v3_v3(verts[top_verts_start + i].co, float3(x * radius_top, y * radius_top, height));
    }
    if (!bottom_is_point) {
      copy_v3_v3(verts[bottom_verts_start + i].co,
                 float3(x * radius_bottom, y * radius_bottom, -height));
    }
    angle += angle_delta;
  }
  if (top_is_point) {
    copy_v3_v3(verts[top_verts_start].co, float3(0.0f, 0.0f, height));
  }
  if (bottom_is_point) {
    copy_v3_v3(verts[bottom_verts_start].co, float3(0.0f, 0.0f, -height));
  }

  /* Add center vertices for the triangle fans at the end. */
  const int top_center_vert_index = bottom_verts_start + (bottom_is_point ? 1 : verts_num);
  const int bottom_center_vert_index = top_center_vert_index + (top_is_point ? 0 : 1);
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    if (!top_is_point) {
      copy_v3_v3(verts[top_center_vert_index].co, float3(0.0f, 0.0f, height));
    }
    if (!bottom_is_point) {
      copy_v3_v3(verts[bottom_center_vert_index].co, float3(0.0f, 0.0f, -height));
    }
  }

  /* Create top edges. */
  const int top_edges_start = 0;
  const int top_fan_edges_start = (!top_is_point &&
                                   fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) ?
                                      top_edges_start + verts_num :
                                      top_edges_start;
  if (!top_is_point) {
    for (const int i : IndexRange(verts_num)) {
      MEdge &edge = edges[top_edges_start + i];
      edge.v1 = top_verts_start + i;
      edge.v2 = top_verts_start + (i + 1) % verts_num;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      for (const int i : IndexRange(verts_num)) {
        MEdge &edge = edges[top_fan_edges_start + i];
        edge.v1 = top_center_vert_index;
        edge.v2 = top_verts_start + i;
        edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
      }
    }
  }

  /* Create connecting edges. */
  const int connecting_edges_start = top_fan_edges_start + (!top_is_point ? verts_num : 0);
  for (const int i : IndexRange(verts_num)) {
    MEdge &edge = edges[connecting_edges_start + i];
    edge.v1 = top_verts_start + (!top_is_point ? i : 0);
    edge.v2 = bottom_verts_start + (!bottom_is_point ? i : 0);
    edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
  }

  /* Create bottom edges. */
  const int bottom_edges_start = connecting_edges_start + verts_num;
  const int bottom_fan_edges_start = (!bottom_is_point &&
                                      fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) ?
                                         bottom_edges_start + verts_num :
                                         bottom_edges_start;
  if (!bottom_is_point) {
    for (const int i : IndexRange(verts_num)) {
      MEdge &edge = edges[bottom_edges_start + i];
      edge.v1 = bottom_verts_start + i;
      edge.v2 = bottom_verts_start + (i + 1) % verts_num;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      for (const int i : IndexRange(verts_num)) {
        MEdge &edge = edges[bottom_fan_edges_start + i];
        edge.v1 = bottom_center_vert_index;
        edge.v2 = bottom_verts_start + i;
        edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
      }
    }
  }

  /* Create top corners and faces. */
  int loop_index = 0;
  int poly_index = 0;
  if (!top_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = verts_num;

      for (const int i : IndexRange(verts_num)) {
        MLoop &loop = loops[loop_index++];
        loop.v = top_verts_start + i;
        loop.e = top_edges_start + i;
      }
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      for (const int i : IndexRange(verts_num)) {
        MPoly &poly = polys[poly_index++];
        poly.loopstart = loop_index;
        poly.totloop = 3;

        MLoop &loop_a = loops[loop_index++];
        loop_a.v = top_verts_start + i;
        loop_a.e = top_edges_start + i;
        MLoop &loop_b = loops[loop_index++];
        loop_b.v = top_verts_start + (i + 1) % verts_num;
        loop_b.e = top_fan_edges_start + (i + 1) % verts_num;
        MLoop &loop_c = loops[loop_index++];
        loop_c.v = top_center_vert_index;
        loop_c.e = top_fan_edges_start + i;
      }
    }
  }

  /* Create side corners and faces. */
  if (!top_is_point && !bottom_is_point) {
    /* Quads connect the top and bottom. */
    for (const int i : IndexRange(verts_num)) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = 4;

      MLoop &loop_a = loops[loop_index++];
      loop_a.v = top_verts_start + i;
      loop_a.e = connecting_edges_start + i;
      MLoop &loop_b = loops[loop_index++];
      loop_b.v = bottom_verts_start + i;
      loop_b.e = bottom_edges_start + i;
      MLoop &loop_c = loops[loop_index++];
      loop_c.v = bottom_verts_start + (i + 1) % verts_num;
      loop_c.e = connecting_edges_start + (i + 1) % verts_num;
      MLoop &loop_d = loops[loop_index++];
      loop_d.v = top_verts_start + (i + 1) % verts_num;
      loop_d.e = top_edges_start + i;
    }
  }
  else {
    /* Triangles connect the top and bottom section. */
    if (!top_is_point) {
      for (const int i : IndexRange(verts_num)) {
        MPoly &poly = polys[poly_index++];
        poly.loopstart = loop_index;
        poly.totloop = 3;

        MLoop &loop_a = loops[loop_index++];
        loop_a.v = top_verts_start + i;
        loop_a.e = connecting_edges_start + i;
        MLoop &loop_b = loops[loop_index++];
        loop_b.v = bottom_verts_start;
        loop_b.e = connecting_edges_start + (i + 1) % verts_num;
        MLoop &loop_c = loops[loop_index++];
        loop_c.v = top_verts_start + (i + 1) % verts_num;
        loop_c.e = top_edges_start + i;
      }
    }
    else {
      BLI_assert(!bottom_is_point);
      for (const int i : IndexRange(verts_num)) {
        MPoly &poly = polys[poly_index++];
        poly.loopstart = loop_index;
        poly.totloop = 3;

        MLoop &loop_a = loops[loop_index++];
        loop_a.v = bottom_verts_start + i;
        loop_a.e = bottom_edges_start + i;
        MLoop &loop_b = loops[loop_index++];
        loop_b.v = bottom_verts_start + (i + 1) % verts_num;
        loop_b.e = connecting_edges_start + (i + 1) % verts_num;
        MLoop &loop_c = loops[loop_index++];
        loop_c.v = top_verts_start;
        loop_c.e = connecting_edges_start + i;
      }
    }
  }

  /* Create bottom corners and faces. */
  if (!bottom_is_point) {
    if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = verts_num;

      for (const int i : IndexRange(verts_num)) {
        /* Go backwards to reverse surface normal. */
        MLoop &loop = loops[loop_index++];
        loop.v = bottom_verts_start + verts_num - 1 - i;
        loop.e = bottom_edges_start + verts_num - 1 - (i + 1) % verts_num;
      }
    }
    else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
      for (const int i : IndexRange(verts_num)) {
        MPoly &poly = polys[poly_index++];
        poly.loopstart = loop_index;
        poly.totloop = 3;

        MLoop &loop_a = loops[loop_index++];
        loop_a.v = bottom_verts_start + i;
        loop_a.e = bottom_fan_edges_start + i;
        MLoop &loop_b = loops[loop_index++];
        loop_b.v = bottom_center_vert_index;
        loop_b.e = bottom_fan_edges_start + (i + 1) % verts_num;
        MLoop &loop_c = loops[loop_index++];
        loop_c.v = bottom_verts_start + (i + 1) % verts_num;
        loop_c.e = bottom_edges_start + i;
      }
    }
  }

  BKE_mesh_calc_normals(mesh);

  calculate_uvs(mesh, top_is_point, bottom_is_point, verts_num, fill_type);

  BLI_assert(BKE_mesh_is_valid(mesh));

  return mesh;
}

static void geo_node_mesh_primitive_cone_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const NodeGeometryMeshCone &storage = *(const NodeGeometryMeshCone *)node.storage;

  const GeometryNodeMeshCircleFillType fill_type = (const GeometryNodeMeshCircleFillType)
                                                       storage.fill_type;

  const int verts_num = params.extract_input<int>("Vertices");
  if (verts_num < 3) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  const float radius_top = params.extract_input<float>("Radius Top");
  const float radius_bottom = params.extract_input<float>("Radius Bottom");
  const float depth = params.extract_input<float>("Depth");

  Mesh *mesh = create_cylinder_or_cone_mesh(
      radius_top, radius_bottom, depth, verts_num, fill_type);

  /* Transform the mesh so that the base of the cone is at the origin. */
  BKE_mesh_translate(mesh, float3(0.0f, 0.0f, depth * 0.5f), false);

  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_cone()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CONE, "Cone", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_cone_in, geo_node_mesh_primitive_cone_out);
  node_type_init(&ntype, geo_node_mesh_primitive_cone_init);
  node_type_storage(
      &ntype, "NodeGeometryMeshCone", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_cone_exec;
  ntype.draw_buttons = geo_node_mesh_primitive_cone_layout;
  nodeRegisterType(&ntype);
}

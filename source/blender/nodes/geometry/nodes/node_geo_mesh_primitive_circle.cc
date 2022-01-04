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

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_circle_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshCircle)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Vertices"))
      .default_value(32)
      .min(3)
      .description(N_("Number of vertices on the circle"));
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Distance of the vertices from the origin"));
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshCircle *node_storage = MEM_cnew<NodeGeometryMeshCircle>(__func__);

  node_storage->fill_type = GEO_NODE_MESH_CIRCLE_FILL_NONE;

  node->storage = node_storage;
}

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
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  /* Assign vertex coordinates. */
  const float angle_delta = 2.0f * (M_PI / static_cast<float>(verts_num));
  for (const int i : IndexRange(verts_num)) {
    const float angle = i * angle_delta;
    copy_v3_v3(verts[i].co, float3(std::cos(angle) * radius, std::sin(angle) * radius, 0.0f));
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
  const short edge_flag = (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NONE) ?
                              ME_LOOSEEDGE :
                              (ME_EDGEDRAW | ME_EDGERENDER); /* NGON or TRIANGLE_FAN */
  for (const int i : IndexRange(verts_num)) {
    MEdge &edge = edges[i];
    edge.v1 = i;
    edge.v2 = (i + 1) % verts_num;
    edge.flag = edge_flag;
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

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryMeshCircle &storage = node_storage(params.node());
  const GeometryNodeMeshCircleFillType fill = (GeometryNodeMeshCircleFillType)storage.fill_type;

  const float radius = params.extract_input<float>("Radius");
  const int verts_num = params.extract_input<int>("Vertices");
  if (verts_num < 3) {
    params.error_message_add(NodeWarningType::Info, TIP_("Vertices must be at least 3"));
    params.set_default_remaining_outputs();
    return;
  }

  Mesh *mesh = create_circle_mesh(radius, verts_num, fill);

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes::node_geo_mesh_primitive_circle_cc

void register_node_type_geo_mesh_primitive_circle()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_circle_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CIRCLE, "Mesh Circle", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(
      &ntype, "NodeGeometryMeshCircle", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

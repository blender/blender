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

namespace blender::nodes::node_geo_mesh_primitive_ellipse_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshEllipse)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Vertices"))
      .default_value(32)
      .min(3)
      .description(N_("Number of vertices on the ellipse"));
  b.add_input<decl::Float>(N_("Minor Radius"))
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Semi minor axis distance"));
  b.add_input<decl::Float>(N_("Major Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Semi major axis distance"));
  b.add_input<decl::Float>(N_("Phase"))
      .default_value(0.0f)
      .description(N_("Phase"));
  b.add_input<decl::Float>(N_("Rotation"))
      .default_value(0.0f)
      .description(N_("Rotation around the centering point"));
  b.add_input<decl::Float>(N_("Scale"))
      .default_value(1.0f)
      .min(0.0f)
      .description(N_("Scale the minor and major radii"));
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "definition_mode", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "centering_mode", 0, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshEllipse *node_storage = MEM_cnew<NodeGeometryMeshEllipse>(__func__);

  node_storage->fill_type = GEO_NODE_MESH_ELLIPSE_FILL_NONE;
  node_storage->definition_mode = GEO_NODE_MESH_ELLIPSE_DEFINITION_MINOR_MAJOR;
  node_storage->centering_mode = GEO_NODE_MESH_ELLIPSE_CENTERING_ORIGIN;

  node->storage = node_storage;
}

static int ellipse_vert_total(const GeometryNodeMeshEllipseFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_ELLIPSE_FILL_NONE:
    case GEO_NODE_MESH_ELLIPSE_FILL_NGON:
      return verts_num;
    case GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN:
      return verts_num + 1;
  }
  BLI_assert_unreachable();
  return 0;
}

static int ellipse_edge_total(const GeometryNodeMeshEllipseFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_ELLIPSE_FILL_NONE:
    case GEO_NODE_MESH_ELLIPSE_FILL_NGON:
      return verts_num;
    case GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN:
      return verts_num * 2;
  }
  BLI_assert_unreachable();
  return 0;
}

static int ellipse_corner_total(const GeometryNodeMeshEllipseFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_ELLIPSE_FILL_NONE:
      return 0;
    case GEO_NODE_MESH_ELLIPSE_FILL_NGON:
      return verts_num;
    case GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN:
      return verts_num * 3;
  }
  BLI_assert_unreachable();
  return 0;
}

static int ellipse_face_total(const GeometryNodeMeshEllipseFillType fill_type, const int verts_num)
{
  switch (fill_type) {
    case GEO_NODE_MESH_ELLIPSE_FILL_NONE:
      return 0;
    case GEO_NODE_MESH_ELLIPSE_FILL_NGON:
      return 1;
    case GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN:
      return verts_num;
  }
  BLI_assert_unreachable();
  return 0;
}

static Mesh *create_ellipse_mesh(const float minor_radius,
                                 const float major_radius,
                                 const float rotation,
                                 const float phase,
                                 const float scale,
                                 const int verts_num,
                                 const GeometryNodeMeshEllipseCenteringMode centering_mode,
                                 const GeometryNodeMeshEllipseFillType fill_type)
{
  Mesh *mesh = BKE_mesh_new_nomain(ellipse_vert_total(fill_type, verts_num),
                                   ellipse_edge_total(fill_type, verts_num),
                                   0,
                                   ellipse_corner_total(fill_type, verts_num),
                                   ellipse_face_total(fill_type, verts_num));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  float rx = major_radius * scale;
  float ry = minor_radius * scale;
  
  float cx = 0.0f;
  float cy = 0.0f;
  float dx = 0.0f;
  float dy = 0.0f;

  if (rx > ry)
  {
    dx = sqrtf(rx * rx - ry * ry);
    dy = 0;
  }
  else
  {
    dx = 0;
    dy = sqrtf(ry * ry - rx * rx);
  }

  if (centering_mode == GEO_NODE_MESH_ELLIPSE_CENTERING_FOCUS1)
  {
    cx = -dx;
    cy = -dy;
  }
  else if (centering_mode == GEO_NODE_MESH_ELLIPSE_CENTERING_FOCUS2)
  {
    cx = dx;
    cy = dy;
  }

  float sins = std::sin(rotation);  // cached for performance
  float coss = std::cos(rotation);  // cached for performance
        
  /* Assign vertex coordinates. */
  const float angle_delta = 2.0f * (M_PI / static_cast<float>(verts_num));
  for (const int i : IndexRange(verts_num)) {
    const float angle = i * angle_delta + phase;
    float x = cx + std::cos(angle) * rx;
    float y = cy + std::sin(angle) * ry;
    float xx = x * coss - y * sins;
    float yy = x * sins + y * coss;

    copy_v3_v3(verts[i].co, float3(xx, yy, 0));
  }
  if (fill_type == GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN) {
    copy_v3_v3(verts.last().co, float3(0));
  }

  /* Point all vertex normals in the up direction. */
  const short up_normal[3] = {0, 0, SHRT_MAX};
  for (MVert &vert : verts) {
    copy_v3_v3_short(vert.no, up_normal);
  }

  /* Create outer edges. */
  const short edge_flag = (fill_type == GEO_NODE_MESH_ELLIPSE_FILL_NONE) ?
                              ME_LOOSEEDGE :
                              (ME_EDGEDRAW | ME_EDGERENDER); /* NGON or TRIANGLE_FAN */
  for (const int i : IndexRange(verts_num)) {
    MEdge &edge = edges[i];
    edge.v1 = i;
    edge.v2 = (i + 1) % verts_num;
    edge.flag = edge_flag;
  }

  /* Create triangle fan edges. */
  if (fill_type == GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN) {
    for (const int i : IndexRange(verts_num)) {
      MEdge &edge = edges[verts_num + i];
      edge.v1 = verts_num;
      edge.v2 = i;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Create corners and faces. */
  if (fill_type == GEO_NODE_MESH_ELLIPSE_FILL_NGON) {
    MPoly &poly = polys[0];
    poly.loopstart = 0;
    poly.totloop = loops.size();

    for (const int i : IndexRange(verts_num)) {
      MLoop &loop = loops[i];
      loop.e = i;
      loop.v = i;
    }
  }
  else if (fill_type == GEO_NODE_MESH_ELLIPSE_FILL_TRIANGLE_FAN) {
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
  const NodeGeometryMeshEllipse &storage = node_storage(params.node());
  const GeometryNodeMeshEllipseFillType fill = (GeometryNodeMeshEllipseFillType)storage.fill_type;
  const GeometryNodeMeshEllipseDefinitionMode definition = (GeometryNodeMeshEllipseDefinitionMode)storage.definition_mode;
  const GeometryNodeMeshEllipseCenteringMode centering = (GeometryNodeMeshEllipseCenteringMode)storage.centering_mode;

  const float radius1 = params.extract_input<float>("Minor Radius");
  const float radius2 = params.extract_input<float>("Major Radius");
  const float rotation = params.extract_input<float>("Rotation");
  const float phase = params.extract_input<float>("Phase");
  const float scale = params.extract_input<float>("Scale");

  const int verts_num = params.extract_input<int>("Vertices");
  if (verts_num < 3) {
    params.error_message_add(NodeWarningType::Info, TIP_("Vertices must be at least 3"));
    params.set_default_remaining_outputs();
    return;
  }

  Mesh *mesh = create_ellipse_mesh(radius1, radius2, rotation, phase, scale, verts_num, centering, fill);

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes::node_geo_mesh_primitive_ellipse_cc

void register_node_type_geo_mesh_primitive_ellipse()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_ellipse_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_ELLIPSE, "Mesh Ellipse", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype, "NodeGeometryMeshEllipse", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

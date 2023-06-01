/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"
#include "BKE_mesh.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_circle_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshCircle)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Vertices")
      .default_value(32)
      .min(3)
      .description("Number of vertices on the circle");
  b.add_input<decl::Float>("Radius")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Distance of the vertices from the origin");
  b.add_output<decl::Geometry>("Mesh");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
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

static Bounds<float3> calculate_bounds_circle(const float radius, const int verts_num)
{
  return calculate_bounds_radial_primitive(0.0f, radius, verts_num, 0.0f);
}

static Mesh *create_circle_mesh(const float radius,
                                const int verts_num,
                                const GeometryNodeMeshCircleFillType fill_type)
{
  Mesh *mesh = BKE_mesh_new_nomain(circle_vert_total(fill_type, verts_num),
                                   circle_edge_total(fill_type, verts_num),
                                   circle_face_total(fill_type, verts_num),
                                   circle_corner_total(fill_type, verts_num));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int2> edges = mesh->edges_for_write();
  MutableSpan<int> poly_offsets = mesh->poly_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();
  BKE_mesh_smooth_flag_set(mesh, false);

  /* Assign vertex coordinates. */
  const float angle_delta = 2.0f * (M_PI / float(verts_num));
  for (const int i : IndexRange(verts_num)) {
    const float angle = i * angle_delta;
    positions[i] = float3(std::cos(angle) * radius, std::sin(angle) * radius, 0.0f);
  }
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    positions.last() = float3(0);
  }

  /* Create outer edges. */
  for (const int i : IndexRange(verts_num)) {
    int2 &edge = edges[i];
    edge[0] = i;
    edge[1] = (i + 1) % verts_num;
  }

  /* Create triangle fan edges. */
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    for (const int i : IndexRange(verts_num)) {
      int2 &edge = edges[verts_num + i];
      edge[0] = verts_num;
      edge[1] = i;
    }
  }

  /* Create corners and faces. */
  if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    poly_offsets.first() = 0;
    poly_offsets.last() = corner_verts.size();

    std::iota(corner_verts.begin(), corner_verts.end(), 0);
    std::iota(corner_edges.begin(), corner_edges.end(), 0);

    mesh->tag_loose_edges_none();
  }
  else if (fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN) {
    for (const int i : poly_offsets.index_range()) {
      poly_offsets[i] = 3 * i;
    }
    for (const int i : IndexRange(verts_num)) {
      corner_verts[3 * i] = i;
      corner_edges[3 * i] = i;

      corner_verts[3 * i + 1] = (i + 1) % verts_num;
      corner_edges[3 * i + 1] = verts_num + ((i + 1) % verts_num);

      corner_verts[3 * i + 2] = verts_num;
      corner_edges[3 * i + 2] = verts_num + i;
    }
  }

  mesh->tag_loose_verts_none();
  mesh->bounds_set_eager(calculate_bounds_circle(radius, verts_num));

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
  ntype.initfunc = file_ns::node_init;
  node_type_storage(
      &ntype, "NodeGeometryMeshCircle", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

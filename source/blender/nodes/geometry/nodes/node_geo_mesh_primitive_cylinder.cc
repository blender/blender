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

namespace blender::nodes {

static void geo_node_mesh_primitive_cylinder_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Vertices"))
      .default_value(32)
      .min(3)
      .max(512)
      .description(N_("The number of vertices around the circumference"));
  b.add_input<decl::Int>(N_("Side Segments"))
      .default_value(1)
      .min(1)
      .max(512)
      .description(N_("The number of segments along the side"));
  b.add_input<decl::Int>(N_("Fill Segments"))
      .default_value(1)
      .min(1)
      .max(512)
      .description(N_("The number of concentric segments of the fill"));
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("The radius of the cylinder"));
  b.add_input<decl::Float>(N_("Depth"))
      .default_value(2.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("The height of the cylinder on the Z axis"));
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void geo_node_mesh_primitive_cylinder_layout(uiLayout *layout,
                                                    bContext *UNUSED(C),
                                                    PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

static void geo_node_mesh_primitive_cylinder_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshCylinder *node_storage = (NodeGeometryMeshCylinder *)MEM_callocN(
      sizeof(NodeGeometryMeshCylinder), __func__);

  node_storage->fill_type = GEO_NODE_MESH_CIRCLE_FILL_NGON;

  node->storage = node_storage;
}

static void geo_node_mesh_primitive_cylinder_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *vertices_socket = (bNodeSocket *)node->inputs.first;
  bNodeSocket *rings_socket = vertices_socket->next;
  bNodeSocket *fill_subdiv_socket = rings_socket->next;

  const NodeGeometryMeshCone &storage = *(const NodeGeometryMeshCone *)node->storage;
  const GeometryNodeMeshCircleFillType fill_type =
      static_cast<const GeometryNodeMeshCircleFillType>(storage.fill_type);
  const bool has_fill = fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE;
  nodeSetSocketAvailability(fill_subdiv_socket, has_fill);
}

static void geo_node_mesh_primitive_cylinder_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const NodeGeometryMeshCylinder &storage = *(const NodeGeometryMeshCylinder *)node.storage;

  const GeometryNodeMeshCircleFillType fill_type = (const GeometryNodeMeshCircleFillType)
                                                       storage.fill_type;

  const float radius = params.extract_input<float>("Radius");
  const float depth = params.extract_input<float>("Depth");
  const int circle_segments = params.extract_input<int>("Vertices");
  if (circle_segments < 3) {
    params.error_message_add(NodeWarningType::Info, TIP_("Vertices must be at least 3"));
    params.set_output("Mesh", GeometrySet());
    return;
  }

  const int side_segments = params.extract_input<int>("Side Segments");
  if (side_segments < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Side Segments must be at least 1"));
    params.set_output("Mesh", GeometrySet());
    return;
  }

  const bool no_fill = fill_type == GEO_NODE_MESH_CIRCLE_FILL_NONE;
  const int fill_segments = no_fill ? 1 : params.extract_input<int>("Fill Segments");
  if (fill_segments < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Fill Segments must be at least 1"));
    params.set_output("Mesh", GeometrySet());
    return;
  }

  /* The cylinder is a special case of the cone mesh where the top and bottom radius are equal. */
  Mesh *mesh = create_cylinder_or_cone_mesh(
      radius, radius, depth, circle_segments, side_segments, fill_segments, fill_type);

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_cylinder()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CYLINDER, "Cylinder", NODE_CLASS_GEOMETRY, 0);
  node_type_init(&ntype, blender::nodes::geo_node_mesh_primitive_cylinder_init);
  node_type_update(&ntype, blender::nodes::geo_node_mesh_primitive_cylinder_update);
  node_type_storage(
      &ntype, "NodeGeometryMeshCylinder", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_mesh_primitive_cylinder_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_cylinder_exec;
  ntype.draw_buttons = blender::nodes::geo_node_mesh_primitive_cylinder_layout;
  nodeRegisterType(&ntype);
}

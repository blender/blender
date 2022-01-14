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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_attribute_domain_size_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Int>("Point Count").make_available([](bNode &node) {
    node.custom1 = GEO_COMPONENT_TYPE_MESH;
  });
  b.add_output<decl::Int>("Edge Count").make_available([](bNode &node) {
    node.custom1 = GEO_COMPONENT_TYPE_MESH;
  });
  b.add_output<decl::Int>("Face Count").make_available([](bNode &node) {
    node.custom1 = GEO_COMPONENT_TYPE_MESH;
  });
  b.add_output<decl::Int>("Face Corner Count").make_available([](bNode &node) {
    node.custom1 = GEO_COMPONENT_TYPE_MESH;
  });
  b.add_output<decl::Int>("Spline Count").make_available([](bNode &node) {
    node.custom1 = GEO_COMPONENT_TYPE_CURVE;
  });
  b.add_output<decl::Int>("Instance Count").make_available([](bNode &node) {
    node.custom1 = GEO_COMPONENT_TYPE_INSTANCES;
  });
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = GEO_COMPONENT_TYPE_MESH;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *point_socket = (bNodeSocket *)node->outputs.first;
  bNodeSocket *edge_socket = point_socket->next;
  bNodeSocket *face_socket = edge_socket->next;
  bNodeSocket *face_corner_socket = face_socket->next;
  bNodeSocket *spline_socket = face_corner_socket->next;
  bNodeSocket *instances_socket = spline_socket->next;

  nodeSetSocketAvailability(ntree,
                            point_socket,
                            ELEM(node->custom1,
                                 GEO_COMPONENT_TYPE_MESH,
                                 GEO_COMPONENT_TYPE_CURVE,
                                 GEO_COMPONENT_TYPE_POINT_CLOUD));
  nodeSetSocketAvailability(ntree, edge_socket, node->custom1 == GEO_COMPONENT_TYPE_MESH);
  nodeSetSocketAvailability(ntree, face_socket, node->custom1 == GEO_COMPONENT_TYPE_MESH);
  nodeSetSocketAvailability(ntree, face_corner_socket, node->custom1 == GEO_COMPONENT_TYPE_MESH);
  nodeSetSocketAvailability(ntree, spline_socket, node->custom1 == GEO_COMPONENT_TYPE_CURVE);
  nodeSetSocketAvailability(
      ntree, instances_socket, node->custom1 == GEO_COMPONENT_TYPE_INSTANCES);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometryComponentType component = (GeometryComponentType)params.node().custom1;
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  switch (component) {
    case GEO_COMPONENT_TYPE_MESH: {
      if (geometry_set.has_mesh()) {
        const MeshComponent *component = geometry_set.get_component_for_read<MeshComponent>();
        params.set_output("Point Count", component->attribute_domain_size(ATTR_DOMAIN_POINT));
        params.set_output("Edge Count", component->attribute_domain_size(ATTR_DOMAIN_EDGE));
        params.set_output("Face Count", component->attribute_domain_size(ATTR_DOMAIN_FACE));
        params.set_output("Face Corner Count",
                          component->attribute_domain_size(ATTR_DOMAIN_CORNER));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GEO_COMPONENT_TYPE_CURVE: {
      if (geometry_set.has_curve()) {
        const CurveComponent *component = geometry_set.get_component_for_read<CurveComponent>();
        params.set_output("Point Count", component->attribute_domain_size(ATTR_DOMAIN_POINT));
        params.set_output("Spline Count", component->attribute_domain_size(ATTR_DOMAIN_CURVE));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GEO_COMPONENT_TYPE_POINT_CLOUD: {
      if (geometry_set.has_pointcloud()) {
        const PointCloudComponent *component =
            geometry_set.get_component_for_read<PointCloudComponent>();
        params.set_output("Point Count", component->attribute_domain_size(ATTR_DOMAIN_POINT));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GEO_COMPONENT_TYPE_INSTANCES: {
      if (geometry_set.has_instances()) {
        const InstancesComponent *component =
            geometry_set.get_component_for_read<InstancesComponent>();
        params.set_output("Instance Count",
                          component->attribute_domain_size(ATTR_DOMAIN_INSTANCE));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    default:
      BLI_assert_unreachable();
  }
}

}  // namespace blender::nodes::node_geo_attribute_domain_size_cc

void register_node_type_geo_attribute_domain_size()
{
  namespace file_ns = blender::nodes::node_geo_attribute_domain_size_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_DOMAIN_SIZE, "Domain Size", NODE_CLASS_ATTRIBUTE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_init(&ntype, file_ns::node_init);
  ntype.updatefunc = file_ns::node_update;

  nodeRegisterType(&ntype);
}

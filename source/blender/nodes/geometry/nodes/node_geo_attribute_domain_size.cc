/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = GEO_COMPONENT_TYPE_MESH;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *point_socket = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *edge_socket = point_socket->next;
  bNodeSocket *face_socket = edge_socket->next;
  bNodeSocket *face_corner_socket = face_socket->next;
  bNodeSocket *spline_socket = face_corner_socket->next;
  bNodeSocket *instances_socket = spline_socket->next;

  bke::nodeSetSocketAvailability(ntree,
                                 point_socket,
                                 ELEM(node->custom1,
                                      GEO_COMPONENT_TYPE_MESH,
                                      GEO_COMPONENT_TYPE_CURVE,
                                      GEO_COMPONENT_TYPE_POINT_CLOUD));
  bke::nodeSetSocketAvailability(ntree, edge_socket, node->custom1 == GEO_COMPONENT_TYPE_MESH);
  bke::nodeSetSocketAvailability(ntree, face_socket, node->custom1 == GEO_COMPONENT_TYPE_MESH);
  bke::nodeSetSocketAvailability(
      ntree, face_corner_socket, node->custom1 == GEO_COMPONENT_TYPE_MESH);
  bke::nodeSetSocketAvailability(ntree, spline_socket, node->custom1 == GEO_COMPONENT_TYPE_CURVE);
  bke::nodeSetSocketAvailability(
      ntree, instances_socket, node->custom1 == GEO_COMPONENT_TYPE_INSTANCES);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const GeometryComponentType component = (GeometryComponentType)params.node().custom1;
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  switch (component) {
    case GEO_COMPONENT_TYPE_MESH: {
      if (const MeshComponent *component = geometry_set.get_component_for_read<MeshComponent>()) {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(ATTR_DOMAIN_POINT));
        params.set_output("Edge Count", attributes.domain_size(ATTR_DOMAIN_EDGE));
        params.set_output("Face Count", attributes.domain_size(ATTR_DOMAIN_FACE));
        params.set_output("Face Corner Count", attributes.domain_size(ATTR_DOMAIN_CORNER));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GEO_COMPONENT_TYPE_CURVE: {
      if (const CurveComponent *component = geometry_set.get_component_for_read<CurveComponent>())
      {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(ATTR_DOMAIN_POINT));
        params.set_output("Spline Count", attributes.domain_size(ATTR_DOMAIN_CURVE));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GEO_COMPONENT_TYPE_POINT_CLOUD: {
      if (const PointCloudComponent *component =
              geometry_set.get_component_for_read<PointCloudComponent>())
      {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(ATTR_DOMAIN_POINT));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GEO_COMPONENT_TYPE_INSTANCES: {
      if (const InstancesComponent *component =
              geometry_set.get_component_for_read<InstancesComponent>())
      {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Instance Count", attributes.domain_size(ATTR_DOMAIN_INSTANCE));
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
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;

  nodeRegisterType(&ntype);
}

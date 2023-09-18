/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_attribute_domain_size_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Int>("Point Count").make_available([](bNode &node) {
    node.custom1 = int16_t(GeometryComponent::Type::Mesh);
  });
  b.add_output<decl::Int>("Edge Count").make_available([](bNode &node) {
    node.custom1 = int16_t(GeometryComponent::Type::Mesh);
  });
  b.add_output<decl::Int>("Face Count").make_available([](bNode &node) {
    node.custom1 = int16_t(GeometryComponent::Type::Mesh);
  });
  b.add_output<decl::Int>("Face Corner Count").make_available([](bNode &node) {
    node.custom1 = int16_t(GeometryComponent::Type::Mesh);
  });
  b.add_output<decl::Int>("Spline Count").make_available([](bNode &node) {
    node.custom1 = int16_t(GeometryComponent::Type::Curve);
  });
  b.add_output<decl::Int>("Instance Count").make_available([](bNode &node) {
    node.custom1 = int16_t(GeometryComponent::Type::Instance);
  });
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(GeometryComponent::Type::Mesh);
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
                                      int16_t(GeometryComponent::Type::Mesh),
                                      int16_t(GeometryComponent::Type::Curve),
                                      int16_t(GeometryComponent::Type::PointCloud)));
  bke::nodeSetSocketAvailability(
      ntree, edge_socket, node->custom1 == int16_t(GeometryComponent::Type::Mesh));
  bke::nodeSetSocketAvailability(
      ntree, face_socket, node->custom1 == int16_t(GeometryComponent::Type::Mesh));
  bke::nodeSetSocketAvailability(
      ntree, face_corner_socket, node->custom1 == int16_t(GeometryComponent::Type::Mesh));
  bke::nodeSetSocketAvailability(
      ntree, spline_socket, node->custom1 == int16_t(GeometryComponent::Type::Curve));
  bke::nodeSetSocketAvailability(
      ntree, instances_socket, node->custom1 == int16_t(GeometryComponent::Type::Instance));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const GeometryComponent::Type component = GeometryComponent::Type(params.node().custom1);
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  switch (component) {
    case GeometryComponent::Type::Mesh: {
      if (const MeshComponent *component = geometry_set.get_component<MeshComponent>()) {
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
    case GeometryComponent::Type::Curve: {
      if (const CurveComponent *component = geometry_set.get_component<CurveComponent>()) {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(ATTR_DOMAIN_POINT));
        params.set_output("Spline Count", attributes.domain_size(ATTR_DOMAIN_CURVE));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GeometryComponent::Type::PointCloud: {
      if (const PointCloudComponent *component = geometry_set.get_component<PointCloudComponent>())
      {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(ATTR_DOMAIN_POINT));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GeometryComponent::Type::Instance: {
      if (const InstancesComponent *component = geometry_set.get_component<InstancesComponent>()) {
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

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "component",
                    "Component",
                    "",
                    rna_enum_geometry_component_type_items,
                    NOD_inline_enum_accessors(custom1),
                    int(blender::bke::GeometryComponent::Type::Mesh));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_DOMAIN_SIZE, "Domain Size", NODE_CLASS_ATTRIBUTE);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;

  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_attribute_domain_size_cc

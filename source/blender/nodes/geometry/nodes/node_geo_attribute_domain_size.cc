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
  auto &total_points = b.add_output<decl::Int>("Point Count")
                           .make_available([](bNode &node) {
                             node.custom1 = int16_t(GeometryComponent::Type::Mesh);
                           })
                           .available(false);
  auto &total_edges = b.add_output<decl::Int>("Edge Count")
                          .make_available([](bNode &node) {
                            node.custom1 = int16_t(GeometryComponent::Type::Mesh);
                          })
                          .available(false);
  auto &total_faces = b.add_output<decl::Int>("Face Count")
                          .make_available([](bNode &node) {
                            node.custom1 = int16_t(GeometryComponent::Type::Mesh);
                          })
                          .available(false);
  auto &total_corners = b.add_output<decl::Int>("Face Corner Count")
                            .make_available([](bNode &node) {
                              node.custom1 = int16_t(GeometryComponent::Type::Mesh);
                            })
                            .available(false);
  auto &total_curves = b.add_output<decl::Int>("Spline Count")
                           .make_available([](bNode &node) {
                             node.custom1 = int16_t(GeometryComponent::Type::Curve);
                           })
                           .available(false);
  auto &total_instances = b.add_output<decl::Int>("Instance Count")
                              .make_available([](bNode &node) {
                                node.custom1 = int16_t(GeometryComponent::Type::Instance);
                              })
                              .available(false);
  auto &total_layers = b.add_output<decl::Int>("Layer Count")
                           .make_available([](bNode &node) {
                             node.custom1 = int16_t(GeometryComponent::Type::GreasePencil);
                           })
                           .available(false);

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    switch (GeometryComponent::Type(node->custom1)) {
      case GeometryComponent::Type::Mesh:
        total_points.available(true);
        total_edges.available(true);
        total_faces.available(true);
        total_corners.available(true);
        break;
      case GeometryComponent::Type::Curve:
        total_points.available(true);
        total_curves.available(true);
        break;
      case GeometryComponent::Type::PointCloud:
        total_points.available(true);
        break;
      case GeometryComponent::Type::Instance:
        total_instances.available(true);
        break;
      case GeometryComponent::Type::GreasePencil:
        total_layers.available(true);
        break;
      default:
        BLI_assert_unreachable();
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(GeometryComponent::Type::Mesh);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const GeometryComponent::Type component = GeometryComponent::Type(params.node().custom1);
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  switch (component) {
    case GeometryComponent::Type::Mesh: {
      if (const MeshComponent *component = geometry_set.get_component<MeshComponent>()) {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(AttrDomain::Point));
        params.set_output("Edge Count", attributes.domain_size(AttrDomain::Edge));
        params.set_output("Face Count", attributes.domain_size(AttrDomain::Face));
        params.set_output("Face Corner Count", attributes.domain_size(AttrDomain::Corner));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GeometryComponent::Type::Curve: {
      if (const CurveComponent *component = geometry_set.get_component<CurveComponent>()) {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Point Count", attributes.domain_size(AttrDomain::Point));
        params.set_output("Spline Count", attributes.domain_size(AttrDomain::Curve));
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
        params.set_output("Point Count", attributes.domain_size(AttrDomain::Point));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GeometryComponent::Type::Instance: {
      if (const InstancesComponent *component = geometry_set.get_component<InstancesComponent>()) {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Instance Count", attributes.domain_size(AttrDomain::Instance));
      }
      else {
        params.set_default_remaining_outputs();
      }
      break;
    }
    case GeometryComponent::Type::GreasePencil: {
      if (const GreasePencilComponent *component =
              geometry_set.get_component<GreasePencilComponent>())
      {
        const AttributeAccessor attributes = *component->attributes();
        params.set_output("Layer Count", attributes.domain_size(AttrDomain::Layer));
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
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeAttributeDomainSize", GEO_NODE_ATTRIBUTE_DOMAIN_SIZE);
  ntype.ui_name = "Domain Size";
  ntype.ui_description = "Retrieve the number of elements in a geometry for each attribute domain";
  ntype.enum_name_legacy = "ATTRIBUTE_DOMAIN_SIZE";
  ntype.nclass = NODE_CLASS_ATTRIBUTE;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;

  blender::bke::node_register_type(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_attribute_domain_size_cc

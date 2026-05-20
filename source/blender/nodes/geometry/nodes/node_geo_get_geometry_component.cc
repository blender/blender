/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_get_geometry_component_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Geometry"_ustr);
  b.add_output<decl::Geometry>("Geometry"_ustr)
      .align_with_previous()
      .description("The input geometry with optionally the selected component removed");
  b.add_output<decl::Geometry>("Component"_ustr);
  b.add_output<decl::Bool>("Exists"_ustr)
      .description(
          "Whether the geometry had a component of the type. This does not check if the component "
          "is empty");
  b.add_input<decl::Menu>("Type"_ustr)
      .static_items(rna_enum_geometry_component_type_items)
      .optional_label();
  b.add_input<decl::Bool>("Remove"_ustr).default_value(true);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry"_ustr);
  const GeometryComponent::Type type = params.extract_input<GeometryComponent::Type>("Type"_ustr);
  const bool remove = params.extract_input<bool>("Remove"_ustr);

  GeometrySet component_geo;
  bool exists = false;

  switch (type) {
    case bke::GeometryComponent::Type::Mesh:
    case bke::GeometryComponent::Type::PointCloud:
    case bke::GeometryComponent::Type::Instance:
    case bke::GeometryComponent::Type::Volume:
    case bke::GeometryComponent::Type::Curve:
    case bke::GeometryComponent::Type::GreasePencil: {
      if (const GeometryComponent *component = geometry.get_component(type)) {
        component_geo.add(*component);
        exists = true;
      }
      component_geo.copy_bundle_from(geometry);
      component_geo.set_name(geometry.name());
      if (remove) {
        geometry.remove(type);
      }
      break;
    }
    case bke::GeometryComponent::Type::Edit: {
      break;
    }
  }

  params.set_output("Geometry"_ustr, geometry);
  params.set_output("Component"_ustr, component_geo);
  params.set_output("Exists"_ustr, exists);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGetGeometryComponent"_ustr);
  ntype.ui_name = "Get Geometry Component";
  ntype.ui_description = "Get a single component of a geometry";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.default_width = bke::NodeWidth::_180;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_geometry_component_cc

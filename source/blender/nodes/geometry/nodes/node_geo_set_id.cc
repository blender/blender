/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_id_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry").description("Geometry to update the ID attribute on");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("ID").implicit_field_on_all(NODE_DEFAULT_INPUT_INDEX_FIELD);
}

static void set_id_in_component(GeometryComponent &component,
                                const Field<bool> &selection_field,
                                const Field<int> &id_field)
{
  const AttrDomain domain = (component.type() == GeometryComponent::Type::Instance) ?
                                AttrDomain::Instance :
                                AttrDomain::Point;
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }
  MutableAttributeAccessor attributes = *component.attributes_for_write();

  const bke::GeometryFieldContext field_context{component, domain};
  bke::try_capture_field_on_geometry(
      attributes, field_context, "id", domain, selection_field, id_field);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<int> id_field = params.extract_input<Field<int>>("ID");

  for (const GeometryComponent::Type type : {GeometryComponent::Type::Instance,
                                             GeometryComponent::Type::Mesh,
                                             GeometryComponent::Type::PointCloud,
                                             GeometryComponent::Type::Curve})
  {
    if (geometry_set.has(type)) {
      set_id_in_component(geometry_set.get_component_for_write(type), selection_field, id_field);
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetID", GEO_NODE_SET_ID);
  ntype.ui_name = "Set ID";
  ntype.ui_description =
      "Set the id attribute on the input geometry, mainly used internally for randomizing";
  ntype.enum_name_legacy = "SET_ID";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_id_cc

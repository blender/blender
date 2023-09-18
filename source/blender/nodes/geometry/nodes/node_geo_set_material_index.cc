/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_material_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("Material Index").field_on_all().min(0);
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void set_material_index_in_component(GeometryComponent &component,
                                            const Field<bool> &selection_field,
                                            const Field<int> &index_field,
                                            const eAttrDomain domain)
{
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }
  MutableAttributeAccessor attributes = *component.attributes_for_write();

  const bke::AttributeValidator validator = attributes.lookup_validator("material_index");
  AttributeWriter<int> indices = attributes.lookup_or_add_for_write<int>("material_index", domain);

  const bke::GeometryFieldContext field_context{component, domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(validator.validate_field_if_necessary(index_field),
                                 indices.varray);
  evaluator.evaluate();
  indices.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<int> index_field = params.extract_input<Field<int>>("Material Index");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      set_material_index_in_component(geometry_set.get_component_for_write<MeshComponent>(),
                                      selection_field,
                                      index_field,
                                      ATTR_DOMAIN_FACE);
    }
  });
  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_MATERIAL_INDEX, "Set Material Index", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_material_index_cc

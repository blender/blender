/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_id_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("ID").implicit_field_on_all(implicit_field_inputs::index);
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void set_id_in_component(GeometryComponent &component,
                                const Field<bool> &selection_field,
                                const Field<int> &id_field)
{
  const eAttrDomain domain = (component.type() == GeometryComponent::Type::Instance) ?
                                 ATTR_DOMAIN_INSTANCE :
                                 ATTR_DOMAIN_POINT;
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }
  MutableAttributeAccessor attributes = *component.attributes_for_write();

  const bke::GeometryFieldContext field_context{component, domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);

  /* Since adding the ID attribute can change the result of the field evaluation (the random value
   * node uses the index if the ID is unavailable), make sure that it isn't added before evaluating
   * the field. However, as an optimization, use a faster code path when it already exists. */
  if (attributes.contains("id")) {
    AttributeWriter<int> id_attribute = attributes.lookup_or_add_for_write<int>("id", domain);
    evaluator.add_with_destination(id_field, id_attribute.varray);
    evaluator.evaluate();
    id_attribute.finish();
  }
  else {
    evaluator.add(id_field);
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
    const VArray<int> result_ids = evaluator.get_evaluated<int>(0);
    SpanAttributeWriter<int> id_attribute = attributes.lookup_or_add_for_write_span<int>("id",
                                                                                         domain);
    result_ids.materialize(selection, id_attribute.span);
    id_attribute.finish();
  }
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
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_ID, "Set ID", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_id_cc

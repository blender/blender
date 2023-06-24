/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_spline_cyclic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Bool>("Cyclic").field_on_all();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void set_cyclic(bke::CurvesGeometry &curves,
                       const Field<bool> &selection_field,
                       const Field<bool> &cyclic_field)
{
  if (curves.curves_num() == 0) {
    return;
  }
  MutableAttributeAccessor attributes = curves.attributes_for_write();
  AttributeWriter<bool> cyclics = attributes.lookup_or_add_for_write<bool>("cyclic",
                                                                           ATTR_DOMAIN_CURVE);

  const bke::CurvesFieldContext field_context{curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(cyclic_field, cyclics.varray);
  evaluator.evaluate();

  cyclics.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<bool> cyclic_field = params.extract_input<Field<bool>>("Cyclic");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      set_cyclic(curves_id->geometry.wrap(), selection_field, cyclic_field);
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_spline_cyclic_cc

void register_node_type_geo_set_spline_cyclic()
{
  namespace file_ns = blender::nodes::node_geo_set_spline_cyclic_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_SPLINE_CYCLIC, "Set Spline Cyclic", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

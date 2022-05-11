/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_curve_tilt_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>(N_("Tilt")).subtype(PROP_ANGLE).supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void set_tilt_in_component(GeometryComponent &component,
                                  const Field<bool> &selection_field,
                                  const Field<float> &tilt_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_num = component.attribute_domain_num(ATTR_DOMAIN_POINT);
  if (domain_num == 0) {
    return;
  }

  OutputAttribute_Typed<float> tilts = component.attribute_try_get_for_output_only<float>(
      "tilt", ATTR_DOMAIN_POINT);

  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(tilt_field, tilts.varray());
  evaluator.evaluate();

  tilts.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> tilt_field = params.extract_input<Field<float>>("Tilt");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curves()) {
      set_tilt_in_component(
          geometry_set.get_component_for_write<CurveComponent>(), selection_field, tilt_field);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_curve_tilt_cc

void register_node_type_geo_set_curve_tilt()
{
  namespace file_ns = blender::nodes::node_geo_set_curve_tilt_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_CURVE_TILT, "Set Curve Tilt", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

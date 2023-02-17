/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_spline_resolution_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>(N_("Resolution")).min(1).default_value(12).field_on_all();
  b.add_output<decl::Geometry>(N_("Geometry")).propagate_all();
}

static void set_resolution(bke::CurvesGeometry &curves,
                           const Field<bool> &selection_field,
                           const Field<int> &resolution_field)
{
  if (curves.curves_num() == 0) {
    return;
  }
  MutableAttributeAccessor attributes = curves.attributes_for_write();
  AttributeWriter<int> resolutions = attributes.lookup_or_add_for_write<int>("resolution",
                                                                             ATTR_DOMAIN_CURVE);
  bke::AttributeValidator validator = attributes.lookup_validator("resolution");

  bke::CurvesFieldContext field_context{curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(validator.validate_field_if_necessary(resolution_field),
                                 resolutions.varray);
  evaluator.evaluate();

  resolutions.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  Field<int> resolution = params.extract_input<Field<int>>("Resolution");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      set_resolution(curves_id->geometry.wrap(), selection, resolution);
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_spline_resolution_cc

void register_node_type_geo_set_spline_resolution()
{
  namespace file_ns = blender::nodes::node_geo_set_spline_resolution_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_SPLINE_RESOLUTION, "Set Spline Resolution", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

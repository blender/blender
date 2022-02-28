/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_reverse_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }

    Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
    CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
    GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_CURVE};
    const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_CURVE);

    fn::FieldEvaluator selection_evaluator{field_context, domain_size};
    selection_evaluator.add(selection_field);
    selection_evaluator.evaluate();
    const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

    std::unique_ptr<CurveEval> curve = curves_to_curve_eval(*component.get_for_write());
    MutableSpan<SplinePtr> splines = curve->splines();
    threading::parallel_for(selection.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        splines[selection[i]]->reverse();
      }
    });

    component.replace(curve_eval_to_curves(*curve));
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_reverse_cc

void register_node_type_geo_curve_reverse()
{
  namespace file_ns = blender::nodes::node_geo_curve_reverse_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REVERSE_CURVE, "Reverse Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

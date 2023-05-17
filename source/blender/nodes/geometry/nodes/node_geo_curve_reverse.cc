/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_reverse_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }
    const Curves &src_curves_id = *geometry_set.get_curves_for_read();
    const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();

    const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_CURVE};
    fn::FieldEvaluator selection_evaluator{field_context, src_curves.curves_num()};
    selection_evaluator.add(params.get_input<Field<bool>>("Selection"));
    selection_evaluator.evaluate();
    const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);
    if (selection.is_empty()) {
      return;
    }

    Curves &curves_id = *geometry_set.get_curves_for_write();
    bke::CurvesGeometry &curves = curves_id.geometry.wrap();
    curves.reverse_curves(selection);
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

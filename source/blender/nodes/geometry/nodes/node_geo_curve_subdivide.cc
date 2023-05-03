/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "GEO_subdivide_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_subdivide_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Int>(N_("Cuts"))
      .default_value(1)
      .min(0)
      .max(1000)
      .field_on_all()
      .description(
          N_("The number of control points to create on the segment following each point"));
  b.add_output<decl::Geometry>(N_("Curve")).propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<int> cuts_field = params.extract_input<Field<int>>("Cuts");

  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }

    const Curves &src_curves_id = *geometry_set.get_curves_for_read();
    const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();

    const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_POINT};
    fn::FieldEvaluator evaluator{field_context, src_curves.points_num()};
    evaluator.add(cuts_field);
    evaluator.evaluate();
    const VArray<int> cuts = evaluator.get_evaluated<int>(0);
    if (cuts.is_single() && cuts.get_internal_single() < 1) {
      return;
    }

    bke::CurvesGeometry dst_curves = geometry::subdivide_curves(
        src_curves, src_curves.curves_range(), cuts, params.get_output_propagation_info("Curve"));

    Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
    bke::curves_copy_parameters(src_curves_id, *dst_curves_id);
    geometry_set.replace_curves(dst_curves_id);
  });
  params.set_output("Curve", geometry_set);
}

}  // namespace blender::nodes::node_geo_curve_subdivide_cc

void register_node_type_geo_curve_subdivide()
{
  namespace file_ns = blender::nodes::node_geo_curve_subdivide_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SUBDIVIDE_CURVE, "Subdivide Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

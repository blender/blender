/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_curve_reverse_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve"));
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  geometry_set = geometry::realize_instances_legacy(geometry_set);
  if (!geometry_set.has_curves()) {
    params.set_output("Curve", geometry_set);
    return;
  }

  /* Retrieve data for write access so we can avoid new allocations for the reversed data. */
  CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
  std::unique_ptr<CurveEval> curve = curves_to_curve_eval(*curve_component.get_for_read());
  MutableSpan<SplinePtr> splines = curve->splines();

  const std::string selection_name = params.extract_input<std::string>("Selection");
  VArray<bool> selection = curve_component.attribute_get_for_read(
      selection_name, ATTR_DOMAIN_CURVE, true);

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      if (selection[i]) {
        splines[i]->reverse();
      }
    }
  });

  geometry_set.replace_curve(curve_eval_to_curves(*curve));

  params.set_output("Curve", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_curve_reverse_cc

void register_node_type_geo_legacy_curve_reverse()
{
  namespace file_ns = blender::nodes::node_geo_legacy_curve_reverse_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_LEGACY_CURVE_REVERSE, "Curve Reverse", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

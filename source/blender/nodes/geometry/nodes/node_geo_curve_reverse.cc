/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_curve_reverse_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>("Curve");
}

static void geo_node_curve_reverse_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curve()) {
      return;
    }

    Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
    CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
    GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_CURVE};
    const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_CURVE);

    fn::FieldEvaluator selection_evaluator{field_context, domain_size};
    selection_evaluator.add(selection_field);
    selection_evaluator.evaluate();
    const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

    CurveEval &curve = *component.get_for_write();
    MutableSpan<SplinePtr> splines = curve.splines();
    threading::parallel_for(selection.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        splines[selection[i]]->reverse();
      }
    });
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_reverse()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REVERSE_CURVE, "Reverse Curve", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_curve_reverse_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_reverse_exec;
  nodeRegisterType(&ntype);
}

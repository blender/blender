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

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_set_spline_cyclic_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Bool>(N_("Cyclic")).supports_field();
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void set_cyclic_in_component(GeometryComponent &component,
                                    const Field<bool> &selection_field,
                                    const Field<bool> &cyclic_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_CURVE};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_CURVE);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

  OutputAttribute_Typed<bool> cyclics = component.attribute_try_get_for_output_only<bool>(
      "cyclic", ATTR_DOMAIN_CURVE);
  fn::FieldEvaluator cyclic_evaluator{field_context, &selection};
  cyclic_evaluator.add_with_destination(cyclic_field, cyclics.varray());
  cyclic_evaluator.evaluate();
  cyclics.save();
}

static void geo_node_set_spline_cyclic_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<bool> cyclic_field = params.extract_input<Field<bool>>("Cyclic");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curve()) {
      set_cyclic_in_component(
          geometry_set.get_component_for_write<CurveComponent>(), selection_field, cyclic_field);
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_set_spline_cyclic()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_SPLINE_CYCLIC, "Set Spline Cyclic", NODE_CLASS_GEOMETRY, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_set_spline_cyclic_exec;
  ntype.declare = blender::nodes::geo_node_set_spline_cyclic_declare;
  nodeRegisterType(&ntype);
}

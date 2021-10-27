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

static void geo_node_set_curve_tilt_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>("Tilt").subtype(PROP_ANGLE).supports_field();
  b.add_output<decl::Geometry>("Curve");
}

static void set_tilt_in_component(GeometryComponent &component,
                                  const Field<bool> &selection_field,
                                  const Field<float> &tilt_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

  OutputAttribute_Typed<float> tilts = component.attribute_try_get_for_output_only<float>(
      "tilt", ATTR_DOMAIN_POINT);
  fn::FieldEvaluator tilt_evaluator{field_context, &selection};
  tilt_evaluator.add_with_destination(tilt_field, tilts.varray());
  tilt_evaluator.evaluate();
  tilts.save();
}

static void geo_node_set_curve_tilt_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> tilt_field = params.extract_input<Field<float>>("Tilt");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curve()) {
      set_tilt_in_component(
          geometry_set.get_component_for_write<CurveComponent>(), selection_field, tilt_field);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_set_curve_tilt()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_CURVE_TILT, "Set Tilt", NODE_CLASS_GEOMETRY, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_set_curve_tilt_exec;
  ntype.declare = blender::nodes::geo_node_set_curve_tilt_declare;
  nodeRegisterType(&ntype);
}

/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_primitive_star_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Points")
      .default_value(8)
      .min(3)
      .max(256)
      .subtype(PROP_UNSIGNED)
      .description("Number of points on each of the circles");
  b.add_input<decl::Float>("Inner Radius")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Radius of the inner circle; can be larger than outer radius");
  b.add_input<decl::Float>("Outer Radius")
      .default_value(2.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Radius of the outer circle; can be smaller than inner radius");
  b.add_input<decl::Float>("Twist")
      .subtype(PROP_ANGLE)
      .description("The counterclockwise rotation of the inner set of points");
  b.add_output<decl::Geometry>("Curve");
  b.add_output<decl::Bool>("Outer Points")
      .field_on_all()
      .description("An attribute field with a selection of the outer points");
}

static Curves *create_star_curve(const float inner_radius,
                                 const float outer_radius,
                                 const float twist,
                                 const int points)
{
  Curves *curves_id = bke::curves_new_nomain_single(points * 2, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  curves.cyclic_for_write().first() = true;

  MutableSpan<float3> positions = curves.positions_for_write();

  const float theta_step = (2.0f * M_PI) / float(points);
  for (const int i : IndexRange(points)) {
    const float x = outer_radius * cos(theta_step * i);
    const float y = outer_radius * sin(theta_step * i);
    positions[i * 2] = {x, y, 0.0f};

    const float inner_x = inner_radius * cos(theta_step * i + theta_step * 0.5f + twist);
    const float inner_y = inner_radius * sin(theta_step * i + theta_step * 0.5f + twist);
    positions[i * 2 + 1] = {inner_x, inner_y, 0.0f};
  }

  return curves_id;
}

static void create_selection_output(CurveComponent &component,
                                    AnonymousAttributeIDPtr &r_attribute)
{
  SpanAttributeWriter<bool> selection =
      component.attributes_for_write()->lookup_or_add_for_write_only_span<bool>(*r_attribute,
                                                                                ATTR_DOMAIN_POINT);
  for (int i : selection.span.index_range()) {
    selection.span[i] = i % 2 == 0;
  }
  selection.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Curves *curves = create_star_curve(std::max(params.extract_input<float>("Inner Radius"), 0.0f),
                                     std::max(params.extract_input<float>("Outer Radius"), 0.0f),
                                     params.extract_input<float>("Twist"),
                                     std::max(params.extract_input<int>("Points"), 3));
  GeometrySet output = GeometrySet::from_curves(curves);

  if (AnonymousAttributeIDPtr outer_points_id = params.get_output_anonymous_attribute_id_if_needed(
          "Outer Points"))
  {
    create_selection_output(output.get_component_for_write<CurveComponent>(), outer_points_id);
  }
  params.set_output("Curve", std::move(output));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_STAR, "Star", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_primitive_star_cc

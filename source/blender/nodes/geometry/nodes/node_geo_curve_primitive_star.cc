/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_primitive_star_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Points"))
      .default_value(8)
      .min(3)
      .max(256)
      .subtype(PROP_UNSIGNED)
      .description(N_("Number of points on each of the circles"));
  b.add_input<decl::Float>(N_("Inner Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Radius of the inner circle; can be larger than outer radius"));
  b.add_input<decl::Float>(N_("Outer Radius"))
      .default_value(2.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Radius of the outer circle; can be smaller than inner radius"));
  b.add_input<decl::Float>(N_("Twist"))
      .subtype(PROP_ANGLE)
      .description(N_("The counterclockwise rotation of the inner set of points"));
  b.add_output<decl::Geometry>(N_("Curve"));
  b.add_output<decl::Bool>(N_("Outer Points"))
      .field_on_all()
      .description(N_("An attribute field with a selection of the outer points"));
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
                                    AutoAnonymousAttributeID &r_attribute)
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
  GeometrySet output = GeometrySet::create_with_curves(curves);

  if (AutoAnonymousAttributeID outer_points_id =
          params.get_output_anonymous_attribute_id_if_needed("Outer Points")) {
    create_selection_output(output.get_component_for_write<CurveComponent>(), outer_points_id);
    params.set_output("Outer Points",
                      AnonymousAttributeFieldInput::Create<bool>(
                          std::move(outer_points_id), params.attribute_producer_name()));
  }
  params.set_output("Curve", std::move(output));
}
}  // namespace blender::nodes::node_geo_curve_primitive_star_cc

void register_node_type_geo_curve_primitive_star()
{
  namespace file_ns = blender::nodes::node_geo_curve_primitive_star_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_STAR, "Star", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

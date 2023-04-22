/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_primitive_spiral_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Resolution")
      .default_value(32)
      .min(1)
      .max(1024)
      .subtype(PROP_UNSIGNED)
      .description("Number of points in one rotation of the spiral");
  b.add_input<decl::Float>("Rotations")
      .default_value(2.0f)
      .min(0.0f)
      .description("Number of times the spiral makes a full rotation");
  b.add_input<decl::Float>("Start Radius")
      .default_value(1.0f)
      .subtype(PROP_DISTANCE)
      .description("Horizontal Distance from the Z axis at the start of the spiral");
  b.add_input<decl::Float>("End Radius")
      .default_value(2.0f)
      .subtype(PROP_DISTANCE)
      .description("Horizontal Distance from the Z axis at the end of the spiral");
  b.add_input<decl::Float>("Height")
      .default_value(2.0f)
      .subtype(PROP_DISTANCE)
      .description("The height perpendicular to the base of the spiral");
  b.add_input<decl::Bool>("Reverse").description(
      "Switch the direction from clockwise to counterclockwise");
  b.add_output<decl::Geometry>("Curve");
}

static Curves *create_spiral_curve(const float rotations,
                                   const int resolution,
                                   const float start_radius,
                                   const float end_radius,
                                   const float height,
                                   const bool direction)
{
  const int totalpoints = std::max(int(resolution * rotations), 1);
  const float delta_radius = (end_radius - start_radius) / float(totalpoints);
  const float delta_height = height / float(totalpoints);
  const float delta_theta = (M_PI * 2 * rotations) / float(totalpoints) *
                            (direction ? 1.0f : -1.0f);

  Curves *curves_id = bke::curves_new_nomain_single(totalpoints + 1, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  MutableSpan<float3> positions = curves.positions_for_write();

  for (const int i : IndexRange(totalpoints + 1)) {
    const float theta = i * delta_theta;
    const float radius = start_radius + i * delta_radius;
    const float x = radius * cos(theta);
    const float y = radius * sin(theta);
    const float z = delta_height * i;

    positions[i] = {x, y, z};
  }

  return curves_id;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const float rotations = std::max(params.extract_input<float>("Rotations"), 0.0f);
  if (rotations == 0.0f) {
    params.set_default_remaining_outputs();
    return;
  }

  Curves *curves = create_spiral_curve(rotations,
                                       std::max(params.extract_input<int>("Resolution"), 1),
                                       params.extract_input<float>("Start Radius"),
                                       params.extract_input<float>("End Radius"),
                                       params.extract_input<float>("Height"),
                                       params.extract_input<bool>("Reverse"));
  params.set_output("Curve", GeometrySet::create_with_curves(curves));
}

}  // namespace blender::nodes::node_geo_curve_primitive_spiral_cc

void register_node_type_geo_curve_primitive_spiral()
{
  namespace file_ns = blender::nodes::node_geo_curve_primitive_spiral_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_SPIRAL, "Spiral", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

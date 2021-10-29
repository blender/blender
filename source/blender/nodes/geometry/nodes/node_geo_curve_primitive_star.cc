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

#include "BKE_spline.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_curve_primitive_star_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Points")).default_value(8).min(3).max(256).subtype(PROP_UNSIGNED);
  b.add_input<decl::Float>(N_("Inner Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Outer Radius"))
      .default_value(2.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Twist")).subtype(PROP_ANGLE);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static std::unique_ptr<CurveEval> create_star_curve(const float inner_radius,
                                                    const float outer_radius,
                                                    const float twist,
                                                    const int points)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  const float theta_step = (2.0f * M_PI) / float(points);
  for (int i : IndexRange(points)) {
    const float x = outer_radius * cos(theta_step * i);
    const float y = outer_radius * sin(theta_step * i);
    spline->add_point(float3(x, y, 0.0f), 1.0f, 0.0f);

    const float inner_x = inner_radius * cos(theta_step * i + theta_step * 0.5f + twist);
    const float inner_y = inner_radius * sin(theta_step * i + theta_step * 0.5f + twist);
    spline->add_point(float3(inner_x, inner_y, 0.0f), 1.0f, 0.0f);
  }
  spline->set_cyclic(true);
  spline->attributes.reallocate(spline->size());
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void geo_node_curve_primitive_star_exec(GeoNodeExecParams params)
{
  std::unique_ptr<CurveEval> curve = create_star_curve(
      std::max(params.extract_input<float>("Inner Radius"), 0.0f),
      std::max(params.extract_input<float>("Outer Radius"), 0.0f),
      params.extract_input<float>("Twist"),
      std::max(params.extract_input<int>("Points"), 3));
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_star()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_STAR, "Star", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_curve_primitive_star_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_star_exec;
  nodeRegisterType(&ntype);
}

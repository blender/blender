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

static bNodeSocketTemplate geo_node_curve_primitive_star_in[] = {
    {SOCK_INT, N_("Points"), 8.0f, 0.0f, 0.0f, 0.0f, 4, 256, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Inner Radius"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("Outer Radius"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("Twist"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_ANGLE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_primitive_star_out[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {-1, ""},
};

namespace blender::nodes {

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
      std::max(params.extract_input<int>("Points"), 4));
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_star()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_STAR, "Star", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_curve_primitive_star_in, geo_node_curve_primitive_star_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_star_exec;
  nodeRegisterType(&ntype);
}

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

static bNodeSocketTemplate geo_node_curve_primitive_quadratic_bezier_in[] = {
    {SOCK_INT, N_("Resolution"), 16.0f, 0.0f, 0.0f, 0.0f, 3, 256, PROP_UNSIGNED},
    {SOCK_VECTOR, N_("Start"), -1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Middle"), 0.0f, 2.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("End"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_primitive_quadratic_bezier_out[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {-1, ""},
};

namespace blender::nodes {

static std::unique_ptr<CurveEval> create_quadratic_bezier_curve(const float3 p1,
                                                                const float3 p2,
                                                                const float3 p3,
                                                                const int resolution)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  const float step = 1.0f / resolution;
  for (int i : IndexRange(resolution + 1)) {
    const float factor = step * i;
    const float3 q1 = float3::interpolate(p1, p2, factor);
    const float3 q2 = float3::interpolate(p2, p3, factor);
    const float3 out = float3::interpolate(q1, q2, factor);
    spline->add_point(out, 1.0f, 0.0f);
  }
  spline->attributes.reallocate(spline->size());
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void geo_node_curve_primitive_quadratic_bezier_exec(GeoNodeExecParams params)
{
  std::unique_ptr<CurveEval> curve = create_quadratic_bezier_curve(
      params.extract_input<float3>("Start"),
      params.extract_input<float3>("Middle"),
      params.extract_input<float3>("End"),
      std::max(params.extract_input<int>("Resolution"), 3));
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_quadratic_bezier()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype,
                     GEO_NODE_CURVE_PRIMITIVE_QUADRATIC_BEZIER,
                     "Quadratic Bezier",
                     NODE_CLASS_GEOMETRY,
                     0);
  node_type_socket_templates(&ntype,
                             geo_node_curve_primitive_quadratic_bezier_in,
                             geo_node_curve_primitive_quadratic_bezier_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_quadratic_bezier_exec;
  nodeRegisterType(&ntype);
}

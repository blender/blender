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

static bNodeSocketTemplate geo_node_curve_primitive_spiral_in[] = {
    {SOCK_INT, N_("Resolution"), 32.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1024.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Rotations"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_FLOAT},
    {SOCK_FLOAT, N_("Start Radius"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("End Radius"), 2.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("Height"), 2.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_DISTANCE},
    {SOCK_BOOLEAN, N_("Reverse")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_primitive_spiral_out[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {-1, ""},
};

namespace blender::nodes {

static std::unique_ptr<CurveEval> create_spiral_curve(const float rotations,
                                                      const int resolution,
                                                      const float start_radius,
                                                      const float end_radius,
                                                      const float height,
                                                      const bool direction)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  const int totalpoints = resolution * rotations;
  const float delta_radius = (end_radius - start_radius) / (float)totalpoints;
  float radius = start_radius;
  const float delta_height = height / (float)totalpoints;
  const float delta_theta = (M_PI * 2 * rotations) / (float)totalpoints;
  float theta = 0.0f;

  for (const int i : IndexRange(totalpoints + 1)) {
    const float x = radius * cos(theta);
    const float y = radius * sin(theta);
    const float z = delta_height * i;

    spline->add_point(float3(x, y, z), 1.0f, 0.0f);

    radius += delta_radius;

    if (direction) {
      theta += delta_theta;
    }
    else {
      theta -= delta_theta;
    }
  }

  spline->attributes.reallocate(spline->size());
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void geo_node_curve_primitive_spiral_exec(GeoNodeExecParams params)
{
  const float rotations = std::max(params.extract_input<float>("Rotations"), 0.0f);
  if (rotations == 0.0f) {
    params.set_output("Curve", GeometrySet());
    return;
  }

  std::unique_ptr<CurveEval> curve = create_spiral_curve(
      rotations,
      std::max(params.extract_input<int>("Resolution"), 1),
      params.extract_input<float>("Start Radius"),
      params.extract_input<float>("End Radius"),
      params.extract_input<float>("Height"),
      params.extract_input<bool>("Reverse"));
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_spiral()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_SPIRAL, "Spiral", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_curve_primitive_spiral_in, geo_node_curve_primitive_spiral_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_spiral_exec;
  nodeRegisterType(&ntype);
}

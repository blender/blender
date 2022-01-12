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

namespace blender::nodes::node_geo_curve_primitive_quadratic_bezier_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Resolution"))
      .default_value(16)
      .min(3)
      .max(256)
      .subtype(PROP_UNSIGNED)
      .description(N_("The number of edges on the curve"));
  b.add_input<decl::Vector>(N_("Start"))
      .default_value({-1.0f, 0.0f, 0.0f})
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the first control point"));
  b.add_input<decl::Vector>(N_("Middle"))
      .default_value({0.0f, 2.0f, 0.0f})
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the middle control point"));
  b.add_input<decl::Vector>(N_("End"))
      .default_value({1.0f, 0.0f, 0.0f})
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the last control point"));
  b.add_output<decl::Geometry>(N_("Curve"));
}

static std::unique_ptr<CurveEval> create_quadratic_bezier_curve(const float3 p1,
                                                                const float3 p2,
                                                                const float3 p3,
                                                                const int resolution)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  spline->resize(resolution + 1);
  MutableSpan<float3> positions = spline->positions();
  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);

  const float step = 1.0f / resolution;
  for (const int i : IndexRange(resolution + 1)) {
    const float factor = step * i;
    const float3 q1 = math::interpolate(p1, p2, factor);
    const float3 q2 = math::interpolate(p2, p3, factor);
    positions[i] = math::interpolate(q1, q2, factor);
  }

  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  std::unique_ptr<CurveEval> curve = create_quadratic_bezier_curve(
      params.extract_input<float3>("Start"),
      params.extract_input<float3>("Middle"),
      params.extract_input<float3>("End"),
      std::max(params.extract_input<int>("Resolution"), 3));
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes::node_geo_curve_primitive_quadratic_bezier_cc

void register_node_type_geo_curve_primitive_quadratic_bezier()
{
  namespace file_ns = blender::nodes::node_geo_curve_primitive_quadratic_bezier_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_PRIMITIVE_QUADRATIC_BEZIER, "Quadratic Bezier", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

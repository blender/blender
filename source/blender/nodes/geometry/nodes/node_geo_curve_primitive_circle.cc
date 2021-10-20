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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_curve_primitive_circle_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Resolution").default_value(32).min(3).max(512);
  b.add_input<decl::Vector>("Point 1")
      .default_value({-1.0f, 0.0f, 0.0f})
      .subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>("Point 2").default_value({0.0f, 1.0f, 0.0f}).subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>("Point 3").default_value({1.0f, 0.0f, 0.0f}).subtype(PROP_TRANSLATION);
  b.add_input<decl::Float>("Radius").default_value(1.0f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>("Curve");
  b.add_output<decl::Vector>("Center");
}

static void geo_node_curve_primitive_circle_layout(uiLayout *layout,
                                                   bContext *UNUSED(C),
                                                   PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_curve_primitive_circle_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurvePrimitiveCircle *data = (NodeGeometryCurvePrimitiveCircle *)MEM_callocN(
      sizeof(NodeGeometryCurvePrimitiveCircle), __func__);

  data->mode = GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_RADIUS;
  node->storage = data;
}

static void geo_node_curve_primitive_circle_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeGeometryCurvePrimitiveCircle *node_storage = (NodeGeometryCurvePrimitiveCircle *)
                                                             node->storage;
  const GeometryNodeCurvePrimitiveCircleMode mode = (const GeometryNodeCurvePrimitiveCircleMode)
                                                        node_storage->mode;

  bNodeSocket *start_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *middle_socket = start_socket->next;
  bNodeSocket *end_socket = middle_socket->next;
  bNodeSocket *radius_socket = end_socket->next;

  bNodeSocket *center_socket = ((bNodeSocket *)node->outputs.first)->next;

  nodeSetSocketAvailability(start_socket, mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS);
  nodeSetSocketAvailability(middle_socket, mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS);
  nodeSetSocketAvailability(end_socket, mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS);
  nodeSetSocketAvailability(center_socket, mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS);
  nodeSetSocketAvailability(radius_socket, mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_RADIUS);
}

static bool colinear_f3_f3_f3(const float3 p1, const float3 p2, const float3 p3)
{
  const float3 a = (p2 - p1).normalized();
  const float3 b = (p3 - p1).normalized();
  return (a == b || a == b * -1.0f);
}

static std::unique_ptr<CurveEval> create_point_circle_curve(
    const float3 p1, const float3 p2, const float3 p3, const int resolution, float3 &r_center)
{
  if (colinear_f3_f3_f3(p1, p2, p3)) {
    r_center = float3(0);
    return nullptr;
  }

  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  spline->resize(resolution);
  MutableSpan<float3> positions = spline->positions();

  float3 center;
  /* Midpoints of `P1->P2` and `P2->P3`. */
  const float3 q1 = float3::interpolate(p1, p2, 0.5f);
  const float3 q2 = float3::interpolate(p2, p3, 0.5f);

  /* Normal Vectors of `P1->P2` and `P2->P3` */
  const float3 v1 = (p2 - p1).normalized();
  const float3 v2 = (p3 - p2).normalized();

  /* Normal of plane of main 2 segments P1->P2 and `P2->P3`. */
  const float3 v3 = float3::cross(v1, v2).normalized();

  /* Normal of plane of first perpendicular bisector and `P1->P2`. */
  const float3 v4 = float3::cross(v3, v1).normalized();

  /* Determine Center-point from the intersection of 3 planes. */
  float plane_1[4], plane_2[4], plane_3[4];
  plane_from_point_normal_v3(plane_1, q1, v3);
  plane_from_point_normal_v3(plane_2, q1, v1);
  plane_from_point_normal_v3(plane_3, q2, v2);

  /* If the 3 planes do not intersect at one point, just return empty geometry. */
  if (!isect_plane_plane_plane_v3(plane_1, plane_2, plane_3, center)) {
    r_center = float3(0);
    return nullptr;
  }

  /* Get the radius from the center-point to p1. */
  const float r = float3::distance(p1, center);
  const float theta_step = ((2 * M_PI) / (float)resolution);
  for (const int i : IndexRange(resolution)) {

    /* Formula for a circle around a point and 2 unit vectors perpendicular
     * to each other and the axis of the circle from:
     * https://math.stackexchange.com/questions/73237/parametric-equation-of-a-circle-in-3d-space
     */

    const float theta = theta_step * i;
    positions[i] = center + r * sin(theta) * v1 + r * cos(theta) * v4;
  }

  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  spline->set_cyclic(true);
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());

  r_center = center;
  return curve;
}

static std::unique_ptr<CurveEval> create_radius_circle_curve(const int resolution,
                                                             const float radius)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  spline->resize(resolution);
  MutableSpan<float3> positions = spline->positions();

  const float theta_step = (2.0f * M_PI) / float(resolution);
  for (int i : IndexRange(resolution)) {
    const float theta = theta_step * i;
    const float x = radius * cos(theta);
    const float y = radius * sin(theta);
    positions[i] = float3(x, y, 0.0f);
  }
  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  spline->set_cyclic(true);
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void geo_node_curve_primitive_circle_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveCircle *node_storage =
      (NodeGeometryCurvePrimitiveCircle *)params.node().storage;

  const GeometryNodeCurvePrimitiveCircleMode mode = (GeometryNodeCurvePrimitiveCircleMode)
                                                        node_storage->mode;

  std::unique_ptr<CurveEval> curve;
  if (mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS) {
    float3 center_point;
    curve = create_point_circle_curve(params.extract_input<float3>("Point 1"),
                                      params.extract_input<float3>("Point 2"),
                                      params.extract_input<float3>("Point 3"),
                                      std::max(params.extract_input<int>("Resolution"), 3),
                                      center_point);
    params.set_output("Center", center_point);
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_RADIUS) {
    curve = create_radius_circle_curve(std::max(params.extract_input<int>("Resolution"), 3),
                                       params.extract_input<float>("Radius"));
  }

  if (curve) {
    params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
  }
  else {
    params.set_output("Curve", GeometrySet());
  }
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_circle()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_PRIMITIVE_CIRCLE, "Curve Circle", NODE_CLASS_GEOMETRY, 0);

  node_type_init(&ntype, blender::nodes::geo_node_curve_primitive_circle_init);
  node_type_update(&ntype, blender::nodes::geo_node_curve_primitive_circle_update);
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveCircle",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_curve_primitive_circle_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_circle_exec;
  ntype.draw_buttons = blender::nodes::geo_node_curve_primitive_circle_layout;
  nodeRegisterType(&ntype);
}

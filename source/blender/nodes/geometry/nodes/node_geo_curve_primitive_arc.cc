/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <numeric>

#include "BLI_math_base_safe.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "BKE_curves.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_primitive_arc_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurvePrimitiveArc)

static void node_declare(NodeDeclarationBuilder &b)
{
  auto enable_points = [](bNode &node) {
    node_storage(node).mode = GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_POINTS;
  };
  auto enable_radius = [](bNode &node) {
    node_storage(node).mode = GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS;
  };

  b.add_input<decl::Int>("Resolution")
      .default_value(16)
      .min(2)
      .max(256)
      .subtype(PROP_UNSIGNED)
      .description("The number of points on the arc");
  b.add_input<decl::Vector>("Start")
      .default_value({-1.0f, 0.0f, 0.0f})
      .subtype(PROP_TRANSLATION)
      .description("Position of the first control point")
      .make_available(enable_points);
  b.add_input<decl::Vector>("Middle")
      .default_value({0.0f, 2.0f, 0.0f})
      .subtype(PROP_TRANSLATION)
      .description("Position of the middle control point")
      .make_available(enable_points);
  b.add_input<decl::Vector>("End")
      .default_value({1.0f, 0.0f, 0.0f})
      .subtype(PROP_TRANSLATION)
      .description("Position of the last control point")
      .make_available(enable_points);
  b.add_input<decl::Float>("Radius")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Distance of the points from the origin")
      .make_available(enable_radius);
  b.add_input<decl::Float>("Start Angle")
      .default_value(0.0f)
      .subtype(PROP_ANGLE)
      .description("Starting angle of the arc")
      .make_available(enable_radius);
  b.add_input<decl::Float>("Sweep Angle")
      .default_value(1.75f * M_PI)
      .min(-2 * M_PI)
      .max(2 * M_PI)
      .subtype(PROP_ANGLE)
      .description("Length of the arc")
      .make_available(enable_radius);
  b.add_input<decl::Float>("Offset Angle")
      .default_value(0.0f)
      .subtype(PROP_ANGLE)
      .description("Offset angle of the arc")
      .make_available(enable_points);
  b.add_input<decl::Bool>("Connect Center")
      .default_value(false)
      .description("Connect the arc at the center");
  b.add_input<decl::Bool>("Invert Arc")
      .default_value(false)
      .description("Invert and draw opposite arc");

  b.add_output<decl::Geometry>("Curve");
  b.add_output<decl::Vector>("Center")
      .description("The center of the circle described by the three points")
      .make_available(enable_points);
  b.add_output<decl::Vector>("Normal")
      .description(
          "The normal direction of the plane described by the three points, pointing "
          "towards the positive Z axis")
      .make_available(enable_points);
  b.add_output<decl::Float>("Radius")
      .description("The radius of the circle described by the three points")
      .make_available(enable_points);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurvePrimitiveArc *data = MEM_cnew<NodeGeometryCurvePrimitiveArc>(__func__);

  data->mode = GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurvePrimitiveArc &storage = node_storage(*node);
  const GeometryNodeCurvePrimitiveArcMode mode = (GeometryNodeCurvePrimitiveArcMode)storage.mode;

  bNodeSocket *start_socket = static_cast<bNodeSocket *>(node->inputs.first)->next;
  bNodeSocket *middle_socket = start_socket->next;
  bNodeSocket *end_socket = middle_socket->next;

  bNodeSocket *radius_socket = end_socket->next;
  bNodeSocket *start_angle_socket = radius_socket->next;
  bNodeSocket *sweep_angle_socket = start_angle_socket->next;

  bNodeSocket *offset_angle_socket = sweep_angle_socket->next;

  bNodeSocket *center_out_socket = static_cast<bNodeSocket *>(node->outputs.first)->next;
  bNodeSocket *normal_out_socket = center_out_socket->next;
  bNodeSocket *radius_out_socket = normal_out_socket->next;

  const bool radius_mode = (mode == GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS);
  const bool points_mode = (mode == GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_POINTS);

  bke::nodeSetSocketAvailability(ntree, start_socket, points_mode);
  bke::nodeSetSocketAvailability(ntree, middle_socket, points_mode);
  bke::nodeSetSocketAvailability(ntree, end_socket, points_mode);

  bke::nodeSetSocketAvailability(ntree, radius_socket, radius_mode);
  bke::nodeSetSocketAvailability(ntree, start_angle_socket, radius_mode);
  bke::nodeSetSocketAvailability(ntree, sweep_angle_socket, radius_mode);

  bke::nodeSetSocketAvailability(ntree, offset_angle_socket, points_mode);

  bke::nodeSetSocketAvailability(ntree, center_out_socket, points_mode);
  bke::nodeSetSocketAvailability(ntree, normal_out_socket, points_mode);
  bke::nodeSetSocketAvailability(ntree, radius_out_socket, points_mode);
}

static float3 rotate_vector_around_axis(const float3 vector, const float3 axis, const float angle)
{
  float3 result = vector;
  float mat[3][3];
  axis_angle_to_mat3(mat, axis, angle);
  mul_m3_v3(mat, result);
  return result;
}

static bool colinear_f3_f3_f3(const float3 p1, const float3 p2, const float3 p3)
{
  const float3 a = math::normalize(p2 - p1);
  const float3 b = math::normalize(p3 - p1);
  return ELEM(a, b, b * -1.0f);
}

static Curves *create_arc_curve_from_points(const int resolution,
                                            const float3 a,
                                            const float3 b,
                                            const float3 c,
                                            float angle_offset,
                                            const bool connect_center,
                                            const bool invert_arc,
                                            float3 &r_center,
                                            float3 &r_normal,
                                            float &r_radius)
{
  const int size = connect_center ? resolution + 1 : resolution;
  Curves *curves_id = bke::curves_new_nomain_single(size, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  const int stepcount = resolution - 1;
  const int centerpoint = resolution;
  MutableSpan<float3> positions = curves.positions_for_write();

  const bool is_colinear = colinear_f3_f3_f3(a, b, c);

  float3 center;
  float3 normal;
  float radius;
  const float3 mid_ac = math::midpoint(a, c);
  normal_tri_v3(normal, a, c, b);

  if (is_colinear || a == c || a == b || b == c || resolution == 2) {
    /* If colinear, generate a point line between points. */
    float3 p1, p2;

    /* Find the two points that are furthest away from each other. */
    const float ab = math::distance_squared(a, b);
    const float ac = math::distance_squared(a, c);
    const float bc = math::distance_squared(b, c);
    if (ab > ac && ab > bc) {
      p1 = a;
      p2 = b;
    }
    else if (bc > ab && bc > ac) {
      p1 = b;
      p2 = c;
    }
    else {
      p1 = a;
      p2 = c;
    }

    const float step = 1.0f / stepcount;
    for (const int i : IndexRange(resolution)) {
      const float factor = step * i;
      positions[i] = math::interpolate(p1, p2, factor);
    }
    center = mid_ac;
    radius = 0.0f;
  }
  else {
    /* Midpoints of `A->B` and `B->C`. */
    const float3 mid_ab = math::midpoint(a, b);
    const float3 mid_bc = math::midpoint(c, b);

    /* Normalized vectors of `A->B` and `B->C`. */
    const float3 nba = math::normalize(b - a);
    const float3 ncb = math::normalize(c - b);

    /* Normal of plane of main 2 segments A->B and `B->C`. */
    const float3 nabc = math::normalize(math::cross(nba, ncb));

    /* Determine center point from the intersection of 3 planes. */
    float plane_1[4], plane_2[4], plane_3[4];
    plane_from_point_normal_v3(plane_1, mid_ab, nabc);
    plane_from_point_normal_v3(plane_2, mid_ab, nba);
    plane_from_point_normal_v3(plane_3, mid_bc, ncb);

    /* If the 3 planes do not intersect at one point, just return empty geometry. */
    if (!isect_plane_plane_plane_v3(plane_1, plane_2, plane_3, center)) {
      r_center = mid_ac;
      r_normal = normal;
      r_radius = 0.0f;
      return nullptr;
    }

    /* Radial vectors. */
    const float3 rad_a = math::normalize(a - center);
    const float3 rad_b = math::normalize(b - center);
    const float3 rad_c = math::normalize(c - center);

    /* Calculate angles. */
    radius = math::distance(center, b);
    float angle_ab = angle_signed_on_axis_v3v3_v3(rad_a, rad_b, normal) + 2.0f * M_PI;
    float angle_ac = angle_signed_on_axis_v3v3_v3(rad_a, rad_c, normal) + 2.0f * M_PI;
    float angle = (angle_ac > angle_ab) ? angle_ac : angle_ab;
    angle -= 2.0f * M_PI;
    if (invert_arc) {
      angle = -(2.0f * M_PI - angle);
    }

    /* Create arc. */
    const float step = angle / stepcount;
    for (const int i : IndexRange(resolution)) {
      const float factor = step * i + angle_offset;
      float3 out = rotate_vector_around_axis(rad_a, -normal, factor);
      positions[i] = out * radius + center;
    }
  }

  if (connect_center) {
    curves.cyclic_for_write().first() = true;
    positions[centerpoint] = center;
  }

  /* Ensure normal is relative to Z-up. */
  if (math::dot(float3(0, 0, 1), normal) < 0) {
    normal = -normal;
  }

  r_center = center;
  r_radius = radius;
  r_normal = normal;
  return curves_id;
}

static Curves *create_arc_curve_from_radius(const int resolution,
                                            const float radius,
                                            const float start_angle,
                                            const float sweep_angle,
                                            const bool connect_center,
                                            const bool invert_arc)
{
  const int size = connect_center ? resolution + 1 : resolution;
  Curves *curves_id = bke::curves_new_nomain_single(size, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  const int stepcount = resolution - 1;
  const int centerpoint = resolution;
  MutableSpan<float3> positions = curves.positions_for_write();

  const float sweep = (invert_arc) ? -(2.0f * M_PI - sweep_angle) : sweep_angle;

  const float theta_step = sweep / float(stepcount);
  for (const int i : IndexRange(resolution)) {
    const float theta = theta_step * i + start_angle;
    const float x = radius * cos(theta);
    const float y = radius * sin(theta);
    positions[i] = float3(x, y, 0.0f);
  }

  if (connect_center) {
    curves.cyclic_for_write().first() = true;
    positions[centerpoint] = float3(0.0f, 0.0f, 0.0f);
  }

  return curves_id;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveArc &storage = node_storage(params.node());

  const GeometryNodeCurvePrimitiveArcMode mode = (GeometryNodeCurvePrimitiveArcMode)storage.mode;

  switch (mode) {
    case GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_POINTS: {
      float3 r_center, r_normal;
      float r_radius;
      Curves *curves = create_arc_curve_from_points(
          std::max(params.extract_input<int>("Resolution"), 2),
          params.extract_input<float3>("Start"),
          params.extract_input<float3>("Middle"),
          params.extract_input<float3>("End"),
          params.extract_input<float>("Offset Angle"),
          params.extract_input<bool>("Connect Center"),
          params.extract_input<bool>("Invert Arc"),
          r_center,
          r_normal,
          r_radius);
      params.set_output("Curve", GeometrySet::from_curves(curves));
      params.set_output("Center", r_center);
      params.set_output("Normal", r_normal);
      params.set_output("Radius", r_radius);
      break;
    }
    case GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS: {
      Curves *curves = create_arc_curve_from_radius(
          std::max(params.extract_input<int>("Resolution"), 2),
          params.extract_input<float>("Radius"),
          params.extract_input<float>("Start Angle"),
          params.extract_input<float>("Sweep Angle"),
          params.extract_input<bool>("Connect Center"),
          params.extract_input<bool>("Invert Arc"));

      params.set_output("Curve", GeometrySet::from_curves(curves));
      break;
    }
  }
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_POINTS,
       "POINTS",
       ICON_NONE,
       "Points",
       "Define arc by 3 points on circle. Arc is calculated between start and end points"},
      {GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS,
       "RADIUS",
       ICON_NONE,
       "Radius",
       "Define radius with a float"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Method used to determine radius and placement",
                    mode_items,
                    NOD_storage_enum_accessors(mode),
                    GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS);
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_ARC, "Arc", NODE_CLASS_GEOMETRY);
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveArc",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_primitive_arc_cc

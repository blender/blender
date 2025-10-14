/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 * Operator for creating bézier splines.
 */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_report.hh"

#include "BLI_array_utils.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DEG_depsgraph.hh"

#include "DNA_material_types.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"

namespace blender::ed::curves {

namespace pen_tool {

enum class PenModal : int8_t {
  /* Move the handles of the adjacent control point. */
  MoveHandle = 0,
  /* Move the entire point even if only the handles are selected. */
  MoveEntire = 1,
  /* Snap the handles to multiples of 45 degrees. */
  SnapAngle = 2,
};

static const EnumPropertyItem prop_handle_types[] = {
    {BEZIER_HANDLE_AUTO, "AUTO", 0, "Auto", ""},
    {BEZIER_HANDLE_VECTOR, "VECTOR", 0, "Vector", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Used to scale the default select distance. */
constexpr float selection_distance_factor = 0.9f;
constexpr float selection_distance_factor_edge = 0.5f;

/* Used when creating a single curve from nothing. */
constexpr float default_handle_px_distance = 16.0f;

/* Total number of curve handle types. */
constexpr int CURVE_HANDLE_TYPES_NUM = 4;

/* Edges are prioritized less than all other types. */
constexpr float selection_edge_priority_factor = 0.1f;
/* Points will overwrite edges to allow control point to be selected easier. */
constexpr float selection_point_overwrite_edge_distance_factor = 0.7f;

constexpr float selection_point_overwrite_edge_distance_factor_sq =
    selection_point_overwrite_edge_distance_factor *
    selection_point_overwrite_edge_distance_factor;

bool ClosestElement::is_closer(const float new_distance_squared,
                               const ElementMode new_element_mode,
                               const float threshold_distance) const
{
  const float threshold_distance_sq = threshold_distance * threshold_distance;

  if (new_distance_squared > threshold_distance_sq) {
    return false;
  }

  float old_priority = 1.0f;
  float new_priority = 1.0f;

  if (this->element_mode == ElementMode::Edge) {
    if (new_element_mode != ElementMode::Edge) {
      old_priority = selection_edge_priority_factor;

      /* Overwrite edges with points if the point is within the overwrite distance. */
      if (new_distance_squared <
          threshold_distance_sq * selection_point_overwrite_edge_distance_factor_sq)
      {
        return true;
      }
    }
  }
  else {
    if (new_element_mode == ElementMode::Edge) {
      new_priority = selection_edge_priority_factor;

      /* Overwrite edges with points if the point is within the overwrite distance. */
      if (this->distance_squared <
          threshold_distance_sq * selection_point_overwrite_edge_distance_factor_sq)
      {
        return false;
      }
    }
  }

  if (new_distance_squared * old_priority < this->distance_squared * new_priority) {
    return true;
  }

  return false;
}

/* Will check if the point is closer than the existing element. */
static void pen_find_closest_point(const PenToolOperation &ptd,
                                   const bke::CurvesGeometry &curves,
                                   const IndexMask &editable_curves,
                                   const float4x4 &layer_to_object,
                                   const int drawing_index,
                                   const float2 &mouse_co,
                                   ClosestElement &r_closest_element)
{
  const Span<float3> positions = curves.positions();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  editable_curves.foreach_index([&](const int curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    for (const int point_i : points) {
      const float2 pos_proj = ptd.layer_to_screen(layer_to_object, positions[point_i]);
      const float distance_squared = math::distance_squared(pos_proj, mouse_co);

      /* Save the closest point. */
      if (r_closest_element.is_closer(
              distance_squared, ElementMode::Point, ptd.threshold_distance))
      {
        r_closest_element.curve_index = curve_i;
        r_closest_element.point_index = point_i;
        r_closest_element.element_mode = ElementMode::Point;
        r_closest_element.distance_squared = distance_squared;
        r_closest_element.drawing_index = drawing_index;
      }
    }
  });
}

/* Will check if the handle is closer than the existing element. */
static void pen_find_closest_handle(const PenToolOperation &ptd,
                                    const bke::CurvesGeometry &curves,
                                    const IndexMask &bezier_points,
                                    const float4x4 &layer_to_object,
                                    const int drawing_index,
                                    const float2 &mouse_co,
                                    ClosestElement &r_closest_element)
{
  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  const Span<float3> handle_left = *curves.handle_positions_left();
  const Span<float3> handle_right = *curves.handle_positions_right();

  bezier_points.foreach_index([&](const int point_i) {
    const float2 pos_proj = ptd.layer_to_screen(layer_to_object, handle_left[point_i]);
    const float distance_squared = math::distance_squared(pos_proj, mouse_co);

    /* Save the closest point. */
    if (r_closest_element.is_closer(
            distance_squared, ElementMode::HandleLeft, ptd.threshold_distance))
    {
      r_closest_element.curve_index = point_to_curve_map[point_i];
      r_closest_element.point_index = point_i;
      r_closest_element.element_mode = ElementMode::HandleLeft;
      r_closest_element.distance_squared = distance_squared;
      r_closest_element.drawing_index = drawing_index;
    }
  });

  bezier_points.foreach_index([&](const int point_i) {
    const float2 pos_proj = ptd.layer_to_screen(layer_to_object, handle_right[point_i]);
    const float distance_squared = math::distance_squared(pos_proj, mouse_co);

    /* Save the closest point. */
    if (r_closest_element.is_closer(
            distance_squared, ElementMode::HandleRight, ptd.threshold_distance))
    {
      r_closest_element.curve_index = point_to_curve_map[point_i];
      r_closest_element.point_index = point_i;
      r_closest_element.element_mode = ElementMode::HandleRight;
      r_closest_element.distance_squared = distance_squared;
      r_closest_element.drawing_index = drawing_index;
    }
  });
}

static float2 line_segment_closest_point(const float2 &pos_1,
                                         const float2 &pos_2,
                                         const float2 &pos,
                                         float &r_local_t)
{
  const float2 dif_m = pos - pos_1;
  const float2 dif_l = pos_2 - pos_1;
  const float d = math::dot(dif_m, dif_l);
  const float l2 = math::dot(dif_l, dif_l);
  const float t = math::clamp(d / l2, 0.0f, 1.0f);
  r_local_t = t;
  return dif_l * t + pos_1;
}

/* Will check if the edge point is closer than the existing element. */
static void pen_find_closest_edge_point(const PenToolOperation &ptd,
                                        const bke::CurvesGeometry &curves,
                                        const IndexMask &editable_curves,
                                        const float4x4 &layer_to_object,
                                        const int drawing_index,
                                        const float2 &mouse_co,
                                        ClosestElement &r_closest_element)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const OffsetIndices<int> evaluated_points_by_curve = curves.evaluated_points_by_curve();
  const Span<float3> positions = curves.positions();
  const Span<float3> evaluated_positions = curves.evaluated_positions();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<int8_t> types = curves.curve_types();

  editable_curves.foreach_index([&](const int curve_i) {
    const IndexRange src_points = points_by_curve[curve_i];
    const IndexRange eval_points = evaluated_points_by_curve[curve_i];

    for (const int src_i : src_points.index_range().drop_back(cyclic[curve_i] ? 0 : 1)) {
      if (types[curve_i] != CURVE_TYPE_BEZIER) {
        const int src_i_1 = src_i + src_points.first();
        const int src_i_2 = (src_i + 1) % src_points.size() + src_points.first();
        const float2 pos_1_proj = ptd.layer_to_screen(layer_to_object, positions[src_i_1]);
        const float2 pos_2_proj = ptd.layer_to_screen(layer_to_object, positions[src_i_2]);
        float local_t;
        const float2 closest_pos = line_segment_closest_point(
            pos_1_proj, pos_2_proj, mouse_co, local_t);

        const float distance_squared = math::distance_squared(closest_pos, mouse_co);
        const float t = local_t;

        /* Save the closest point. */
        if (r_closest_element.is_closer(
                distance_squared, ElementMode::Edge, ptd.threshold_distance_edge))
        {
          r_closest_element.point_index = src_points.first() + src_i;
          r_closest_element.edge_t = t;
          r_closest_element.element_mode = ElementMode::Edge;
          r_closest_element.curve_index = curve_i;
          r_closest_element.distance_squared = distance_squared;
          r_closest_element.drawing_index = drawing_index;
        }
      }
      else {
        const Span<int> offsets = curves.bezier_evaluated_offsets_for_curve(curve_i);
        const IndexRange eval_range = IndexRange::from_begin_end_inclusive(offsets[src_i],
                                                                           offsets[src_i + 1])
                                          .shift(eval_points.first());
        const int point_num = eval_range.size() - 1;

        for (const int eval_i : IndexRange(point_num)) {
          const int eval_point_i_1 = eval_range.first() + eval_i;
          const int eval_point_i_2 = (eval_range.first() + eval_i + 1 - eval_points.first()) %
                                         eval_points.size() +
                                     eval_points.first();
          const float2 pos_1_proj = ptd.layer_to_screen(layer_to_object,
                                                        evaluated_positions[eval_point_i_1]);
          const float2 pos_2_proj = ptd.layer_to_screen(layer_to_object,
                                                        evaluated_positions[eval_point_i_2]);
          float local_t;
          const float2 closest_pos = line_segment_closest_point(
              pos_1_proj, pos_2_proj, mouse_co, local_t);

          const float distance_squared = math::distance_squared(closest_pos, mouse_co);
          const float t = (eval_i + local_t) / float(point_num);

          /* Save the closest point. */
          if (r_closest_element.is_closer(
                  distance_squared, ElementMode::Edge, ptd.threshold_distance_edge))
          {
            r_closest_element.point_index = src_points.first() + src_i;
            r_closest_element.element_mode = ElementMode::Edge;
            r_closest_element.edge_t = t;
            r_closest_element.curve_index = curve_i;
            r_closest_element.distance_squared = distance_squared;
            r_closest_element.drawing_index = drawing_index;
          }
        }
      }
    }
  });
}

static ClosestElement find_closest_element(const PenToolOperation &ptd, const float2 &mouse_co)
{
  ClosestElement closest_element;
  closest_element.element_mode = ElementMode::None;

  for (const int curves_index : ptd.curves_range()) {
    const bke::CurvesGeometry &curves = ptd.get_curves(curves_index);
    const float4x4 layer_to_object = ptd.layer_to_object_per_curves[curves_index];

    IndexMaskMemory memory;
    const IndexMask bezier_points = ptd.visible_bezier_handle_points(curves_index, memory);
    const IndexMask editable_curves = ptd.editable_curves(curves_index, memory);

    pen_find_closest_point(
        ptd, curves, editable_curves, layer_to_object, curves_index, mouse_co, closest_element);
    pen_find_closest_handle(
        ptd, curves, bezier_points, layer_to_object, curves_index, mouse_co, closest_element);
    pen_find_closest_edge_point(
        ptd, curves, editable_curves, layer_to_object, curves_index, mouse_co, closest_element);
  }
  return closest_element;
}

static void pen_status_indicators(bContext *C, wmOperator *op)
{
  WorkspaceStatus status(C);
  status.opmodal(IFACE_("Snap Angle"), op->type, int(PenModal::SnapAngle));
  status.opmodal(IFACE_("Move Current Handle"), op->type, int(PenModal::MoveHandle));
  status.opmodal(IFACE_("Move Entire Point"), op->type, int(PenModal::MoveEntire));
}

/* Snaps to the closest diagonal, horizontal or vertical. */
static float2 snap_8_angles(const float2 &p)
{
  using namespace math;
  const float sin225 = sin(AngleRadian::from_degree(22.5f));
  return sign(p) * length(p) * normalize(sign(normalize(abs(p)) - sin225) + 1.0f);
}

static void move_segment(const PenToolOperation &ptd,
                         bke::CurvesGeometry &curves,
                         const float4x4 &layer_to_world)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<int8_t> handle_types_left = curves.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_right = curves.handle_types_right_for_write();
  MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();

  const int curve_i = ptd.closest_element.curve_index;
  const IndexRange points = points_by_curve[curve_i];
  const int point_i1 = ptd.closest_element.point_index;
  const int point_i2 = (ptd.closest_element.point_index + 1 - points.first()) % points.size() +
                       points.first();

  const float3 depth_point = positions[point_i1];
  const float3 Pm = ptd.screen_to_layer(layer_to_world, ptd.mouse_co, depth_point);
  const float3 P0 = positions[point_i1];
  const float3 P3 = positions[point_i2];
  const float3 p1 = handles_right[point_i1];
  const float3 p2 = handles_left[point_i2];
  const float3 k2 = p1 - p2;

  const float t = ptd.closest_element.edge_t;
  const float t_sq = t * t;
  const float t_cu = t_sq * t;
  const float one_minus_t = 1.0f - t;
  const float one_minus_t_sq = one_minus_t * one_minus_t;
  const float one_minus_t_cu = one_minus_t_sq * one_minus_t;

  /**
   * Equation of the starting Bezier Curve:
   *      => b(t) = (1-t)^3 * p0 + 3(1-t)^2 * t * p1 + 3(1-t) * t^2 * p2 + t^3 * p3
   *
   * Equation of the moved Bezier Curve:
   *      => B(t) = (1-t)^3 * P0 + 3(1-t)^2 * t * P1 + 3(1-t) * t^2 * P2 + t^3 * P3
   *
   * The moved Bezier curve has four unknowns: P0, P1, P2 and P3
   * We want the end points to stay the same so: P0 = p0 and P3 = p3
   *
   * Mouse location (Pm) should satisfy the equation Pm = B(t).
   * The last constraint used is that the vector between P1 and P2 doesn't change after moving.
   * Therefore: => k2 = p1 - p2 = P1 - P2
   *
   * Using all four equations we can solve for P1 as:
   *      => P1 = (Pm - (1-t)^3 * P0 - t^3 * P3) / (3(1-t) * t) + k2 * t
   * And P2 as:
   *      => P2 = P1 - k2
   */

  const float denom = 3.0f * one_minus_t * t;
  if (denom == 0.0f) {
    return;
  }

  const float3 P1 = (Pm - one_minus_t_cu * P0 - t_cu * P3) / denom + k2 * t;
  const float3 P2 = P1 - k2;

  handles_right[point_i1] = P1;
  handles_left[point_i2] = P2;
  handle_types_right[point_i1] = BEZIER_HANDLE_FREE;
  handle_types_left[point_i2] = BEZIER_HANDLE_FREE;

  /* Only change `Align`, Keep `Vector` and `Auto` the same. */
  if (handle_types_left[point_i1] == BEZIER_HANDLE_ALIGN) {
    handle_types_left[point_i1] = BEZIER_HANDLE_FREE;
  }
  if (handle_types_right[point_i2] == BEZIER_HANDLE_ALIGN) {
    handle_types_right[point_i2] = BEZIER_HANDLE_FREE;
  }

  curves.calculate_bezier_auto_handles();
}

static bool move_handles_in_curve(const PenToolOperation &ptd,
                                  bke::CurvesGeometry &curves,
                                  const IndexMask &selection,
                                  const float4x4 &layer_to_world,
                                  const float4x4 &layer_to_object)
{
  if (selection.is_empty()) {
    return false;
  }

  MutableSpan<float3> positions = curves.positions_for_write();
  const bke::AttributeAccessor attributes = curves.attributes();
  const Array<int> point_to_curve_map = curves.point_to_curve_map();

  MutableSpan<int8_t> handle_types_left = curves.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_right = curves.handle_types_right_for_write();
  MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();

  const VArray<bool> left_selected = *attributes.lookup_or_default<bool>(
      ".selection_handle_left", bke::AttrDomain::Point, true);
  const VArray<bool> right_selected = *attributes.lookup_or_default<bool>(
      ".selection_handle_right", bke::AttrDomain::Point, true);

  selection.foreach_index(GrainSize(2048), [&](const int64_t point_i) {
    const float3 depth_point = positions[point_i];
    float2 offset = ptd.xy - ptd.prev_xy;

    if ((ptd.move_point && !ptd.point_added &&
         !(left_selected[point_i] || right_selected[point_i])) ||
        ptd.move_entire)
    {
      const float2 pos = ptd.layer_to_screen(layer_to_object, positions[point_i]);
      const float2 pos_left = ptd.layer_to_screen(layer_to_object, handles_left[point_i]);
      const float2 pos_right = ptd.layer_to_screen(layer_to_object, handles_right[point_i]);
      positions[point_i] = ptd.screen_to_layer(layer_to_world, pos + offset, depth_point);
      handles_left[point_i] = ptd.screen_to_layer(layer_to_world, pos_left + offset, depth_point);
      handles_right[point_i] = ptd.screen_to_layer(
          layer_to_world, pos_right + offset, depth_point);
      return;
    }

    const bool is_left = !right_selected[point_i];
    if (ptd.move_handle) {
      if (is_left) {
        const float2 pos_left = ptd.layer_to_screen(layer_to_object, handles_left[point_i]);
        handles_left[point_i] = ptd.screen_to_layer(
            layer_to_world, pos_left + offset, depth_point);
      }
      else {
        const float2 pos_right = ptd.layer_to_screen(layer_to_object, handles_right[point_i]);
        handles_right[point_i] = ptd.screen_to_layer(
            layer_to_world, pos_right + offset, depth_point);
      }
      handle_types_left[point_i] = BEZIER_HANDLE_FREE;
      handle_types_right[point_i] = BEZIER_HANDLE_FREE;
      return;
    }

    const float2 center_point = ptd.layer_to_screen(layer_to_object, depth_point);
    offset = ptd.mouse_co - ptd.center_of_mass_co;

    if (ptd.snap_angle) {
      offset = snap_8_angles(offset);
    }

    /* Set both handles to be `Aligned` if this point is newly added or is
     * no longer control freely. */
    if (ptd.point_added || ptd.handle_moved) {
      handle_types_left[point_i] = BEZIER_HANDLE_ALIGN;
      handle_types_right[point_i] = BEZIER_HANDLE_ALIGN;
    }

    if (is_left) {
      if (handle_types_right[point_i] == BEZIER_HANDLE_AUTO) {
        handle_types_right[point_i] = BEZIER_HANDLE_ALIGN;
      }
      handle_types_left[point_i] = handle_types_right[point_i];
      if (handle_types_right[point_i] == BEZIER_HANDLE_VECTOR) {
        handle_types_left[point_i] = BEZIER_HANDLE_FREE;
      }

      if (ptd.point_added) {
        handles_left[point_i] = ptd.project(center_point + offset);
      }
      else {
        handles_left[point_i] = ptd.screen_to_layer(
            layer_to_world, center_point + offset, depth_point);
      }

      if (handle_types_right[point_i] == BEZIER_HANDLE_ALIGN) {
        handles_right[point_i] = 2.0f * depth_point - handles_left[point_i];
      }
    }
    else {
      if (handle_types_left[point_i] == BEZIER_HANDLE_AUTO) {
        handle_types_left[point_i] = BEZIER_HANDLE_ALIGN;
      }
      handle_types_right[point_i] = handle_types_left[point_i];
      if (handle_types_left[point_i] == BEZIER_HANDLE_VECTOR) {
        handle_types_right[point_i] = BEZIER_HANDLE_FREE;
      }

      if (ptd.point_added) {
        handles_right[point_i] = ptd.project(center_point + offset);
      }
      else {
        handles_right[point_i] = ptd.screen_to_layer(
            layer_to_world, center_point + offset, depth_point);
      }

      if (handle_types_left[point_i] == BEZIER_HANDLE_ALIGN) {
        handles_left[point_i] = 2.0f * depth_point - handles_right[point_i];
      }
    }
  });

  curves.calculate_bezier_auto_handles();

  return true;
}

static std::optional<bke::CurvesGeometry> extrude_curves(const PenToolOperation &ptd,
                                                         const bke::CurvesGeometry &src,
                                                         const float4x4 &layer_to_object,
                                                         const IndexMask editable_curves)
{
  const bke::AttributeAccessor src_attributes = src.attributes();
  const OffsetIndices<int> points_by_curve = src.points_by_curve();
  const VArray<bool> &src_cyclic = src.cyclic();
  const VArray<int8_t> types = src.curve_types();
  const int old_points_num = src.points_num();

  const VArray<bool> point_selection = *src_attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  const VArray<bool> left_selected = *src_attributes.lookup_or_default<bool>(
      ".selection_handle_left", bke::AttrDomain::Point, true);
  const VArray<bool> right_selected = *src_attributes.lookup_or_default<bool>(
      ".selection_handle_right", bke::AttrDomain::Point, true);

  Vector<int> dst_to_src_points(old_points_num);
  array_utils::fill_index_range(dst_to_src_points.as_mutable_span());

  Vector<bool> dst_selected_start(old_points_num, false);
  Vector<bool> dst_selected_center(old_points_num, false);
  Vector<bool> dst_selected_end(old_points_num, false);

  Array<int> dst_curve_counts(src.curves_num());
  offset_indices::copy_group_sizes(
      points_by_curve, src.curves_range(), dst_curve_counts.as_mutable_span());

  /* Point offset keeps track of the points inserted. */
  int point_offset = 0;
  editable_curves.foreach_index([&](const int curve_index) {
    const IndexRange curve_points = points_by_curve[curve_index];
    /* Skip cyclic curves unless they only have one point. */
    if (src_cyclic[curve_index] && curve_points.size() != 1) {
      return;
    }
    const bool is_bezier = types[curve_index] == CURVE_TYPE_BEZIER;

    bool first_selected = point_selection[curve_points.first()];
    if (is_bezier) {
      first_selected |= left_selected[curve_points.first()];
      first_selected |= right_selected[curve_points.first()];
    }

    bool last_selected = point_selection[curve_points.last()];
    if (is_bezier) {
      last_selected |= left_selected[curve_points.last()];
      last_selected |= right_selected[curve_points.last()];
    }

    if (first_selected) {
      if (curve_points.size() != 1) {
        /* Start-point extruded, we insert a new point at the beginning of the curve. */
        dst_to_src_points.insert(curve_points.first() + point_offset, curve_points.first());
        dst_selected_start.insert(curve_points.first() + point_offset, true);
        dst_selected_center.insert(curve_points.first() + point_offset, !is_bezier);
        dst_selected_end.insert(curve_points.first() + point_offset, false);
        dst_curve_counts[curve_index]++;
        point_offset++;
      }
    }

    if (last_selected) {
      /* End-point extruded, we insert a new point at the end of the curve. */
      dst_to_src_points.insert(curve_points.last() + point_offset + 1, curve_points.last());
      dst_selected_end.insert(curve_points.last() + point_offset + 1, true);
      dst_selected_center.insert(curve_points.last() + point_offset + 1, !is_bezier);
      dst_selected_start.insert(curve_points.last() + point_offset + 1, false);
      dst_curve_counts[curve_index]++;
      point_offset++;
    }
  });

  if (point_offset == 0) {
    return std::nullopt;
  }

  bke::CurvesGeometry dst(dst_to_src_points.size(), src.curves_num());
  BKE_defgroup_copy_list(&dst.vertex_group_names, &src.vertex_group_names);

  /* Setup curve offsets, based on the number of points in each curve. */
  MutableSpan<int> new_curve_offsets = dst.offsets_for_write();
  array_utils::copy(dst_curve_counts.as_span(), new_curve_offsets.drop_back(1));
  offset_indices::accumulate_counts_to_offsets(new_curve_offsets);

  bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

  /* Selection attribute. */
  bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
      dst, bke::AttrDomain::Point, bke::AttrType::Bool);
  bke::GSpanAttributeWriter selection_left = ed::curves::ensure_selection_attribute(
      dst, bke::AttrDomain::Point, bke::AttrType::Bool, ".selection_handle_left");
  bke::GSpanAttributeWriter selection_right = ed::curves::ensure_selection_attribute(
      dst, bke::AttrDomain::Point, bke::AttrType::Bool, ".selection_handle_right");
  selection_left.span.copy_from(dst_selected_start.as_span());
  selection.span.copy_from(dst_selected_center.as_span());
  selection_right.span.copy_from(dst_selected_end.as_span());
  selection_left.finish();
  selection.finish();
  selection_right.finish();

  bke::copy_attributes(
      src_attributes, bke::AttrDomain::Curve, bke::AttrDomain::Curve, {}, dst_attributes);

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         bke::attribute_filter_from_skip_ref(
                             {".selection", ".selection_handle_left", ".selection_handle_right"}),
                         dst_to_src_points,
                         dst_attributes);

  Span<float3> src_positions = src.positions();
  MutableSpan<float3> dst_positions = dst.positions_for_write();
  MutableSpan<bool> dst_cyclic = dst.cyclic_for_write();
  const Array<int> dst_point_to_curve_map = dst.point_to_curve_map();
  MutableSpan<int8_t> handle_types_left = dst.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_right = dst.handle_types_right_for_write();
  MutableSpan<float> radius = dst.radius_for_write();
  for (const int i : dst_to_src_points.index_range()) {
    if (!(dst_selected_end[i] || dst_selected_start[i])) {
      continue;
    }
    const float3 depth_point = src_positions[dst_to_src_points[i]];
    const float2 pos = ptd.layer_to_screen(layer_to_object, depth_point) - ptd.center_of_mass_co +
                       ptd.mouse_co;
    dst_positions[i] = ptd.project(pos);
    handle_types_left[i] = ptd.extrude_handle;
    handle_types_right[i] = ptd.extrude_handle;
    radius[i] = ptd.radius;
    dst_cyclic[dst_point_to_curve_map[i]] = false;
  }

  dst.update_curve_types();
  dst.calculate_bezier_auto_handles();
  if (src.nurbs_has_custom_knots()) {
    IndexMaskMemory memory;
    const VArray<int8_t> curve_types = src.curve_types();
    const VArray<int8_t> knot_modes = dst.nurbs_knots_modes();
    const OffsetIndices<int> dst_points_by_curve = dst.points_by_curve();
    const IndexMask include_curves = IndexMask::from_predicate(
        src.curves_range(), GrainSize(512), memory, [&](const int64_t curve_index) {
          return curve_types[curve_index] == CURVE_TYPE_NURBS &&
                 knot_modes[curve_index] == NURBS_KNOT_MODE_CUSTOM &&
                 points_by_curve[curve_index].size() == dst_points_by_curve[curve_index].size();
        });
    bke::curves::nurbs::update_custom_knot_modes(
        include_curves.complement(dst.curves_range(), memory),
        NURBS_KNOT_MODE_ENDPOINT,
        NURBS_KNOT_MODE_NORMAL,
        dst);
    bke::curves::nurbs::gather_custom_knots(src, include_curves, 0, dst);
  }
  return dst;
}

static void insert_point_to_curve(const PenToolOperation &ptd, bke::CurvesGeometry &src)
{
  const bke::AttributeAccessor src_attributes = src.attributes();
  const OffsetIndices<int> points_by_curve = src.points_by_curve();
  const int old_points_num = src.points_num();
  const int src_point_index = ptd.closest_element.point_index;
  const int dst_point_index = src_point_index + 1;
  const int curve_index = ptd.closest_element.curve_index;
  const IndexRange points = points_by_curve[curve_index];
  const int src_point_index_2 = (src_point_index + 1 - points.first()) % points.size() +
                                points.first();
  const int dst_point_index_2 = (dst_point_index - points.first() + 1) % (points.size() + 1) +
                                points.first();

  Vector<int> dst_to_src_points(old_points_num);
  array_utils::fill_index_range(dst_to_src_points.as_mutable_span());

  Array<int> dst_curve_counts(src.curves_num());
  offset_indices::copy_group_sizes(
      points_by_curve, src.curves_range(), dst_curve_counts.as_mutable_span());

  dst_to_src_points.insert(src_point_index + 1, src_point_index);
  dst_curve_counts[curve_index]++;

  bke::CurvesGeometry dst(dst_to_src_points.size(), src.curves_num());
  BKE_defgroup_copy_list(&dst.vertex_group_names, &src.vertex_group_names);

  /* Setup curve offsets, based on the number of points in each curve. */
  MutableSpan<int> new_curve_offsets = dst.offsets_for_write();
  array_utils::copy(dst_curve_counts.as_span(), new_curve_offsets.drop_back(1));
  offset_indices::accumulate_counts_to_offsets(new_curve_offsets);

  bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

  /* Selection attribute. */
  for (const StringRef selection_attribute_name :
       ed::curves::get_curves_selection_attribute_names(src))
  {
    bke::GSpanAttributeWriter selection_writer = ed::curves::ensure_selection_attribute(
        dst, bke::AttrDomain::Point, bke::AttrType::Bool, selection_attribute_name);
    ed::curves::fill_selection_false(selection_writer.span);
    ed::curves::fill_selection_true(selection_writer.span,
                                    IndexRange::from_single(dst_point_index));
    selection_writer.finish();
  }

  bke::copy_attributes(
      src_attributes, bke::AttrDomain::Curve, bke::AttrDomain::Curve, {}, dst_attributes);
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         bke::attribute_filter_from_skip_ref(
                             {".selection", ".selection_handle_left", ".selection_handle_right"}),
                         dst_to_src_points,
                         dst_attributes);

  Span<float3> src_positions = src.positions();
  MutableSpan<float3> dst_positions = dst.positions_for_write();
  MutableSpan<int8_t> handle_types_left = dst.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_right = dst.handle_types_right_for_write();
  const Span<float3> src_handles_left = *src.handle_positions_left();
  const Span<float3> src_handles_right = *src.handle_positions_right();
  MutableSpan<float3> dst_handles_left = dst.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_right = dst.handle_positions_right_for_write();
  handle_types_left[dst_point_index] = BEZIER_HANDLE_ALIGN;
  handle_types_right[dst_point_index] = BEZIER_HANDLE_ALIGN;

  const bke::curves::bezier::Insertion inserted_point = bke::curves::bezier::insert(
      src_positions[src_point_index],
      src_handles_right[src_point_index],
      src_handles_left[src_point_index_2],
      src_positions[src_point_index_2],
      ptd.closest_element.edge_t);

  dst_positions[dst_point_index] = inserted_point.position;
  dst_handles_left[dst_point_index] = inserted_point.left_handle;
  dst_handles_right[dst_point_index] = inserted_point.right_handle;
  dst_handles_right[dst_point_index - 1] = inserted_point.handle_prev;
  dst_handles_left[dst_point_index_2] = inserted_point.handle_next;
  handle_types_right[dst_point_index - 1] = BEZIER_HANDLE_FREE;
  handle_types_left[dst_point_index_2] = BEZIER_HANDLE_FREE;

  dst.update_curve_types();
  dst.calculate_bezier_auto_handles();
  if (src.nurbs_has_custom_knots()) {
    IndexMaskMemory memory;
    const VArray<int8_t> curve_types = src.curve_types();
    const VArray<int8_t> knot_modes = dst.nurbs_knots_modes();
    const OffsetIndices<int> dst_points_by_curve = dst.points_by_curve();
    const IndexMask include_curves = IndexMask::from_predicate(
        src.curves_range(), GrainSize(512), memory, [&](const int64_t curve_index) {
          return curve_types[curve_index] == CURVE_TYPE_NURBS &&
                 knot_modes[curve_index] == NURBS_KNOT_MODE_CUSTOM &&
                 points_by_curve[curve_index].size() == dst_points_by_curve[curve_index].size();
        });
    bke::curves::nurbs::update_custom_knot_modes(
        include_curves.complement(dst.curves_range(), memory),
        NURBS_KNOT_MODE_ENDPOINT,
        NURBS_KNOT_MODE_NORMAL,
        dst);
    bke::curves::nurbs::gather_custom_knots(src, include_curves, 0, dst);
  }

  src = std::move(dst);
}

static void add_single_point_and_curve(const PenToolOperation &ptd,
                                       bke::CurvesGeometry &curves,
                                       const float4x4 &layer_to_world)
{
  const float3 depth_point = ptd.project(ptd.mouse_co);

  ed::greasepencil::add_single_curve(curves, true);
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  Set<std::string> curve_attributes_to_skip;

  curves.positions_for_write().last() = depth_point;
  curves.curve_types_for_write().last() = CURVE_TYPE_BEZIER;
  curve_attributes_to_skip.add("curve_type");
  curves.handle_types_left_for_write().last() = ptd.extrude_handle;
  curves.handle_types_right_for_write().last() = ptd.extrude_handle;
  curves.update_curve_types();
  curves.resolution_for_write().last() = 12;
  curve_attributes_to_skip.add("resolution");

  const int material_index = ptd.vc.obact->actcol - 1;
  if (material_index != -1) {
    bke::SpanAttributeWriter<int> material_indexes = attributes.lookup_or_add_for_write_span<int>(
        "material_index",
        bke::AttrDomain::Curve,
        bke::AttributeInitVArray(VArray<int>::from_single(0, curves.curves_num())));
    material_indexes.span.last() = material_index;
    material_indexes.finish();
    curve_attributes_to_skip.add("material_index");
  }

  MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();
  handles_left.last() = ptd.screen_to_layer(
      layer_to_world, ptd.mouse_co - float2(default_handle_px_distance / 2.0f, 0.0f), depth_point);
  handles_right.last() = ptd.screen_to_layer(
      layer_to_world, ptd.mouse_co + float2(default_handle_px_distance / 2.0f, 0.0f), depth_point);
  curves.radius_for_write().last() = ptd.radius;

  for (const StringRef selection_attribute_name :
       ed::curves::get_curves_selection_attribute_names(curves))
  {
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        curves, bke::AttrDomain::Point, bke::AttrType::Bool, selection_attribute_name);

    ed::curves::fill_selection_true(selection.span,
                                    IndexRange::from_single(curves.points_range().last()));
    selection.finish();
  }

  /* Initialize the rest of the attributes with default values. */
  bke::fill_attribute_range_default(
      attributes,
      bke::AttrDomain::Point,
      bke::attribute_filter_from_skip_ref({"position",
                                           "radius",
                                           "handle_left",
                                           "handle_right",
                                           "handle_type_left",
                                           "handle_type_right",
                                           ".selection",
                                           ".selection_handle_left",
                                           ".selection_handle_right"}),
      curves.points_range().take_back(1));
  bke::fill_attribute_range_default(attributes,
                                    bke::AttrDomain::Curve,
                                    bke::attribute_filter_from_skip_ref(curve_attributes_to_skip),
                                    curves.curves_range().take_back(1));
}

static bool close_curve_and_select(const PenToolOperation &ptd,
                                   bke::CurvesGeometry &curves,
                                   const IndexRange points,
                                   const bool clear_selection)
{
  bool changed = false;

  for (const StringRef selection_attribute_name :
       ed::curves::get_curves_selection_attribute_names(curves))
  {
    bke::GSpanAttributeWriter selection_writer = ed::curves::ensure_selection_attribute(
        curves, bke::AttrDomain::Point, bke::AttrType::Bool, selection_attribute_name);

    const bool last_selected = ed::curves::has_anything_selected(
        selection_writer.span.slice(IndexRange::from_single(points.last())));
    const bool first_selected = ed::curves::has_anything_selected(
        selection_writer.span.slice(IndexRange::from_single(points.first())));

    /* Close the curve by selecting the other end point. */
    if ((ptd.closest_element.point_index == points.first() && last_selected) ||
        (ptd.closest_element.point_index == points.last() && first_selected))
    {
      curves.cyclic_for_write()[ptd.closest_element.curve_index] = true;
      curves.calculate_bezier_auto_handles();
      changed = true;
    }

    if (clear_selection) {
      ed::curves::fill_selection_false(selection_writer.span);
    }

    if (ptd.select_point) {
      if ((selection_attribute_name == ".selection" &&
           ptd.closest_element.element_mode == ElementMode::Point) ||
          (selection_attribute_name == ".selection_handle_left" &&
           ptd.closest_element.element_mode == ElementMode::HandleLeft) ||
          (selection_attribute_name == ".selection_handle_right" &&
           ptd.closest_element.element_mode == ElementMode::HandleRight))
      {

        ed::curves::fill_selection_true(selection_writer.span,
                                        IndexRange::from_single(ptd.closest_element.point_index));
        changed = true;
      }
    }

    selection_writer.finish();
  }

  return changed;
}

static float2 calculate_center_of_mass(const PenToolOperation &ptd, const bool ends_only)
{
  float2 pos = float2(0.0f, 0.0f);
  int num = 0;

  for (const int curves_index : ptd.curves_range()) {
    const bke::CurvesGeometry &curves = ptd.get_curves(curves_index);
    const float4x4 &layer_to_object = ptd.layer_to_object_per_curves[curves_index];
    const Span<float3> positions = curves.positions();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const Array<int> point_to_curve_map = curves.point_to_curve_map();
    const VArray<bool> &cyclic = curves.cyclic();

    IndexMaskMemory memory;
    const IndexMask selection = ptd.all_selected_points(curves_index, memory);

    selection.foreach_index([&](const int64_t point_i) {
      if (ends_only) {
        const int curve_i = point_to_curve_map[point_i];
        const IndexRange points = points_by_curve[curve_i];

        /* Skip cyclic curves unless they only have one point. */
        if (cyclic[curve_i] && points.size() != 1) {
          return;
        }

        if (point_i != points.first() && point_i != points.last()) {
          return;
        }
      }
      pos += ptd.layer_to_screen(layer_to_object, positions[point_i]);
      num++;
    });
  }

  if (num == 0) {
    return pos;
  }
  return pos / num;
}

static void invoke_curves(PenToolOperation &ptd, bContext *C, wmOperator *op, const wmEvent *event)
{
  ptd.center_of_mass_co = calculate_center_of_mass(ptd, true);
  ptd.closest_element = find_closest_element(ptd, ptd.mouse_co);

  std::atomic<bool> add_single = ptd.extrude_point;
  std::atomic<bool> changed = false;
  std::atomic<bool> point_added = false;
  std::atomic<bool> point_removed = false;

  threading::parallel_for(ptd.curves_range(), 1, [&](const IndexRange curves_range) {
    for (const int curves_index : curves_range) {
      bke::CurvesGeometry &curves = ptd.get_curves(curves_index);

      if (curves.is_empty()) {
        continue;
      }

      if (ptd.closest_element.element_mode == ElementMode::Edge) {
        add_single.store(false, std::memory_order_relaxed);
        if (ptd.insert_point && ptd.closest_element.drawing_index == curves_index) {
          insert_point_to_curve(ptd, curves);
          ptd.tag_curve_changed(curves_index);
          changed.store(true, std::memory_order_relaxed);
        }
        continue;
      }

      if (ptd.closest_element.element_mode == ElementMode::None) {
        if (ptd.extrude_point) {
          IndexMaskMemory memory;
          const IndexMask editable_curves = ptd.editable_curves(curves_index, memory);
          const float4x4 &layer_to_object = ptd.layer_to_object_per_curves[curves_index];

          if (std::optional<bke::CurvesGeometry> result = extrude_curves(
                  ptd, curves, layer_to_object, editable_curves))
          {
            curves = std::move(*result);
          }
          else {
            for (const StringRef selection_attribute_name :
                 ed::curves::get_curves_selection_attribute_names(curves))
            {
              bke::GSpanAttributeWriter selection_writer = ed::curves::ensure_selection_attribute(
                  curves, bke::AttrDomain::Point, bke::AttrType::Bool, selection_attribute_name);
              ed::curves::fill_selection_false(selection_writer.span);
              selection_writer.finish();
            }
            continue;
          }

          add_single.store(false, std::memory_order_relaxed);
          point_added.store(true, std::memory_order_relaxed);
          ptd.tag_curve_changed(curves_index);

          changed.store(true, std::memory_order_relaxed);
          continue;
        }

        continue;
      }

      if (curves_index != ptd.closest_element.drawing_index) {
        if (event->val != KM_DBL_CLICK && !ptd.delete_point) {
          for (const StringRef selection_attribute_name :
               ed::curves::get_curves_selection_attribute_names(curves))
          {
            bke::GSpanAttributeWriter selection_writer = ed::curves::ensure_selection_attribute(
                curves, bke::AttrDomain::Point, bke::AttrType::Bool, selection_attribute_name);
            ed::curves::fill_selection_false(selection_writer.span);
            selection_writer.finish();
          }
        }

        continue;
      }

      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      const IndexRange points = points_by_curve[ptd.closest_element.curve_index];

      if (event->val == KM_DBL_CLICK && ptd.cycle_handle_type) {
        const int8_t handle_type = curves.handle_types_right()[ptd.closest_element.point_index];
        /* Cycle to the next type. */
        const int8_t new_handle_type = (handle_type + 1) % CURVE_HANDLE_TYPES_NUM;

        curves.handle_types_left_for_write()[ptd.closest_element.point_index] = new_handle_type;
        curves.handle_types_right_for_write()[ptd.closest_element.point_index] = new_handle_type;
        curves.calculate_bezier_auto_handles();
        ptd.tag_curve_changed(curves_index);
        add_single.store(false, std::memory_order_relaxed);
      }

      if (ptd.delete_point) {
        curves.remove_points(IndexRange::from_single(ptd.closest_element.point_index), {});
        add_single.store(false, std::memory_order_relaxed);
        point_removed.store(true, std::memory_order_relaxed);
        ptd.tag_curve_changed(curves_index);
        continue;
      }

      const bool clear_selection = event->val != KM_DBL_CLICK && !ptd.delete_point;
      if (close_curve_and_select(ptd, curves, points, clear_selection)) {
        ptd.tag_curve_changed(curves_index);
        add_single.store(false, std::memory_order_relaxed);
      }

      changed.store(true, std::memory_order_relaxed);
    }
  });

  if (add_single) {
    if (ptd.can_create_new_curve(op)) {
      const int curves_index = *ptd.active_drawing_index;

      const float4x4 &layer_to_world = ptd.layer_to_world_per_curves[curves_index];
      bke::CurvesGeometry &curves = ptd.get_curves(curves_index);

      add_single_point_and_curve(ptd, curves, layer_to_world);
      ptd.single_point_attributes(curves, curves_index);
      ptd.tag_curve_changed(curves_index);

      changed.store(true, std::memory_order_relaxed);
      point_added = true;
    }
  }

  ptd.point_added = point_added;
  ptd.point_removed = point_removed;

  pen_status_indicators(C, op);
  if (changed) {
    ptd.update_view(C);
  }
}

static IndexMask retrieve_visible_bezier_handle_points(const bke::CurvesGeometry &curves,
                                                       const int handle_display,
                                                       IndexMaskMemory &memory)
{
  if (handle_display == CURVE_HANDLE_NONE) {
    return IndexMask(0);
  }
  else if (handle_display == CURVE_HANDLE_ALL) {
    return curves.points_range();
  }
  /* else handle_display == CURVE_HANDLE_SELECTED */

  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return IndexMask(0);
  }

  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  const VArray<int8_t> types = curves.curve_types();

  const VArray<bool> selected_point = *curves.attributes().lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  const VArray<bool> selected_left = *curves.attributes().lookup_or_default<bool>(
      ".selection_handle_left", bke::AttrDomain::Point, true);
  const VArray<bool> selected_right = *curves.attributes().lookup_or_default<bool>(
      ".selection_handle_right", bke::AttrDomain::Point, true);

  const IndexMask selected_points = IndexMask::from_predicate(
      curves.points_range(), GrainSize(4096), memory, [&](const int64_t point_i) {
        const bool is_selected = selected_point[point_i] || selected_left[point_i] ||
                                 selected_right[point_i];
        const bool is_bezier = types[point_to_curve_map[point_i]] == CURVE_TYPE_BEZIER;
        return is_selected && is_bezier;
      });

  return selected_points;
}

float2 PenToolOperation::layer_to_screen(const float4x4 &layer_to_object,
                                         const float3 &point) const
{
  return ED_view3d_project_float_v2_m4(
      vc.region, math::transform_point(layer_to_object, point), projection);
}

float3 PenToolOperation::screen_to_layer(const float4x4 &layer_to_world,
                                         const float2 &screen_co,
                                         const float3 &depth_point_layer) const
{
  const float3 depth_point = math::transform_point(layer_to_world, depth_point_layer);
  float3 proj_point;
  ED_view3d_win_to_3d(vc.v3d, vc.region, depth_point, screen_co, proj_point);
  return math::transform_point(math::invert(layer_to_world), proj_point);
}

wmOperatorStatus PenToolOperation::invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* If in tools region, wait till we get to the main (3D-space)
   * region before allowing drawing to take place. */
  op->flag |= OP_IS_MODAL_CURSOR_REGION;

  wmWindow *win = CTX_wm_window(C);
  /* Set cursor to indicate modal. */
  WM_cursor_modal_set(win, WM_CURSOR_CROSS);

  ViewContext vc = ED_view3d_viewcontext_init(C, CTX_data_depsgraph_pointer(C));

  this->vc = vc;
  this->projection = ED_view3d_ob_project_mat_get(this->vc.rv3d, this->vc.obact);

  /* Distance threshold for mouse clicks to affect the spline or its points */
  this->mouse_co = float2(event->mval);
  this->threshold_distance = ED_view3d_select_dist_px() * selection_distance_factor;
  this->threshold_distance_edge = ED_view3d_select_dist_px() * selection_distance_factor_edge;

  this->extrude_point = RNA_boolean_get(op->ptr, "extrude_point");
  this->delete_point = RNA_boolean_get(op->ptr, "delete_point");
  this->insert_point = RNA_boolean_get(op->ptr, "insert_point");
  this->move_seg = RNA_boolean_get(op->ptr, "move_segment");
  this->select_point = RNA_boolean_get(op->ptr, "select_point");
  this->move_point = RNA_boolean_get(op->ptr, "move_point");
  this->cycle_handle_type = RNA_boolean_get(op->ptr, "cycle_handle_type");
  this->extrude_handle = RNA_enum_get(op->ptr, "extrude_handle");
  this->radius = RNA_float_get(op->ptr, "radius");

  this->move_entire = false;
  this->snap_angle = false;

  this->handle_moved = false;

  if (!(ELEM(event->type, LEFTMOUSE) && ELEM(event->val, KM_PRESS, KM_DBL_CLICK))) {
    return OPERATOR_RUNNING_MODAL;
  }

  if (std::optional<wmOperatorStatus> result = this->initialize(C, op, event)) {
    return *result;
  }

  /* Add a modal handler for this operator. */
  WM_event_add_modal_handler(C, op);

  invoke_curves(*this, C, op, event);

  return OPERATOR_RUNNING_MODAL;
}

wmOperatorStatus PenToolOperation::modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  this->mouse_co = float2(event->mval);
  this->xy = float2(event->xy);
  this->prev_xy = float2(event->prev_xy);

  if (event->type == EVENT_NONE) {
    return OPERATOR_RUNNING_MODAL;
  }

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    return OPERATOR_FINISHED;
  }
  if (this->point_removed) {
    return OPERATOR_FINISHED;
  }

  if (event->type == EVT_MODAL_MAP) {
    if (event->val == int(PenModal::MoveEntire)) {
      this->move_entire = !this->move_entire;
    }
    else if (event->val == int(PenModal::SnapAngle)) {
      this->snap_angle = !this->snap_angle;
    }
    else if (event->val == int(PenModal::MoveHandle)) {
      this->move_handle = !this->move_handle;

      /* Record if handle has every been moved. */
      if (this->move_handle) {
        this->handle_moved = true;
      }
    }
  }

  std::atomic<bool> changed = false;
  this->center_of_mass_co = calculate_center_of_mass(*this, false);

  if (this->move_seg && this->closest_element.element_mode == ElementMode::Edge) {
    const int curves_index = this->closest_element.drawing_index;
    const float4x4 &layer_to_world = this->layer_to_world_per_curves[curves_index];
    bke::CurvesGeometry &curves = this->get_curves(curves_index);

    move_segment(*this, curves, layer_to_world);
    this->tag_curve_changed(curves_index);
    changed.store(true, std::memory_order_relaxed);
  }
  else {
    threading::parallel_for(this->curves_range(), 1, [&](const IndexRange curves_range) {
      for (const int curves_index : curves_range) {
        bke::CurvesGeometry &curves = this->get_curves(curves_index);
        const float4x4 &layer_to_object = this->layer_to_object_per_curves[curves_index];
        const float4x4 &layer_to_world = this->layer_to_world_per_curves[curves_index];

        IndexMaskMemory memory;
        const IndexMask selection = this->all_selected_points(curves_index, memory);

        if (move_handles_in_curve(*this, curves, selection, layer_to_world, layer_to_object)) {
          changed.store(true, std::memory_order_relaxed);
          this->tag_curve_changed(curves_index);
        }
      }
    });
  }

  pen_status_indicators(C, op);
  if (changed) {
    this->update_view(C);
  }

  /* Still running... */
  return OPERATOR_RUNNING_MODAL;
}

class CurvesPenToolOperation : public PenToolOperation {
 public:
  Vector<Curves *> all_curves;

  float3 project(const float2 &screen_co) const
  {
    const float4x4 &layer_to_world = this->layer_to_world_per_curves[*this->active_drawing_index];
    return this->screen_to_layer(layer_to_world, screen_co, float3(0.0f));
  }

  IndexMask all_selected_points(const int curves_index, IndexMaskMemory &memory) const
  {
    const Curves *curves_id = this->all_curves[curves_index];
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    return retrieve_all_selected_points(curves, this->vc.v3d->overlay.handle_display, memory);
  }

  IndexMask visible_bezier_handle_points(const int curves_index, IndexMaskMemory &memory) const
  {
    const Curves *curves_id = this->all_curves[curves_index];
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    return retrieve_visible_bezier_handle_points(
        curves, this->vc.v3d->overlay.handle_display, memory);
  }

  IndexMask editable_curves(const int curves_index, IndexMaskMemory & /*memory*/) const
  {
    const Curves *curves_id = this->all_curves[curves_index];
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    return curves.curves_range();
  }

  void tag_curve_changed(const int curves_index) const
  {
    Curves *curves_id = this->all_curves[curves_index];
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    curves.tag_topology_changed();
  }

  bke::CurvesGeometry &get_curves(const int curves_index) const
  {
    Curves *curves_id = this->all_curves[curves_index];
    return curves_id->geometry.wrap();
  }

  IndexRange curves_range() const
  {
    return this->all_curves.index_range();
  }

  void single_point_attributes(bke::CurvesGeometry & /*curves*/, const int /*curves_index*/) const
  {
    return;
  }

  bool can_create_new_curve(wmOperator *op) const
  {
    if (this->active_drawing_index == std::nullopt) {
      BKE_report(op->reports, RPT_ERROR, "No active Curves Object");
      return false;
    }

    return true;
  }

  void update_view(bContext *C) const
  {
    for (Curves *curves_id : this->all_curves) {
      DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
    }
    ED_region_tag_redraw(this->vc.region);
  }

  std::optional<wmOperatorStatus> initialize(bContext *C,
                                             wmOperator * /*op*/,
                                             const wmEvent * /*event*/)
  {
    this->active_drawing_index = std::nullopt;
    VectorSet<Curves *> unique_curves;

    const Main &bmain = *CTX_data_main(C);

    Object *object = CTX_data_active_object(C);
    if (object && object_has_editable_curves(bmain, *object)) {
      unique_curves.add_new(static_cast<Curves *>(object->data));
      this->layer_to_world_per_curves.append(object->object_to_world());
      this->active_drawing_index = 0;
    }

    CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
      if (object_has_editable_curves(bmain, *object)) {
        if (unique_curves.add(static_cast<Curves *>(object->data))) {
          this->layer_to_world_per_curves.append(object->object_to_world());
        }
      }
    }
    CTX_DATA_END;

    for (Curves *curves_id : unique_curves) {
      this->all_curves.append(curves_id);
    }

    this->layer_to_object_per_curves.append_n_times(float4x4::identity(), this->all_curves.size());

    return std::nullopt;
  }
};

/* Exit and free memory. */
static void curves_pen_exit(bContext *C, wmOperator *op)
{
  CurvesPenToolOperation *ptd = static_cast<CurvesPenToolOperation *>(op->customdata);

  /* Clear status message area. */
  ED_workspace_status_text(C, nullptr);

  WM_cursor_modal_restore(ptd->vc.win);

  ptd->update_view(C);

  MEM_delete(ptd);
  /* Clear pointer. */
  op->customdata = nullptr;
}

/* Invoke handler: Initialize the operator. */
static wmOperatorStatus curves_pen_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Allocate new data. */
  CurvesPenToolOperation *ptd_pointer = MEM_new<CurvesPenToolOperation>(__func__);
  op->customdata = ptd_pointer;
  CurvesPenToolOperation &ptd = *ptd_pointer;

  const wmOperatorStatus result = ptd.invoke(C, op, event);
  if (result != OPERATOR_RUNNING_MODAL) {
    curves_pen_exit(C, op);
  }
  return result;
}

/* Modal handler: Events handling during interactive part. */
static wmOperatorStatus curves_pen_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  CurvesPenToolOperation &ptd = *reinterpret_cast<CurvesPenToolOperation *>(op->customdata);

  const wmOperatorStatus result = ptd.modal(C, op, event);
  if (result != OPERATOR_RUNNING_MODAL) {
    curves_pen_exit(C, op);
  }
  return result;
}

void pen_tool_common_props(wmOperatorType *ot)
{
  WM_operator_properties_mouse_select(ot);

  RNA_def_boolean(ot->srna,
                  "extrude_point",
                  false,
                  "Extrude Point",
                  "Add a point connected to the last selected point");
  RNA_def_enum(ot->srna,
               "extrude_handle",
               prop_handle_types,
               BEZIER_HANDLE_VECTOR,
               "Extrude Handle Type",
               "Type of the extruded handle");
  RNA_def_boolean(ot->srna, "delete_point", false, "Delete Point", "Delete an existing point");
  RNA_def_boolean(
      ot->srna, "insert_point", false, "Insert Point", "Insert Point into a curve segment");
  RNA_def_boolean(ot->srna, "move_segment", false, "Move Segment", "Delete an existing point");
  RNA_def_boolean(
      ot->srna, "select_point", false, "Select Point", "Select a point or its handles");
  RNA_def_boolean(ot->srna, "move_point", false, "Move Point", "Move a point or its handles");
  RNA_def_boolean(ot->srna,
                  "cycle_handle_type",
                  false,
                  "Cycle Handle Type",
                  "Cycle between all four handle types");
  RNA_def_float_distance(ot->srna, "radius", 0.01f, 0.0f, FLT_MAX, "Radius", "", 0.0f, 10.0f);
}

wmKeyMap *ensure_keymap(wmKeyConfig *keyconf)
{
  using namespace blender::ed::curves::pen_tool;
  static const EnumPropertyItem modal_items[] = {
      {int(PenModal::MoveHandle),
       "MOVE_HANDLE",
       0,
       "Move Current Handle",
       "Move the current handle of the control point freely"},
      {int(PenModal::MoveEntire),
       "MOVE_ENTIRE",
       0,
       "Move Entire Point",
       "Move the entire point using its handles"},
      {int(PenModal::SnapAngle),
       "SNAP_ANGLE",
       0,
       "Snap Angle",
       "Snap the handle angle to 45 degrees"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Pen Tool Modal Map");

  /* This function is called for each space-type and both Grease Pencil and Curves, only needs to
   * add map once. */
  if (keymap && keymap->modal_items) {
    return keymap;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Pen Tool Modal Map", modal_items);

  return keymap;
}

}  // namespace pen_tool

static void CURVES_OT_pen(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Curves Pen";
  ot->idname = "CURVES_OT_pen";
  ot->description = "Construct and edit Bézier curves";

  /* Callbacks. */
  ot->invoke = pen_tool::curves_pen_invoke;
  ot->modal = pen_tool::curves_pen_modal;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  pen_tool::pen_tool_common_props(ot);
}

void ED_operatortypes_curves_pen()
{
  WM_operatortype_append(CURVES_OT_pen);
}

void ED_curves_pentool_modal_keymap(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = pen_tool::ensure_keymap(keyconf);
  WM_modalkeymap_assign(keymap, "CURVES_OT_pen");
}

}  // namespace blender::ed::curves

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 * Operators for creating new Grease Pencil primitives (boxes, circles, ...).
 */

#include <fmt/format.h>

#include <cstring>

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"
#include "BKE_paint.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "BLI_array_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::ed::greasepencil {

enum class PrimitiveType : int8_t {
  Line = 0,
  Polyline = 1,
  Arc = 2,
  Curve = 3,
  Box = 4,
  Circle = 5,
};

enum class OperatorMode : int8_t {
  Idle = 0,
  Extruding = 1,
  /* Set the active control point to the mouse. */
  Grab = 2,
  /* Move the active control point. */
  Drag = 3,
  /* Move all control points. */
  DragAll = 4,
  /* Rotate all control points. */
  RotateAll = 5,
  /* Scale all control points. */
  ScaleAll = 6,
};

enum class ControlPointType : int8_t {
  /* The points that are at the end of segments. */
  JoinPoint = 0,
  /* The points inside of the segments not including the end points. */
  HandlePoint = 1,
};

enum class ModelKeyMode : int8_t {
  Cancel = 1,
  Confirm,
  Extrude,
  Panning,
  Grab,
  Rotate,
  Scale,
  IncreaseSubdivision,
  DecreaseSubdivision,
};

static constexpr float ui_primary_point_draw_size_px = 8.0f;
static constexpr float ui_secondary_point_draw_size_px = 5.0f;
static constexpr float ui_tertiary_point_draw_size_px = 3.0f;
static constexpr float ui_point_hit_size_px = 20.0f;
static constexpr float ui_point_max_hit_size_px = 600.0f;

/* These three points are only used for `Box` and `Circle` type. */
static constexpr int control_point_first = 0;
static constexpr int control_point_center = 1;
static constexpr int control_point_last = 2;

struct PrimitiveToolOperation {
  ARegion *region;
  /* For drawing preview loop. */
  void *draw_handle;
  ViewContext vc;

  int segments;
  Vector<float3> control_points;
  /* Store the control points temporally. */
  Vector<float3> temp_control_points;
  int temp_segments;

  PrimitiveType type;
  int subdivision;
  float4x4 projection;
  /* Helper class to project screen space coordinates to 3D. */
  DrawingPlacement placement;

  bke::greasepencil::Drawing *drawing;
  BrushGpencilSettings *settings;
  float4 vertex_color;
  int material_index;
  float hardness;
  Brush *brush;

  OperatorMode mode;
  float2 start_position_2d;
  int active_control_point_index;

  ViewOpsData *vod;
};

static int control_points_per_segment(const PrimitiveToolOperation &ptd)
{
  switch (ptd.type) {
    case PrimitiveType::Polyline:
    case PrimitiveType::Line: {
      return 1;
    }
    case PrimitiveType::Box:
    case PrimitiveType::Circle:
    case PrimitiveType::Arc: {
      return 2;
    }
    case PrimitiveType::Curve: {
      return 3;
    }
  }

  BLI_assert_unreachable();
  return 0;
}

static ControlPointType get_control_point_type(const PrimitiveToolOperation &ptd, const int point)
{
  BLI_assert(point != -1);
  if (ELEM(ptd.type, PrimitiveType::Circle, PrimitiveType::Box)) {
    return ControlPointType::JoinPoint;
  }

  const int num_shared_points = control_points_per_segment(ptd);
  if (math::mod(point, num_shared_points) == 0) {
    return ControlPointType::JoinPoint;
  }
  return ControlPointType::HandlePoint;
}

static void control_point_colors_and_sizes(const PrimitiveToolOperation &ptd,
                                           MutableSpan<ColorGeometry4f> colors,
                                           MutableSpan<float> sizes)
{
  ColorGeometry4f color_gizmo_primary;
  ColorGeometry4f color_gizmo_secondary;
  ColorGeometry4f color_gizmo_a;
  ColorGeometry4f color_gizmo_b;
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color_gizmo_primary);
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color_gizmo_secondary);
  UI_GetThemeColor4fv(TH_GIZMO_A, color_gizmo_a);
  UI_GetThemeColor4fv(TH_GIZMO_B, color_gizmo_b);

  const float size_primary = ui_primary_point_draw_size_px;
  const float size_secondary = ui_secondary_point_draw_size_px;
  const float size_tertiary = ui_tertiary_point_draw_size_px;

  if (ptd.segments == 0) {
    colors.fill(color_gizmo_primary);
    sizes.fill(size_primary);
    return;
  }

  if (ELEM(ptd.type, PrimitiveType::Box, PrimitiveType::Circle)) {
    colors.fill(color_gizmo_primary);
    sizes.fill(size_primary);

    /* Set the center point's color. */
    colors[control_point_center] = color_gizmo_b;
    sizes[control_point_center] = size_secondary;
  }
  else {
    colors.fill(color_gizmo_secondary);
    sizes.fill(size_secondary);

    for (const int i : colors.index_range()) {
      const ControlPointType control_point_type = get_control_point_type(ptd, i);

      if (control_point_type == ControlPointType::JoinPoint) {
        colors[i] = color_gizmo_b;
        sizes[i] = size_tertiary;
      }
    }

    colors.last() = color_gizmo_primary;
    sizes.last() = size_primary;

    if (ELEM(ptd.type, PrimitiveType::Line, PrimitiveType::Polyline)) {
      colors.last(1) = color_gizmo_secondary;
      sizes.last(1) = size_primary;
    }
  }

  const int active_index = ptd.active_control_point_index;
  if (active_index != -1) {
    sizes[active_index] *= 1.5;
    colors[active_index] = math::interpolate(colors[active_index], color_gizmo_a, 0.5f);
  }
}

static void draw_control_points(PrimitiveToolOperation &ptd)
{
  GPUVertFormat *format3d = immVertexFormat();
  const uint pos3d = GPU_vertformat_attr_add(format3d, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  const uint col3d = GPU_vertformat_attr_add(format3d, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  const uint siz3d = GPU_vertformat_attr_add(format3d, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);

  GPU_program_point_size(true);
  immBegin(GPU_PRIM_POINTS, ptd.control_points.size());

  Array<ColorGeometry4f> colors(ptd.control_points.size());
  Array<float> sizes(ptd.control_points.size());
  control_point_colors_and_sizes(ptd, colors, sizes);

  for (const int point : ptd.control_points.index_range()) {
    const float3 point3d = ptd.control_points[point];
    const ColorGeometry4f color = colors[point];
    const float size = sizes[point];

    immAttr4f(col3d, color[0], color[1], color[2], color[3]);
    immAttr1f(siz3d, size * 2.0f);
    immVertex3fv(pos3d, point3d);
  }

  immEnd();
  immUnbindProgram();
  GPU_program_point_size(false);
}

static void grease_pencil_primitive_draw(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  PrimitiveToolOperation &ptd = *reinterpret_cast<PrimitiveToolOperation *>(arg);
  draw_control_points(ptd);
}

static void grease_pencil_primitive_save(PrimitiveToolOperation &ptd)
{
  ptd.temp_segments = ptd.segments;
  ptd.temp_control_points.resize(ptd.control_points.size());
  array_utils::copy(ptd.control_points.as_span(), ptd.temp_control_points.as_mutable_span());
}

static void grease_pencil_primitive_load(PrimitiveToolOperation &ptd)
{
  ptd.segments = ptd.temp_segments;
  ptd.control_points.resize(ptd.temp_control_points.size());
  array_utils::copy(ptd.temp_control_points.as_span(), ptd.control_points.as_mutable_span());
}

static void primitive_calulate_curve_positions(PrimitiveToolOperation &ptd,
                                               Span<float2> control_points,
                                               MutableSpan<float2> new_positions)
{
  const int subdivision = ptd.subdivision;
  const int new_points_num = new_positions.size();

  if (ptd.segments == 0) {
    new_positions.fill(control_points.last());
    return;
  }

  switch (ptd.type) {
    case PrimitiveType::Line:
    case PrimitiveType::Polyline: {
      for (const int i : new_positions.index_range().drop_back(1)) {
        const float t = math::mod(i / float(subdivision + 1), 1.0f);
        const int point = int(i / (subdivision + 1));
        const int point_next = point + 1;
        new_positions[i] = math::interpolate(control_points[point], control_points[point_next], t);
      }
      new_positions.last() = control_points.last();
      return;
    }
    case PrimitiveType::Arc: {
      const int num_shared_points = control_points_per_segment(ptd);
      const int num_segments = ptd.segments;
      for (const int segment_i : IndexRange(num_segments)) {
        const float2 A = control_points[num_shared_points * segment_i + 0];
        const float2 B = control_points[num_shared_points * segment_i + 1];
        const float2 C = control_points[num_shared_points * segment_i + 2];
        for (const int i : IndexRange(subdivision + 1)) {
          const float t = i / float(subdivision + 1);
          const float2 AB = math::interpolate(A, B, t);
          const float2 BC = math::interpolate(B, C, t);
          new_positions[i + segment_i * (subdivision + 1)] = math::interpolate(AB, BC, t);
        }
      }
      new_positions.last() = control_points.last();
      return;
    }
    case PrimitiveType::Curve: {
      const int num_shared_points = control_points_per_segment(ptd);
      const int num_segments = ptd.segments;

      for (const int segment_i : IndexRange(num_segments)) {
        const float2 A = control_points[num_shared_points * segment_i + 0];
        const float2 B = control_points[num_shared_points * segment_i + 1];
        const float2 C = control_points[num_shared_points * segment_i + 2];
        const float2 D = control_points[num_shared_points * segment_i + 3];
        for (const int i : IndexRange(subdivision + 1)) {
          const float t = i / float(subdivision + 1);
          const float2 AB = math::interpolate(A, B, t);
          const float2 BC = math::interpolate(B, C, t);
          const float2 CD = math::interpolate(C, D, t);
          const float2 ABBC = math::interpolate(AB, BC, t);
          const float2 BCCD = math::interpolate(BC, CD, t);
          new_positions[i + segment_i * (subdivision + 1)] = math::interpolate(ABBC, BCCD, t);
        }
      }
      new_positions.last() = control_points.last();
      return;
    }
    case PrimitiveType::Circle: {
      const float2 center = control_points[control_point_center];
      const float2 offset = control_points[control_point_first] - center;
      for (const int i : new_positions.index_range()) {
        const float t = i / float(new_points_num);
        const float a = t * math::numbers::pi * 2.0f;
        new_positions[i] = offset * float2(sinf(a), cosf(a)) + center;
      }
      return;
    }
    case PrimitiveType::Box: {
      const float2 center = control_points[control_point_center];
      const float2 offset = control_points[control_point_first] - center;
      /*
       * Calculate the 4 corners of the box.
       * Here's a diagram.
       *
       * +-----------+
       * |A         B|
       * |           |
       * |   center  |
       * |           |
       * |D         C|
       * +-----------+
       *
       */
      const float2 A = center + offset * float2(1.0f, 1.0f);
      const float2 B = center + offset * float2(-1.0f, 1.0f);
      const float2 C = center + offset * float2(-1.0f, -1.0f);
      const float2 D = center + offset * float2(1.0f, -1.0f);
      const float2 corners[4] = {A, B, C, D};
      for (const int i : new_positions.index_range()) {
        const float t = math::mod(i / float(subdivision + 1), 1.0f);
        const int point = int(i / (subdivision + 1));
        const int point_next = math::mod(point + 1, 4);
        new_positions[i] = math::interpolate(corners[point], corners[point_next], t);
      }
      return;
    }
  }
}

static void primitive_calulate_curve_positions_2d(PrimitiveToolOperation &ptd,
                                                  MutableSpan<float2> new_positions)
{
  Array<float2> control_points_2d(ptd.control_points.size());
  for (const int i : ptd.control_points.index_range()) {
    control_points_2d[i] = ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.control_points[i], ptd.projection);
  }

  primitive_calulate_curve_positions(ptd, control_points_2d, new_positions);
}

static int grease_pencil_primitive_curve_points_number(PrimitiveToolOperation &ptd)
{
  const int subdivision = ptd.subdivision;

  switch (ptd.type) {
    case PrimitiveType::Polyline:
    case PrimitiveType::Curve:
    case PrimitiveType::Line:
    case PrimitiveType::Circle:
    case PrimitiveType::Arc: {
      const int join_points = ptd.segments + 1;
      return join_points + subdivision * ptd.segments;
      break;
    }
    case PrimitiveType::Box: {
      return 4 + subdivision * 4;
      break;
    }
  }

  BLI_assert_unreachable();
  return 0;
}

static void grease_pencil_primitive_update_curves(PrimitiveToolOperation &ptd)
{
  bke::CurvesGeometry &curves = ptd.drawing->strokes_for_write();

  const int last_points_num = curves.points_by_curve()[curves.curves_range().last()].size();

  const int new_points_num = grease_pencil_primitive_curve_points_number(ptd);

  curves.resize(curves.points_num() - last_points_num + new_points_num, curves.curves_num());
  curves.offsets_for_write().last() = curves.points_num();
  const IndexRange curve_points = curves.points_by_curve()[curves.curves_range().last()];

  MutableSpan<float3> positions_3d = curves.positions_for_write().slice(curve_points);
  Array<float2> positions_2d(new_points_num);

  primitive_calulate_curve_positions_2d(ptd, positions_2d);
  ptd.placement.project(positions_2d, positions_3d);

  MutableSpan<float> new_radii = ptd.drawing->radii_for_write().slice(curve_points);
  MutableSpan<float> new_opacities = ptd.drawing->opacities_for_write().slice(curve_points);
  MutableSpan<ColorGeometry4f> new_vertex_colors = ptd.drawing->vertex_colors_for_write().slice(
      curve_points);

  new_vertex_colors.fill(ColorGeometry4f(ptd.vertex_color));

  const ToolSettings *ts = ptd.vc.scene->toolsettings;
  const GP_Sculpt_Settings *gset = &ts->gp_sculpt;

  for (const int point : curve_points.index_range()) {
    float pressure = 1.0f;
    /* Apply pressure curve. */
    if (gset->flag & GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE) {
      const float t = point / float(new_points_num - 1);
      pressure = BKE_curvemapping_evaluateF(gset->cur_primitive, 0, t);
    }

    const float radius = ed::greasepencil::radius_from_input_sample(
        pressure, positions_3d[point], ptd.vc, ptd.brush, ptd.vc.scene, ptd.settings);
    const float opacity = ed::greasepencil::opacity_from_input_sample(
        pressure, ptd.brush, ptd.vc.scene, ptd.settings);

    new_radii[point] = radius;
    new_opacities[point] = opacity;
  }

  ptd.drawing->tag_topology_changed();
}

static void grease_pencil_primitive_init_curves(PrimitiveToolOperation &ptd)
{
  /* Resize the curves geometry so there is one more curve with a single point. */
  bke::CurvesGeometry &curves = ptd.drawing->strokes_for_write();
  const int num_old_points = curves.points_num();
  curves.resize(curves.points_num() + 1, curves.curves_num() + 1);
  curves.offsets_for_write().last(1) = num_old_points;

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<int> materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<bool> cyclic = attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<float> hardnesses = attributes.lookup_or_add_for_write_span<float>(
      "hardness",
      bke::AttrDomain::Curve,
      bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, curves.curves_num())));

  /* Only set the attribute if the type is not the default or if it already exists. */
  if (ptd.settings->caps_type != GP_STROKE_CAP_TYPE_ROUND || attributes.contains("start_cap")) {
    bke::SpanAttributeWriter<int8_t> start_caps = attributes.lookup_or_add_for_write_span<int8_t>(
        "start_cap", bke::AttrDomain::Curve);
    start_caps.span.last() = ptd.settings->caps_type;
    start_caps.finish();
  }

  if (ptd.settings->caps_type != GP_STROKE_CAP_TYPE_ROUND || attributes.contains("end_cap")) {
    bke::SpanAttributeWriter<int8_t> end_caps = attributes.lookup_or_add_for_write_span<int8_t>(
        "end_cap", bke::AttrDomain::Curve);
    end_caps.span.last() = ptd.settings->caps_type;
    end_caps.finish();
  }

  const bool is_cyclic = ELEM(ptd.type, PrimitiveType::Box, PrimitiveType::Circle);
  cyclic.span.last() = is_cyclic;
  materials.span.last() = ptd.material_index;
  hardnesses.span.last() = ptd.hardness;

  cyclic.finish();
  materials.finish();
  hardnesses.finish();

  curves.curve_types_for_write().last() = CURVE_TYPE_POLY;
  curves.update_curve_types();

  /* Initialize the rest of the attributes with default values. */
  bke::fill_attribute_range_default(attributes,
                                    bke::AttrDomain::Point,
                                    {"position", "radius", "opacity", "vertex_color"},
                                    curves.points_range().take_back(1));
  bke::fill_attribute_range_default(
      attributes,
      bke::AttrDomain::Curve,
      {"curve_type", "material_index", "cyclic", "hardness", "start_cap", "end_cap"},
      curves.curves_range().take_back(1));

  grease_pencil_primitive_update_curves(ptd);
}

static void grease_pencil_primitive_undo_curves(PrimitiveToolOperation &ptd)
{
  bke::CurvesGeometry &curves = ptd.drawing->strokes_for_write();
  curves.remove_curves(IndexMask({curves.curves_range().last(), 1}), {});
  ptd.drawing->tag_topology_changed();
}

/* Helper: Draw status message while the user is running the operator. */
static void grease_pencil_primitive_status_indicators(bContext *C,
                                                      wmOperator *op,
                                                      PrimitiveToolOperation &ptd)
{
  std::string header;

  switch (ptd.type) {
    case PrimitiveType::Line: {
      header += RPT_("Line: ");
      break;
    }
    case (PrimitiveType::Polyline): {
      header += RPT_("Polyline: ");
      break;
    }
    case (PrimitiveType::Box): {
      header += RPT_("Rectangle: ");
      break;
    }
    case (PrimitiveType::Circle): {
      header += RPT_("Circle: ");
      break;
    }
    case (PrimitiveType::Arc): {
      header += RPT_("Arc: ");
      break;
    }
    case (PrimitiveType::Curve): {
      header += RPT_("Curve: ");
      break;
    }
  }

  auto get_modal_key_str = [&](ModelKeyMode id) {
    return WM_modalkeymap_operator_items_to_string(op->type, int(id), true).value_or("");
  };

  header += fmt::format(IFACE_("{}: confirm, {}: cancel, Shift: align"),
                        get_modal_key_str(ModelKeyMode::Confirm),
                        get_modal_key_str(ModelKeyMode::Cancel));

  header += fmt::format(IFACE_(", {}/{}: adjust subdivisions: {}"),
                        get_modal_key_str(ModelKeyMode::IncreaseSubdivision),
                        get_modal_key_str(ModelKeyMode::DecreaseSubdivision),
                        int(ptd.subdivision));

  if (ptd.segments == 1) {
    header += IFACE_(", Alt: center");
  }

  if (ELEM(ptd.type,
           PrimitiveType::Line,
           PrimitiveType::Polyline,
           PrimitiveType::Arc,
           PrimitiveType::Curve))
  {
    header += fmt::format(IFACE_(", {}: extrude"), get_modal_key_str(ModelKeyMode::Extrude));
  }

  header += fmt::format(IFACE_(", {}: grab, {}: rotate, {}: scale"),
                        get_modal_key_str(ModelKeyMode::Grab),
                        get_modal_key_str(ModelKeyMode::Rotate),
                        get_modal_key_str(ModelKeyMode::Scale));

  ED_workspace_status_text(C, header.c_str());
}

static void grease_pencil_primitive_update_view(bContext *C, PrimitiveToolOperation &ptd)
{
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(ptd.vc.obact->data);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, grease_pencil);

  ED_region_tag_redraw(ptd.region);
}

/* Invoke handler: Initialize the operator. */
static int grease_pencil_primitive_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int return_value = ed::greasepencil::grease_pencil_draw_operator_invoke(C, op);
  if (return_value != OPERATOR_RUNNING_MODAL) {
    return return_value;
  }

  /* If in tools region, wait till we get to the main (3D-space)
   * region before allowing drawing to take place. */
  op->flag |= OP_IS_MODAL_CURSOR_REGION;

  wmWindow *win = CTX_wm_window(C);

  /* Set cursor to indicate modal. */
  WM_cursor_modal_set(win, WM_CURSOR_CROSS);

  ViewContext vc = ED_view3d_viewcontext_init(C, CTX_data_depsgraph_pointer(C));

  /* Allocate new data. */
  PrimitiveToolOperation *ptd_pointer = MEM_new<PrimitiveToolOperation>(__func__);
  op->customdata = ptd_pointer;

  PrimitiveToolOperation &ptd = *ptd_pointer;

  ptd.vc = vc;
  ptd.region = vc.region;
  View3D *view3d = CTX_wm_view3d(C);
  const float2 start_coords = float2(event->mval);

  GreasePencil *grease_pencil = static_cast<GreasePencil *>(vc.obact->data);

  /* Initialize helper class for projecting screen space coordinates. */
  DrawingPlacement placement = DrawingPlacement(
      *vc.scene, *vc.region, *view3d, *vc.obact, *grease_pencil->get_active_layer());
  if (placement.use_project_to_surface()) {
    placement.cache_viewport_depths(CTX_data_depsgraph_pointer(C), vc.region, view3d);
  }
  else if (placement.use_project_to_nearest_stroke()) {
    placement.cache_viewport_depths(CTX_data_depsgraph_pointer(C), vc.region, view3d);
    placement.set_origin_to_nearest_stroke(start_coords);
  }

  ptd.placement = placement;

  ptd.vod = ED_view3d_navigation_init(C, nullptr);

  ptd.start_position_2d = start_coords;
  ptd.subdivision = RNA_int_get(op->ptr, "subdivision");
  ptd.type = PrimitiveType(RNA_enum_get(op->ptr, "type"));
  const float3 pos = ptd.placement.project(ptd.start_position_2d);
  ptd.segments = 0;
  ptd.control_points = Vector<float3>({pos});

  grease_pencil_primitive_save(ptd);

  ptd.mode = OperatorMode::Extruding;
  ptd.segments++;
  ptd.control_points.append_n_times(pos, control_points_per_segment(ptd));
  ptd.active_control_point_index = -1;
  ptd.projection = ED_view3d_ob_project_mat_get(ptd.vc.rv3d, ptd.vc.obact);

  Paint *paint = &vc.scene->toolsettings->gp_paint->paint;
  ptd.brush = BKE_paint_brush(paint);
  ptd.settings = ptd.brush->gpencil_settings;

  BKE_curvemapping_init(ptd.settings->curve_sensitivity);
  BKE_curvemapping_init(ptd.settings->curve_strength);
  BKE_curvemapping_init(ptd.settings->curve_jitter);
  BKE_curvemapping_init(ptd.settings->curve_rand_pressure);
  BKE_curvemapping_init(ptd.settings->curve_rand_strength);
  BKE_curvemapping_init(ptd.settings->curve_rand_uv);
  BKE_curvemapping_init(ptd.settings->curve_rand_hue);
  BKE_curvemapping_init(ptd.settings->curve_rand_saturation);
  BKE_curvemapping_init(ptd.settings->curve_rand_value);

  ToolSettings *ts = vc.scene->toolsettings;
  GP_Sculpt_Settings *gset = &ts->gp_sculpt;
  /* Initialize pressure curve. */
  if (gset->flag & GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE) {
    BKE_curvemapping_init(ts->gp_sculpt.cur_primitive);
  }

  Material *material = BKE_grease_pencil_object_material_ensure_from_active_input_brush(
      CTX_data_main(C), vc.obact, ptd.brush);
  ptd.material_index = BKE_object_material_index_get(vc.obact, material);

  const bool use_vertex_color = (vc.scene->toolsettings->gp_paint->mode ==
                                 GPPAINT_FLAG_USE_VERTEXCOLOR);
  const bool use_vertex_color_stroke = use_vertex_color && ELEM(ptd.settings->vertex_mode,
                                                                GPPAINT_MODE_STROKE,
                                                                GPPAINT_MODE_BOTH);
  ptd.vertex_color = use_vertex_color_stroke ? float4(ptd.brush->rgb[0],
                                                      ptd.brush->rgb[1],
                                                      ptd.brush->rgb[2],
                                                      ptd.settings->vertex_factor) :
                                               float4(0.0f);
  srgb_to_linearrgb_v4(ptd.vertex_color, ptd.vertex_color);

  /* TODO: Add UI for hardness. */
  ptd.hardness = 1.0f;

  BLI_assert(grease_pencil->has_active_layer());
  ptd.drawing = grease_pencil->get_editable_drawing_at(*grease_pencil->get_active_layer(),
                                                       vc.scene->r.cfra);

  grease_pencil_primitive_init_curves(ptd);
  grease_pencil_primitive_update_view(C, ptd);

  ptd.draw_handle = ED_region_draw_cb_activate(
      ptd.region->type, grease_pencil_primitive_draw, ptd_pointer, REGION_DRAW_POST_VIEW);

  /* Updates indicator in header. */
  grease_pencil_primitive_status_indicators(C, op, ptd);

  /* Add a modal handler for this operator. */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* Exit and free memory. */
static void grease_pencil_primitive_exit(bContext *C, wmOperator *op)
{
  PrimitiveToolOperation *ptd = static_cast<PrimitiveToolOperation *>(op->customdata);

  /* Clear status message area. */
  ED_workspace_status_text(C, nullptr);

  WM_cursor_modal_restore(ptd->vc.win);

  /* Deactivate the extra drawing stuff in 3D-View. */
  ED_region_draw_cb_exit(ptd->region->type, ptd->draw_handle);

  ED_view3d_navigation_free(C, ptd->vod);

  grease_pencil_primitive_update_view(C, *ptd);

  MEM_delete<PrimitiveToolOperation>(ptd);
  /* Clear pointer. */
  op->customdata = nullptr;
}

static float2 snap_diagonals(float2 p)
{
  using namespace math;
  return sign(p) * float2(1.0f / numbers::sqrt2) * length(p);
}

/* Using Chebyshev distance instead of Euclidean. */
static float2 snap_diagonals_box(float2 p)
{
  using namespace math;
  return sign(p) * float2(std::max(abs(p[0]), abs(p[1])));
}

/* Snaps to the closest diagonal, horizontal or vertical. */
static float2 snap_8_angles(float2 p)
{
  using namespace math;
  /* sin(pi/8) or sin of 22.5 degrees.*/
  const float sin225 = 0.3826834323650897717284599840304f;
  return sign(p) * length(p) * normalize(sign(normalize(abs(p)) - sin225) + 1.0f);
}

static void grease_pencil_primitive_extruding_update(PrimitiveToolOperation &ptd,
                                                     const wmEvent *event)
{
  using namespace math;
  const float2 start = ptd.start_position_2d;
  const float2 end = float2(event->mval);

  const float2 dif = end - start;
  float2 offset = dif;

  if (event->modifier & KM_SHIFT) {
    if (ptd.type == PrimitiveType::Box) {
      offset = snap_diagonals_box(dif);
    }
    else if (ptd.type == PrimitiveType::Circle) {
      offset = snap_diagonals(dif);
    }
    else { /* Line, Polyline, Arc and Curve. */
      offset = snap_8_angles(dif);
    }
  }
  offset *= 0.5f;

  float2 center = start + offset;

  if (event->modifier & KM_ALT && ptd.segments == 1) {
    center = start;
    offset *= 2.0f;
  }

  const float3 start_pos = ptd.placement.project(center - offset);
  const float3 end_pos = ptd.placement.project(center + offset);

  const int number_control_points = control_points_per_segment(ptd);
  for (const int i : IndexRange(number_control_points + 1)) {
    ptd.control_points.last(i) = interpolate(
        end_pos, start_pos, (i / float(number_control_points)));
  }
}

static void grease_pencil_primitive_drag_all_update(PrimitiveToolOperation &ptd,
                                                    const wmEvent *event)
{
  const float2 start = ptd.start_position_2d;
  const float2 end = float2(event->mval);
  const float2 dif = end - start;

  for (const int point_index : ptd.control_points.index_range()) {
    const float2 start_pos2 = ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.temp_control_points[point_index], ptd.projection);

    float3 pos = ptd.placement.project(start_pos2 + dif);
    ptd.control_points[point_index] = pos;
  }
}

static void grease_pencil_primitive_grab_update(PrimitiveToolOperation &ptd, const wmEvent *event)
{
  BLI_assert(ptd.active_control_point_index != -1);
  const float3 pos = ptd.placement.project(float2(event->mval));
  ptd.control_points[ptd.active_control_point_index] = pos;

  if (!ELEM(ptd.type, PrimitiveType::Circle, PrimitiveType::Box)) {
    return;
  }

  /* If the center point is been grabbed, move all points. */
  if (ptd.active_control_point_index == control_point_center) {
    grease_pencil_primitive_drag_all_update(ptd, event);
    return;
  }

  const int other_point = ptd.active_control_point_index == control_point_first ?
                              control_point_last :
                              control_point_first;

  /* Get the location of the other control point.*/
  const float2 other_point_2d = ED_view3d_project_float_v2_m4(
      ptd.vc.region, ptd.temp_control_points[other_point], ptd.projection);

  /* Set the center point to between the first and last point. */
  ptd.control_points[control_point_center] = ptd.placement.project(
      (other_point_2d + float2(event->mval)) / 2.0f);
}

static void grease_pencil_primitive_drag_update(PrimitiveToolOperation &ptd, const wmEvent *event)
{
  BLI_assert(ptd.active_control_point_index != -1);
  const float2 start = ptd.start_position_2d;
  const float2 end = float2(event->mval);
  const float2 dif = end - start;

  const float2 start_pos2 = ED_view3d_project_float_v2_m4(
      ptd.vc.region, ptd.temp_control_points[ptd.active_control_point_index], ptd.projection);

  const float3 pos = ptd.placement.project(start_pos2 + dif);
  ptd.control_points[ptd.active_control_point_index] = pos;
}

static float2 primitive_center_of_mass(const PrimitiveToolOperation &ptd)
{
  if (ELEM(ptd.type, PrimitiveType::Box, PrimitiveType::Circle)) {
    return ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.temp_control_points[control_point_center], ptd.projection);
  }
  float2 center_of_mass = float2(0.0f, 0.0f);

  for (const int point_index : ptd.control_points.index_range()) {
    center_of_mass += ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.temp_control_points[point_index], ptd.projection);
  }
  center_of_mass /= ptd.control_points.size();
  return center_of_mass;
}

static void grease_pencil_primitive_rotate_all_update(PrimitiveToolOperation &ptd,
                                                      const wmEvent *event)
{
  const float2 start = ptd.start_position_2d;
  const float2 end = float2(event->mval);

  const float2 center_of_mass = primitive_center_of_mass(ptd);

  const float2 end_ = end - center_of_mass;
  const float2 start_ = start - center_of_mass;
  const float rotation = math::atan2(start_[0], start_[1]) - math::atan2(end_[0], end_[1]);

  for (const int point_index : ptd.control_points.index_range()) {
    const float2 start_pos2 = ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.temp_control_points[point_index], ptd.projection);

    const float2 dif = start_pos2 - center_of_mass;
    const float c = math::cos(rotation);
    const float s = math::sin(rotation);
    const float2x2 rot = float2x2(float2(c, s), float2(-s, c));
    const float2 pos2 = rot * dif + center_of_mass;
    const float3 pos = ptd.placement.project(pos2);
    ptd.control_points[point_index] = pos;
  }
}

static void grease_pencil_primitive_scale_all_update(PrimitiveToolOperation &ptd,
                                                     const wmEvent *event)
{
  const float2 start = ptd.start_position_2d;
  const float2 end = float2(event->mval);

  const float2 center_of_mass = primitive_center_of_mass(ptd);

  const float scale = math::length(end - center_of_mass) / math::length(start - center_of_mass);

  for (const int point_index : ptd.control_points.index_range()) {
    const float2 start_pos2 = ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.temp_control_points[point_index], ptd.projection);

    const float2 pos2 = (start_pos2 - center_of_mass) * scale + center_of_mass;
    const float3 pos = ptd.placement.project(pos2);
    ptd.control_points[point_index] = pos;
  }
}

static int primitive_check_ui_hover(const PrimitiveToolOperation &ptd, const wmEvent *event)
{
  float closest_distance_squared = std::numeric_limits<float>::max();
  int closest_point = -1;

  for (const int i : ptd.control_points.index_range()) {
    const int point = (ptd.control_points.size() - 1) - i;
    const float2 pos_proj = ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.control_points[point], ptd.projection);
    const float radius_sq = ui_point_hit_size_px * ui_point_hit_size_px;
    const float distance_squared = math::distance_squared(pos_proj, float2(event->mval));
    /* If the mouse is over a control point. */
    if (distance_squared <= radius_sq) {
      return point;
    }

    const ControlPointType control_point_type = get_control_point_type(ptd, point);

    /* Save the closest handle point. */
    if (distance_squared < closest_distance_squared &&
        control_point_type == ControlPointType::HandlePoint &&
        distance_squared < ui_point_max_hit_size_px * ui_point_max_hit_size_px)
    {
      closest_point = point;
      closest_distance_squared = distance_squared;
    }
  }

  if (closest_point != -1) {
    return closest_point;
  }

  return -1;
}

static void grease_pencil_primitive_cursor_update(bContext *C,
                                                  PrimitiveToolOperation &ptd,
                                                  const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);

  if (ptd.mode != OperatorMode::Idle) {
    WM_cursor_modal_set(win, WM_CURSOR_CROSS);
    return;
  }

  const int ui_id = primitive_check_ui_hover(ptd, event);
  ptd.active_control_point_index = ui_id;
  if (ui_id == -1) {
    if (ptd.type == PrimitiveType::Polyline) {
      WM_cursor_modal_set(win, WM_CURSOR_CROSS);
      return;
    }

    WM_cursor_modal_set(win, WM_CURSOR_HAND);
    return;
  }

  WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  return;
}

static int grease_pencil_primitive_event_model_map(bContext *C,
                                                   wmOperator *op,
                                                   PrimitiveToolOperation &ptd,
                                                   const wmEvent *event)
{
  switch (event->val) {
    case int(ModelKeyMode::Cancel): {
      grease_pencil_primitive_undo_curves(ptd);
      grease_pencil_primitive_exit(C, op);

      return OPERATOR_CANCELLED;
    }
    case int(ModelKeyMode::Confirm): {
      grease_pencil_primitive_exit(C, op);

      return OPERATOR_FINISHED;
    }
    case int(ModelKeyMode::Extrude): {
      if (ptd.mode == OperatorMode::Idle &&
          ELEM(ptd.type, PrimitiveType::Line, PrimitiveType::Arc, PrimitiveType::Curve))
      {
        ptd.mode = OperatorMode::Extruding;
        grease_pencil_primitive_save(ptd);

        ptd.start_position_2d = ED_view3d_project_float_v2_m4(
            ptd.vc.region, ptd.control_points.last(), ptd.projection);
        const float3 pos = ptd.placement.project(ptd.start_position_2d);

        const int number_control_points = control_points_per_segment(ptd);
        ptd.control_points.append_n_times(pos, number_control_points);
        ptd.active_control_point_index = -1;
        ptd.segments++;

        return OPERATOR_RUNNING_MODAL;
      }

      if (ptd.type == PrimitiveType::Polyline &&
          ELEM(ptd.mode, OperatorMode::Idle, OperatorMode::Extruding))
      {
        ptd.mode = OperatorMode::Extruding;
        grease_pencil_primitive_save(ptd);

        ptd.start_position_2d = ED_view3d_project_float_v2_m4(
            ptd.vc.region, ptd.control_points.last(), ptd.projection);
        ptd.active_control_point_index = -1;
        const float3 pos = ptd.placement.project(float2(event->mval));

        /* If we have only two points and they're the same then don't extrude new a point. */
        if (ptd.segments == 1 &&
            math::distance_squared(ptd.control_points.first(), ptd.control_points.last()) == 0.0f)
        {
          ptd.control_points.last() = pos;
        }
        else {
          ptd.control_points.append(pos);
          ptd.segments++;
        }

        return OPERATOR_RUNNING_MODAL;
      }

      return OPERATOR_RUNNING_MODAL;
    }
    case int(ModelKeyMode::Grab): {
      if (ptd.mode == OperatorMode::Idle) {
        ptd.start_position_2d = float2(event->mval);
        ptd.mode = OperatorMode::DragAll;

        grease_pencil_primitive_save(ptd);
      }
      return OPERATOR_RUNNING_MODAL;
    }
    case int(ModelKeyMode::Rotate): {
      if (ptd.mode == OperatorMode::Idle) {
        ptd.start_position_2d = float2(event->mval);
        ptd.mode = OperatorMode::RotateAll;

        grease_pencil_primitive_save(ptd);
      }
      return OPERATOR_RUNNING_MODAL;
    }
    case int(ModelKeyMode::Scale): {
      if (ptd.mode == OperatorMode::Idle) {
        ptd.start_position_2d = float2(event->mval);
        ptd.mode = OperatorMode::ScaleAll;

        grease_pencil_primitive_save(ptd);
      }
      return OPERATOR_RUNNING_MODAL;
    }
    case int(ModelKeyMode::IncreaseSubdivision): {
      if (event->val != KM_RELEASE) {
        ptd.subdivision++;
        RNA_int_set(op->ptr, "subdivision", ptd.subdivision);
      }
      return OPERATOR_RUNNING_MODAL;
    }
    case int(ModelKeyMode::DecreaseSubdivision): {
      if (event->val != KM_RELEASE) {
        ptd.subdivision--;
        ptd.subdivision = std::max(ptd.subdivision, 0);
        RNA_int_set(op->ptr, "subdivision", ptd.subdivision);
      }
      return OPERATOR_RUNNING_MODAL;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_primitive_mouse_event(PrimitiveToolOperation &ptd, const wmEvent *event)
{
  if (event->val == KM_RELEASE && ELEM(ptd.mode,
                                       OperatorMode::Grab,
                                       OperatorMode::Drag,
                                       OperatorMode::Extruding,
                                       OperatorMode::DragAll,
                                       OperatorMode::RotateAll,
                                       OperatorMode::ScaleAll))
  {
    ptd.mode = OperatorMode::Idle;
    return OPERATOR_RUNNING_MODAL;
  }

  if (ptd.mode == OperatorMode::Idle && event->val == KM_PRESS) {
    const int ui_id = primitive_check_ui_hover(ptd, event);
    ptd.active_control_point_index = ui_id;
    if (ui_id == -1) {
      if (ptd.type != PrimitiveType::Polyline) {
        ptd.start_position_2d = float2(event->mval);
        ptd.mode = OperatorMode::DragAll;

        grease_pencil_primitive_save(ptd);

        return OPERATOR_RUNNING_MODAL;
      }
    }
    else {
      const ControlPointType control_point_type = get_control_point_type(ptd, ui_id);

      if (control_point_type == ControlPointType::JoinPoint) {
        ptd.start_position_2d = ED_view3d_project_float_v2_m4(
            ptd.vc.region, ptd.control_points[ptd.active_control_point_index], ptd.projection);
        ptd.mode = OperatorMode::Grab;

        grease_pencil_primitive_save(ptd);
      }
      else if (control_point_type == ControlPointType::HandlePoint) {
        ptd.start_position_2d = float2(event->mval);
        ptd.mode = OperatorMode::Drag;

        grease_pencil_primitive_save(ptd);
      }

      return OPERATOR_RUNNING_MODAL;
    }
  }

  if (ptd.type == PrimitiveType::Polyline && ptd.mode == OperatorMode::Idle &&
      event->val == KM_PRESS)
  {
    ptd.mode = OperatorMode::Extruding;
    grease_pencil_primitive_save(ptd);

    ptd.start_position_2d = ED_view3d_project_float_v2_m4(
        ptd.vc.region, ptd.control_points.last(), ptd.projection);
    const float3 pos = ptd.placement.project(float2(event->mval));

    /* If we have only two points and they're the same then don't extrude new a point. */
    if (ptd.segments == 1 &&
        math::distance_squared(ptd.control_points.first(), ptd.control_points.last()) == 0.0f)
    {
      ptd.control_points.last() = pos;
    }
    else {
      ptd.control_points.append(pos);
      ptd.segments++;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void grease_pencil_primitive_operator_update(PrimitiveToolOperation &ptd,
                                                    const wmEvent *event)
{
  switch (ptd.mode) {
    case OperatorMode::Extruding: {
      grease_pencil_primitive_extruding_update(ptd, event);
      break;
    }
    case OperatorMode::Grab: {
      grease_pencil_primitive_grab_update(ptd, event);
      break;
    }
    case OperatorMode::Drag: {
      grease_pencil_primitive_drag_update(ptd, event);
      break;
    }
    case OperatorMode::DragAll: {
      grease_pencil_primitive_drag_all_update(ptd, event);
      break;
    }
    case OperatorMode::ScaleAll: {
      grease_pencil_primitive_scale_all_update(ptd, event);
      break;
    }
    case OperatorMode::RotateAll: {
      grease_pencil_primitive_rotate_all_update(ptd, event);
      break;
    }
    case OperatorMode::Idle: {
      /* Do nothing. */
      break;
    }
  }
}

/* Modal handler: Events handling during interactive part. */
static int grease_pencil_primitive_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  PrimitiveToolOperation &ptd = *reinterpret_cast<PrimitiveToolOperation *>(op->customdata);

  ptd.projection = ED_view3d_ob_project_mat_get(ptd.vc.rv3d, ptd.vc.obact);
  grease_pencil_primitive_cursor_update(C, ptd, event);

  if (event->type == EVT_MODAL_MAP) {
    const int return_val = grease_pencil_primitive_event_model_map(C, op, ptd, event);
    if (return_val != OPERATOR_RUNNING_MODAL) {
      return return_val;
    }
  }

  switch (event->type) {
    case LEFTMOUSE: {
      const int return_val = grease_pencil_primitive_mouse_event(ptd, event);
      if (return_val != OPERATOR_RUNNING_MODAL) {
        return return_val;
      }

      break;
    }
    case RIGHTMOUSE: {
      if (event->val != KM_PRESS) {
        break;
      }

      if (ptd.mode == OperatorMode::Idle) {
        grease_pencil_primitive_undo_curves(ptd);
        grease_pencil_primitive_exit(C, op);

        return OPERATOR_CANCELLED;
      }
      else {
        ptd.mode = OperatorMode::Idle;

        grease_pencil_primitive_load(ptd);
        break;
      }
    }
  }

  /* Updating is done every event not just `MOUSEMOVE`. */
  grease_pencil_primitive_operator_update(ptd, event);
  grease_pencil_primitive_update_curves(ptd);

  /* Updates indicator in header. */
  grease_pencil_primitive_status_indicators(C, op, ptd);
  grease_pencil_primitive_update_view(C, ptd);

  /* Still running... */
  return OPERATOR_RUNNING_MODAL;
}

/* Cancel handler. */
static void grease_pencil_primitive_cancel(bContext *C, wmOperator *op)
{
  /* This is just a wrapper around exit() */
  grease_pencil_primitive_exit(C, op);
}

static void grease_pencil_primitive_common_props(wmOperatorType *ot,
                                                 const int default_subdiv,
                                                 const PrimitiveType default_type)
{
  static const EnumPropertyItem grease_pencil_primitive_type[] = {
      {int(PrimitiveType::Box), "BOX", 0, "Box", ""},
      {int(PrimitiveType::Line), "LINE", 0, "Line", ""},
      {int(PrimitiveType::Polyline), "POLYLINE", 0, "Polyline", ""},
      {int(PrimitiveType::Circle), "CIRCLE", 0, "Circle", ""},
      {int(PrimitiveType::Arc), "ARC", 0, "Arc", ""},
      {int(PrimitiveType::Curve), "CURVE", 0, "Curve", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_int(ot->srna,
                     "subdivision",
                     default_subdiv,
                     0,
                     INT_MAX,
                     "Subdivisions",
                     "Number of subdivisions per segment",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_enum(
      ot->srna, "type", grease_pencil_primitive_type, int(default_type), "Type", "Type of shape");
}

static void GREASE_PENCIL_OT_primitive_line(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Line Shape";
  ot->idname = "GREASE_PENCIL_OT_primitive_line";
  ot->description = "Create predefined grease pencil stroke lines";

  /* Callbacks. */
  ot->invoke = grease_pencil_primitive_invoke;
  ot->modal = grease_pencil_primitive_modal;
  ot->cancel = grease_pencil_primitive_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties and Flags. */
  grease_pencil_primitive_common_props(ot, 6, PrimitiveType::Line);
}

static void GREASE_PENCIL_OT_primitive_polyline(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Polyline Shape";
  ot->idname = "GREASE_PENCIL_OT_primitive_polyline";
  ot->description = "Create predefined grease pencil stroke polylines";

  /* Callbacks. */
  ot->invoke = grease_pencil_primitive_invoke;
  ot->modal = grease_pencil_primitive_modal;
  ot->cancel = grease_pencil_primitive_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  grease_pencil_primitive_common_props(ot, 6, PrimitiveType::Polyline);
}

static void GREASE_PENCIL_OT_primitive_arc(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Arc Shape";
  ot->idname = "GREASE_PENCIL_OT_primitive_arc";
  ot->description = "Create predefined grease pencil stroke arcs";

  /* Callbacks. */
  ot->invoke = grease_pencil_primitive_invoke;
  ot->modal = grease_pencil_primitive_modal;
  ot->cancel = grease_pencil_primitive_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  grease_pencil_primitive_common_props(ot, 62, PrimitiveType::Arc);
}

static void GREASE_PENCIL_OT_primitive_curve(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Curve Shape";
  ot->idname = "GREASE_PENCIL_OT_primitive_curve";
  ot->description = "Create predefined grease pencil stroke curve shapes";

  /* Callbacks. */
  ot->invoke = grease_pencil_primitive_invoke;
  ot->modal = grease_pencil_primitive_modal;
  ot->cancel = grease_pencil_primitive_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  grease_pencil_primitive_common_props(ot, 62, PrimitiveType::Curve);
}

static void GREASE_PENCIL_OT_primitive_box(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Box Shape";
  ot->idname = "GREASE_PENCIL_OT_primitive_box";
  ot->description = "Create predefined grease pencil stroke boxes";

  /* Callbacks. */
  ot->invoke = grease_pencil_primitive_invoke;
  ot->modal = grease_pencil_primitive_modal;
  ot->cancel = grease_pencil_primitive_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  grease_pencil_primitive_common_props(ot, 3, PrimitiveType::Box);
}

static void GREASE_PENCIL_OT_primitive_circle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Circle Shape";
  ot->idname = "GREASE_PENCIL_OT_primitive_circle";
  ot->description = "Create predefined grease pencil stroke circles";

  /* Callbacks. */
  ot->invoke = grease_pencil_primitive_invoke;
  ot->modal = grease_pencil_primitive_modal;
  ot->cancel = grease_pencil_primitive_cancel;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  grease_pencil_primitive_common_props(ot, 94, PrimitiveType::Circle);
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_primitives()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_primitive_line);
  WM_operatortype_append(GREASE_PENCIL_OT_primitive_polyline);
  WM_operatortype_append(GREASE_PENCIL_OT_primitive_arc);
  WM_operatortype_append(GREASE_PENCIL_OT_primitive_curve);
  WM_operatortype_append(GREASE_PENCIL_OT_primitive_box);
  WM_operatortype_append(GREASE_PENCIL_OT_primitive_circle);
}

void ED_primitivetool_modal_keymap(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  static const EnumPropertyItem modal_items[] = {
      {int(ModelKeyMode::Cancel), "CANCEL", 0, "Cancel", ""},
      {int(ModelKeyMode::Confirm), "CONFIRM", 0, "Confirm", ""},
      {int(ModelKeyMode::Panning), "PANNING", 0, "Panning", ""},
      {int(ModelKeyMode::Extrude), "EXTRUDE", 0, "Extrude", ""},
      {int(ModelKeyMode::Grab), "GRAB", 0, "Grab", ""},
      {int(ModelKeyMode::Rotate), "ROTATE", 0, "Rotate", ""},
      {int(ModelKeyMode::Scale), "SCALE", 0, "Scale", ""},
      {int(ModelKeyMode::IncreaseSubdivision),
       "INCREASE_SUBDIVISION",
       0,
       "increase_subdivision",
       ""},
      {int(ModelKeyMode::DecreaseSubdivision),
       "DECREASE_SUBDIVISION",
       0,
       "decrease_subdivision",
       ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Primitive Tool Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Primitive Tool Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_primitive_line");
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_primitive_polyline");
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_primitive_arc");
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_primitive_curve");
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_primitive_box");
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_primitive_circle");
}

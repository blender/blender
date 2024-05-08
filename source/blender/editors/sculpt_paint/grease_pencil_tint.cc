/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"

#include "BLI_bounds.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"

#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

using ed::greasepencil::MutableDrawingInfo;

class TintOperation : public GreasePencilStrokeOperation {
 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;

 private:
  float radius_;
  float strength_;
  bool active_layer_only_;
  ColorGeometry4f color_;
  Vector<MutableDrawingInfo> drawings_;
  Array<Array<float2>> screen_positions_per_drawing_;

  void execute_tint(const bContext &C, const InputSample &extension_sample);
};

void TintOperation::on_stroke_begin(const bContext &C, const InputSample & /*start_sample*/)
{
  using namespace blender::bke::greasepencil;
  Scene *scene = CTX_data_scene(&C);
  Paint *paint = BKE_paint_get_active_from_context(&C);
  Brush *brush = BKE_paint_brush(paint);

  BKE_curvemapping_init(brush->gpencil_settings->curve_sensitivity);
  BKE_curvemapping_init(brush->gpencil_settings->curve_strength);

  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  BLI_assert(brush->gpencil_settings != nullptr);

  BKE_curvemapping_init(brush->curve);

  radius_ = BKE_brush_size_get(scene, brush);
  strength_ = BKE_brush_alpha_get(scene, brush);
  active_layer_only_ = ((brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0);

  float4 color_linear;
  color_linear[3] = 1.0f;
  srgb_to_linearrgb_v3_v3(color_linear, brush->rgb);

  color_ = ColorGeometry4f(color_linear);

  Object *obact = CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

  if (active_layer_only_) {
    /* Tint only on the drawings of the active layer. */
    const Layer *active_layer = grease_pencil.get_active_layer();
    if (!active_layer) {
      return;
    }
    drawings_ = ed::greasepencil::retrieve_editable_drawings_from_layer(
        *scene, grease_pencil, *active_layer);
  }
  else {
    /* Tint on all editable drawings. */
    drawings_ = ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
  }

  if (drawings_.is_empty()) {
    return;
  }

  ARegion *region = CTX_wm_region(&C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, obact);

  screen_positions_per_drawing_.reinitialize(drawings_.size());

  threading::parallel_for_each(drawings_, [&](const MutableDrawingInfo &drawing_info) {
    const int drawing_index = (&drawing_info - drawings_.data());

    bke::CurvesGeometry &strokes = drawing_info.drawing.strokes_for_write();
    const Layer &layer = *grease_pencil.layer(drawing_info.layer_index);

    screen_positions_per_drawing_[drawing_index].reinitialize(strokes.points_num());

    bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
            ob_eval, *obact, drawing_info.layer_index, drawing_info.frame_number);

    for (const int point : strokes.points_range()) {
      ED_view3d_project_float_global(
          region,
          math::transform_point(layer.to_world_space(*ob_eval), deformation.positions[point]),
          screen_positions_per_drawing_[drawing_index][point],
          V3D_PROJ_TEST_NOP);
    }
  });
}

void TintOperation::execute_tint(const bContext &C, const InputSample &extension_sample)
{
  if (drawings_.is_empty()) {
    return;
  }

  using namespace blender::bke::greasepencil;
  Scene *scene = CTX_data_scene(&C);
  Object *obact = CTX_data_active_object(&C);

  Paint *paint = &scene->toolsettings->gp_paint->paint;
  Brush *brush = BKE_paint_brush(paint);

  /* Get the tool's data. */
  const float2 mouse_position = extension_sample.mouse_position;
  float radius = radius_;
  float strength = strength_;
  if (BKE_brush_use_size_pressure(brush)) {
    radius *= BKE_curvemapping_evaluateF(
        brush->gpencil_settings->curve_sensitivity, 0, extension_sample.pressure);
  }
  if (BKE_brush_use_alpha_pressure(brush)) {
    strength *= BKE_curvemapping_evaluateF(
        brush->gpencil_settings->curve_strength, 0, extension_sample.pressure);
  }
  /* Attenuate factor to get a smoother tinting. */
  float fill_strength = strength / 100.0f;

  strength = math::clamp(strength, 0.0f, 1.0f);
  fill_strength = math::clamp(fill_strength, 0.0f, 1.0f);

  const bool tint_strokes = ELEM(
      brush->gpencil_settings->vertex_mode, GPPAINT_MODE_STROKE, GPPAINT_MODE_BOTH);
  const bool tint_fills = ELEM(
      brush->gpencil_settings->vertex_mode, GPPAINT_MODE_FILL, GPPAINT_MODE_BOTH);

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

  std::atomic<bool> changed = false;
  const auto execute_tint_on_drawing = [&](Drawing &drawing, const int drawing_index) {
    bke::CurvesGeometry &strokes = drawing.strokes_for_write();

    MutableSpan<ColorGeometry4f> vertex_colors = drawing.vertex_colors_for_write();
    MutableSpan<ColorGeometry4f> fill_colors = drawing.fill_colors_for_write();
    OffsetIndices<int> points_by_curve = strokes.points_by_curve();

    const Span<float2> screen_space_positions =
        screen_positions_per_drawing_[drawing_index].as_span();

    auto point_inside_stroke = [&](const Span<float2> points, const float2 mouse) {
      std::optional<Bounds<float2>> bbox = bounds::min_max(points);
      if (!bbox.has_value()) {
        return false;
      }
      Bounds<float2> &box = bbox.value();
      if (mouse.x < box.min.x || mouse.x > box.max.x || mouse.y < box.min.y || mouse.y > box.max.y)
      {
        return false;
      }
      return isect_point_poly_v2(
          mouse, reinterpret_cast<const float(*)[2]>(points.data()), points.size());
    };

    threading::parallel_for(strokes.curves_range(), 128, [&](const IndexRange range) {
      for (const int curve : range) {
        bool stroke_touched = false;
        for (const int curve_point : points_by_curve[curve].index_range()) {
          if (tint_strokes) {
            const int point = curve_point + points_by_curve[curve].first();
            const float distance = math::distance(screen_space_positions[point], mouse_position);
            const float influence = strength * BKE_brush_curve_strength(brush, distance, radius);
            if (influence > 0.0f) {
              stroke_touched = true;
              /* Manually do an alpha-over mix, not using `ColorGeometry4f::premultiply_alpha`
               * since the vertex color in GPv3 is stored as straight alpha (which is technically
               * `ColorPaint4f`). */
              float4 premultiplied;
              straight_to_premul_v4_v4(premultiplied, vertex_colors[point]);
              float4 rgba = float4(
                  math::interpolate(float3(premultiplied), float3(color_), influence),
                  vertex_colors[point][3]);
              rgba[3] = rgba[3] * (1.0f - influence) + influence;
              premul_to_straight_v4_v4(vertex_colors[point], rgba);
            }
          }
        }
        if (tint_fills && !fill_colors.is_empty()) {
          /* Will tint fill color when either the brush being inside the fill region or touching
           * the stroke. */
          const bool fill_effective = stroke_touched ||
                                      point_inside_stroke(screen_space_positions.slice(
                                                              points_by_curve[curve].first(),
                                                              points_by_curve[curve].size()),
                                                          mouse_position);
          if (fill_effective) {
            float4 premultiplied;
            straight_to_premul_v4_v4(premultiplied, fill_colors[curve]);
            float4 rgba = float4(
                math::interpolate(float3(premultiplied), float3(color_), fill_strength),
                fill_colors[curve][3]);
            rgba[3] = rgba[3] * (1.0f - fill_strength) + fill_strength;
            premul_to_straight_v4_v4(fill_colors[curve], rgba);
            stroke_touched = true;
          }
        }
        if (stroke_touched) {
          changed.store(true, std::memory_order_relaxed);
        }
      }
    });
  };

  threading::parallel_for_each(drawings_, [&](const MutableDrawingInfo &info) {
    const int drawing_index = (&info - drawings_.data());
    execute_tint_on_drawing(info.drawing, drawing_index);
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void TintOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  execute_tint(C, extension_sample);
}

void TintOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_tint_operation()
{
  return std::make_unique<TintOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil

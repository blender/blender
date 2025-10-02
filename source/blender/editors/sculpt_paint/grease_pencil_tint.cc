/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "BLI_bounds.hh"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"

#include "DNA_brush_types.h"

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
  TintOperation(bool temp_eraser = false) : temp_eraser_(temp_eraser) {};
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;

 private:
  float radius_;
  float strength_;
  bool temp_eraser_;
  bool active_layer_only_;
  ColorGeometry4f color_;
  Vector<MutableDrawingInfo> drawings_;
  Array<Array<float2>> screen_positions_per_drawing_;

  void execute_tint(const bContext &C, const InputSample &extension_sample);

  void tint_strokes(const bke::CurvesGeometry &strokes,
                    const OffsetIndices<int> points_by_curve,
                    const Brush *brush,
                    const Span<float2> screen_space_positions,
                    const float2 mouse_position,
                    const float radius,
                    const float strength,
                    MutableSpan<ColorGeometry4f> vertex_colors,
                    MutableSpan<bool> touched_strokes);
  void tint_fills(const bke::CurvesGeometry &strokes,
                  const OffsetIndices<int> points_by_curve,
                  const Span<float2> screen_space_positions,
                  const float2 mouse_position,
                  const float fill_strength,
                  MutableSpan<ColorGeometry4f> fill_colors,
                  MutableSpan<bool> touched_strokes);
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

  BKE_curvemapping_init(brush->curve_distance_falloff);

  radius_ = brush->size / 2.0f;
  strength_ = brush->alpha;
  active_layer_only_ = ((brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0);

  float4 color_linear;
  color_linear[3] = 1.0f;
  copy_v3_v3(color_linear, BKE_brush_color_get(paint, brush));

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
  Object *ob_eval = DEG_get_evaluated(depsgraph, obact);

  screen_positions_per_drawing_.reinitialize(drawings_.size());

  threading::parallel_for_each(drawings_, [&](const MutableDrawingInfo &drawing_info) {
    const int drawing_index = (&drawing_info - drawings_.data());

    bke::CurvesGeometry &strokes = drawing_info.drawing.strokes_for_write();
    const Layer &layer = grease_pencil.layer(drawing_info.layer_index);

    screen_positions_per_drawing_[drawing_index].reinitialize(strokes.points_num());

    bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
            ob_eval, *obact, drawing_info.drawing);

    for (const int point : strokes.points_range()) {
      ED_view3d_project_float_global(
          region,
          math::transform_point(layer.to_world_space(*ob_eval), deformation.positions[point]),
          screen_positions_per_drawing_[drawing_index][point],
          V3D_PROJ_TEST_NOP);
    }
  });
}

void TintOperation::tint_strokes(const bke::CurvesGeometry &strokes,
                                 const OffsetIndices<int> points_by_curve,
                                 const Brush *brush,
                                 const Span<float2> screen_space_positions,
                                 const float2 mouse_position,
                                 const float radius,
                                 const float strength,
                                 MutableSpan<ColorGeometry4f> vertex_colors,
                                 MutableSpan<bool> touched_strokes)
{
  threading::parallel_for(strokes.curves_range(), 512, [&](const IndexRange curves) {
    for (const int curve : curves) {
      std::atomic<bool> changed = false;
      threading::parallel_for(points_by_curve[curve], 4096, [&](const IndexRange points) {
        for (const int point : points) {
          const float distance = math::distance(screen_space_positions[point], mouse_position);
          const float influence = strength * BKE_brush_curve_strength(brush, distance, radius);
          if (influence <= 0.0f) {
            continue;
          }

          if (temp_eraser_) {
            float &alpha = vertex_colors[point][3];
            alpha -= influence;
            alpha = math::max(alpha, 0.0f);
          }
          else {
            /* Manually do an alpha-over mix, not using `ColorGeometry4f::premultiply_alpha`
             * since the vertex color is stored as straight alpha (which is technically
             * `ColorPaint4f`). */
            float4 premultiplied;
            straight_to_premul_v4_v4(premultiplied, vertex_colors[point]);
            float4 rgba = float4(
                math::interpolate(float3(premultiplied), float3(color_), influence),
                vertex_colors[point][3]);
            rgba[3] = rgba[3] * (1.0f - influence) + influence;
            premul_to_straight_v4_v4(vertex_colors[point], rgba);
          }

          changed.store(true, std::memory_order_relaxed);
        }
      });

      touched_strokes[curve] = changed;
    }
  });
}

void TintOperation::tint_fills(const bke::CurvesGeometry &strokes,
                               const OffsetIndices<int> points_by_curve,
                               const Span<float2> screen_space_positions,
                               const float2 mouse_position,
                               const float fill_strength,
                               MutableSpan<ColorGeometry4f> fill_colors,
                               MutableSpan<bool> touched_strokes)
{
  auto point_inside_stroke = [](const Span<float2> points, const float2 mouse) {
    std::optional<Bounds<float2>> bbox = bounds::min_max(points);
    if (!bbox.has_value()) {
      return false;
    }
    Bounds<float2> &box = bbox.value();
    if (mouse.x < box.min.x || mouse.x > box.max.x || mouse.y < box.min.y || mouse.y > box.max.y) {
      return false;
    }
    return isect_point_poly_v2(
        mouse, reinterpret_cast<const float (*)[2]>(points.data()), points.size());
  };

  threading::parallel_for(strokes.curves_range(), 512, [&](const IndexRange curves) {
    for (const int curve : curves) {
      const IndexRange points = points_by_curve[curve];
      const bool stroke_touched = touched_strokes[curve];
      const bool fill_effective = stroke_touched ||
                                  point_inside_stroke(screen_space_positions.slice(points),
                                                      mouse_position);
      if (!fill_effective) {
        continue;
      }

      if (temp_eraser_) {
        float &alpha = fill_colors[curve][3];
        alpha -= fill_strength;
        alpha = math::max(alpha, 0.0f);
      }
      else {
        float4 premultiplied;
        straight_to_premul_v4_v4(premultiplied, fill_colors[curve]);
        float4 rgba = float4(
            math::interpolate(float3(premultiplied), float3(color_), fill_strength),
            fill_colors[curve][3]);
        rgba[3] = rgba[3] * (1.0f - fill_strength) + fill_strength;
        premul_to_straight_v4_v4(fill_colors[curve], rgba);
      }

      touched_strokes[curve] = true;
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

  /* Get the brush's data. */
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

    Array<bool> touched_strokes(strokes.curves_num(), false);
    if (tint_strokes) {
      this->tint_strokes(strokes,
                         points_by_curve,
                         brush,
                         screen_space_positions,
                         mouse_position,
                         radius,
                         strength,
                         vertex_colors,
                         touched_strokes.as_mutable_span());
    }

    if (tint_fills && !fill_colors.is_empty()) {
      this->tint_fills(strokes,
                       points_by_curve,
                       screen_space_positions,
                       mouse_position,
                       fill_strength,
                       fill_colors,
                       touched_strokes.as_mutable_span());
    }

    for (const bool touched : touched_strokes) {
      if (touched) {
        changed.store(true, std::memory_order_relaxed);
        break;
      }
    }
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

std::unique_ptr<GreasePencilStrokeOperation> new_tint_operation(bool temp_eraser)
{
  return std::make_unique<TintOperation>(temp_eraser);
}

}  // namespace blender::ed::sculpt_paint::greasepencil

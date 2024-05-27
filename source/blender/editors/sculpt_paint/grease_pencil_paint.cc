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
#include "BKE_paint.hh"
#include "BKE_scene.hh"

#include "BLI_color.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_enums.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "GEO_smooth_curves.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"

#include <optional>

namespace blender::ed::sculpt_paint::greasepencil {

static constexpr float POINT_OVERRIDE_THRESHOLD_PX = 3.0f;

template<typename T>
static inline void linear_interpolation(const T &a,
                                        const T &b,
                                        MutableSpan<T> dst,
                                        const bool include_first_point)
{
  if (include_first_point) {
    const float step = math::safe_rcp(float(dst.size() - 1));
    for (const int i : dst.index_range()) {
      dst[i] = bke::attribute_math::mix2(float(i) * step, a, b);
    }
  }
  else {
    const float step = 1.0f / float(dst.size());
    for (const int i : dst.index_range()) {
      dst[i] = bke::attribute_math::mix2(float(i + 1) * step, a, b);
    }
  }
}

static float2 arithmetic_mean(Span<float2> values)
{
  return std::accumulate(values.begin(), values.end(), float2(0)) / values.size();
}

/** Sample a bezier curve at a fixed resolution and return the sampled points in an array. */
static Array<float2> sample_curve_2d(Span<float2> positions, const int64_t resolution)
{
  BLI_assert(positions.size() % 3 == 0);
  const int64_t num_handles = positions.size() / 3;
  if (num_handles == 1) {
    return Array<float2>(resolution, positions[1]);
  }
  const int64_t num_segments = num_handles - 1;
  const int64_t num_points = num_segments * resolution;

  Array<float2> points(num_points);
  const Span<float2> curve_segments = positions.drop_front(1).drop_back(1);
  threading::parallel_for(IndexRange(num_segments), 32 * resolution, [&](const IndexRange range) {
    for (const int64_t segment_i : range) {
      IndexRange segment_range(segment_i * resolution, resolution);
      bke::curves::bezier::evaluate_segment(curve_segments[segment_i * 3 + 0],
                                            curve_segments[segment_i * 3 + 1],
                                            curve_segments[segment_i * 3 + 2],
                                            curve_segments[segment_i * 3 + 3],
                                            points.as_mutable_span().slice(segment_range));
    }
  });
  return points;
}

/**
 * Morph \a src onto \a target such that the points have the same spacing as in \a src and
 * write the result to \a dst.
 */
static void morph_points_to_curve(Span<float2> src, Span<float2> target, MutableSpan<float2> dst)
{
  BLI_assert(src.size() == dst.size());
  Array<float> accumulated_lengths_src(src.size() - 1);
  length_parameterize::accumulate_lengths<float2>(src, false, accumulated_lengths_src);

  Array<float> accumulated_lengths_target(target.size() - 1);
  length_parameterize::accumulate_lengths<float2>(target, false, accumulated_lengths_target);

  Array<int> segment_indices(accumulated_lengths_src.size());
  Array<float> segment_factors(accumulated_lengths_src.size());
  length_parameterize::sample_at_lengths(
      accumulated_lengths_target, accumulated_lengths_src, segment_indices, segment_factors);

  length_parameterize::interpolate<float2>(
      target, segment_indices, segment_factors, dst.drop_back(1));
  dst.last() = src.last();
}

class PaintOperation : public GreasePencilStrokeOperation {
 private:
  /* Screen space coordinates from input samples. */
  Vector<float2> screen_space_coords_orig_;
  /* Temporary vector of curve fitted screen space coordinates per input sample from the active
   * smoothing window. */
  Vector<Vector<float2>> screen_space_curve_fitted_coords_;
  /* Screen space coordinates after smoothing. */
  Vector<float2> screen_space_smoothed_coords_;
  /* The start index of the smoothing window. */
  int active_smooth_start_index_ = 0;
  blender::float4x2 texture_space_ = float4x2::identity();

  /* Helper class to project screen space coordinates to 3d. */
  ed::greasepencil::DrawingPlacement placement_;

  friend struct PaintOperationExecutor;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;

 private:
  void simplify_stroke(const bContext &C, bke::greasepencil::Drawing &drawing, float epsilon_px);
  void process_stroke_end(const bContext &C, bke::greasepencil::Drawing &drawing);
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct PaintOperationExecutor {
  Scene *scene_;
  GreasePencil *grease_pencil_;

  Brush *brush_;

  BrushGpencilSettings *settings_;
  std::optional<ColorGeometry4f> vertex_color_;
  std::optional<ColorGeometry4f> fill_color_;
  float softness_;

  bke::greasepencil::Drawing *drawing_;

  PaintOperationExecutor(const bContext &C)
  {
    scene_ = CTX_data_scene(&C);
    Object *object = CTX_data_active_object(&C);
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);

    Paint *paint = &scene_->toolsettings->gp_paint->paint;
    brush_ = BKE_paint_brush(paint);
    settings_ = brush_->gpencil_settings;

    const bool use_vertex_color = (scene_->toolsettings->gp_paint->mode ==
                                   GPPAINT_FLAG_USE_VERTEXCOLOR);
    if (use_vertex_color) {
      ColorGeometry4f color_base;
      srgb_to_linearrgb_v3_v3(color_base, brush_->rgb);
      color_base.a = settings_->vertex_factor;
      vertex_color_ = ELEM(settings_->vertex_mode, GPPAINT_MODE_STROKE, GPPAINT_MODE_BOTH) ?
                          std::make_optional(color_base) :
                          std::nullopt;
      fill_color_ = ELEM(settings_->vertex_mode, GPPAINT_MODE_FILL, GPPAINT_MODE_BOTH) ?
                        std::make_optional(color_base) :
                        std::nullopt;
    }
    softness_ = 1.0f - settings_->hardness;

    BLI_assert(grease_pencil->has_active_layer());
    drawing_ = grease_pencil->get_editable_drawing_at(*grease_pencil->get_active_layer(),
                                                      scene_->r.cfra);
    BLI_assert(drawing_ != nullptr);
  }

  /**
   * Creates a new curve with one point at the beginning or end.
   * Note: Does not initialize the new curve or points.
   */
  static void create_blank_curve(bke::CurvesGeometry &curves, const bool on_back)
  {
    if (!on_back) {
      const int num_old_points = curves.points_num();
      curves.resize(curves.points_num() + 1, curves.curves_num() + 1);
      curves.offsets_for_write().last(1) = num_old_points;
      return;
    }

    curves.resize(curves.points_num() + 1, curves.curves_num() + 1);
    MutableSpan<int> offsets = curves.offsets_for_write();
    offsets.first() = 0;

    /* Loop through backwards to not overwrite the data. */
    for (int i = curves.curves_num() - 2; i >= 0; i--) {
      offsets[i + 1] = offsets[i] + 1;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    attributes.for_all(
        [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData /*meta_data*/) {
          bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(id);

          GMutableSpan attribute_data = dst.span;

          bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
            using T = decltype(dummy);
            MutableSpan<T> span_data = attribute_data.typed<T>();

            /* Loop through backwards to not overwrite the data. */
            for (int i = span_data.size() - 2; i >= 0; i--) {
              span_data[i + 1] = span_data[i];
            }
          });
          dst.finish();
          return true;
        });
  }

  /**
   * Extends the first or last curve by `new_points_num` number of points.
   * Note: Does not initialize the new points.
   */
  static void extend_curve(bke::CurvesGeometry &curves,
                           const bool on_back,
                           const int new_points_num)
  {
    if (!on_back) {
      curves.resize(curves.points_num() + new_points_num, curves.curves_num());
      curves.offsets_for_write().last() = curves.points_num();
      return;
    }

    const int last_active_point = curves.points_by_curve()[0].last();

    curves.resize(curves.points_num() + new_points_num, curves.curves_num());
    MutableSpan<int> offsets = curves.offsets_for_write();

    for (const int src_curve : curves.curves_range().drop_front(1)) {
      offsets[src_curve] = offsets[src_curve] + new_points_num;
    }
    offsets.last() = curves.points_num();

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    attributes.for_all([&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
      if (meta_data.domain != bke::AttrDomain::Point) {
        return true;
      }

      bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(id);
      GMutableSpan attribute_data = dst.span;

      bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
        using T = decltype(dummy);
        MutableSpan<T> span_data = attribute_data.typed<T>();

        /* Loop through backwards to not overwrite the data. */
        for (int i = (span_data.size() - 1) - new_points_num; i >= last_active_point; i--) {
          span_data[i + new_points_num] = span_data[i];
        }
      });
      dst.finish();
      return true;
    });

    curves.tag_topology_changed();
  }

  /* Attributes that are defined explicitly and should not be copied from original geometry. */
  Set<std::string> skipped_attribute_ids(const bke::AttrDomain domain) const
  {
    switch (domain) {
      case bke::AttrDomain::Point:
        if (vertex_color_) {
          return {"position", "radius", "opacity", "vertex_color"};
        }
        else {
          return {"position", "radius", "opacity"};
        }
      case bke::AttrDomain::Curve:
        if (fill_color_) {
          return {"curve_type",
                  "material_index",
                  "cyclic",
                  "softness",
                  "start_cap",
                  "end_cap",
                  "fill_color"};
        }
        else {
          return {"curve_type", "material_index", "cyclic", "softness", "start_cap", "end_cap"};
        }
      default:
        return {};
    }
    return {};
  }

  void process_start_sample(PaintOperation &self,
                            const bContext &C,
                            const InputSample &start_sample,
                            const int material_index)
  {
    const float2 start_coords = start_sample.mouse_position;
    const RegionView3D *rv3d = CTX_wm_region_view3d(&C);
    const ARegion *region = CTX_wm_region(&C);

    const float3 start_location = self.placement_.project(start_coords);
    const float start_radius = ed::greasepencil::radius_from_input_sample(
        rv3d,
        region,
        scene_,
        brush_,
        start_sample.pressure,
        start_location,
        self.placement_.to_world_space(),
        settings_);
    const float start_opacity = ed::greasepencil::opacity_from_input_sample(
        start_sample.pressure, brush_, scene_, settings_);
    Scene *scene = CTX_data_scene(&C);
    const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;

    self.screen_space_coords_orig_.append(start_coords);
    self.screen_space_curve_fitted_coords_.append(Vector<float2>({start_coords}));
    self.screen_space_smoothed_coords_.append(start_coords);

    /* Resize the curves geometry so there is one more curve with a single point. */
    bke::CurvesGeometry &curves = drawing_->strokes_for_write();
    create_blank_curve(curves, on_back);

    const int active_curve = on_back ? curves.curves_range().first() :
                                       curves.curves_range().last();
    const IndexRange curve_points = curves.points_by_curve()[active_curve];
    const int last_active_point = curve_points.last();

    curves.positions_for_write()[last_active_point] = start_location;
    drawing_->radii_for_write()[last_active_point] = start_radius;
    drawing_->opacities_for_write()[last_active_point] = start_opacity;
    if (vertex_color_) {
      drawing_->vertex_colors_for_write()[last_active_point] = *vertex_color_;
    }
    if (fill_color_) {
      drawing_->fill_colors_for_write()[active_curve] = *fill_color_;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    bke::SpanAttributeWriter<int> materials = attributes.lookup_or_add_for_write_span<int>(
        "material_index", bke::AttrDomain::Curve);
    bke::SpanAttributeWriter<bool> cyclic = attributes.lookup_or_add_for_write_span<bool>(
        "cyclic", bke::AttrDomain::Curve);
    bke::SpanAttributeWriter<float> softness = attributes.lookup_or_add_for_write_span<float>(
        "softness", bke::AttrDomain::Curve);
    cyclic.span[active_curve] = false;
    materials.span[active_curve] = material_index;
    softness.span[active_curve] = softness_;

    /* Only set the attribute if the type is not the default or if it already exists. */
    if (settings_->caps_type != GP_STROKE_CAP_TYPE_ROUND || attributes.contains("start_cap")) {
      bke::SpanAttributeWriter<int8_t> start_caps =
          attributes.lookup_or_add_for_write_span<int8_t>("start_cap", bke::AttrDomain::Curve);
      start_caps.span[active_curve] = settings_->caps_type;
      start_caps.finish();
    }

    if (settings_->caps_type != GP_STROKE_CAP_TYPE_ROUND || attributes.contains("end_cap")) {
      bke::SpanAttributeWriter<int8_t> end_caps = attributes.lookup_or_add_for_write_span<int8_t>(
          "end_cap", bke::AttrDomain::Curve);
      end_caps.span[active_curve] = settings_->caps_type;
      end_caps.finish();
    }

    cyclic.finish();
    materials.finish();
    softness.finish();

    curves.curve_types_for_write()[active_curve] = CURVE_TYPE_POLY;
    curves.update_curve_types();

    /* Initialize the rest of the attributes with default values. */
    bke::fill_attribute_range_default(attributes,
                                      bke::AttrDomain::Point,
                                      this->skipped_attribute_ids(bke::AttrDomain::Point),
                                      IndexRange(last_active_point, 1));
    bke::fill_attribute_range_default(attributes,
                                      bke::AttrDomain::Curve,
                                      this->skipped_attribute_ids(bke::AttrDomain::Curve),
                                      IndexRange(active_curve, 1));

    drawing_->tag_topology_changed();
  }

  void active_smoothing(PaintOperation &self,
                        const IndexRange smooth_window,
                        MutableSpan<float3> curve_positions)
  {
    const Span<float2> coords_to_smooth = self.screen_space_coords_orig_.as_span().slice(
        smooth_window);

    /* Detect corners in the current slice of coordinates. */
    const float corner_min_radius_px = 5.0f;
    const float corner_max_radius_px = 30.0f;
    const int64_t corner_max_samples = 64;
    const float corner_angle_threshold = 0.6f;
    IndexMaskMemory memory;
    const IndexMask corner_mask = ed::greasepencil::polyline_detect_corners(
        coords_to_smooth.drop_front(1).drop_back(1),
        corner_min_radius_px,
        corner_max_radius_px,
        corner_max_samples,
        corner_angle_threshold,
        memory);

    /* Pre-blur the coordinates for the curve fitting. This generally leads to a better (more
     * stable) fit. */
    Array<float2> coords_pre_blur(smooth_window.size());
    const int pre_blur_iterations = 3;
    geometry::gaussian_blur_1D(
        coords_to_smooth,
        pre_blur_iterations,
        VArray<float>::ForSingle(settings_->active_smooth, smooth_window.size()),
        true,
        true,
        false,
        coords_pre_blur.as_mutable_span());

    /* Curve fitting. The output will be a set of handles (float2 triplets) in a flat array. */
    const float max_error_threshold_px = 5.0f;
    Array<float2> curve_points = ed::greasepencil::polyline_fit_curve(
        coords_pre_blur, max_error_threshold_px * settings_->active_smooth, corner_mask);

    /* Sampling the curve at a fixed resolution. */
    const int64_t sample_resolution = 32;
    Array<float2> sampled_curve_points = sample_curve_2d(curve_points, sample_resolution);

    /* Morphing the coordinates onto the curve. Result is stored in a temporary array. */
    Array<float2> coords_smoothed(coords_to_smooth.size());
    morph_points_to_curve(coords_to_smooth, sampled_curve_points, coords_smoothed);

    MutableSpan<float2> window_coords = self.screen_space_smoothed_coords_.as_mutable_span().slice(
        smooth_window);
    MutableSpan<float3> positions_slice = curve_positions.slice(smooth_window);
    const float converging_threshold_px = 0.1f;
    bool stop_counting_converged = false;
    int num_converged = 0;
    for (const int64_t window_i : smooth_window.index_range()) {
      /* Record the curve fitting of this point. */
      self.screen_space_curve_fitted_coords_[window_i].append(coords_smoothed[window_i]);
      Span<float2> fit_coords = self.screen_space_curve_fitted_coords_[window_i];

      /* We compare the previous arithmetic mean to the current. Going from the back to the front,
       * if a point hasn't moved by a minimum threshold, it counts as converged. */
      float2 new_pos = arithmetic_mean(fit_coords);
      if (!stop_counting_converged) {
        float2 prev_pos = window_coords[window_i];
        if (math::distance(new_pos, prev_pos) < converging_threshold_px) {
          num_converged++;
        }
        else {
          stop_counting_converged = true;
        }
      }

      /* Update the positions in the current cache. */
      window_coords[window_i] = new_pos;
      positions_slice[window_i] = self.placement_.project(new_pos);
    }

    /* Remove all the converged points from the active window and shrink the window accordingly. */
    if (num_converged > 0) {
      self.active_smooth_start_index_ += num_converged;
      self.screen_space_curve_fitted_coords_.remove(0, num_converged);
    }
  }

  void process_extension_sample(PaintOperation &self,
                                const bContext &C,
                                const InputSample &extension_sample)
  {
    const float2 coords = extension_sample.mouse_position;
    const RegionView3D *rv3d = CTX_wm_region_view3d(&C);
    const ARegion *region = CTX_wm_region(&C);

    const float3 position = self.placement_.project(coords);
    const float radius = ed::greasepencil::radius_from_input_sample(
        rv3d,
        region,
        scene_,
        brush_,
        extension_sample.pressure,
        position,
        self.placement_.to_world_space(),
        settings_);
    const float opacity = ed::greasepencil::opacity_from_input_sample(
        extension_sample.pressure, brush_, scene_, settings_);
    Scene *scene = CTX_data_scene(&C);
    const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;

    bke::CurvesGeometry &curves = drawing_->strokes_for_write();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    const int active_curve = on_back ? curves.curves_range().first() :
                                       curves.curves_range().last();
    const IndexRange curve_points = curves.points_by_curve()[active_curve];
    const int last_active_point = curve_points.last();

    const float2 prev_coords = self.screen_space_coords_orig_.last();
    const float prev_radius = drawing_->radii()[last_active_point];
    const float prev_opacity = drawing_->opacities()[last_active_point];
    const ColorGeometry4f prev_vertex_color = drawing_->vertex_colors()[last_active_point];

    /* Overwrite last point if it's very close. */
    const IndexRange points_range = curves.points_by_curve()[curves.curves_range().last()];
    const bool is_first_sample = (points_range.size() == 1);
    if (math::distance(coords, prev_coords) < POINT_OVERRIDE_THRESHOLD_PX) {
      /* Don't move the first point of the stroke. */
      if (!is_first_sample) {
        curves.positions_for_write()[last_active_point] = position;
      }
      drawing_->radii_for_write()[last_active_point] = math::max(radius, prev_radius);
      drawing_->opacities_for_write()[last_active_point] = math::max(opacity, prev_opacity);
      return;
    }

    /* If the next sample is far away, we subdivide the segment to add more points. */
    int new_points_num = 1;
    const float distance_px = math::distance(coords, prev_coords);
    if (distance_px > float(settings_->input_samples)) {
      const int subdivisions = int(math::floor(distance_px / float(settings_->input_samples))) - 1;
      new_points_num += subdivisions;
    }

    /* Resize the curves geometry. */
    extend_curve(curves, on_back, new_points_num);
    /* Subdivide stroke in new_points. */
    const IndexRange new_points = curves.points_by_curve()[active_curve].take_back(new_points_num);
    Array<float2> new_screen_space_coords(new_points_num);
    MutableSpan<float3> positions = curves.positions_for_write();
    MutableSpan<float3> new_positions = positions.slice(new_points);
    MutableSpan<float> new_radii = drawing_->radii_for_write().slice(new_points);
    MutableSpan<float> new_opacities = drawing_->opacities_for_write().slice(new_points);
    linear_interpolation<float2>(prev_coords, coords, new_screen_space_coords, is_first_sample);
    linear_interpolation<float>(prev_radius, radius, new_radii, is_first_sample);
    linear_interpolation<float>(prev_opacity, opacity, new_opacities, is_first_sample);
    if (vertex_color_) {
      MutableSpan<ColorGeometry4f> new_vertex_colors = drawing_->vertex_colors_for_write().slice(
          new_points);
      linear_interpolation<ColorGeometry4f>(
          prev_vertex_color, *vertex_color_, new_vertex_colors, is_first_sample);
    }

    /* Update screen space buffers with new points. */
    self.screen_space_coords_orig_.extend(new_screen_space_coords);
    self.screen_space_smoothed_coords_.extend(new_screen_space_coords);
    for (float2 new_position : new_screen_space_coords) {
      self.screen_space_curve_fitted_coords_.append(Vector<float2>({new_position}));
    }

    /* Only start smoothing if there are enough points. */
    const int64_t min_active_smoothing_points_num = 8;
    const IndexRange smooth_window = self.screen_space_coords_orig_.index_range().drop_front(
        self.active_smooth_start_index_);
    if (smooth_window.size() < min_active_smoothing_points_num) {
      self.placement_.project(new_screen_space_coords, new_positions);
    }
    else {
      /* Active smoothing is done in a window at the end of the new stroke. */
      this->active_smoothing(
          self, smooth_window, positions.slice(curves.points_by_curve()[active_curve]));
    }

    /* Initialize the rest of the attributes with default values. */
    bke::fill_attribute_range_default(attributes,
                                      bke::AttrDomain::Point,
                                      this->skipped_attribute_ids(bke::AttrDomain::Point),
                                      curves.points_range().take_back(1));

    drawing_->set_texture_matrices(Span<float4x2>(&(self.texture_space_), 1),
                                   IndexMask(IndexRange(active_curve, 1)));
  }

  void execute(PaintOperation &self, const bContext &C, const InputSample &extension_sample)
  {
    this->process_extension_sample(self, C, extension_sample);
    drawing_->tag_topology_changed();
  }
};

void PaintOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  ARegion *region = CTX_wm_region(&C);
  View3D *view3d = CTX_wm_view3d(&C);
  Scene *scene = CTX_data_scene(&C);
  Object *object = CTX_data_active_object(&C);
  Object *eval_object = DEG_get_evaluated_object(depsgraph, object);
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);

  Paint *paint = &scene->toolsettings->gp_paint->paint;
  Brush *brush = BKE_paint_brush(paint);

  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }

  BKE_curvemapping_init(brush->gpencil_settings->curve_sensitivity);
  BKE_curvemapping_init(brush->gpencil_settings->curve_strength);
  BKE_curvemapping_init(brush->gpencil_settings->curve_jitter);
  BKE_curvemapping_init(brush->gpencil_settings->curve_rand_pressure);
  BKE_curvemapping_init(brush->gpencil_settings->curve_rand_strength);
  BKE_curvemapping_init(brush->gpencil_settings->curve_rand_uv);
  BKE_curvemapping_init(brush->gpencil_settings->curve_rand_hue);
  BKE_curvemapping_init(brush->gpencil_settings->curve_rand_saturation);
  BKE_curvemapping_init(brush->gpencil_settings->curve_rand_value);

  const bke::greasepencil::Layer &layer = *grease_pencil->get_active_layer();
  /* Initialize helper class for projecting screen space coordinates. */
  placement_ = ed::greasepencil::DrawingPlacement(*scene, *region, *view3d, *eval_object, &layer);
  if (placement_.use_project_to_surface()) {
    placement_.cache_viewport_depths(CTX_data_depsgraph_pointer(&C), region, view3d);
  }
  else if (placement_.use_project_to_nearest_stroke()) {
    placement_.cache_viewport_depths(CTX_data_depsgraph_pointer(&C), region, view3d);
    placement_.set_origin_to_nearest_stroke(start_sample.mouse_position);
  }

  float3 u_dir;
  float3 v_dir;
  /* Set the texture space origin to be the first point. */
  float3 origin = placement_.project(start_sample.mouse_position);
  /* Align texture with the drawing plane. */
  switch (scene->toolsettings->gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_VIEW:
      u_dir = math::normalize(
          placement_.project(float2(region->winx, 0.0f) + start_sample.mouse_position) - origin);
      v_dir = math::normalize(
          placement_.project(float2(0.0f, region->winy) + start_sample.mouse_position) - origin);
      break;
    case GP_LOCKAXIS_Y:
      u_dir = float3(1.0f, 0.0f, 0.0f);
      v_dir = float3(0.0f, 0.0f, 1.0f);
      break;
    case GP_LOCKAXIS_X:
      u_dir = float3(0.0f, 1.0f, 0.0f);
      v_dir = float3(0.0f, 0.0f, 1.0f);
      break;
    case GP_LOCKAXIS_Z:
      u_dir = float3(1.0f, 0.0f, 0.0f);
      v_dir = float3(0.0f, 1.0f, 0.0f);
      break;
    case GP_LOCKAXIS_CURSOR: {
      float3x3 mat;
      BKE_scene_cursor_rot_to_mat3(&scene->cursor, mat.ptr());
      u_dir = mat * float3(1.0f, 0.0f, 0.0f);
      v_dir = mat * float3(0.0f, 1.0f, 0.0f);
      origin = float3(scene->cursor.location);
      break;
    }
  }

  this->texture_space_ = math::transpose(float2x4(float4(u_dir, -math::dot(u_dir, origin)),
                                                  float4(v_dir, -math::dot(v_dir, origin))));

  /* `View` is already stored in object space but all others are in layer space. */
  if (scene->toolsettings->gp_sculpt.lock_axis != GP_LOCKAXIS_VIEW) {
    this->texture_space_ = this->texture_space_ * layer.to_object_space(*object);
  }

  Material *material = BKE_grease_pencil_object_material_ensure_from_active_input_brush(
      CTX_data_main(&C), object, brush);
  const int material_index = BKE_object_material_index_get(object, material);

  PaintOperationExecutor executor{C};
  executor.process_start_sample(*this, C, start_sample, material_index);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, grease_pencil);
}

void PaintOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  Object *object = CTX_data_active_object(&C);
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);

  PaintOperationExecutor executor{C};
  executor.execute(*this, C, extension_sample);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, grease_pencil);
}

static float dist_to_interpolated_2d(
    float2 pos, float2 posA, float2 posB, float val, float valA, float valB)
{
  const float dist1 = math::distance_squared(posA, pos);
  const float dist2 = math::distance_squared(posB, pos);

  if (dist1 + dist2 > 1e-5f) {
    const float interpolated_val = math::interpolate(valB, valA, dist1 / (dist1 + dist2));
    return math::distance(interpolated_val, val);
  }
  return 0.0f;
}

void PaintOperation::simplify_stroke(const bContext &C,
                                     bke::greasepencil::Drawing &drawing,
                                     const float epsilon_px)
{
  const Scene *scene = CTX_data_scene(&C);
  const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;
  const int active_curve = on_back ? drawing.strokes().curves_range().first() :
                                     drawing.strokes().curves_range().last();
  const IndexRange points = drawing.strokes().points_by_curve()[active_curve];
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const VArray<float> radii = drawing.radii();

  /* Distance function for `ramer_douglas_peucker_simplify`. */
  const Span<float2> positions_2d = this->screen_space_smoothed_coords_.as_span();
  const auto dist_function = [&](int64_t first_index, int64_t last_index, int64_t index) {
    /* 2D coordinates are only stored for the current stroke, so offset the indices. */
    const float dist_position_px = dist_to_line_segment_v2(
        positions_2d[index - points.first()],
        positions_2d[first_index - points.first()],
        positions_2d[last_index - points.first()]);
    const float dist_radii_px = dist_to_interpolated_2d(positions_2d[index - points.first()],
                                                        positions_2d[first_index - points.first()],
                                                        positions_2d[last_index - points.first()],
                                                        radii[index],
                                                        radii[first_index],
                                                        radii[last_index]);
    return math::max(dist_position_px, dist_radii_px);
  };

  Array<bool> points_to_delete(curves.points_num(), false);
  int64_t total_points_to_delete = ed::greasepencil::ramer_douglas_peucker_simplify(
      points, epsilon_px, dist_function, points_to_delete.as_mutable_span());

  if (total_points_to_delete > 0) {
    IndexMaskMemory memory;
    curves.remove_points(IndexMask::from_bools(points_to_delete, memory), {});
  }
}

static void remove_points_from_end_of_active_curve(bke::CurvesGeometry &curves,
                                                   const bool on_back,
                                                   const int rem_points_num)
{
  if (!on_back) {
    curves.resize(curves.points_num() - rem_points_num, curves.curves_num());
    curves.offsets_for_write().last() = curves.points_num();
    return;
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  const int last_active_point = curves.points_by_curve()[0].last();

  /* Shift the data before resizing to not delete the data at the end. */
  attributes.for_all([&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
    if (meta_data.domain != bke::AttrDomain::Point) {
      return true;
    }

    bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(id);
    GMutableSpan attribute_data = dst.span;

    bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
      using T = decltype(dummy);
      MutableSpan<T> span_data = attribute_data.typed<T>();

      for (int i = last_active_point - rem_points_num + 1;
           i < curves.points_num() - rem_points_num;
           i++)
      {
        span_data[i] = span_data[i + rem_points_num];
      }
    });
    dst.finish();
    return true;
  });

  curves.resize(curves.points_num() - rem_points_num, curves.curves_num());
  MutableSpan<int> offsets = curves.offsets_for_write();
  for (const int src_curve : curves.curves_range().drop_front(1)) {
    offsets[src_curve] = offsets[src_curve] - rem_points_num;
  }
  offsets.last() = curves.points_num();

  curves.tag_topology_changed();
}

void PaintOperation::process_stroke_end(const bContext &C, bke::greasepencil::Drawing &drawing)
{
  Scene *scene = CTX_data_scene(&C);
  const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;
  const int active_curve = on_back ? drawing.strokes().curves_range().first() :
                                     drawing.strokes().curves_range().last();
  IndexRange points = drawing.strokes().points_by_curve()[active_curve];
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const VArray<float> radii = drawing.radii();

  /* Remove points at the end that have a radius close to 0. */
  int64_t points_to_remove = 0;
  for (int64_t index = points.last(); index >= points.first(); index--) {
    if (radii[index] < 1e-5f) {
      points_to_remove++;
    }
    else {
      break;
    }
  }
  if (points_to_remove > 0) {
    remove_points_from_end_of_active_curve(curves, on_back, points_to_remove);
    points = points.drop_back(points_to_remove);
  }

  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      scene->toolsettings);

  bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);

  if (selection_domain == bke::AttrDomain::Curve) {
    ed::curves::fill_selection_false(selection.span.slice(IndexRange(active_curve, 1)));
  }
  else if (selection_domain == bke::AttrDomain::Point) {
    ed::curves::fill_selection_false(selection.span.slice(points));
  }

  selection.finish();

  drawing.set_texture_matrices(Span<float4x2>(&(this->texture_space_), 1),
                               IndexMask(IndexRange(curves.curves_range().last(), 1)));
}

void PaintOperation::on_stroke_done(const bContext &C)
{
  using namespace blender::bke;
  Scene *scene = CTX_data_scene(&C);
  Object *object = CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  /* Grease Pencil should have an active layer. */
  BLI_assert(grease_pencil.has_active_layer());
  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();
  /* Drawing should exist. */
  bke::greasepencil::Drawing &drawing = *grease_pencil.get_editable_drawing_at(active_layer,
                                                                               scene->r.cfra);

  const float simplifiy_threshold_px = 0.5f;
  this->simplify_stroke(C, drawing, simplifiy_threshold_px);
  this->process_stroke_end(C, drawing);
  drawing.tag_topology_changed();

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil.id);
}

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation()
{
  return std::make_unique<PaintOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil

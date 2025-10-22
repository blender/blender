/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_action.hh"
#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_grease_pencil_vertex_groups.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"

#include "BLI_bounds.hh"
#include "BLI_color.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_noise.hh"
#include "BLI_rand.hh"
#include "BLI_rect.h"
#include "BLI_time.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"

#include "DNA_scene_types.h"
#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "GEO_join_geometries.hh"
#include "GEO_simplify_curves.hh"
#include "GEO_smooth_curves.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"

#include <numeric>
#include <optional>

namespace blender::ed::sculpt_paint::greasepencil {

static float brush_radius_to_pixel_radius(const RegionView3D *rv3d,
                                          const Brush *brush,
                                          const float3 pos)
{
  if ((brush->flag & BRUSH_LOCK_SIZE) != 0) {
    const float pixel_size = ED_view3d_pixel_size(rv3d, pos);
    return (brush->unprojected_size / 2.0f) / pixel_size;
  }
  return float(brush->size / 2.0f);
}

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

/** Creates a temporary brush with the fill guide settings. */
static Brush *create_fill_guide_brush()
{
  Brush *fill_guides_brush = BKE_id_new_nomain<Brush>("Draw Fill Guides");
  fill_guides_brush->ob_mode = OB_MODE_PAINT_GREASE_PENCIL;

  if (fill_guides_brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(fill_guides_brush);
  }
  BrushGpencilSettings *settings = fill_guides_brush->gpencil_settings;

  BKE_curvemapping_init(settings->curve_sensitivity);
  BKE_curvemapping_init(settings->curve_strength);
  BKE_curvemapping_init(settings->curve_jitter);
  BKE_curvemapping_init(settings->curve_rand_pressure);
  BKE_curvemapping_init(settings->curve_rand_strength);
  BKE_curvemapping_init(settings->curve_rand_uv);
  BKE_curvemapping_init(fill_guides_brush->curve_rand_hue);
  BKE_curvemapping_init(fill_guides_brush->curve_rand_saturation);
  BKE_curvemapping_init(fill_guides_brush->curve_rand_value);

  fill_guides_brush->flag |= BRUSH_LOCK_SIZE;
  fill_guides_brush->unprojected_size = 0.01f;

  settings->flag &= ~GP_BRUSH_USE_PRESSURE;

  settings->brush_draw_mode = GP_BRUSH_MODE_VERTEXCOLOR;
  /* TODO: Use theme setting. */
  copy_v3_fl3(fill_guides_brush->color, 0.0f, 1.0f, 1.0f);
  settings->vertex_factor = 1.0f;

  settings->active_smooth = 0.35f;
  settings->hardness = 1.0f;
  fill_guides_brush->spacing = 100;

  settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  settings->simplify_px = 0.4f;

  return fill_guides_brush;
}

class PaintOperation : public GreasePencilStrokeOperation {
 private:
  bke::greasepencil::Drawing *drawing_;
  int frame_number_;
  Vector<ed::greasepencil::MutableDrawingInfo> multi_frame_drawings_;

  /** Screen space coordinates from input samples. */
  Vector<float2> screen_space_coords_orig_;

  /**
   * Temporary vector of curve fitted screen space coordinates per input sample from the active
   * smoothing window. The length of this depends on `active_smooth_start_index_`.
   */
  Vector<Vector<float2>> screen_space_curve_fitted_coords_;
  /** Temporary vector of screen space offsets. */
  Vector<float2> screen_space_jitter_offsets_;
  /** Projection planes for every point in "Stroke" placement mode. */
  Vector<std::optional<float>> stroke_placement_depths_;

  /** Screen space coordinates after smoothing. */
  Vector<float2> screen_space_smoothed_coords_;
  /** Screen space coordinates after smoothing and jittering. */
  Vector<float2> screen_space_final_coords_;

  /** The start index of the smoothing window. */
  int active_smooth_start_index_ = 0;
  blender::float4x2 texture_space_ = float4x2::identity();

  /** Helper class to project screen space coordinates to 3d. */
  ed::greasepencil::DrawingPlacement placement_;
  /** Last valid stroke intersection, for use in Stroke projection mode. */
  std::optional<float> last_stroke_placement_depth_;
  /** Point index of the last valid stroke placement. */
  std::optional<int> last_stroke_placement_point_;

  /** Direction the pen is moving in smoothed over time. */
  float2 smoothed_pen_direction_ = float2(0.0f);

  /** Accumulated distance along the stroke. */
  float accum_distance_ = 0.0f;

  RandomNumberGenerator rng_;

  float stroke_random_radius_factor_;
  float stroke_random_opacity_factor_;
  float stroke_random_rotation_factor_;

  float stroke_random_hue_factor_;
  float stroke_random_sat_factor_;
  float stroke_random_val_factor_;

  /** The current time at which the paint operation begins. */
  double start_time_;
  /** Current delta time from #start_time_, updated after each extension sample. */
  double delta_time_;

  /** Set to true when the paint operation is used to draw fill guides. */
  bool do_fill_guides_;

  friend struct PaintOperationExecutor;

  Brush *saved_active_brush_;
  Brush *fill_guides_brush_;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;

  PaintOperation(const bool do_fill_guides = false) : do_fill_guides_(do_fill_guides) {}

  bool update_stroke_depth_placement(const bContext &C, const InputSample &sample);
  /** Returns the range of actually reprojected points. */
  IndexRange interpolate_stroke_depth(const bContext &C,
                                      std::optional<int> start_point,
                                      float from_depth,
                                      float to_depth);
  void toggle_fill_guides_brush_on(const bContext &C);
  void toggle_fill_guides_brush_off(const bContext &C);
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
  std::optional<BrushColorJitterSettings> jitter_settings_;

  ColorGeometry4f vertex_color_ = ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f);
  ColorGeometry4f fill_color_ = ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f);
  float softness_;
  float aspect_ratio_;

  bool use_vertex_color_;
  bool use_settings_random_;

  PaintOperationExecutor(const bContext &C)
  {
    scene_ = CTX_data_scene(&C);
    Paint *paint = &scene_->toolsettings->gp_paint->paint;
    brush_ = BKE_paint_brush(paint);
    settings_ = brush_->gpencil_settings;

    use_settings_random_ = (settings_->flag & GP_BRUSH_GROUP_RANDOM) != 0;
    use_vertex_color_ = brush_using_vertex_color(scene_->toolsettings->gp_paint, brush_);
    if (use_vertex_color_) {
      ColorGeometry4f color_base;
      copy_v3_v3(color_base, brush_->color);
      color_base.a = settings_->vertex_factor;
      if (ELEM(settings_->vertex_mode, GPPAINT_MODE_STROKE, GPPAINT_MODE_BOTH)) {
        vertex_color_ = color_base;
      }
      if (ELEM(settings_->vertex_mode, GPPAINT_MODE_FILL, GPPAINT_MODE_BOTH)) {
        fill_color_ = color_base;
      }
    }
    softness_ = 1.0f - settings_->hardness;
    aspect_ratio_ = settings_->aspect_ratio[0] / math::max(settings_->aspect_ratio[1], 1e-8f);
    jitter_settings_ = BKE_brush_color_jitter_get_settings(paint, brush_);
  }

  void process_start_sample(PaintOperation &self,
                            const bContext &C,
                            const InputSample &start_sample,
                            const int material_index,
                            const bool use_fill)
  {
    const float2 start_coords = start_sample.mouse_position;
    const RegionView3D *rv3d = CTX_wm_region_view3d(&C);
    const ARegion *region = CTX_wm_region(&C);

    float3 start_location;
    if (self.placement_.use_project_to_stroke()) {
      const std::optional<float> depth = self.placement_.get_depth(start_coords);
      if (depth) {
        start_location = self.placement_.place(start_coords, *depth);
      }
      else {
        start_location = self.placement_.project(start_coords);
      }
    }
    else {
      start_location = self.placement_.project(start_coords);
    }
    float start_radius = ed::greasepencil::radius_from_input_sample(
        rv3d,
        region,
        brush_,
        start_sample.pressure,
        start_location,
        self.placement_.to_world_space(),
        settings_);
    start_radius = ed::greasepencil::randomize_radius(
        *settings_, self.stroke_random_radius_factor_, 0.0f, start_radius, start_sample.pressure);

    float start_opacity = ed::greasepencil::opacity_from_input_sample(
        start_sample.pressure, brush_, settings_);
    start_opacity = ed::greasepencil::randomize_opacity(*settings_,
                                                        self.stroke_random_opacity_factor_,
                                                        0.0f,
                                                        start_opacity,
                                                        start_sample.pressure);

    const float start_rotation = ed::greasepencil::randomize_rotation(
        *settings_, self.rng_, self.stroke_random_rotation_factor_, start_sample.pressure);
    Scene *scene = CTX_data_scene(&C);
    if (use_vertex_color_) {
      vertex_color_ = ed::greasepencil::randomize_color(*settings_,
                                                        jitter_settings_,
                                                        self.stroke_random_hue_factor_,
                                                        self.stroke_random_sat_factor_,
                                                        self.stroke_random_val_factor_,
                                                        0.0f,
                                                        vertex_color_,
                                                        start_sample.pressure);
    }

    const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;

    self.screen_space_coords_orig_.append(start_coords);
    self.screen_space_curve_fitted_coords_.append(Vector<float2>({start_coords}));
    self.screen_space_jitter_offsets_.append(float2(0.0f));
    self.screen_space_smoothed_coords_.append(start_coords);
    self.screen_space_final_coords_.append(start_coords);

    /* Resize the curves geometry so there is one more curve with a single point. */
    bke::CurvesGeometry &curves = self.drawing_->strokes_for_write();
    ed::greasepencil::add_single_curve(curves, on_back == false);

    const int active_curve = on_back ? curves.curves_range().first() :
                                       curves.curves_range().last();
    const IndexRange curve_points = curves.points_by_curve()[active_curve];
    const int last_active_point = curve_points.last();

    Set<std::string> point_attributes_to_skip;
    Set<std::string> curve_attributes_to_skip;
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    curves.positions_for_write()[last_active_point] = start_location;
    self.drawing_->radii_for_write()[last_active_point] = start_radius;
    self.drawing_->opacities_for_write()[last_active_point] = start_opacity;
    point_attributes_to_skip.add_multiple({"position", "radius", "opacity"});
    if (use_vertex_color_ || attributes.contains("vertex_color")) {
      self.drawing_->vertex_colors_for_write()[last_active_point] = vertex_color_;
      point_attributes_to_skip.add("vertex_color");
    }
    if (use_fill || attributes.contains("fill_color")) {
      self.drawing_->fill_colors_for_write()[active_curve] = fill_color_;
      curve_attributes_to_skip.add("fill_color");
    }
    if (bke::SpanAttributeWriter<float> delta_times =
            attributes.lookup_or_add_for_write_span<float>("delta_time", bke::AttrDomain::Point))
    {
      delta_times.span[last_active_point] = 0.0f;
      point_attributes_to_skip.add("delta_time");
      delta_times.finish();
    }

    bke::SpanAttributeWriter<int> materials = attributes.lookup_or_add_for_write_span<int>(
        "material_index", bke::AttrDomain::Curve);
    bke::SpanAttributeWriter<bool> cyclic = attributes.lookup_or_add_for_write_span<bool>(
        "cyclic", bke::AttrDomain::Curve);
    cyclic.span[active_curve] = false;
    materials.span[active_curve] = material_index;
    curve_attributes_to_skip.add_multiple({"material_index", "cyclic"});
    cyclic.finish();
    materials.finish();

    if (bke::SpanAttributeWriter<float> softness = attributes.lookup_or_add_for_write_span<float>(
            "softness", bke::AttrDomain::Curve))
    {
      softness.span[active_curve] = softness_;
      curve_attributes_to_skip.add("softness");
      softness.finish();
    }
    if (bke::SpanAttributeWriter<float> u_scale = attributes.lookup_or_add_for_write_span<float>(
            "u_scale",
            bke::AttrDomain::Curve,
            bke::AttributeInitVArray(VArray<float>::from_single(1.0f, curves.curves_num()))))
    {
      u_scale.span[active_curve] = 1.0f;
      curve_attributes_to_skip.add("u_scale");
      u_scale.finish();
    }
    if (bke::SpanAttributeWriter<float> aspect_ratio =
            attributes.lookup_or_add_for_write_span<float>(
                "aspect_ratio",
                bke::AttrDomain::Curve,
                bke::AttributeInitVArray(VArray<float>::from_single(1.0f, curves.curves_num()))))
    {
      aspect_ratio.span[active_curve] = aspect_ratio_;
      curve_attributes_to_skip.add("aspect_ratio");
      aspect_ratio.finish();
    }

    if (settings_->uv_random > 0.0f || attributes.contains("rotation")) {
      if (bke::SpanAttributeWriter<float> rotations =
              attributes.lookup_or_add_for_write_span<float>("rotation", bke::AttrDomain::Point))
      {
        rotations.span[last_active_point] = start_rotation;
        point_attributes_to_skip.add("rotation");
        rotations.finish();
      }
    }

    /* Only set the attribute if the type is not the default or if it already exists. */
    if (settings_->caps_type != GP_STROKE_CAP_TYPE_ROUND || attributes.contains("start_cap")) {
      if (bke::SpanAttributeWriter<int8_t> start_caps =
              attributes.lookup_or_add_for_write_span<int8_t>("start_cap", bke::AttrDomain::Curve))
      {
        start_caps.span[active_curve] = settings_->caps_type;
        curve_attributes_to_skip.add("start_cap");
        start_caps.finish();
      }
    }

    if (settings_->caps_type != GP_STROKE_CAP_TYPE_ROUND || attributes.contains("end_cap")) {
      if (bke::SpanAttributeWriter<int8_t> end_caps =
              attributes.lookup_or_add_for_write_span<int8_t>("end_cap", bke::AttrDomain::Curve))
      {
        end_caps.span[active_curve] = settings_->caps_type;
        curve_attributes_to_skip.add("end_cap");
        end_caps.finish();
      }
    }

    if (use_fill && (start_opacity < 1.0f || attributes.contains("fill_opacity"))) {
      if (bke::SpanAttributeWriter<float> fill_opacities =
              attributes.lookup_or_add_for_write_span<float>(
                  "fill_opacity",
                  bke::AttrDomain::Curve,
                  bke::AttributeInitVArray(VArray<float>::from_single(1.0f, curves.curves_num()))))
      {
        fill_opacities.span[active_curve] = start_opacity;
        curve_attributes_to_skip.add("fill_opacity");
        fill_opacities.finish();
      }
    }

    if (bke::SpanAttributeWriter<float> init_times =
            attributes.lookup_or_add_for_write_span<float>("init_time", bke::AttrDomain::Curve))
    {
      /* Truncating time in ms to uint32 then we don't lose precision in lower bits. */
      init_times.span[active_curve] = float(uint64_t(self.start_time_ * 1e3)) / float(1e3);
      curve_attributes_to_skip.add("init_time");
      init_times.finish();
    }

    if (self.do_fill_guides_) {
      if (bke::SpanAttributeWriter<bool> is_fill_boundary =
              attributes.lookup_or_add_for_write_span<bool>(".is_fill_guide",
                                                            bke::AttrDomain::Curve))
      {
        is_fill_boundary.span[active_curve] = true;
        curve_attributes_to_skip.add(".is_fill_guide");
        is_fill_boundary.finish();
      }
    }

    curves.curve_types_for_write()[active_curve] = CURVE_TYPE_POLY;
    curve_attributes_to_skip.add("curve_type");
    curves.update_curve_types();

    if (self.placement_.use_project_to_stroke()) {
      self.stroke_placement_depths_.append(self.stroke_placement_depths_.is_empty() ?
                                               std::nullopt :
                                               self.stroke_placement_depths_.last());
      /* Initialize the snap point. */
      self.update_stroke_depth_placement(C, start_sample);
    }

    /* Initialize the rest of the attributes with default values. */
    bke::fill_attribute_range_default(
        attributes,
        bke::AttrDomain::Point,
        bke::attribute_filter_from_skip_ref(point_attributes_to_skip),
        IndexRange(last_active_point, 1));
    bke::fill_attribute_range_default(
        attributes,
        bke::AttrDomain::Curve,
        bke::attribute_filter_from_skip_ref(curve_attributes_to_skip),
        IndexRange(active_curve, 1));

    self.drawing_->tag_topology_changed();
  }

  void active_smoothing(PaintOperation &self, const IndexRange smooth_window)
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
        VArray<float>::from_single(settings_->active_smooth, smooth_window.size()),
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
    }

    /* Remove all the converged points from the active window and shrink the window accordingly. */
    if (num_converged > 0) {
      self.active_smooth_start_index_ += num_converged;
      self.screen_space_curve_fitted_coords_.remove(0, num_converged);
    }
  }

  void active_jitter(PaintOperation &self,
                     const int new_points_num,
                     const float brush_radius_px,
                     const float pressure,
                     const IndexRange active_window,
                     MutableSpan<float3> curve_positions)
  {
    float jitter_factor = 1.0f;
    if (settings_->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
      jitter_factor = BKE_curvemapping_evaluateF(settings_->curve_jitter, 0, pressure);
    }
    const float2 tangent = math::normalize(self.smoothed_pen_direction_);
    const float2 cotangent = float2(-tangent.y, tangent.x);
    for ([[maybe_unused]] const int _ : IndexRange(new_points_num)) {
      const float rand = self.rng_.get_float() * 2.0f - 1.0f;
      const float factor = rand * settings_->draw_jitter * jitter_factor;
      self.screen_space_jitter_offsets_.append(cotangent * factor * brush_radius_px);
    }
    const Span<float2> jitter_slice = self.screen_space_jitter_offsets_.as_mutable_span().slice(
        active_window);
    MutableSpan<float2> smoothed_coords =
        self.screen_space_smoothed_coords_.as_mutable_span().slice(active_window);
    MutableSpan<float2> final_coords = self.screen_space_final_coords_.as_mutable_span().slice(
        active_window);
    MutableSpan<float3> positions_slice = curve_positions.slice(active_window);
    if (self.placement_.use_project_to_stroke()) {
      BLI_assert(self.stroke_placement_depths_.size() == self.screen_space_coords_orig_.size());
      const Span<std::optional<float>> stroke_depths =
          self.stroke_placement_depths_.as_span().slice(active_window);
      for (const int64_t window_i : active_window.index_range()) {
        final_coords[window_i] = smoothed_coords[window_i] + jitter_slice[window_i];
        const std::optional<float> depth = stroke_depths[window_i];
        positions_slice[window_i] = depth ? self.placement_.place(final_coords[window_i], *depth) :
                                            self.placement_.project(final_coords[window_i]);
      }
    }
    else {
      for (const int64_t window_i : active_window.index_range()) {
        final_coords[window_i] = smoothed_coords[window_i] + jitter_slice[window_i];
        positions_slice[window_i] = self.placement_.project(final_coords[window_i]);
      }
    }
  }

  void process_extension_sample(PaintOperation &self,
                                const bContext &C,
                                const InputSample &extension_sample)
  {
    Scene *scene = CTX_data_scene(&C);
    const RegionView3D *rv3d = CTX_wm_region_view3d(&C);
    const ARegion *region = CTX_wm_region(&C);
    const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;

    const float2 coords = extension_sample.mouse_position;
    float3 position;
    if (self.placement_.use_project_to_stroke()) {
      const std::optional<float> depth = self.stroke_placement_depths_.is_empty() ?
                                             std::nullopt :
                                             self.stroke_placement_depths_.last();
      if (depth) {
        position = self.placement_.place(coords, *depth);
      }
      else {
        position = self.placement_.project(coords);
      }
    }
    else {
      position = self.placement_.project(coords);
    }

    float radius = ed::greasepencil::radius_from_input_sample(rv3d,
                                                              region,
                                                              brush_,
                                                              extension_sample.pressure,
                                                              position,
                                                              self.placement_.to_world_space(),
                                                              settings_);
    float opacity = ed::greasepencil::opacity_from_input_sample(
        extension_sample.pressure, brush_, settings_);

    const float brush_radius_px = brush_radius_to_pixel_radius(
        rv3d, brush_, math::transform_point(self.placement_.to_world_space(), position));

    bke::CurvesGeometry &curves = self.drawing_->strokes_for_write();
    OffsetIndices<int> points_by_curve = curves.points_by_curve();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    const int active_curve = on_back ? curves.curves_range().first() :
                                       curves.curves_range().last();
    const IndexRange curve_points = points_by_curve[active_curve];
    const int last_active_point = curve_points.last();

    const float2 prev_coords = self.screen_space_coords_orig_.last();
    float prev_radius = self.drawing_->radii()[last_active_point];
    const float prev_opacity = self.drawing_->opacities()[last_active_point];
    const ColorGeometry4f prev_vertex_color = self.drawing_->vertex_colors()[last_active_point];

    const bool is_first_sample = (curve_points.size() == 1);

    /* Use the vector from the previous to the next point. Set the direction based on the first two
     * samples. For subsequent samples, interpolate with the previous direction to get a smoothed
     * value over time. */
    if (is_first_sample) {
      self.smoothed_pen_direction_ = self.screen_space_coords_orig_.last() - coords;
    }
    else {
      /* The smoothing rate is a factor from 0 to 1 that represents how quickly the
       * `smoothed_pen_direction_` "reacts" to changes in direction.
       *  - 1.0f: Immediate reaction.
       *  - 0.0f: No reaction (value never changes). */
      constexpr float smoothing_rate_factor = 0.3f;
      self.smoothed_pen_direction_ = math::interpolate(self.smoothed_pen_direction_,
                                                       self.screen_space_coords_orig_.last() -
                                                           coords,
                                                       smoothing_rate_factor);
    }

    /* Approximate brush with non-circular shape by changing the radius based on the angle. */
    float radius_factor = 1.0f;
    if (settings_->draw_angle_factor > 0.0f) {
      /* `angle` is the angle to the horizontal line in screen space. */
      const float angle = settings_->draw_angle;
      const float2 angle_vec = float2(math::cos(angle), math::sin(angle));

      /* The angle factor is 1.0f when the direction is aligned with the angle vector and 0.0f when
       * it is orthogonal to the angle vector. This is consistent with the behavior from GPv2. */
      const float angle_factor = math::abs(
          math::dot(angle_vec, math::normalize(self.smoothed_pen_direction_)));

      /* Influence is controlled by `draw_angle_factor`. */
      radius_factor = math::interpolate(1.0f, angle_factor, settings_->draw_angle_factor);
      radius *= radius_factor;
    }

    /* Overwrite last point if it's very close. */
    const float distance_px = math::distance(coords, prev_coords);
    constexpr float point_override_threshold_px = 2.0f;
    if (distance_px < point_override_threshold_px) {
      self.accum_distance_ += distance_px;
      /* Don't move the first point of the stroke. */
      if (!is_first_sample) {
        curves.positions_for_write()[last_active_point] = position;
      }
      if (use_settings_random_ && settings_->draw_random_press > 0.0f) {
        radius = ed::greasepencil::randomize_radius(*settings_,
                                                    self.stroke_random_radius_factor_,
                                                    self.accum_distance_,
                                                    radius,
                                                    extension_sample.pressure);
      }
      if (use_settings_random_ && settings_->draw_random_strength > 0.0f) {
        opacity = ed::greasepencil::randomize_opacity(*settings_,
                                                      self.stroke_random_opacity_factor_,
                                                      self.accum_distance_,
                                                      opacity,
                                                      extension_sample.pressure);
      }
      self.drawing_->radii_for_write()[last_active_point] = math::max(radius, prev_radius);
      self.drawing_->opacities_for_write()[last_active_point] = math::max(opacity, prev_opacity);
      return;
    }

    /* Adjust the first points radius based on the computed angle. */
    if (is_first_sample && settings_->draw_angle_factor > 0.0f) {
      self.drawing_->radii_for_write()[last_active_point] *= radius_factor;
      prev_radius = self.drawing_->radii()[last_active_point];
    }

    /* Clamp the number of points within a pixel in screen space. */
    constexpr int max_points_per_pixel = 4;
    /* The value `brush_->spacing` is a percentage of the brush radius in pixels. */
    const float max_spacing_px = math::max((float(brush_->spacing) / 100.0f) *
                                               float(brush_radius_px),
                                           1.0f / float(max_points_per_pixel));
    /* If the next sample is far away, we subdivide the segment to add more points. */
    const int new_points_num = (distance_px > max_spacing_px) ?
                                   int(math::floor(distance_px / max_spacing_px)) :
                                   1;
    /* Resize the curves geometry. */
    ed::greasepencil::resize_single_curve(
        curves, on_back == false, curve_points.size() + new_points_num);

    Set<std::string> point_attributes_to_skip;
    /* Subdivide new segment. */
    const IndexRange new_points = curves.points_by_curve()[active_curve].take_back(new_points_num);
    Array<float2> new_screen_space_coords(new_points_num);
    MutableSpan<float3> positions = curves.positions_for_write();
    MutableSpan<float3> new_positions = positions.slice(new_points);
    MutableSpan<float> new_radii = self.drawing_->radii_for_write().slice(new_points);
    MutableSpan<float> new_opacities = self.drawing_->opacities_for_write().slice(new_points);

    /* Interpolate the screen space positions. */
    linear_interpolation<float2>(prev_coords, coords, new_screen_space_coords, is_first_sample);
    linear_interpolation<float>(prev_radius, radius, new_radii, is_first_sample);
    linear_interpolation<float>(prev_opacity, opacity, new_opacities, is_first_sample);
    point_attributes_to_skip.add_multiple({"position", "radius", "opacity"});

    /* Randomize radii. */
    if (use_settings_random_ && settings_->draw_random_press > 0.0f) {
      for (const int i : IndexRange(new_points_num)) {
        new_radii[i] = ed::greasepencil::randomize_radius(*settings_,
                                                          self.stroke_random_radius_factor_,
                                                          self.accum_distance_ +
                                                              max_spacing_px * i,
                                                          new_radii[i],
                                                          extension_sample.pressure);
      }
    }

    /* Randomize opacities. */
    if (use_settings_random_ && settings_->draw_random_strength > 0.0f) {
      for (const int i : IndexRange(new_points_num)) {
        new_opacities[i] = ed::greasepencil::randomize_opacity(*settings_,
                                                               self.stroke_random_opacity_factor_,
                                                               self.accum_distance_ +
                                                                   max_spacing_px * i,
                                                               new_opacities[i],
                                                               extension_sample.pressure);
      }
    }

    /* Randomize rotations. */
    if (use_settings_random_ && (settings_->uv_random > 0.0f || attributes.contains("rotation"))) {
      if (bke::SpanAttributeWriter<float> rotations =
              attributes.lookup_or_add_for_write_span<float>("rotation", bke::AttrDomain::Point))
      {
        const MutableSpan<float> new_rotations = rotations.span.slice(new_points);
        for (const int i : IndexRange(new_points_num)) {
          new_rotations[i] = ed::greasepencil::randomize_rotation(
              *settings_,
              self.rng_,
              self.stroke_random_rotation_factor_,
              extension_sample.pressure);
        }
        point_attributes_to_skip.add("rotation");
        rotations.finish();
      }
    }

    /* Randomize vertex color. */
    if (use_vertex_color_ || attributes.contains("vertex_color")) {
      MutableSpan<ColorGeometry4f> new_vertex_colors =
          self.drawing_->vertex_colors_for_write().slice(new_points);
      linear_interpolation<ColorGeometry4f>(
          prev_vertex_color, vertex_color_, new_vertex_colors, is_first_sample);
      if (use_settings_random_ || attributes.contains("vertex_color")) {
        for (const int i : IndexRange(new_points_num)) {
          new_vertex_colors[i] = ed::greasepencil::randomize_color(*settings_,
                                                                   jitter_settings_,
                                                                   self.stroke_random_hue_factor_,
                                                                   self.stroke_random_sat_factor_,
                                                                   self.stroke_random_val_factor_,
                                                                   self.accum_distance_ +
                                                                       max_spacing_px * i,
                                                                   new_vertex_colors[i],
                                                                   extension_sample.pressure);
        }
      }
      point_attributes_to_skip.add("vertex_color");
    }

    const double new_delta_time = BLI_time_now_seconds() - self.start_time_;
    if (bke::SpanAttributeWriter<float> delta_times =
            attributes.lookup_or_add_for_write_span<float>("delta_time", bke::AttrDomain::Point))
    {
      linear_interpolation<float>(float(self.delta_time_),
                                  float(new_delta_time),
                                  delta_times.span.slice(new_points),
                                  is_first_sample);
      point_attributes_to_skip.add("delta_time");
      delta_times.finish();
    }

    /* Update the accumulated distance along the stroke in pixels. */
    self.accum_distance_ += distance_px;

    /* Update the current delta time. */
    self.delta_time_ = new_delta_time;

    /* Update screen space buffers with new points. */
    self.screen_space_coords_orig_.extend(new_screen_space_coords);
    self.screen_space_smoothed_coords_.extend(new_screen_space_coords);
    self.screen_space_final_coords_.extend(new_screen_space_coords);
    for (float2 new_position : new_screen_space_coords) {
      self.screen_space_curve_fitted_coords_.append(Vector<float2>({new_position}));
    }
    if (self.placement_.use_project_to_stroke()) {
      const std::optional<float> last_depth = self.stroke_placement_depths_.is_empty() ?
                                                  std::nullopt :
                                                  self.stroke_placement_depths_.last();
      self.stroke_placement_depths_.append_n_times(last_depth, new_points_num);
    }

    /* Only start smoothing if there are enough points. */
    constexpr int64_t min_active_smoothing_points_num = 8;
    const IndexRange smooth_window = self.screen_space_coords_orig_.index_range().drop_front(
        self.active_smooth_start_index_);
    if (smooth_window.size() < min_active_smoothing_points_num) {
      if (self.placement_.use_project_to_stroke()) {
        const Span<std::optional<float>> new_depths =
            self.stroke_placement_depths_.as_mutable_span().take_back(new_points_num);
        for (const int64_t i : new_positions.index_range()) {
          const std::optional<float> depth = new_depths[i];
          if (depth) {
            new_positions[i] = self.placement_.place(coords, *depth);
          }
          else {
            new_positions[i] = self.placement_.project(coords);
          }
        }
      }
      else {
        self.placement_.project(new_screen_space_coords, new_positions);
      }
    }
    else {
      /* Active smoothing is done in a window at the end of the new stroke.
       * Final positions are written below. */
      this->active_smoothing(self, smooth_window);
    }

    /* Jitter uses smoothed coordinates as input. In case smoothing is not applied these are the
     * unsmoothed original coordinates. */
    MutableSpan<float3> curve_positions = positions.slice(curves.points_by_curve()[active_curve]);
    if (use_settings_random_ && settings_->draw_jitter > 0.0f) {
      this->active_jitter(self,
                          new_points_num,
                          brush_radius_px,
                          extension_sample.pressure,
                          smooth_window,
                          curve_positions);
    }
    else {
      MutableSpan<float2> smoothed_coords =
          self.screen_space_smoothed_coords_.as_mutable_span().slice(smooth_window);
      MutableSpan<float2> final_coords = self.screen_space_final_coords_.as_mutable_span().slice(
          smooth_window);
      /* Not jitter, so we just copy the positions over. */
      final_coords.copy_from(smoothed_coords);
      MutableSpan<float3> curve_positions_slice = curve_positions.slice(smooth_window);
      if (self.placement_.use_project_to_stroke()) {
        BLI_assert(self.stroke_placement_depths_.size() == self.screen_space_coords_orig_.size());
        const Span<std::optional<float>> stroke_depths =
            self.stroke_placement_depths_.as_mutable_span().slice(smooth_window);
        for (const int64_t window_i : smooth_window.index_range()) {
          const std::optional<float> depth = stroke_depths[window_i];
          curve_positions_slice[window_i] = depth ?
                                                self.placement_.place(final_coords[window_i],
                                                                      *depth) :
                                                self.placement_.project(final_coords[window_i]);
        }
      }
      else {
        for (const int64_t window_i : smooth_window.index_range()) {
          curve_positions_slice[window_i] = self.placement_.project(final_coords[window_i]);
        }
      }
    }

    if (self.placement_.use_project_to_stroke()) {
      /* Find a new snap point and apply projection to trailing points. */
      self.update_stroke_depth_placement(C, extension_sample);
    }

    /* Initialize the rest of the attributes with default values. */
    bke::fill_attribute_range_default(
        attributes,
        bke::AttrDomain::Point,
        bke::attribute_filter_from_skip_ref(point_attributes_to_skip),
        curves.points_range().take_back(new_points_num));

    self.drawing_->set_texture_matrices({self.texture_space_},
                                        IndexRange::from_single(active_curve));
  }

  void execute(PaintOperation &self, const bContext &C, const InputSample &extension_sample)
  {
    const Scene *scene = CTX_data_scene(&C);
    const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;

    this->process_extension_sample(self, C, extension_sample);

    const bke::CurvesGeometry &curves = self.drawing_->strokes();
    const int active_curve = on_back ? curves.curves_range().first() :
                                       curves.curves_range().last();
    self.drawing_->tag_topology_changed(IndexRange::from_single(active_curve));
  }
};

enum class StrokeSnapMode {
  AllPoints,
  EndPoints,
  FirstPoint,
};

static StrokeSnapMode get_snap_mode(const bContext &C)
{
  /* gpencil_v3d_align is an awkward combination of multiple properties. If none of the non-zero
   * flags are set the AllPoints mode is the default. */
  const Scene &scene = *CTX_data_scene(&C);
  const char align_flags = scene.toolsettings->gpencil_v3d_align;
  if (align_flags & GP_PROJECT_DEPTH_STROKE_ENDPOINTS) {
    return StrokeSnapMode::EndPoints;
  }
  if (align_flags & GP_PROJECT_DEPTH_STROKE_FIRST) {
    return StrokeSnapMode::FirstPoint;
  }
  return StrokeSnapMode::AllPoints;
}

bool PaintOperation::update_stroke_depth_placement(const bContext &C, const InputSample &sample)
{
  BLI_assert(placement_.use_project_to_stroke());

  const std::optional<float> new_stroke_placement_depth = placement_.get_depth(
      sample.mouse_position);
  if (!new_stroke_placement_depth) {
    return false;
  }

  const StrokeSnapMode snap_mode = get_snap_mode(C);
  switch (snap_mode) {
    case StrokeSnapMode::AllPoints: {
      const float start_depth = last_stroke_placement_depth_ ? *last_stroke_placement_depth_ :
                                                               *new_stroke_placement_depth;
      const float end_depth = *new_stroke_placement_depth;
      const IndexRange reprojected_points = this->interpolate_stroke_depth(
          C, last_stroke_placement_point_, start_depth, end_depth);
      /* Only reproject newly added points next time a hit point is found. */
      if (!reprojected_points.is_empty()) {
        last_stroke_placement_point_ = reprojected_points.one_after_last();
      }

      last_stroke_placement_depth_ = new_stroke_placement_depth;
      break;
    }
    case StrokeSnapMode::EndPoints: {
      const float start_depth = last_stroke_placement_depth_ ? *last_stroke_placement_depth_ :
                                                               *new_stroke_placement_depth;
      const float end_depth = *new_stroke_placement_depth;
      const IndexRange reprojected_points = this->interpolate_stroke_depth(
          C, last_stroke_placement_point_, start_depth, end_depth);

      /* Only update depth on the first hit. */
      if (!last_stroke_placement_depth_) {
        /* Keep reprojecting all points from the first hit onward. */
        if (!reprojected_points.is_empty()) {
          last_stroke_placement_point_ = reprojected_points.one_after_last();
        }
        last_stroke_placement_depth_ = new_stroke_placement_depth;
      }
      break;
    }
    case StrokeSnapMode::FirstPoint: {
      /* Only reproject once in "First Point" mode. */
      if (!last_stroke_placement_depth_) {
        const float start_depth = *new_stroke_placement_depth;
        const float end_depth = *new_stroke_placement_depth;
        this->interpolate_stroke_depth(C, last_stroke_placement_point_, start_depth, end_depth);

        last_stroke_placement_depth_ = new_stroke_placement_depth;
        break;
      }
    }
  }

  return true;
}

IndexRange PaintOperation::interpolate_stroke_depth(const bContext &C,
                                                    std::optional<int> start_point,
                                                    const float from_depth,
                                                    const float to_depth)
{
  using namespace blender::bke;

  Scene *scene = CTX_data_scene(&C);
  const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;

  /* Drawing should exist. */
  BLI_assert(drawing_);
  bke::greasepencil::Drawing &drawing = *drawing_;
  const int active_curve = on_back ? drawing.strokes().curves_range().first() :
                                     drawing.strokes().curves_range().last();
  const offset_indices::OffsetIndices<int> points_by_curve = drawing.strokes().points_by_curve();
  const IndexRange all_points = points_by_curve[active_curve];
  BLI_assert(screen_space_final_coords_.size() == all_points.size());
  if (all_points.is_empty()) {
    return {};
  }

  IndexRange active_points = all_points;
  if (start_point) {
    active_points = IndexRange::from_begin_end_inclusive(*start_point, all_points.last());
  }
  if (active_points.is_empty()) {
    return {};
  }

  /* Point slice relative to the curve, valid for 2D coordinate array. */
  const IndexRange active_curve_points = active_points.shift(-all_points.start());

  MutableSpan<std::optional<float>> depths = stroke_placement_depths_.as_mutable_span().slice(
      active_curve_points);
  MutableSpan<float3> positions = drawing.strokes_for_write().positions_for_write().slice(
      active_points);
  const Span<float2> final_coords = screen_space_final_coords_.as_span().slice(
      active_curve_points);
  const float step_size = 1.0f / std::max(int(active_points.size()) - 1, 1);
  for (const int i : positions.index_range()) {
    /* Update the placement depth for later reprojection (active smoothing). */
    depths[i] = math::interpolate(from_depth, to_depth, float(i) * step_size);
    positions[i] = placement_.place(final_coords[i], *depths[i]);
  }

  return active_points;
}

void PaintOperation::toggle_fill_guides_brush_on(const bContext &C)
{
  Paint *paint = BKE_paint_get_active_from_context(&C);
  Brush *current_brush = BKE_paint_brush(paint);

  fill_guides_brush_ = create_fill_guide_brush();
  BLI_assert(fill_guides_brush_ != nullptr);
  BKE_paint_brush_set(paint, fill_guides_brush_);

  saved_active_brush_ = current_brush;
}

void PaintOperation::toggle_fill_guides_brush_off(const bContext &C)
{
  Paint *paint = BKE_paint_get_active_from_context(&C);
  BLI_assert(saved_active_brush_ != nullptr);
  BKE_paint_brush_set(paint, saved_active_brush_);
  saved_active_brush_ = nullptr;
  /* Free the temporary brush. */
  BKE_id_free_ex(nullptr, fill_guides_brush_, LIB_ID_FREE_NO_MAIN, false);
  fill_guides_brush_ = nullptr;
}

void PaintOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  ARegion *region = CTX_wm_region(&C);
  View3D *view3d = CTX_wm_view3d(&C);
  Scene *scene = CTX_data_scene(&C);
  Object *object = CTX_data_active_object(&C);
  Object *eval_object = DEG_get_evaluated(depsgraph, object);
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);

  if (do_fill_guides_) {
    this->toggle_fill_guides_brush_on(C);
  }

  Paint *paint = &scene->toolsettings->gp_paint->paint;
  Brush *brush = BKE_paint_brush(paint);

  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  BrushGpencilSettings *settings = brush->gpencil_settings;

  BKE_curvemapping_init(settings->curve_sensitivity);
  BKE_curvemapping_init(settings->curve_strength);
  BKE_curvemapping_init(settings->curve_jitter);
  BKE_curvemapping_init(settings->curve_rand_pressure);
  BKE_curvemapping_init(settings->curve_rand_strength);
  BKE_curvemapping_init(settings->curve_rand_uv);
  BKE_curvemapping_init(brush->curve_rand_hue);
  BKE_curvemapping_init(brush->curve_rand_saturation);
  BKE_curvemapping_init(brush->curve_rand_value);

  BLI_assert(grease_pencil->has_active_layer());
  const bke::greasepencil::Layer &layer = *grease_pencil->get_active_layer();
  /* Initialize helper class for projecting screen space coordinates. */
  placement_ = ed::greasepencil::DrawingPlacement(*scene, *region, *view3d, *eval_object, &layer);
  if (placement_.use_project_to_surface()) {
    placement_.cache_viewport_depths(depsgraph, region, view3d);
  }
  else if (placement_.use_project_to_stroke()) {
    placement_.cache_viewport_depths(depsgraph, region, view3d);
  }

  texture_space_ = ed::greasepencil::calculate_texture_space(
      scene, region, start_sample.mouse_position, placement_);

  /* `View` is already stored in object space but all others are in layer space. */
  if (scene->toolsettings->gp_sculpt.lock_axis != GP_LOCKAXIS_VIEW) {
    texture_space_ = texture_space_ * layer.to_object_space(*object);
  }

  rng_ = RandomNumberGenerator::from_random_seed();
  if ((settings->flag & GP_BRUSH_GROUP_RANDOM) != 0) {
    /* Since we want stroke properties to randomize around set values, it's easier for us to have a
     * signed value in range (-1,1) in calculations downstream. */
    stroke_random_radius_factor_ = rng_.get_float() * 2.0f - 1.0f;
    stroke_random_opacity_factor_ = rng_.get_float() * 2.0f - 1.0f;
    stroke_random_rotation_factor_ = rng_.get_float() * 2.0f - 1.0f;

    stroke_random_hue_factor_ = rng_.get_float() * 2.0f - 1.0f;
    stroke_random_sat_factor_ = rng_.get_float() * 2.0f - 1.0f;
    stroke_random_val_factor_ = rng_.get_float() * 2.0f - 1.0f;
  }

  Material *material = BKE_grease_pencil_object_material_ensure_from_brush(
      CTX_data_main(&C), object, brush);
  const int material_index = BKE_object_material_index_get(object, material);
  const bool use_fill = (material->gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0;

  frame_number_ = scene->r.cfra;
  drawing_ = grease_pencil->get_editable_drawing_at(layer, frame_number_);
  multi_frame_drawings_ = ed::greasepencil::retrieve_editable_drawings(*scene, *grease_pencil);
  BLI_assert(drawing_ != nullptr);

  /* We're now starting to draw. */
  grease_pencil->runtime->is_drawing_stroke = true;

  /* Initialize the start time to the current time. */
  start_time_ = BLI_time_now_seconds();
  /* Delta time starts at 0. */
  delta_time_ = 0.0f;

  PaintOperationExecutor executor{C};
  executor.process_start_sample(*this, C, start_sample, material_index, use_fill);

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

static void smooth_stroke(bke::greasepencil::Drawing &drawing,
                          const float influence,
                          const int iterations,
                          const int active_curve)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const IndexRange stroke = IndexRange::from_single(active_curve);
  const offset_indices::OffsetIndices<int> points_by_curve = drawing.strokes().points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<bool> point_selection = VArray<bool>::from_single(true, curves.points_num());

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::GSpanAttributeWriter positions = attributes.lookup_for_write_span("position");
  geometry::smooth_curve_attribute(stroke,
                                   points_by_curve,
                                   point_selection,
                                   cyclic,
                                   iterations,
                                   influence,
                                   false,
                                   true,
                                   positions.span);
  positions.finish();
  drawing.tag_positions_changed();

  if (drawing.opacities().is_span()) {
    bke::GSpanAttributeWriter opacities = attributes.lookup_for_write_span("opacity");
    geometry::smooth_curve_attribute(stroke,
                                     points_by_curve,
                                     point_selection,
                                     cyclic,
                                     iterations,
                                     influence,
                                     true,
                                     false,
                                     opacities.span);
    opacities.finish();
  }
  if (drawing.radii().is_span()) {
    bke::GSpanAttributeWriter radii = attributes.lookup_for_write_span("radius");
    geometry::smooth_curve_attribute(stroke,
                                     points_by_curve,
                                     point_selection,
                                     cyclic,
                                     iterations,
                                     influence,
                                     true,
                                     false,
                                     radii.span);
    radii.finish();
  }
}

static void simplify_stroke(bke::greasepencil::Drawing &drawing,
                            const float epsilon,
                            const int active_curve)
{
  const bke::CurvesGeometry &curves = drawing.strokes();
  const bke::AttributeAccessor attributes = curves.attributes();
  const IndexRange points = curves.points_by_curve()[active_curve];
  const VArray<float2> screen_space_positions_attribute = *attributes.lookup<float2>(
      ".draw_tool_screen_space_positions");
  BLI_assert(screen_space_positions_attribute.is_span());

  const Span<float2> screen_space_positions =
      screen_space_positions_attribute.get_internal_span().slice(points);

  Array<bool> points_to_delete_arr(drawing.strokes().points_num(), false);
  points_to_delete_arr.as_mutable_span().slice(points).fill(true);
  geometry::curve_simplify(curves.positions().slice(points),
                           curves.cyclic()[active_curve],
                           epsilon,
                           screen_space_positions,
                           points_to_delete_arr.as_mutable_span().slice(points));

  IndexMaskMemory memory;
  const IndexMask points_to_delete = IndexMask::from_bools(points_to_delete_arr, memory);
  if (!points_to_delete.is_empty()) {
    drawing.strokes_for_write().remove_points(points_to_delete, {});
    drawing.tag_topology_changed();
  }
}

static void add_strokes_to_drawing(const bool on_back,
                                   Curves *strokes,
                                   bke::greasepencil::Drawing &drawing)
{
  Curves *other_curves = bke::curves_new_nomain(std::move(drawing.strokes_for_write()));
  std::array<bke::GeometrySet, 2> geometry_sets;
  if (on_back) {
    geometry_sets = {bke::GeometrySet::from_curves(strokes),
                     bke::GeometrySet::from_curves(other_curves)};
  }
  else {
    geometry_sets = {bke::GeometrySet::from_curves(other_curves),
                     bke::GeometrySet::from_curves(strokes)};
  }
  drawing.strokes_for_write() = std::move(
      geometry::join_geometries(geometry_sets, {}).get_curves_for_write()->geometry.wrap());
  drawing.tag_topology_changed();
}

static void trim_stroke_ends(bke::greasepencil::Drawing &drawing,
                             const int active_curve,
                             const bool on_back)
{
  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange points = curves.points_by_curve()[active_curve];
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<float2> screen_space_positions_attribute = *attributes.lookup<float2>(
      ".draw_tool_screen_space_positions");
  BLI_assert(screen_space_positions_attribute.is_span());
  const Span<float2> screen_space_positions =
      screen_space_positions_attribute.get_internal_span().slice(points);
  /* Extract the drawn stroke into a separate geometry, so we can trim the ends for just this
   * stroke. */
  bke::CurvesGeometry stroke = bke::curves_copy_curve_selection(
      drawing.strokes(), IndexRange::from_single(active_curve), {});
  auto bounds = bounds::min_max(screen_space_positions);
  rcti screen_space_bounds;
  BLI_rcti_init(&screen_space_bounds,
                int(bounds->min.x),
                int(bounds->max.x),
                int(bounds->min.y),
                int(bounds->max.y));
  /* Use the first and last point. */
  const Vector<Vector<int>> point_selection = {{0, int(points.index_range().last())}};
  /* Trim the stroke ends by finding self intersections using the screen space positions. */
  bke::CurvesGeometry stroke_trimmed = ed::greasepencil::trim::trim_curve_segments(
      stroke,
      screen_space_positions,
      {screen_space_bounds},
      IndexRange::from_single(0),
      point_selection,
      true);

  /* No intersection found. */
  if (stroke_trimmed.is_empty()) {
    return;
  }

  /* Remove the original stroke. */
  drawing.strokes_for_write().remove_curves(IndexRange::from_single(active_curve), {});

  /* Join the trimmed stroke into the drawing. */
  add_strokes_to_drawing(on_back, bke::curves_new_nomain(std::move(stroke_trimmed)), drawing);
}

static void outline_stroke(bke::greasepencil::Drawing &drawing,
                           const int active_curve,
                           const float4x4 &viewmat,
                           const ed::greasepencil::DrawingPlacement &placement,
                           const float outline_radius,
                           const int material_index,
                           const bool on_back)
{
  /* Get the outline stroke (single curve). */
  bke::CurvesGeometry outline = ed::greasepencil::create_curves_outline(
      drawing,
      IndexRange::from_single(active_curve),
      viewmat,
      3,
      outline_radius,
      0.0f,
      material_index);

  /* Reproject the outline onto the drawing placement. */
  placement.reproject(outline.positions(), outline.positions_for_write());

  /* Remove the original stroke. */
  drawing.strokes_for_write().remove_curves(IndexRange::from_single(active_curve), {});

  /* Join the outline stroke into the drawing. */
  add_strokes_to_drawing(on_back, bke::curves_new_nomain(std::move(outline)), drawing);
}

static int trim_end_points(bke::greasepencil::Drawing &drawing,
                           const float epsilon,
                           const bool on_back,
                           const int active_curve)
{
  const IndexRange points = drawing.strokes().points_by_curve()[active_curve];
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const VArray<float> radii = drawing.radii();

  /* Remove points at the end that have a radius close to 0. */
  int64_t num_points_to_remove = 0;
  for (int64_t index = points.last(); index >= points.first(); index--) {
    if (radii[index] < epsilon) {
      num_points_to_remove++;
    }
    else {
      break;
    }
  }

  if (num_points_to_remove <= 0) {
    return 0;
  }

  /* Don't remove the entire stroke. Leave at least one point. */
  if (points.size() - num_points_to_remove < 1) {
    num_points_to_remove = points.size() - 1;
  }

  if (!on_back) {
    curves.resize(curves.points_num() - num_points_to_remove, curves.curves_num());
    curves.offsets_for_write().last() = curves.points_num();
    return num_points_to_remove;
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  const int last_active_point = curves.points_by_curve()[0].last();

  /* Shift the data before resizing to not delete the data at the end. */
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Point) {
      return;
    }

    bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(iter.name);
    GMutableSpan attribute_data = dst.span;

    bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
      using T = decltype(dummy);
      MutableSpan<T> span_data = attribute_data.typed<T>();

      for (int i = last_active_point - num_points_to_remove + 1;
           i < curves.points_num() - num_points_to_remove;
           i++)
      {
        span_data[i] = span_data[i + num_points_to_remove];
      }
    });
    dst.finish();
  });

  curves.resize(curves.points_num() - num_points_to_remove, curves.curves_num());
  MutableSpan<int> offsets = curves.offsets_for_write();
  for (const int src_curve : curves.curves_range().drop_front(1)) {
    offsets[src_curve] = offsets[src_curve] - num_points_to_remove;
  }
  offsets.last() = curves.points_num();

  return num_points_to_remove;
}

static void deselect_stroke(const bContext &C,
                            bke::greasepencil::Drawing &drawing,
                            const int active_curve)
{
  Scene *scene = CTX_data_scene(&C);
  const IndexRange points = drawing.strokes().points_by_curve()[active_curve];

  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      scene->toolsettings);

  bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
      curves, selection_domain, bke::AttrType::Bool);

  if (selection_domain == bke::AttrDomain::Curve) {
    ed::curves::fill_selection_false(selection.span.slice(IndexRange::from_single(active_curve)));
  }
  else if (selection_domain == bke::AttrDomain::Point) {
    ed::curves::fill_selection_false(selection.span.slice(points));
  }

  selection.finish();
}

static void process_stroke_weights(const Scene &scene,
                                   const Object &object,
                                   bke::greasepencil::Drawing &drawing,
                                   const int active_curve)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const IndexRange points = curves.points_by_curve()[active_curve];

  const int def_nr = BKE_object_defgroup_active_index_get(&object) - 1;

  if (def_nr == -1) {
    return;
  }

  const bDeformGroup *defgroup = static_cast<const bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(&object), def_nr));

  const StringRef vertex_group_name = defgroup->name;

  blender::bke::greasepencil::assign_to_vertex_group_from_mask(
      curves, IndexMask(points), vertex_group_name, scene.toolsettings->vgroup_weight);

  if (scene.toolsettings->vgroup_weight == 0.0f) {
    return;
  }

  /* Loop through all modifiers trying to find the pose channel for the vertex group name. */
  bPoseChannel *channel = nullptr;
  Object *ob_arm = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &(&object)->modifiers) {
    if (md->type != eModifierType_GreasePencilArmature) {
      continue;
    }

    /* Skip not visible modifiers. */
    if (!(md->mode & eModifierMode_Realtime)) {
      continue;
    }

    GreasePencilArmatureModifierData *amd = reinterpret_cast<GreasePencilArmatureModifierData *>(
        md);
    if (amd == nullptr) {
      continue;
    }

    ob_arm = amd->object;
    /* Not an armature. */
    if (ob_arm->type != OB_ARMATURE || ob_arm->pose == nullptr) {
      continue;
    }

    channel = BKE_pose_channel_find_name(ob_arm->pose, vertex_group_name.data());
    if (channel == nullptr) {
      continue;
    }

    /* Found the channel. */
    break;
  }

  /* Nothing valid was found. */
  if (channel == nullptr) {
    return;
  }

  const float4x4 obinv = math::invert(object.object_to_world());

  const float4x4 postmat = obinv * ob_arm->object_to_world();
  const float4x4 premat = math::invert(postmat);

  const float4x4 matrix = postmat * math::invert(float4x4(channel->chan_mat)) * premat;

  /* Update the position of the stroke to undo the movement caused by the modifier. */
  MutableSpan<float3> positions = curves.positions_for_write().slice(points);
  math::transform_points(matrix, positions);
}

static bke::CurvesGeometry get_single_stroke(const bke::CurvesGeometry &src, const int curve)
{

  const IndexRange points = src.points_by_curve()[curve];
  bke::CurvesGeometry dst(points.size(), 1);

  Array<int> src_offsets({points.first(), points.one_after_last()});
  Array<int> dst_offsets({0, int(points.size())});

  copy_attributes_group_to_group(src.attributes(),
                                 bke::AttrDomain::Point,
                                 bke::AttrDomain::Point,
                                 {},
                                 src_offsets.as_span(),
                                 dst_offsets.as_span(),
                                 IndexMask{1},
                                 dst.attributes_for_write());

  src_offsets = {curve, curve + 1};
  dst_offsets = {0, 1};

  copy_attributes_group_to_group(src.attributes(),
                                 bke::AttrDomain::Curve,
                                 bke::AttrDomain::Curve,
                                 {},
                                 src_offsets.as_span(),
                                 dst_offsets.as_span(),
                                 IndexMask{1},
                                 dst.attributes_for_write());
  return dst;
}

static void append_stroke_to_multiframe_drawings(
    const bke::CurvesGeometry &src_strokes,
    const int curve,
    const int current_frame,
    const bool on_back,
    Span<ed::greasepencil::MutableDrawingInfo> drawings)
{
  const bke::CurvesGeometry stroke = get_single_stroke(src_strokes, curve);

  for (const ed::greasepencil::MutableDrawingInfo &drawing_info : drawings) {
    if (drawing_info.frame_number == current_frame) {
      continue;
    }
    add_strokes_to_drawing(on_back, bke::curves_new_nomain(stroke), drawing_info.drawing);
  }
}

void PaintOperation::on_stroke_done(const bContext &C)
{
  using namespace blender::bke;
  Scene *scene = CTX_data_scene(&C);
  Object *object = CTX_data_active_object(&C);
  RegionView3D *rv3d = CTX_wm_region_view3d(&C);
  const ARegion *region = CTX_wm_region(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  Paint *paint = &scene->toolsettings->gp_paint->paint;
  Brush *brush = BKE_paint_brush(paint);
  BrushGpencilSettings *settings = brush->gpencil_settings;
  const bool on_back = (scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) != 0;
  const bool do_post_processing = (settings->flag & GP_BRUSH_GROUP_SETTINGS) != 0;
  const bool do_automerge_endpoints = (scene->toolsettings->gpencil_flags &
                                       GP_TOOL_FLAG_AUTOMERGE_STROKE) != 0;

  /* Grease Pencil should have an active layer. */
  BLI_assert(grease_pencil.has_active_layer());
  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();
  /* Drawing should exist. */
  bke::greasepencil::Drawing &drawing = *drawing_;
  const int active_curve = on_back ? drawing.strokes().curves_range().first() :
                                     drawing.strokes().curves_range().last();
  const offset_indices::OffsetIndices<int> points_by_curve = drawing.strokes().points_by_curve();
  const IndexRange points = points_by_curve[active_curve];

  /* Write the screen space positions of the new stroke as a temporary attribute, so all the
   * changes in topology with the operations below get propagated correctly. */
  bke::MutableAttributeAccessor attributes = drawing.strokes_for_write().attributes_for_write();
  bke::SpanAttributeWriter<float2> screen_space_positions =
      attributes.lookup_or_add_for_write_only_span<float2>(".draw_tool_screen_space_positions",
                                                           bke::AttrDomain::Point);
  BLI_assert(screen_space_positions);
  screen_space_positions.span.slice(points).copy_from(this->screen_space_final_coords_);
  screen_space_positions.finish();

  /* Remove trailing points with radii close to zero. */
  trim_end_points(drawing, 1e-5f, on_back, active_curve);

  /* Set the selection of the newly drawn stroke to false. */
  deselect_stroke(C, drawing, active_curve);

  if (do_post_processing) {
    if (settings->draw_smoothfac > 0.0f && settings->draw_smoothlvl > 0) {
      smooth_stroke(drawing, settings->draw_smoothfac, settings->draw_smoothlvl, active_curve);
    }
    if (settings->simplify_px > 0.0f) {
      simplify_stroke(drawing, settings->simplify_px, active_curve);
    }
    if ((settings->flag & GP_BRUSH_TRIM_STROKE) != 0) {
      trim_stroke_ends(drawing, active_curve, on_back);
    }
    if ((scene->toolsettings->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) != 0) {
      process_stroke_weights(*scene, *object, drawing, active_curve);
    }
    if ((settings->flag & GP_BRUSH_OUTLINE_STROKE) != 0) {
      const float outline_radius = brush->unprojected_size / 2.0f * settings->outline_fac * 0.5f;
      const int material_index = [&]() {
        Material *material = BKE_grease_pencil_object_material_alt_ensure_from_brush(
            CTX_data_main(&C), object, brush);
        return BKE_object_material_index_get(object, material);
      }();
      outline_stroke(drawing,
                     active_curve,
                     float4x4(rv3d->viewmat),
                     placement_,
                     outline_radius,
                     material_index,
                     on_back);
    }
  }
  /* Remove the temporary attribute. */
  attributes.remove(".draw_tool_screen_space_positions");

  drawing.set_texture_matrices({texture_space_}, IndexRange::from_single(active_curve));

  if (do_automerge_endpoints) {
    constexpr float merge_distance = 20.0f;
    const float4x4 layer_to_world = active_layer.to_world_space(*object);
    const IndexMask selection = IndexRange::from_single(active_curve);
    drawing.strokes_for_write() = ed::greasepencil::curves_merge_endpoints_by_distance(
        *region, drawing.strokes(), layer_to_world, merge_distance, selection, {});
  }

  drawing.tag_topology_changed();

  const bool use_multi_frame_editing = (scene->toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;

  if (use_multi_frame_editing) {
    append_stroke_to_multiframe_drawings(
        drawing.strokes(), active_curve, frame_number_, on_back, multi_frame_drawings_);
  }

  /* Now we're done drawing. */
  grease_pencil.runtime->is_drawing_stroke = false;

  if (do_fill_guides_) {
    this->toggle_fill_guides_brush_off(C);
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil.id);
}

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation(const bool do_fill_guides)
{
  return std::make_unique<PaintOperation>(do_fill_guides);
}

}  // namespace blender::ed::sculpt_paint::greasepencil

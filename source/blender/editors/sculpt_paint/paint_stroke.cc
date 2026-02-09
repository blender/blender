/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_rand.hh"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "IMB_imbuf_types.hh"

#include "paint_intern.hh"

#include "mesh/sculpt_cloth.hh"
#include "mesh/sculpt_intern.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time_utildefines.h"
#endif

namespace blender::ed::sculpt_paint {

/*** Cursors ***/
static void paint_draw_smooth_cursor(bContext *C,
                                     const int2 &xy,
                                     const float2 & /*tilt*/,
                                     void *customdata)
{
  PaintStroke *data = static_cast<PaintStroke *>(customdata);

  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  ARegion *region = CTX_wm_region(C);

  if ((mode == PaintMode::GPencil) && (paint->flags & PAINT_SHOW_BRUSH) == 0) {
    return;
  }

  if (data && brush) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    const uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    const uchar4 color = uchar4(255, 100, 100, 128);
    immUniformColor4ubv(color);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos, float2(xy));
    immVertex2f(pos,
                data->last_mouse_position[0] + region->winrct.xmin,
                data->last_mouse_position[1] + region->winrct.ymin);

    immEnd();

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

static void paint_draw_line_cursor(bContext * /*C*/,
                                   const int2 &xy,
                                   const float2 & /*tilt*/,
                                   void *customdata)
{
  PaintStroke *stroke = static_cast<PaintStroke *>(customdata);

  GPU_line_smooth(true);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float4 viewport_size;
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniform4f("color", 0.0f, 0.0f, 0.0f, 0.5);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 0.5);
  immUniform1f("dash_width", 6.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 2);

  const ARegion *region = stroke->vc.region;

  if (stroke->constrain_line) {
    immVertex2f(shdr_pos,
                stroke->last_mouse_position[0] + region->winrct.xmin,
                stroke->last_mouse_position[1] + region->winrct.ymin);

    immVertex2f(shdr_pos,
                stroke->constrained_pos[0] + region->winrct.xmin,
                stroke->constrained_pos[1] + region->winrct.ymin);
  }
  else {
    immVertex2f(shdr_pos,
                stroke->last_mouse_position[0] + region->winrct.xmin,
                stroke->last_mouse_position[1] + region->winrct.ymin);

    immVertex2fv(shdr_pos, float2(xy));
  }

  immEnd();

  immUnbindProgram();

  GPU_line_smooth(false);
}

static bool paint_brush_type_require_location(const Brush &brush, const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
      if (ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
               SCULPT_BRUSH_TYPE_POSE,
               SCULPT_BRUSH_TYPE_BOUNDARY,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_SNAKE_HOOK,
               SCULPT_BRUSH_TYPE_THUMB))
      {
        return false;
      }
      else if (cloth::is_cloth_deform_brush(brush)) {
        return false;
      }
      else {
        return true;
      }
    default:
      break;
  }

  return true;
}

static bool paint_stroke_use_scene_spacing(const Brush &brush, const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
      return brush.flag & BRUSH_SCENE_SPACING;
    default:
      break;
  }
  return false;
}

static bool paint_brush_type_raycast_original(const Brush &brush, PaintMode /*mode*/)
{
  return ELEM(brush.stroke_method, BRUSH_STROKE_ANCHORED, BRUSH_STROKE_DRAG_DOT);
}

static bool paint_brush_type_require_inbetween_mouse_events(const Brush &brush,
                                                            const PaintMode mode)
{
  if (brush.stroke_method == BRUSH_STROKE_ANCHORED) {
    return false;
  }

  switch (mode) {
    case PaintMode::Sculpt:
      if (ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_THUMB,
               SCULPT_BRUSH_TYPE_SNAKE_HOOK,
               SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
               SCULPT_BRUSH_TYPE_CLOTH,
               SCULPT_BRUSH_TYPE_BOUNDARY,
               SCULPT_BRUSH_TYPE_POSE))
      {
        return false;
      }
      else {
        return true;
      }
    default:
      break;
  }

  return true;
}

bool PaintStroke::update(bContext *C,
                         const Brush &brush,
                         const PaintMode mode,
                         const float mouse_init[2],
                         float mouse[2],
                         const float pressure,
                         float r_location[3],
                         bool *r_location_is_set)
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_paintmode(scene, mode);
  bke::PaintRuntime &paint_runtime = *paint->runtime;
  bool location_sampled = false;
  bool location_success = false;
  /* Use to perform all operations except applying the stroke,
   * needed for operations that require cursor motion (rake). */
  bool is_dry_run = false;
  bool do_random = false;
  bool do_random_mask = false;
  *r_location_is_set = false;
  /* XXX: Use pressure value from first brush step for brushes which don't
   *      support strokes (grab, thumb). They depends on initial state and
   *      brush coord/pressure/etc.
   *      It's more an events design issue, which doesn't split coordinate/pressure/angle
   *      changing events. We should avoid this after events system re-design */
  if (!input_init_) {
    copy_v2_v2(initial_mouse_, mouse);
    copy_v2_v2(paint_runtime.last_rake, mouse);
    copy_v2_v2(paint_runtime.tex_mouse, mouse);
    copy_v2_v2(paint_runtime.mask_tex_mouse, mouse);
    cached_size_pressure_ = pressure;

    input_init_ = true;
  }

  if (paint_supports_dynamic_size(brush, mode)) {
    copy_v2_v2(paint_runtime.tex_mouse, mouse);
    copy_v2_v2(paint_runtime.mask_tex_mouse, mouse);
  }

  /* Truly temporary data that isn't stored in properties */

  paint_runtime.stroke_active = true;
  const float pressure_to_evaluate = paint_supports_dynamic_size(brush, mode) ?
                                         pressure :
                                         cached_size_pressure_;
  paint_runtime.size_pressure_value = BKE_brush_use_size_pressure(&brush) ?
                                          BKE_curvemapping_evaluateF(
                                              brush.curve_size, 0, pressure_to_evaluate) :
                                          1.0f;

  paint_runtime.pixel_radius = BKE_brush_radius_get(paint, &brush) *
                               paint_runtime.size_pressure_value;
  paint_runtime.initial_pixel_radius = BKE_brush_radius_get(paint, &brush);

  if (paint_supports_dynamic_tex_coords(brush, mode)) {

    if (ELEM(brush.mtex.brush_map_mode,
             MTEX_MAP_MODE_VIEW,
             MTEX_MAP_MODE_AREA,
             MTEX_MAP_MODE_RANDOM))
    {
      do_random = true;
    }

    if (brush.mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      BKE_brush_randomize_texture_coords(paint, false);
    }
    else {
      copy_v2_v2(paint_runtime.tex_mouse, mouse);
    }

    /* take care of mask texture, if any */
    if (brush.mask_mtex.tex) {

      if (ELEM(brush.mask_mtex.brush_map_mode,
               MTEX_MAP_MODE_VIEW,
               MTEX_MAP_MODE_AREA,
               MTEX_MAP_MODE_RANDOM))
      {
        do_random_mask = true;
      }

      if (brush.mask_mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
        BKE_brush_randomize_texture_coords(paint, true);
      }
      else {
        copy_v2_v2(paint_runtime.mask_tex_mouse, mouse);
      }
    }
  }

  if (brush.stroke_method == BRUSH_STROKE_ANCHORED) {
    bool hit = false;
    float2 halfway;

    const float dx = mouse[0] - initial_mouse_[0];
    const float dy = mouse[1] - initial_mouse_[1];

    paint_runtime.anchored_size = paint_runtime.pixel_radius = sqrtf(dx * dx + dy * dy);

    paint_runtime.brush_rotation = paint_runtime.brush_rotation_sec = atan2f(dy, dx) +
                                                                      float(0.5f * M_PI);

    if (brush.flag & BRUSH_EDGE_TO_EDGE) {
      halfway[0] = dx * 0.5f + initial_mouse_[0];
      halfway[1] = dy * 0.5f + initial_mouse_[1];

      if (mode != PaintMode::Texture2D) {
        if (this->get_location(r_location, halfway, original_)) {
          hit = true;
          location_sampled = true;
          location_success = true;
          *r_location_is_set = true;
        }
        else if (!paint_brush_type_require_location(brush, mode)) {
          hit = true;
        }
      }
      else {
        hit = true;
      }
    }
    if (hit) {
      copy_v2_v2(paint_runtime.anchored_initial_mouse, halfway);
      copy_v2_v2(paint_runtime.tex_mouse, halfway);
      copy_v2_v2(paint_runtime.mask_tex_mouse, halfway);
      copy_v2_v2(mouse, halfway);
      paint_runtime.anchored_size /= 2.0f;
      paint_runtime.pixel_radius /= 2.0f;
      stroke_distance_ = paint_runtime.pixel_radius;
    }
    else {
      copy_v2_v2(paint_runtime.anchored_initial_mouse, initial_mouse_);
      copy_v2_v2(mouse, initial_mouse_);
      stroke_distance_ = paint_runtime.pixel_radius;
    }
    paint_runtime.pixel_radius /= zoom_2d_;
    paint_runtime.draw_anchored = true;
  }
  else {
    /* curve strokes do their own rake calculation */
    if (brush.stroke_method != BRUSH_STROKE_CURVE) {
      if (!paint_calculate_rake_rotation(*paint, brush, mouse_init, mode, rake_started_)) {
        /* Not enough motion to define an angle. */
        if (!rake_started_) {
          is_dry_run = true;
        }
      }
      else {
        rake_started_ = true;
      }
    }
  }

  if ((do_random || do_random_mask) && !rng_) {
    /* Lazy initialization. */
    rng_ = RandomNumberGenerator::from_random_seed();
  }

  if (do_random) {
    if (brush.mtex.brush_angle_mode & MTEX_ANGLE_RANDOM) {
      paint_runtime.brush_rotation += -brush.mtex.random_angle / 2.0f +
                                      brush.mtex.random_angle * rng_->get_float();
    }
  }

  if (do_random_mask) {
    if (brush.mask_mtex.brush_angle_mode & MTEX_ANGLE_RANDOM) {
      paint_runtime.brush_rotation_sec += -brush.mask_mtex.random_angle / 2.0f +
                                          brush.mask_mtex.random_angle * rng_->get_float();
    }
  }

  if (!location_sampled) {
    if (mode != PaintMode::Texture2D) {
      if (this->get_location(r_location, mouse, original_)) {
        location_success = true;
        *r_location_is_set = true;
      }
      else if (!paint_brush_type_require_location(brush, mode)) {
        location_success = true;
      }
    }
    else {
      zero_v3(r_location);
      location_success = true;
      /* don't set 'r_location_is_set', since we don't want to use the value. */
    }
  }

  return location_success && !is_dry_run;
}

static bool paint_stroke_use_dash(const Brush &brush)
{
  /* Only these stroke modes support dash lines */
  return ELEM(brush.stroke_method, BRUSH_STROKE_SPACE, BRUSH_STROKE_LINE, BRUSH_STROKE_CURVE);
}

static bool paint_stroke_use_jitter(const PaintMode mode, const Brush &brush, const bool invert)
{
  bool use_jitter = brush.flag & BRUSH_ABSOLUTE_JITTER ? brush.jitter_absolute != 0 :
                                                         brush.jitter != 0;

  /* jitter-ed brush gives weird and unpredictable result for this
   * kinds of stroke, so manually disable jitter usage (sergey) */
  use_jitter &= (ELEM(brush.stroke_method, BRUSH_STROKE_DRAG_DOT, BRUSH_STROKE_ANCHORED)) == 0;
  use_jitter &= !ELEM(mode, PaintMode::Texture2D, PaintMode::Texture3D) ||
                !(invert && brush.image_brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE);

  return use_jitter;
}

void paint_stroke_jitter_pos(Paint *paint,
                             PaintMode mode,
                             const Brush &brush,
                             float pressure,
                             BrushStrokeMode stroke_mode,
                             float zoom_2d,
                             const float mval[2],
                             float r_mouse_out[2])
{
  if (paint_stroke_use_jitter(mode, brush, stroke_mode == BrushStrokeMode::Invert)) {
    float factor = zoom_2d;

    if (brush.flag & BRUSH_JITTER_PRESSURE) {
      factor *= BKE_curvemapping_evaluateF(brush.curve_jitter, 0, pressure);
    }

    BKE_brush_jitter_pos(*paint, brush, mval, r_mouse_out);

    /* XXX: meh, this is round about because
     * BKE_brush_jitter_pos isn't written in the best way to
     * be reused here */
    if (factor != 1.0f) {
      float2 delta;
      sub_v2_v2v2(delta, r_mouse_out, mval);
      mul_v2_fl(delta, factor);
      add_v2_v2v2(r_mouse_out, mval, delta);
    }
  }
  else {
    copy_v2_v2(r_mouse_out, mval);
  }
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
void PaintStroke::add_step(bContext *C, wmOperator *op, const float2 mval, float pressure)
{
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const Brush &brush = *BKE_paint_brush_for_read(this->paint);
  bke::PaintRuntime *paint_runtime = this->paint->runtime;

/* the following code is adapted from texture paint. It may not be needed but leaving here
 * just in case for reference (code in texpaint removed as part of refactoring).
 * It's strange that only texpaint had these guards. */
#if 0
  /* special exception here for too high pressure values on first touch in
   * windows for some tablets, then we just skip first touch. */
  if (tablet && (pressure >= 0.99f) &&
      ((pop->s.brush.flag & BRUSH_SPACING_PRESSURE) ||
       BKE_brush_use_alpha_pressure(pop->s.brush) || BKE_brush_use_size_pressure(pop->s.brush)))
  {
    return;
  }

  /* This can be removed once fixed properly in
   * BKE_brush_painter_paint(
   *     BrushPainter *painter, BrushFunc func,
   *     float *pos, double time, float pressure, void *user);
   * at zero pressure we should do nothing 1/2^12 is 0.0002
   * which is the sensitivity of the most sensitive pen tablet available */
  if (tablet && (pressure < 0.0002f) &&
      ((pop->s.brush.flag & BRUSH_SPACING_PRESSURE) ||
       BKE_brush_use_alpha_pressure(pop->s.brush) || BKE_brush_use_size_pressure(pop->s.brush)))
  {
    return;
  }
#endif

  /* copy last position -before- jittering, or space fill code
   * will create too many dabs */
  this->last_mouse_position = mval;
  last_pressure_ = pressure;

  if (paint_stroke_use_scene_spacing(brush, mode)) {
    BLI_assert(mode != PaintMode::Texture2D);
    float3 world_space_position;

    if (this->get_location(world_space_position, this->last_mouse_position, original_)) {
      last_world_space_position_ = math::transform_point(this->vc.obact->object_to_world(),
                                                         world_space_position);
    }
    else {
      last_world_space_position_ += last_scene_spacing_delta_;
    }
  }

  float2 mouse_out;
  /* Get jitter position (same as mval if no jitter is used). */
  paint_stroke_jitter_pos(
      this->paint, mode, brush, pressure, stroke_mode_, zoom_2d_, mval, mouse_out);

  float3 location;
  bool is_location_is_set;
  paint_runtime->last_hit = update(
      C, brush, mode, mval, mouse_out, pressure, location, &is_location_is_set);
  if (is_location_is_set) {
    copy_v3_v3(paint_runtime->last_location, location);
  }
  if (!paint_runtime->last_hit) {
    return;
  }

  /* Dash */
  bool add_step = true;
  if (paint_stroke_use_dash(brush)) {
    const int dash_samples = tot_samples_ % brush.dash_samples;
    const float dash = float(dash_samples) / float(brush.dash_samples);
    if (dash > brush.dash_ratio) {
      add_step = false;
    }
  }

  /* Add to stroke */
  if (add_step) {
    PointerRNA itemptr;
    RNA_collection_add(op->ptr, "stroke", &itemptr);
    RNA_float_set(&itemptr, "size", paint_runtime->pixel_radius);
    RNA_float_set_array(&itemptr, "location", location);
    /* Mouse coordinates modified by the stroke type options. */
    RNA_float_set_array(&itemptr, "mouse", mouse_out);
    /* Original mouse coordinates. */
    RNA_float_set_array(&itemptr, "mouse_event", mval);
    RNA_float_set(&itemptr, "pressure", pressure);
    RNA_float_set(&itemptr, "x_tilt", tilt_.x);
    RNA_float_set(&itemptr, "y_tilt", tilt_.y);

    this->update_step(op, &itemptr);

    /* don't record this for now, it takes up a lot of memory when doing long
     * strokes with small brush size, and operators have register disabled */
    RNA_collection_clear(op->ptr, "stroke");
  }

  tot_samples_++;
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static bool paint_smooth_stroke(const Brush &brush,
                                const PaintSample *sample,
                                const PaintMode mode,
                                const BrushSwitchMode brush_switch_mode,
                                float zoom_2d,
                                float2 last_mouse_position,
                                float last_pressure,
                                float2 &r_mouse,
                                float &r_pressure)
{
  if (paint_supports_smooth_stroke(brush, mode, brush_switch_mode)) {
    const float radius = brush.smooth_stroke_radius * zoom_2d;
    const float u = brush.smooth_stroke_factor;

    /* If the mouse is moving within the radius of the last move,
     * don't update the mouse position. This allows sharp turns. */
    if (math::distance_squared(last_mouse_position, sample->mouse) < square_f(radius)) {
      return false;
    }

    r_mouse = math::interpolate(sample->mouse, last_mouse_position, u);
    r_pressure = math::interpolate(sample->pressure, last_pressure, u);
  }
  else {
    r_mouse = sample->mouse;
    r_pressure = sample->pressure;
  }

  return true;
}

static float paint_space_stroke_spacing(const ViewContext &vc,
                                        const Paint *paint,
                                        const Brush *brush,
                                        float3 last_world_space_position,
                                        float zoom_2d,
                                        const float size_factor,
                                        const float pressure)
{
  const PaintMode mode = paint->runtime->paint_mode;

  float size_clamp = 0.0f;
  if (paint_stroke_use_scene_spacing(*brush, mode)) {
    const float3 last_object_space_position = math::transform_point(vc.obact->world_to_object(),
                                                                    last_world_space_position);
    size_clamp = object_space_radius_get(
        vc, *paint, *brush, last_object_space_position, size_factor);
  }
  else {
    /* brushes can have a minimum size of 1.0 but with pressure it can be smaller than a pixel
     * causing very high step sizes, hanging blender #32381. */
    size_clamp = max_ff(1.0f, BKE_brush_radius_get(paint, brush) * size_factor);
  }

  float spacing = brush->spacing;

  /* apply spacing pressure */
  if (brush->stroke_method == BRUSH_STROKE_SPACE && brush->flag & BRUSH_SPACING_PRESSURE) {
    spacing = spacing * (1.5f - pressure);
  }

  if (cloth::is_cloth_deform_brush(*brush)) {
    /* The spacing in tools that use the cloth solver should not be affected by the brush radius to
     * avoid affecting the simulation update rate when changing the radius of the brush.
     * With a value of 100 and the brush default of 10 for spacing, a simulation step runs every 2
     * pixels movement of the cursor. */
    size_clamp = 100.0f;
  }

  /* stroke system is used for 2d paint too, so we need to account for
   * the fact that brush can be scaled there. */
  spacing *= zoom_2d;

  if (paint_stroke_use_scene_spacing(*brush, mode)) {
    /* Low pressure on size (with tablets) can cause infinite recursion in paint_space_stroke(),
     * see #129853. */
    return max_ff(FLT_EPSILON, size_clamp * spacing / 50.0f);
  }
  return max_ff(zoom_2d, size_clamp * spacing / 50.0f);
}

static float paint_space_stroke_spacing_no_pressure(const ViewContext &vc,
                                                    const Paint *paint,
                                                    const Brush *brush,
                                                    float3 last_world_space_position,
                                                    float zoom_2d)
{
  /* Unlike many paint pressure curves, spacing assumes that a stroke without pressure (e.g. with
   * the mouse, or with the setting turned off) represents an input of 0.5, not 1.0. */
  return paint_space_stroke_spacing(
      vc, paint, brush, last_world_space_position, zoom_2d, 1.0f, 0.5f);
}

static float paint_stroke_overlapped_curve(const Brush &br, const float x, const float spacing)
{
  /* Avoid division by small numbers, can happen
   * on some pen setups. See #105341.
   */

  const float clamped_spacing = max_ff(spacing, 0.1f);

  const int n = 100 / clamped_spacing;
  const float h = clamped_spacing / 50.0f;
  const float x0 = x - 1;

  float sum = 0;
  for (int i = 0; i < n; i++) {
    float xx = fabsf(x0 + i * h);

    if (xx < 1.0f) {
      sum += BKE_brush_curve_strength(&br, xx, 1);
    }
  }

  return sum;
}

static float paint_stroke_integrate_overlap(const Brush &br, const float factor)
{
  const float spacing = br.spacing * factor;

  if (!(br.flag & BRUSH_SPACE_ATTEN && (br.spacing < 100))) {
    return 1.0;
  }

  constexpr int m = 10;
  float g = 1.0f / m;
  float max = 0;
  for (int i = 0; i < m; i++) {
    const float overlap = fabs(paint_stroke_overlapped_curve(br, i * g, spacing));

    max = std::max(overlap, max);
  }

  if (max == 0.0f) {
    return 1.0f;
  }
  return 1.0f / max;
}

static float paint_space_stroke_spacing_variable(ViewContext &vc,
                                                 const Paint *paint,
                                                 const Brush *brush,
                                                 float3 last_world_space_position,
                                                 float zoom_2d,
                                                 const float last_pressure,
                                                 const float pressure,
                                                 const float pressure_delta,
                                                 const float length)
{
  if (BKE_brush_use_size_pressure(brush)) {
    const float max_size_factor = BKE_curvemapping_evaluateF(brush->curve_size, 0, 1.0f);
    /* use pressure to modify size. set spacing so that at 100%, the circles
     * are aligned nicely with no overlap. for this the spacing needs to be
     * the average of the previous and next size. */
    const float s = paint_space_stroke_spacing(
        vc, paint, brush, last_world_space_position, zoom_2d, max_size_factor, pressure);
    const float q = s * pressure_delta / (2.0f * length);
    const float pressure_fac = (1.0f + q) / (1.0f - q);

    const float last_size_factor = BKE_curvemapping_evaluateF(brush->curve_size, 0, last_pressure);
    const float new_size_factor = BKE_curvemapping_evaluateF(
        brush->curve_size, 0, last_pressure * pressure_fac);

    /* average spacing */
    const float last_spacing = paint_space_stroke_spacing(
        vc, paint, brush, last_world_space_position, zoom_2d, last_size_factor, pressure);
    const float new_spacing = paint_space_stroke_spacing(
        vc, paint, brush, last_world_space_position, zoom_2d, new_size_factor, pressure);

    return 0.5f * (last_spacing + new_spacing);
  }

  /* no size pressure */
  return paint_space_stroke_spacing(
      vc, paint, brush, last_world_space_position, zoom_2d, 1.0f, pressure);
}

/* For brushes with stroke spacing enabled, moves mouse in steps
 * towards the final mouse location. */
int PaintStroke::space_stroke(bContext *C,
                              wmOperator *op,
                              const float2 final_mouse,
                              const float final_pressure)
{
  const ARegion *region = CTX_wm_region(C);
  bke::PaintRuntime *paint_runtime = this->paint->runtime;
  const Paint &paint = *BKE_paint_get_active_from_context(C);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const Brush &brush = *BKE_paint_brush_for_read(&paint);

  float2 mouse_delta = final_mouse - this->last_mouse_position;
  float length = normalize_v2(mouse_delta);

  float3 world_space_position_delta;
  const bool use_scene_spacing = paint_stroke_use_scene_spacing(brush, mode);
  if (use_scene_spacing) {
    BLI_assert(mode != PaintMode::Texture2D);
    float3 world_space_position;
    const bool hit = this->get_location(world_space_position, final_mouse, original_);
    world_space_position = math::transform_point(this->vc.obact->object_to_world(),
                                                 world_space_position);
    if (hit && stroke_over_mesh_) {
      world_space_position_delta = world_space_position - last_world_space_position_;
      length = math::length(world_space_position_delta);
      stroke_over_mesh_ = true;
    }
    else {
      length = 0.0f;
      world_space_position_delta = {0.0f, 0.0f, 0.0f};
      stroke_over_mesh_ = hit;
      if (stroke_over_mesh_) {
        last_world_space_position_ = world_space_position;
      }
    }
  }

  float pressure = last_pressure_;
  float pressure_delta = final_pressure - last_pressure_;
  const float no_pressure_spacing = paint_space_stroke_spacing_no_pressure(
      this->vc, &paint, &brush, last_world_space_position_, zoom_2d_);
  int count = 0;
  while (length > 0.0f) {
    const float spacing = paint_space_stroke_spacing_variable(this->vc,
                                                              &paint,
                                                              &brush,
                                                              last_world_space_position_,
                                                              zoom_2d_,
                                                              last_pressure_,
                                                              pressure,
                                                              pressure_delta,
                                                              length);
    BLI_assert(spacing >= 0.0f);

    if (length >= spacing) {
      float2 mouse;
      if (use_scene_spacing) {
        float3 final_world_space_position;
        world_space_position_delta = math::normalize(world_space_position_delta);
        final_world_space_position = world_space_position_delta * spacing +
                                     last_world_space_position_;
        ED_view3d_project_v2(region, final_world_space_position, mouse);

        last_scene_spacing_delta_ = world_space_position_delta * spacing;
      }
      else {
        mouse = this->last_mouse_position + mouse_delta * spacing;
      }
      pressure = last_pressure_ + (spacing / length) * pressure_delta;

      paint_runtime->overlap_factor = paint_stroke_integrate_overlap(
          brush, spacing / no_pressure_spacing);

      stroke_distance_ += spacing / zoom_2d_;
      this->add_step(C, op, mouse, pressure);

      length -= spacing;
      pressure = last_pressure_;
      pressure_delta = final_pressure - last_pressure_;

      count++;
    }
    else {
      break;
    }
  }

  return count;
}

static bool print_pressure_status_enabled()
{
  return U.tablet_flag & USER_TABLET_SHOW_DEBUG_VALUES;
}

/**** Public API ****/

PaintStroke::PaintStroke(bContext *C, wmOperator *op, int event_type) : event_type_(event_type)
{
  this->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  this->paint = BKE_paint_get_active_from_context(C);
  this->ups = &paint->unified_paint_settings;
  bke::PaintRuntime *paint_runtime = this->paint->runtime;
  this->brush = BKE_paint_brush(this->paint);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  this->evil_C = C;
  this->vc = ED_view3d_viewcontext_init(C, this->depsgraph);
  this->object = CTX_data_active_object(C);
  this->scene = CTX_data_scene(C);

  stroke_mode_ = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));
  brush_switch_mode_ = BrushSwitchMode(RNA_enum_get(op->ptr, "brush_toggle"));

  original_ = paint_brush_type_raycast_original(*this->brush,
                                                BKE_paintmode_get_active_from_context(C));

  float zoomx;
  float zoomy;
  get_imapaint_zoom(C, &zoomx, &zoomy);
  zoom_2d_ = std::max(zoomx, zoomy);

  /* Check here if color sampling the main brush should do color conversion. This is done here
   * to avoid locking up to get the image buffer during sampling. */
  paint_runtime->do_linear_conversion = false;
  paint_runtime->colorspace = nullptr;

  if (this->brush->mtex.tex && this->brush->mtex.tex->type == TEX_IMAGE &&
      this->brush->mtex.tex->ima)
  {
    ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(
        this->brush->mtex.tex->ima, &this->brush->mtex.tex->iuser, nullptr);
    if (tex_ibuf && tex_ibuf->float_buffer.data == nullptr) {
      paint_runtime->do_linear_conversion = true;
      paint_runtime->colorspace = tex_ibuf->byte_buffer.colorspace;
    }
    BKE_image_pool_release_ibuf(this->brush->mtex.tex->ima, tex_ibuf, nullptr);
  }

  if (stroke_mode_ == BrushStrokeMode::Invert) {
    if (this->brush->stroke_method == BRUSH_STROKE_CURVE) {
      RNA_enum_set(op->ptr, "mode", int(BrushStrokeMode::Normal));
    }
  }
  /* initialize here */
  paint_runtime->overlap_factor = 1.0;
  paint_runtime->stroke_active = true;

  if (rv3d) {
    rv3d->rflag |= RV3D_PAINTING;
  }

  /* Preserve location from last stroke while applying and resetting
   * ups->average_stroke_counter to 1.
   */
  if (paint_runtime->average_stroke_counter) {
    mul_v3_fl(paint_runtime->average_stroke_accum,
              1.0f / float(paint_runtime->average_stroke_counter));
    paint_runtime->average_stroke_counter = 1;
  }

  /* initialize here to avoid initialization conflict with threaded strokes */
  bke::brush::common_pressure_curves_init(*this->brush);
  if (this->paint->flags & PAINT_USE_CAVITY_MASK) {
    BKE_curvemapping_init(this->paint->cavity_curve);
  }

  BKE_paint_set_overlay_override(eOverlayFlags(this->brush->overlay_flags));

  paint_runtime->start_pixel_radius = BKE_brush_radius_get(this->paint, this->brush);
}

void PaintStroke::free(bContext *C, wmOperator * /*op*/)
{
  if (RegionView3D *rv3d = CTX_wm_region_view3d(C)) {
    rv3d->rflag &= ~RV3D_PAINTING;
  }

  BKE_paint_set_overlay_override(eOverlayFlags(0));

  bke::PaintRuntime *paint_runtime = this->paint->runtime;
  paint_runtime->draw_anchored = false;
  paint_runtime->stroke_active = false;

  if (timer_) {
    WM_event_timer_remove(CTX_wm_manager(C), CTX_wm_window(C), timer_);
  }

  if (stroke_cursor_) {
    WM_paint_cursor_end(static_cast<wmPaintCursor *>(stroke_cursor_));
  }
}

void PaintStroke::stroke_done(bContext *C, wmOperator *op, const bool is_cancel)
{
  if (print_pressure_status_enabled()) {
    ED_workspace_status_text(C, nullptr);
  }
  bke::PaintRuntime *paint_runtime = this->paint->runtime;

  /* reset rotation here to avoid doing so in cursor display */
  if (this->brush) {
    if (!(this->brush->mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
      paint_runtime->brush_rotation = 0.0f;
    }

    if (!(this->brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
      paint_runtime->brush_rotation_sec = 0.0f;
    }
  }

  if (stroke_started_) {
    this->redraw(true);

    this->done(is_cancel);
  }

  this->free(C, op);
}

static bool curves_sculpt_brush_uses_spacing(const eBrushCurvesSculptType tool)
{
  return ELEM(tool, CURVES_SCULPT_BRUSH_TYPE_ADD, CURVES_SCULPT_BRUSH_TYPE_DENSITY);
}

bool paint_space_stroke_enabled(const Brush &br, const PaintMode mode)
{
  if (br.stroke_method != BRUSH_STROKE_SPACE) {
    return false;
  }

  if (br.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH || cloth::is_cloth_deform_brush(br)) {
    /* The Cloth Brush is a special case for stroke spacing. Even if it has grab modes which do
     * not support dynamic size, stroke spacing needs to be enabled so it is possible to control
     * whether the simulation runs constantly or only when the brush moves when using the cloth
     * grab brushes. */
    return true;
  }

  if (mode == PaintMode::SculptCurves &&
      !curves_sculpt_brush_uses_spacing(eBrushCurvesSculptType(br.curves_sculpt_brush_type)))
  {
    return false;
  }

  if (ELEM(mode, PaintMode::GPencil, PaintMode::SculptGPencil)) {
    /* No spacing needed for now. */
    return false;
  }

  return paint_supports_dynamic_size(br, mode);
}

static bool sculpt_is_grab_tool(const Brush &br)
{
  if (br.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH &&
      br.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB)
  {
    return true;
  }
  return ELEM(br.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_GRAB,
              SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
              SCULPT_BRUSH_TYPE_POSE,
              SCULPT_BRUSH_TYPE_BOUNDARY,
              SCULPT_BRUSH_TYPE_THUMB,
              SCULPT_BRUSH_TYPE_ROTATE,
              SCULPT_BRUSH_TYPE_SNAKE_HOOK);
}

bool paint_supports_dynamic_size(const Brush &br, const PaintMode mode)
{
  if (br.stroke_method == BRUSH_STROKE_ANCHORED) {
    return false;
  }

  switch (mode) {
    case PaintMode::Sculpt:
      return bke::brush::supports_size_pressure(br);
      break;

    case PaintMode::Texture2D: /* fall through */
    case PaintMode::Texture3D:
      if ((br.image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) && (br.flag & BRUSH_USE_GRADIENT)) {
        return false;
      }
      break;

    default:
      break;
  }
  return true;
}

bool paint_supports_smooth_stroke(const Brush &brush,
                                  const PaintMode mode,
                                  const BrushSwitchMode brush_switch_mode)
{
  /* The grease pencil draw tool needs to enable this when the `stroke_mode` is set to
   * `BrushSwitchMode::Smooth`. */
  if (mode == PaintMode::GPencil &&
      eBrushGPaintType(brush.gpencil_brush_type) == GPAINT_BRUSH_TYPE_DRAW &&
      brush_switch_mode == BrushSwitchMode::Smooth)
  {
    return true;
  }
  if (!(brush.flag & BRUSH_SMOOTH_STROKE) ||
      ELEM(brush.stroke_method, BRUSH_STROKE_ANCHORED | BRUSH_STROKE_DRAG_DOT | BRUSH_STROKE_LINE))
  {
    return false;
  }

  switch (mode) {
    case PaintMode::Sculpt:
      if (sculpt_is_grab_tool(brush)) {
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

bool paint_supports_texture(const PaintMode mode)
{
  /* omit: PAINT_WEIGHT, PAINT_SCULPT_UV, PAINT_INVALID */
  return ELEM(
      mode, PaintMode::Sculpt, PaintMode::Vertex, PaintMode::Texture3D, PaintMode::Texture2D);
}

bool paint_supports_dynamic_tex_coords(const Brush &br, const PaintMode mode)
{
  if (br.stroke_method == BRUSH_STROKE_ANCHORED) {
    return false;
  }

  switch (mode) {
    case PaintMode::Sculpt:
      if (sculpt_is_grab_tool(br)) {
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

#define PAINT_STROKE_MODAL_CANCEL 1

wmKeyMap *paint_stroke_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {PAINT_STROKE_MODAL_CANCEL, "CANCEL", 0, "Cancel", "Cancel and undo a stroke in progress"},
      {0}};

  static const char *name = "Paint Stroke Modal";

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, name);

  /* This function is called for each space-type, only needs to add map once. */
  if (!keymap) {
    keymap = WM_modalkeymap_ensure(keyconf, name, modal_items);
  }

  return keymap;
}

void PaintStroke::add_sample(const int input_samples,
                             const float x,
                             const float y,
                             const float pressure)
{
  PaintSample *sample = &samples_[cur_sample_];
  const int max_samples = std::clamp(input_samples, 1, PAINT_MAX_INPUT_SAMPLES);

  sample->mouse[0] = x;
  sample->mouse[1] = y;
  sample->pressure = pressure;

  cur_sample_++;
  if (cur_sample_ >= max_samples) {
    cur_sample_ = 0;
  }
  if (num_samples_ < max_samples) {
    num_samples_++;
  }
}

void PaintStroke::calc_average_sample(PaintSample *average)
{
  BLI_assert(num_samples_ > 0);

  for (int i = 0; i < num_samples_; i++) {
    average->mouse += samples_[i].mouse;
    average->pressure += samples_[i].pressure;
  }

  average->mouse /= num_samples_;
  average->pressure /= num_samples_;

  // printf("avg=(%f, %f), num=%d\n", average->mouse[0], average->mouse[1], stroke->num_samples);
}

/**
 * Slightly different version of spacing for line/curve strokes,
 * makes sure the dabs stay on the line path.
 */
void PaintStroke::lines_spacing(bContext *C,
                                wmOperator *op,
                                const float spacing,
                                float *length_residue,
                                const float2 old_pos,
                                const float2 new_pos)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  bke::PaintRuntime *paint_runtime = paint->runtime;
  const Brush &brush = *BKE_paint_brush(paint);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const ARegion *region = CTX_wm_region(C);

  const bool use_scene_spacing = paint_stroke_use_scene_spacing(brush, mode);

  float2 mouse_delta;
  float length;
  float3 world_space_position_delta;
  float3 world_space_position_old;

  this->last_mouse_position = old_pos;

  if (use_scene_spacing) {
    BLI_assert(mode != PaintMode::Texture2D);
    const bool hit_old = this->get_location(world_space_position_old, old_pos, original_);

    float3 world_space_position_new;
    const bool hit_new = this->get_location(world_space_position_new, new_pos, original_);

    world_space_position_old = math::transform_point(this->vc.obact->object_to_world(),
                                                     world_space_position_old);
    world_space_position_new = math::transform_point(this->vc.obact->object_to_world(),
                                                     world_space_position_new);
    if (hit_old && hit_new && stroke_over_mesh_) {
      world_space_position_delta = world_space_position_new - world_space_position_old;
      length = math::length(world_space_position_delta);
      stroke_over_mesh_ = true;
    }
    else {
      length = 0.0f;
      world_space_position_delta = {0.0f, 0.0f, 0.0f};
      stroke_over_mesh_ = hit_new;
      if (stroke_over_mesh_) {
        last_world_space_position_ = world_space_position_old;
      }
    }
  }
  else {
    mouse_delta = new_pos - old_pos;
    mouse_delta = math::normalize_and_get_length(mouse_delta, length);
  }

  BLI_assert(length >= 0.0f);

  if (length == 0.0f) {
    return;
  }

  float2 mouse;
  while (length > 0.0f) {
    float spacing_final = spacing - *length_residue;
    length += *length_residue;
    *length_residue = 0.0;

    if (length >= spacing) {
      if (use_scene_spacing) {
        world_space_position_delta = math::normalize(world_space_position_delta);
        const float3 final_world_space_position = world_space_position_delta * spacing_final +
                                                  world_space_position_old;
        ED_view3d_project_v2(region, final_world_space_position, mouse);
      }
      else {
        mouse = this->last_mouse_position + mouse_delta * spacing_final;
      }

      paint_runtime->overlap_factor = paint_stroke_integrate_overlap(brush, 1.0);

      stroke_distance_ += spacing / zoom_2d_;
      this->add_step(C, op, mouse, 1.0);

      length -= spacing;
      spacing_final = spacing;
    }
    else {
      break;
    }
  }

  *length_residue = length;
}

void PaintStroke::line_end(bContext *C, wmOperator *op, const float2 mouse)
{
  Brush *br = this->brush;
  bke::PaintRuntime *paint_runtime = this->paint->runtime;
  if (stroke_started_ && br->stroke_method == BRUSH_STROKE_LINE) {
    paint_runtime->overlap_factor = paint_stroke_integrate_overlap(*br, 1.0);

    this->add_step(C, op, this->last_mouse_position, 1.0);
    this->space_stroke(C, op, mouse, 1.0);
  }
}

bool PaintStroke::curve_end(bContext *C, wmOperator *op)
{
  const Brush &br = *this->brush;
  if (br.stroke_method != BRUSH_STROKE_CURVE) {
    return false;
  }

  Paint *paint = BKE_paint_get_active_from_context(C);
  bke::PaintRuntime *paint_runtime = paint->runtime;
  const PaintMode mode = paint_runtime->paint_mode;
  const float no_pressure_spacing = paint_space_stroke_spacing_no_pressure(
      this->vc, paint, this->brush, last_world_space_position_, zoom_2d_);
  const PaintCurve *pc = br.paint_curve;

  if (!pc) {
    return true;
  }

#ifdef DEBUG_TIME
  TIMEIT_START_AVERAGED(whole_stroke);
#endif

  const PaintCurvePoint *pcp = pc->points;
  paint_runtime->overlap_factor = paint_stroke_integrate_overlap(br, 1.0);

  float length_residue = 0.0f;
  for (int i = 0; i < pc->tot_points - 1; i++, pcp++) {
    float data[(PAINT_CURVE_NUM_SEGMENTS + 1) * 2];
    float tangents[(PAINT_CURVE_NUM_SEGMENTS + 1) * 2];
    const PaintCurvePoint *pcp_next = pcp + 1;
    bool do_rake = false;

    for (int j = 0; j < 2; j++) {
      BKE_curve_forward_diff_bezier(pcp->bez.vec[1][j],
                                    pcp->bez.vec[2][j],
                                    pcp_next->bez.vec[0][j],
                                    pcp_next->bez.vec[1][j],
                                    data + j,
                                    PAINT_CURVE_NUM_SEGMENTS,
                                    sizeof(float[2]));
    }

    if ((br.mtex.brush_angle_mode & MTEX_ANGLE_RAKE) ||
        (br.mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE))
    {
      do_rake = true;
      for (int j = 0; j < 2; j++) {
        BKE_curve_forward_diff_tangent_bezier(pcp->bez.vec[1][j],
                                              pcp->bez.vec[2][j],
                                              pcp_next->bez.vec[0][j],
                                              pcp_next->bez.vec[1][j],
                                              tangents + j,
                                              PAINT_CURVE_NUM_SEGMENTS,
                                              sizeof(float[2]));
      }
    }

    for (int j = 0; j < PAINT_CURVE_NUM_SEGMENTS; j++) {
      if (do_rake) {
        const float rotation = atan2f(tangents[2 * j + 1], tangents[2 * j]) + float(0.5f * M_PI);
        paint_update_brush_rake_rotation(*paint, br, rotation);
      }

      if (!stroke_started_) {
        last_pressure_ = 1.0;
        copy_v2_v2(this->last_mouse_position, data + 2 * j);

        if (paint_stroke_use_scene_spacing(br, mode)) {
          BLI_assert(mode != PaintMode::Texture2D);
          stroke_over_mesh_ = this->get_location(
              last_world_space_position_, data + 2 * j, original_);
          mul_m4_v3(this->vc.obact->object_to_world().ptr(), last_world_space_position_);
        }

        stroke_started_ = this->test_start(op, this->last_mouse_position);

        if (stroke_started_) {
          this->add_step(C, op, data + 2 * j, 1.0);
          this->lines_spacing(
              C, op, no_pressure_spacing, &length_residue, data + 2 * j, data + 2 * (j + 1));
        }
      }
      else {
        this->lines_spacing(
            C, op, no_pressure_spacing, &length_residue, data + 2 * j, data + 2 * (j + 1));
      }
    }
  }

  this->stroke_done(C, op, false);

#ifdef DEBUG_TIME
  TIMEIT_END_AVERAGED(whole_stroke);
#endif

  return true;
}

static void paint_stroke_line_constrain(float2 last_mouse_position,
                                        float2 constrained_pos,
                                        float2 &mouse)
{
  float2 line = mouse - last_mouse_position;
  float angle = atan2f(line[1], line[0]);
  const float len = math::length(line);

  /* divide angle by PI/4 */
  angle = 4.0f * angle / float(M_PI);

  /* now take residue */
  const float res = angle - floorf(angle);

  /* residue decides how close we are at a certain angle */
  if (res <= 0.5f) {
    angle = floorf(angle) * float(M_PI_4);
  }
  else {
    angle = (floorf(angle) + 1.0f) * float(M_PI_4);
  }

  mouse[0] = constrained_pos[0] = len * cosf(angle) + last_mouse_position[0];
  mouse[1] = constrained_pos[1] = len * sinf(angle) + last_mouse_position[1];
}

wmOperatorStatus PaintStroke::modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* TODO: Temporary, used to facilitate removing bContext usage in subclasses */
  this->evil_C = C;

  Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *br = this->brush = BKE_paint_brush(paint);
  if (paint == nullptr || br == nullptr) {
    /* In some circumstances, the context may change during modal execution. In this case,
     * we need to cancel the operator. See #147544 and related issues for further information. */
    this->stroke_done(C, op, true);
    return OPERATOR_CANCELLED;
  }
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  bke::PaintRuntime &paint_runtime = *paint->runtime;
  bool first_dab = false;
  bool first_modal = false;
  bool needs_redraw = false;

  if (event->type == INBETWEEN_MOUSEMOVE &&
      !paint_brush_type_require_inbetween_mouse_events(*br, mode))
  {
    return OPERATOR_RUNNING_MODAL;
  }

  /* see if tablet affects event. Line, anchored and drag dot strokes do not support pressure */
  const float tablet_pressure = WM_event_tablet_data(event, &pen_flip_, nullptr);
  float pressure =
      ELEM(br->stroke_method, BRUSH_STROKE_LINE, BRUSH_STROKE_ANCHORED, BRUSH_STROKE_DRAG_DOT) ?
          1.0f :
          tablet_pressure;

  if (print_pressure_status_enabled() && WM_event_is_tablet(event)) {
    std::string msg = fmt::format("Tablet Pressure: {:.4f}", pressure);
    ED_workspace_status_text(C, msg.c_str());
  }

  /* When processing a timer event the pressure from the event is 0, so use the last valid
   * pressure. */
  if (event->type == TIMER) {
    pressure = last_tablet_event_pressure_;
  }
  else {
    last_tablet_event_pressure_ = pressure;
  }

  const int input_samples = BKE_brush_input_samples_get(paint, br);
  this->add_sample(input_samples, event->mval[0], event->mval[1], pressure);

  PaintSample sample_average;
  this->calc_average_sample(&sample_average);

  /* Tilt. */
  if (WM_event_is_tablet(event)) {
    tilt_ = event->tablet.tilt;
  }

#ifdef WITH_INPUT_NDOF
  /* let NDOF motion pass through to the 3D view so we can paint and rotate simultaneously!
   * this isn't perfect... even when an extra MOUSEMOVE is spoofed, the stroke discards it
   * since the 2D deltas are zero -- code in this file needs to be updated to use the
   * post-NDOF_MOTION MOUSEMOVE */
  if (event->type == NDOF_MOTION) {
    return OPERATOR_PASS_THROUGH;
  }
#endif

  /* one time initialization */
  if (!stroke_init_) {
    if (this->curve_end(C, op)) {
      return OPERATOR_FINISHED;
    }

    stroke_init_ = true;
    first_modal = true;
  }

  /* one time stroke initialization */
  if (!stroke_started_) {
    RNA_boolean_set(op->ptr, "pen_flip", pen_flip_);

    last_pressure_ = sample_average.pressure;
    this->last_mouse_position = sample_average.mouse;
    if (paint_stroke_use_scene_spacing(*br, mode)) {
      BLI_assert(mode != PaintMode::Texture2D);
      stroke_over_mesh_ = this->get_location(
          last_world_space_position_, sample_average.mouse, original_);
      last_world_space_position_ = math::transform_point(this->vc.obact->object_to_world(),
                                                         last_world_space_position_);
    }
    stroke_started_ = this->test_start(op, sample_average.mouse);

    if (stroke_started_) {
      /* StrokeTestStart often updates the currently active brush so we need to re-retrieve it
       * here. */
      br = BKE_paint_brush(paint);

      if (paint_supports_smooth_stroke(*br, mode, brush_switch_mode_)) {

        stroke_cursor_ = WM_paint_cursor_activate(
            SPACE_TYPE_ANY, RGN_TYPE_ANY, paint_brush_cursor_poll, paint_draw_smooth_cursor, this);
      }

      if (br->stroke_method == BRUSH_STROKE_AIRBRUSH) {
        timer_ = WM_event_timer_add(CTX_wm_manager(C), CTX_wm_window(C), TIMER, this->brush->rate);
      }

      if (br->stroke_method == BRUSH_STROKE_LINE) {
        stroke_cursor_ = WM_paint_cursor_activate(
            SPACE_TYPE_ANY, RGN_TYPE_ANY, paint_brush_cursor_poll, paint_draw_line_cursor, this);
      }

      first_dab = true;
    }
  }

  /* Cancel */
  if (event->type == EVT_MODAL_MAP && event->val == PAINT_STROKE_MODAL_CANCEL) {
    if (op->type->cancel) {
      if (this->test_cancel()) {
        op->type->cancel(C, op);
        return OPERATOR_CANCELLED;
      }
    }
    BKE_report(op->reports, RPT_WARNING, "Cancelling this stroke is unsupported");
  }

  /* Handles shift-key active smooth toggling during a grease pencil stroke. */
  if (mode == PaintMode::GPencil) {
    if (event->modifier & KM_SHIFT) {
      brush_switch_mode_ = BrushSwitchMode::Smooth;
      if (!stroke_cursor_) {
        stroke_cursor_ = WM_paint_cursor_activate(
            SPACE_TYPE_ANY, RGN_TYPE_ANY, paint_brush_cursor_poll, paint_draw_smooth_cursor, this);
      }
    }
    else {
      stroke_mode_ = BrushStrokeMode::Normal;
      if (stroke_cursor_ != nullptr) {
        WM_paint_cursor_end(static_cast<wmPaintCursor *>(stroke_cursor_));
        stroke_cursor_ = nullptr;
      }
    }
  }

  float2 mouse;
  if (event->type == event_type_ && !first_modal) {
    if (event->val == KM_RELEASE) {
      mouse = {float(event->mval[0]), float(event->mval[1])};
      if (this->constrain_line) {
        paint_stroke_line_constrain(this->last_mouse_position, this->constrained_pos, mouse);
      }
      this->line_end(C, op, mouse);
      this->stroke_done(C, op, false);
      return OPERATOR_FINISHED;
    }
  }
  else if (ELEM(event->type, EVT_RETKEY, EVT_SPACEKEY)) {
    this->line_end(C, op, sample_average.mouse);
    this->stroke_done(C, op, false);
    return OPERATOR_FINISHED;
  }
  else if (br->stroke_method == BRUSH_STROKE_LINE) {
    if (event->modifier & KM_ALT) {
      this->constrain_line = true;
    }
    else {
      this->constrain_line = false;
    }

    mouse = {float(event->mval[0]), float(event->mval[1])};
    paint_stroke_line_constrain(this->last_mouse_position, this->constrained_pos, mouse);

    if (stroke_started_ && (first_modal || ISMOUSE_MOTION(event->type))) {
      if ((br->mtex.brush_angle_mode & MTEX_ANGLE_RAKE) ||
          (br->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE))
      {
        copy_v2_v2(paint_runtime.last_rake, this->last_mouse_position);
      }
      paint_calculate_rake_rotation(*paint, *br, mouse, mode, true);
    }
  }
  else if (first_modal ||
           /* regular dabs */
           (!(br->stroke_method == BRUSH_STROKE_AIRBRUSH) && ISMOUSE_MOTION(event->type)) ||
           /* airbrush */
           ((br->stroke_method == BRUSH_STROKE_AIRBRUSH) && event->type == TIMER &&
            event->customdata == timer_))
  {
    if (paint_smooth_stroke(*this->brush,
                            &sample_average,
                            mode,
                            brush_switch_mode_,
                            zoom_2d_,
                            this->last_mouse_position,
                            last_pressure_,
                            mouse,
                            pressure))
    {
      if (stroke_started_) {
        if (paint_space_stroke_enabled(*br, mode)) {
          if (this->space_stroke(C, op, mouse, pressure)) {
            needs_redraw = true;
          }
        }
        else {
          const float2 mouse_delta = mouse - this->last_mouse_position;
          stroke_distance_ += math::length(mouse_delta);
          this->add_step(C, op, mouse, pressure);
          needs_redraw = true;
        }
      }
    }
  }

  /* we want the stroke to have the first daub at the start location
   * instead of waiting till we have moved the space distance */
  if (first_dab && paint_space_stroke_enabled(*br, mode) && !(br->flag & BRUSH_SMOOTH_STROKE)) {
    paint_runtime.overlap_factor = paint_stroke_integrate_overlap(*br, 1.0);
    this->add_step(C, op, sample_average.mouse, sample_average.pressure);
    needs_redraw = true;
  }

  /* Don't update the paint cursor in #INBETWEEN_MOUSEMOVE events. */
  if (event->type != INBETWEEN_MOUSEMOVE) {
    wmWindow *window = CTX_wm_window(C);
    ARegion *region = CTX_wm_region(C);

    if (region && (paint->flags & PAINT_SHOW_BRUSH)) {
      WM_paint_cursor_tag_redraw(window, region);
    }
  }

  /* Draw for all events (even in between) otherwise updating the brush
   * display is noticeably delayed.
   */
  if (needs_redraw) {
    this->redraw(false);
  }

  return OPERATOR_RUNNING_MODAL;
}

wmOperatorStatus PaintStroke::exec(bContext *C, wmOperator *op)
{
  /* TODO: Temporary, used to facilitate removing bContext usage in subclasses */
  this->evil_C = C;

  /* only when executed for the first time */
  if (!stroke_started_) {
    PointerRNA firstpoint;
    PropertyRNA *strokeprop = RNA_struct_find_property(op->ptr, "stroke");

    if (RNA_property_collection_lookup_int(op->ptr, strokeprop, 0, &firstpoint)) {
      float2 mouse;
      RNA_float_get_array(&firstpoint, "mouse", mouse);
      stroke_started_ = this->test_start(op, mouse);
    }
  }

  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "override_location");
  const bool override_location = prop && RNA_property_boolean_get(op->ptr, prop) &&
                                 mode != PaintMode::Texture2D;

  if (stroke_started_) {
    RNA_BEGIN (op->ptr, itemptr, "stroke") {
      float2 mval;
      RNA_float_get_array(&itemptr, "mouse_event", mval);

      const float pressure = RNA_float_get(&itemptr, "pressure");
      float2 dummy_mouse;
      RNA_float_get_array(&itemptr, "mouse", dummy_mouse);

      float3 dummy_location;
      bool dummy_is_set;

      this->update(
          C, *this->brush, mode, mval, dummy_mouse, pressure, dummy_location, &dummy_is_set);

      if (override_location) {
        float3 location;
        if (this->get_location(location, mval, false)) {
          RNA_float_set_array(&itemptr, "location", location);
          this->update_step(op, &itemptr);
        }
      }
      else {
        this->update_step(op, &itemptr);
      }
    }
    RNA_END;
  }

  const bool ok = stroke_started_;

  this->stroke_done(C, op, !ok);

  return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void PaintStroke::cancel(bContext *C, wmOperator *op)
{
  this->stroke_done(C, op, true);
}

static const bToolRef *brush_tool_get(const ScrArea *area, const ARegion *region)
{
  if ((area && ELEM(area->spacetype, SPACE_VIEW3D, SPACE_IMAGE)) &&
      (region && region->regiontype == RGN_TYPE_WINDOW))
  {
    if (area->runtime.tool && area->runtime.tool->runtime &&
        (area->runtime.tool->runtime->flag & TOOLREF_FLAG_USE_BRUSHES))
    {
      return area->runtime.tool;
    }
  }
  return nullptr;
}

bool paint_brush_tool_poll(bContext *C)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Object *ob = CTX_data_active_object(C);
  const ScrArea *area = CTX_wm_area(C);
  const ARegion *region = CTX_wm_region(C);
  return paint_brush_tool_poll(area, region, paint, ob);
}

bool paint_brush_tool_poll(const ScrArea *area,
                           const ARegion *region,
                           const Paint *paint,
                           const Object *ob)
{
  if (!paint) {
    return false;
  }

  if (!BKE_paint_brush_for_read(paint)) {
    return false;
  }

  const bToolRef *tref = brush_tool_get(area, region);
  if (!tref) {
    return false;
  }

  if (ob) {
    return true;
  }

  /* Be permissive painting in the Image Editor without an active object. */
  return BKE_paintmode_get_from_tool(tref) == PaintMode::Texture2D;
}

bool paint_brush_cursor_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);

  if (!paint) {
    return false;
  }

  if (!BKE_paint_brush_for_read(paint)) {
    return false;
  }

  const ScrArea *area = CTX_wm_area(C);
  const ARegion *region = CTX_wm_region(C);
  const bToolRef *tref = brush_tool_get(area, region);

  if (!tref) {
    return false;
  }

  /* Don't use brush cursor when the tool sets its own cursor. */
  if (tref->runtime->cursor != WM_CURSOR_DEFAULT) {
    return false;
  }

  if (CTX_data_active_object(C)) {
    return true;
  }

  /* Be permissive painting in the Image Editor without an active object. */
  return BKE_paintmode_get_from_tool(tref) == PaintMode::Texture2D;
}

}  // namespace blender::ed::sculpt_paint

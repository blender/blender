/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "fmt/format.h"

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
#include "sculpt_cloth.hh"
#include "sculpt_intern.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time_utildefines.h"
#endif

namespace blender::ed::sculpt_paint {

struct PaintSample {
  float2 mouse;
  float pressure;
};

/**
 * Common structure for various paint operators (e.g. Sculpt, Grease Pencil, Curves Sculpt)
 *
 * Callback functions defined and stored on this struct (e.g. `StrokeGetLocation`) allow each of
 * these modes to customize specific behavior while still sharing other common handing.
 *
 * See #paint_stroke_modal for the majority of the paint operator logic.
 */
struct PaintStroke {
  std::unique_ptr<PaintModeData> mode_data;
  void *stroke_cursor;
  wmTimer *timer;
  std::optional<RandomNumberGenerator> rng;

  /* Cached values */
  ViewContext vc;
  Paint *paint;
  Brush *brush;
  UnifiedPaintSettings *ups;

  /* Paint stroke can use up to PAINT_MAX_INPUT_SAMPLES prior inputs
   * to smooth the stroke */
  PaintSample samples[PAINT_MAX_INPUT_SAMPLES];
  int num_samples;
  int cur_sample;
  int tot_samples;

  float2 last_mouse_position;
  float3 last_world_space_position;
  float3 last_scene_spacing_delta;

  bool stroke_over_mesh;
  /* space distance covered so far */
  float stroke_distance;

  /* Set whether any stroke step has yet occurred
   * e.g. in sculpt mode, stroke doesn't start until cursor
   * passes over the mesh */
  bool stroke_started;
  /* Set when enough motion was found for rake rotation */
  bool rake_started;
  /* event that started stroke, for modal() return */
  int event_type;
  /* check if stroke variables have been initialized */
  bool stroke_init;
  /* check if input variables have been initialized (e.g. cursor position & pressure)*/
  bool input_init;
  float2 initial_mouse;
  float cached_size_pressure;
  /* last pressure will store last pressure value for use in interpolation for space strokes */
  float last_pressure;
  int stroke_mode;

  float last_tablet_event_pressure;

  float zoom_2d;
  bool pen_flip;

  /* Tilt, as read from the event. */
  float2 tilt;

  /* line constraint */
  bool constrain_line;
  float2 constrained_pos;

  StrokeGetLocation get_location;
  StrokeTestStart test_start;
  StrokeUpdateStep update_step;
  StrokeRedraw redraw;
  StrokeDone done;

  bool original; /* Ray-cast original mesh at start of stroke. */
};

/*** Cursors ***/
static void paint_draw_smooth_cursor(bContext *C,
                                     const blender::int2 &xy,
                                     const blender::float2 & /*tilt*/,
                                     void *customdata)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  PaintStroke *stroke = static_cast<PaintStroke *>(customdata);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);

  if ((mode == PaintMode::GPencil) && (paint->flags & PAINT_SHOW_BRUSH) == 0) {
    return;
  }

  if (stroke && brush) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    const ARegion *region = stroke->vc.region;

    const uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    const uchar4 color = uchar4(255, 100, 100, 128);
    immUniformColor4ubv(color);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos, blender::float2(xy));
    immVertex2f(pos,
                stroke->last_mouse_position[0] + region->winrct.xmin,
                stroke->last_mouse_position[1] + region->winrct.ymin);

    immEnd();

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

static void paint_draw_line_cursor(bContext * /*C*/,
                                   const blender::int2 &xy,
                                   const blender::float2 & /*tilt*/,
                                   void *customdata)
{
  PaintStroke *stroke = static_cast<PaintStroke *>(customdata);

  GPU_line_smooth(true);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

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

static bool image_paint_brush_type_require_location(const Brush &brush, const PaintMode mode)
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

static bool image_paint_brush_type_raycast_original(const Brush &brush, PaintMode /*mode*/)
{
  return brush.flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT);
}

static bool image_paint_brush_type_require_inbetween_mouse_events(const Brush &brush,
                                                                  const PaintMode mode)
{
  if (brush.flag & BRUSH_ANCHORED) {
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

bool paint_brush_update(bContext *C,
                        const Brush &brush,
                        const PaintMode mode,
                        PaintStroke *stroke,
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
  if (!stroke->input_init) {
    copy_v2_v2(stroke->initial_mouse, mouse);
    copy_v2_v2(paint_runtime.last_rake, mouse);
    copy_v2_v2(paint_runtime.tex_mouse, mouse);
    copy_v2_v2(paint_runtime.mask_tex_mouse, mouse);
    stroke->cached_size_pressure = pressure;

    stroke->input_init = true;
  }

  if (paint_supports_dynamic_size(brush, mode)) {
    copy_v2_v2(paint_runtime.tex_mouse, mouse);
    copy_v2_v2(paint_runtime.mask_tex_mouse, mouse);
  }

  /* Truly temporary data that isn't stored in properties */

  paint_runtime.stroke_active = true;
  const float pressure_to_evaluate = paint_supports_dynamic_size(brush, mode) ?
                                         pressure :
                                         stroke->cached_size_pressure;
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

  if (brush.flag & BRUSH_ANCHORED) {
    bool hit = false;
    float2 halfway;

    const float dx = mouse[0] - stroke->initial_mouse[0];
    const float dy = mouse[1] - stroke->initial_mouse[1];

    paint_runtime.anchored_size = paint_runtime.pixel_radius = sqrtf(dx * dx + dy * dy);

    paint_runtime.brush_rotation = paint_runtime.brush_rotation_sec = atan2f(dy, dx) +
                                                                      float(0.5f * M_PI);

    if (brush.flag & BRUSH_EDGE_TO_EDGE) {
      halfway[0] = dx * 0.5f + stroke->initial_mouse[0];
      halfway[1] = dy * 0.5f + stroke->initial_mouse[1];

      if (stroke->get_location) {
        if (stroke->get_location(C, r_location, halfway, stroke->original)) {
          hit = true;
          location_sampled = true;
          location_success = true;
          *r_location_is_set = true;
        }
        else if (!image_paint_brush_type_require_location(brush, mode)) {
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
      stroke->stroke_distance = paint_runtime.pixel_radius;
    }
    else {
      copy_v2_v2(paint_runtime.anchored_initial_mouse, stroke->initial_mouse);
      copy_v2_v2(mouse, stroke->initial_mouse);
      stroke->stroke_distance = paint_runtime.pixel_radius;
    }
    paint_runtime.pixel_radius /= stroke->zoom_2d;
    paint_runtime.draw_anchored = true;
  }
  else {
    /* curve strokes do their own rake calculation */
    if (!(brush.flag & BRUSH_CURVE)) {
      if (!paint_calculate_rake_rotation(*paint, brush, mouse_init, mode, stroke->rake_started)) {
        /* Not enough motion to define an angle. */
        if (!stroke->rake_started) {
          is_dry_run = true;
        }
      }
      else {
        stroke->rake_started = true;
      }
    }
  }

  if ((do_random || do_random_mask) && !stroke->rng) {
    /* Lazy initialization. */
    stroke->rng = RandomNumberGenerator::from_random_seed();
  }

  if (do_random) {
    if (brush.mtex.brush_angle_mode & MTEX_ANGLE_RANDOM) {
      paint_runtime.brush_rotation += -brush.mtex.random_angle / 2.0f +
                                      brush.mtex.random_angle * stroke->rng->get_float();
    }
  }

  if (do_random_mask) {
    if (brush.mask_mtex.brush_angle_mode & MTEX_ANGLE_RANDOM) {
      paint_runtime.brush_rotation_sec += -brush.mask_mtex.random_angle / 2.0f +
                                          brush.mask_mtex.random_angle * stroke->rng->get_float();
    }
  }

  if (!location_sampled) {
    if (stroke->get_location) {
      if (stroke->get_location(C, r_location, mouse, stroke->original)) {
        location_success = true;
        *r_location_is_set = true;
      }
      else if (!image_paint_brush_type_require_location(brush, mode)) {
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
  return brush.flag & BRUSH_SPACE || brush.flag & BRUSH_LINE || brush.flag & BRUSH_CURVE;
}

static bool paint_stroke_use_jitter(const PaintMode mode, const Brush &brush, const bool invert)
{
  bool use_jitter = brush.flag & BRUSH_ABSOLUTE_JITTER ? brush.jitter_absolute != 0 :
                                                         brush.jitter != 0;

  /* jitter-ed brush gives weird and unpredictable result for this
   * kinds of stroke, so manually disable jitter usage (sergey) */
  use_jitter &= (brush.flag & (BRUSH_DRAG_DOT | BRUSH_ANCHORED)) == 0;
  use_jitter &= !ELEM(mode, PaintMode::Texture2D, PaintMode::Texture3D) ||
                !(invert && brush.image_brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE);

  return use_jitter;
}

void paint_stroke_jitter_pos(const PaintStroke &stroke,
                             const PaintMode mode,
                             const Brush &brush,
                             const float pressure,
                             const float mval[2],
                             float r_mouse_out[2])
{
  if (paint_stroke_use_jitter(mode, brush, stroke.stroke_mode == BRUSH_STROKE_INVERT)) {
    float factor = stroke.zoom_2d;

    if (brush.flag & BRUSH_JITTER_PRESSURE) {
      factor *= BKE_curvemapping_evaluateF(brush.curve_jitter, 0, pressure);
    }

    BKE_brush_jitter_pos(*stroke.paint, brush, mval, r_mouse_out);

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
static void paint_brush_stroke_add_step(
    bContext *C, wmOperator *op, PaintStroke *stroke, const float2 mval, float pressure)
{
  const Paint &paint = *BKE_paint_get_active_from_context(C);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const Brush &brush = *BKE_paint_brush_for_read(&paint);
  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;

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
  stroke->last_mouse_position = mval;
  stroke->last_pressure = pressure;

  if (paint_stroke_use_scene_spacing(brush, mode)) {
    float3 world_space_position;

    if (stroke_get_location_bvh(
            C, world_space_position, stroke->last_mouse_position, stroke->original))
    {
      stroke->last_world_space_position = math::transform_point(
          stroke->vc.obact->object_to_world(), world_space_position);
    }
    else {
      stroke->last_world_space_position += stroke->last_scene_spacing_delta;
    }
  }

  float2 mouse_out;
  /* Get jitter position (same as mval if no jitter is used). */
  paint_stroke_jitter_pos(*stroke, mode, brush, pressure, mval, mouse_out);

  float3 location;
  bool is_location_is_set;
  paint_runtime->last_hit = paint_brush_update(
      C, brush, mode, stroke, mval, mouse_out, pressure, location, &is_location_is_set);
  if (is_location_is_set) {
    copy_v3_v3(paint_runtime->last_location, location);
  }
  if (!paint_runtime->last_hit) {
    return;
  }

  /* Dash */
  bool add_step = true;
  if (paint_stroke_use_dash(brush)) {
    const int dash_samples = stroke->tot_samples % brush.dash_samples;
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
    RNA_float_set(&itemptr, "x_tilt", stroke->tilt.x);
    RNA_float_set(&itemptr, "y_tilt", stroke->tilt.y);

    stroke->update_step(C, op, stroke, &itemptr);

    /* don't record this for now, it takes up a lot of memory when doing long
     * strokes with small brush size, and operators have register disabled */
    RNA_collection_clear(op->ptr, "stroke");
  }

  stroke->tot_samples++;
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static bool paint_smooth_stroke(PaintStroke *stroke,
                                const PaintSample *sample,
                                const PaintMode mode,
                                float2 &r_mouse,
                                float &r_pressure)
{
  if (paint_supports_smooth_stroke(stroke, *stroke->brush, mode)) {
    const float radius = stroke->brush->smooth_stroke_radius * stroke->zoom_2d;
    const float u = stroke->brush->smooth_stroke_factor;

    /* If the mouse is moving within the radius of the last move,
     * don't update the mouse position. This allows sharp turns. */
    if (math::distance_squared(stroke->last_mouse_position, sample->mouse) < square_f(radius)) {
      return false;
    }

    r_mouse = math::interpolate(sample->mouse, stroke->last_mouse_position, u);
    r_pressure = math::interpolate(sample->pressure, stroke->last_pressure, u);
  }
  else {
    r_mouse = sample->mouse;
    r_pressure = sample->pressure;
  }

  return true;
}

static float paint_space_stroke_spacing(const bContext *C,
                                        PaintStroke *stroke,
                                        const float size_factor,
                                        const float pressure)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const Brush &brush = *BKE_paint_brush_for_read(paint);

  float size_clamp = 0.0f;
  if (paint_stroke_use_scene_spacing(brush, mode)) {
    const float3 last_object_space_position = math::transform_point(
        stroke->vc.obact->world_to_object(), stroke->last_world_space_position);
    size_clamp = object_space_radius_get(
        stroke->vc, *paint, brush, last_object_space_position, size_factor);
  }
  else {
    /* brushes can have a minimum size of 1.0 but with pressure it can be smaller than a pixel
     * causing very high step sizes, hanging blender #32381. */
    size_clamp = max_ff(1.0f, BKE_brush_radius_get(stroke->paint, stroke->brush) * size_factor);
  }

  float spacing = stroke->brush->spacing;

  /* apply spacing pressure */
  if (stroke->brush->flag & BRUSH_SPACE && stroke->brush->flag & BRUSH_SPACING_PRESSURE) {
    spacing = spacing * (1.5f - pressure);
  }

  if (cloth::is_cloth_deform_brush(brush)) {
    /* The spacing in tools that use the cloth solver should not be affected by the brush radius to
     * avoid affecting the simulation update rate when changing the radius of the brush.
     * With a value of 100 and the brush default of 10 for spacing, a simulation step runs every 2
     * pixels movement of the cursor. */
    size_clamp = 100.0f;
  }

  /* stroke system is used for 2d paint too, so we need to account for
   * the fact that brush can be scaled there. */
  spacing *= stroke->zoom_2d;

  if (paint_stroke_use_scene_spacing(brush, mode)) {
    /* Low pressure on size (with tablets) can cause infinite recursion in paint_space_stroke(),
     * see #129853. */
    return max_ff(FLT_EPSILON, size_clamp * spacing / 50.0f);
  }
  return max_ff(stroke->zoom_2d, size_clamp * spacing / 50.0f);
}

static float paint_space_stroke_spacing_no_pressure(const bContext *C, PaintStroke *stroke)
{
  /* Unlike many paint pressure curves, spacing assumes that a stroke without pressure (e.g. with
   * the mouse, or with the setting turned off) represents an input of 0.5, not 1.0. */
  return paint_space_stroke_spacing(C, stroke, 1.0f, 0.5f);
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

static float paint_space_stroke_spacing_variable(bContext *C,
                                                 PaintStroke *stroke,
                                                 const float pressure,
                                                 const float pressure_delta,
                                                 const float length)
{
  if (BKE_brush_use_size_pressure(stroke->brush)) {
    const float max_size_factor = BKE_curvemapping_evaluateF(stroke->brush->curve_size, 0, 1.0f);
    /* use pressure to modify size. set spacing so that at 100%, the circles
     * are aligned nicely with no overlap. for this the spacing needs to be
     * the average of the previous and next size. */
    const float s = paint_space_stroke_spacing(C, stroke, max_size_factor, pressure);
    const float q = s * pressure_delta / (2.0f * length);
    const float pressure_fac = (1.0f + q) / (1.0f - q);

    const float last_size_factor = BKE_curvemapping_evaluateF(
        stroke->brush->curve_size, 0, stroke->last_pressure);
    const float new_size_factor = BKE_curvemapping_evaluateF(
        stroke->brush->curve_size, 0, stroke->last_pressure * pressure_fac);

    /* average spacing */
    const float last_spacing = paint_space_stroke_spacing(C, stroke, last_size_factor, pressure);
    const float new_spacing = paint_space_stroke_spacing(C, stroke, new_size_factor, pressure);

    return 0.5f * (last_spacing + new_spacing);
  }

  /* no size pressure */
  return paint_space_stroke_spacing(C, stroke, 1.0f, pressure);
}

/* For brushes with stroke spacing enabled, moves mouse in steps
 * towards the final mouse location. */
static int paint_space_stroke(bContext *C,
                              wmOperator *op,
                              PaintStroke *stroke,
                              const float2 final_mouse,
                              const float final_pressure)
{
  const ARegion *region = CTX_wm_region(C);
  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;
  const Paint &paint = *BKE_paint_get_active_from_context(C);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const Brush &brush = *BKE_paint_brush_for_read(&paint);

  float2 mouse_delta = final_mouse - stroke->last_mouse_position;
  float length = normalize_v2(mouse_delta);

  float3 world_space_position_delta;
  const bool use_scene_spacing = paint_stroke_use_scene_spacing(brush, mode);
  if (use_scene_spacing) {
    float3 world_space_position;
    const bool hit = stroke_get_location_bvh(
        C, world_space_position, final_mouse, stroke->original);
    world_space_position = math::transform_point(stroke->vc.obact->object_to_world(),
                                                 world_space_position);
    if (hit && stroke->stroke_over_mesh) {
      world_space_position_delta = world_space_position - stroke->last_world_space_position;
      length = math::length(world_space_position_delta);
      stroke->stroke_over_mesh = true;
    }
    else {
      length = 0.0f;
      world_space_position_delta = {0.0f, 0.0f, 0.0f};
      stroke->stroke_over_mesh = hit;
      if (stroke->stroke_over_mesh) {
        stroke->last_world_space_position = world_space_position;
      }
    }
  }

  float pressure = stroke->last_pressure;
  float pressure_delta = final_pressure - stroke->last_pressure;
  const float no_pressure_spacing = paint_space_stroke_spacing_no_pressure(C, stroke);
  int count = 0;
  while (length > 0.0f) {
    const float spacing = paint_space_stroke_spacing_variable(
        C, stroke, pressure, pressure_delta, length);
    BLI_assert(spacing >= 0.0f);

    if (length >= spacing) {
      float2 mouse;
      if (use_scene_spacing) {
        float3 final_world_space_position;
        world_space_position_delta = math::normalize(world_space_position_delta);
        final_world_space_position = world_space_position_delta * spacing +
                                     stroke->last_world_space_position;
        ED_view3d_project_v2(region, final_world_space_position, mouse);

        stroke->last_scene_spacing_delta = world_space_position_delta * spacing;
      }
      else {
        mouse = stroke->last_mouse_position + mouse_delta * spacing;
      }
      pressure = stroke->last_pressure + (spacing / length) * pressure_delta;

      paint_runtime->overlap_factor = paint_stroke_integrate_overlap(
          *stroke->brush, spacing / no_pressure_spacing);

      stroke->stroke_distance += spacing / stroke->zoom_2d;
      paint_brush_stroke_add_step(C, op, stroke, mouse, pressure);

      length -= spacing;
      pressure = stroke->last_pressure;
      pressure_delta = final_pressure - stroke->last_pressure;

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
  return (G.debug_value == 887);
}

/**** Public API ****/

PaintStroke *paint_stroke_new(bContext *C,
                              wmOperator *op,
                              const StrokeGetLocation get_location,
                              const StrokeTestStart test_start,
                              const StrokeUpdateStep update_step,
                              const StrokeRedraw redraw,
                              const StrokeDone done,
                              const int event_type)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  PaintStroke *stroke = MEM_new<PaintStroke>(__func__);
  Paint *paint = BKE_paint_get_active_from_context(C);
  stroke->paint = paint;
  UnifiedPaintSettings *ups = &paint->unified_paint_settings;
  bke::PaintRuntime *paint_runtime = paint->runtime;
  Brush *br = stroke->brush = BKE_paint_brush(paint);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  stroke->vc = ED_view3d_viewcontext_init(C, depsgraph);

  stroke->get_location = get_location;
  stroke->test_start = test_start;
  stroke->update_step = update_step;
  stroke->redraw = redraw;
  stroke->done = done;
  stroke->event_type = event_type; /* for modal, return event */
  stroke->ups = ups;
  stroke->stroke_mode = RNA_enum_get(op->ptr, "mode");

  stroke->original = image_paint_brush_type_raycast_original(
      *br, BKE_paintmode_get_active_from_context(C));

  float zoomx;
  float zoomy;
  get_imapaint_zoom(C, &zoomx, &zoomy);
  stroke->zoom_2d = std::max(zoomx, zoomy);

  /* Check here if color sampling the main brush should do color conversion. This is done here
   * to avoid locking up to get the image buffer during sampling. */
  paint_runtime->do_linear_conversion = false;
  paint_runtime->colorspace = nullptr;

  if (br->mtex.tex && br->mtex.tex->type == TEX_IMAGE && br->mtex.tex->ima) {
    ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(
        br->mtex.tex->ima, &br->mtex.tex->iuser, nullptr);
    if (tex_ibuf && tex_ibuf->float_buffer.data == nullptr) {
      paint_runtime->do_linear_conversion = true;
      paint_runtime->colorspace = tex_ibuf->byte_buffer.colorspace;
    }
    BKE_image_pool_release_ibuf(br->mtex.tex->ima, tex_ibuf, nullptr);
  }

  if (stroke->stroke_mode == BRUSH_STROKE_INVERT) {
    if (br->flag & BRUSH_CURVE) {
      RNA_enum_set(op->ptr, "mode", BRUSH_STROKE_NORMAL);
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
  BKE_curvemapping_init(br->curve_distance_falloff);
  if (paint->flags & PAINT_USE_CAVITY_MASK) {
    BKE_curvemapping_init(paint->cavity_curve);
  }

  BKE_paint_set_overlay_override(eOverlayFlags(br->overlay_flags));

  paint_runtime->start_pixel_radius = BKE_brush_radius_get(stroke->paint, br);

  return stroke;
}

void paint_stroke_free(bContext *C, wmOperator * /*op*/, PaintStroke *stroke)
{
  if (RegionView3D *rv3d = CTX_wm_region_view3d(C)) {
    rv3d->rflag &= ~RV3D_PAINTING;
  }

  BKE_paint_set_overlay_override(eOverlayFlags(0));

  if (stroke == nullptr) {
    return;
  }

  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;
  paint_runtime->draw_anchored = false;
  paint_runtime->stroke_active = false;

  if (stroke->timer) {
    WM_event_timer_remove(CTX_wm_manager(C), CTX_wm_window(C), stroke->timer);
  }

  if (stroke->stroke_cursor) {
    WM_paint_cursor_end(static_cast<wmPaintCursor *>(stroke->stroke_cursor));
  }

  MEM_delete(stroke);
}

static void stroke_done(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  if (print_pressure_status_enabled()) {
    ED_workspace_status_text(C, nullptr);
  }
  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;

  /* reset rotation here to avoid doing so in cursor display */
  if (!(stroke->brush->mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
    paint_runtime->brush_rotation = 0.0f;
  }

  if (!(stroke->brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
    paint_runtime->brush_rotation_sec = 0.0f;
  }

  if (stroke->stroke_started) {
    if (stroke->redraw) {
      stroke->redraw(C, stroke, true);
    }

    if (stroke->done) {
      stroke->done(C, stroke);
    }
  }

  paint_stroke_free(C, op, stroke);
}

static bool curves_sculpt_brush_uses_spacing(const eBrushCurvesSculptType tool)
{
  return ELEM(tool, CURVES_SCULPT_BRUSH_TYPE_ADD, CURVES_SCULPT_BRUSH_TYPE_DENSITY);
}

bool paint_space_stroke_enabled(const Brush &br, const PaintMode mode)
{
  if ((br.flag & BRUSH_SPACE) == 0) {
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
  if (br.flag & BRUSH_ANCHORED) {
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

bool paint_supports_smooth_stroke(PaintStroke *stroke, const Brush &brush, const PaintMode mode)
{
  /* The grease pencil draw tool needs to enable this when the `stroke_mode` is set to
   * `BRUSH_STROKE_SMOOTH`. */
  if (mode == PaintMode::GPencil &&
      eBrushGPaintType(brush.gpencil_brush_type) == GPAINT_BRUSH_TYPE_DRAW &&
      stroke->stroke_mode == BRUSH_STROKE_SMOOTH)
  {
    return true;
  }
  if (!(brush.flag & BRUSH_SMOOTH_STROKE) ||
      (brush.flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT | BRUSH_LINE)))
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
  if (br.flag & BRUSH_ANCHORED) {
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

static void paint_stroke_add_sample(PaintStroke *stroke,
                                    const int input_samples,
                                    const float x,
                                    const float y,
                                    const float pressure)
{
  PaintSample *sample = &stroke->samples[stroke->cur_sample];
  const int max_samples = std::clamp(input_samples, 1, PAINT_MAX_INPUT_SAMPLES);

  sample->mouse[0] = x;
  sample->mouse[1] = y;
  sample->pressure = pressure;

  stroke->cur_sample++;
  if (stroke->cur_sample >= max_samples) {
    stroke->cur_sample = 0;
  }
  if (stroke->num_samples < max_samples) {
    stroke->num_samples++;
  }
}

static void paint_stroke_sample_average(const PaintStroke *stroke, PaintSample *average)
{
  memset(average, 0, sizeof(*average));

  BLI_assert(stroke->num_samples > 0);

  for (int i = 0; i < stroke->num_samples; i++) {
    average->mouse += stroke->samples[i].mouse;
    average->pressure += stroke->samples[i].pressure;
  }

  average->mouse /= stroke->num_samples;
  average->pressure /= stroke->num_samples;

  // printf("avg=(%f, %f), num=%d\n", average->mouse[0], average->mouse[1], stroke->num_samples);
}

/**
 * Slightly different version of spacing for line/curve strokes,
 * makes sure the dabs stay on the line path.
 */
static void paint_line_strokes_spacing(bContext *C,
                                       wmOperator *op,
                                       PaintStroke *stroke,
                                       const float spacing,
                                       float *length_residue,
                                       const float2 old_pos,
                                       const float2 new_pos)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;
  const Brush &brush = *BKE_paint_brush(paint);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const ARegion *region = CTX_wm_region(C);

  const bool use_scene_spacing = paint_stroke_use_scene_spacing(brush, mode);

  float2 mouse_delta;
  float length;
  float3 world_space_position_delta;
  float3 world_space_position_old;

  stroke->last_mouse_position = old_pos;

  if (use_scene_spacing) {
    const bool hit_old = stroke_get_location_bvh(
        C, world_space_position_old, old_pos, stroke->original);

    float3 world_space_position_new;
    const bool hit_new = stroke_get_location_bvh(
        C, world_space_position_new, new_pos, stroke->original);

    world_space_position_old = math::transform_point(stroke->vc.obact->object_to_world(),
                                                     world_space_position_old);
    world_space_position_new = math::transform_point(stroke->vc.obact->object_to_world(),
                                                     world_space_position_new);
    if (hit_old && hit_new && stroke->stroke_over_mesh) {
      world_space_position_delta = world_space_position_new - world_space_position_old;
      length = math::length(world_space_position_delta);
      stroke->stroke_over_mesh = true;
    }
    else {
      length = 0.0f;
      world_space_position_delta = {0.0f, 0.0f, 0.0f};
      stroke->stroke_over_mesh = hit_new;
      if (stroke->stroke_over_mesh) {
        stroke->last_world_space_position = world_space_position_old;
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
        mouse = stroke->last_mouse_position + mouse_delta * spacing_final;
      }

      paint_runtime->overlap_factor = paint_stroke_integrate_overlap(*stroke->brush, 1.0);

      stroke->stroke_distance += spacing / stroke->zoom_2d;
      paint_brush_stroke_add_step(C, op, stroke, mouse, 1.0);

      length -= spacing;
      spacing_final = spacing;
    }
    else {
      break;
    }
  }

  *length_residue = length;
}

static void paint_stroke_line_end(bContext *C,
                                  wmOperator *op,
                                  PaintStroke *stroke,
                                  const float2 mouse)
{
  Brush *br = stroke->brush;
  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;
  if (stroke->stroke_started && (br->flag & BRUSH_LINE)) {
    paint_runtime->overlap_factor = paint_stroke_integrate_overlap(*br, 1.0);

    paint_brush_stroke_add_step(C, op, stroke, stroke->last_mouse_position, 1.0);
    paint_space_stroke(C, op, stroke, mouse, 1.0);
  }
}

static bool paint_stroke_curve_end(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  const Brush &br = *stroke->brush;
  if (!(br.flag & BRUSH_CURVE)) {
    return false;
  }

  Paint *paint = BKE_paint_get_active_from_context(C);
  bke::PaintRuntime *paint_runtime = stroke->paint->runtime;
  const float no_pressure_spacing = paint_space_stroke_spacing_no_pressure(C, stroke);
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

      if (!stroke->stroke_started) {
        stroke->last_pressure = 1.0;
        copy_v2_v2(stroke->last_mouse_position, data + 2 * j);

        if (paint_stroke_use_scene_spacing(br, BKE_paintmode_get_active_from_context(C))) {
          stroke->stroke_over_mesh = stroke_get_location_bvh(
              C, stroke->last_world_space_position, data + 2 * j, stroke->original);
          mul_m4_v3(stroke->vc.obact->object_to_world().ptr(), stroke->last_world_space_position);
        }

        stroke->stroke_started = stroke->test_start(C, op, stroke->last_mouse_position);

        if (stroke->stroke_started) {
          paint_brush_stroke_add_step(C, op, stroke, data + 2 * j, 1.0);
          paint_line_strokes_spacing(C,
                                     op,
                                     stroke,
                                     no_pressure_spacing,
                                     &length_residue,
                                     data + 2 * j,
                                     data + 2 * (j + 1));
        }
      }
      else {
        paint_line_strokes_spacing(
            C, op, stroke, no_pressure_spacing, &length_residue, data + 2 * j, data + 2 * (j + 1));
      }
    }
  }

  stroke_done(C, op, stroke);

#ifdef DEBUG_TIME
  TIMEIT_END_AVERAGED(whole_stroke);
#endif

  return true;
}

static void paint_stroke_line_constrain(PaintStroke *stroke, float2 &mouse)
{
  if (stroke->constrain_line) {
    float2 line = mouse - stroke->last_mouse_position;
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

    mouse[0] = stroke->constrained_pos[0] = len * cosf(angle) + stroke->last_mouse_position[0];
    mouse[1] = stroke->constrained_pos[1] = len * sinf(angle) + stroke->last_mouse_position[1];
  }
}

wmOperatorStatus paint_stroke_modal(bContext *C,
                                    wmOperator *op,
                                    const wmEvent *event,
                                    PaintStroke **stroke_p)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  bke::PaintRuntime &paint_runtime = *paint->runtime;
  PaintStroke *stroke = *stroke_p;
  const Brush *br = stroke->brush = BKE_paint_brush(paint);
  bool first_dab = false;
  bool first_modal = false;
  bool redraw = false;

  if (event->type == INBETWEEN_MOUSEMOVE &&
      !image_paint_brush_type_require_inbetween_mouse_events(*br, mode))
  {
    return OPERATOR_RUNNING_MODAL;
  }

  /* see if tablet affects event. Line, anchored and drag dot strokes do not support pressure */
  const float tablet_pressure = WM_event_tablet_data(event, &stroke->pen_flip, nullptr);
  float pressure = ((br->flag & (BRUSH_LINE | BRUSH_ANCHORED | BRUSH_DRAG_DOT)) ? 1.0f :
                                                                                  tablet_pressure);

  if (print_pressure_status_enabled() && WM_event_is_tablet(event)) {
    std::string msg = fmt::format("Tablet Pressure: {:.4f}", pressure);
    ED_workspace_status_text(C, msg.c_str());
  }

  /* When processing a timer event the pressure from the event is 0, so use the last valid
   * pressure. */
  if (event->type == TIMER) {
    pressure = stroke->last_tablet_event_pressure;
  }
  else {
    stroke->last_tablet_event_pressure = pressure;
  }

  const int input_samples = BKE_brush_input_samples_get(stroke->paint, br);
  paint_stroke_add_sample(stroke, input_samples, event->mval[0], event->mval[1], pressure);

  PaintSample sample_average;
  paint_stroke_sample_average(stroke, &sample_average);

  /* Tilt. */
  if (WM_event_is_tablet(event)) {
    stroke->tilt = event->tablet.tilt;
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
  if (!stroke->stroke_init) {
    if (paint_stroke_curve_end(C, op, stroke)) {
      *stroke_p = nullptr;
      return OPERATOR_FINISHED;
    }

    stroke->stroke_init = true;
    first_modal = true;
  }

  /* one time stroke initialization */
  if (!stroke->stroke_started) {
    RNA_boolean_set(op->ptr, "pen_flip", stroke->pen_flip);

    stroke->last_pressure = sample_average.pressure;
    stroke->last_mouse_position = sample_average.mouse;
    if (paint_stroke_use_scene_spacing(*br, mode)) {
      stroke->stroke_over_mesh = stroke_get_location_bvh(
          C, stroke->last_world_space_position, sample_average.mouse, stroke->original);
      stroke->last_world_space_position = math::transform_point(
          stroke->vc.obact->object_to_world(), stroke->last_world_space_position);
    }
    stroke->stroke_started = stroke->test_start(C, op, sample_average.mouse);

    if (stroke->stroke_started) {
      /* StrokeTestStart often updates the currently active brush so we need to re-retrieve it
       * here. */
      br = BKE_paint_brush(paint);

      if (paint_supports_smooth_stroke(stroke, *br, mode)) {
        stroke->stroke_cursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                                         RGN_TYPE_ANY,
                                                         paint_brush_cursor_poll,
                                                         paint_draw_smooth_cursor,
                                                         stroke);
      }

      if (br->flag & BRUSH_AIRBRUSH) {
        stroke->timer = WM_event_timer_add(
            CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);
      }

      if (br->flag & BRUSH_LINE) {
        stroke->stroke_cursor = WM_paint_cursor_activate(
            SPACE_TYPE_ANY, RGN_TYPE_ANY, paint_brush_cursor_poll, paint_draw_line_cursor, stroke);
      }

      BKE_curvemapping_init(br->curve_size);
      BKE_curvemapping_init(br->curve_strength);
      BKE_curvemapping_init(br->curve_jitter);

      first_dab = true;
    }
  }

  /* Cancel */
  if (event->type == EVT_MODAL_MAP && event->val == PAINT_STROKE_MODAL_CANCEL) {
    if (op->type->cancel) {
      op->type->cancel(C, op);
    }
    else {
      paint_stroke_cancel(C, op, stroke);
    }
    return OPERATOR_CANCELLED;
  }

  /* Handles shift-key active smooth toggling during a grease pencil stroke. */
  if (mode == PaintMode::GPencil) {
    if (event->modifier & KM_SHIFT) {
      stroke->stroke_mode = BRUSH_STROKE_SMOOTH;
      if (!stroke->stroke_cursor) {
        stroke->stroke_cursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                                         RGN_TYPE_ANY,
                                                         paint_brush_cursor_poll,
                                                         paint_draw_smooth_cursor,
                                                         stroke);
      }
    }
    else {
      stroke->stroke_mode = BRUSH_STROKE_NORMAL;
      if (stroke->stroke_cursor != nullptr) {
        WM_paint_cursor_end(static_cast<wmPaintCursor *>(stroke->stroke_cursor));
        stroke->stroke_cursor = nullptr;
      }
    }
  }

  float2 mouse;
  if (event->type == stroke->event_type && !first_modal) {
    if (event->val == KM_RELEASE) {
      mouse = {float(event->mval[0]), float(event->mval[1])};
      paint_stroke_line_constrain(stroke, mouse);
      paint_stroke_line_end(C, op, stroke, mouse);
      stroke_done(C, op, stroke);
      *stroke_p = nullptr;
      return OPERATOR_FINISHED;
    }
  }
  else if (ELEM(event->type, EVT_RETKEY, EVT_SPACEKEY)) {
    paint_stroke_line_end(C, op, stroke, sample_average.mouse);
    stroke_done(C, op, stroke);
    *stroke_p = nullptr;
    return OPERATOR_FINISHED;
  }
  else if (br->flag & BRUSH_LINE) {
    if (event->modifier & KM_ALT) {
      stroke->constrain_line = true;
    }
    else {
      stroke->constrain_line = false;
    }

    mouse = {float(event->mval[0]), float(event->mval[1])};
    paint_stroke_line_constrain(stroke, mouse);

    if (stroke->stroke_started && (first_modal || ISMOUSE_MOTION(event->type))) {
      if ((br->mtex.brush_angle_mode & MTEX_ANGLE_RAKE) ||
          (br->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE))
      {
        copy_v2_v2(paint_runtime.last_rake, stroke->last_mouse_position);
      }
      paint_calculate_rake_rotation(*stroke->paint, *br, mouse, mode, true);
    }
  }
  else if (first_modal ||
           /* regular dabs */
           (!(br->flag & BRUSH_AIRBRUSH) && ISMOUSE_MOTION(event->type)) ||
           /* airbrush */
           ((br->flag & BRUSH_AIRBRUSH) && event->type == TIMER &&
            event->customdata == stroke->timer))
  {
    if (paint_smooth_stroke(stroke, &sample_average, mode, mouse, pressure)) {
      if (stroke->stroke_started) {
        if (paint_space_stroke_enabled(*br, mode)) {
          if (paint_space_stroke(C, op, stroke, mouse, pressure)) {
            redraw = true;
          }
        }
        else {
          const float2 mouse_delta = mouse - stroke->last_mouse_position;
          stroke->stroke_distance += math::length(mouse_delta);
          paint_brush_stroke_add_step(C, op, stroke, mouse, pressure);
          redraw = true;
        }
      }
    }
  }

  /* we want the stroke to have the first daub at the start location
   * instead of waiting till we have moved the space distance */
  if (first_dab && paint_space_stroke_enabled(*br, mode) && !(br->flag & BRUSH_SMOOTH_STROKE)) {
    paint_runtime.overlap_factor = paint_stroke_integrate_overlap(*br, 1.0);
    paint_brush_stroke_add_step(C, op, stroke, sample_average.mouse, sample_average.pressure);
    redraw = true;
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
  if (redraw && stroke->redraw) {
    stroke->redraw(C, stroke, false);
  }

  return OPERATOR_RUNNING_MODAL;
}

wmOperatorStatus paint_stroke_exec(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  /* only when executed for the first time */
  if (!stroke->stroke_started) {
    PointerRNA firstpoint;
    PropertyRNA *strokeprop = RNA_struct_find_property(op->ptr, "stroke");

    if (RNA_property_collection_lookup_int(op->ptr, strokeprop, 0, &firstpoint)) {
      float2 mouse;
      RNA_float_get_array(&firstpoint, "mouse", mouse);
      stroke->stroke_started = stroke->test_start(C, op, mouse);
    }
  }

  const PaintMode mode = BKE_paintmode_get_active_from_context(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "override_location");
  const bool override_location = prop && RNA_property_boolean_get(op->ptr, prop) &&
                                 stroke->get_location;
  if (stroke->stroke_started) {
    RNA_BEGIN (op->ptr, itemptr, "stroke") {
      float2 mval;
      RNA_float_get_array(&itemptr, "mouse_event", mval);

      const float pressure = RNA_float_get(&itemptr, "pressure");
      float2 dummy_mouse;
      RNA_float_get_array(&itemptr, "mouse", dummy_mouse);

      float3 dummy_location;
      bool dummy_is_set;

      paint_brush_update(C,
                         *stroke->brush,
                         mode,
                         stroke,
                         mval,
                         dummy_mouse,
                         pressure,
                         dummy_location,
                         &dummy_is_set);

      if (override_location) {
        float3 location;
        if (stroke->get_location(C, location, mval, false)) {
          RNA_float_set_array(&itemptr, "location", location);
          stroke->update_step(C, op, stroke, &itemptr);
        }
      }
      else {
        stroke->update_step(C, op, stroke, &itemptr);
      }
    }
    RNA_END;
  }

  const bool ok = stroke->stroke_started;

  stroke_done(C, op, stroke);

  return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void paint_stroke_cancel(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  stroke_done(C, op, stroke);
}

ViewContext *paint_stroke_view_context(PaintStroke *stroke)
{
  return &stroke->vc;
}

void *paint_stroke_mode_data(PaintStroke *stroke)
{
  return stroke->mode_data.get();
}

bool paint_stroke_flipped(PaintStroke *stroke)
{
  return stroke->pen_flip;
}

bool paint_stroke_inverted(PaintStroke *stroke)
{
  return stroke->stroke_mode == BRUSH_STROKE_INVERT;
}

float paint_stroke_distance_get(PaintStroke *stroke)
{
  return stroke->stroke_distance;
}

void paint_stroke_set_mode_data(PaintStroke *stroke, std::unique_ptr<PaintModeData> mode_data)
{
  stroke->mode_data = std::move(mode_data);
}

bool paint_stroke_started(PaintStroke *stroke)
{
  return stroke->stroke_started;
}

static const bToolRef *brush_tool_get(const bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  const Object *ob = CTX_data_active_object(C);
  const ScrArea *area = CTX_wm_area(C);
  const ARegion *region = CTX_wm_region(C);

  if (paint && ob && BKE_paint_brush(paint) &&
      (area && ELEM(area->spacetype, SPACE_VIEW3D, SPACE_IMAGE)) &&
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
  /* Check the current tool is a brush. */
  return brush_tool_get(C) != nullptr;
}

bool paint_brush_cursor_poll(bContext *C)
{
  const bToolRef *tref = brush_tool_get(C);
  if (!tref) {
    return false;
  }

  /* Don't use brush cursor when the tool sets its own cursor. */
  if (tref->runtime->cursor != WM_CURSOR_DEFAULT) {
    return false;
  }

  return true;
}

}  // namespace blender::ed::sculpt_paint

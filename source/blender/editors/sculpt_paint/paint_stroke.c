/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 by Nicholas Bishop. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_image.h"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "IMB_imbuf_types.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include <float.h>
#include <math.h>

//#define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

typedef struct PaintSample {
  float mouse[2];
  float pressure;
} PaintSample;

typedef struct PaintStroke {
  void *mode_data;
  void *stroke_cursor;
  wmTimer *timer;
  struct RNG *rng;

  /* Cached values */
  ViewContext vc;
  Brush *brush;
  UnifiedPaintSettings *ups;

  /* used for lines and curves */
  ListBase line;

  /* Paint stroke can use up to PAINT_MAX_INPUT_SAMPLES prior inputs
   * to smooth the stroke */
  PaintSample samples[PAINT_MAX_INPUT_SAMPLES];
  int num_samples;
  int cur_sample;
  int tot_samples;

  float last_mouse_position[2];
  float last_world_space_position[3];
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
  /* check if various brush mapping variables have been initialized */
  bool brush_init;
  float initial_mouse[2];
  /* cached_pressure stores initial pressure for size pressure influence mainly */
  float cached_size_pressure;
  /* last pressure will store last pressure value for use in interpolation for space strokes */
  float last_pressure;
  int stroke_mode;

  float last_tablet_event_pressure;

  float zoom_2d;
  int pen_flip;

  /* Tilt, as read from the event. */
  float x_tilt;
  float y_tilt;

  /* line constraint */
  bool constrain_line;
  float constrained_pos[2];

  StrokeGetLocation get_location;
  StrokeTestStart test_start;
  StrokeUpdateStep update_step;
  StrokeRedraw redraw;
  StrokeDone done;
} PaintStroke;

/*** Cursors ***/
static void paint_draw_smooth_cursor(bContext *C, int x, int y, void *customdata)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  PaintStroke *stroke = customdata;

  if (stroke && brush) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    ARegion *region = stroke->vc.region;

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor4ubv(paint->paint_cursor_col);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, x, y);
    immVertex2f(pos,
                stroke->last_mouse_position[0] + region->winrct.xmin,
                stroke->last_mouse_position[1] + region->winrct.ymin);

    immEnd();

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

static void paint_draw_line_cursor(bContext *C, int x, int y, void *customdata)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  PaintStroke *stroke = customdata;

  GPU_line_smooth(true);

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  const float alpha = (float)paint->paint_cursor_col[3] / 255.0f;
  immUniform4f("color", 0.0f, 0.0f, 0.0f, alpha);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, alpha);
  immUniform1f("dash_width", 6.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 2);

  ARegion *region = stroke->vc.region;

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

    immVertex2f(shdr_pos, x, y);
  }

  immEnd();

  immUnbindProgram();

  GPU_line_smooth(false);
}

static bool paint_tool_require_location(Brush *brush, ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_SCULPT:
      if (ELEM(brush->sculpt_tool,
               SCULPT_TOOL_GRAB,
               SCULPT_TOOL_ELASTIC_DEFORM,
               SCULPT_TOOL_POSE,
               SCULPT_TOOL_BOUNDARY,
               SCULPT_TOOL_ROTATE,
               SCULPT_TOOL_SNAKE_HOOK,
               SCULPT_TOOL_THUMB)) {
        return false;
      }
      else if (SCULPT_is_cloth_deform_brush(brush)) {
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

static bool paint_stroke_use_scene_spacing(Brush *brush, ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_SCULPT:
      return brush->flag & BRUSH_SCENE_SPACING;
    default:
      break;
  }
  return false;
}

static bool paint_tool_require_inbetween_mouse_events(Brush *brush, ePaintMode mode)
{
  if (brush->flag & BRUSH_ANCHORED) {
    return false;
  }

  switch (mode) {
    case PAINT_MODE_SCULPT:
      if (ELEM(brush->sculpt_tool,
               SCULPT_TOOL_GRAB,
               SCULPT_TOOL_ROTATE,
               SCULPT_TOOL_THUMB,
               SCULPT_TOOL_SNAKE_HOOK,
               SCULPT_TOOL_ELASTIC_DEFORM,
               SCULPT_TOOL_CLOTH,
               SCULPT_TOOL_BOUNDARY,
               SCULPT_TOOL_POSE)) {
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

/* Initialize the stroke cache variants from operator properties */
static bool paint_brush_update(bContext *C,
                               Brush *brush,
                               ePaintMode mode,
                               struct PaintStroke *stroke,
                               const float mouse_init[2],
                               float mouse[2],
                               float pressure,
                               float r_location[3],
                               bool *r_location_is_set)
{
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = stroke->ups;
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
  if (!stroke->brush_init) {
    copy_v2_v2(stroke->initial_mouse, mouse);
    copy_v2_v2(ups->last_rake, mouse);
    copy_v2_v2(ups->tex_mouse, mouse);
    copy_v2_v2(ups->mask_tex_mouse, mouse);
    stroke->cached_size_pressure = pressure;

    ups->do_linear_conversion = false;
    ups->colorspace = NULL;

    /* check here if color sampling the main brush should do color conversion. This is done here
     * to avoid locking up to get the image buffer during sampling */
    if (brush->mtex.tex && brush->mtex.tex->type == TEX_IMAGE && brush->mtex.tex->ima) {
      ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(
          brush->mtex.tex->ima, &brush->mtex.tex->iuser, NULL);
      if (tex_ibuf && tex_ibuf->rect_float == NULL) {
        ups->do_linear_conversion = true;
        ups->colorspace = tex_ibuf->rect_colorspace;
      }
      BKE_image_pool_release_ibuf(brush->mtex.tex->ima, tex_ibuf, NULL);
    }

    stroke->brush_init = true;
  }

  if (paint_supports_dynamic_size(brush, mode)) {
    copy_v2_v2(ups->tex_mouse, mouse);
    copy_v2_v2(ups->mask_tex_mouse, mouse);
    stroke->cached_size_pressure = pressure;
  }

  /* Truly temporary data that isn't stored in properties */

  ups->stroke_active = true;
  ups->size_pressure_value = stroke->cached_size_pressure;

  ups->pixel_radius = BKE_brush_size_get(scene, brush);
  ups->initial_pixel_radius = BKE_brush_size_get(scene, brush);

  if (BKE_brush_use_size_pressure(brush) && paint_supports_dynamic_size(brush, mode)) {
    ups->pixel_radius *= stroke->cached_size_pressure;
  }

  if (paint_supports_dynamic_tex_coords(brush, mode)) {

    if (ELEM(brush->mtex.brush_map_mode,
             MTEX_MAP_MODE_VIEW,
             MTEX_MAP_MODE_AREA,
             MTEX_MAP_MODE_RANDOM)) {
      do_random = true;
    }

    if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      BKE_brush_randomize_texture_coords(ups, false);
    }
    else {
      copy_v2_v2(ups->tex_mouse, mouse);
    }

    /* take care of mask texture, if any */
    if (brush->mask_mtex.tex) {

      if (ELEM(brush->mask_mtex.brush_map_mode,
               MTEX_MAP_MODE_VIEW,
               MTEX_MAP_MODE_AREA,
               MTEX_MAP_MODE_RANDOM)) {
        do_random_mask = true;
      }

      if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
        BKE_brush_randomize_texture_coords(ups, true);
      }
      else {
        copy_v2_v2(ups->mask_tex_mouse, mouse);
      }
    }
  }

  if (brush->flag & BRUSH_ANCHORED) {
    bool hit = false;
    float halfway[2];

    const float dx = mouse[0] - stroke->initial_mouse[0];
    const float dy = mouse[1] - stroke->initial_mouse[1];

    ups->anchored_size = ups->pixel_radius = sqrtf(dx * dx + dy * dy);

    ups->brush_rotation = ups->brush_rotation_sec = atan2f(dx, dy) + (float)M_PI;

    if (brush->flag & BRUSH_EDGE_TO_EDGE) {
      halfway[0] = dx * 0.5f + stroke->initial_mouse[0];
      halfway[1] = dy * 0.5f + stroke->initial_mouse[1];

      if (stroke->get_location) {
        if (stroke->get_location(C, r_location, halfway)) {
          hit = true;
          location_sampled = true;
          location_success = true;
          *r_location_is_set = true;
        }
        else if (!paint_tool_require_location(brush, mode)) {
          hit = true;
        }
      }
      else {
        hit = true;
      }
    }
    if (hit) {
      copy_v2_v2(ups->anchored_initial_mouse, halfway);
      copy_v2_v2(ups->tex_mouse, halfway);
      copy_v2_v2(ups->mask_tex_mouse, halfway);
      copy_v2_v2(mouse, halfway);
      ups->anchored_size /= 2.0f;
      ups->pixel_radius /= 2.0f;
      stroke->stroke_distance = ups->pixel_radius;
    }
    else {
      copy_v2_v2(ups->anchored_initial_mouse, stroke->initial_mouse);
      copy_v2_v2(mouse, stroke->initial_mouse);
      stroke->stroke_distance = ups->pixel_radius;
    }
    ups->pixel_radius /= stroke->zoom_2d;
    ups->draw_anchored = true;
  }
  else {
    /* here we are using the initial mouse coordinate because we do not want the rake
     * result to depend on jittering */
    if (!stroke->brush_init) {
      copy_v2_v2(ups->last_rake, mouse_init);
    }
    /* curve strokes do their own rake calculation */
    else if (!(brush->flag & BRUSH_CURVE)) {
      if (!paint_calculate_rake_rotation(ups, brush, mouse_init)) {
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

  if ((do_random || do_random_mask) && stroke->rng == NULL) {
    /* Lazy initialization. */
    uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
    rng_seed ^= (uint)POINTER_AS_INT(brush);
    stroke->rng = BLI_rng_new(rng_seed);
  }

  if (do_random) {
    if (brush->mtex.brush_angle_mode & MTEX_ANGLE_RANDOM) {
      ups->brush_rotation += -brush->mtex.random_angle / 2.0f +
                             brush->mtex.random_angle * BLI_rng_get_float(stroke->rng);
    }
  }

  if (do_random_mask) {
    if (brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RANDOM) {
      ups->brush_rotation_sec += -brush->mask_mtex.random_angle / 2.0f +
                                 brush->mask_mtex.random_angle * BLI_rng_get_float(stroke->rng);
    }
  }

  if (!location_sampled) {
    if (stroke->get_location) {
      if (stroke->get_location(C, r_location, mouse)) {
        location_success = true;
        *r_location_is_set = true;
      }
      else if (!paint_tool_require_location(brush, mode)) {
        location_success = true;
      }
    }
    else {
      zero_v3(r_location);
      location_success = true;
      /* don't set 'r_location_is_set', since we don't want to use the value. */
    }
  }

  return location_success && (is_dry_run == false);
}

static bool paint_stroke_use_dash(Brush *brush)
{
  /* Only these stroke modes support dash lines */
  return brush->flag & BRUSH_SPACE || brush->flag & BRUSH_LINE || brush->flag & BRUSH_CURVE;
}

static bool paint_stroke_use_jitter(ePaintMode mode, Brush *brush, bool invert)
{
  bool use_jitter = (brush->flag & BRUSH_ABSOLUTE_JITTER) ? (brush->jitter_absolute != 0) :
                                                            (brush->jitter != 0);

  /* jitter-ed brush gives weird and unpredictable result for this
   * kinds of stroke, so manually disable jitter usage (sergey) */
  use_jitter &= (brush->flag & (BRUSH_DRAG_DOT | BRUSH_ANCHORED)) == 0;
  use_jitter &= (!ELEM(mode, PAINT_MODE_TEXTURE_2D, PAINT_MODE_TEXTURE_3D) ||
                 !(invert && brush->imagepaint_tool == PAINT_TOOL_CLONE));

  return use_jitter;
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(
    bContext *C, wmOperator *op, PaintStroke *stroke, const float mval[2], float pressure)
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  UnifiedPaintSettings *ups = stroke->ups;
  float mouse_out[2];
  PointerRNA itemptr;
  float location[3];

/* the following code is adapted from texture paint. It may not be needed but leaving here
 * just in case for reference (code in texpaint removed as part of refactoring).
 * It's strange that only texpaint had these guards. */
#if 0
  /* special exception here for too high pressure values on first touch in
   * windows for some tablets, then we just skip first touch. */
  if (tablet && (pressure >= 0.99f) &&
      ((pop->s.brush->flag & BRUSH_SPACING_PRESSURE) ||
       BKE_brush_use_alpha_pressure(pop->s.brush) ||
       BKE_brush_use_size_pressure(pop->s.brush))) {
    return;
  }

  /* This can be removed once fixed properly in
   * BKE_brush_painter_paint(
   *     BrushPainter *painter, BrushFunc func,
   *     float *pos, double time, float pressure, void *user);
   * at zero pressure we should do nothing 1/2^12 is 0.0002
   * which is the sensitivity of the most sensitive pen tablet available */
  if (tablet && (pressure < 0.0002f) &&
      ((pop->s.brush->flag & BRUSH_SPACING_PRESSURE) ||
       BKE_brush_use_alpha_pressure(pop->s.brush) ||
       BKE_brush_use_size_pressure(pop->s.brush))) {
    return;
  }
#endif

  /* copy last position -before- jittering, or space fill code
   * will create too many dabs */
  copy_v2_v2(stroke->last_mouse_position, mval);
  stroke->last_pressure = pressure;

  if (paint_stroke_use_scene_spacing(brush, mode)) {
    SCULPT_stroke_get_location(C, stroke->last_world_space_position, stroke->last_mouse_position);
    mul_m4_v3(stroke->vc.obact->obmat, stroke->last_world_space_position);
  }

  if (paint_stroke_use_jitter(mode, brush, stroke->stroke_mode == BRUSH_STROKE_INVERT)) {
    float delta[2];
    float factor = stroke->zoom_2d;

    if (brush->flag & BRUSH_JITTER_PRESSURE) {
      factor *= pressure;
    }

    BKE_brush_jitter_pos(scene, brush, mval, mouse_out);

    /* XXX: meh, this is round about because
     * BKE_brush_jitter_pos isn't written in the best way to
     * be reused here */
    if (factor != 1.0f) {
      sub_v2_v2v2(delta, mouse_out, mval);
      mul_v2_fl(delta, factor);
      add_v2_v2v2(mouse_out, mval, delta);
    }
  }
  else {
    copy_v2_v2(mouse_out, mval);
  }

  bool is_location_is_set;
  ups->last_hit = paint_brush_update(
      C, brush, mode, stroke, mval, mouse_out, pressure, location, &is_location_is_set);
  if (is_location_is_set) {
    copy_v3_v3(ups->last_location, location);
  }
  if (!ups->last_hit) {
    return;
  }

  /* Dash */
  bool add_step = true;
  if (paint_stroke_use_dash(brush)) {
    int dash_samples = stroke->tot_samples % brush->dash_samples;
    float dash = (float)dash_samples / (float)brush->dash_samples;
    if (dash > brush->dash_ratio) {
      add_step = false;
    }
  }

  /* Add to stroke */
  if (add_step) {
    RNA_collection_add(op->ptr, "stroke", &itemptr);
    RNA_float_set(&itemptr, "size", ups->pixel_radius);
    RNA_float_set_array(&itemptr, "location", location);
    /* Mouse coordinates modified by the stroke type options. */
    RNA_float_set_array(&itemptr, "mouse", mouse_out);
    /* Original mouse coordinates. */
    RNA_float_set_array(&itemptr, "mouse_event", mval);
    RNA_boolean_set(&itemptr, "pen_flip", stroke->pen_flip);
    RNA_float_set(&itemptr, "pressure", pressure);
    RNA_float_set(&itemptr, "x_tilt", stroke->x_tilt);
    RNA_float_set(&itemptr, "y_tilt", stroke->y_tilt);

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
                                ePaintMode mode,
                                float r_mouse[2],
                                float *r_pressure)
{
  if (paint_supports_smooth_stroke(stroke->brush, mode)) {
    float radius = stroke->brush->smooth_stroke_radius * stroke->zoom_2d;
    float u = stroke->brush->smooth_stroke_factor;

    /* If the mouse is moving within the radius of the last move,
     * don't update the mouse position. This allows sharp turns. */
    if (len_squared_v2v2(stroke->last_mouse_position, sample->mouse) < square_f(radius)) {
      return false;
    }

    interp_v2_v2v2(r_mouse, sample->mouse, stroke->last_mouse_position, u);
    *r_pressure = interpf(sample->pressure, stroke->last_pressure, u);
  }
  else {
    r_mouse[0] = sample->mouse[0];
    r_mouse[1] = sample->mouse[1];
    *r_pressure = sample->pressure;
  }

  return true;
}

static float paint_space_stroke_spacing(bContext *C,
                                        const Scene *scene,
                                        PaintStroke *stroke,
                                        float size_pressure,
                                        float spacing_pressure)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  float size_clamp = 0.0f;
  float size = BKE_brush_size_get(scene, stroke->brush) * size_pressure;
  if (paint_stroke_use_scene_spacing(brush, mode)) {
    if (!BKE_brush_use_locked_size(scene, brush)) {
      float last_object_space_position[3];
      mul_v3_m4v3(
          last_object_space_position, stroke->vc.obact->imat, stroke->last_world_space_position);
      size_clamp = paint_calc_object_space_radius(&stroke->vc, last_object_space_position, size);
    }
    else {
      size_clamp = BKE_brush_unprojected_radius_get(scene, brush) * size_pressure;
    }
  }
  else {
    /* brushes can have a minimum size of 1.0 but with pressure it can be smaller than a pixel
     * causing very high step sizes, hanging blender T32381. */
    size_clamp = max_ff(1.0f, size);
  }

  float spacing = stroke->brush->spacing;

  /* apply spacing pressure */
  if (stroke->brush->flag & BRUSH_SPACING_PRESSURE) {
    spacing = spacing * (1.5f - spacing_pressure);
  }

  if (SCULPT_is_cloth_deform_brush(brush)) {
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
    return max_ff(0.001f, size_clamp * spacing / 50.0f);
  }
  return max_ff(stroke->zoom_2d, size_clamp * spacing / 50.0f);
}

static float paint_stroke_overlapped_curve(Brush *br, float x, float spacing)
{
  const int n = 100 / spacing;
  const float h = spacing / 50.0f;
  const float x0 = x - 1;

  float sum = 0;
  for (int i = 0; i < n; i++) {
    float xx;

    xx = fabsf(x0 + i * h);

    if (xx < 1.0f) {
      sum += BKE_brush_curve_strength(br, xx, 1);
    }
  }

  return sum;
}

static float paint_stroke_integrate_overlap(Brush *br, float factor)
{
  float spacing = br->spacing * factor;

  if (!(br->flag & BRUSH_SPACE_ATTEN && (br->spacing < 100))) {
    return 1.0;
  }

  int m = 10;
  float g = 1.0f / m;
  float max = 0;
  for (int i = 0; i < m; i++) {
    float overlap = fabs(paint_stroke_overlapped_curve(br, i * g, spacing));

    if (overlap > max) {
      max = overlap;
    }
  }

  if (max == 0.0f) {
    return 1.0f;
  }
  return 1.0f / max;
}

static float paint_space_stroke_spacing_variable(bContext *C,
                                                 const Scene *scene,
                                                 PaintStroke *stroke,
                                                 float pressure,
                                                 float dpressure,
                                                 float length)
{
  if (BKE_brush_use_size_pressure(stroke->brush)) {
    /* use pressure to modify size. set spacing so that at 100%, the circles
     * are aligned nicely with no overlap. for this the spacing needs to be
     * the average of the previous and next size. */
    float s = paint_space_stroke_spacing(C, scene, stroke, 1.0f, pressure);
    float q = s * dpressure / (2.0f * length);
    float pressure_fac = (1.0f + q) / (1.0f - q);

    float last_size_pressure = stroke->last_pressure;
    float new_size_pressure = stroke->last_pressure * pressure_fac;

    /* average spacing */
    float last_spacing = paint_space_stroke_spacing(
        C, scene, stroke, last_size_pressure, pressure);
    float new_spacing = paint_space_stroke_spacing(C, scene, stroke, new_size_pressure, pressure);

    return 0.5f * (last_spacing + new_spacing);
  }

  /* no size pressure */
  return paint_space_stroke_spacing(C, scene, stroke, 1.0f, pressure);
}

/* For brushes with stroke spacing enabled, moves mouse in steps
 * towards the final mouse location. */
static int paint_space_stroke(bContext *C,
                              wmOperator *op,
                              PaintStroke *stroke,
                              const float final_mouse[2],
                              float final_pressure)
{
  const Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  UnifiedPaintSettings *ups = stroke->ups;
  Paint *paint = BKE_paint_get_active_from_context(C);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  int count = 0;

  const bool use_scene_spacing = paint_stroke_use_scene_spacing(brush, mode);
  float d_world_space_position[3] = {0.0f};

  float no_pressure_spacing = paint_space_stroke_spacing(C, scene, stroke, 1.0f, 1.0f);
  float pressure = stroke->last_pressure;
  float dpressure = final_pressure - stroke->last_pressure;

  float dmouse[2];
  sub_v2_v2v2(dmouse, final_mouse, stroke->last_mouse_position);
  float length = normalize_v2(dmouse);

  if (use_scene_spacing) {
    float world_space_position[3];
    bool hit = SCULPT_stroke_get_location(C, world_space_position, final_mouse);
    mul_m4_v3(stroke->vc.obact->obmat, world_space_position);
    if (hit && stroke->stroke_over_mesh) {
      sub_v3_v3v3(d_world_space_position, world_space_position, stroke->last_world_space_position);
      length = len_v3(d_world_space_position);
      stroke->stroke_over_mesh = true;
    }
    else {
      length = 0.0f;
      zero_v3(d_world_space_position);
      stroke->stroke_over_mesh = hit;
      if (stroke->stroke_over_mesh) {
        copy_v3_v3(stroke->last_world_space_position, world_space_position);
      }
    }
  }

  while (length > 0.0f) {
    float spacing = paint_space_stroke_spacing_variable(
        C, scene, stroke, pressure, dpressure, length);
    float mouse[2];

    if (length >= spacing) {
      if (use_scene_spacing) {
        float final_world_space_position[3];
        normalize_v3(d_world_space_position);
        mul_v3_v3fl(final_world_space_position, d_world_space_position, spacing);
        add_v3_v3v3(final_world_space_position,
                    stroke->last_world_space_position,
                    final_world_space_position);
        ED_view3d_project_v2(region, final_world_space_position, mouse);
      }
      else {
        mouse[0] = stroke->last_mouse_position[0] + dmouse[0] * spacing;
        mouse[1] = stroke->last_mouse_position[1] + dmouse[1] * spacing;
      }
      pressure = stroke->last_pressure + (spacing / length) * dpressure;

      ups->overlap_factor = paint_stroke_integrate_overlap(stroke->brush,
                                                           spacing / no_pressure_spacing);

      stroke->stroke_distance += spacing / stroke->zoom_2d;
      paint_brush_stroke_add_step(C, op, stroke, mouse, pressure);

      length -= spacing;
      pressure = stroke->last_pressure;
      dpressure = final_pressure - stroke->last_pressure;

      count++;
    }
    else {
      break;
    }
  }

  return count;
}

/**** Public API ****/

PaintStroke *paint_stroke_new(bContext *C,
                              wmOperator *op,
                              StrokeGetLocation get_location,
                              StrokeTestStart test_start,
                              StrokeUpdateStep update_step,
                              StrokeRedraw redraw,
                              StrokeDone done,
                              int event_type)
{
  struct Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  PaintStroke *stroke = MEM_callocN(sizeof(PaintStroke), "PaintStroke");
  ToolSettings *toolsettings = CTX_data_tool_settings(C);
  UnifiedPaintSettings *ups = &toolsettings->unified_paint_settings;
  Paint *p = BKE_paint_get_active_from_context(C);
  Brush *br = stroke->brush = BKE_paint_brush(p);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  float zoomx, zoomy;

  ED_view3d_viewcontext_init(C, &stroke->vc, depsgraph);

  stroke->get_location = get_location;
  stroke->test_start = test_start;
  stroke->update_step = update_step;
  stroke->redraw = redraw;
  stroke->done = done;
  stroke->event_type = event_type; /* for modal, return event */
  stroke->ups = ups;
  stroke->stroke_mode = RNA_enum_get(op->ptr, "mode");

  get_imapaint_zoom(C, &zoomx, &zoomy);
  stroke->zoom_2d = max_ff(zoomx, zoomy);

  if (stroke->stroke_mode == BRUSH_STROKE_INVERT) {
    if (br->flag & BRUSH_CURVE) {
      RNA_enum_set(op->ptr, "mode", BRUSH_STROKE_NORMAL);
    }
  }
  /* initialize here */
  ups->overlap_factor = 1.0;
  ups->stroke_active = true;

  if (rv3d) {
    rv3d->rflag |= RV3D_PAINTING;
  }

  /* Preserve location from last stroke while applying and resetting
   * ups->average_stroke_counter to 1.
   */
  if (ups->average_stroke_counter) {
    mul_v3_fl(ups->average_stroke_accum, 1.0f / (float)ups->average_stroke_counter);
    ups->average_stroke_counter = 1;
  }

  /* initialize here to avoid initialization conflict with threaded strokes */
  BKE_curvemapping_init(br->curve);
  if (p->flags & PAINT_USE_CAVITY_MASK) {
    BKE_curvemapping_init(p->cavity_curve);
  }

  BKE_paint_set_overlay_override(br->overlay_flags);

  ups->start_pixel_radius = BKE_brush_size_get(CTX_data_scene(C), br);

  return stroke;
}

void paint_stroke_free(bContext *C, wmOperator *UNUSED(op), PaintStroke *stroke)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (rv3d) {
    rv3d->rflag &= ~RV3D_PAINTING;
  }

  BKE_paint_set_overlay_override(0);

  if (stroke == NULL) {
    return;
  }

  UnifiedPaintSettings *ups = stroke->ups;
  ups->draw_anchored = false;
  ups->stroke_active = false;

  if (stroke->timer) {
    WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), stroke->timer);
  }

  if (stroke->rng) {
    BLI_rng_free(stroke->rng);
  }

  if (stroke->stroke_cursor) {
    WM_paint_cursor_end(stroke->stroke_cursor);
  }

  BLI_freelistN(&stroke->line);

  MEM_SAFE_FREE(stroke);
}

static void stroke_done(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  UnifiedPaintSettings *ups = stroke->ups;

  /* reset rotation here to avoid doing so in cursor display */
  if (!(stroke->brush->mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
    ups->brush_rotation = 0.0f;
  }

  if (!(stroke->brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
    ups->brush_rotation_sec = 0.0f;
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

static bool curves_sculpt_brush_uses_spacing(const eBrushCurvesSculptTool tool)
{
  return ELEM(tool, CURVES_SCULPT_TOOL_ADD);
}

bool paint_space_stroke_enabled(Brush *br, ePaintMode mode)
{
  if ((br->flag & BRUSH_SPACE) == 0) {
    return false;
  }

  if (br->sculpt_tool == SCULPT_TOOL_CLOTH || SCULPT_is_cloth_deform_brush(br)) {
    /* The Cloth Brush is a special case for stroke spacing. Even if it has grab modes which do
     * not support dynamic size, stroke spacing needs to be enabled so it is possible to control
     * whether the simulation runs constantly or only when the brush moves when using the cloth
     * grab brushes. */
    return true;
  }

  if (mode == PAINT_MODE_SCULPT_CURVES &&
      !curves_sculpt_brush_uses_spacing(br->curves_sculpt_tool)) {
    return false;
  }

  return paint_supports_dynamic_size(br, mode);
}

static bool sculpt_is_grab_tool(Brush *br)
{

  if (br->sculpt_tool == SCULPT_TOOL_CLOTH && br->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
    return true;
  }
  return ELEM(br->sculpt_tool,
              SCULPT_TOOL_GRAB,
              SCULPT_TOOL_ELASTIC_DEFORM,
              SCULPT_TOOL_POSE,
              SCULPT_TOOL_BOUNDARY,
              SCULPT_TOOL_THUMB,
              SCULPT_TOOL_ROTATE,
              SCULPT_TOOL_SNAKE_HOOK);
}

bool paint_supports_dynamic_size(Brush *br, ePaintMode mode)
{
  if (br->flag & BRUSH_ANCHORED) {
    return false;
  }

  switch (mode) {
    case PAINT_MODE_SCULPT:
      if (sculpt_is_grab_tool(br)) {
        return false;
      }
      break;

    case PAINT_MODE_TEXTURE_2D: /* fall through */
    case PAINT_MODE_TEXTURE_3D:
      if ((br->imagepaint_tool == PAINT_TOOL_FILL) && (br->flag & BRUSH_USE_GRADIENT)) {
        return false;
      }
      break;

    default:
      break;
  }
  return true;
}

bool paint_supports_smooth_stroke(Brush *br, ePaintMode mode)
{
  if (!(br->flag & BRUSH_SMOOTH_STROKE) ||
      (br->flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT | BRUSH_LINE))) {
    return false;
  }

  switch (mode) {
    case PAINT_MODE_SCULPT:
      if (sculpt_is_grab_tool(br)) {
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

bool paint_supports_texture(ePaintMode mode)
{
  /* omit: PAINT_WEIGHT, PAINT_SCULPT_UV, PAINT_INVALID */
  return ELEM(
      mode, PAINT_MODE_SCULPT, PAINT_MODE_VERTEX, PAINT_MODE_TEXTURE_3D, PAINT_MODE_TEXTURE_2D);
}

bool paint_supports_dynamic_tex_coords(Brush *br, ePaintMode mode)
{
  if (br->flag & BRUSH_ANCHORED) {
    return false;
  }

  switch (mode) {
    case PAINT_MODE_SCULPT:
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

struct wmKeyMap *paint_stroke_modal_keymap(struct wmKeyConfig *keyconf)
{
  static struct EnumPropertyItem modal_items[] = {
      {PAINT_STROKE_MODAL_CANCEL, "CANCEL", 0, "Cancel", "Cancel and undo a stroke in progress"},

      {0}};

  static const char *name = "Paint Stroke Modal";

  struct wmKeyMap *keymap = WM_modalkeymap_find(keyconf, name);

  /* this function is called for each spacetype, only needs to add map once */
  if (!keymap) {
    keymap = WM_modalkeymap_ensure(keyconf, name, modal_items);
  }

  return keymap;
}

static void paint_stroke_add_sample(
    const Paint *paint, PaintStroke *stroke, float x, float y, float pressure)
{
  PaintSample *sample = &stroke->samples[stroke->cur_sample];
  int max_samples = CLAMPIS(paint->num_input_samples, 1, PAINT_MAX_INPUT_SAMPLES);

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
    add_v2_v2(average->mouse, stroke->samples[i].mouse);
    average->pressure += stroke->samples[i].pressure;
  }

  mul_v2_fl(average->mouse, 1.0f / stroke->num_samples);
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
                                       float spacing,
                                       float *length_residue,
                                       const float old_pos[2],
                                       const float new_pos[2])
{
  UnifiedPaintSettings *ups = stroke->ups;
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  ARegion *region = CTX_wm_region(C);

  const bool use_scene_spacing = paint_stroke_use_scene_spacing(brush, mode);

  float mouse[2], dmouse[2];
  float length;
  float d_world_space_position[3] = {0.0f};
  float world_space_position_old[3], world_space_position_new[3];

  copy_v2_v2(stroke->last_mouse_position, old_pos);

  if (use_scene_spacing) {
    bool hit_old = SCULPT_stroke_get_location(C, world_space_position_old, old_pos);
    bool hit_new = SCULPT_stroke_get_location(C, world_space_position_new, new_pos);
    mul_m4_v3(stroke->vc.obact->obmat, world_space_position_old);
    mul_m4_v3(stroke->vc.obact->obmat, world_space_position_new);
    if (hit_old && hit_new && stroke->stroke_over_mesh) {
      sub_v3_v3v3(d_world_space_position, world_space_position_new, world_space_position_old);
      length = len_v3(d_world_space_position);
      stroke->stroke_over_mesh = true;
    }
    else {
      length = 0.0f;
      zero_v3(d_world_space_position);
      stroke->stroke_over_mesh = hit_new;
      if (stroke->stroke_over_mesh) {
        copy_v3_v3(stroke->last_world_space_position, world_space_position_old);
      }
    }
  }
  else {
    sub_v2_v2v2(dmouse, new_pos, old_pos);
    length = normalize_v2(dmouse);
  }

  BLI_assert(length >= 0.0f);

  if (length == 0.0f) {
    return;
  }

  while (length > 0.0f) {
    float spacing_final = spacing - *length_residue;
    length += *length_residue;
    *length_residue = 0.0;

    if (length >= spacing) {
      if (use_scene_spacing) {
        float final_world_space_position[3];
        normalize_v3(d_world_space_position);
        mul_v3_v3fl(final_world_space_position, d_world_space_position, spacing_final);
        add_v3_v3v3(
            final_world_space_position, world_space_position_old, final_world_space_position);
        ED_view3d_project_v2(region, final_world_space_position, mouse);
      }
      else {
        mouse[0] = stroke->last_mouse_position[0] + dmouse[0] * spacing_final;
        mouse[1] = stroke->last_mouse_position[1] + dmouse[1] * spacing_final;
      }

      ups->overlap_factor = paint_stroke_integrate_overlap(stroke->brush, 1.0);

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
                                  const float mouse[2])
{
  Brush *br = stroke->brush;
  if (stroke->stroke_started && (br->flag & BRUSH_LINE)) {
    stroke->ups->overlap_factor = paint_stroke_integrate_overlap(br, 1.0);

    paint_brush_stroke_add_step(C, op, stroke, stroke->last_mouse_position, 1.0);
    paint_space_stroke(C, op, stroke, mouse, 1.0);
  }
}

static bool paint_stroke_curve_end(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  Brush *br = stroke->brush;

  if (br->flag & BRUSH_CURVE) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    const Scene *scene = CTX_data_scene(C);
    const float spacing = paint_space_stroke_spacing(C, scene, stroke, 1.0f, 1.0f);
    PaintCurve *pc = br->paint_curve;
    PaintCurvePoint *pcp;
    float length_residue = 0.0f;
    int i;

    if (!pc) {
      return true;
    }

#ifdef DEBUG_TIME
    TIMEIT_START_AVERAGED(whole_stroke);
#endif

    pcp = pc->points;
    stroke->ups->overlap_factor = paint_stroke_integrate_overlap(br, 1.0);

    for (i = 0; i < pc->tot_points - 1; i++, pcp++) {
      int j;
      float data[(PAINT_CURVE_NUM_SEGMENTS + 1) * 2];
      float tangents[(PAINT_CURVE_NUM_SEGMENTS + 1) * 2];
      PaintCurvePoint *pcp_next = pcp + 1;
      bool do_rake = false;

      for (j = 0; j < 2; j++) {
        BKE_curve_forward_diff_bezier(pcp->bez.vec[1][j],
                                      pcp->bez.vec[2][j],
                                      pcp_next->bez.vec[0][j],
                                      pcp_next->bez.vec[1][j],
                                      data + j,
                                      PAINT_CURVE_NUM_SEGMENTS,
                                      sizeof(float[2]));
      }

      if ((br->mtex.brush_angle_mode & MTEX_ANGLE_RAKE) ||
          (br->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
        do_rake = true;
        for (j = 0; j < 2; j++) {
          BKE_curve_forward_diff_tangent_bezier(pcp->bez.vec[1][j],
                                                pcp->bez.vec[2][j],
                                                pcp_next->bez.vec[0][j],
                                                pcp_next->bez.vec[1][j],
                                                tangents + j,
                                                PAINT_CURVE_NUM_SEGMENTS,
                                                sizeof(float[2]));
        }
      }

      for (j = 0; j < PAINT_CURVE_NUM_SEGMENTS; j++) {
        if (do_rake) {
          float rotation = atan2f(tangents[2 * j], tangents[2 * j + 1]);
          paint_update_brush_rake_rotation(ups, br, rotation);
        }

        if (!stroke->stroke_started) {
          stroke->last_pressure = 1.0;
          copy_v2_v2(stroke->last_mouse_position, data + 2 * j);

          if (paint_stroke_use_scene_spacing(br, BKE_paintmode_get_active_from_context(C))) {
            stroke->stroke_over_mesh = SCULPT_stroke_get_location(
                C, stroke->last_world_space_position, data + 2 * j);
            mul_m4_v3(stroke->vc.obact->obmat, stroke->last_world_space_position);
          }

          stroke->stroke_started = stroke->test_start(C, op, stroke->last_mouse_position);

          if (stroke->stroke_started) {
            paint_brush_stroke_add_step(C, op, stroke, data + 2 * j, 1.0);
            paint_line_strokes_spacing(
                C, op, stroke, spacing, &length_residue, data + 2 * j, data + 2 * (j + 1));
          }
        }
        else {
          paint_line_strokes_spacing(
              C, op, stroke, spacing, &length_residue, data + 2 * j, data + 2 * (j + 1));
        }
      }
    }

    stroke_done(C, op, stroke);

#ifdef DEBUG_TIME
    TIMEIT_END_AVERAGED(whole_stroke);
#endif

    return true;
  }

  return false;
}

static void paint_stroke_line_constrain(PaintStroke *stroke, float mouse[2])
{
  if (stroke->constrain_line) {
    float line[2];
    float angle, len, res;

    sub_v2_v2v2(line, mouse, stroke->last_mouse_position);
    angle = atan2f(line[1], line[0]);
    len = len_v2(line);

    /* divide angle by PI/4 */
    angle = 4.0f * angle / (float)M_PI;

    /* now take residue */
    res = angle - floorf(angle);

    /* residue decides how close we are at a certain angle */
    if (res <= 0.5f) {
      angle = floorf(angle) * (float)M_PI_4;
    }
    else {
      angle = (floorf(angle) + 1.0f) * (float)M_PI_4;
    }

    mouse[0] = stroke->constrained_pos[0] = len * cosf(angle) + stroke->last_mouse_position[0];
    mouse[1] = stroke->constrained_pos[1] = len * sinf(angle) + stroke->last_mouse_position[1];
  }
}

int paint_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event, PaintStroke **stroke_p)
{
  Paint *p = BKE_paint_get_active_from_context(C);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  PaintStroke *stroke = *stroke_p;
  Brush *br = stroke->brush = BKE_paint_brush(p);
  PaintSample sample_average;
  float mouse[2];
  bool first_dab = false;
  bool first_modal = false;
  bool redraw = false;
  float pressure;

  if (event->type == INBETWEEN_MOUSEMOVE && !paint_tool_require_inbetween_mouse_events(br, mode)) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* see if tablet affects event. Line, anchored and drag dot strokes do not support pressure */
  pressure = ((br->flag & (BRUSH_LINE | BRUSH_ANCHORED | BRUSH_DRAG_DOT)) ?
                  1.0f :
                  WM_event_tablet_data(event, &stroke->pen_flip, NULL));

  /* When processing a timer event the pressure from the event is 0, so use the last valid
   * pressure. */
  if (event->type == TIMER) {
    pressure = stroke->last_tablet_event_pressure;
  }
  else {
    stroke->last_tablet_event_pressure = pressure;
  }

  paint_stroke_add_sample(p, stroke, event->mval[0], event->mval[1], pressure);
  paint_stroke_sample_average(stroke, &sample_average);

  /* Tilt. */
  if (WM_event_is_tablet(event)) {
    stroke->x_tilt = event->tablet.x_tilt;
    stroke->y_tilt = event->tablet.y_tilt;
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
      *stroke_p = NULL;
      return OPERATOR_FINISHED;
    }

    if (paint_supports_smooth_stroke(br, mode)) {
      stroke->stroke_cursor = WM_paint_cursor_activate(
          SPACE_TYPE_ANY, RGN_TYPE_ANY, PAINT_brush_tool_poll, paint_draw_smooth_cursor, stroke);
    }

    stroke->stroke_init = true;
    first_modal = true;
  }

  /* one time stroke initialization */
  if (!stroke->stroke_started) {
    stroke->last_pressure = sample_average.pressure;
    copy_v2_v2(stroke->last_mouse_position, sample_average.mouse);
    if (paint_stroke_use_scene_spacing(br, mode)) {
      stroke->stroke_over_mesh = SCULPT_stroke_get_location(
          C, stroke->last_world_space_position, sample_average.mouse);
      mul_m4_v3(stroke->vc.obact->obmat, stroke->last_world_space_position);
    }
    stroke->stroke_started = stroke->test_start(C, op, sample_average.mouse);
    BLI_assert((stroke->stroke_started & ~1) == 0); /* 0/1 */

    if (stroke->stroke_started) {
      if (br->flag & BRUSH_AIRBRUSH) {
        stroke->timer = WM_event_add_timer(
            CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);
      }

      if (br->flag & BRUSH_LINE) {
        stroke->stroke_cursor = WM_paint_cursor_activate(
            SPACE_TYPE_ANY, RGN_TYPE_ANY, PAINT_brush_tool_poll, paint_draw_line_cursor, stroke);
      }

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

  if (event->type == stroke->event_type && !first_modal) {
    if (event->val == KM_RELEASE) {
      copy_v2_fl2(mouse, event->mval[0], event->mval[1]);
      paint_stroke_line_constrain(stroke, mouse);
      paint_stroke_line_end(C, op, stroke, mouse);
      stroke_done(C, op, stroke);
      *stroke_p = NULL;
      return OPERATOR_FINISHED;
    }
  }
  else if (ELEM(event->type, EVT_RETKEY, EVT_SPACEKEY)) {
    paint_stroke_line_end(C, op, stroke, sample_average.mouse);
    stroke_done(C, op, stroke);
    *stroke_p = NULL;
    return OPERATOR_FINISHED;
  }
  else if (br->flag & BRUSH_LINE) {
    if (event->modifier & KM_ALT) {
      stroke->constrain_line = true;
    }
    else {
      stroke->constrain_line = false;
    }

    copy_v2_fl2(mouse, event->mval[0], event->mval[1]);
    paint_stroke_line_constrain(stroke, mouse);

    if (stroke->stroke_started &&
        (first_modal || (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)))) {
      if ((br->mtex.brush_angle_mode & MTEX_ANGLE_RAKE) ||
          (br->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
        copy_v2_v2(stroke->ups->last_rake, stroke->last_mouse_position);
      }
      paint_calculate_rake_rotation(stroke->ups, br, mouse);
    }
  }
  else if (first_modal ||
           /* regular dabs */
           (!(br->flag & BRUSH_AIRBRUSH) && (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE))) ||
           /* airbrush */
           ((br->flag & BRUSH_AIRBRUSH) && event->type == TIMER &&
            event->customdata == stroke->timer)) {
    if (paint_smooth_stroke(stroke, &sample_average, mode, mouse, &pressure)) {
      if (stroke->stroke_started) {
        if (paint_space_stroke_enabled(br, mode)) {
          if (paint_space_stroke(C, op, stroke, mouse, pressure)) {
            redraw = true;
          }
        }
        else {
          float dmouse[2];
          sub_v2_v2v2(dmouse, mouse, stroke->last_mouse_position);
          stroke->stroke_distance += len_v2(dmouse);
          paint_brush_stroke_add_step(C, op, stroke, mouse, pressure);
          redraw = true;
        }
      }
    }
  }

  /* we want the stroke to have the first daub at the start location
   * instead of waiting till we have moved the space distance */
  if (first_dab && paint_space_stroke_enabled(br, mode) && !(br->flag & BRUSH_SMOOTH_STROKE)) {
    stroke->ups->overlap_factor = paint_stroke_integrate_overlap(br, 1.0);
    paint_brush_stroke_add_step(C, op, stroke, sample_average.mouse, sample_average.pressure);
    redraw = true;
  }

  /* do updates for redraw. if event is in between mouse-move there are more
   * coming, so postpone potentially slow redraw updates until all are done */
  if (event->type != INBETWEEN_MOUSEMOVE) {
    wmWindow *window = CTX_wm_window(C);
    ARegion *region = CTX_wm_region(C);

    /* At the very least, invalidate the cursor */
    if (region && (p->flags & PAINT_SHOW_BRUSH)) {
      WM_paint_cursor_tag_redraw(window, region);
    }

    if (redraw && stroke->redraw) {
      stroke->redraw(C, stroke, false);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

int paint_stroke_exec(bContext *C, wmOperator *op, PaintStroke *stroke)
{
  /* only when executed for the first time */
  if (stroke->stroke_started == 0) {
    PropertyRNA *strokeprop;
    PointerRNA firstpoint;
    float mouse[2];

    strokeprop = RNA_struct_find_property(op->ptr, "stroke");

    if (RNA_property_collection_lookup_int(op->ptr, strokeprop, 0, &firstpoint)) {
      RNA_float_get_array(&firstpoint, "mouse", mouse);
      stroke->stroke_started = stroke->test_start(C, op, mouse);
    }
  }

  if (stroke->stroke_started) {
    RNA_BEGIN (op->ptr, itemptr, "stroke") {
      stroke->update_step(C, op, stroke, &itemptr);
    }
    RNA_END;
  }

  bool ok = (stroke->stroke_started != 0);

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

void *paint_stroke_mode_data(struct PaintStroke *stroke)
{
  return stroke->mode_data;
}

bool paint_stroke_flipped(struct PaintStroke *stroke)
{
  return stroke->pen_flip;
}

bool paint_stroke_inverted(struct PaintStroke *stroke)
{
  return stroke->stroke_mode == BRUSH_STROKE_INVERT;
}

float paint_stroke_distance_get(struct PaintStroke *stroke)
{
  return stroke->stroke_distance;
}

void paint_stroke_set_mode_data(PaintStroke *stroke, void *mode_data)
{
  stroke->mode_data = mode_data;
}

bool PAINT_brush_tool_poll(bContext *C)
{
  Paint *p = BKE_paint_get_active_from_context(C);
  Object *ob = CTX_data_active_object(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (p && ob && BKE_paint_brush(p) &&
      (area && ELEM(area->spacetype, SPACE_VIEW3D, SPACE_IMAGE)) &&
      (region && region->regiontype == RGN_TYPE_WINDOW)) {
    /* Check the current tool is a brush. */
    bToolRef *tref = area->runtime.tool;
    if (tref && tref->runtime && tref->runtime->data_block[0]) {
      return true;
    }
  }
  return false;
}

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_defaults.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_icons.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_render_ext.h" /* externtex */

static RNG *brush_rng;

void BKE_brush_system_init(void)
{
  brush_rng = BLI_rng_new(0);
  BLI_rng_srandom(brush_rng, 31415682);
}

void BKE_brush_system_exit(void)
{
  if (brush_rng == NULL) {
    return;
  }
  BLI_rng_free(brush_rng);
  brush_rng = NULL;
}

static void brush_defaults(Brush *brush)
{

  const Brush *brush_def = DNA_struct_default_get(Brush);

#define FROM_DEFAULT(member) memcpy(&brush->member, &brush_def->member, sizeof(brush->member))
#define FROM_DEFAULT_PTR(member) memcpy(brush->member, brush_def->member, sizeof(brush->member))

  FROM_DEFAULT(blend);
  FROM_DEFAULT(flag);
  FROM_DEFAULT(weight);
  FROM_DEFAULT(size);
  FROM_DEFAULT(alpha);
  FROM_DEFAULT(hardness);
  FROM_DEFAULT(autosmooth_factor);
  FROM_DEFAULT(topology_rake_factor);
  FROM_DEFAULT(crease_pinch_factor);
  FROM_DEFAULT(normal_radius_factor);
  FROM_DEFAULT(area_radius_factor);
  FROM_DEFAULT(sculpt_plane);
  FROM_DEFAULT(plane_offset);
  FROM_DEFAULT(clone.alpha);
  FROM_DEFAULT(normal_weight);
  FROM_DEFAULT(fill_threshold);
  FROM_DEFAULT(flag);
  FROM_DEFAULT(sampling_flag);
  FROM_DEFAULT_PTR(rgb);
  FROM_DEFAULT_PTR(secondary_rgb);
  FROM_DEFAULT(spacing);
  FROM_DEFAULT(smooth_stroke_radius);
  FROM_DEFAULT(smooth_stroke_factor);
  FROM_DEFAULT(rate);
  FROM_DEFAULT(jitter);
  FROM_DEFAULT(texture_sample_bias);
  FROM_DEFAULT(texture_overlay_alpha);
  FROM_DEFAULT(mask_overlay_alpha);
  FROM_DEFAULT(cursor_overlay_alpha);
  FROM_DEFAULT(overlay_flags);
  FROM_DEFAULT_PTR(add_col);
  FROM_DEFAULT_PTR(sub_col);
  FROM_DEFAULT(stencil_pos);
  FROM_DEFAULT(stencil_dimension);
  FROM_DEFAULT(mtex);
  FROM_DEFAULT(mask_mtex);

#undef FROM_DEFAULT
#undef FROM_DEFAULT_PTR
}

/* Datablock add/copy/free/make_local */

void BKE_brush_init(Brush *brush)
{
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(brush, id));

  MEMCPY_STRUCT_AFTER(brush, DNA_struct_default_get(Brush), id);

  /* enable fake user by default */
  id_fake_user_set(&brush->id);

  /* the default alpha falloff curve */
  BKE_brush_curve_preset(brush, CURVE_PRESET_SMOOTH);
}

/**
 * \note Resulting brush will have two users: one as a fake user,
 * another is assumed to be used by the caller.
 */
Brush *BKE_brush_add(Main *bmain, const char *name, const eObjectMode ob_mode)
{
  Brush *brush;

  brush = BKE_libblock_alloc(bmain, ID_BR, name, 0);

  BKE_brush_init(brush);

  brush->ob_mode = ob_mode;

  return brush;
}

/* add grease pencil settings */
void BKE_brush_init_gpencil_settings(Brush *brush)
{
  if (brush->gpencil_settings == NULL) {
    brush->gpencil_settings = MEM_callocN(sizeof(BrushGpencilSettings), "BrushGpencilSettings");
  }

  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->flag = 0;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;
  brush->gpencil_settings->draw_strength = 1.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_PEN;
  brush->gpencil_settings->flag |= GP_BRUSH_ENABLE_CURSOR;

  /* curves */
  brush->gpencil_settings->curve_sensitivity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_strength = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_jitter = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

/* add a new gp-brush */
Brush *BKE_brush_add_gpencil(Main *bmain, ToolSettings *ts, const char *name)
{
  Brush *brush;
  Paint *paint = &ts->gp_paint->paint;
  brush = BKE_brush_add(bmain, name, OB_MODE_PAINT_GPENCIL);

  BKE_paint_brush_set(paint, brush);
  id_us_min(&brush->id);

  brush->size = 3;

  /* grease pencil basic settings */
  BKE_brush_init_gpencil_settings(brush);

  /* return brush */
  return brush;
}

/* grease pencil cumapping->preset */
typedef enum eGPCurveMappingPreset {
  GPCURVE_PRESET_PENCIL = 0,
  GPCURVE_PRESET_INK = 1,
  GPCURVE_PRESET_INKNOISE = 2,
  GPCURVE_PRESET_MARKER = 3,
} eGPCurveMappingPreset;

static void brush_gpencil_curvemap_reset(CurveMap *cuma, int tot, int preset)
{
  if (cuma->curve) {
    MEM_freeN(cuma->curve);
  }

  cuma->totpoint = tot;
  cuma->curve = MEM_callocN(cuma->totpoint * sizeof(CurveMapPoint), __func__);

  switch (preset) {
    case GPCURVE_PRESET_PENCIL:
      cuma->curve[0].x = 0.0f;
      cuma->curve[0].y = 0.0f;
      cuma->curve[1].x = 0.75115f;
      cuma->curve[1].y = 0.25f;
      cuma->curve[2].x = 1.0f;
      cuma->curve[2].y = 1.0f;
      break;
    case GPCURVE_PRESET_INK:
      cuma->curve[0].x = 0.0f;
      cuma->curve[0].y = 0.0f;
      cuma->curve[1].x = 0.63448f;
      cuma->curve[1].y = 0.375f;
      cuma->curve[2].x = 1.0f;
      cuma->curve[2].y = 1.0f;
      break;
    case GPCURVE_PRESET_INKNOISE:
      cuma->curve[0].x = 0.0f;
      cuma->curve[0].y = 0.0f;
      cuma->curve[1].x = 0.63134f;
      cuma->curve[1].y = 0.3625f;
      cuma->curve[2].x = 1.0f;
      cuma->curve[2].y = 1.0f;
      break;
    case GPCURVE_PRESET_MARKER:
      cuma->curve[0].x = 0.0f;
      cuma->curve[0].y = 0.0f;
      cuma->curve[1].x = 0.38f;
      cuma->curve[1].y = 0.22f;
      cuma->curve[2].x = 0.65f;
      cuma->curve[2].y = 0.68f;
      cuma->curve[3].x = 1.0f;
      cuma->curve[3].y = 1.0f;
      break;
  }

  if (cuma->table) {
    MEM_freeN(cuma->table);
    cuma->table = NULL;
  }
}

/* create a set of grease pencil presets. */
void BKE_brush_gpencil_presets(Main *bmain, ToolSettings *ts)
{
#define SMOOTH_STROKE_RADIUS 40
#define SMOOTH_STROKE_FACTOR 0.9f

  Paint *paint = &ts->gp_paint->paint;

  Brush *brush, *deft;
  CurveMapping *custom_curve;

  /* Airbrush brush. */
  brush = BLI_findstring(&bmain->brushes, "Airbrush", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Airbrush");
  }

  brush->size = 300.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 0.4f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_STENGTH_PRESSURE;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.98f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 0.211f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->draw_sensitivity = 1.0f;

  brush->gpencil_tool = GPAINT_TOOL_DRAW;
  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_AIRBRUSH;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Create and link Black Dots material to brush.
   * This material is required because the brush uses the material to define how the stroke is
   * drawn. */
  Material *ma = BLI_findstring(&bmain->materials, "Black Dots", offsetof(ID, name) + 2);
  if (ma == NULL) {
    ma = BKE_gpencil_material_add(bmain, "Black Dots");
  }
  brush->gpencil_settings->material = ma;
  /* Pin the matterial to the brush. */
  brush->gpencil_settings->flag |= GP_BRUSH_MATERIAL_PINNED;

  /* Ink Pen brush. */
  brush = BLI_findstring(&bmain->brushes, "Ink Pen", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Ink Pen");
  }
  brush->size = 60.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 1.0f;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.7f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.1f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 3;
  brush->gpencil_settings->draw_subdivide = 0;
  brush->gpencil_settings->draw_random_sub = 0.0f;
  brush->gpencil_settings->simplify_f = 0.002f;

  brush->gpencil_settings->draw_random_press = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  /* Curve. */
  custom_curve = brush->gpencil_settings->curve_sensitivity;
  BKE_curvemapping_set_defaults(custom_curve, 0, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_initialize(custom_curve);
  brush_gpencil_curvemap_reset(custom_curve->cm, 3, GPCURVE_PRESET_INK);

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_INK;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Ink Pen Rough brush. */
  brush = BLI_findstring(&bmain->brushes, "Ink Pen Rough", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Ink Pen Rough");
  }

  brush->size = 60.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 1.0f;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.5f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag &= ~GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.0f;
  brush->gpencil_settings->draw_smoothlvl = 2;
  brush->gpencil_settings->thick_smoothfac = 0.0f;
  brush->gpencil_settings->thick_smoothlvl = 2;
  brush->gpencil_settings->draw_subdivide = 0;
  brush->gpencil_settings->draw_random_sub = 0.0f;
  brush->gpencil_settings->simplify_f = 0.000f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_RANDOM;
  brush->gpencil_settings->draw_random_press = 1.0f;
  brush->gpencil_settings->draw_random_strength = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  /* Curve. */
  custom_curve = brush->gpencil_settings->curve_sensitivity;
  BKE_curvemapping_set_defaults(custom_curve, 0, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_initialize(custom_curve);
  brush_gpencil_curvemap_reset(custom_curve->cm, 3, GPCURVE_PRESET_INKNOISE);

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_INKNOISE;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Marker Bold brush. */
  brush = BLI_findstring(&bmain->brushes, "Marker Bold", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Marker Bold");
  }
  brush->size = 150.0f;
  brush->gpencil_settings->flag &= ~GP_BRUSH_USE_PRESSURE;

  brush->gpencil_settings->draw_strength = 0.3f;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.6f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.1f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 3;
  brush->gpencil_settings->draw_subdivide = 0;
  brush->gpencil_settings->draw_random_sub = 0.0f;
  brush->gpencil_settings->simplify_f = 0.002f;

  brush->gpencil_settings->flag &= ~GP_BRUSH_GROUP_RANDOM;
  brush->gpencil_settings->draw_random_press = 0.0f;
  brush->gpencil_settings->draw_random_strength = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  /* Curve. */
  custom_curve = brush->gpencil_settings->curve_sensitivity;
  BKE_curvemapping_set_defaults(custom_curve, 0, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_initialize(custom_curve);
  brush_gpencil_curvemap_reset(custom_curve->cm, 4, GPCURVE_PRESET_MARKER);

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_MARKER;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Marker Chisel brush. */
  brush = BLI_findstring(&bmain->brushes, "Marker Chisel", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Marker Chisel");
  }
  brush->size = 80.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 1.0f;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.5f;
  brush->gpencil_settings->draw_angle = DEG2RAD(20.0f);
  brush->gpencil_settings->draw_angle_factor = 1.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.0f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 3;
  brush->gpencil_settings->draw_subdivide = 0;
  brush->gpencil_settings->draw_random_sub = 0;
  brush->gpencil_settings->simplify_f = 0.002f;

  brush->gpencil_settings->flag &= ~GP_BRUSH_GROUP_RANDOM;
  brush->gpencil_settings->draw_random_press = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_CHISEL;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Pen brush. */
  brush = BLI_findstring(&bmain->brushes, "Pen", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Pen");
  }
  brush->size = 30.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 1.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_STENGTH_PRESSURE;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.3f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.0f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 1;
  brush->gpencil_settings->draw_subdivide = 1;
  brush->gpencil_settings->draw_random_sub = 0.0f;
  brush->gpencil_settings->simplify_f = 0.002f;

  brush->gpencil_settings->draw_random_press = 0.0f;
  brush->gpencil_settings->draw_random_strength = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_PEN;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Pencil Soft brush. */
  brush = BLI_findstring(&bmain->brushes, "Pencil Soft", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Pencil Soft");
  }

  brush->size = 80.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 0.4f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_STENGTH_PRESSURE;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.64f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 0.8f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.0f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 3;
  brush->gpencil_settings->draw_subdivide = 0;
  brush->gpencil_settings->draw_random_sub = 0.0f;
  brush->gpencil_settings->simplify_f = 0.000f;

  brush->gpencil_settings->draw_random_press = 0.0f;
  brush->gpencil_settings->draw_random_strength = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_PENCIL;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Pencil brush. */
  brush = BLI_findstring(&bmain->brushes, "Pencil", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Pencil");
  }
  deft = brush; /* save default brush. */

  brush->size = 25.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_USE_PRESSURE | GP_BRUSH_ENABLE_CURSOR);

  brush->gpencil_settings->draw_strength = 0.6f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_STENGTH_PRESSURE;

  brush->gpencil_settings->input_samples = 10;
  brush->gpencil_settings->active_smooth = 0.55f;
  brush->gpencil_settings->draw_angle = 0.0f;
  brush->gpencil_settings->draw_angle_factor = 0.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;

  brush->gpencil_settings->flag |= GP_BRUSH_GROUP_SETTINGS;
  brush->gpencil_settings->draw_smoothfac = 0.0f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 3;
  brush->gpencil_settings->draw_subdivide = 0;
  brush->gpencil_settings->draw_random_sub = 0.0f;
  brush->gpencil_settings->simplify_f = 0.002f;

  brush->gpencil_settings->draw_random_press = 0.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;
  brush->gpencil_settings->draw_sensitivity = 1.0f;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_PENCIL;
  brush->gpencil_tool = GPAINT_TOOL_DRAW;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Fill brush. */
  brush = BLI_findstring(&bmain->brushes, "Fill Area", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Fill Area");
  }
  brush->size = 20.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_ENABLE_CURSOR;

  brush->gpencil_settings->fill_leak = 3;
  brush->gpencil_settings->fill_threshold = 0.1f;
  brush->gpencil_settings->fill_simplylvl = 1;
  brush->gpencil_settings->fill_factor = 1;

  brush->gpencil_settings->draw_strength = 1.0f;
  brush->gpencil_settings->gradient_f = 1.0f;
  brush->gpencil_settings->gradient_s[0] = 1.0f;
  brush->gpencil_settings->gradient_s[1] = 1.0f;
  brush->gpencil_settings->draw_smoothfac = 0.1f;
  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->thick_smoothfac = 1.0f;
  brush->gpencil_settings->thick_smoothlvl = 3;
  brush->gpencil_settings->draw_subdivide = 1;

  brush->gpencil_settings->draw_sensitivity = 1.0f;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_FILL;
  brush->gpencil_tool = GPAINT_TOOL_FILL;

  brush->smooth_stroke_radius = SMOOTH_STROKE_RADIUS;
  brush->smooth_stroke_factor = SMOOTH_STROKE_FACTOR;

  /* Soft Eraser brush. */
  brush = BLI_findstring(&bmain->brushes, "Eraser Soft", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Eraser Soft");
  }
  brush->size = 30.0f;
  brush->gpencil_settings->draw_strength = 0.5f;
  brush->gpencil_settings->flag |= (GP_BRUSH_ENABLE_CURSOR | GP_BRUSH_DEFAULT_ERASER);
  brush->gpencil_settings->flag |= GP_BRUSH_USE_PRESSURE;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_STENGTH_PRESSURE;
  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_SOFT;
  brush->gpencil_tool = GPAINT_TOOL_ERASE;
  brush->gpencil_settings->eraser_mode = GP_BRUSH_ERASER_SOFT;
  brush->gpencil_settings->era_strength_f = 100.0f;
  brush->gpencil_settings->era_thickness_f = 10.0f;

  /* Hard Eraser brush. */
  brush = BLI_findstring(&bmain->brushes, "Eraser Hard", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Eraser Hard");
  }
  brush->size = 30.0f;
  brush->gpencil_settings->draw_strength = 1.0f;
  brush->gpencil_settings->flag |= (GP_BRUSH_ENABLE_CURSOR | GP_BRUSH_DEFAULT_ERASER);
  brush->gpencil_settings->eraser_mode = GP_BRUSH_ERASER_SOFT;
  brush->gpencil_settings->era_strength_f = 100.0f;
  brush->gpencil_settings->era_thickness_f = 50.0f;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_HARD;
  brush->gpencil_tool = GPAINT_TOOL_ERASE;

  /* Point Eraser brush. */
  brush = BLI_findstring(&bmain->brushes, "Eraser Point", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Eraser Point");
  }
  brush->size = 30.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_ENABLE_CURSOR;
  brush->gpencil_settings->eraser_mode = GP_BRUSH_ERASER_HARD;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_HARD;
  brush->gpencil_tool = GPAINT_TOOL_ERASE;

  /* Stroke Eraser brush. */
  brush = BLI_findstring(&bmain->brushes, "Eraser Stroke", offsetof(ID, name) + 2);
  if (brush == NULL) {
    brush = BKE_brush_add_gpencil(bmain, ts, "Eraser Stroke");
  }
  brush->size = 30.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_ENABLE_CURSOR;
  brush->gpencil_settings->eraser_mode = GP_BRUSH_ERASER_STROKE;

  brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_STROKE;
  brush->gpencil_tool = GPAINT_TOOL_ERASE;

  /* set default brush. */
  BKE_paint_brush_set(paint, deft);
}

struct Brush *BKE_brush_first_search(struct Main *bmain, const eObjectMode ob_mode)
{
  Brush *brush;

  for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
    if (brush->ob_mode & ob_mode) {
      return brush;
    }
  }
  return NULL;
}

/**
 * Only copy internal data of Brush ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
void BKE_brush_copy_data(Main *UNUSED(bmain),
                         Brush *brush_dst,
                         const Brush *brush_src,
                         const int flag)
{
  if (brush_src->icon_imbuf) {
    brush_dst->icon_imbuf = IMB_dupImBuf(brush_src->icon_imbuf);
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&brush_dst->id, &brush_src->id);
  }
  else {
    brush_dst->preview = NULL;
  }

  brush_dst->curve = BKE_curvemapping_copy(brush_src->curve);
  if (brush_src->gpencil_settings != NULL) {
    brush_dst->gpencil_settings = MEM_dupallocN(brush_src->gpencil_settings);
    brush_dst->gpencil_settings->curve_sensitivity = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_sensitivity);
    brush_dst->gpencil_settings->curve_strength = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_strength);
    brush_dst->gpencil_settings->curve_jitter = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_jitter);
  }

  /* enable fake user by default */
  id_fake_user_set(&brush_dst->id);
}

Brush *BKE_brush_copy(Main *bmain, const Brush *brush)
{
  Brush *brush_copy;
  BKE_id_copy(bmain, &brush->id, (ID **)&brush_copy);
  return brush_copy;
}

/** Free (or release) any data used by this brush (does not free the brush itself). */
void BKE_brush_free(Brush *brush)
{
  if (brush->icon_imbuf) {
    IMB_freeImBuf(brush->icon_imbuf);
  }
  BKE_curvemapping_free(brush->curve);

  if (brush->gpencil_settings != NULL) {
    BKE_curvemapping_free(brush->gpencil_settings->curve_sensitivity);
    BKE_curvemapping_free(brush->gpencil_settings->curve_strength);
    BKE_curvemapping_free(brush->gpencil_settings->curve_jitter);
    MEM_SAFE_FREE(brush->gpencil_settings);
  }

  MEM_SAFE_FREE(brush->gradient);

  BKE_previewimg_free(&(brush->preview));
}

void BKE_brush_make_local(Main *bmain, Brush *brush, const int flags)
{
  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;
  bool is_local = false, is_lib = false;

  /* - only lib users: do nothing (unless force_local is set)
   * - only local users: set flag
   * - mixed: make copy
   */

  if (!ID_IS_LINKED(brush)) {
    return;
  }

  if (brush->clone.image) {
    /* Special case: ima always local immediately. Clone image should only have one user anyway. */
    BKE_lib_id_make_local(bmain, &brush->clone.image->id, false, 0);
  }

  BKE_library_ID_test_usages(bmain, brush, &is_local, &is_lib);

  if (lib_local || is_local) {
    if (!is_lib) {
      BKE_lib_id_clear_library_data(bmain, &brush->id);
      BKE_lib_id_expand_local(bmain, &brush->id);

      /* enable fake user by default */
      id_fake_user_set(&brush->id);
    }
    else {
      Brush *brush_new = BKE_brush_copy(bmain, brush); /* Ensures FAKE_USER is set */

      brush_new->id.us = 0;

      /* setting newid is mandatory for complex make_lib_local logic... */
      ID_NEW_SET(brush, brush_new);

      if (!lib_local) {
        BKE_libblock_remap(bmain, brush, brush_new, ID_REMAP_SKIP_INDIRECT_USAGE);
      }
    }
  }
}

void BKE_brush_debug_print_state(Brush *br)
{
  /* create a fake brush and set it to the defaults */
  Brush def = {{NULL}};
  brush_defaults(&def);

#define BR_TEST(field, t) \
  if (br->field != def.field) \
  printf("br->" #field " = %" #t ";\n", br->field)

#define BR_TEST_FLAG(_f) \
  if ((br->flag & _f) && !(def.flag & _f)) \
    printf("br->flag |= " #_f ";\n"); \
  else if (!(br->flag & _f) && (def.flag & _f)) \
  printf("br->flag &= ~" #_f ";\n")

#define BR_TEST_FLAG_OVERLAY(_f) \
  if ((br->overlay_flags & _f) && !(def.overlay_flags & _f)) \
    printf("br->overlay_flags |= " #_f ";\n"); \
  else if (!(br->overlay_flags & _f) && (def.overlay_flags & _f)) \
  printf("br->overlay_flags &= ~" #_f ";\n")

  /* print out any non-default brush state */
  BR_TEST(normal_weight, f);

  BR_TEST(blend, d);
  BR_TEST(size, d);

  /* br->flag */
  BR_TEST_FLAG(BRUSH_AIRBRUSH);
  BR_TEST_FLAG(BRUSH_ALPHA_PRESSURE);
  BR_TEST_FLAG(BRUSH_SIZE_PRESSURE);
  BR_TEST_FLAG(BRUSH_JITTER_PRESSURE);
  BR_TEST_FLAG(BRUSH_SPACING_PRESSURE);
  BR_TEST_FLAG(BRUSH_ANCHORED);
  BR_TEST_FLAG(BRUSH_DIR_IN);
  BR_TEST_FLAG(BRUSH_SPACE);
  BR_TEST_FLAG(BRUSH_SMOOTH_STROKE);
  BR_TEST_FLAG(BRUSH_PERSISTENT);
  BR_TEST_FLAG(BRUSH_ACCUMULATE);
  BR_TEST_FLAG(BRUSH_LOCK_ALPHA);
  BR_TEST_FLAG(BRUSH_ORIGINAL_NORMAL);
  BR_TEST_FLAG(BRUSH_OFFSET_PRESSURE);
  BR_TEST_FLAG(BRUSH_SPACE_ATTEN);
  BR_TEST_FLAG(BRUSH_ADAPTIVE_SPACE);
  BR_TEST_FLAG(BRUSH_LOCK_SIZE);
  BR_TEST_FLAG(BRUSH_EDGE_TO_EDGE);
  BR_TEST_FLAG(BRUSH_DRAG_DOT);
  BR_TEST_FLAG(BRUSH_INVERSE_SMOOTH_PRESSURE);
  BR_TEST_FLAG(BRUSH_PLANE_TRIM);
  BR_TEST_FLAG(BRUSH_FRONTFACE);
  BR_TEST_FLAG(BRUSH_CUSTOM_ICON);

  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_CURSOR);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_PRIMARY);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_SECONDARY);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE);

  BR_TEST(jitter, f);
  BR_TEST(spacing, d);
  BR_TEST(smooth_stroke_radius, d);
  BR_TEST(smooth_stroke_factor, f);
  BR_TEST(rate, f);

  BR_TEST(alpha, f);

  BR_TEST(sculpt_plane, d);

  BR_TEST(plane_offset, f);

  BR_TEST(autosmooth_factor, f);

  BR_TEST(topology_rake_factor, f);

  BR_TEST(crease_pinch_factor, f);

  BR_TEST(plane_trim, f);

  BR_TEST(texture_sample_bias, f);
  BR_TEST(texture_overlay_alpha, d);

  BR_TEST(add_col[0], f);
  BR_TEST(add_col[1], f);
  BR_TEST(add_col[2], f);
  BR_TEST(add_col[3], f);
  BR_TEST(sub_col[0], f);
  BR_TEST(sub_col[1], f);
  BR_TEST(sub_col[2], f);
  BR_TEST(sub_col[3], f);

  printf("\n");

#undef BR_TEST
#undef BR_TEST_FLAG
}

void BKE_brush_sculpt_reset(Brush *br)
{
  /* enable this to see any non-default
   * settings used by a brush: */
  // BKE_brush_debug_print_state(br);

  brush_defaults(br);
  BKE_brush_curve_preset(br, CURVE_PRESET_SMOOTH);

  /* Use the curve presets by default */
  br->curve_preset = BRUSH_CURVE_SMOOTH;

  /* Note that sculpt defaults where set when 0.5 was the default (now it's 1.0)
   * assign this so logic below can remain the same. */
  br->alpha = 0.5f;

  /* Brush settings */
  switch (br->sculpt_tool) {
    case SCULPT_TOOL_DRAW_SHARP:
      br->flag |= BRUSH_DIR_IN;
      br->curve_preset = BRUSH_CURVE_POW4;
      br->spacing = 5;
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      br->spacing = 10;
      br->alpha = 1.0f;
      break;
    case SCULPT_TOOL_CLAY:
      br->flag |= BRUSH_SIZE_PRESSURE;
      br->spacing = 3;
      br->autosmooth_factor = 0.25f;
      br->normal_radius_factor = 0.75f;
      break;
    case SCULPT_TOOL_CLAY_THUMB:
      br->alpha = 0.5f;
      br->normal_radius_factor = 1.0f;
      br->spacing = 6;
      br->hardness = 0.5f;
      br->flag |= BRUSH_SIZE_PRESSURE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      br->flag |= BRUSH_ACCUMULATE | BRUSH_SIZE_PRESSURE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      br->alpha = 0.6f;
      br->spacing = 5;
      br->normal_radius_factor = 1.55f;
      br->tip_roundness = 0.18f;
      br->curve_preset = BRUSH_CURVE_SMOOTHER;
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      br->flag2 |= BRUSH_MULTIPLANE_SCRAPE_DYNAMIC | BRUSH_MULTIPLANE_SCRAPE_PLANES_PREVIEW;
      br->alpha = 0.7f;
      br->normal_radius_factor = 0.70f;
      br->multiplane_scrape_angle = 60;
      br->curve_preset = BRUSH_CURVE_SMOOTH;
      br->spacing = 5;
      break;
    case SCULPT_TOOL_CREASE:
      br->flag |= BRUSH_DIR_IN;
      br->alpha = 0.25;
      break;
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FILL:
      br->alpha = 1.0f;
      br->spacing = 7;
      br->flag |= BRUSH_ACCUMULATE;
      br->flag |= BRUSH_INVERT_TO_SCRAPE_FILL;
      break;
    case SCULPT_TOOL_ROTATE:
      br->alpha = 1.0;
      break;
    case SCULPT_TOOL_SMOOTH:
      br->flag &= ~BRUSH_SPACE_ATTEN;
      br->spacing = 5;
      br->alpha = 0.7f;
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      br->alpha = 1.0f;
      br->rake_factor = 1.0f;
      break;
    case SCULPT_TOOL_THUMB:
      br->size = 75;
      br->flag &= ~BRUSH_ALPHA_PRESSURE;
      br->flag &= ~BRUSH_SPACE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      br->elastic_deform_volume_preservation = 0.4f;
      br->elastic_deform_type = BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE;
      br->flag &= ~BRUSH_ALPHA_PRESSURE;
      br->flag &= ~BRUSH_SPACE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      break;
    case SCULPT_TOOL_POSE:
      br->pose_smooth_iterations = 4;
      br->pose_ik_segments = 1;
      br->flag2 |= BRUSH_POSE_IK_ANCHORED;
      br->flag &= ~BRUSH_ALPHA_PRESSURE;
      br->flag &= ~BRUSH_SPACE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      break;
    case SCULPT_TOOL_DRAW_FACE_SETS:
      br->alpha = 0.5f;
      br->flag &= ~BRUSH_ALPHA_PRESSURE;
      br->flag &= ~BRUSH_SPACE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      break;
    case SCULPT_TOOL_GRAB:
      br->alpha = 0.4f;
      br->size = 75;
      br->flag &= ~BRUSH_ALPHA_PRESSURE;
      br->flag &= ~BRUSH_SPACE;
      br->flag &= ~BRUSH_SPACE_ATTEN;
      break;
    case SCULPT_TOOL_CLOTH:
      br->cloth_mass = 1.0f;
      br->cloth_damping = 0.01f;
      br->cloth_sim_limit = 2.5f;
      br->cloth_sim_falloff = 0.75f;
      br->cloth_deform_type = BRUSH_CLOTH_DEFORM_DRAG;
      br->flag &= ~(BRUSH_ALPHA_PRESSURE | BRUSH_SIZE_PRESSURE);
      break;
    default:
      break;
  }

  /* Cursor colors */

  /* Default Alpha */
  br->add_col[3] = 0.90f;
  br->sub_col[3] = 0.90f;

  switch (br->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_DRAW_SHARP:
    case SCULPT_TOOL_CLAY:
    case SCULPT_TOOL_CLAY_STRIPS:
    case SCULPT_TOOL_CLAY_THUMB:
    case SCULPT_TOOL_LAYER:
    case SCULPT_TOOL_INFLATE:
    case SCULPT_TOOL_BLOB:
    case SCULPT_TOOL_CREASE:
      br->add_col[0] = 0.0f;
      br->add_col[1] = 0.5f;
      br->add_col[2] = 1.0f;
      br->sub_col[0] = 0.0f;
      br->sub_col[1] = 0.5f;
      br->sub_col[2] = 1.0f;
      break;

    case SCULPT_TOOL_SMOOTH:
    case SCULPT_TOOL_FLATTEN:
    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      br->add_col[0] = 0.877f;
      br->add_col[1] = 0.142f;
      br->add_col[2] = 0.117f;
      br->sub_col[0] = 0.877f;
      br->sub_col[1] = 0.142f;
      br->sub_col[2] = 0.117f;
      break;

    case SCULPT_TOOL_PINCH:
    case SCULPT_TOOL_GRAB:
    case SCULPT_TOOL_SNAKE_HOOK:
    case SCULPT_TOOL_THUMB:
    case SCULPT_TOOL_NUDGE:
    case SCULPT_TOOL_ROTATE:
    case SCULPT_TOOL_ELASTIC_DEFORM:
    case SCULPT_TOOL_POSE:
    case SCULPT_TOOL_SLIDE_RELAX:
      br->add_col[0] = 1.0f;
      br->add_col[1] = 0.95f;
      br->add_col[2] = 0.005f;
      br->sub_col[0] = 1.0f;
      br->sub_col[1] = 0.95f;
      br->sub_col[2] = 0.005f;
      break;

    case SCULPT_TOOL_SIMPLIFY:
    case SCULPT_TOOL_MASK:
    case SCULPT_TOOL_DRAW_FACE_SETS:
      br->add_col[0] = 0.750000;
      br->add_col[1] = 0.750000;
      br->add_col[2] = 0.750000;
      br->sub_col[0] = 0.750000;
      br->sub_col[1] = 0.750000;
      br->sub_col[2] = 0.750000;
      break;

    case SCULPT_TOOL_CLOTH:
      br->add_col[0] = 1.0f;
      br->add_col[1] = 0.5f;
      br->add_col[2] = 0.1f;
      br->sub_col[0] = 1.0f;
      br->sub_col[1] = 0.5f;
      br->sub_col[2] = 0.1f;
      break;
    default:
      break;
  }
}

/**
 * Library Operations
 */
void BKE_brush_curve_preset(Brush *b, eCurveMappingPreset preset)
{
  CurveMapping *cumap = NULL;
  CurveMap *cuma = NULL;

  if (!b->curve) {
    b->curve = BKE_curvemapping_add(1, 0, 0, 1, 1);
  }
  cumap = b->curve;
  cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  cumap->preset = preset;

  cuma = b->curve->cm;
  BKE_curvemap_reset(cuma, &cumap->clipr, cumap->preset, CURVEMAP_SLOPE_NEGATIVE);
  BKE_curvemapping_changed(cumap, false);
}

/* Generic texture sampler for 3D painting systems. point has to be either in
 * region space mouse coordinates, or 3d world coordinates for 3D mapping.
 *
 * rgba outputs straight alpha. */
float BKE_brush_sample_tex_3d(const Scene *scene,
                              const Brush *br,
                              const float point[3],
                              float rgba[4],
                              const int thread,
                              struct ImagePool *pool)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  const MTex *mtex = &br->mtex;
  float intensity = 1.0;
  bool hasrgb = false;

  if (!mtex->tex) {
    intensity = 1;
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex
     * location directly into a texture */
    hasrgb = externtex(
        mtex, point, &intensity, rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool, false, false);
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) {
    float rotation = -mtex->rot;
    float point_2d[2] = {point[0], point[1]};
    float x, y;
    float co[3];

    x = point_2d[0] - br->stencil_pos[0];
    y = point_2d[1] - br->stencil_pos[1];

    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    if (fabsf(x) > br->stencil_dimension[0] || fabsf(y) > br->stencil_dimension[1]) {
      zero_v4(rgba);
      return 0.0f;
    }
    x /= (br->stencil_dimension[0]);
    y /= (br->stencil_dimension[1]);

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    hasrgb = externtex(
        mtex, co, &intensity, rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool, false, false);
  }
  else {
    float rotation = -mtex->rot;
    float point_2d[2] = {point[0], point[1]};
    float x = 0.0f, y = 0.0f; /* Quite warnings */
    float invradius = 1.0f;   /* Quite warnings */
    float co[3];

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      /* keep coordinates relative to mouse */

      rotation += ups->brush_rotation;

      x = point_2d[0] - ups->tex_mouse[0];
      y = point_2d[1] - ups->tex_mouse[1];

      /* use pressure adjusted size for fixed mode */
      invradius = 1.0f / ups->pixel_radius;
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      /* leave the coordinates relative to the screen */

      /* use unadjusted size for tiled mode */
      invradius = 1.0f / BKE_brush_size_get(scene, br);

      x = point_2d[0];
      y = point_2d[1];
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      rotation += ups->brush_rotation;
      /* these contain a random coordinate */
      x = point_2d[0] - ups->tex_mouse[0];
      y = point_2d[1] - ups->tex_mouse[1];

      invradius = 1.0f / ups->pixel_radius;
    }

    x *= invradius;
    y *= invradius;

    /* it is probably worth optimizing for those cases where
     * the texture is not rotated by skipping the calls to
     * atan2, sqrtf, sin, and cos. */
    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    hasrgb = externtex(
        mtex, co, &intensity, rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool, false, false);
  }

  intensity += br->texture_sample_bias;

  if (!hasrgb) {
    rgba[0] = intensity;
    rgba[1] = intensity;
    rgba[2] = intensity;
    rgba[3] = 1.0f;
  }
  /* For consistency, sampling always returns color in linear space */
  else if (ups->do_linear_conversion) {
    IMB_colormanagement_colorspace_to_scene_linear_v3(rgba, ups->colorspace);
  }

  return intensity;
}

float BKE_brush_sample_masktex(
    const Scene *scene, Brush *br, const float point[2], const int thread, struct ImagePool *pool)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  MTex *mtex = &br->mask_mtex;
  float rgba[4], intensity;

  if (!mtex->tex) {
    return 1.0f;
  }
  if (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) {
    float rotation = -mtex->rot;
    float point_2d[2] = {point[0], point[1]};
    float x, y;
    float co[3];

    x = point_2d[0] - br->mask_stencil_pos[0];
    y = point_2d[1] - br->mask_stencil_pos[1];

    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    if (fabsf(x) > br->mask_stencil_dimension[0] || fabsf(y) > br->mask_stencil_dimension[1]) {
      zero_v4(rgba);
      return 0.0f;
    }
    x /= (br->mask_stencil_dimension[0]);
    y /= (br->mask_stencil_dimension[1]);

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    externtex(
        mtex, co, &intensity, rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool, false, false);
  }
  else {
    float rotation = -mtex->rot;
    float point_2d[2] = {point[0], point[1]};
    float x = 0.0f, y = 0.0f; /* Quite warnings */
    float invradius = 1.0f;   /* Quite warnings */
    float co[3];

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      /* keep coordinates relative to mouse */

      rotation += ups->brush_rotation_sec;

      x = point_2d[0] - ups->mask_tex_mouse[0];
      y = point_2d[1] - ups->mask_tex_mouse[1];

      /* use pressure adjusted size for fixed mode */
      invradius = 1.0f / ups->pixel_radius;
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      /* leave the coordinates relative to the screen */

      /* use unadjusted size for tiled mode */
      invradius = 1.0f / BKE_brush_size_get(scene, br);

      x = point_2d[0];
      y = point_2d[1];
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      rotation += ups->brush_rotation_sec;
      /* these contain a random coordinate */
      x = point_2d[0] - ups->mask_tex_mouse[0];
      y = point_2d[1] - ups->mask_tex_mouse[1];

      invradius = 1.0f / ups->pixel_radius;
    }

    x *= invradius;
    y *= invradius;

    /* it is probably worth optimizing for those cases where
     * the texture is not rotated by skipping the calls to
     * atan2, sqrtf, sin, and cos. */
    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    externtex(
        mtex, co, &intensity, rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool, false, false);
  }

  CLAMP(intensity, 0.0f, 1.0f);

  switch (br->mask_pressure) {
    case BRUSH_MASK_PRESSURE_CUTOFF:
      intensity = ((1.0f - intensity) < ups->size_pressure_value) ? 1.0f : 0.0f;
      break;
    case BRUSH_MASK_PRESSURE_RAMP:
      intensity = ups->size_pressure_value + intensity * (1.0f - ups->size_pressure_value);
      break;
    default:
      break;
  }

  return intensity;
}

/* Unified Size / Strength / Color */

/* XXX: be careful about setting size and unprojected radius
 * because they depend on one another
 * these functions do not set the other corresponding value
 * this can lead to odd behavior if size and unprojected
 * radius become inconsistent.
 * the biggest problem is that it isn't possible to change
 * unprojected radius because a view context is not
 * available.  my usual solution to this is to use the
 * ratio of change of the size to change the unprojected
 * radius.  Not completely convinced that is correct.
 * In any case, a better solution is needed to prevent
 * inconsistency. */

const float *BKE_brush_color_get(const struct Scene *scene, const struct Brush *brush)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  return (ups->flag & UNIFIED_PAINT_COLOR) ? ups->rgb : brush->rgb;
}

const float *BKE_brush_secondary_color_get(const struct Scene *scene, const struct Brush *brush)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  return (ups->flag & UNIFIED_PAINT_COLOR) ? ups->secondary_rgb : brush->secondary_rgb;
}

void BKE_brush_color_set(struct Scene *scene, struct Brush *brush, const float color[3])
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_COLOR) {
    copy_v3_v3(ups->rgb, color);
  }
  else {
    copy_v3_v3(brush->rgb, color);
  }
}

void BKE_brush_size_set(Scene *scene, Brush *brush, int size)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  /* make sure range is sane */
  CLAMP(size, 1, MAX_BRUSH_PIXEL_RADIUS);

  if (ups->flag & UNIFIED_PAINT_SIZE) {
    ups->size = size;
  }
  else {
    brush->size = size;
  }
}

int BKE_brush_size_get(const Scene *scene, const Brush *brush)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  int size = (ups->flag & UNIFIED_PAINT_SIZE) ? ups->size : brush->size;

  return size;
}

bool BKE_brush_use_locked_size(const Scene *scene, const Brush *brush)
{
  const short us_flag = scene->toolsettings->unified_paint_settings.flag;

  return (us_flag & UNIFIED_PAINT_SIZE) ? (us_flag & UNIFIED_PAINT_BRUSH_LOCK_SIZE) :
                                          (brush->flag & BRUSH_LOCK_SIZE);
}

bool BKE_brush_use_size_pressure(const Brush *brush)
{
  return brush->flag & BRUSH_SIZE_PRESSURE;
}

bool BKE_brush_use_alpha_pressure(const Brush *brush)
{
  return brush->flag & BRUSH_ALPHA_PRESSURE;
}

bool BKE_brush_sculpt_has_secondary_color(const Brush *brush)
{
  return ELEM(brush->sculpt_tool,
              SCULPT_TOOL_BLOB,
              SCULPT_TOOL_DRAW,
              SCULPT_TOOL_DRAW_SHARP,
              SCULPT_TOOL_INFLATE,
              SCULPT_TOOL_CLAY,
              SCULPT_TOOL_CLAY_STRIPS,
              SCULPT_TOOL_CLAY_THUMB,
              SCULPT_TOOL_PINCH,
              SCULPT_TOOL_CREASE,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_FLATTEN,
              SCULPT_TOOL_FILL,
              SCULPT_TOOL_SCRAPE,
              SCULPT_TOOL_MASK);
}

void BKE_brush_unprojected_radius_set(Scene *scene, Brush *brush, float unprojected_radius)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_SIZE) {
    ups->unprojected_radius = unprojected_radius;
  }
  else {
    brush->unprojected_radius = unprojected_radius;
  }
}

float BKE_brush_unprojected_radius_get(const Scene *scene, const Brush *brush)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_SIZE) ? ups->unprojected_radius : brush->unprojected_radius;
}

void BKE_brush_alpha_set(Scene *scene, Brush *brush, float alpha)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_ALPHA) {
    ups->alpha = alpha;
  }
  else {
    brush->alpha = alpha;
  }
}

float BKE_brush_alpha_get(const Scene *scene, const Brush *brush)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_ALPHA) ? ups->alpha : brush->alpha;
}

float BKE_brush_weight_get(const Scene *scene, const Brush *brush)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_WEIGHT) ? ups->weight : brush->weight;
}

void BKE_brush_weight_set(const Scene *scene, Brush *brush, float value)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_WEIGHT) {
    ups->weight = value;
  }
  else {
    brush->weight = value;
  }
}

/* scale unprojected radius to reflect a change in the brush's 2D size */
void BKE_brush_scale_unprojected_radius(float *unprojected_radius,
                                        int new_brush_size,
                                        int old_brush_size)
{
  float scale = new_brush_size;
  /* avoid division by zero */
  if (old_brush_size != 0) {
    scale /= (float)old_brush_size;
  }
  (*unprojected_radius) *= scale;
}

/* scale brush size to reflect a change in the brush's unprojected radius */
void BKE_brush_scale_size(int *r_brush_size,
                          float new_unprojected_radius,
                          float old_unprojected_radius)
{
  float scale = new_unprojected_radius;
  /* avoid division by zero */
  if (old_unprojected_radius != 0) {
    scale /= new_unprojected_radius;
  }
  (*r_brush_size) = (int)((float)(*r_brush_size) * scale);
}

void BKE_brush_jitter_pos(const Scene *scene, Brush *brush, const float pos[2], float jitterpos[2])
{
  float rand_pos[2];
  float spread;
  int diameter;

  do {
    rand_pos[0] = BLI_rng_get_float(brush_rng) - 0.5f;
    rand_pos[1] = BLI_rng_get_float(brush_rng) - 0.5f;
  } while (len_squared_v2(rand_pos) > SQUARE(0.5f));

  if (brush->flag & BRUSH_ABSOLUTE_JITTER) {
    diameter = 2 * brush->jitter_absolute;
    spread = 1.0;
  }
  else {
    diameter = 2 * BKE_brush_size_get(scene, brush);
    spread = brush->jitter;
  }
  /* find random position within a circle of diameter 1 */
  jitterpos[0] = pos[0] + 2 * rand_pos[0] * diameter * spread;
  jitterpos[1] = pos[1] + 2 * rand_pos[1] * diameter * spread;
}

void BKE_brush_randomize_texture_coords(UnifiedPaintSettings *ups, bool mask)
{
  /* we multiply with brush radius as an optimization for the brush
   * texture sampling functions */
  if (mask) {
    ups->mask_tex_mouse[0] = BLI_rng_get_float(brush_rng) * ups->pixel_radius;
    ups->mask_tex_mouse[1] = BLI_rng_get_float(brush_rng) * ups->pixel_radius;
  }
  else {
    ups->tex_mouse[0] = BLI_rng_get_float(brush_rng) * ups->pixel_radius;
    ups->tex_mouse[1] = BLI_rng_get_float(brush_rng) * ups->pixel_radius;
  }
}

/* Uses the brush curve control to find a strength value */
float BKE_brush_curve_strength(const Brush *br, float p, const float len)
{
  float strength = 1.0f;

  if (p >= len) {
    return 0;
  }
  else {
    p = p / len;
    p = 1.0f - p;
  }

  switch (br->curve_preset) {
    case BRUSH_CURVE_CUSTOM:
      strength = BKE_curvemapping_evaluateF(br->curve, 0, 1.0f - p);
      break;
    case BRUSH_CURVE_SHARP:
      strength = p * p;
      break;
    case BRUSH_CURVE_SMOOTH:
      strength = 3.0f * p * p - 2.0f * p * p * p;
      break;
    case BRUSH_CURVE_SMOOTHER:
      strength = pow3f(p) * (p * (p * 6.0f - 15.0f) + 10.0f);
      break;
    case BRUSH_CURVE_ROOT:
      strength = sqrtf(p);
      break;
    case BRUSH_CURVE_LIN:
      strength = p;
      break;
    case BRUSH_CURVE_CONSTANT:
      strength = 1.0f;
      break;
    case BRUSH_CURVE_SPHERE:
      strength = sqrtf(2 * p - p * p);
      break;
    case BRUSH_CURVE_POW4:
      strength = p * p * p * p;
      break;
    case BRUSH_CURVE_INVSQUARE:
      strength = p * (2.0f - p);
      break;
  }

  return strength;
}

/* Uses the brush curve control to find a strength value between 0 and 1 */
float BKE_brush_curve_strength_clamped(Brush *br, float p, const float len)
{
  float strength = BKE_brush_curve_strength(br, p, len);

  CLAMP(strength, 0.0f, 1.0f);

  return strength;
}

/* TODO: should probably be unified with BrushPainter stuff? */
unsigned int *BKE_brush_gen_texture_cache(Brush *br, int half_side, bool use_secondary)
{
  unsigned int *texcache = NULL;
  MTex *mtex = (use_secondary) ? &br->mask_mtex : &br->mtex;
  float intensity;
  float rgba[4];
  int ix, iy;
  int side = half_side * 2;

  if (mtex->tex) {
    float x, y, step = 2.0 / side, co[3];

    texcache = MEM_callocN(sizeof(int) * side * side, "Brush texture cache");

    /* do normalized canonical view coords for texture */
    for (y = -1.0, iy = 0; iy < side; iy++, y += step) {
      for (x = -1.0, ix = 0; ix < side; ix++, x += step) {
        co[0] = x;
        co[1] = y;
        co[2] = 0.0f;

        /* This is copied from displace modifier code */
        /* TODO(sergey): brush are always caching with CM enabled for now. */
        externtex(mtex, co, &intensity, rgba, rgba + 1, rgba + 2, rgba + 3, 0, NULL, false, false);

        ((char *)texcache)[(iy * side + ix) * 4] = ((char *)texcache)[(iy * side + ix) * 4 + 1] =
            ((char *)texcache)[(iy * side + ix) * 4 + 2] = ((
                char *)texcache)[(iy * side + ix) * 4 + 3] = (char)(intensity * 255.0f);
      }
    }
  }

  return texcache;
}

/**** Radial Control ****/
struct ImBuf *BKE_brush_gen_radial_control_imbuf(Brush *br, bool secondary, bool display_gradient)
{
  ImBuf *im = MEM_callocN(sizeof(ImBuf), "radial control texture");
  unsigned int *texcache;
  int side = 512;
  int half = side / 2;
  int i, j;

  BKE_curvemapping_initialize(br->curve);
  texcache = BKE_brush_gen_texture_cache(br, half, secondary);
  im->rect_float = MEM_callocN(sizeof(float) * side * side, "radial control rect");
  im->x = im->y = side;

  if (display_gradient || texcache) {
    for (i = 0; i < side; i++) {
      for (j = 0; j < side; j++) {
        float magn = sqrtf(pow2f(i - half) + pow2f(j - half));
        im->rect_float[i * side + j] = BKE_brush_curve_strength_clamped(br, magn, half);
      }
    }
  }

  if (texcache) {
    /* Modulate curve with texture */
    for (i = 0; i < side; i++) {
      for (j = 0; j < side; j++) {
        const int col = texcache[i * side + j];
        im->rect_float[i * side + j] *= (((char *)&col)[0] + ((char *)&col)[1] +
                                         ((char *)&col)[2]) /
                                        3.0f / 255.0f;
      }
    }
    MEM_freeN(texcache);
  }

  return im;
}

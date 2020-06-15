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
 *
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * This is a new part of Blender
 * Brush based operators for editing Grease Pencil strokes
 */

/** \file
 * \ingroup edgpencil
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object_deform.h"
#include "BKE_report.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* General Brush Editing Context */

/* Context for brush operators */
typedef struct tGP_BrushEditData {
  /* Current editor/region/etc. */
  Depsgraph *depsgraph;
  Main *bmain;
  Scene *scene;
  Object *object;

  ScrArea *area;
  ARegion *region;

  /* Current GPencil datablock */
  bGPdata *gpd;

  /* Brush Settings */
  GP_Sculpt_Settings *settings;
  Brush *brush;
  Brush *brush_prev;

  eGP_Sculpt_Flag flag;
  eGP_Sculpt_SelectMaskFlag mask;

  /* Space Conversion Data */
  GP_SpaceConversion gsc;

  /* Is the brush currently painting? */
  bool is_painting;
  bool is_transformed;

  /* Start of new sculpt stroke */
  bool first;

  /* Is multiframe editing enabled, and are we using falloff for that? */
  bool is_multiframe;
  bool use_multiframe_falloff;

  /* Current frame */
  int cfra;

  /* Brush Runtime Data: */
  /* - position and pressure
   * - the *_prev variants are the previous values
   */
  float mval[2], mval_prev[2];
  float pressure, pressure_prev;

  /* - effect vector (e.g. 2D/3D translation for grab brush) */
  float dvec[3];

  /* rotation for evaluated data */
  float rot_eval;

  /* - multiframe falloff factor */
  float mf_falloff;

  /* active vertex group */
  int vrgroup;

  /* brush geometry (bounding box) */
  rcti brush_rect;

  /* Custom data for certain brushes */
  /* - map from bGPDstroke's to structs containing custom data about those strokes */
  GHash *stroke_customdata;
  /* - general customdata */
  void *customdata;

  /* Timer for in-place accumulation of brush effect */
  wmTimer *timer;
  bool timerTick; /* is this event from a timer */

  /* Object invert matrix */
  float inv_mat[4][4];

  RNG *rng;
} tGP_BrushEditData;

/* Callback for performing some brush operation on a single point */
typedef bool (*GP_BrushApplyCb)(tGP_BrushEditData *gso,
                                bGPDstroke *gps,
                                float rotation,
                                int pt_index,
                                const int radius,
                                const int co[2]);

/* ************************************************ */
/* Utility Functions */

/* apply lock axis reset */
static void gpsculpt_compute_lock_axis(tGP_BrushEditData *gso,
                                       bGPDspoint *pt,
                                       const float save_pt[3])
{
  const ToolSettings *ts = gso->scene->toolsettings;
  const View3DCursor *cursor = &gso->scene->cursor;
  const int axis = ts->gp_sculpt.lock_axis;

  /* lock axis control */
  switch (axis) {
    case GP_LOCKAXIS_X: {
      pt->x = save_pt[0];
      break;
    }
    case GP_LOCKAXIS_Y: {
      pt->y = save_pt[1];
      break;
    }
    case GP_LOCKAXIS_Z: {
      pt->z = save_pt[2];
      break;
    }
    case GP_LOCKAXIS_CURSOR: {
      /* Compute a plane with cursor normal and position of the point before do the sculpt. */
      const float scale[3] = {1.0f, 1.0f, 1.0f};
      float plane_normal[3] = {0.0f, 0.0f, 1.0f};
      float plane[4];
      float mat[4][4];
      float r_close[3];

      loc_eul_size_to_mat4(mat, cursor->location, cursor->rotation_euler, scale);

      mul_mat3_m4_v3(mat, plane_normal);
      plane_from_point_normal_v3(plane, save_pt, plane_normal);

      /* find closest point to the plane with the new position */
      closest_to_plane_v3(r_close, plane, &pt->x);
      copy_v3_v3(&pt->x, r_close);
      break;
    }
    default: {
      break;
    }
  }
}

/* Context ---------------------------------------- */

/* Get the sculpting settings */
static GP_Sculpt_Settings *gpsculpt_get_settings(Scene *scene)
{
  return &scene->toolsettings->gp_sculpt;
}

/* Brush Operations ------------------------------- */

/* Invert behavior of brush? */
static bool gp_brush_invert_check(tGP_BrushEditData *gso)
{
  /* The basic setting is the brush's setting (from the panel) */
  bool invert = ((gso->brush->gpencil_settings->sculpt_flag & GP_SCULPT_FLAG_INVERT) != 0) ||
                (gso->brush->gpencil_settings->sculpt_flag & BRUSH_DIR_IN);
  /* During runtime, the user can hold down the Ctrl key to invert the basic behavior */
  if (gso->flag & GP_SCULPT_FLAG_INVERT) {
    invert ^= true;
  }

  /* set temporary status */
  if (invert) {
    gso->brush->gpencil_settings->sculpt_flag |= GP_SCULPT_FLAG_TMP_INVERT;
  }
  else {
    gso->brush->gpencil_settings->sculpt_flag &= ~GP_SCULPT_FLAG_TMP_INVERT;
  }

  return invert;
}

/* Compute strength of effect */
static float gp_brush_influence_calc(tGP_BrushEditData *gso, const int radius, const int co[2])
{
  Brush *brush = gso->brush;

  /* basic strength factor from brush settings */
  float influence = brush->alpha;

  /* use pressure? */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    influence *= gso->pressure;
  }

  /* distance fading */
  int mval_i[2];
  round_v2i_v2fl(mval_i, gso->mval);
  float distance = (float)len_v2v2_int(mval_i, co);

  /* Apply Brush curve. */
  float brush_fallof = BKE_brush_curve_strength(brush, distance, (float)radius);
  influence *= brush_fallof;

  /* apply multiframe falloff */
  influence *= gso->mf_falloff;

  /* return influence */
  return influence;
}

/* Tag stroke to be recalculated. */
static void gpencil_recalc_geometry_tag(bGPDstroke *gps)
{
  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  gps_active->flag |= GP_STROKE_TAG;
}

/* Recalc any stroke tagged. */
static void gpencil_update_geometry(bGPdata *gpd)
{
  if (gpd == NULL) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if ((gpl->actframe != gpf) && ((gpf->flag & GP_FRAME_SELECT) == 0)) {
        continue;
      }

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->flag & GP_STROKE_TAG) {
          BKE_gpencil_stroke_geometry_update(gps);
          gps->flag &= ~GP_STROKE_TAG;
        }
      }
    }
  }
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

/* ************************************************ */
/* Brush Callbacks */
/* This section defines the callbacks used by each brush to perform their magic.
 * These are called on each point within the brush's radius.
 */

/* ----------------------------------------------- */
/* Smooth Brush */

/* A simple (but slower + inaccurate)
 * smooth-brush implementation to test the algorithm for stroke smoothing. */
static bool gp_brush_smooth_apply(tGP_BrushEditData *gso,
                                  bGPDstroke *gps,
                                  float UNUSED(rot_eval),
                                  int pt_index,
                                  const int radius,
                                  const int co[2])
{
  float inf = gp_brush_influence_calc(gso, radius, co);

  /* perform smoothing */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
    BKE_gpencil_stroke_smooth(gps, pt_index, inf);
  }
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_STRENGTH) {
    BKE_gpencil_stroke_smooth_strength(gps, pt_index, inf);
  }
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_THICKNESS) {
    BKE_gpencil_stroke_smooth_thickness(gps, pt_index, inf);
  }
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_UV) {
    BKE_gpencil_stroke_smooth_uv(gps, pt_index, inf);
  }

  return true;
}

/* ----------------------------------------------- */
/* Line Thickness Brush */

/* Make lines thicker or thinner by the specified amounts */
static bool gp_brush_thickness_apply(tGP_BrushEditData *gso,
                                     bGPDstroke *gps,
                                     float UNUSED(rot_eval),
                                     int pt_index,
                                     const int radius,
                                     const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float inf;

  /* Compute strength of effect
   * - We divide the strength by 10, so that users can set "sane" values.
   *   Otherwise, good default values are in the range of 0.093
   */
  inf = gp_brush_influence_calc(gso, radius, co) / 10.0f;

  /* apply */
  /* XXX: this is much too strong,
   * and it should probably do some smoothing with the surrounding stuff. */
  if (gp_brush_invert_check(gso)) {
    /* make line thinner - reduce stroke pressure */
    pt->pressure -= inf;
  }
  else {
    /* make line thicker - increase stroke pressure */
    pt->pressure += inf;
  }

  /* Pressure should stay within [0.0, 1.0]
   * However, it is nice for volumetric strokes to be able to exceed
   * the upper end of this range. Therefore, we don't actually clamp
   * down on the upper end.
   */
  if (pt->pressure < 0.0f) {
    pt->pressure = 0.0f;
  }

  return true;
}

/* ----------------------------------------------- */
/* Color Strength Brush */

/* Make color more or less transparent by the specified amounts */
static bool gp_brush_strength_apply(tGP_BrushEditData *gso,
                                    bGPDstroke *gps,
                                    float UNUSED(rot_eval),
                                    int pt_index,
                                    const int radius,
                                    const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float inf;

  /* Compute strength of effect */
  inf = gp_brush_influence_calc(gso, radius, co) * 0.125f;

  /* Invert effect. */
  if (gp_brush_invert_check(gso)) {
    inf *= -1.0f;
  }

  pt->strength = clamp_f(pt->strength + inf, 0.0f, 1.0f);

  return true;
}

/* ----------------------------------------------- */
/* Grab Brush */

/* Custom data per stroke for the Grab Brush
 *
 * This basically defines the strength of the effect for each
 * affected stroke point that was within the initial range of
 * the brush region.
 */
typedef struct tGPSB_Grab_StrokeData {
  /* array of indices to corresponding points in the stroke */
  int *points;
  /* array of influence weights for each of the included points */
  float *weights;
  /* angles to calc transformation */
  float *rot_eval;

  /* capacity of the arrays */
  int capacity;
  /* actual number of items currently stored */
  int size;
} tGPSB_Grab_StrokeData;

/* initialise custom data for handling this stroke */
static void gp_brush_grab_stroke_init(tGP_BrushEditData *gso, bGPDstroke *gps)
{
  tGPSB_Grab_StrokeData *data = NULL;

  BLI_assert(gps->totpoints > 0);

  /* Check if there are buffers already (from a prior run) */
  if (BLI_ghash_haskey(gso->stroke_customdata, gps)) {
    /* Ensure that the caches are empty
     * - Since we reuse these between different strokes, we don't
     *   want the previous invocation's data polluting the arrays
     */
    data = BLI_ghash_lookup(gso->stroke_customdata, gps);
    BLI_assert(data != NULL);

    data->size = 0; /* minimum requirement - so that we can repopulate again */

    memset(data->points, 0, sizeof(int) * data->capacity);
    memset(data->weights, 0, sizeof(float) * data->capacity);
    memset(data->rot_eval, 0, sizeof(float) * data->capacity);
  }
  else {
    /* Create new instance */
    data = MEM_callocN(sizeof(tGPSB_Grab_StrokeData), "GP Stroke Grab Data");

    data->capacity = gps->totpoints;
    data->size = 0;

    data->points = MEM_callocN(sizeof(int) * data->capacity, "GP Stroke Grab Indices");
    data->weights = MEM_callocN(sizeof(float) * data->capacity, "GP Stroke Grab Weights");
    data->rot_eval = MEM_callocN(sizeof(float) * data->capacity, "GP Stroke Grab Rotations");

    /* hook up to the cache */
    BLI_ghash_insert(gso->stroke_customdata, gps, data);
  }
}

/* store references to stroke points in the initial stage */
static bool gp_brush_grab_store_points(tGP_BrushEditData *gso,
                                       bGPDstroke *gps,
                                       float rot_eval,
                                       int pt_index,
                                       const int radius,
                                       const int co[2])
{
  tGPSB_Grab_StrokeData *data = BLI_ghash_lookup(gso->stroke_customdata, gps);
  float inf = gp_brush_influence_calc(gso, radius, co);

  BLI_assert(data != NULL);
  BLI_assert(data->size < data->capacity);

  /* insert this point into the set of affected points */
  data->points[data->size] = pt_index;
  data->weights[data->size] = inf;
  data->rot_eval[data->size] = rot_eval;
  data->size++;

  /* done */
  return true;
}

/* Compute effect vector for grab brush */
static void gp_brush_grab_calc_dvec(tGP_BrushEditData *gso)
{
  /* Convert mouse-movements to movement vector */
  RegionView3D *rv3d = gso->region->regiondata;
  float *rvec = gso->object->loc;
  float zfac = ED_view3d_calc_zfac(rv3d, rvec, NULL);

  float mval_f[2];

  /* convert from 2D screenspace to 3D... */
  mval_f[0] = (float)(gso->mval[0] - gso->mval_prev[0]);
  mval_f[1] = (float)(gso->mval[1] - gso->mval_prev[1]);

  /* apply evaluated data transformation */
  if (gso->rot_eval != 0.0f) {
    const float cval = cos(gso->rot_eval);
    const float sval = sin(gso->rot_eval);
    float r[2];
    r[0] = (mval_f[0] * cval) - (mval_f[1] * sval);
    r[1] = (mval_f[0] * sval) + (mval_f[1] * cval);
    copy_v2_v2(mval_f, r);
  }

  ED_view3d_win_to_delta(gso->region, mval_f, gso->dvec, zfac);
}

/* Apply grab transform to all relevant points of the affected strokes */
static void gp_brush_grab_apply_cached(tGP_BrushEditData *gso,
                                       bGPDstroke *gps,
                                       const float diff_mat[4][4])
{
  tGPSB_Grab_StrokeData *data = BLI_ghash_lookup(gso->stroke_customdata, gps);
  /* If a new frame is created, could be impossible find the stroke. */
  if (data == NULL) {
    return;
  }

  int i;
  float inverse_diff_mat[4][4];
  invert_m4_m4(inverse_diff_mat, diff_mat);

  /* Apply dvec to all of the stored points */
  for (i = 0; i < data->size; i++) {
    bGPDspoint *pt = &gps->points[data->points[i]];
    float delta[3] = {0.0f};

    /* get evaluated transformation */
    gso->rot_eval = data->rot_eval[i];
    gp_brush_grab_calc_dvec(gso);

    /* adjust the amount of displacement to apply */
    mul_v3_v3fl(delta, gso->dvec, data->weights[i]);

    float fpt[3];
    float save_pt[3];
    copy_v3_v3(save_pt, &pt->x);
    /* apply transformation */
    mul_v3_m4v3(fpt, diff_mat, &pt->x);
    /* apply */
    add_v3_v3v3(&pt->x, fpt, delta);
    /* undo transformation to the init parent position */
    mul_m4_v3(inverse_diff_mat, &pt->x);

    /* compute lock axis */
    gpsculpt_compute_lock_axis(gso, pt, save_pt);
  }
}

/* free customdata used for handling this stroke */
static void gp_brush_grab_stroke_free(void *ptr)
{
  tGPSB_Grab_StrokeData *data = (tGPSB_Grab_StrokeData *)ptr;

  /* free arrays */
  MEM_SAFE_FREE(data->points);
  MEM_SAFE_FREE(data->weights);
  MEM_SAFE_FREE(data->rot_eval);

  /* ... and this item itself, since it was also allocated */
  MEM_freeN(data);
}

/* ----------------------------------------------- */
/* Push Brush */
/* NOTE: Depends on gp_brush_grab_calc_dvec() */
static bool gp_brush_push_apply(tGP_BrushEditData *gso,
                                bGPDstroke *gps,
                                float UNUSED(rot_eval),
                                int pt_index,
                                const int radius,
                                const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  float inf = gp_brush_influence_calc(gso, radius, co);
  float delta[3] = {0.0f};

  /* adjust the amount of displacement to apply */
  mul_v3_v3fl(delta, gso->dvec, inf);

  /* apply */
  mul_mat3_m4_v3(gso->inv_mat, delta); /* only rotation component */
  add_v3_v3(&pt->x, delta);

  /* compute lock axis */
  gpsculpt_compute_lock_axis(gso, pt, save_pt);

  /* done */
  return true;
}

/* ----------------------------------------------- */
/* Pinch Brush */
/* Compute reference midpoint for the brush - this is what we'll be moving towards */
static void gp_brush_calc_midpoint(tGP_BrushEditData *gso)
{
  /* Convert mouse position to 3D space
   * See: gpencil_paint.c :: gp_stroke_convertcoords()
   */
  RegionView3D *rv3d = gso->region->regiondata;
  const float *rvec = gso->object->loc;
  float zfac = ED_view3d_calc_zfac(rv3d, rvec, NULL);

  float mval_f[2];
  copy_v2_v2(mval_f, gso->mval);
  float mval_prj[2];
  float dvec[3];

  if (ED_view3d_project_float_global(gso->region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
      V3D_PROJ_RET_OK) {
    sub_v2_v2v2(mval_f, mval_prj, mval_f);
    ED_view3d_win_to_delta(gso->region, mval_f, dvec, zfac);
    sub_v3_v3v3(gso->dvec, rvec, dvec);
  }
  else {
    zero_v3(gso->dvec);
  }
}

/* Shrink distance between midpoint and this point... */
static bool gp_brush_pinch_apply(tGP_BrushEditData *gso,
                                 bGPDstroke *gps,
                                 float UNUSED(rot_eval),
                                 int pt_index,
                                 const int radius,
                                 const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float fac, inf;
  float vec[3];
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  /* Scale down standard influence value to get it more manageable...
   * - No damping = Unmanageable at > 0.5 strength
   * - Div 10     = Not enough effect
   * - Div 5      = Happy medium... (by trial and error)
   */
  inf = gp_brush_influence_calc(gso, radius, co) / 5.0f;

  /* 1) Make this point relative to the cursor/midpoint (dvec) */
  float fpt[3];
  mul_v3_m4v3(fpt, gso->object->obmat, &pt->x);
  sub_v3_v3v3(vec, fpt, gso->dvec);

  /* 2) Shrink the distance by pulling the point towards the midpoint
   *    (0.0 = at midpoint, 1 = at edge of brush region)
   *                         OR
   *    Increase the distance (if inverting the brush action!)
   */
  if (gp_brush_invert_check(gso)) {
    /* Inflate (inverse) */
    fac = 1.0f + (inf * inf); /* squared to temper the effect... */
  }
  else {
    /* Shrink (default) */
    fac = 1.0f - (inf * inf); /* squared to temper the effect... */
  }
  mul_v3_fl(vec, fac);

  /* 3) Translate back to original space, with the shrinkage applied */
  add_v3_v3v3(fpt, gso->dvec, vec);
  mul_v3_m4v3(&pt->x, gso->object->imat, fpt);

  /* compute lock axis */
  gpsculpt_compute_lock_axis(gso, pt, save_pt);

  /* done */
  return true;
}

/* ----------------------------------------------- */
/* Twist Brush - Rotate Around midpoint */
/* Take the screenspace coordinates of the point, rotate this around the brush midpoint,
 * convert the rotated point and convert it into "data" space
 */

static bool gp_brush_twist_apply(tGP_BrushEditData *gso,
                                 bGPDstroke *gps,
                                 float UNUSED(rot_eval),
                                 int pt_index,
                                 const int radius,
                                 const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float angle, inf;
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  /* Angle to rotate by */
  inf = gp_brush_influence_calc(gso, radius, co);
  angle = DEG2RADF(1.0f) * inf;

  if (gp_brush_invert_check(gso)) {
    /* invert angle that we rotate by */
    angle *= -1;
  }

  /* Rotate in 2D or 3D space? */
  if (gps->flag & GP_STROKE_3DSPACE) {
    /* Perform rotation in 3D space... */
    RegionView3D *rv3d = gso->region->regiondata;
    float rmat[3][3];
    float axis[3];
    float vec[3];

    /* Compute rotation matrix - rotate around view vector by angle */
    negate_v3_v3(axis, rv3d->persinv[2]);
    normalize_v3(axis);

    axis_angle_normalized_to_mat3(rmat, axis, angle);

    /* Rotate point */
    float fpt[3];
    mul_v3_m4v3(fpt, gso->object->obmat, &pt->x);
    sub_v3_v3v3(vec, fpt, gso->dvec); /* make relative to center
                                       * (center is stored in dvec) */
    mul_m3_v3(rmat, vec);
    add_v3_v3v3(fpt, vec, gso->dvec); /* restore */
    mul_v3_m4v3(&pt->x, gso->object->imat, fpt);

    /* compute lock axis */
    gpsculpt_compute_lock_axis(gso, pt, save_pt);
  }
  else {
    const float axis[3] = {0.0f, 0.0f, 1.0f};
    float vec[3] = {0.0f};
    float rmat[3][3];

    /* Express position of point relative to cursor, ready to rotate */
    // XXX: There is still some offset here, but it's close to working as expected...
    vec[0] = (float)(co[0] - gso->mval[0]);
    vec[1] = (float)(co[1] - gso->mval[1]);

    /* rotate point */
    axis_angle_normalized_to_mat3(rmat, axis, angle);
    mul_m3_v3(rmat, vec);

    /* Convert back to screen-coordinates */
    vec[0] += (float)gso->mval[0];
    vec[1] += (float)gso->mval[1];

    /* Map from screen-coordinates to final coordinate space */
    if (gps->flag & GP_STROKE_2DSPACE) {
      View2D *v2d = gso->gsc.v2d;
      UI_view2d_region_to_view(v2d, vec[0], vec[1], &pt->x, &pt->y);
    }
    else {
      // XXX
      copy_v2_v2(&pt->x, vec);
    }
  }

  /* done */
  return true;
}

/* ----------------------------------------------- */
/* Randomize Brush */
/* Apply some random jitter to the point */
static bool gp_brush_randomize_apply(tGP_BrushEditData *gso,
                                     bGPDstroke *gps,
                                     float UNUSED(rot_eval),
                                     int pt_index,
                                     const int radius,
                                     const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  /* Amount of jitter to apply depends on the distance of the point to the cursor,
   * as well as the strength of the brush
   */
  const float inf = gp_brush_influence_calc(gso, radius, co) / 2.0f;
  const float fac = BLI_rng_get_float(gso->rng) * inf;

  /* apply random to position */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
    /* Jitter is applied perpendicular to the mouse movement vector
     * - We compute all effects in screenspace (since it's easier)
     *   and then project these to get the points/distances in
     *   view-space as needed.
     */
    float mvec[2], svec[2];

    /* mouse movement in ints -> floats */
    mvec[0] = (float)(gso->mval[0] - gso->mval_prev[0]);
    mvec[1] = (float)(gso->mval[1] - gso->mval_prev[1]);

    /* rotate mvec by 90 degrees... */
    svec[0] = -mvec[1];
    svec[1] = mvec[0];

    /* scale the displacement by the random displacement, and apply */
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      mul_v2_fl(svec, -fac);
    }
    else {
      mul_v2_fl(svec, fac);
    }

    /* convert to dataspace */
    if (gps->flag & GP_STROKE_3DSPACE) {
      /* 3D: Project to 3D space */
      bool flip;
      RegionView3D *rv3d = gso->region->regiondata;
      float zfac = ED_view3d_calc_zfac(rv3d, &pt->x, &flip);
      if (flip == false) {
        float dvec[3];
        ED_view3d_win_to_delta(gso->gsc.region, svec, dvec, zfac);
        add_v3_v3(&pt->x, dvec);
        /* compute lock axis */
        gpsculpt_compute_lock_axis(gso, pt, save_pt);
      }
    }
  }
  /* apply random to strength */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_STRENGTH) {
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      pt->strength += fac;
    }
    else {
      pt->strength -= fac;
    }
    CLAMP_MIN(pt->strength, 0.0f);
    CLAMP_MAX(pt->strength, 1.0f);
  }
  /* apply random to thickness (use pressure) */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_THICKNESS) {
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      pt->pressure += fac;
    }
    else {
      pt->pressure -= fac;
    }
    /* only limit lower value */
    CLAMP_MIN(pt->pressure, 0.0f);
  }
  /* apply random to UV (use pressure) */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_UV) {
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      pt->uv_rot += fac;
    }
    else {
      pt->uv_rot -= fac;
    }
    CLAMP(pt->uv_rot, -M_PI_2, M_PI_2);
  }

  /* done */
  return true;
}

/* ************************************************ */
/* Non Callback-Based Brushes */
/* Clone Brush ------------------------------------- */
/* How this brush currently works:
 * - If this is start of the brush stroke, paste immediately under the cursor
 *   by placing the midpoint of the buffer strokes under the cursor now
 *
 * - Otherwise, in:
 *   "Stamp Mode" - Move the newly pasted strokes so that their center follows the cursor
 *   "Continuous" - Repeatedly just paste new copies for where the brush is now
 */

/* Custom state data for clone brush */
typedef struct tGPSB_CloneBrushData {
  /* midpoint of the strokes on the clipboard */
  float buffer_midpoint[3];

  /* number of strokes in the paste buffer (and/or to be created each time) */
  size_t totitems;

  /* for "stamp" mode, the currently pasted brushes */
  bGPDstroke **new_strokes;

  /** Mapping from colors referenced per stroke, to the new colors in the "pasted" strokes. */
  GHash *new_colors;
} tGPSB_CloneBrushData;

/* Initialise "clone" brush data */
static void gp_brush_clone_init(bContext *C, tGP_BrushEditData *gso)
{
  tGPSB_CloneBrushData *data;
  bGPDstroke *gps;

  /* init custom data */
  gso->customdata = data = MEM_callocN(sizeof(tGPSB_CloneBrushData), "CloneBrushData");

  /* compute midpoint of strokes on clipboard */
  for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
    if (ED_gpencil_stroke_can_use(C, gps)) {
      const float dfac = 1.0f / ((float)gps->totpoints);
      float mid[3] = {0.0f};

      bGPDspoint *pt;
      int i;

      /* compute midpoint of this stroke */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        float co[3];

        mul_v3_v3fl(co, &pt->x, dfac);
        add_v3_v3(mid, co);
      }

      /* combine this stroke's data with the main data */
      add_v3_v3(data->buffer_midpoint, mid);
      data->totitems++;
    }
  }

  /* Divide the midpoint by the number of strokes, to finish averaging it */
  if (data->totitems > 1) {
    mul_v3_fl(data->buffer_midpoint, 1.0f / (float)data->totitems);
  }

  /* Create a buffer for storing the current strokes */
  if (1 /*gso->brush->mode == GP_EDITBRUSH_CLONE_MODE_STAMP*/) {
    data->new_strokes = MEM_callocN(sizeof(bGPDstroke *) * data->totitems,
                                    "cloned strokes ptr array");
  }

  /* Init colormap for mapping between the pasted stroke's source color (names)
   * and the final colors that will be used here instead.
   */
  data->new_colors = gp_copybuf_validate_colormap(C);
}

/* Free custom data used for "clone" brush */
static void gp_brush_clone_free(tGP_BrushEditData *gso)
{
  tGPSB_CloneBrushData *data = gso->customdata;

  /* free strokes array */
  if (data->new_strokes) {
    MEM_freeN(data->new_strokes);
    data->new_strokes = NULL;
  }

  /* free copybuf colormap */
  if (data->new_colors) {
    BLI_ghash_free(data->new_colors, NULL, NULL);
    data->new_colors = NULL;
  }

  /* free the customdata itself */
  MEM_freeN(data);
  gso->customdata = NULL;
}

/* Create new copies of the strokes on the clipboard */
static void gp_brush_clone_add(bContext *C, tGP_BrushEditData *gso)
{
  tGPSB_CloneBrushData *data = gso->customdata;

  Object *ob = gso->object;
  bGPdata *gpd = (bGPdata *)ob->data;
  Scene *scene = gso->scene;
  bGPDstroke *gps;

  float delta[3];
  size_t strokes_added = 0;

  /* Compute amount to offset the points by */
  /* NOTE: This assumes that screenspace strokes are NOT used in the 3D view... */

  gp_brush_calc_midpoint(gso); /* this puts the cursor location into gso->dvec */
  sub_v3_v3v3(delta, gso->dvec, data->buffer_midpoint);

  /* Copy each stroke into the layer */
  for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
    if (ED_gpencil_stroke_can_use(C, gps)) {
      bGPDstroke *new_stroke;
      bGPDspoint *pt;
      int i;

      bGPDlayer *gpl = NULL;
      /* Try to use original layer. */
      if (gps->runtime.tmp_layerinfo != NULL) {
        gpl = BKE_gpencil_layer_named_get(gpd, gps->runtime.tmp_layerinfo);
      }

      /* if not available, use active layer. */
      if (gpl == NULL) {
        gpl = CTX_data_active_gpencil_layer(C);
      }
      bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, CFRA, GP_GETFRAME_ADD_NEW);

      /* Make a new stroke */
      new_stroke = BKE_gpencil_stroke_duplicate(gps, true);

      new_stroke->next = new_stroke->prev = NULL;
      BLI_addtail(&gpf->strokes, new_stroke);

      /* Fix color references */
      Material *ma = BLI_ghash_lookup(data->new_colors, POINTER_FROM_INT(new_stroke->mat_nr));
      new_stroke->mat_nr = BKE_gpencil_object_material_index_get(ob, ma);
      if (!ma || new_stroke->mat_nr < 0) {
        new_stroke->mat_nr = 0;
      }
      /* Adjust all the stroke's points, so that the strokes
       * get pasted relative to where the cursor is now
       */
      for (i = 0, pt = new_stroke->points; i < new_stroke->totpoints; i++, pt++) {
        /* Rotate around center new position */
        mul_mat3_m4_v3(gso->object->obmat, &pt->x); /* only rotation component */

        /* assume that the delta can just be applied, and then everything works */
        add_v3_v3(&pt->x, delta);
        mul_m4_v3(gso->object->imat, &pt->x);
      }

      /* Store ref for later */
      if ((data->new_strokes) && (strokes_added < data->totitems)) {
        data->new_strokes[strokes_added] = new_stroke;
        strokes_added++;
      }
    }
  }
}

/* Move newly-added strokes around - "Stamp" mode of the Clone brush */
static void gp_brush_clone_adjust(tGP_BrushEditData *gso)
{
  tGPSB_CloneBrushData *data = gso->customdata;
  size_t snum;

  /* Compute the amount of movement to apply (overwrites dvec) */
  gso->rot_eval = 0.0f;
  gp_brush_grab_calc_dvec(gso);

  /* For each of the stored strokes, apply the offset to each point */
  /* NOTE: Again this assumes that in the 3D view,
   * we only have 3d space and not screenspace strokes... */
  for (snum = 0; snum < data->totitems; snum++) {
    bGPDstroke *gps = data->new_strokes[snum];
    bGPDspoint *pt;
    int i;

    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      /* "Smudge" Effect falloff */
      float delta[3] = {0.0f};
      int sco[2] = {0};
      float influence;

      /* compute influence on point */
      gp_point_to_xy(&gso->gsc, gps, pt, &sco[0], &sco[1]);
      influence = gp_brush_influence_calc(gso, gso->brush->size, sco);

      /* adjust the amount of displacement to apply */
      mul_v3_v3fl(delta, gso->dvec, influence);

      /* apply */
      add_v3_v3(&pt->x, delta);
    }
  }
}

/* Entrypoint for applying "clone" brush */
static bool gpsculpt_brush_apply_clone(bContext *C, tGP_BrushEditData *gso)
{
  /* Which "mode" are we operating in? */
  if (gso->first) {
    /* Create initial clones */
    gp_brush_clone_add(C, gso);
  }
  else {
    /* Stamp or Continuous Mode */
    if (1 /*gso->brush->mode == GP_EDITBRUSH_CLONE_MODE_STAMP*/) {
      /* Stamp - Proceed to translate the newly added strokes */
      gp_brush_clone_adjust(gso);
    }
    else {
      /* Continuous - Just keep pasting everytime we move */
      /* TODO: The spacing of repeat should be controlled using a
       * "stepsize" or similar property? */
      gp_brush_clone_add(C, gso);
    }
  }

  return true;
}

/* ************************************************ */
/* Header Info for GPencil Sculpt */

static void gpsculpt_brush_header_set(bContext *C, tGP_BrushEditData *gso)
{
  Brush *brush = gso->brush;
  char str[UI_MAX_DRAW_STR] = "";

  BLI_snprintf(str,
               sizeof(str),
               TIP_("GPencil Sculpt: %s Stroke  | LMB to paint | RMB/Escape to Exit"
                    " | Ctrl to Invert Action | Wheel Up/Down for Size "
                    " | Shift-Wheel Up/Down for Strength"),
               brush->id.name + 2);

  ED_workspace_status_text(C, str);
}

/* ************************************************ */
/* Grease Pencil Sculpting Operator */

/* Init/Exit ----------------------------------------------- */

static bool gpsculpt_brush_init(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  Object *ob = CTX_data_active_object(C);

  /* set the brush using the tool */
  tGP_BrushEditData *gso;

  /* setup operator data */
  gso = MEM_callocN(sizeof(tGP_BrushEditData), "tGP_BrushEditData");
  op->customdata = gso;

  gso->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  gso->bmain = CTX_data_main(C);
  /* store state */
  gso->settings = gpsculpt_get_settings(scene);

  /* Random generator, only init once. */
  uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
  rng_seed ^= POINTER_AS_UINT(gso);
  gso->rng = BLI_rng_new(rng_seed);

  gso->is_painting = false;
  gso->first = true;

  gso->gpd = ED_gpencil_data_get_active(C);
  gso->cfra = INT_MAX; /* NOTE: So that first stroke will get handled in init_stroke() */

  gso->scene = scene;
  gso->object = ob;
  if (ob) {
    invert_m4_m4(gso->inv_mat, ob->obmat);
    gso->vrgroup = ob->actdef - 1;
    if (!BLI_findlink(&ob->defbase, gso->vrgroup)) {
      gso->vrgroup = -1;
    }
    /* Check if some modifier can transform the stroke. */
    gso->is_transformed = BKE_gpencil_has_transform_modifiers(ob);
  }
  else {
    unit_m4(gso->inv_mat);
    gso->vrgroup = -1;
    gso->is_transformed = false;
  }

  gso->area = CTX_wm_area(C);
  gso->region = CTX_wm_region(C);

  Paint *paint = &ts->gp_sculptpaint->paint;
  gso->brush = paint->brush;
  BKE_curvemapping_initialize(gso->brush->curve);

  /* save mask */
  gso->mask = ts->gpencil_selectmode_sculpt;

  /* multiframe settings */
  gso->is_multiframe = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gso->gpd);
  gso->use_multiframe_falloff = (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;

  /* Init multi-edit falloff curve data before doing anything,
   * so we won't have to do it again later. */
  if (gso->is_multiframe) {
    BKE_curvemapping_initialize(ts->gp_sculpt.cur_falloff);
  }

  /* initialise custom data for brushes */
  char tool = gso->brush->gpencil_sculpt_tool;
  switch (tool) {
    case GPSCULPT_TOOL_CLONE: {
      bGPDstroke *gps;
      bool found = false;

      /* check that there are some usable strokes in the buffer */
      for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
        if (ED_gpencil_stroke_can_use(C, gps)) {
          found = true;
          break;
        }
      }

      if (found == false) {
        /* STOP HERE! Nothing to paste! */
        BKE_report(op->reports,
                   RPT_ERROR,
                   "Copy some strokes to the clipboard before using the Clone brush to paste "
                   "copies of them");

        MEM_freeN(gso);
        op->customdata = NULL;
        return false;
      }
      else {
        /* initialise customdata */
        gp_brush_clone_init(C, gso);
      }
      break;
    }

    case GPSCULPT_TOOL_GRAB: {
      /* initialise the cache needed for this brush */
      gso->stroke_customdata = BLI_ghash_ptr_new("GP Grab Brush - Strokes Hash");
      break;
    }

    /* Others - No customdata needed */
    default:
      break;
  }

  /* setup space conversions */
  gp_point_conversion_init(C, &gso->gsc);

  /* update header */
  gpsculpt_brush_header_set(C, gso);

  return true;
}

static void gpsculpt_brush_exit(bContext *C, wmOperator *op)
{
  tGP_BrushEditData *gso = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  char tool = gso->brush->gpencil_sculpt_tool;

  /* free brush-specific data */
  switch (tool) {
    case GPSCULPT_TOOL_GRAB: {
      /* Free per-stroke customdata
       * - Keys don't need to be freed, as those are the strokes
       * - Values assigned to those keys do, as they are custom structs
       */
      BLI_ghash_free(gso->stroke_customdata, NULL, gp_brush_grab_stroke_free);
      break;
    }

    case GPSCULPT_TOOL_CLONE: {
      /* Free customdata */
      gp_brush_clone_free(gso);
      break;
    }

    default:
      break;
  }

  /* unregister timer (only used for realtime) */
  if (gso->timer) {
    WM_event_remove_timer(CTX_wm_manager(C), win, gso->timer);
  }

  if (gso->rng != NULL) {
    BLI_rng_free(gso->rng);
  }

  /* Disable headerprints. */
  ED_workspace_status_text(C, NULL);

  /* disable temp invert flag */
  gso->brush->gpencil_settings->sculpt_flag &= ~GP_SCULPT_FLAG_TMP_INVERT;

  /* Update geometry data for tagged strokes. */
  gpencil_update_geometry(gso->gpd);

  /* free operator data */
  MEM_freeN(gso);
  op->customdata = NULL;
}

/* poll callback for stroke sculpting operator(s) */
static bool gpsculpt_brush_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype != SPACE_VIEW3D) {
    return false;
  }

  /* NOTE: this is a bit slower, but is the most accurate... */
  return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* Init Sculpt Stroke ---------------------------------- */

static void gpsculpt_brush_init_stroke(bContext *C, tGP_BrushEditData *gso)
{
  bGPdata *gpd = gso->gpd;

  Scene *scene = gso->scene;
  int cfra = CFRA;

  /* only try to add a new frame if this is the first stroke, or the frame has changed */
  if ((gpd == NULL) || (cfra == gso->cfra)) {
    return;
  }

  /* go through each layer, and ensure that we've got a valid frame to use */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf = gpl->actframe;

      /* Make a new frame to work on if the layer's frame
       * and the current scene frame don't match up:
       * - This is useful when animating as it saves that "uh-oh" moment when you realize you've
       *   spent too much time editing the wrong frame.
       */
      if (gpf->framenum != cfra) {
        BKE_gpencil_frame_addcopy(gpl, cfra);
        /* Need tag to recalculate evaluated data to avoid crashes. */
        DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
        WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
      }
    }
  }

  /* save off new current frame, so that next update works fine */
  gso->cfra = cfra;
}

/* Apply ----------------------------------------------- */

/* Get angle of the segment relative to the original segment before any transformation
 * For strokes with one point only this is impossible to calculate because there isn't a
 * valid reference point.
 */
static float gpsculpt_rotation_eval_get(tGP_BrushEditData *gso,
                                        bGPDstroke *gps_eval,
                                        bGPDspoint *pt_eval,
                                        int idx_eval)
{
  /* If multiframe or no modifiers, return 0. */
  if ((GPENCIL_MULTIEDIT_SESSIONS_ON(gso->gpd)) || (!gso->is_transformed)) {
    return 0.0f;
  }

  GP_SpaceConversion *gsc = &gso->gsc;
  bGPDstroke *gps_orig = (gps_eval->runtime.gps_orig) ? gps_eval->runtime.gps_orig : gps_eval;
  bGPDspoint *pt_orig = &gps_orig->points[pt_eval->runtime.idx_orig];
  bGPDspoint *pt_prev_eval = NULL;
  bGPDspoint *pt_orig_prev = NULL;
  if (idx_eval != 0) {
    pt_prev_eval = &gps_eval->points[idx_eval - 1];
  }
  else {
    if (gps_eval->totpoints > 1) {
      pt_prev_eval = &gps_eval->points[idx_eval + 1];
    }
    else {
      return 0.0f;
    }
  }

  if (pt_eval->runtime.idx_orig != 0) {
    pt_orig_prev = &gps_orig->points[pt_eval->runtime.idx_orig - 1];
  }
  else {
    if (gps_orig->totpoints > 1) {
      pt_orig_prev = &gps_orig->points[pt_eval->runtime.idx_orig + 1];
    }
    else {
      return 0.0f;
    }
  }

  /* create 2D vectors of the stroke segments */
  float v_orig_a[2], v_orig_b[2], v_eval_a[2], v_eval_b[2];

  gp_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_orig->x, v_orig_a);
  gp_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_orig_prev->x, v_orig_b);
  sub_v2_v2(v_orig_a, v_orig_b);

  gp_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_eval->x, v_eval_a);
  gp_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_prev_eval->x, v_eval_b);
  sub_v2_v2(v_eval_a, v_eval_b);

  return angle_v2v2(v_orig_a, v_eval_a);
}

/* Apply brush operation to points in this stroke */
static bool gpsculpt_brush_do_stroke(tGP_BrushEditData *gso,
                                     bGPDstroke *gps,
                                     const float diff_mat[4][4],
                                     GP_BrushApplyCb apply)
{
  GP_SpaceConversion *gsc = &gso->gsc;
  rcti *rect = &gso->brush_rect;
  Brush *brush = gso->brush;
  char tool = gso->brush->gpencil_sculpt_tool;
  const int radius = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;

  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = NULL;

  bGPDspoint *pt1, *pt2;
  bGPDspoint *pt = NULL;
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int index;
  bool include_last = false;
  bool changed = false;
  float rot_eval = 0.0f;

  /* Check if the stroke collide with brush. */
  if (!ED_gpencil_stroke_check_collision(gsc, gps, gso->mval, radius, diff_mat)) {
    return false;
  }

  if (gps->totpoints == 1) {
    bGPDspoint pt_temp;
    pt = &gps->points[0];
    gp_point_to_parent_space(gps->points, diff_mat, &pt_temp);
    gp_point_to_xy(gsc, gps, &pt_temp, &pc1[0], &pc1[1]);

    pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
    /* do boundbox check first */
    if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
      /* only check if point is inside */
      int mval_i[2];
      round_v2i_v2fl(mval_i, gso->mval);
      if (len_v2v2_int(mval_i, pc1) <= radius) {
        /* apply operation to this point */
        if (pt_active != NULL) {
          rot_eval = gpsculpt_rotation_eval_get(gso, gps, pt, 0);
          changed = apply(gso, gps_active, rot_eval, 0, radius, pc1);
        }
      }
    }
  }
  else {
    /* Loop over the points in the stroke, checking for intersections
     * - an intersection means that we touched the stroke
     */
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* Get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

      /* Skip if neither one is selected
       * (and we are only allowed to edit/consider selected points) */
      if (GPENCIL_ANY_SCULPT_MASK(gso->mask)) {
        if (!(pt1->flag & GP_SPOINT_SELECT) && !(pt2->flag & GP_SPOINT_SELECT)) {
          include_last = false;
          continue;
        }
      }
      bGPDspoint npt;
      gp_point_to_parent_space(pt1, diff_mat, &npt);
      gp_point_to_xy(gsc, gps, &npt, &pc1[0], &pc1[1]);

      gp_point_to_parent_space(pt2, diff_mat, &npt);
      gp_point_to_xy(gsc, gps, &npt, &pc2[0], &pc2[1]);

      /* Check that point segment of the boundbox of the selection stroke */
      if (((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          ((!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1])) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1]))) {
        /* Check if point segment of stroke had anything to do with
         * brush region  (either within stroke painted, or on its lines)
         * - this assumes that linewidth is irrelevant
         */
        if (gp_stroke_inside_circle(gso->mval, radius, pc1[0], pc1[1], pc2[0], pc2[1])) {
          /* Apply operation to these points */
          bool ok = false;

          /* To each point individually... */
          pt = &gps->points[i];
          if ((pt->runtime.pt_orig == NULL) && (tool != GPSCULPT_TOOL_GRAB)) {
            continue;
          }
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          /* If masked and the point is not selected, skip it. */
          if ((GPENCIL_ANY_SCULPT_MASK(gso->mask)) &&
              ((pt_active->flag & GP_SPOINT_SELECT) == 0)) {
            continue;
          }
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if ((pt_active != NULL) && (index < gps_active->totpoints)) {
            rot_eval = gpsculpt_rotation_eval_get(gso, gps, pt, i);
            ok = apply(gso, gps_active, rot_eval, index, radius, pc1);
          }

          /* Only do the second point if this is the last segment,
           * and it is unlikely that the point will get handled
           * otherwise.
           *
           * NOTE: There is a small risk here that the second point wasn't really
           *       actually in-range. In that case, it only got in because
           *       the line linking the points was!
           */
          if (i + 1 == gps->totpoints - 1) {
            pt = &gps->points[i + 1];
            pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i + 1;
            if ((pt_active != NULL) && (index < gps_active->totpoints)) {
              rot_eval = gpsculpt_rotation_eval_get(gso, gps, pt, i + 1);
              ok |= apply(gso, gps_active, rot_eval, index, radius, pc2);
              include_last = false;
            }
          }
          else {
            include_last = true;
          }

          changed |= ok;
        }
        else if (include_last) {
          /* This case is for cases where for whatever reason the second vert (1st here)
           * doesn't get included because the whole edge isn't in bounds,
           * but it would've qualified since it did with the previous step
           * (but wasn't added then, to avoid double-ups).
           */
          pt = &gps->points[i];
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if ((pt_active != NULL) && (index < gps_active->totpoints)) {
            rot_eval = gpsculpt_rotation_eval_get(gso, gps, pt, i);
            changed |= apply(gso, gps_active, rot_eval, index, radius, pc1);
            include_last = false;
          }
        }
      }
    }
  }

  return changed;
}

/* Apply sculpt brushes to strokes in the given frame */
static bool gpsculpt_brush_do_frame(bContext *C,
                                    tGP_BrushEditData *gso,
                                    bGPDlayer *gpl,
                                    bGPDframe *gpf,
                                    const float diff_mat[4][4])
{
  bool changed = false;
  bool redo_geom = false;
  Object *ob = gso->object;
  char tool = gso->brush->gpencil_sculpt_tool;

  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    /* skip strokes that are invalid for current view */
    if (ED_gpencil_stroke_can_use(C, gps) == false) {
      continue;
    }
    /* check if the color is editable */
    if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
      continue;
    }

    switch (tool) {
      case GPSCULPT_TOOL_SMOOTH: /* Smooth strokes */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_smooth_apply);
        redo_geom |= changed;
        break;
      }

      case GPSCULPT_TOOL_THICKNESS: /* Adjust stroke thickness */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_thickness_apply);
        break;
      }

      case GPSCULPT_TOOL_STRENGTH: /* Adjust stroke color strength */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_strength_apply);
        break;
      }

      case GPSCULPT_TOOL_GRAB: /* Grab points */
      {
        bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
        if (gps_active != NULL) {
          if (gso->first) {
            /* First time this brush stroke is being applied:
             * 1) Prepare data buffers (init/clear) for this stroke
             * 2) Use the points now under the cursor
             */
            gp_brush_grab_stroke_init(gso, gps_active);
            changed |= gpsculpt_brush_do_stroke(
                gso, gps_active, diff_mat, gp_brush_grab_store_points);
          }
          else {
            /* Apply effect to the stored points */
            gp_brush_grab_apply_cached(gso, gps_active, diff_mat);
            changed |= true;
          }
        }
        redo_geom |= changed;
        break;
      }

      case GPSCULPT_TOOL_PUSH: /* Push points */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_push_apply);
        redo_geom |= changed;
        break;
      }

      case GPSCULPT_TOOL_PINCH: /* Pinch points */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_pinch_apply);
        redo_geom |= changed;
        break;
      }

      case GPSCULPT_TOOL_TWIST: /* Twist points around midpoint */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_twist_apply);
        redo_geom |= changed;
        break;
      }

      case GPSCULPT_TOOL_RANDOMIZE: /* Apply jitter */
      {
        changed |= gpsculpt_brush_do_stroke(gso, gps, diff_mat, gp_brush_randomize_apply);
        redo_geom |= changed;
        break;
      }

      default:
        printf("ERROR: Unknown type of GPencil Sculpt brush \n");
        break;
    }

    /* Triangulation must be calculated. */
    if (redo_geom) {
      bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
      if (gpl->actframe == gpf) {
        MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
        /* Update active frame now, only if material has fill. */
        if (gp_style->flag & GP_MATERIAL_FILL_SHOW) {
          BKE_gpencil_stroke_geometry_update(gps_active);
        }
        else {
          gpencil_recalc_geometry_tag(gps_active);
        }
      }
      else {
        /* Delay a full recalculation for other frames. */
        gpencil_recalc_geometry_tag(gps_active);
      }
    }
  }

  return changed;
}

/* Perform two-pass brushes which modify the existing strokes */
static bool gpsculpt_brush_apply_standard(bContext *C, tGP_BrushEditData *gso)
{
  ToolSettings *ts = gso->scene->toolsettings;
  Depsgraph *depsgraph = gso->depsgraph;
  Object *obact = gso->object;
  bool changed = false;

  Object *ob_eval = (Object *)DEG_get_evaluated_id(depsgraph, &obact->id);
  bGPdata *gpd = (bGPdata *)ob_eval->data;

  /* Calculate brush-specific data which applies equally to all points */
  char tool = gso->brush->gpencil_sculpt_tool;
  switch (tool) {
    case GPSCULPT_TOOL_GRAB: /* Grab points */
    case GPSCULPT_TOOL_PUSH: /* Push points */
    {
      /* calculate amount of displacement to apply */
      gso->rot_eval = 0.0f;
      gp_brush_grab_calc_dvec(gso);
      break;
    }

    case GPSCULPT_TOOL_PINCH: /* Pinch points */
    case GPSCULPT_TOOL_TWIST: /* Twist points around midpoint */
    {
      /* calculate midpoint of the brush (in data space) */
      gp_brush_calc_midpoint(gso);
      break;
    }

    case GPSCULPT_TOOL_RANDOMIZE: /* Random jitter */
    {
      /* compute the displacement vector for the cursor (in data space) */
      gso->rot_eval = 0.0f;
      gp_brush_grab_calc_dvec(gso);
      break;
    }

    default:
      break;
  }

  /* Find visible strokes, and perform operations on those if hit */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* If no active frame, don't do anything... */
    if ((!BKE_gpencil_layer_is_editable(gpl)) || (gpl->actframe == NULL)) {
      continue;
    }

    /* calculate difference matrix */
    float diff_mat[4][4];
    BKE_gpencil_parent_matrix_get(depsgraph, obact, gpl, diff_mat);

    /* Active Frame or MultiFrame? */
    if (gso->is_multiframe) {
      /* init multiframe falloff options */
      int f_init = 0;
      int f_end = 0;

      if (gso->use_multiframe_falloff) {
        BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
      }

      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        /* Always do active frame; Otherwise, only include selected frames */
        if ((gpf == gpl->actframe) || (gpf->flag & GP_FRAME_SELECT)) {
          /* compute multiframe falloff factor */
          if (gso->use_multiframe_falloff) {
            /* Falloff depends on distance to active frame
             * (relative to the overall frame range). */
            gso->mf_falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }
          else {
            /* No falloff */
            gso->mf_falloff = 1.0f;
          }

          /* affect strokes in this frame */
          changed |= gpsculpt_brush_do_frame(C, gso, gpl, gpf, diff_mat);
        }
      }
    }
    else {
      if (gpl->actframe != NULL) {
        /* Apply to active frame's strokes */
        gso->mf_falloff = 1.0f;
        changed |= gpsculpt_brush_do_frame(C, gso, gpl, gpl->actframe, diff_mat);
      }
    }
  }

  return changed;
}

/* Calculate settings for applying brush */
static void gpsculpt_brush_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
  tGP_BrushEditData *gso = op->customdata;
  Brush *brush = gso->brush;
  const int radius = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  float mousef[2];
  int mouse[2];
  bool changed = false;

  /* Get latest mouse coordinates */
  RNA_float_get_array(itemptr, "mouse", mousef);
  gso->mval[0] = mouse[0] = (int)(mousef[0]);
  gso->mval[1] = mouse[1] = (int)(mousef[1]);

  gso->pressure = RNA_float_get(itemptr, "pressure");

  if (RNA_boolean_get(itemptr, "pen_flip")) {
    gso->flag |= GP_SCULPT_FLAG_INVERT;
  }
  else {
    gso->flag &= ~GP_SCULPT_FLAG_INVERT;
  }

  /* Store coordinates as reference, if operator just started running */
  if (gso->first) {
    gso->mval_prev[0] = gso->mval[0];
    gso->mval_prev[1] = gso->mval[1];
    gso->pressure_prev = gso->pressure;
  }

  /* Update brush_rect, so that it represents the bounding rectangle of brush */
  gso->brush_rect.xmin = mouse[0] - radius;
  gso->brush_rect.ymin = mouse[1] - radius;
  gso->brush_rect.xmax = mouse[0] + radius;
  gso->brush_rect.ymax = mouse[1] + radius;

  /* Apply brush */
  char tool = gso->brush->gpencil_sculpt_tool;
  if (tool == GPSCULPT_TOOL_CLONE) {
    changed = gpsculpt_brush_apply_clone(C, gso);
  }
  else {
    changed = gpsculpt_brush_apply_standard(C, gso);
  }

  /* Updates */
  if (changed) {
    DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
  }

  /* Store values for next step */
  gso->mval_prev[0] = gso->mval[0];
  gso->mval_prev[1] = gso->mval[1];
  gso->pressure_prev = gso->pressure;
  gso->first = false;
}

/* Running --------------------------------------------- */
static Brush *gpsculpt_get_smooth_brush(tGP_BrushEditData *gso)
{
  Main *bmain = gso->bmain;
  Brush *brush = BLI_findstring(&bmain->brushes, "Smooth Stroke", offsetof(ID, name) + 2);

  return brush;
}

/* helper - a record stroke, and apply paint event */
static void gpsculpt_brush_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushEditData *gso = op->customdata;
  PointerRNA itemptr;
  float mouse[2];

  mouse[0] = event->mval[0] + 1;
  mouse[1] = event->mval[1] + 1;

  /* fill in stroke */
  RNA_collection_add(op->ptr, "stroke", &itemptr);

  RNA_float_set_array(&itemptr, "mouse", mouse);
  RNA_boolean_set(&itemptr, "pen_flip", event->ctrl != false);
  RNA_boolean_set(&itemptr, "is_start", gso->first);

  /* handle pressure sensitivity (which is supplied by tablets and otherwise 1.0) */
  float pressure = event->tablet.pressure;
  /* special exception here for too high pressure values on first touch in
   * windows for some tablets: clamp the values to be sane */
  if (pressure >= 0.99f) {
    pressure = 1.0f;
  }
  RNA_float_set(&itemptr, "pressure", pressure);

  if (event->shift) {
    gso->brush_prev = gso->brush;

    gso->brush = gpsculpt_get_smooth_brush(gso);
    if (gso->brush == NULL) {
      gso->brush = gso->brush_prev;
    }
  }
  else {
    if (gso->brush_prev != NULL) {
      gso->brush = gso->brush_prev;
    }
  }

  /* apply */
  gpsculpt_brush_apply(C, op, &itemptr);
}

/* reapply */
static int gpsculpt_brush_exec(bContext *C, wmOperator *op)
{
  if (!gpsculpt_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    gpsculpt_brush_apply(C, op, &itemptr);
  }
  RNA_END;

  gpsculpt_brush_exit(C, op);

  return OPERATOR_FINISHED;
}

/* start modal painting */
static int gpsculpt_brush_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushEditData *gso = NULL;
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  const bool is_playing = ED_screen_animation_playing(CTX_wm_manager(C)) != NULL;
  bool needs_timer = false;
  float brush_rate = 0.0f;

  /* the operator cannot work while play animation */
  if (is_playing) {
    BKE_report(op->reports, RPT_ERROR, "Cannot sculpt while play animation");

    return OPERATOR_CANCELLED;
  }

  /* init painting data */
  if (!gpsculpt_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  gso = op->customdata;

  /* initialise type-specific data (used for the entire session) */
  char tool = gso->brush->gpencil_sculpt_tool;
  switch (tool) {
    /* Brushes requiring timer... */
    case GPSCULPT_TOOL_THICKNESS:
      brush_rate = 0.01f;
      needs_timer = true;
      break;

    case GPSCULPT_TOOL_STRENGTH:
      brush_rate = 0.01f;
      needs_timer = true;
      break;

    case GPSCULPT_TOOL_PINCH:
      brush_rate = 0.001f;
      needs_timer = true;
      break;

    case GPSCULPT_TOOL_TWIST:
      brush_rate = 0.01f;
      needs_timer = true;
      break;

    default:
      break;
  }

  /* register timer for increasing influence by hovering over an area */
  if (needs_timer) {
    gso->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, brush_rate);
  }

  /* register modal handler */
  WM_event_add_modal_handler(C, op);

  /* start drawing immediately? */
  if (is_modal == false) {
    ARegion *region = CTX_wm_region(C);

    /* ensure that we'll have a new frame to draw on */
    gpsculpt_brush_init_stroke(C, gso);

    /* apply first dab... */
    gso->is_painting = true;
    gpsculpt_brush_apply_event(C, op, event);

    /* redraw view with feedback */
    ED_region_tag_redraw(region);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* painting - handle events */
static int gpsculpt_brush_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushEditData *gso = op->customdata;
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  bool redraw_region = false;
  bool redraw_toolsettings = false;

  /* The operator can be in 2 states: Painting and Idling */
  if (gso->is_painting) {
    /* Painting  */
    switch (event->type) {
      /* Mouse Move = Apply somewhere else */
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE:
        /* apply brush effect at new position */
        gpsculpt_brush_apply_event(C, op, event);

        /* force redraw, so that the cursor will at least be valid */
        redraw_region = true;
        break;

      /* Timer Tick - Only if this was our own timer */
      case TIMER:
        if (event->customdata == gso->timer) {
          gso->timerTick = true;
          gpsculpt_brush_apply_event(C, op, event);
          gso->timerTick = false;
        }
        break;

      /* Painting mbut release = Stop painting (back to idle) */
      case LEFTMOUSE:
        // BLI_assert(event->val == KM_RELEASE);
        if (is_modal) {
          /* go back to idling... */
          gso->is_painting = false;
        }
        else {
          /* end sculpt session, since we're not modal */
          gso->is_painting = false;

          gpsculpt_brush_exit(C, op);
          return OPERATOR_FINISHED;
        }
        break;

      /* Abort painting if any of the usual things are tried */
      case MIDDLEMOUSE:
      case RIGHTMOUSE:
      case EVT_ESCKEY:
        gpsculpt_brush_exit(C, op);
        return OPERATOR_FINISHED;
    }
  }
  else {
    /* Idling */
    BLI_assert(is_modal == true);

    switch (event->type) {
      /* Painting mbut press = Start painting (switch to painting state) */
      case LEFTMOUSE:
        /* do initial "click" apply */
        gso->is_painting = true;
        gso->first = true;

        gpsculpt_brush_init_stroke(C, gso);
        gpsculpt_brush_apply_event(C, op, event);
        break;

      /* Exit modal operator, based on the "standard" ops */
      case RIGHTMOUSE:
      case EVT_ESCKEY:
        gpsculpt_brush_exit(C, op);
        return OPERATOR_FINISHED;

      /* MMB is often used for view manipulations */
      case MIDDLEMOUSE:
        return OPERATOR_PASS_THROUGH;

      /* Mouse movements should update the brush cursor - Just redraw the active region */
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE:
        redraw_region = true;
        break;

        /* Change Frame - Allowed */
      case EVT_LEFTARROWKEY:
      case EVT_RIGHTARROWKEY:
      case EVT_UPARROWKEY:
      case EVT_DOWNARROWKEY:
        return OPERATOR_PASS_THROUGH;

      /* Camera/View Gizmo's - Allowed */
      /* (See rationale in gpencil_paint.c -> gpencil_draw_modal()) */
      case EVT_PAD0:
      case EVT_PAD1:
      case EVT_PAD2:
      case EVT_PAD3:
      case EVT_PAD4:
      case EVT_PAD5:
      case EVT_PAD6:
      case EVT_PAD7:
      case EVT_PAD8:
      case EVT_PAD9:
        return OPERATOR_PASS_THROUGH;

      /* Unhandled event */
      default:
        break;
    }
  }

  /* Redraw region? */
  if (redraw_region) {
    ARegion *region = CTX_wm_region(C);
    ED_region_tag_redraw(region);
  }

  /* Redraw toolsettings (brush settings)? */
  if (redraw_toolsettings) {
    DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Also used for weight paint. */
void GPENCIL_OT_sculpt_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stroke Sculpt";
  ot->idname = "GPENCIL_OT_sculpt_paint";
  ot->description = "Apply tweaks to strokes by painting over the strokes";  // XXX

  /* api callbacks */
  ot->exec = gpsculpt_brush_exec;
  ot->invoke = gpsculpt_brush_invoke;
  ot->modal = gpsculpt_brush_modal;
  ot->cancel = gpsculpt_brush_exit;
  ot->poll = gpsculpt_brush_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "wait_for_input",
      true,
      "Wait for Input",
      "Enter a mini 'sculpt-mode' if enabled, otherwise, exit after drawing a single stroke");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

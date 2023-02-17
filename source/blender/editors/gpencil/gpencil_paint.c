/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. */

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
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_curve.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_update_cache.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"

#include "UI_view2d.h"

#include "ED_clip.h"
#include "ED_gpencil.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ******************************************* */
/* 'Globals' and Defines */

/* values for tGPsdata->status */
typedef enum eGPencil_PaintStatus {
  GP_STATUS_IDLING = 0, /* stroke isn't in progress yet */
  GP_STATUS_PAINTING,   /* a stroke is in progress */
  GP_STATUS_ERROR,      /* something wasn't correctly set up */
  GP_STATUS_DONE,       /* painting done */
} eGPencil_PaintStatus;

/* Return flags for adding points to stroke buffer */
typedef enum eGP_StrokeAdd_Result {
  GP_STROKEADD_INVALID = -2,  /* error occurred - insufficient info to do so */
  GP_STROKEADD_OVERFLOW = -1, /* error occurred - cannot fit any more points */
  GP_STROKEADD_NORMAL,        /* point was successfully added */
  GP_STROKEADD_FULL,          /* cannot add any more points to buffer */
} eGP_StrokeAdd_Result;

/* Runtime flags */
typedef enum eGPencil_PaintFlags {
  GP_PAINTFLAG_FIRSTRUN = (1 << 0), /* operator just started */
  GP_PAINTFLAG_SELECTMASK = (1 << 3),
  GP_PAINTFLAG_HARD_ERASER = (1 << 4),
  GP_PAINTFLAG_STROKE_ERASER = (1 << 5),
  GP_PAINTFLAG_REQ_VECTOR = (1 << 6),
} eGPencil_PaintFlags;

/* Temporary Guide data */
typedef struct tGPguide {
  /** guide spacing */
  float spacing;
  /** half guide spacing */
  float half_spacing;
  /** origin */
  float origin[2];
  /** rotated point */
  float rot_point[2];
  /** rotated point */
  float rot_angle;
  /** initial stroke direction */
  float stroke_angle;
  /** initial origin direction */
  float origin_angle;
  /** initial origin distance */
  float origin_distance;
  /** initial line for guides */
  float unit[2];
} tGPguide;

/* Temporary 'Stroke' Operation data
 *   "p" = op->customdata
 */
typedef struct tGPsdata {
  bContext *C;

  /** main database pointer. */
  Main *bmain;
  /** current scene from context. */
  Scene *scene;
  struct Depsgraph *depsgraph;

  /** Current object. */
  Object *ob;
  /** Evaluated object. */
  Object *ob_eval;
  /** window where painting originated. */
  wmWindow *win;
  /** area where painting originated. */
  ScrArea *area;
  /** region where painting originated. */
  ARegion *region;
  /** needed for GP_STROKE_2DSPACE. */
  View2D *v2d;
  /** For operations that require occlusion testing. */
  ViewDepths *depths;
  /** for using the camera rect within the 3d view. */
  rctf *subrect;
  rctf subrect_data;

  /** settings to pass to gp_points_to_xy(). */
  GP_SpaceConversion gsc;

  /** pointer to owner of gp-datablock. */
  PointerRNA ownerPtr;
  /** gp-datablock layer comes from. */
  bGPdata *gpd;
  /** layer we're working on. */
  bGPDlayer *gpl;
  /** frame we're working on. */
  bGPDframe *gpf;

  /** projection-mode flags (toolsettings - eGPencil_Placement_Flags) */
  char *align_flag;

  /** current status of painting. */
  eGPencil_PaintStatus status;
  /** mode for painting. */
  eGPencil_PaintModes paintmode;
  /** flags that can get set during runtime (eGPencil_PaintFlags) */
  eGPencil_PaintFlags flags;

  /** radius of influence for eraser. */
  short radius;

  /** current mouse-position. */
  float mval[2];
  /** previous recorded mouse-position. */
  float mvalo[2];
  /** initial recorded mouse-position */
  float mvali[2];

  /** current stylus pressure. */
  float pressure;
  /** previous stylus pressure. */
  float opressure;

  /* These need to be doubles, as (at least under unix) they are in seconds since epoch,
   * float (and its 7 digits precision) is definitively not enough here!
   * double, with its 15 digits precision,
   * ensures us millisecond precision for a few centuries at least.
   */
  /** Used when converting to path. */
  double inittime;
  /** Used when converting to path. */
  double curtime;
  /** Used when converting to path. */
  double ocurtime;

  /** Inverted transformation matrix applying when converting coords from screen-space
   * to region space. */
  float imat[4][4];
  float mat[4][4];

  float diff_mat[4][4];

  /** custom color - hack for enforcing a particular color for track/mask editing. */
  float custom_color[4];

  /** radial cursor data for drawing eraser. */
  void *erasercursor;

  /* mat settings are only used for 3D view */
  /** current material */
  Material *material;
  /** current drawing brush */
  Brush *brush;
  /** default eraser brush */
  Brush *eraser;

  /** 1: line horizontal, 2: line vertical, other: not defined */
  short straight;
  /** lock drawing to one axis */
  int lock_axis;
  /** the stroke is no fill mode */
  bool disable_fill;

  RNG *rng;

  /** key used for invoking the operator */
  short keymodifier;
  /** shift modifier flag */
  bool shift;
  /** size in pixels for uv calculation */
  float totpixlen;
  /** Special mode for fill brush. */
  bool disable_stabilizer;
  /* guide */
  tGPguide guide;

  ReportList *reports;

  /** Random settings by stroke */
  GpRandomSettings random_settings;

} tGPsdata;

/* ------ */

#define STROKE_HORIZONTAL 1
#define STROKE_VERTICAL 2

/* Macros for accessing sensitivity thresholds... */
/* minimum number of pixels mouse should move before new point created */
#define MIN_MANHATTEN_PX (U.gp_manhattandist)
/* minimum length of new segment before new point can be added */
#define MIN_EUCLIDEAN_PX (U.gp_euclideandist)

static void gpencil_update_cache(bGPdata *gpd)
{
  if (gpd) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  }
}

/* ------ */
/* Forward defines for some functions... */

static void gpencil_session_validatebuffer(tGPsdata *p);

/* ******************************************* */
/* Context Wrangling... */

/* check if context is suitable for drawing */
static bool gpencil_draw_poll(bContext *C)
{
  if (ED_operator_regionactive(C)) {
    ScrArea *area = CTX_wm_area(C);
    /* 3D Viewport */
    if (area->spacetype != SPACE_VIEW3D) {
      return false;
    }

    /* check if Grease Pencil isn't already running */
    if (ED_gpencil_session_active() != 0) {
      CTX_wm_operator_poll_msg_set(C, "Grease Pencil operator is already active");
      return false;
    }

    /* only grease pencil object type */
    Object *ob = CTX_data_active_object(C);
    if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
      return false;
    }

    bGPdata *gpd = (bGPdata *)ob->data;
    if (!GPENCIL_PAINT_MODE(gpd)) {
      return false;
    }

    ToolSettings *ts = CTX_data_scene(C)->toolsettings;
    if (!ts->gp_paint->paint.brush) {
      CTX_wm_operator_poll_msg_set(C, "Grease Pencil has no active paint tool");
      return false;
    }

    return true;
  }

  CTX_wm_operator_poll_msg_set(C, "Active region not set");
  return false;
}

/* check if projecting strokes into 3d-geometry in the 3D-View */
static bool gpencil_project_check(tGPsdata *p)
{
  bGPdata *gpd = p->gpd;
  return ((gpd->runtime.sbuffer_sflag & GP_STROKE_3DSPACE) &&
          (*p->align_flag & (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE)));
}

/* ******************************************* */
/* Calculations/Conversions */

/* Utilities --------------------------------- */

/* get the reference point for stroke-point conversions */
static void gpencil_get_3d_reference(tGPsdata *p, float vec[3])
{
  Object *ob = NULL;
  if (p->ownerPtr.type == &RNA_Object) {
    ob = (Object *)p->ownerPtr.data;
  }
  ED_gpencil_drawing_reference_get(p->scene, ob, *p->align_flag, vec);
}

/* Stroke Editing ---------------------------- */
/* check if the current mouse position is suitable for adding a new point */
static bool gpencil_stroke_filtermval(tGPsdata *p, const float mval[2], const float mvalo[2])
{
  Brush *brush = p->brush;
  int dx = (int)fabsf(mval[0] - mvalo[0]);
  int dy = (int)fabsf(mval[1] - mvalo[1]);
  brush->gpencil_settings->flag &= ~GP_BRUSH_STABILIZE_MOUSE_TEMP;

  /* if buffer is empty, just let this go through (i.e. so that dots will work) */
  if (p->gpd->runtime.sbuffer_used == 0) {
    return true;
  }
  /* if lazy mouse, check minimum distance */
  if (GPENCIL_LAZY_MODE(brush, p->shift) && (!p->disable_stabilizer)) {
    brush->gpencil_settings->flag |= GP_BRUSH_STABILIZE_MOUSE_TEMP;
    if ((dx * dx + dy * dy) > (brush->smooth_stroke_radius * brush->smooth_stroke_radius)) {
      return true;
    }

    /* If the mouse is moving within the radius of the last move,
     * don't update the mouse position. This allows sharp turns. */
    copy_v2_v2(p->mval, p->mvalo);
    return false;
  }
  /* check if mouse moved at least certain distance on both axes (best case)
   * - aims to eliminate some jitter-noise from input when trying to draw straight lines freehand
   */
  if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX)) {
    return true;
  }
  /* Check if the distance since the last point is significant enough:
   * - Prevents points being added too densely
   * - Distance here doesn't use sqrt to prevent slowness.
   *   We should still be safe from overflows though.
   */
  if ((dx * dx + dy * dy) > MIN_EUCLIDEAN_PX * MIN_EUCLIDEAN_PX) {
    return true;
  }
  /* mouse 'didn't move' */
  return false;
}

/* reproject stroke to plane locked to axis in 3d cursor location */
static void gpencil_reproject_toplane(tGPsdata *p, bGPDstroke *gps)
{
  bGPdata *gpd = p->gpd;
  Object *obact = (Object *)p->ownerPtr.data;

  float origin[3];
  RegionView3D *rv3d = p->region->regiondata;

  /* verify the stroke mode is CURSOR 3d space mode */
  if ((gpd->runtime.sbuffer_sflag & GP_STROKE_3DSPACE) == 0) {
    return;
  }
  if ((*p->align_flag & GP_PROJECT_VIEWSPACE) == 0) {
    return;
  }
  if ((*p->align_flag & GP_PROJECT_DEPTH_VIEW) || (*p->align_flag & GP_PROJECT_DEPTH_STROKE)) {
    return;
  }

  /* get drawing origin */
  gpencil_get_3d_reference(p, origin);
  ED_gpencil_project_stroke_to_plane(p->scene, obact, rv3d, p->gpl, gps, origin, p->lock_axis - 1);
}

/* convert screen-coordinates to buffer-coordinates */
static void gpencil_stroke_convertcoords(tGPsdata *p,
                                         const float mval[2],
                                         float out[3],
                                         float *depth)
{
  bGPdata *gpd = p->gpd;
  if (depth && (*depth == DEPTH_INVALID)) {
    depth = NULL;
  }

  /* in 3d-space - pt->x/y/z are 3 side-by-side floats */
  if (gpd->runtime.sbuffer_sflag & GP_STROKE_3DSPACE) {

    /* add small offset to keep stroke over the surface */
    if ((depth) && (gpd->zdepth_offset > 0.0f) && (*p->align_flag & GP_PROJECT_DEPTH_VIEW)) {
      *depth *= (1.0f - (gpd->zdepth_offset / 1000.0f));
    }

    int mval_i[2];
    float rmval[2];
    rmval[0] = mval[0] - 0.5f;
    rmval[1] = mval[1] - 0.5f;
    round_v2i_v2fl(mval_i, rmval);

    if (gpencil_project_check(p) && ED_view3d_autodist_simple(p->region, mval_i, out, 0, depth)) {
      /* projecting onto 3D-Geometry
       * - nothing more needs to be done here, since view_autodist_simple() has already done it
       */

      /* verify valid zdepth, if it's wrong, the default drawing mode is used
       * and the function doesn't return now */
      if ((depth == NULL) || (*depth <= 1.0f)) {
        return;
      }
    }

    float mval_prj[2];
    float rvec[3];

    /* Current method just converts each point in screen-coordinates to
     * 3D-coordinates using the 3D-cursor as reference. In general, this
     * works OK, but it could of course be improved. */

    gpencil_get_3d_reference(p, rvec);
    const float zfac = ED_view3d_calc_zfac(p->region->regiondata, rvec);

    if (ED_view3d_project_float_global(p->region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK) {
      float dvec[3];
      float xy_delta[2];
      sub_v2_v2v2(xy_delta, mval_prj, mval);
      ED_view3d_win_to_delta(p->region, xy_delta, zfac, dvec);
      sub_v3_v3v3(out, rvec, dvec);
    }
    else {
      zero_v3(out);
    }
  }
}

/* Apply jitter to stroke point. */
static void gpencil_brush_jitter(bGPdata *gpd, tGPspoint *pt, const float amplitude)
{
  const float axis[2] = {0.0f, 1.0f};
  /* Jitter is applied perpendicular to the mouse movement vector (2D space). */
  float mvec[2];
  /* Mouse movement in ints -> floats. */
  if (gpd->runtime.sbuffer_used > 1) {
    tGPspoint *pt_prev = pt - 1;
    sub_v2_v2v2(mvec, pt->m_xy, pt_prev->m_xy);
    normalize_v2(mvec);
    /* Rotate mvec by 90 degrees... */
    float angle = angle_v2v2(mvec, axis);
    /* Reduce noise in the direction of the stroke. */
    mvec[0] *= cos(angle);
    mvec[1] *= sin(angle);

    /* Scale by displacement amount, and apply. */
    madd_v2_v2fl(pt->m_xy, mvec, amplitude * 10.0f);
  }
}

/* Apply pressure change depending of the angle of the stroke to simulate a pen with shape */
static void gpencil_brush_angle(bGPdata *gpd, Brush *brush, tGPspoint *pt, const float mval[2])
{
  float mvec[2];
  float sen = brush->gpencil_settings->draw_angle_factor; /* sensitivity */
  float fac;

  /* default angle of brush in radians */
  float angle = brush->gpencil_settings->draw_angle;
  /* angle vector of the brush with full thickness */
  const float v0[2] = {cos(angle), sin(angle)};

  /* Apply to first point (only if there are 2 points because before no data to do it ) */
  if (gpd->runtime.sbuffer_used == 1) {
    sub_v2_v2v2(mvec, mval, (pt - 1)->m_xy);
    normalize_v2(mvec);

    /* uses > 1.0f to get a smooth transition in first point */
    fac = 1.4f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
    (pt - 1)->pressure = (pt - 1)->pressure - (sen * fac);

    CLAMP((pt - 1)->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
  }

  /* apply from second point */
  if (gpd->runtime.sbuffer_used >= 1) {
    sub_v2_v2v2(mvec, mval, (pt - 1)->m_xy);
    normalize_v2(mvec);

    fac = 1.0f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
    /* interpolate with previous point for smoother transitions */
    pt->pressure = interpf(pt->pressure - (sen * fac), (pt - 1)->pressure, 0.3f);
    CLAMP(pt->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
  }
}

/**
 * Apply smooth to buffer while drawing
 * to smooth point C, use 2 before (A, B) and current point (D):
 *
 * `A----B-----C------D`
 *
 * \param p: Temp data
 * \param inf: Influence factor
 * \param idx: Index of the last point (need minimum 3 points in the array)
 */
static void gpencil_smooth_buffer(tGPsdata *p, float inf, int idx)
{
  bGPdata *gpd = p->gpd;
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  const short num_points = gpd->runtime.sbuffer_used;

  /* Do nothing if not enough points to smooth out */
  if ((num_points < 3) || (idx < 3) || (inf == 0.0f)) {
    return;
  }

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  const float steps = (idx < 4) ? 3.0f : 4.0f;

  tGPspoint *pta = idx >= 4 ? &points[idx - 4] : NULL;
  tGPspoint *ptb = idx >= 3 ? &points[idx - 3] : NULL;
  tGPspoint *ptc = idx >= 2 ? &points[idx - 2] : NULL;
  tGPspoint *ptd = &points[idx - 1];

  float sco[2] = {0.0f};
  float a[2], b[2], c[2], d[2];
  float pressure = 0.0f;
  float strength = 0.0f;
  const float average_fac = 1.0f / steps;

  /* Compute smoothed coordinate by taking the ones nearby */
  if (pta) {
    copy_v2_v2(a, pta->m_xy);
    madd_v2_v2fl(sco, a, average_fac);
    pressure += pta->pressure * average_fac;
    strength += pta->strength * average_fac;
  }
  if (ptb) {
    copy_v2_v2(b, ptb->m_xy);
    madd_v2_v2fl(sco, b, average_fac);
    pressure += ptb->pressure * average_fac;
    strength += ptb->strength * average_fac;
  }
  if (ptc) {
    copy_v2_v2(c, ptc->m_xy);
    madd_v2_v2fl(sco, c, average_fac);
    pressure += ptc->pressure * average_fac;
    strength += ptc->strength * average_fac;
  }
  if (ptd) {
    copy_v2_v2(d, ptd->m_xy);
    madd_v2_v2fl(sco, d, average_fac);
    pressure += ptd->pressure * average_fac;
    strength += ptd->strength * average_fac;
  }

  /* Based on influence factor, blend between original and optimal smoothed coordinate but not
   * for Guide mode. */
  if (!guide->use_guide) {
    interp_v2_v2v2(c, c, sco, inf);
    copy_v2_v2(ptc->m_xy, c);
  }
  /* Interpolate pressure. */
  ptc->pressure = interpf(ptc->pressure, pressure, inf);
  /* Interpolate strength. */
  ptc->strength = interpf(ptc->strength, strength, inf);
}

/* Helper: Apply smooth to segment from Index to Index */
static void gpencil_smooth_segment(bGPdata *gpd, const float inf, int from_idx, int to_idx)
{
  const short num_points = to_idx - from_idx;
  /* Do nothing if not enough points to smooth out */
  if ((num_points < 3) || (inf == 0.0f)) {
    return;
  }

  if (from_idx <= 2) {
    return;
  }

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  const float average_fac = 0.25f;

  for (int i = from_idx; i < to_idx + 1; i++) {

    tGPspoint *pta = i >= 3 ? &points[i - 3] : NULL;
    tGPspoint *ptb = i >= 2 ? &points[i - 2] : NULL;
    tGPspoint *ptc = i >= 1 ? &points[i - 1] : &points[i];
    tGPspoint *ptd = &points[i];

    float sco[2] = {0.0f};
    float pressure = 0.0f;
    float strength = 0.0f;

    /* Compute smoothed coordinate by taking the ones nearby */
    if (pta) {
      madd_v2_v2fl(sco, pta->m_xy, average_fac);
      pressure += pta->pressure * average_fac;
      strength += pta->strength * average_fac;
    }
    else {
      madd_v2_v2fl(sco, ptc->m_xy, average_fac);
      pressure += ptc->pressure * average_fac;
      strength += ptc->strength * average_fac;
    }

    if (ptb) {
      madd_v2_v2fl(sco, ptb->m_xy, average_fac);
      pressure += ptb->pressure * average_fac;
      strength += ptb->strength * average_fac;
    }
    else {
      madd_v2_v2fl(sco, ptc->m_xy, average_fac);
      pressure += ptc->pressure * average_fac;
      strength += ptc->strength * average_fac;
    }

    madd_v2_v2fl(sco, ptc->m_xy, average_fac);
    pressure += ptc->pressure * average_fac;
    strength += ptc->strength * average_fac;

    madd_v2_v2fl(sco, ptd->m_xy, average_fac);
    pressure += ptd->pressure * average_fac;
    strength += ptd->strength * average_fac;

    /* Based on influence factor, blend between original and optimal smoothed coordinate. */
    interp_v2_v2v2(ptc->m_xy, ptc->m_xy, sco, inf);

    /* Interpolate pressure. */
    ptc->pressure = interpf(ptc->pressure, pressure, inf);
    /* Interpolate strength. */
    ptc->strength = interpf(ptc->strength, strength, inf);
  }
}

static void gpencil_apply_randomness(tGPsdata *p,
                                     BrushGpencilSettings *brush_settings,
                                     tGPspoint *pt,
                                     const bool press,
                                     const bool strength,
                                     const bool uv)
{
  bGPdata *gpd = p->gpd;
  GpRandomSettings random_settings = p->random_settings;
  float value = 0.0f;
  /* Apply randomness to pressure. */
  if ((brush_settings->draw_random_press > 0.0f) && (press)) {
    if ((brush_settings->flag2 & GP_BRUSH_USE_PRESS_AT_STROKE) == 0) {
      float rand = BLI_rng_get_float(p->rng) * 2.0f - 1.0f;
      value = 1.0 + rand * 2.0 * brush_settings->draw_random_press;
    }
    else {
      value = 1.0 + random_settings.pressure * brush_settings->draw_random_press;
    }

    /* Apply random curve. */
    if (brush_settings->flag2 & GP_BRUSH_USE_PRESSURE_RAND_PRESS) {
      value *= BKE_curvemapping_evaluateF(
          brush_settings->curve_rand_pressure, 0, random_settings.pen_press);
    }

    pt->pressure *= value;
    CLAMP(pt->pressure, 0.1f, 1.0f);
  }

  /* Apply randomness to color strength. */
  if ((brush_settings->draw_random_strength) && (strength)) {
    if ((brush_settings->flag2 & GP_BRUSH_USE_STRENGTH_AT_STROKE) == 0) {
      float rand = BLI_rng_get_float(p->rng) * 2.0f - 1.0f;
      value = 1.0 + rand * brush_settings->draw_random_strength;
    }
    else {
      value = 1.0 + random_settings.strength * brush_settings->draw_random_strength;
    }

    /* Apply random curve. */
    if (brush_settings->flag2 & GP_BRUSH_USE_STRENGTH_RAND_PRESS) {
      value *= BKE_curvemapping_evaluateF(
          brush_settings->curve_rand_pressure, 0, random_settings.pen_press);
    }

    pt->strength *= value;
    CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
  }

  /* Apply randomness to uv texture rotation. */
  if ((brush_settings->uv_random > 0.0f) && (uv)) {
    if ((brush_settings->flag2 & GP_BRUSH_USE_UV_AT_STROKE) == 0) {
      float rand = BLI_hash_int_01(BLI_hash_int_2d((int)pt->m_xy[0], gpd->runtime.sbuffer_used)) *
                       2.0f -
                   1.0f;
      value = rand * M_PI_2 * brush_settings->uv_random;
    }
    else {
      value = random_settings.uv * M_PI_2 * brush_settings->uv_random;
    }

    /* Apply random curve. */
    if (brush_settings->flag2 & GP_BRUSH_USE_UV_RAND_PRESS) {
      value *= BKE_curvemapping_evaluateF(
          brush_settings->curve_rand_uv, 0, random_settings.pen_press);
    }

    pt->uv_rot += value;
    CLAMP(pt->uv_rot, -M_PI_2, M_PI_2);
  }
}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gpencil_stroke_addpoint(tGPsdata *p,
                                     const float mval[2],
                                     float pressure,
                                     double curtime)
{
  bGPdata *gpd = p->gpd;
  Brush *brush = p->brush;
  BrushGpencilSettings *brush_settings = p->brush->gpencil_settings;
  tGPspoint *pt;
  Object *obact = (Object *)p->ownerPtr.data;
  RegionView3D *rv3d = p->region->regiondata;

  /* check painting mode */
  if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    /* straight lines only - i.e. only store start and end point in buffer */
    if (gpd->runtime.sbuffer_used == 0) {
      /* first point in buffer (start point) */
      pt = (tGPspoint *)(gpd->runtime.sbuffer);

      /* store settings */
      copy_v2_v2(pt->m_xy, mval);
      /* Pressure values are unreliable, so ignore for now, see #44932. */
      pt->pressure = 1.0f;
      pt->strength = 1.0f;
      pt->time = (float)(curtime - p->inittime);

      /* increment buffer size */
      gpd->runtime.sbuffer_used++;
    }
    else {
      /* just reset the endpoint to the latest value
       * - assume that pointers for this are always valid...
       */
      pt = ((tGPspoint *)(gpd->runtime.sbuffer) + 1);

      /* store settings */
      copy_v2_v2(pt->m_xy, mval);
      /* Pressure values are unreliable, so ignore for now, see #44932. */
      pt->pressure = 1.0f;
      pt->strength = 1.0f;
      pt->time = (float)(curtime - p->inittime);

      /* now the buffer has 2 points (and shouldn't be allowed to get any larger) */
      gpd->runtime.sbuffer_used = 2;
    }

    /* can keep carrying on this way :) */
    return GP_STROKEADD_NORMAL;
  }

  if (p->paintmode == GP_PAINTMODE_DRAW) { /* normal drawing */
    /* check if still room in buffer or add more */
    gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
        gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

    /* Check the buffer was created. */
    if (gpd->runtime.sbuffer == NULL) {
      return GP_STROKEADD_INVALID;
    }

    /* get pointer to destination point */
    pt = ((tGPspoint *)(gpd->runtime.sbuffer) + gpd->runtime.sbuffer_used);

    /* store settings */
    pt->strength = brush_settings->draw_strength;
    pt->pressure = 1.0f;
    pt->uv_rot = 0.0f;
    copy_v2_v2(pt->m_xy, mval);

    /* pressure */
    if (brush_settings->flag & GP_BRUSH_USE_PRESSURE) {
      pt->pressure *= BKE_curvemapping_evaluateF(brush_settings->curve_sensitivity, 0, pressure);
    }

    /* color strength */
    if (brush_settings->flag & GP_BRUSH_USE_STRENGTH_PRESSURE) {
      pt->strength *= BKE_curvemapping_evaluateF(brush_settings->curve_strength, 0, pressure);
      CLAMP(pt->strength, MIN2(GPENCIL_STRENGTH_MIN, brush_settings->draw_strength), 1.0f);
    }

    /* Set vertex colors for buffer. */
    ED_gpencil_sbuffer_vertex_color_set(p->depsgraph,
                                        p->ob,
                                        p->scene->toolsettings,
                                        p->brush,
                                        p->material,
                                        p->random_settings.hsv,
                                        p->random_settings.pen_press);

    if (brush_settings->flag & GP_BRUSH_GROUP_RANDOM) {
      /* Apply jitter to position */
      if (brush_settings->draw_jitter > 0.0f) {
        float rand = BLI_rng_get_float(p->rng) * 2.0f - 1.0f;
        float jitpress = 1.0f;
        if (brush_settings->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
          jitpress = BKE_curvemapping_evaluateF(brush_settings->curve_jitter, 0, pressure);
        }
        /* FIXME the +2 means minimum jitter is 4 which is a bit strange for UX. */
        const float exp_factor = brush_settings->draw_jitter + 2.0f;
        const float fac = rand * square_f(exp_factor) * jitpress;
        gpencil_brush_jitter(gpd, pt, fac);
      }

      /* Apply other randomness. */
      gpencil_apply_randomness(p, brush_settings, pt, true, true, true);
    }

    /* apply angle of stroke to brush size */
    if (brush_settings->draw_angle_factor != 0.0f) {
      gpencil_brush_angle(gpd, brush, pt, mval);
    }

    /* point time */
    pt->time = (float)(curtime - p->inittime);

    /* point uv (only 3d view) */
    if (gpd->runtime.sbuffer_used > 0) {
      tGPspoint *ptb = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
      bGPDspoint spt, spt2;

      /* get origin to reproject point */
      float origin[3];
      gpencil_get_3d_reference(p, origin);
      /* reproject current */
      ED_gpencil_tpoint_to_point(p->region, origin, pt, &spt);
      ED_gpencil_project_point_to_plane(
          p->scene, obact, p->gpl, rv3d, origin, p->lock_axis - 1, &spt);

      /* reproject previous */
      ED_gpencil_tpoint_to_point(p->region, origin, ptb, &spt2);
      ED_gpencil_project_point_to_plane(
          p->scene, obact, p->gpl, rv3d, origin, p->lock_axis - 1, &spt2);
      p->totpixlen += len_v3v3(&spt.x, &spt2.x);
      pt->uv_fac = p->totpixlen;
    }
    else {
      p->totpixlen = 0.0f;
      pt->uv_fac = 0.0f;
    }

    /* increment counters */
    gpd->runtime.sbuffer_used++;

    /* Smooth while drawing previous points with a reduction factor for previous. */
    if (brush->gpencil_settings->active_smooth > 0.0f) {
      for (int s = 0; s < 3; s++) {
        gpencil_smooth_buffer(p,
                              brush->gpencil_settings->active_smooth * ((3.0f - s) / 3.0f),
                              gpd->runtime.sbuffer_used - s);
      }
    }

    /* Update evaluated data. */
    ED_gpencil_sbuffer_update_eval(gpd, p->ob_eval);

    return GP_STROKEADD_NORMAL;
  }
  /* return invalid state for now... */
  return GP_STROKEADD_INVALID;
}

static void gpencil_stroke_unselect(bGPdata *gpd, bGPDstroke *gps)
{
  gps->flag &= ~GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_reset(gps);
  for (int i = 0; i < gps->totpoints; i++) {
    gps->points[i].flag &= ~GP_SPOINT_SELECT;
  }
  /* Update the selection from the stroke to the curve. */
  if (gps->editcurve) {
    BKE_gpencil_editcurve_stroke_sync_selection(gpd, gps, gps->editcurve);
  }
}

static bGPDstroke *gpencil_stroke_to_outline(tGPsdata *p, bGPDstroke *gps)
{
  bGPDlayer *gpl = p->gpl;
  RegionView3D *rv3d = p->region->regiondata;
  Brush *brush = p->brush;
  BrushGpencilSettings *gpencil_settings = brush->gpencil_settings;
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(p->ob, gps->mat_nr + 1);
  const bool is_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0);

  if (!is_stroke) {
    return gps;
  }

  /* Duplicate the stroke to apply any layer thickness change. */
  bGPDstroke *gps_duplicate = BKE_gpencil_stroke_duplicate(gps, true, false);

  /* Apply layer thickness change. */
  gps_duplicate->thickness += gpl->line_change;
  /* Apply object scale to thickness. */
  gps_duplicate->thickness *= mat4_to_scale(p->ob->object_to_world);
  CLAMP_MIN(gps_duplicate->thickness, 1.0f);

  /* Stroke. */
  float diff_mat[4][4];
  unit_m4(diff_mat);
  const float outline_thickness = (float)brush->size * gpencil_settings->outline_fac * 0.5f;
  bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
      rv3d->viewmat, p->gpd, gpl, gps_duplicate, 3, diff_mat, outline_thickness);
  /* Assign material. */
  if (gpencil_settings->material_alt == NULL) {
    gps_perimeter->mat_nr = gps->mat_nr;
  }
  else {
    Material *ma = gpencil_settings->material_alt;
    int mat_idx = BKE_gpencil_material_find_index_by_name_prefix(p->ob, ma->id.name + 2);
    if (mat_idx > -1) {
      gps_perimeter->mat_nr = mat_idx;
    }
    else {
      gps_perimeter->mat_nr = gps->mat_nr;
    }
  }

  /* Set pressure constant. */
  gps_perimeter->thickness = max_ii((int)outline_thickness, 1);
  /* Apply Fill Vertex Color data. */
  ED_gpencil_fill_vertex_color_set(p->scene->toolsettings, brush, gps_perimeter);

  bGPDspoint *pt;
  for (int i = 0; i < gps_perimeter->totpoints; i++) {
    pt = &gps_perimeter->points[i];
    pt->pressure = 1.0f;
    /* Apply Point Vertex Color data. */
    ED_gpencil_point_vertex_color_set(p->scene->toolsettings, brush, pt, NULL);
  }

  /* Remove original stroke. */
  BKE_gpencil_free_stroke(gps);

  /* Free Temp stroke. */
  BKE_gpencil_free_stroke(gps_duplicate);

  return gps_perimeter;
}

/* make a new stroke from the buffer data */
static void gpencil_stroke_newfrombuffer(tGPsdata *p)
{
  bGPdata *gpd = p->gpd;
  bGPDlayer *gpl = p->gpl;
  bGPDstroke *gps;
  bGPDspoint *pt;
  tGPspoint *ptc;
  MDeformVert *dvert = NULL;
  Brush *brush = p->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  ToolSettings *ts = p->scene->toolsettings;
  Depsgraph *depsgraph = p->depsgraph;
  Object *obact = (Object *)p->ownerPtr.data;
  RegionView3D *rv3d = p->region->regiondata;
  const int def_nr = gpd->vertex_group_active_index - 1;
  const bool have_weight = (bool)BLI_findlink(&gpd->vertex_group_names, def_nr);
  const char align_flag = ts->gpencil_v3d_align;
  const bool is_depth = (bool)(align_flag & (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE));
  const bool is_lock_axis_view = (bool)(ts->gp_sculpt.lock_axis == 0);
  const bool is_camera = is_lock_axis_view && (rv3d->persp == RV3D_CAMOB) && (!is_depth);
  int totelem;

  /* For very low pressure at the end, truncate stroke. */
  if (p->paintmode == GP_PAINTMODE_DRAW) {
    int last_i = gpd->runtime.sbuffer_used - 1;
    while (last_i > 0) {
      ptc = (tGPspoint *)gpd->runtime.sbuffer + last_i;
      if (ptc->pressure > 0.001f) {
        break;
      }
      gpd->runtime.sbuffer_used = last_i - 1;
      CLAMP_MIN(gpd->runtime.sbuffer_used, 1);

      last_i--;
    }
  }
  /* Since strokes are so fine,
   * when using their depth we need a margin otherwise they might get missed. */
  int depth_margin = (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 4 : 0;

  /* get total number of points to allocate space for
   * - drawing straight-lines only requires the endpoints
   */
  if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    totelem = (gpd->runtime.sbuffer_used >= 2) ? 2 : gpd->runtime.sbuffer_used;
  }
  else {
    totelem = gpd->runtime.sbuffer_used;
  }

  /* exit with error if no valid points from this stroke */
  if (totelem == 0) {
    return;
  }

  /* allocate memory for a new stroke */
  gps = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");

  /* copy appropriate settings for stroke */
  gps->totpoints = totelem;
  gps->thickness = brush->size;
  gps->fill_opacity_fac = 1.0f;
  gps->hardeness = brush->gpencil_settings->hardeness;
  copy_v2_v2(gps->aspect_ratio, brush->gpencil_settings->aspect_ratio);
  gps->flag = gpd->runtime.sbuffer_sflag;
  gps->inittime = p->inittime;
  gps->uv_scale = 1.0f;

  /* Set stroke caps. */
  gps->caps[0] = gps->caps[1] = (short)brush->gpencil_settings->caps_type;

  /* allocate enough memory for a continuous array for storage points */
  const int subdivide = brush->gpencil_settings->draw_subdivide;

  gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
  gps->dvert = NULL;

  /* set pointer to first non-initialized point */
  pt = gps->points + (gps->totpoints - totelem);
  if (gps->dvert != NULL) {
    dvert = gps->dvert + (gps->totpoints - totelem);
  }

  /* Apply the vertex color to fill. */
  ED_gpencil_fill_vertex_color_set(ts, brush, gps);

  /* copy points from the buffer to the stroke */
  if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    /* straight lines only -> only endpoints */
    {
      /* first point */
      ptc = gpd->runtime.sbuffer;

      /* convert screen-coordinates to appropriate coordinates (and store them) */
      gpencil_stroke_convertcoords(p, ptc->m_xy, &pt->x, NULL);
      /* copy pressure and time */
      pt->pressure = ptc->pressure;
      pt->strength = ptc->strength;
      CLAMP(pt->strength, MIN2(GPENCIL_STRENGTH_MIN, brush_settings->draw_strength), 1.0f);
      copy_v4_v4(pt->vert_color, ptc->vert_color);
      pt->time = ptc->time;
      /* Apply the vertex color to point. */
      ED_gpencil_point_vertex_color_set(ts, brush, pt, ptc);

      pt++;

      if ((ts->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) && (have_weight)) {
        BKE_gpencil_dvert_ensure(gps);
        MDeformWeight *dw = BKE_defvert_ensure_index(dvert, def_nr);
        if (dw) {
          dw->weight = ts->vgroup_weight;
        }
        dvert++;
      }
      else {
        if (dvert != NULL) {
          dvert->totweight = 0;
          dvert->dw = NULL;
          dvert++;
        }
      }
    }

    if (totelem == 2) {
      /* last point if applicable */
      ptc = ((tGPspoint *)gpd->runtime.sbuffer) + (gpd->runtime.sbuffer_used - 1);

      /* convert screen-coordinates to appropriate coordinates (and store them) */
      gpencil_stroke_convertcoords(p, ptc->m_xy, &pt->x, NULL);
      /* copy pressure and time */
      pt->pressure = ptc->pressure;
      pt->strength = ptc->strength;
      CLAMP(pt->strength, MIN2(GPENCIL_STRENGTH_MIN, brush_settings->draw_strength), 1.0f);
      pt->time = ptc->time;
      /* Apply the vertex color to point. */
      ED_gpencil_point_vertex_color_set(ts, brush, pt, ptc);

      if ((ts->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) && (have_weight)) {
        BKE_gpencil_dvert_ensure(gps);
        MDeformWeight *dw = BKE_defvert_ensure_index(dvert, def_nr);
        if (dw) {
          dw->weight = ts->vgroup_weight;
        }
      }
      else {
        if (dvert != NULL) {
          dvert->totweight = 0;
          dvert->dw = NULL;
        }
      }
    }

    /* reproject to plane (only in 3d space) */
    gpencil_reproject_toplane(p, gps);

    /* Change position relative to object. */
    gpencil_world_to_object_space(depsgraph, obact, gpl, gps);

    /* If camera view or view projection, reproject flat to view to avoid perspective effect. */
    if ((!is_depth) &&
        (((align_flag & GP_PROJECT_VIEWSPACE) && is_lock_axis_view) || (is_camera))) {
      ED_gpencil_project_stroke_to_view(p->C, p->gpl, gps);
    }
  }
  else {
    float *depth_arr = NULL;

    /* get an array of depths, far depths are blended */
    if (gpencil_project_check(p)) {
      int mval_i[2], mval_prev[2] = {0};
      int interp_depth = 0;
      int found_depth = 0;

      depth_arr = MEM_mallocN(sizeof(float) * gpd->runtime.sbuffer_used, "depth_points");

      const ViewDepths *depths = p->depths;
      int i;
      for (i = 0, ptc = gpd->runtime.sbuffer; i < gpd->runtime.sbuffer_used; i++, ptc++, pt++) {

        round_v2i_v2fl(mval_i, ptc->m_xy);

        if ((ED_view3d_depth_read_cached(depths, mval_i, depth_margin, depth_arr + i) == 0) &&
            (i && (ED_view3d_depth_read_cached_seg(
                       depths, mval_i, mval_prev, depth_margin + 1, depth_arr + i) == 0))) {
          interp_depth = true;
        }
        else {
          found_depth = true;
        }

        copy_v2_v2_int(mval_prev, mval_i);
      }

      if (found_depth == false) {
        /* Unfortunately there is not much we can do when the depth isn't found,
         * ignore depth in this case, use the 3D cursor. */
        for (i = gpd->runtime.sbuffer_used - 1; i >= 0; i--) {
          depth_arr[i] = 0.9999f;
        }
      }
      else {
        if ((ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) &&
            ((ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_ENDPOINTS) ||
             (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_FIRST))) {
          int first_valid = 0;
          int last_valid = 0;

          /* find first valid contact point */
          for (i = 0; i < gpd->runtime.sbuffer_used; i++) {
            if (depth_arr[i] != DEPTH_INVALID) {
              break;
            }
          }
          first_valid = i;

          /* find last valid contact point */
          if (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_FIRST) {
            last_valid = first_valid;
          }
          else {
            for (i = gpd->runtime.sbuffer_used - 1; i >= 0; i--) {
              if (depth_arr[i] != DEPTH_INVALID) {
                break;
              }
            }
            last_valid = i;
          }
          /* invalidate any other point, to interpolate between
           * first and last contact in an imaginary line between them */
          for (i = 0; i < gpd->runtime.sbuffer_used; i++) {
            if (!ELEM(i, first_valid, last_valid)) {
              depth_arr[i] = DEPTH_INVALID;
            }
          }
          interp_depth = true;
        }

        if (interp_depth) {
          interp_sparse_array(depth_arr, gpd->runtime.sbuffer_used, DEPTH_INVALID);
        }
      }
    }

    pt = gps->points;
    dvert = gps->dvert;

    /* convert all points (normal behavior) */
    int i;
    for (i = 0, ptc = gpd->runtime.sbuffer; i < gpd->runtime.sbuffer_used && ptc;
         i++, ptc++, pt++) {
      /* convert screen-coordinates to appropriate coordinates (and store them) */
      gpencil_stroke_convertcoords(p, ptc->m_xy, &pt->x, depth_arr ? depth_arr + i : NULL);

      /* copy pressure and time */
      pt->pressure = ptc->pressure;
      pt->strength = ptc->strength;
      CLAMP(pt->strength, MIN2(GPENCIL_STRENGTH_MIN, brush_settings->draw_strength), 1.0f);
      copy_v4_v4(pt->vert_color, ptc->vert_color);
      pt->time = ptc->time;
      pt->uv_fac = ptc->uv_fac;
      pt->uv_rot = ptc->uv_rot;
      /* Apply the vertex color to point. */
      ED_gpencil_point_vertex_color_set(ts, brush, pt, ptc);

      if (dvert != NULL) {
        dvert->totweight = 0;
        dvert->dw = NULL;
        dvert++;
      }
    }

    /* subdivide and smooth the stroke */
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) && (subdivide > 0)) {
      gpencil_subdivide_stroke(gpd, gps, subdivide);
    }

    /* Smooth stroke after subdiv - only if there's something to do for each iteration.
     * Keep the original stroke shape as much as possible. */
    const float smoothfac = brush->gpencil_settings->draw_smoothfac;
    const int iterations = brush->gpencil_settings->draw_smoothlvl;
    if (brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) {
      BKE_gpencil_stroke_smooth(gps, smoothfac, iterations, true, true, false, false, true, NULL);
    }
    /* If reproject the stroke using Stroke mode, need to apply a smooth because
     * the reprojection creates small jitter. */
    if (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) {
      float ifac = (float)brush->gpencil_settings->input_samples / 10.0f;
      float sfac = interpf(1.0f, 0.2f, ifac);
      for (i = 0; i < gps->totpoints; i++) {
        BKE_gpencil_stroke_smooth_point(gps, i, sfac, 2, false, true, gps);
        BKE_gpencil_stroke_smooth_strength(gps, i, sfac, 2, gps);
      }
    }

    /* Simplify adaptive */
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) &&
        (brush->gpencil_settings->simplify_f > 0.0f)) {
      BKE_gpencil_stroke_simplify_adaptive(gpd, gps, brush->gpencil_settings->simplify_f);
    }

    /* Set material index. */
    gps->mat_nr = BKE_gpencil_object_material_get_index_from_brush(p->ob, p->brush);
    if (gps->mat_nr < 0) {
      if (p->ob->actcol - 1 < 0) {
        gps->mat_nr = 0;
      }
      else {
        gps->mat_nr = p->ob->actcol - 1;
      }
    }

    /* Convert to Outline. */
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) &&
        (brush->gpencil_settings->flag & GP_BRUSH_OUTLINE_STROKE)) {
      gps = gpencil_stroke_to_outline(p, gps);
    }

    /* reproject to plane (only in 3d space) */
    gpencil_reproject_toplane(p, gps);
    /* Change position relative to parent object. */
    gpencil_world_to_object_space(depsgraph, obact, gpl, gps);
    /* If camera view or view projection, reproject flat to view to avoid perspective effect. */
    if ((!is_depth) && (((align_flag & GP_PROJECT_VIEWSPACE) && is_lock_axis_view) || is_camera)) {
      ED_gpencil_project_stroke_to_view(p->C, p->gpl, gps);
    }

    if (depth_arr) {
      MEM_freeN(depth_arr);
    }
  }

  /* add stroke to frame, usually on tail of the listbase, but if on back is enabled the stroke
   * is added on listbase head because the drawing order is inverse and the head stroke is the
   * first to draw. This is very useful for artist when drawing the background.
   */
  if (ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) {
    BLI_addhead(&p->gpf->strokes, gps);
  }
  else {
    BLI_addtail(&p->gpf->strokes, gps);
  }
  /* add weights */
  if ((ts->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) && (have_weight)) {
    BKE_gpencil_dvert_ensure(gps);
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformVert *ve = &gps->dvert[i];
      MDeformWeight *dw = BKE_defvert_ensure_index(ve, def_nr);
      if (dw) {
        dw->weight = ts->vgroup_weight;
      }
    }
  }

  /* post process stroke */
  if ((p->brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) &&
      p->brush->gpencil_settings->flag & GP_BRUSH_TRIM_STROKE) {
    BKE_gpencil_stroke_trim(gpd, gps);
  }

  /* Join with existing strokes. */
  if (ts->gpencil_flags & GP_TOOL_FLAG_AUTOMERGE_STROKE) {
    if ((gps->prev != NULL) || (gps->next != NULL)) {
      BKE_gpencil_stroke_boundingbox_calc(gps);
      float diff_mat[4][4], ctrl1[2], ctrl2[2];
      BKE_gpencil_layer_transform_matrix_get(depsgraph, p->ob, gpl, diff_mat);
      ED_gpencil_stroke_extremes_to2d(&p->gsc, diff_mat, gps, ctrl1, ctrl2);

      int pt_index = 0;
      bool doit = true;
      while (doit && gps) {
        bGPDstroke *gps_target = ED_gpencil_stroke_nearest_to_ends(p->C,
                                                                   &p->gsc,
                                                                   gpl,
                                                                   gpl->actframe,
                                                                   gps,
                                                                   ctrl1,
                                                                   ctrl2,
                                                                   GPENCIL_MINIMUM_JOIN_DIST,
                                                                   &pt_index);

        if (gps_target != NULL) {
          /* Unselect all points of source and destination strokes. This is required to avoid
           * a change in the resolution of the original strokes during the join. */
          gpencil_stroke_unselect(gpd, gps);
          gpencil_stroke_unselect(gpd, gps_target);
          gps = ED_gpencil_stroke_join_and_trim(p->gpd, p->gpf, gps, gps_target, pt_index);
        }
        else {
          doit = false;
        }
      }
    }
    ED_gpencil_stroke_close_by_distance(gps, 0.02f);
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  /* In multi-frame mode, duplicate the stroke in other frames. */
  if (GPENCIL_MULTIEDIT_SESSIONS_ON(p->gpd)) {
    const bool tail = (ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK);
    BKE_gpencil_stroke_copy_to_keyframes(gpd, gpl, p->gpf, gps, tail);
  }

  gpencil_update_cache(p->gpd);
  BKE_gpencil_tag_full_update(
      p->gpd, gpl, GPENCIL_MULTIEDIT_SESSIONS_ON(p->gpd) ? NULL : p->gpf, NULL);
}

/* --- 'Eraser' for 'Paint' Tool ------ */

/* only erase stroke points that are visible */
static bool gpencil_stroke_eraser_is_occluded(
    tGPsdata *p, bGPDlayer *gpl, bGPDspoint *pt, const int x, const int y)
{
  Object *obact = (Object *)p->ownerPtr.data;
  Brush *brush = p->brush;
  Brush *eraser = p->eraser;
  BrushGpencilSettings *gp_settings = NULL;

  if (brush->gpencil_tool == GPAINT_TOOL_ERASE) {
    gp_settings = brush->gpencil_settings;
  }
  else if ((eraser != NULL) & (eraser->gpencil_tool == GPAINT_TOOL_ERASE)) {
    gp_settings = eraser->gpencil_settings;
  }

  if ((gp_settings != NULL) && (gp_settings->flag & GP_BRUSH_OCCLUDE_ERASER)) {
    RegionView3D *rv3d = p->region->regiondata;

    const int mval_i[2] = {x, y};
    float mval_3d[3];
    float fpt[3];

    float diff_mat[4][4];
    /* calculate difference matrix if parent object */
    BKE_gpencil_layer_transform_matrix_get(p->depsgraph, obact, gpl, diff_mat);

    float p_depth;
    if (ED_view3d_depth_read_cached(p->depths, mval_i, 0, &p_depth)) {
      ED_view3d_depth_unproject_v3(p->region, mval_i, (double)p_depth, mval_3d);

      const float depth_mval = ED_view3d_calc_depth_for_comparison(rv3d, mval_3d);

      mul_v3_m4v3(fpt, diff_mat, &pt->x);
      const float depth_pt = ED_view3d_calc_depth_for_comparison(rv3d, fpt);

      /* Checked occlusion flag. */
      pt->flag |= GP_SPOINT_TEMP_TAG;
      if (depth_pt > depth_mval) {
        /* Is occluded. */
        pt->flag |= GP_SPOINT_TEMP_TAG2;
        return true;
      }
    }
  }
  return false;
}

/* apply a falloff effect to brush strength, based on distance */
static float gpencil_stroke_eraser_calc_influence(tGPsdata *p,
                                                  const float mval[2],
                                                  const int radius,
                                                  const int co[2])
{
  Brush *brush = p->brush;
  /* Linear Falloff... */
  int mval_i[2];
  round_v2i_v2fl(mval_i, mval);
  float distance = (float)len_v2v2_int(mval_i, co);
  float fac;

  CLAMP(distance, 0.0f, (float)radius);
  fac = 1.0f - (distance / (float)radius);

  /* apply strength factor */
  fac *= brush->gpencil_settings->draw_strength;

  /* Control this further using pen pressure */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    fac *= p->pressure;
  }
  /* Return influence factor computed here */
  return fac;
}

/* helper to free a stroke */
static void gpencil_free_stroke(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps)
{
  if (gps->points) {
    MEM_freeN(gps->points);
  }

  if (gps->dvert) {
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->dvert);
  }

  if (gps->triangles) {
    MEM_freeN(gps->triangles);
  }
  BLI_freelinkN(&gpf->strokes, gps);
  gpencil_update_cache(gpd);
}

/**
 * Analyze points to be removed when soft eraser is used
 * to avoid that segments gets the end points rounded.
 * The round caps breaks the artistic effect.
 */
static void gpencil_stroke_soft_refine(bGPDstroke *gps)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt2 = NULL;
  int i;

  /* Check if enough points. */
  if (gps->totpoints < 3) {
    return;
  }

  /* loop all points to untag any point that next is not tagged */
  pt = gps->points;
  for (i = 1; i < gps->totpoints - 1; i++, pt++) {
    if (pt->flag & GP_SPOINT_TAG) {
      pt2 = &gps->points[i + 1];
      if ((pt2->flag & GP_SPOINT_TAG) == 0) {
        pt->flag &= ~GP_SPOINT_TAG;
      }
    }
  }

  /* loop reverse all points to untag any point that previous is not tagged */
  pt = &gps->points[gps->totpoints - 1];
  for (i = gps->totpoints - 1; i > 0; i--, pt--) {
    if (pt->flag & GP_SPOINT_TAG) {
      pt2 = &gps->points[i - 1];
      if ((pt2->flag & GP_SPOINT_TAG) == 0) {
        pt->flag &= ~GP_SPOINT_TAG;
      }
    }
  }
}

/* eraser tool - evaluation per stroke */
static void gpencil_stroke_eraser_dostroke(tGPsdata *p,
                                           bGPDlayer *gpl,
                                           bGPDframe *gpf,
                                           bGPDstroke *gps,
                                           const float mval[2],
                                           const int radius,
                                           const rcti *rect)
{
  Brush *eraser = p->eraser;
  bGPDspoint *pt0, *pt1, *pt2;
  int pc0[2] = {0};
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int mval_i[2];
  round_v2i_v2fl(mval_i, mval);

  if (gps->totpoints == 0) {
    /* just free stroke */
    gpencil_free_stroke(p->gpd, gpf, gps);
  }
  else if (gps->totpoints == 1) {
    /* only process if it hasn't been masked out... */
    if (!(p->flags & GP_PAINTFLAG_SELECTMASK) || (gps->points->flag & GP_SPOINT_SELECT)) {
      bGPDspoint pt_temp;
      gpencil_point_to_world_space(gps->points, p->diff_mat, &pt_temp);
      gpencil_point_to_xy(&p->gsc, gps, &pt_temp, &pc1[0], &pc1[1]);
      /* Do bound-box check first. */
      if (!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
        /* only check if point is inside */
        if (len_v2v2_int(mval_i, pc1) <= radius) {
          /* free stroke */
          gpencil_free_stroke(p->gpd, gpf, gps);
        }
      }
    }
  }
  else if ((p->flags & GP_PAINTFLAG_STROKE_ERASER) ||
           (eraser->gpencil_settings->eraser_mode == GP_BRUSH_ERASER_STROKE)) {
    for (i = 0; (i + 1) < gps->totpoints; i++) {

      /* only process if it hasn't been masked out... */
      if ((p->flags & GP_PAINTFLAG_SELECTMASK) && !(gps->points->flag & GP_SPOINT_SELECT)) {
        continue;
      }

      /* get points to work with */
      pt1 = gps->points + i;
      bGPDspoint npt;
      gpencil_point_to_world_space(pt1, p->diff_mat, &npt);
      gpencil_point_to_xy(&p->gsc, gps, &npt, &pc1[0], &pc1[1]);

      /* Do bound-box check first. */
      if (!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
        /* only check if point is inside */
        if (len_v2v2_int(mval_i, pc1) <= radius) {
          /* free stroke */
          gpencil_free_stroke(p->gpd, gpf, gps);
          return;
        }
      }
    }
  }
  else {
    /* Pressure threshold at which stroke should be culled */
    const float cull_thresh = 0.005f;

    /* Amount to decrease the pressure of each point with each stroke */
    const float strength = 0.1f;

    /* Perform culling? */
    bool do_cull = false;

    /* Clear Tags
     *
     * NOTE: It's better this way, as we are sure that
     * we don't miss anything, though things will be
     * slightly slower as a result
     */
    for (i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      pt->flag &= ~GP_SPOINT_TAG;
      /* Occlusion already checked. */
      pt->flag &= ~GP_SPOINT_TEMP_TAG;
      /* Point is occluded. */
      pt->flag &= ~GP_SPOINT_TEMP_TAG2;
    }

    /* First Pass: Loop over the points in the stroke
     *   1) Thin out parts of the stroke under the brush
     *   2) Tag "too thin" parts for removal (in second pass)
     */
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* get points to work with */
      pt0 = i > 0 ? gps->points + i - 1 : NULL;
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

      float inf1 = 0.0f;
      float inf2 = 0.0f;

      /* only process if it hasn't been masked out... */
      if ((p->flags & GP_PAINTFLAG_SELECTMASK) && !(gps->points->flag & GP_SPOINT_SELECT)) {
        continue;
      }

      bGPDspoint npt;
      gpencil_point_to_world_space(pt1, p->diff_mat, &npt);
      gpencil_point_to_xy(&p->gsc, gps, &npt, &pc1[0], &pc1[1]);

      gpencil_point_to_world_space(pt2, p->diff_mat, &npt);
      gpencil_point_to_xy(&p->gsc, gps, &npt, &pc2[0], &pc2[1]);

      if (pt0) {
        gpencil_point_to_world_space(pt0, p->diff_mat, &npt);
        gpencil_point_to_xy(&p->gsc, gps, &npt, &pc0[0], &pc0[1]);
      }
      else {
        /* avoid null values */
        copy_v2_v2_int(pc0, pc1);
      }

      /* Check that point segment of the bound-box of the eraser stroke. */
      if ((!ELEM(V2D_IS_CLIPPED, pc0[0], pc0[1]) && BLI_rcti_isect_pt(rect, pc0[0], pc0[1])) ||
          (!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          (!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1]) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1]))) {
        /* Check if point segment of stroke had anything to do with
         * eraser region  (either within stroke painted, or on its lines)
         * - this assumes that line-width is irrelevant.
         */
        if (gpencil_stroke_inside_circle(mval, radius, pc0[0], pc0[1], pc2[0], pc2[1])) {

          bool is_occluded_pt0 = true, is_occluded_pt1 = true, is_occluded_pt2 = true;
          if (pt0) {
            is_occluded_pt0 = ((pt0->flag & GP_SPOINT_TEMP_TAG) != 0) ?
                                  ((pt0->flag & GP_SPOINT_TEMP_TAG2) != 0) :
                                  gpencil_stroke_eraser_is_occluded(p, gpl, pt0, pc0[0], pc0[1]);
          }
          if (is_occluded_pt0) {
            is_occluded_pt1 = ((pt1->flag & GP_SPOINT_TEMP_TAG) != 0) ?
                                  ((pt1->flag & GP_SPOINT_TEMP_TAG2) != 0) :
                                  gpencil_stroke_eraser_is_occluded(p, gpl, pt1, pc1[0], pc1[1]);
            if (is_occluded_pt1) {
              is_occluded_pt2 = ((pt2->flag & GP_SPOINT_TEMP_TAG) != 0) ?
                                    ((pt2->flag & GP_SPOINT_TEMP_TAG2) != 0) :
                                    gpencil_stroke_eraser_is_occluded(p, gpl, pt2, pc2[0], pc2[1]);
            }
          }

          if (!is_occluded_pt0 || !is_occluded_pt1 || !is_occluded_pt2) {
            /* Point is affected: */
            /* Adjust thickness
             *  - Influence of eraser falls off with distance from the middle of the eraser
             *  - Second point gets less influence, as it might get hit again in the next segment
             */

            /* Adjust strength if the eraser is soft */
            if (eraser->gpencil_settings->eraser_mode == GP_BRUSH_ERASER_SOFT) {
              float f_strength = eraser->gpencil_settings->era_strength_f / 100.0f;
              float f_thickness = eraser->gpencil_settings->era_thickness_f / 100.0f;
              float influence = 0.0f;

              if (pt0) {
                influence = gpencil_stroke_eraser_calc_influence(p, mval, radius, pc0);
                pt0->strength -= influence * strength * f_strength * 0.5f;
                CLAMP_MIN(pt0->strength, 0.0f);
                pt0->pressure -= influence * strength * f_thickness * 0.5f;
              }

              influence = gpencil_stroke_eraser_calc_influence(p, mval, radius, pc1);
              pt1->strength -= influence * strength * f_strength;
              CLAMP_MIN(pt1->strength, 0.0f);
              pt1->pressure -= influence * strength * f_thickness;

              influence = gpencil_stroke_eraser_calc_influence(p, mval, radius, pc2);
              pt2->strength -= influence * strength * f_strength * 0.5f;
              CLAMP_MIN(pt2->strength, 0.0f);
              pt2->pressure -= influence * strength * f_thickness * 0.5f;

              /* if invisible, delete point */
              if ((pt0) && ((pt0->strength <= GPENCIL_ALPHA_OPACITY_THRESH) ||
                            (pt0->pressure < cull_thresh))) {
                pt0->flag |= GP_SPOINT_TAG;
                do_cull = true;
              }
              if ((pt1->strength <= GPENCIL_ALPHA_OPACITY_THRESH) ||
                  (pt1->pressure < cull_thresh)) {
                pt1->flag |= GP_SPOINT_TAG;
                do_cull = true;
              }
              if ((pt2->strength <= GPENCIL_ALPHA_OPACITY_THRESH) ||
                  (pt2->pressure < cull_thresh)) {
                pt2->flag |= GP_SPOINT_TAG;
                do_cull = true;
              }

              inf1 = 1.0f;
              inf2 = 1.0f;
            }
            else {
              /* Erase point. Only erase if the eraser is on top of the point. */
              inf1 = gpencil_stroke_eraser_calc_influence(p, mval, radius, pc1);
              if (inf1 > 0.0f) {
                pt1->pressure = 0.0f;
                pt1->flag |= GP_SPOINT_TAG;
                do_cull = true;
              }
              inf2 = gpencil_stroke_eraser_calc_influence(p, mval, radius, pc2);
              if (inf2 > 0.0f) {
                pt2->pressure = 0.0f;
                pt2->flag |= GP_SPOINT_TAG;
                do_cull = true;
              }
            }

            /* 2) Tag any point with overly low influence for removal in the next pass */
            if ((inf1 > 0.0f) &&
                ((pt1->pressure < cull_thresh) || (p->flags & GP_PAINTFLAG_HARD_ERASER) ||
                 (eraser->gpencil_settings->eraser_mode == GP_BRUSH_ERASER_HARD))) {
              pt1->flag |= GP_SPOINT_TAG;
              do_cull = true;
            }
            if ((inf1 > 2.0f) &&
                ((pt2->pressure < cull_thresh) || (p->flags & GP_PAINTFLAG_HARD_ERASER) ||
                 (eraser->gpencil_settings->eraser_mode == GP_BRUSH_ERASER_HARD))) {
              pt2->flag |= GP_SPOINT_TAG;
              do_cull = true;
            }
          }
        }
      }
    }

    /* Second Pass: Remove any points that are tagged */
    if (do_cull) {
      /* if soft eraser, must analyze points to be sure the stroke ends
       * don't get rounded */
      if (eraser->gpencil_settings->eraser_mode == GP_BRUSH_ERASER_SOFT) {
        gpencil_stroke_soft_refine(gps);
      }

      BKE_gpencil_stroke_delete_tagged_points(
          p->gpd, gpf, gps, gps->next, GP_SPOINT_TAG, false, false, 0);
    }
    gpencil_update_cache(p->gpd);
  }
}

/* erase strokes which fall under the eraser strokes */
static void gpencil_stroke_doeraser(tGPsdata *p)
{
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(p->gpd);

  rcti rect;
  Brush *brush = p->brush;
  Brush *eraser = p->eraser;
  bool use_pressure = false;
  float press = 1.0f;
  BrushGpencilSettings *gp_settings = NULL;

  /* detect if use pressure in eraser */
  if (brush->gpencil_tool == GPAINT_TOOL_ERASE) {
    use_pressure = (bool)(brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE);
    gp_settings = brush->gpencil_settings;
  }
  else if ((eraser != NULL) & (eraser->gpencil_tool == GPAINT_TOOL_ERASE)) {
    use_pressure = (bool)(eraser->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE);
    gp_settings = eraser->gpencil_settings;
  }
  if (use_pressure) {
    press = p->pressure;
    CLAMP(press, 0.01f, 1.0f);
  }
  /* rect is rectangle of eraser */
  const int calc_radius = (int)p->radius * press;
  rect.xmin = p->mval[0] - calc_radius;
  rect.ymin = p->mval[1] - calc_radius;
  rect.xmax = p->mval[0] + calc_radius;
  rect.ymax = p->mval[1] + calc_radius;

  if ((gp_settings != NULL) && (gp_settings->flag & GP_BRUSH_OCCLUDE_ERASER)) {
    View3D *v3d = p->area->spacedata.first;
    view3d_region_operator_needs_opengl(p->win, p->region);
    ED_view3d_depth_override(p->depsgraph, p->region, v3d, NULL, V3D_DEPTH_NO_GPENCIL, &p->depths);
  }

  /* loop over all layers too, since while it's easy to restrict editing to
   * only a subset of layers, it is harder to perform the same erase operation
   * on multiple layers...
   */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &p->gpd->layers) {
    /* only affect layer if it's editable (and visible) */
    if (BKE_gpencil_layer_is_editable(gpl) == false) {
      continue;
    }

    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
    if (init_gpf == NULL) {
      continue;
    }

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }
        /* calculate difference matrix */
        BKE_gpencil_layer_transform_matrix_get(p->depsgraph, p->ob, gpl, p->diff_mat);

        /* loop over strokes, checking segments for intersections */
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
          /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(p->ob, gpl, gps) == false) {
            continue;
          }

          /* Check if the stroke collide with mouse. */
          if (!ED_gpencil_stroke_check_collision(
                  &p->gsc, gps, p->mval, calc_radius, p->diff_mat)) {
            continue;
          }

          /* Not all strokes in the datablock may be valid in the current editor/context
           * (e.g. 2D space strokes in the 3D view, if the same datablock is shared)
           */
          if (ED_gpencil_stroke_can_use_direct(p->area, gps)) {
            gpencil_stroke_eraser_dostroke(p, gpl, gpf, gps, p->mval, calc_radius, &rect);
          }
        }

        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
}
/* ******************************************* */
/* Sketching Operator */

/* clear the session buffers (call this before AND after a paint operation) */
static void gpencil_session_validatebuffer(tGPsdata *p)
{
  bGPdata *gpd = p->gpd;
  Brush *brush = p->brush;

  /* clear memory of buffer (or allocate it if starting a new session) */
  gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, true);

  /* reset flags */
  gpd->runtime.sbuffer_sflag = 0;

  /* reset inittime */
  p->inittime = 0.0;

  /* reset lazy */
  if (brush) {
    brush->gpencil_settings->flag &= ~GP_BRUSH_STABILIZE_MOUSE_TEMP;
  }
}

/* helper to get default eraser and create one if no eraser brush */
static Brush *gpencil_get_default_eraser(Main *bmain, ToolSettings *ts)
{
  Brush *brush_dft = NULL;
  Paint *paint = &ts->gp_paint->paint;
  Brush *brush_prev = paint->brush;
  for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
    if (brush->gpencil_settings == NULL) {
      continue;
    }
    if ((brush->ob_mode == OB_MODE_PAINT_GPENCIL) && (brush->gpencil_tool == GPAINT_TOOL_ERASE)) {
      /* save first eraser to use later if no default */
      if (brush_dft == NULL) {
        brush_dft = brush;
      }
      /* found default */
      if (brush->gpencil_settings->flag & GP_BRUSH_DEFAULT_ERASER) {
        return brush;
      }
    }
  }
  /* if no default, but exist eraser brush, return this and set as default */
  if (brush_dft) {
    brush_dft->gpencil_settings->flag |= GP_BRUSH_DEFAULT_ERASER;
    return brush_dft;
  }
  /* create a new soft eraser brush */

  brush_dft = BKE_brush_add_gpencil(bmain, ts, "Soft Eraser", OB_MODE_PAINT_GPENCIL);
  brush_dft->size = 30.0f;
  brush_dft->gpencil_settings->flag |= GP_BRUSH_DEFAULT_ERASER;
  brush_dft->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_SOFT;
  brush_dft->gpencil_tool = GPAINT_TOOL_ERASE;
  brush_dft->gpencil_settings->eraser_mode = GP_BRUSH_ERASER_SOFT;

  /* reset current brush */
  BKE_paint_brush_set(paint, brush_prev);

  return brush_dft;
}

/* helper to set default eraser and disable others */
static void gpencil_set_default_eraser(Main *bmain, Brush *brush_dft)
{
  if (brush_dft == NULL) {
    return;
  }

  for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
    if ((brush->gpencil_settings) && (brush->gpencil_tool == GPAINT_TOOL_ERASE)) {
      if (brush == brush_dft) {
        brush->gpencil_settings->flag |= GP_BRUSH_DEFAULT_ERASER;
      }
      else if (brush->gpencil_settings->flag & GP_BRUSH_DEFAULT_ERASER) {
        brush->gpencil_settings->flag &= ~GP_BRUSH_DEFAULT_ERASER;
      }
    }
  }
}

/* initialize a drawing brush */
static void gpencil_init_drawing_brush(bContext *C, tGPsdata *p)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  Paint *paint = &ts->gp_paint->paint;
  bool changed = false;
  /* if not exist, create a new one */
  if ((paint->brush == NULL) || (paint->brush->gpencil_settings == NULL)) {
    /* create new brushes */
    BKE_brush_gpencil_paint_presets(bmain, ts, true);
    changed = true;
  }
  /* Be sure curves are initialized. */
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_sensitivity);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_strength);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_jitter);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_rand_pressure);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_rand_strength);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_rand_uv);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_rand_hue);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_rand_saturation);
  BKE_curvemapping_init(paint->brush->gpencil_settings->curve_rand_value);

  /* Assign to temp #tGPsdata */
  p->brush = paint->brush;
  if (paint->brush->gpencil_tool != GPAINT_TOOL_ERASE) {
    p->eraser = gpencil_get_default_eraser(p->bmain, ts);
  }
  else {
    p->eraser = paint->brush;
  }
  /* set new eraser as default */
  gpencil_set_default_eraser(p->bmain, p->eraser);

  /* use radius of eraser */
  p->radius = (short)p->eraser->size;

  /* Need this update to synchronize brush with draw manager. */
  if (changed) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }
}

/* initialize a paint brush and a default color if not exist */
static void gpencil_init_colors(tGPsdata *p)
{
  bGPdata *gpd = p->gpd;
  Brush *brush = p->brush;

  /* use brush material */
  p->material = BKE_gpencil_object_material_ensure_from_active_input_brush(p->bmain, p->ob, brush);

  gpd->runtime.matid = BKE_object_material_slot_find_index(p->ob, p->material);
  gpd->runtime.sbuffer_brush = brush;
}

/* (re)init new painting data */
static bool gpencil_session_initdata(bContext *C, wmOperator *op, tGPsdata *p)
{
  Main *bmain = CTX_data_main(C);
  bGPdata **gpd_ptr = NULL;
  ScrArea *curarea = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *obact = CTX_data_active_object(C);

  /* make sure the active view (at the starting time) is a 3d-view */
  if (curarea == NULL) {
    p->status = GP_STATUS_ERROR;
    return 0;
  }

  /* pass on current scene and window */
  p->C = C;
  p->bmain = CTX_data_main(C);
  p->scene = CTX_data_scene(C);
  p->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  p->win = CTX_wm_window(C);
  p->disable_fill = RNA_boolean_get(op->ptr, "disable_fill");
  p->disable_stabilizer = RNA_boolean_get(op->ptr, "disable_stabilizer");

  unit_m4(p->imat);
  unit_m4(p->mat);

  /* set current area
   * - must verify that region data is 3D-view (and not something else)
   */
  /* CAUTION: If this is the "toolbar", then this will change on the first stroke */
  p->area = curarea;
  p->region = region;
  p->align_flag = &ts->gpencil_v3d_align;

  if (region->regiondata == NULL) {
    p->status = GP_STATUS_ERROR;
    return 0;
  }

  if ((!obact) || (obact->type != OB_GPENCIL)) {
    View3D *v3d = p->area->spacedata.first;
    /* if active object doesn't exist or isn't a GP Object, create one */
    const float *cur = p->scene->cursor.location;

    ushort local_view_bits = 0;
    if (v3d->localvd) {
      local_view_bits = v3d->local_view_uuid;
    }
    /* create new default object */
    obact = ED_gpencil_add_object(C, cur, local_view_bits);
  }
  /* assign object after all checks to be sure we have one active */
  p->ob = obact;
  p->ob_eval = (Object *)DEG_get_evaluated_object(p->depsgraph, p->ob);

  /* get gp-data */
  gpd_ptr = ED_gpencil_data_get_pointers(C, &p->ownerPtr);
  if ((gpd_ptr == NULL) || ED_gpencil_data_owner_is_annotation(&p->ownerPtr)) {
    p->status = GP_STATUS_ERROR;
    return 0;
  }

  /* if no existing GPencil block exists, add one */
  if (*gpd_ptr == NULL) {
    *gpd_ptr = BKE_gpencil_data_addnew(bmain, "GPencil");
  }
  p->gpd = *gpd_ptr;

  /* clear out buffer (stored in gp-data), in case something contaminated it */
  gpencil_session_validatebuffer(p);

  /* set brush and create a new one if null */
  gpencil_init_drawing_brush(C, p);

  /* setup active color */
  /* region where paint was originated */
  int totcol = p->ob->totcol;
  gpencil_init_colors(p);

  /* check whether the material was newly added */
  if (totcol != p->ob->totcol) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_PROPERTIES, NULL);
  }

  /* lock axis (in some modes, disable) */
  if (((*p->align_flag & GP_PROJECT_DEPTH_VIEW) == 0) &&
      ((*p->align_flag & GP_PROJECT_DEPTH_STROKE) == 0)) {
    p->lock_axis = ts->gp_sculpt.lock_axis;
  }
  else {
    p->lock_axis = 0;
  }

  return 1;
}

/* init new painting session */
static tGPsdata *gpencil_session_initpaint(bContext *C, wmOperator *op)
{
  tGPsdata *p = NULL;

  /* Create new context data */
  p = MEM_callocN(sizeof(tGPsdata), "GPencil Drawing Data");

  /* Try to initialize context data
   * WARNING: This may not always succeed (e.g. using GP in an annotation-only context)
   */
  if (gpencil_session_initdata(C, op, p) == 0) {
    /* Invalid state - Exit
     * NOTE: It should be safe to just free the data, since failing context checks should
     * only happen when no data has been allocated.
     */
    MEM_freeN(p);
    return NULL;
  }

  /* Random generator, only init once. */
  uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
  rng_seed ^= POINTER_AS_UINT(p);
  p->rng = BLI_rng_new(rng_seed);

  /* return context data for running paint operator */
  return p;
}

/* cleanup after a painting session */
static void gpencil_session_cleanup(tGPsdata *p)
{
  bGPdata *gpd = (p) ? p->gpd : NULL;

  /* error checking */
  if (gpd == NULL) {
    return;
  }

  /* free stroke buffer */
  if (gpd->runtime.sbuffer) {
    MEM_SAFE_FREE(gpd->runtime.sbuffer);
    gpd->runtime.sbuffer = NULL;
  }

  /* clear flags */
  gpd->runtime.sbuffer_used = 0;
  gpd->runtime.sbuffer_size = 0;
  gpd->runtime.sbuffer_sflag = 0;
  /* This update is required for update-on-write because the sbuffer data is not longer overwritten
   * by a copy-on-write. */
  ED_gpencil_sbuffer_update_eval(gpd, p->ob_eval);
  p->inittime = 0.0;
}

static void gpencil_session_free(tGPsdata *p)
{
  if (p->rng != NULL) {
    BLI_rng_free(p->rng);
  }
  if (p->depths != NULL) {
    ED_view3d_depths_free(p->depths);
  }

  MEM_freeN(p);
}

/* init new stroke */
static void gpencil_paint_initstroke(tGPsdata *p,
                                     eGPencil_PaintModes paintmode,
                                     Depsgraph *depsgraph)
{
  Scene *scene = p->scene;
  ToolSettings *ts = scene->toolsettings;
  bool changed = false;

  /* get active layer (or add a new one if non-existent) */
  p->gpl = BKE_gpencil_layer_active_get(p->gpd);
  if (p->gpl == NULL) {
    p->gpl = BKE_gpencil_layer_addnew(p->gpd, DATA_("GP_Layer"), true, false);
    BKE_gpencil_tag_full_update(p->gpd, NULL, NULL, NULL);
    changed = true;
    if (p->custom_color[3]) {
      copy_v3_v3(p->gpl->color, p->custom_color);
    }
  }

  /* Recalculate layer transform matrix to avoid problems if props are animated. */
  loc_eul_size_to_mat4(p->gpl->layer_mat, p->gpl->location, p->gpl->rotation, p->gpl->scale);
  invert_m4_m4(p->gpl->layer_invmat, p->gpl->layer_mat);

  if ((paintmode != GP_PAINTMODE_ERASER) && (p->gpl->flag & GP_LAYER_LOCKED)) {
    p->status = GP_STATUS_ERROR;
    return;
  }

  /* Eraser mode: If no active strokes, add one or just return. */
  if (paintmode == GP_PAINTMODE_ERASER) {
    /* Eraser mode:
     * 1) Add new frames to all frames that we might touch,
     * 2) Ensure that p->gpf refers to the frame used for the active layer
     *    (to avoid problems with other tools which expect it to exist)
     *
     * This is done only if additive drawing is enabled.
     */
    bool has_layer_to_erase = false;

    LISTBASE_FOREACH (bGPDlayer *, gpl, &p->gpd->layers) {
      /* Skip if layer not editable */
      if (BKE_gpencil_layer_is_editable(gpl) == false) {
        continue;
      }

      if (!IS_AUTOKEY_ON(scene) && (gpl->actframe == NULL)) {
        continue;
      }

      /* Add a new frame if needed (and based off the active frame,
       * as we need some existing strokes to erase)
       *
       * NOTE: We don't add a new frame if there's nothing there now, so
       *       -> If there are no frames at all, don't add one
       *       -> If there are no strokes in that frame, don't add a new empty frame
       */
      if (gpl->actframe && gpl->actframe->strokes.first) {
        short frame_mode = IS_AUTOKEY_ON(scene) ? GP_GETFRAME_ADD_COPY : GP_GETFRAME_USE_PREV;
        gpl->actframe = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, frame_mode);
        has_layer_to_erase = true;
        break;
      }
    }

    /* Ensure this gets set. */
    if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) {
      p->gpf = p->gpl->actframe;
    }

    if (has_layer_to_erase == false) {
      p->status = GP_STATUS_ERROR;
      return;
    }
    /* Ensure this gets set... */
    p->gpf = p->gpl->actframe;
  }
  else {
    /* Drawing Modes - Add a new frame if needed on the active layer */
    short add_frame_mode;

    if (IS_AUTOKEY_ON(scene)) {
      if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) {
        add_frame_mode = GP_GETFRAME_ADD_COPY;
      }
      else {
        add_frame_mode = GP_GETFRAME_ADD_NEW;
      }
    }
    else {
      add_frame_mode = GP_GETFRAME_USE_PREV;
    }

    bool need_tag = p->gpl->actframe == NULL;
    bGPDframe *actframe = p->gpl->actframe;

    p->gpf = BKE_gpencil_layer_frame_get(p->gpl, scene->r.cfra, add_frame_mode);
    /* Only if there wasn't an active frame, need update. */
    if (need_tag) {
      DEG_id_tag_update(&p->gpd->id, ID_RECALC_GEOMETRY);
    }
    if (actframe != p->gpl->actframe) {
      BKE_gpencil_tag_full_update(p->gpd, p->gpl, NULL, NULL);
    }

    if (p->gpf == NULL) {
      p->status = GP_STATUS_ERROR;
      if (!IS_AUTOKEY_ON(scene)) {
        BKE_report(p->reports, RPT_INFO, "No available frame for creating stroke");
      }

      return;
    }
    p->gpf->flag |= GP_FRAME_PAINT;
  }

  /* set 'eraser' for this stroke if using eraser */
  p->paintmode = paintmode;
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    p->gpd->runtime.sbuffer_sflag |= GP_STROKE_ERASER;
  }
  else {
    /* disable eraser flags - so that we can switch modes during a session */
    p->gpd->runtime.sbuffer_sflag &= ~GP_STROKE_ERASER;
  }

  /* set special fill stroke mode */
  if (p->disable_fill == true) {
    p->gpd->runtime.sbuffer_sflag |= GP_STROKE_NOFILL;
  }

  /* set 'initial run' flag, which is only used to denote when a new stroke is starting */
  p->flags |= GP_PAINTFLAG_FIRSTRUN;

  /* when drawing in the camera view, in 2D space, set the subrect */
  p->subrect = NULL;
  if ((*p->align_flag & GP_PROJECT_VIEWSPACE) == 0) {
    View3D *v3d = p->area->spacedata.first;
    RegionView3D *rv3d = p->region->regiondata;

    /* for camera view set the subrect */
    if (rv3d->persp == RV3D_CAMOB) {
      /* no shift */
      ED_view3d_calc_camera_border(
          p->scene, depsgraph, p->region, v3d, rv3d, &p->subrect_data, true);
      p->subrect = &p->subrect_data;
    }
  }

  /* init stroke point space-conversion settings... */
  p->gsc.gpd = p->gpd;
  p->gsc.gpl = p->gpl;

  p->gsc.area = p->area;
  p->gsc.region = p->region;
  p->gsc.v2d = p->v2d;

  p->gsc.subrect_data = p->subrect_data;
  p->gsc.subrect = p->subrect;

  copy_m4_m4(p->gsc.mat, p->mat);

  /* check if points will need to be made in view-aligned space */
  if (*p->align_flag & GP_PROJECT_VIEWSPACE) {
    p->gpd->runtime.sbuffer_sflag |= GP_STROKE_3DSPACE;
  }
  if (!changed) {
    /* Copy the brush to avoid a full tag (very slow). */
    bGPdata *gpd_eval = (bGPdata *)p->ob_eval->data;
    gpd_eval->runtime.sbuffer_brush = p->gpd->runtime.sbuffer_brush;
  }
  else {
    gpencil_update_cache(p->gpd);
  }
}

/* finish off a stroke (clears buffer, but doesn't finish the paint operation) */
static void gpencil_paint_strokeend(tGPsdata *p)
{
  ToolSettings *ts = p->scene->toolsettings;
  const bool is_eraser = (p->gpd->runtime.sbuffer_sflag & GP_STROKE_ERASER) != 0;
  /* for surface sketching, need to set the right OpenGL context stuff so
   * that the conversions will project the values correctly...
   */
  if (gpencil_project_check(p)) {
    View3D *v3d = p->area->spacedata.first;

    /* need to restore the original projection settings before packing up */
    view3d_region_operator_needs_opengl(p->win, p->region);
    ED_view3d_depth_override(p->depsgraph,
                             p->region,
                             v3d,
                             NULL,
                             (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ?
                                 V3D_DEPTH_GPENCIL_ONLY :
                                 V3D_DEPTH_NO_GPENCIL,
                             is_eraser ? NULL : &p->depths);
  }

  /* check if doing eraser or not */
  if (!is_eraser) {
    /* transfer stroke to frame */
    gpencil_stroke_newfrombuffer(p);
  }

  /* clean up buffer now */
  gpencil_session_validatebuffer(p);
}

/* finish off stroke painting operation */
static void gpencil_paint_cleanup(tGPsdata *p)
{
  /* p->gpd==NULL happens when stroke failed to initialize,
   * for example when GP is hidden in current space (sergey)
   */
  if (p->gpd) {
    /* finish off a stroke */
    gpencil_paint_strokeend(p);
  }

  /* "unlock" frame */
  if (p->gpf) {
    p->gpf->flag &= ~GP_FRAME_PAINT;
  }
}
/* ------------------------------- */

/* Helper callback for drawing the cursor itself */
static void gpencil_draw_eraser(bContext *UNUSED(C), int x, int y, void *p_ptr)
{
  tGPsdata *p = (tGPsdata *)p_ptr;

  if (p->paintmode == GP_PAINTMODE_ERASER) {
    GPUVertFormat *format = immVertexFormat();
    const uint shdr_pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    immUniformColor4ub(255, 100, 100, 20);
    imm_draw_circle_fill_2d(shdr_pos, x, y, p->radius, 40);

    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

    immUniformColor4f(1.0f, 0.39f, 0.39f, 0.78f);
    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniform1f("dash_width", 12.0f);
    immUniform1f("udash_factor", 0.5f);

    imm_draw_circle_wire_2d(shdr_pos,
                            x,
                            y,
                            p->radius,
                            /* XXX Dashed shader gives bad results with sets of small segments
                             * currently, temp hack around the issue. :( */
                            max_ii(8, p->radius / 2)); /* was fixed 40 */

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

/* Turn brush cursor in 3D view on/off */
static void gpencil_draw_toggle_eraser_cursor(tGPsdata *p, short enable)
{
  if (p->erasercursor && !enable) {
    /* clear cursor */
    WM_paint_cursor_end(p->erasercursor);
    p->erasercursor = NULL;
  }
  else if (enable && !p->erasercursor) {
    ED_gpencil_toggle_brush_cursor(p->C, false, NULL);
    /* enable cursor */
    p->erasercursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                               RGN_TYPE_ANY,
                                               NULL, /* XXX */
                                               gpencil_draw_eraser,
                                               p);
  }
}

/* Check if tablet eraser is being used (when processing events) */
static bool gpencil_is_tablet_eraser_active(const wmEvent *event)
{
  return (event->tablet.active == EVT_TABLET_ERASER);
}

/* ------------------------------- */

static void gpencil_draw_exit(bContext *C, wmOperator *op)
{
  tGPsdata *p = op->customdata;

  /* don't assume that operator data exists at all */
  if (p) {
    /* check size of buffer before cleanup, to determine if anything happened here */
    if (p->paintmode == GP_PAINTMODE_ERASER) {
      /* turn off radial brush cursor */
      gpencil_draw_toggle_eraser_cursor(p, false);
    }

    /* always store the new eraser size to be used again next time
     * NOTE: Do this even when not in eraser mode, as eraser may
     *       have been toggled at some point.
     */
    if (p->eraser) {
      p->eraser->size = p->radius;
    }

    /* drawing batch cache is dirty now */
    bGPdata *gpd = CTX_data_gpencil_data(C);
    gpencil_update_cache(gpd);

    /* clear undo stack */
    gpencil_undo_finish();

    /* cleanup */
    gpencil_paint_cleanup(p);
    gpencil_session_cleanup(p);
    ED_gpencil_toggle_brush_cursor(C, true, NULL);

    /* finally, free the temp data */
    gpencil_session_free(p);
    p = NULL;
  }

  op->customdata = NULL;
}

static void gpencil_draw_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  gpencil_draw_exit(C, op);
}

/* ------------------------------- */

static int gpencil_draw_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPsdata *p;
  eGPencil_PaintModes paintmode = RNA_enum_get(op->ptr, "mode");
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);

  /* if mode is draw and the brush is eraser, cancel */
  if (paintmode != GP_PAINTMODE_ERASER) {
    if ((brush) && (brush->gpencil_tool == GPAINT_TOOL_ERASE)) {
      return 0;
    }
  }

  /* check context */
  p = op->customdata = gpencil_session_initpaint(C, op);
  if ((p == NULL) || (p->status == GP_STATUS_ERROR)) {
    /* something wasn't set correctly in context */
    gpencil_draw_exit(C, op);
    return 0;
  }

  p->reports = op->reports;

  /* init painting data */
  gpencil_paint_initstroke(p, paintmode, CTX_data_ensure_evaluated_depsgraph(C));
  if (p->status == GP_STATUS_ERROR) {
    gpencil_draw_exit(C, op);
    return 0;
  }

  if (event != NULL) {
    p->keymodifier = event->keymodifier;
  }
  else {
    p->keymodifier = -1;
  }

  /* everything is now setup ok */
  return 1;
}

/* ------------------------------- */

/* update UI indicators of status, including cursor and header prints */
static void gpencil_draw_status_indicators(bContext *C, tGPsdata *p)
{
  /* header prints */
  switch (p->status) {
    case GP_STATUS_IDLING: {
      /* print status info */
      switch (p->paintmode) {
        case GP_PAINTMODE_ERASER: {
          ED_workspace_status_text(
              C,
              TIP_("Grease Pencil Erase Session: Hold and drag LMB or RMB to erase | "
                   "ESC/Enter to end  (or click outside this area)"));
          break;
        }
        case GP_PAINTMODE_DRAW_STRAIGHT: {
          ED_workspace_status_text(C,
                                   TIP_("Grease Pencil Line Session: Hold and drag LMB to draw | "
                                        "ESC/Enter to end  (or click outside this area)"));
          break;
        }
        case GP_PAINTMODE_SET_CP: {
          ED_workspace_status_text(
              C,
              TIP_("Grease Pencil Guides: LMB click and release to place reference point | "
                   "Esc/RMB to cancel"));
          break;
        }
        case GP_PAINTMODE_DRAW: {
          GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
          if (guide->use_guide) {
            ED_workspace_status_text(
                C,
                TIP_("Grease Pencil Freehand Session: Hold and drag LMB to draw | "
                     "M key to flip guide | O key to move reference point"));
          }
          else {
            ED_workspace_status_text(
                C, TIP_("Grease Pencil Freehand Session: Hold and drag LMB to draw"));
          }
          break;
        }
        default: /* unhandled future cases */
        {
          ED_workspace_status_text(
              C, TIP_("Grease Pencil Session: ESC/Enter to end (or click outside this area)"));
          break;
        }
      }
      break;
    }
    case GP_STATUS_ERROR:
    case GP_STATUS_DONE: {
      /* clear status string */
      ED_workspace_status_text(C, NULL);
      break;
    }
    case GP_STATUS_PAINTING:
      break;
  }
}

/* ------------------------------- */

/* Helper to rotate point around origin */
static void gpencil_rotate_v2_v2v2fl(float v[2],
                                     const float p[2],
                                     const float origin[2],
                                     const float angle)
{
  float pt[2];
  float r[2];
  sub_v2_v2v2(pt, p, origin);
  rotate_v2_v2fl(r, pt, angle);
  add_v2_v2v2(v, r, origin);
}

/* Helper to snap value to grid */
static float gpencil_snap_to_grid_fl(float v, const float offset, const float spacing)
{
  if (spacing > 0.0f) {
    v -= spacing * 0.5f;
    v -= offset;
    v = roundf((v + spacing * 0.5f) / spacing) * spacing;
    v += offset;
  }
  return v;
}

/* Helper to snap value to grid */
static void gpencil_snap_to_rotated_grid_fl(float v[2],
                                            const float origin[2],
                                            const float spacing,
                                            const float angle)
{
  gpencil_rotate_v2_v2v2fl(v, v, origin, -angle);
  v[1] = gpencil_snap_to_grid_fl(v[1], origin[1], spacing);
  gpencil_rotate_v2_v2v2fl(v, v, origin, angle);
}

/* get reference point - screen coords to buffer coords */
static void gpencil_origin_set(wmOperator *op, const int mval[2])
{
  tGPsdata *p = op->customdata;
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  float origin[2];
  float point[3];
  copy_v2fl_v2i(origin, mval);
  gpencil_stroke_convertcoords(p, origin, point, NULL);
  if (guide->reference_point == GP_GUIDE_REF_CUSTOM) {
    copy_v3_v3(guide->location, point);
  }
  else if (guide->reference_point == GP_GUIDE_REF_CURSOR) {
    copy_v3_v3(p->scene->cursor.location, point);
  }
}

/* get reference point - buffer coords to screen coords */
static void gpencil_origin_get(tGPsdata *p, float origin[2])
{
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  float location[3];
  if (guide->reference_point == GP_GUIDE_REF_CUSTOM) {
    copy_v3_v3(location, guide->location);
  }
  else if (guide->reference_point == GP_GUIDE_REF_OBJECT && guide->reference_object != NULL) {
    copy_v3_v3(location, guide->reference_object->loc);
  }
  else {
    copy_v3_v3(location, p->scene->cursor.location);
  }
  GP_SpaceConversion *gsc = &p->gsc;
  gpencil_point_3d_to_xy(gsc, p->gpd->runtime.sbuffer_sflag, location, origin);
}

/* speed guide initial values */
static void gpencil_speed_guide_init(tGPsdata *p, GP_Sculpt_Guide *guide)
{
  /* calculate initial guide values */
  RegionView3D *rv3d = p->region->regiondata;
  float scale = 1.0f;
  if (rv3d->is_persp) {
    float vec[3];
    gpencil_get_3d_reference(p, vec);
    mul_m4_v3(rv3d->persmat, vec);
    scale = vec[2] * rv3d->pixsize;
  }
  else {
    scale = rv3d->pixsize;
  }
  p->guide.spacing = guide->spacing / scale;
  p->guide.half_spacing = p->guide.spacing * 0.5f;
  gpencil_origin_get(p, p->guide.origin);

  /* reference for angled snap */
  copy_v2_v2(p->guide.unit, p->mvali);
  p->guide.unit[0] += 1.0f;

  float xy[2];
  sub_v2_v2v2(xy, p->mvali, p->guide.origin);
  p->guide.origin_angle = atan2f(xy[1], xy[0]) + (M_PI * 2.0f);

  p->guide.origin_distance = len_v2v2(p->mvali, p->guide.origin);
  if (guide->use_snapping && (guide->spacing > 0.0f)) {
    p->guide.origin_distance = gpencil_snap_to_grid_fl(
        p->guide.origin_distance, 0.0f, p->guide.spacing);
  }

  if (ELEM(guide->type, GP_GUIDE_RADIAL)) {
    float angle;
    float half_angle = guide->angle_snap * 0.5f;
    angle = p->guide.origin_angle + guide->angle;
    angle = fmodf(angle + half_angle, guide->angle_snap);
    angle -= half_angle;
    gpencil_rotate_v2_v2v2fl(p->guide.rot_point, p->mvali, p->guide.origin, -angle);
  }
  else {
    gpencil_rotate_v2_v2v2fl(p->guide.rot_point, p->guide.unit, p->mvali, guide->angle);
  }
}

/* apply speed guide */
static void gpencil_snap_to_guide(const tGPsdata *p, const GP_Sculpt_Guide *guide, float point[2])
{
  switch (guide->type) {
    default:
    case GP_GUIDE_CIRCULAR: {
      dist_ensure_v2_v2fl(point, p->guide.origin, p->guide.origin_distance);
      break;
    }
    case GP_GUIDE_RADIAL: {
      if (guide->use_snapping && (guide->angle_snap > 0.0f)) {
        closest_to_line_v2(point, point, p->guide.rot_point, p->guide.origin);
      }
      else {
        closest_to_line_v2(point, point, p->mvali, p->guide.origin);
      }
      break;
    }
    case GP_GUIDE_PARALLEL: {
      closest_to_line_v2(point, point, p->mvali, p->guide.rot_point);
      if (guide->use_snapping && (guide->spacing > 0.0f)) {
        gpencil_snap_to_rotated_grid_fl(point, p->guide.origin, p->guide.spacing, guide->angle);
      }
      break;
    }
    case GP_GUIDE_ISO: {
      closest_to_line_v2(point, point, p->mvali, p->guide.rot_point);
      if (guide->use_snapping && (guide->spacing > 0.0f)) {
        gpencil_snap_to_rotated_grid_fl(
            point, p->guide.origin, p->guide.spacing, p->guide.rot_angle);
      }
      break;
    }
    case GP_GUIDE_GRID: {
      if (guide->use_snapping && (guide->spacing > 0.0f)) {
        closest_to_line_v2(point, point, p->mvali, p->guide.rot_point);
        if (p->straight == STROKE_HORIZONTAL) {
          point[1] = gpencil_snap_to_grid_fl(point[1], p->guide.origin[1], p->guide.spacing);
        }
        else {
          point[0] = gpencil_snap_to_grid_fl(point[0], p->guide.origin[0], p->guide.spacing);
        }
      }
      else if (p->straight == STROKE_HORIZONTAL) {
        point[1] = p->mvali[1]; /* replace y */
      }
      else {
        point[0] = p->mvali[0]; /* replace x */
      }
      break;
    }
  }
}

/* create a new stroke point at the point indicated by the painting context */
static void gpencil_draw_apply(bContext *C, wmOperator *op, tGPsdata *p, Depsgraph *depsgraph)
{
  bGPdata *gpd = p->gpd;
  tGPspoint *pt = NULL;

  /* handle drawing/erasing -> test for erasing first */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    /* do 'live' erasing now */
    gpencil_stroke_doeraser(p);

    /* store used values */
    copy_v2_v2(p->mvalo, p->mval);
    p->opressure = p->pressure;
  }
  /* Only add current point to buffer if mouse moved
   * (even though we got an event, it might be just noise). */
  else if (gpencil_stroke_filtermval(p, p->mval, p->mvalo)) {

    /* if lazy mouse, interpolate the last and current mouse positions */
    if (GPENCIL_LAZY_MODE(p->brush, p->shift) && (!p->disable_stabilizer)) {
      float now_mouse[2];
      float last_mouse[2];
      copy_v2_v2(now_mouse, p->mval);
      copy_v2_v2(last_mouse, p->mvalo);
      interp_v2_v2v2(now_mouse, now_mouse, last_mouse, p->brush->smooth_stroke_factor);
      copy_v2_v2(p->mval, now_mouse);

      GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
      bool is_speed_guide = ((guide->use_guide) &&
                             (p->brush && (p->brush->gpencil_tool == GPAINT_TOOL_DRAW)));
      if (is_speed_guide) {
        gpencil_snap_to_guide(p, guide, p->mval);
      }
    }

    /* try to add point */
    short ok = gpencil_stroke_addpoint(p, p->mval, p->pressure, p->curtime);

    /* handle errors while adding point */
    if (ELEM(ok, GP_STROKEADD_FULL, GP_STROKEADD_OVERFLOW)) {
      /* finish off old stroke */
      gpencil_paint_strokeend(p);
      /* And start a new one!!! Else, projection errors! */
      gpencil_paint_initstroke(p, p->paintmode, depsgraph);

      /* start a new stroke, starting from previous point */
      /* XXX Must manually reset inittime... */
      /* XXX We only need to reuse previous point if overflow! */
      if (ok == GP_STROKEADD_OVERFLOW) {
        p->inittime = p->ocurtime;
        gpencil_stroke_addpoint(p, p->mvalo, p->opressure, p->ocurtime);
      }
      else {
        p->inittime = p->curtime;
      }
      gpencil_stroke_addpoint(p, p->mval, p->pressure, p->curtime);
    }
    else if (ok == GP_STROKEADD_INVALID) {
      /* the painting operation cannot continue... */
      BKE_report(op->reports, RPT_ERROR, "Cannot paint stroke");
      p->status = GP_STATUS_ERROR;

      return;
    }

    /* store used values */
    copy_v2_v2(p->mvalo, p->mval);
    p->opressure = p->pressure;
    p->ocurtime = p->curtime;

    pt = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
    if (p->paintmode != GP_PAINTMODE_ERASER) {
      ED_gpencil_toggle_brush_cursor(C, true, pt->m_xy);
    }
  }
  else if ((p->brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) &&
           (gpd->runtime.sbuffer_used > 0)) {
    pt = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
    if (p->paintmode != GP_PAINTMODE_ERASER) {
      ED_gpencil_toggle_brush_cursor(C, true, pt->m_xy);
    }
  }
}

/* handle draw event */
static void gpencil_draw_apply_event(bContext *C,
                                     wmOperator *op,
                                     const wmEvent *event,
                                     Depsgraph *depsgraph)
{
  tGPsdata *p = op->customdata;
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  PointerRNA itemptr;
  float mousef[2];
  bool is_speed_guide = ((guide->use_guide) &&
                         (p->brush && (p->brush->gpencil_tool == GPAINT_TOOL_DRAW)));

  /* convert from window-space to area-space mouse coordinates
   * add any x,y override position
   */
  copy_v2fl_v2i(p->mval, event->mval);
  p->shift = (event->modifier & KM_SHIFT) != 0;

  /* verify direction for straight lines and guides */
  if ((is_speed_guide) ||
      ((event->modifier & KM_ALT) && (RNA_boolean_get(op->ptr, "disable_straight") == false))) {
    if (p->straight == 0) {
      int dx = (int)fabsf(p->mval[0] - p->mvali[0]);
      int dy = (int)fabsf(p->mval[1] - p->mvali[1]);
      if ((dx > 0) || (dy > 0)) {
        /* store mouse direction */
        if (dx > dy) {
          p->straight = STROKE_HORIZONTAL;
        }
        else if (dx < dy) {
          p->straight = STROKE_VERTICAL;
        }
      }
      /* reset if a stroke angle is required */
      if ((p->flags & GP_PAINTFLAG_REQ_VECTOR) && ((dx == 0) || (dy == 0))) {
        p->straight = 0;
      }
    }
  }

  p->curtime = PIL_check_seconds_timer();

  /* handle pressure sensitivity (which is supplied by tablets or otherwise 1.0) */
  p->pressure = event->tablet.pressure;
  /* By default use pen pressure for random curves but attenuated. */
  p->random_settings.pen_press = pow(p->pressure, 3.0f);

  /* Hack for pressure sensitive eraser on D+RMB when using a tablet:
   * The pen has to float over the tablet surface, resulting in
   * zero pressure (#47101). Ignore pressure values if floating
   * (i.e. "effectively zero" pressure), and only when the "active"
   * end is the stylus (i.e. the default when not eraser)
   */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    if ((event->tablet.active != EVT_TABLET_ERASER) && (p->pressure < 0.001f)) {
      p->pressure = 1.0f;
    }
  }

  /* special eraser modes */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    if (event->modifier & KM_SHIFT) {
      p->flags |= GP_PAINTFLAG_HARD_ERASER;
    }
    else {
      p->flags &= ~GP_PAINTFLAG_HARD_ERASER;
    }
    if (event->modifier & KM_ALT) {
      p->flags |= GP_PAINTFLAG_STROKE_ERASER;
    }
    else {
      p->flags &= ~GP_PAINTFLAG_STROKE_ERASER;
    }
  }

  /* special exception for start of strokes (i.e. maybe for just a dot) */
  if (p->flags & GP_PAINTFLAG_FIRSTRUN) {

    /* special exception here for too high pressure values on first touch in
     * windows for some tablets, then we just skip first touch...
     */
    if ((event->tablet.active != EVT_TABLET_NONE) && (p->pressure >= 0.99f)) {
      return;
    }

    p->flags &= ~GP_PAINTFLAG_FIRSTRUN;

    /* set values */
    p->opressure = p->pressure;
    p->inittime = p->ocurtime = p->curtime;
    p->straight = 0;

    /* save initial mouse */
    copy_v2_v2(p->mvalo, p->mval);
    copy_v2_v2(p->mvali, p->mval);

    if (is_speed_guide && !ELEM(p->paintmode, GP_PAINTMODE_ERASER, GP_PAINTMODE_SET_CP) &&
        ((guide->use_snapping && (guide->type == GP_GUIDE_GRID)) ||
         (guide->type == GP_GUIDE_ISO))) {
      p->flags |= GP_PAINTFLAG_REQ_VECTOR;
    }

    /* calculate initial guide values */
    if (is_speed_guide) {
      gpencil_speed_guide_init(p, guide);
    }
  }

  /* wait for vector then add initial point */
  if (is_speed_guide && (p->flags & GP_PAINTFLAG_REQ_VECTOR)) {
    if (p->straight == 0) {
      return;
    }

    p->flags &= ~GP_PAINTFLAG_REQ_VECTOR;

    /* get initial point */
    float pt[2];
    sub_v2_v2v2(pt, p->mval, p->mvali);

    /* get stroke angle for grids */
    if (ELEM(guide->type, GP_GUIDE_ISO)) {
      p->guide.stroke_angle = atan2f(pt[1], pt[0]);
      /* determine iso angle, less weight is given for vertical strokes */
      if (((p->guide.stroke_angle >= 0.0f) && (p->guide.stroke_angle < DEG2RAD(75))) ||
          (p->guide.stroke_angle < DEG2RAD(-105))) {
        p->guide.rot_angle = guide->angle;
      }
      else if (((p->guide.stroke_angle < 0.0f) && (p->guide.stroke_angle > DEG2RAD(-75))) ||
               (p->guide.stroke_angle > DEG2RAD(105))) {
        p->guide.rot_angle = -guide->angle;
      }
      else {
        p->guide.rot_angle = DEG2RAD(90);
      }
      gpencil_rotate_v2_v2v2fl(p->guide.rot_point, p->guide.unit, p->mvali, p->guide.rot_angle);
    }
    else if (ELEM(guide->type, GP_GUIDE_GRID)) {
      gpencil_rotate_v2_v2v2fl(p->guide.rot_point,
                               p->guide.unit,
                               p->mvali,
                               (p->straight == STROKE_VERTICAL) ? M_PI_2 : 0.0f);
    }
  }

  /* check if stroke is straight or guided */
  if ((p->paintmode != GP_PAINTMODE_ERASER) && ((p->straight) || (is_speed_guide))) {
    /* guided stroke */
    if (is_speed_guide) {
      gpencil_snap_to_guide(p, guide, p->mval);
    }
    else if (p->straight == STROKE_HORIZONTAL) {
      p->mval[1] = p->mvali[1]; /* replace y */
    }
    else {
      p->mval[0] = p->mvali[0]; /* replace x */
    }
  }

  /* fill in stroke data (not actually used directly by gpencil_draw_apply) */
  RNA_collection_add(op->ptr, "stroke", &itemptr);

  mousef[0] = p->mval[0];
  mousef[1] = p->mval[1];
  RNA_float_set_array(&itemptr, "mouse", mousef);
  RNA_float_set(&itemptr, "pressure", p->pressure);
  RNA_boolean_set(&itemptr, "is_start", (p->flags & GP_PAINTFLAG_FIRSTRUN) != 0);

  RNA_float_set(&itemptr, "time", p->curtime - p->inittime);

  /* apply the current latest drawing point */
  gpencil_draw_apply(C, op, p, depsgraph);

  /* force refresh */
  /* just active area for now, since doing whole screen is too slow */
  ED_region_tag_redraw(p->region);
}

/* ------------------------------- */

/* operator 'redo' (i.e. after changing some properties, but also for repeat last) */
static int gpencil_draw_exec(bContext *C, wmOperator *op)
{
  tGPsdata *p = NULL;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* try to initialize context data needed while drawing */
  if (!gpencil_draw_init(C, op, NULL)) {
    MEM_SAFE_FREE(op->customdata);
    return OPERATOR_CANCELLED;
  }

  p = op->customdata;

  /* loop over the stroke RNA elements recorded (i.e. progress of mouse movement),
   * setting the relevant values in context at each step, then applying
   */
  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    float mousef[2];

    /* get relevant data for this point from stroke */
    RNA_float_get_array(&itemptr, "mouse", mousef);
    p->mval[0] = mousef[0];
    p->mval[1] = mousef[1];
    p->pressure = RNA_float_get(&itemptr, "pressure");
    p->curtime = (double)RNA_float_get(&itemptr, "time") + p->inittime;

    if (RNA_boolean_get(&itemptr, "is_start")) {
      /* if first-run flag isn't set already (i.e. not true first stroke),
       * then we must terminate the previous one first before continuing
       */
      if ((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) {
        /* TODO: both of these ops can set error-status, but we probably don't need to worry */
        gpencil_paint_strokeend(p);
        gpencil_paint_initstroke(p, p->paintmode, depsgraph);
      }
    }

    /* if first run, set previous data too */
    if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
      p->flags &= ~GP_PAINTFLAG_FIRSTRUN;

      p->mvalo[0] = p->mval[0];
      p->mvalo[1] = p->mval[1];
      p->opressure = p->pressure;
      p->ocurtime = p->curtime;
    }

    /* apply this data as necessary now (as per usual) */
    gpencil_draw_apply(C, op, p, depsgraph);
  }
  RNA_END;

  /* cleanup */
  gpencil_draw_exit(C, op);

  /* refreshes */
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

/* ------------------------------- */

/* handle events for guides */
static void gpencil_guide_event_handling(bContext *C,
                                         wmOperator *op,
                                         const wmEvent *event,
                                         tGPsdata *p)
{
  bool add_notifier = false;
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;

  /* Enter or exit set center point mode */
  if ((event->type == EVT_OKEY) && (event->val == KM_RELEASE)) {
    if ((p->paintmode == GP_PAINTMODE_DRAW) && guide->use_guide &&
        (guide->reference_point != GP_GUIDE_REF_OBJECT)) {
      add_notifier = true;
      p->paintmode = GP_PAINTMODE_SET_CP;
      ED_gpencil_toggle_brush_cursor(C, false, NULL);
    }
  }
  /* Freehand mode, turn off speed guide */
  else if ((event->type == EVT_VKEY) && (event->val == KM_RELEASE)) {
    guide->use_guide = false;
    add_notifier = true;
  }
  /* Alternate or flip direction */
  else if ((event->type == EVT_MKEY) && (event->val == KM_RELEASE)) {
    if (guide->type == GP_GUIDE_CIRCULAR) {
      add_notifier = true;
      guide->type = GP_GUIDE_RADIAL;
    }
    else if (guide->type == GP_GUIDE_RADIAL) {
      add_notifier = true;
      guide->type = GP_GUIDE_CIRCULAR;
    }
    else if (guide->type == GP_GUIDE_PARALLEL) {
      add_notifier = true;
      guide->angle += M_PI_2;
      guide->angle = angle_compat_rad(guide->angle, M_PI);
    }
    else {
      add_notifier = false;
    }
  }
  /* Line guides */
  else if ((event->type == EVT_LKEY) && (event->val == KM_RELEASE)) {
    add_notifier = true;
    guide->use_guide = true;
    if (event->modifier & KM_CTRL) {
      guide->angle = 0.0f;
      guide->type = GP_GUIDE_PARALLEL;
    }
    else if (event->modifier & KM_ALT) {
      guide->type = GP_GUIDE_PARALLEL;
      guide->angle = RNA_float_get(op->ptr, "guide_last_angle");
    }
    else {
      guide->type = GP_GUIDE_PARALLEL;
    }
  }
  /* Point guide */
  else if ((event->type == EVT_CKEY) && (event->val == KM_RELEASE)) {
    add_notifier = true;
    if (!guide->use_guide) {
      guide->use_guide = true;
      guide->type = GP_GUIDE_CIRCULAR;
    }
    else if (guide->type == GP_GUIDE_CIRCULAR) {
      guide->type = GP_GUIDE_RADIAL;
    }
    else if (guide->type == GP_GUIDE_RADIAL) {
      guide->type = GP_GUIDE_CIRCULAR;
    }
    else {
      guide->type = GP_GUIDE_CIRCULAR;
    }
  }
  /* Change line angle. */
  else if (ELEM(event->type, EVT_JKEY, EVT_KKEY) && (event->val == KM_RELEASE)) {
    add_notifier = true;
    float angle = guide->angle;
    float adjust = (float)M_PI / 180.0f;
    if (event->modifier & KM_ALT) {
      adjust *= 45.0f;
    }
    else if ((event->modifier & KM_SHIFT) == 0) {
      adjust *= 15.0f;
    }
    angle += (event->type == EVT_JKEY) ? adjust : -adjust;
    angle = angle_compat_rad(angle, M_PI);
    guide->angle = angle;
  }

  if (add_notifier) {
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS | NC_GPENCIL | NA_EDITED, NULL);
  }
}

/* start of interactive drawing part of operator */
static int gpencil_draw_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPsdata *p = NULL;
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  /* support for tablets eraser pen */
  if (gpencil_is_tablet_eraser_active(event)) {
    RNA_enum_set(op->ptr, "mode", GP_PAINTMODE_ERASER);
  }

  /* do not draw in locked or invisible layers */
  eGPencil_PaintModes paintmode = RNA_enum_get(op->ptr, "mode");
  if (paintmode != GP_PAINTMODE_ERASER) {
    bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
    if ((gpl) && ((gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE))) {
      BKE_report(op->reports, RPT_ERROR, "Active layer is locked or hidden");
      return OPERATOR_CANCELLED;
    }
  }
  else {
    /* don't erase empty frames */
    bool has_layer_to_erase = false;
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      /* Skip if layer not editable */
      if (BKE_gpencil_layer_is_editable(gpl)) {
        if (gpl->actframe && gpl->actframe->strokes.first) {
          has_layer_to_erase = true;
          break;
        }
      }
    }
    if (!has_layer_to_erase) {
      BKE_report(op->reports, RPT_ERROR, "Nothing to erase or all layers locked");
      return OPERATOR_FINISHED;
    }
  }

  /* try to initialize context data needed while drawing */
  if (!gpencil_draw_init(C, op, event)) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    return OPERATOR_CANCELLED;
  }

  p = op->customdata;

  /* Init random settings. */
  ED_gpencil_init_random_settings(p->brush, event->mval, &p->random_settings);

  /* TODO: set any additional settings that we can take from the events?
   * if eraser is on, draw radial aid */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    gpencil_draw_toggle_eraser_cursor(p, true);
  }
  else {
    ED_gpencil_toggle_brush_cursor(C, true, NULL);
  }

  /* only start drawing immediately if we're allowed to do so... */
  if (RNA_boolean_get(op->ptr, "wait_for_input") == false) {
    /* hotkey invoked - start drawing */
    p->status = GP_STATUS_PAINTING;

    /* handle the initial drawing - i.e. for just doing a simple dot */
    gpencil_draw_apply_event(C, op, event, CTX_data_ensure_evaluated_depsgraph(C));
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
  }
  else {
    /* toolbar invoked - don't start drawing yet... */
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
  }

  /* enable paint mode */
  /* handle speed guide events before drawing inside view3d */
  if (!ELEM(p->paintmode, GP_PAINTMODE_ERASER, GP_PAINTMODE_SET_CP)) {
    gpencil_guide_event_handling(C, op, event, p);
  }

  if ((ob->type == OB_GPENCIL) && ((p->gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0)) {
    /* FIXME: use the mode switching operator, this misses notifiers, messages. */
    /* Just set paintmode flag... */
    p->gpd->flag |= GP_DATA_STROKE_PAINTMODE;
    /* disable other GP modes */
    p->gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
    p->gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
    p->gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
    /* set workspace mode */
    ob->restore_mode = ob->mode;
    ob->mode = OB_MODE_PAINT_GPENCIL;
    /* redraw mode on screen */
    WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);
  }

  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* add a modal handler for this operator, so that we can then draw continuous strokes */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* gpencil modal operator stores area, which can be removed while using it (like full-screen). */
static bool gpencil_area_exists(bContext *C, ScrArea *area_test)
{
  bScreen *screen = CTX_wm_screen(C);
  return (BLI_findindex(&screen->areabase, area_test) != -1);
}

static tGPsdata *gpencil_stroke_begin(bContext *C, wmOperator *op)
{
  tGPsdata *p = op->customdata;

  /* we must check that we're still within the area that we're set up to work from
   * otherwise we could crash (see bug #20586)
   */
  if (CTX_wm_area(C) != p->area) {
    printf("\t\t\tGP - wrong area execution abort!\n");
    p->status = GP_STATUS_ERROR;
  }

  /* we may need to set up paint env again if we're resuming */
  if (gpencil_session_initdata(C, op, p)) {
    gpencil_paint_initstroke(p, p->paintmode, CTX_data_depsgraph_pointer(C));
  }

  if (p->status != GP_STATUS_ERROR) {
    p->status = GP_STATUS_PAINTING;
    op->flag &= ~OP_IS_MODAL_CURSOR_REGION;
  }

  return op->customdata;
}

/* Apply pressure change depending of the angle of the stroke for a segment. */
static void gpencil_brush_angle_segment(tGPsdata *p, tGPspoint *pt_prev, tGPspoint *pt)
{
  Brush *brush = p->brush;
  /* Sensitivity. */
  const float sen = brush->gpencil_settings->draw_angle_factor;
  /* Default angle of brush in radians */
  const float angle = brush->gpencil_settings->draw_angle;

  float mvec[2];
  float fac;

  /* angle vector of the brush with full thickness */
  const float v0[2] = {cos(angle), sin(angle)};

  sub_v2_v2v2(mvec, pt->m_xy, pt_prev->m_xy);
  normalize_v2(mvec);
  fac = 1.0f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
  /* interpolate with previous point for smoother transitions */
  pt->pressure = interpf(pt->pressure - (sen * fac), pt_prev->pressure, 0.3f);
  CLAMP(pt->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
}

/* Add arc points between two mouse events using the previous segment to determine the vertex of
 * the arc.
 *        /+ CTL
 *       / |
 *      /  |
 * PtA +...|...+ PtB
 *    /
 *   /
 *  + PtA - 1
 * /
 * CTL is the vertex of the triangle created between PtA and PtB */
static void gpencil_add_arc_points(tGPsdata *p, const float mval[2], int segments)
{
  bGPdata *gpd = p->gpd;
  BrushGpencilSettings *brush_settings = p->brush->gpencil_settings;

  if (gpd->runtime.sbuffer_used < 3) {
    tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
    /* Apply other randomness to first points. */
    for (int i = 0; i < gpd->runtime.sbuffer_used; i++) {
      tGPspoint *pt = &points[i];
      gpencil_apply_randomness(p, brush_settings, pt, false, false, true);
    }
    return;
  }
  int idx_prev = gpd->runtime.sbuffer_used;

  /* Add space for new arc points. */
  gpd->runtime.sbuffer_used += segments - 1;

  /* Check if still room in buffer or add more. */
  gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  tGPspoint *pt = NULL;
  tGPspoint *pt_before = &points[idx_prev - 1]; /* current - 2 */
  tGPspoint *pt_prev = &points[idx_prev - 2];   /* previous */

  /* Create two vectors, previous and half way of the actual to get the vertex of the triangle
   * for arc curve.
   */
  float v_prev[2], v_cur[2], v_half[2];
  sub_v2_v2v2(v_cur, mval, pt_prev->m_xy);

  sub_v2_v2v2(v_prev, pt_prev->m_xy, pt_before->m_xy);
  interp_v2_v2v2(v_half, pt_prev->m_xy, mval, 0.5f);
  sub_v2_v2(v_half, pt_prev->m_xy);

  /* If angle is too sharp undo all changes and return. */
  const float min_angle = DEG2RADF(120.0f);
  float angle = angle_v2v2(v_prev, v_half);
  if (angle < min_angle) {
    gpd->runtime.sbuffer_used -= segments - 1;
    return;
  }

  /* Project the half vector to the previous vector and calculate the mid projected point. */
  float dot = dot_v2v2(v_prev, v_half);
  float l = len_squared_v2(v_prev);
  if (l > 0.0f) {
    mul_v2_fl(v_prev, dot / l);
  }

  /* Calc the position of the control point. */
  float ctl[2];
  add_v2_v2v2(ctl, pt_prev->m_xy, v_prev);

  float step = M_PI_2 / (float)(segments + 1);
  float a = step;

  float midpoint[2], start[2], end[2], cp1[2], corner[2];
  mid_v2_v2v2(midpoint, pt_prev->m_xy, mval);
  copy_v2_v2(start, pt_prev->m_xy);
  copy_v2_v2(end, mval);
  copy_v2_v2(cp1, ctl);

  corner[0] = midpoint[0] - (cp1[0] - midpoint[0]);
  corner[1] = midpoint[1] - (cp1[1] - midpoint[1]);
  float stepcolor = 1.0f / segments;

  tGPspoint *pt_step = pt_prev;
  for (int i = 0; i < segments; i++) {
    pt = &points[idx_prev + i - 1];
    pt->m_xy[0] = corner[0] + (end[0] - corner[0]) * sinf(a) + (start[0] - corner[0]) * cosf(a);
    pt->m_xy[1] = corner[1] + (end[1] - corner[1]) * sinf(a) + (start[1] - corner[1]) * cosf(a);

    /* Set pressure and strength equals to previous. It will be smoothed later. */
    pt->pressure = pt_prev->pressure;
    pt->strength = pt_prev->strength;
    /* Interpolate vertex color. */
    interp_v4_v4v4(
        pt->vert_color, pt_before->vert_color, pt_prev->vert_color, stepcolor * (i + 1));

    /* Apply angle of stroke to brush size to interpolated points but slightly attenuated. */
    if (brush_settings->draw_angle_factor != 0.0f) {
      gpencil_brush_angle_segment(p, pt_step, pt);
      CLAMP(pt->pressure, pt_prev->pressure * 0.5f, 1.0f);
      /* Use the previous interpolated point for next segment. */
      pt_step = pt;
    }

    /* Apply other randomness. */
    gpencil_apply_randomness(p, brush_settings, pt, false, false, true);

    a += step;
  }
}

static void gpencil_add_guide_points(const tGPsdata *p,
                                     const GP_Sculpt_Guide *guide,
                                     const float start[2],
                                     const float end[2],
                                     int segments)
{
  bGPdata *gpd = p->gpd;
  if (gpd->runtime.sbuffer_used == 0) {
    return;
  }

  int idx_prev = gpd->runtime.sbuffer_used;

  /* Add space for new points. */
  gpd->runtime.sbuffer_used += segments - 1;

  /* Check if still room in buffer or add more. */
  gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  tGPspoint *pt = NULL;
  tGPspoint *pt_before = &points[idx_prev - 1];

  /* Use arc sampling for circular guide */
  if (guide->type == GP_GUIDE_CIRCULAR) {
    float cw = cross_tri_v2(start, p->guide.origin, end);
    float angle = angle_v2v2v2(start, p->guide.origin, end);

    float step = angle / (float)(segments + 1);
    if (cw < 0.0f) {
      step = -step;
    }

    float a = step;

    for (int i = 0; i < segments; i++) {
      pt = &points[idx_prev + i - 1];

      gpencil_rotate_v2_v2v2fl(pt->m_xy, start, p->guide.origin, -a);
      gpencil_snap_to_guide(p, guide, pt->m_xy);
      a += step;

      /* Set pressure and strength equals to previous. It will be smoothed later. */
      pt->pressure = pt_before->pressure;
      pt->strength = pt_before->strength;
      copy_v4_v4(pt->vert_color, pt_before->vert_color);
    }
  }
  else {
    float step = 1.0f / (float)(segments + 1);
    float a = step;

    for (int i = 0; i < segments; i++) {
      pt = &points[idx_prev + i - 1];

      interp_v2_v2v2(pt->m_xy, start, end, a);
      gpencil_snap_to_guide(p, guide, pt->m_xy);
      a += step;

      /* Set pressure and strength equals to previous. It will be smoothed later. */
      pt->pressure = pt_before->pressure;
      pt->strength = pt_before->strength;
      copy_v4_v4(pt->vert_color, pt_before->vert_color);
    }
  }
}

/**
 * Add fake points for missing mouse movements when the artist draw very fast creating an arc
 * with the vertex in the middle of the segment and using the angle of the previous segment.
 */
static void gpencil_add_fake_points(const wmEvent *event, tGPsdata *p)
{
  Brush *brush = p->brush;
  /* Lazy mode do not use fake events. */
  if (GPENCIL_LAZY_MODE(brush, p->shift) && (!p->disable_stabilizer)) {
    return;
  }

  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  int input_samples = brush->gpencil_settings->input_samples;
  bool is_speed_guide = ((guide->use_guide) &&
                         (p->brush && (p->brush->gpencil_tool == GPAINT_TOOL_DRAW)));

  /* TODO: ensure sampling enough points when using circular guide,
   * but the arc must be around the center. (see if above to check other guides only). */
  if (is_speed_guide && (guide->type == GP_GUIDE_CIRCULAR)) {
    input_samples = GP_MAX_INPUT_SAMPLES;
  }

  if (input_samples == 0) {
    return;
  }

  int samples = GP_MAX_INPUT_SAMPLES - input_samples + 1;

  float mouse_prv[2], mouse_cur[2];
  float min_dist = 4.0f * samples;

  copy_v2_v2(mouse_prv, p->mvalo);
  copy_v2fl_v2i(mouse_cur, event->mval);

  /* get distance in pixels */
  float dist = len_v2v2(mouse_prv, mouse_cur);

  /* get distance for circular guide */
  if (is_speed_guide && (guide->type == GP_GUIDE_CIRCULAR)) {
    float middle[2];
    gpencil_snap_to_guide(p, guide, mouse_prv);
    gpencil_snap_to_guide(p, guide, mouse_cur);
    mid_v2_v2v2(middle, mouse_cur, mouse_prv);
    gpencil_snap_to_guide(p, guide, middle);
    dist = len_v2v2(mouse_prv, middle) + len_v2v2(middle, mouse_cur);
  }

  if ((dist > 3.0f) && (dist > min_dist)) {
    int slices = (dist / min_dist) + 1;

    if (is_speed_guide) {
      gpencil_add_guide_points(p, guide, mouse_prv, mouse_cur, slices);
    }
    else {
      gpencil_add_arc_points(p, mouse_cur, slices);
    }
  }
}

/* events handling during interactive drawing part of operator */
static int gpencil_draw_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPsdata *p = op->customdata;
  // ToolSettings *ts = CTX_data_tool_settings(C);
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;

  /* Default exit state - pass through to support MMB view navigation, etc. */
  int estate = OPERATOR_PASS_THROUGH;

  /* NOTE(mike erwin): Not quite what I was looking for, but a good start!
   * grease-pencil continues to draw on the screen while the 3D mouse moves the viewpoint.
   * Problem is that the stroke is converted to 3D only after it is finished.
   * This approach should work better in tools that immediately apply in 3D space. */
#if 0
  if (event->type == NDOF_MOTION) {
    return OPERATOR_PASS_THROUGH;
  }
#endif

  if (p->status == GP_STATUS_IDLING) {
    ARegion *region = CTX_wm_region(C);
    p->region = region;
  }

  /* special mode for editing control points */
  if (p->paintmode == GP_PAINTMODE_SET_CP) {
    wmWindow *win = p->win;
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
    bool drawmode = false;

    switch (event->type) {
      /* cancel */
      case EVT_ESCKEY:
      case RIGHTMOUSE: {
        if (ELEM(event->val, KM_RELEASE)) {
          drawmode = true;
        }
        break;
      }
      /* set */
      case LEFTMOUSE: {
        if (ELEM(event->val, KM_RELEASE)) {
          gpencil_origin_set(op, event->mval);
          drawmode = true;
        }
        break;
      }
    }
    if (drawmode) {
      p->status = GP_STATUS_IDLING;
      p->paintmode = GP_PAINTMODE_DRAW;
      WM_cursor_modal_restore(p->win);
      ED_gpencil_toggle_brush_cursor(C, true, NULL);
      DEG_id_tag_update(&p->scene->id, ID_RECALC_COPY_ON_WRITE);
    }
    else {
      return OPERATOR_RUNNING_MODAL;
    }
  }

  /* We don't pass on key events, GP is used with key-modifiers -
   * prevents D-key to insert drivers. */
  if (ISKEYBOARD(event->type)) {
    if (ELEM(event->type, EVT_LEFTARROWKEY, EVT_DOWNARROWKEY, EVT_RIGHTARROWKEY, EVT_UPARROWKEY)) {
      /* allow some keys:
       *   - For frame changing #33412.
       *   - For undo (during sketching sessions).
       */
    }
    else if (event->type == EVT_ZKEY) {
      if (event->modifier & KM_CTRL) {
        p->status = GP_STATUS_DONE;
        estate = OPERATOR_FINISHED;
      }
    }
    else if (ELEM(event->type,
                  EVT_PAD0,
                  EVT_PAD1,
                  EVT_PAD2,
                  EVT_PAD3,
                  EVT_PAD4,
                  EVT_PAD5,
                  EVT_PAD6,
                  EVT_PAD7,
                  EVT_PAD8,
                  EVT_PAD9)) {
      /* Allow numpad keys so that camera/view manipulations can still take place
       * - #EVT_PAD0 in particular is really important for Grease Pencil drawing,
       *   as animators may be working "to camera", so having this working
       *   is essential for ensuring that they can quickly return to that view.
       */
    }
    else if (!ELEM(p->paintmode, GP_PAINTMODE_ERASER, GP_PAINTMODE_SET_CP)) {
      gpencil_guide_event_handling(C, op, event, p);
      estate = OPERATOR_RUNNING_MODAL;
    }
    else {
      estate = OPERATOR_RUNNING_MODAL;
    }
  }

  /* Exit painting mode (and/or end current stroke). */
  if (ELEM(event->type, EVT_RETKEY, EVT_PADENTER, EVT_ESCKEY, EVT_SPACEKEY)) {

    p->status = GP_STATUS_DONE;
    estate = OPERATOR_FINISHED;
  }

  /* toggle painting mode upon mouse-button movement
   * - LEFTMOUSE  = standard drawing (all) / straight line drawing (all)
   * - RIGHTMOUSE = eraser (all)
   *   (Disabling RIGHTMOUSE case here results in bugs like #32647)
   * also making sure we have a valid event value, to not exit too early
   */
  if (ELEM(event->type, LEFTMOUSE, RIGHTMOUSE) && ELEM(event->val, KM_PRESS, KM_RELEASE)) {
    /* if painting, end stroke */
    if (p->status == GP_STATUS_PAINTING) {
      p->status = GP_STATUS_DONE;
      estate = OPERATOR_FINISHED;
    }
    else if (event->val == KM_PRESS) {
      bool in_bounds = false;

      /* Check if we're outside the bounds of the active region
       * NOTE: An exception here is that if launched from the toolbar,
       *       whatever region we're now in should become the new region
       */
      if ((p->region) && (p->region->regiontype == RGN_TYPE_TOOLS)) {
        /* Change to whatever region is now under the mouse */
        ARegion *current_region = BKE_area_find_region_xy(p->area, RGN_TYPE_ANY, event->xy);

        if (current_region) {
          /* Assume that since we found the cursor in here, it is in bounds
           * and that this should be the region that we begin drawing in
           */
          p->region = current_region;
          in_bounds = true;
        }
        else {
          /* Out of bounds, or invalid in some other way */
          p->status = GP_STATUS_ERROR;
          estate = OPERATOR_CANCELLED;
        }
      }
      else if (p->region) {
        /* Perform bounds check using. */
        const rcti *region_rect = ED_region_visible_rect(p->region);
        in_bounds = BLI_rcti_isect_pt_v(region_rect, event->mval);
      }
      else {
        /* No region */
        p->status = GP_STATUS_ERROR;
        estate = OPERATOR_CANCELLED;
      }

      if (in_bounds) {
        /* Switch paintmode (temporarily if need be) based on which button was used
         * NOTE: This is to make it more convenient to erase strokes when using drawing sessions
         */
        if ((event->type == RIGHTMOUSE) || gpencil_is_tablet_eraser_active(event)) {
          /* turn on eraser */
          p->paintmode = GP_PAINTMODE_ERASER;
        }
        else if (event->type == LEFTMOUSE) {
          /* restore drawmode to default */
          p->paintmode = RNA_enum_get(op->ptr, "mode");
        }

        gpencil_draw_toggle_eraser_cursor(p, p->paintmode == GP_PAINTMODE_ERASER);

        /* not painting, so start stroke (this should be mouse-button down) */
        p = gpencil_stroke_begin(C, op);

        if (p->status == GP_STATUS_ERROR) {
          estate = OPERATOR_CANCELLED;
        }
      }
      else if (p->status != GP_STATUS_ERROR) {
        /* User clicked outside bounds of window while idling, so exit paintmode
         * NOTE: Don't enter this case if an error occurred while finding the
         *       region (as above)
         */
        p->status = GP_STATUS_DONE;
        estate = OPERATOR_FINISHED;
      }
    }
    else if (event->val == KM_RELEASE) {
      p->status = GP_STATUS_IDLING;
      op->flag |= OP_IS_MODAL_CURSOR_REGION;
      ED_region_tag_redraw(p->region);
    }
  }

  /* handle mode-specific events */
  if (p->status == GP_STATUS_PAINTING) {
    /* handle painting mouse-movements? */
    if (ISMOUSE_MOTION(event->type) || (p->flags & GP_PAINTFLAG_FIRSTRUN)) {
      /* handle drawing event */
      bool is_speed_guide = ((guide->use_guide) &&
                             (p->brush && (p->brush->gpencil_tool == GPAINT_TOOL_DRAW)));

      int size_before = p->gpd->runtime.sbuffer_used;
      if (((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) && (p->paintmode != GP_PAINTMODE_ERASER) &&
          !(is_speed_guide && (p->flags & GP_PAINTFLAG_REQ_VECTOR))) {
        gpencil_add_fake_points(event, p);
      }

      gpencil_draw_apply_event(C, op, event, CTX_data_depsgraph_pointer(C));
      int size_after = p->gpd->runtime.sbuffer_used;

      /* Smooth segments if some fake points were added (need loop to get cumulative smooth).
       * the 0.15 value gets a good result in Windows and Linux. */
      if (!is_speed_guide && (size_after - size_before > 1)) {
        for (int r = 0; r < 5; r++) {
          gpencil_smooth_segment(p->gpd, 0.15f, size_before - 1, size_after - 1);
        }
      }

      /* finish painting operation if anything went wrong just now */
      if (p->status == GP_STATUS_ERROR) {
        printf("\t\t\t\tGP - add error done!\n");
        estate = OPERATOR_CANCELLED;
      }
      else {
        /* event handled, so just tag as running modal */
        estate = OPERATOR_RUNNING_MODAL;
      }
    }
    /* eraser size */
    else if ((p->paintmode == GP_PAINTMODE_ERASER) &&
             ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE, EVT_PADPLUSKEY, EVT_PADMINUS)) {
      /* Just resize the brush (local version). */
      switch (event->type) {
        case WHEELDOWNMOUSE: /* larger */
        case EVT_PADPLUSKEY:
          p->radius += 5;
          break;

        case WHEELUPMOUSE: /* smaller */
        case EVT_PADMINUS:
          p->radius -= 5;

          if (p->radius <= 0) {
            p->radius = 1;
          }
          break;
      }

      /* force refresh */
      /* just active area for now, since doing whole screen is too slow */
      ED_region_tag_redraw(p->region);

      /* event handled, so just tag as running modal */
      estate = OPERATOR_RUNNING_MODAL;
    }
    /* there shouldn't be any other events, but just in case there are, let's swallow them
     * (i.e. to prevent problems with undo)
     */
    else {
      /* swallow event to save ourselves trouble */
      estate = OPERATOR_RUNNING_MODAL;
    }
  }

  /* gpencil modal operator stores area, which can be removed while using it (like full-screen). */
  if (0 == gpencil_area_exists(C, p->area)) {
    estate = OPERATOR_CANCELLED;
  }
  else {
    /* update status indicators - cursor, header, etc. */
    gpencil_draw_status_indicators(C, p);
  }

  /* process last operations before exiting */
  switch (estate) {
    case OPERATOR_FINISHED:
      /* store stroke angle for parallel guide */
      if ((p->straight == 0) || (guide->use_guide && (guide->type == GP_GUIDE_CIRCULAR))) {
        float xy[2];
        sub_v2_v2v2(xy, p->mval, p->mvali);
        float angle = atan2f(xy[1], xy[0]);
        RNA_float_set(op->ptr, "guide_last_angle", angle);
      }
      /* one last flush before we're done */
      gpencil_draw_exit(C, op);
      WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
      break;

    case OPERATOR_CANCELLED:
      gpencil_draw_exit(C, op);
      break;

    case OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH:
      /* event doesn't need to be handled */
      break;
  }

  /* return status code */
  return estate;
}

/* ------------------------------- */

static const EnumPropertyItem prop_gpencil_drawmodes[] = {
    {GP_PAINTMODE_DRAW, "DRAW", 0, "Draw Freehand", "Draw freehand stroke(s)"},
    {GP_PAINTMODE_DRAW_STRAIGHT,
     "DRAW_STRAIGHT",
     0,
     "Draw Straight Lines",
     "Draw straight line segment(s)"},
    {GP_PAINTMODE_ERASER, "ERASER", 0, "Eraser", "Erase Grease Pencil strokes"},
    {0, NULL, 0, NULL, NULL},
};

void GPENCIL_OT_draw(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Grease Pencil Draw";
  ot->idname = "GPENCIL_OT_draw";
  ot->description = "Draw a new stroke in the active Grease Pencil object";

  /* api callbacks */
  ot->exec = gpencil_draw_exec;
  ot->invoke = gpencil_draw_invoke;
  ot->modal = gpencil_draw_modal;
  ot->cancel = gpencil_draw_cancel;
  ot->poll = gpencil_draw_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* settings for drawing */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_gpencil_drawmodes, 0, "Mode", "Way to interpret mouse movements");

  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* NOTE: wait for input is enabled by default,
   * so that all UI code can work properly without needing users to know about this */
  prop = RNA_def_boolean(ot->srna,
                         "wait_for_input",
                         true,
                         "Wait for Input",
                         "Wait for first click instead of painting immediately");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "disable_straight", false, "No Straight lines", "Disable key for straight lines");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "disable_fill",
                         false,
                         "No Fill Areas",
                         "Disable fill to use stroke as fill boundary");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "disable_stabilizer", false, "No Stabilizer", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* guides */
  prop = RNA_def_float(ot->srna,
                       "guide_last_angle",
                       0.0f,
                       -10000.0f,
                       10000.0f,
                       "Angle",
                       "Speed guide angle",
                       -10000.0f,
                       10000.0f);
}

/* additional OPs */

static int gpencil_guide_rotate(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  GP_Sculpt_Guide *guide = &ts->gp_sculpt.guide;
  float angle = RNA_float_get(op->ptr, "angle");
  bool increment = RNA_boolean_get(op->ptr, "increment");
  if (increment) {
    float oldangle = guide->angle;
    oldangle += angle;
    guide->angle = angle_compat_rad(oldangle, M_PI);
  }
  else {
    guide->angle = angle_compat_rad(angle, M_PI);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_guide_rotate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rotate Guide Angle";
  ot->idname = "GPENCIL_OT_guide_rotate";
  ot->description = "Rotate guide angle";

  /* api callbacks */
  ot->exec = gpencil_guide_rotate;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna, "increment", true, "Increment", "Increment angle");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_float(
      ot->srna, "angle", 0.0f, -10000.0f, 10000.0f, "Angle", "Guide angle", -10000.0f, 10000.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

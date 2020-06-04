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
 * The Original Code is Copyright (C) 2008/2018, Blender Foundation
 * This is a new part of Blender
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
#include "BLI_math.h"
#include "BLI_math_geom.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "UI_view2d.h"

#include "ED_clip.h"
#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"

#include "gpencil_intern.h"

/* ******************************************* */
/* 'Globals' and Defines */

/* values for tGPsdata->status */
typedef enum eGPencil_PaintStatus {
  GP_STATUS_IDLING = 0, /* stroke isn't in progress yet */
  GP_STATUS_PAINTING,   /* a stroke is in progress */
  GP_STATUS_ERROR,      /* something wasn't correctly set up */
  GP_STATUS_DONE,       /* painting done */
  GP_STATUS_CAPTURE     /* capture event, but cancel */
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
  GP_PAINTFLAG_STROKEADDED = (1 << 1),
  GP_PAINTFLAG_V3D_ERASER_DEPTH = (1 << 2),
  GP_PAINTFLAG_SELECTMASK = (1 << 3),
  /* Flags used to indicate if stabilization is being used. */
  GP_PAINTFLAG_USE_STABILIZER = (1 << 7),
  GP_PAINTFLAG_USE_STABILIZER_TEMP = (1 << 8),
} eGPencil_PaintFlags;

/* Temporary 'Stroke' Operation data
 *   "p" = op->customdata
 */
typedef struct tGPsdata {
  Main *bmain;
  /** current scene from context. */
  Scene *scene;
  struct Depsgraph *depsgraph;

  /** window where painting originated. */
  wmWindow *win;
  /** area where painting originated. */
  ScrArea *area;
  /** region where painting originated. */
  ARegion *region;
  /** needed for GP_STROKE_2DSPACE. */
  View2D *v2d;
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

  /* Stabilizer. */
  float stabilizer_factor;
  char stabilizer_radius;
  void *stabilizer_cursor;

  /** current mouse-position. */
  float mval[2];
  /** previous recorded mouse-position. */
  float mvalo[2];

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

  /** custom color - hack for enforcing a particular color for track/mask editing. */
  float custom_color[4];

  /** radial cursor data for drawing eraser. */
  void *erasercursor;

  /** 1: line horizontal, 2: line vertical, other: not defined, second element position. */
  short straight[2];

  /** key used for invoking the operator. */
  short keymodifier;
} tGPsdata;

/* ------ */

/* Macros for accessing sensitivity thresholds... */
/* minimum number of pixels mouse should move before new point created */
#define MIN_MANHATTEN_PX (U.gp_manhattendist)
/* minimum length of new segment before new point can be added */
#define MIN_EUCLIDEAN_PX (U.gp_euclideandist)

static bool gp_stroke_added_check(tGPsdata *p)
{
  return (p->gpf && p->gpf->strokes.last && p->flags & GP_PAINTFLAG_STROKEADDED);
}

static void gp_stroke_added_enable(tGPsdata *p)
{
  BLI_assert(p->gpf->strokes.last != NULL);
  p->flags |= GP_PAINTFLAG_STROKEADDED;
}

/* ------ */
/* Forward defines for some functions... */

static void gp_session_validatebuffer(tGPsdata *p);

/* ******************************************* */
/* Context Wrangling... */

/* check if context is suitable for drawing */
static bool gpencil_draw_poll(bContext *C)
{
  if (ED_operator_regionactive(C)) {
    /* check if current context can support GPencil data */
    if (ED_annotation_data_get_pointers(C, NULL) != NULL) {
      /* check if Grease Pencil isn't already running */
      if (ED_gpencil_session_active() == 0) {
        return true;
      }
      else {
        CTX_wm_operator_poll_msg_set(C, "Annotation operator is already active");
      }
    }
    else {
      CTX_wm_operator_poll_msg_set(C, "Failed to find Annotation data to draw into");
    }
  }
  else {
    CTX_wm_operator_poll_msg_set(C, "Active region not set");
  }

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
static void gp_get_3d_reference(tGPsdata *p, float vec[3])
{
  const float *fp = p->scene->cursor.location;

  /* use 3D-cursor */
  copy_v3_v3(vec, fp);
}

/* Stroke Editing ---------------------------- */

/* check if the current mouse position is suitable for adding a new point */
static bool gp_stroke_filtermval(tGPsdata *p, const float mval[2], float pmval[2])
{
  int dx = (int)fabsf(mval[0] - pmval[0]);
  int dy = (int)fabsf(mval[1] - pmval[1]);

  /* if buffer is empty, just let this go through (i.e. so that dots will work) */
  if (p->gpd->runtime.sbuffer_used == 0) {
    return true;

    /* check if mouse moved at least certain distance on both axes (best case)
     * - aims to eliminate some jitter-noise from input when trying to draw straight lines freehand
     */
  }
  /* If lazy mouse, check minimum distance. */
  else if (p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) {
    if ((dx * dx + dy * dy) > (p->stabilizer_radius * p->stabilizer_radius)) {
      return true;
    }
    else {
      /* If the mouse is moving within the radius of the last move,
       * don't update the mouse position. This allows sharp turns. */
      copy_v2_v2(p->mval, p->mvalo);
      return false;
    }
  }
  else if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX)) {
    return true;

    /* Check if the distance since the last point is significant enough:
     * - Prevents points being added too densely
     * - Distance here doesn't use sqrt to prevent slowness.
     *   We should still be safe from overflows though.
     */
  }
  else if ((dx * dx + dy * dy) > MIN_EUCLIDEAN_PX * MIN_EUCLIDEAN_PX) {
    return true;

    /* mouse 'didn't move' */
  }
  else {
    return false;
  }
}

/* convert screen-coordinates to buffer-coordinates */
static void gp_stroke_convertcoords(tGPsdata *p, const float mval[2], float out[3], float *depth)
{
  bGPdata *gpd = p->gpd;

  /* in 3d-space - pt->x/y/z are 3 side-by-side floats */
  if (gpd->runtime.sbuffer_sflag & GP_STROKE_3DSPACE) {
    int mval_i[2];
    round_v2i_v2fl(mval_i, mval);
    if (gpencil_project_check(p) &&
        (ED_view3d_autodist_simple(p->region, mval_i, out, 0, depth))) {
      /* projecting onto 3D-Geometry
       * - nothing more needs to be done here, since view_autodist_simple() has already done it
       */
    }
    else {
      float mval_prj[2];
      float rvec[3], dvec[3];
      float zfac;

      /* Current method just converts each point in screen-coordinates to
       * 3D-coordinates using the 3D-cursor as reference. In general, this
       * works OK, but it could of course be improved.
       *
       * TODO:
       * - investigate using nearest point(s) on a previous stroke as
       *   reference point instead or as offset, for easier stroke matching
       */

      gp_get_3d_reference(p, rvec);
      zfac = ED_view3d_calc_zfac(p->region->regiondata, rvec, NULL);

      if (ED_view3d_project_float_global(p->region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
          V3D_PROJ_RET_OK) {
        float mval_f[2];
        sub_v2_v2v2(mval_f, mval_prj, mval);
        ED_view3d_win_to_delta(p->region, mval_f, dvec, zfac);
        sub_v3_v3v3(out, rvec, dvec);
      }
      else {
        zero_v3(out);
      }
    }
  }

  /* 2d - on 'canvas' (assume that p->v2d is set) */
  else if ((gpd->runtime.sbuffer_sflag & GP_STROKE_2DSPACE) && (p->v2d)) {
    UI_view2d_region_to_view(p->v2d, mval[0], mval[1], &out[0], &out[1]);
    mul_v3_m4v3(out, p->imat, out);
  }

  /* 2d - relative to screen (viewport area) */
  else {
    if (p->subrect == NULL) { /* normal 3D view */
      out[0] = (float)(mval[0]) / (float)(p->region->winx) * 100;
      out[1] = (float)(mval[1]) / (float)(p->region->winy) * 100;
    }
    else { /* camera view, use subrect */
      out[0] = ((mval[0] - p->subrect->xmin) / BLI_rctf_size_x(p->subrect)) * 100;
      out[1] = ((mval[1] - p->subrect->ymin) / BLI_rctf_size_y(p->subrect)) * 100;
    }
  }
}

/* Apply smooth to buffer while drawing
 * to smooth point C, use 2 before (A, B) and current point (D):
 *
 *   A----B-----C------D
 *
 * \param p: Temp data
 * \param inf: Influence factor
 * \param idx: Index of the last point (need minimum 3 points in the array)
 */
static void gp_smooth_buffer(tGPsdata *p, float inf, int idx)
{
  bGPdata *gpd = p->gpd;
  short num_points = gpd->runtime.sbuffer_used;

  /* Do nothing if not enough points to smooth out */
  if ((num_points < 3) || (idx < 3) || (inf == 0.0f)) {
    return;
  }

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  float steps = 4.0f;
  if (idx < 4) {
    steps--;
  }

  tGPspoint *pta = idx >= 4 ? &points[idx - 4] : NULL;
  tGPspoint *ptb = idx >= 3 ? &points[idx - 3] : NULL;
  tGPspoint *ptc = idx >= 2 ? &points[idx - 2] : NULL;
  tGPspoint *ptd = &points[idx - 1];

  float sco[2] = {0.0f};
  float a[2], b[2], c[2], d[2];
  const float average_fac = 1.0f / steps;

  /* Compute smoothed coordinate by taking the ones nearby */
  if (pta) {
    copy_v2_v2(a, &pta->x);
    madd_v2_v2fl(sco, a, average_fac);
  }
  if (ptb) {
    copy_v2_v2(b, &ptb->x);
    madd_v2_v2fl(sco, b, average_fac);
  }
  if (ptc) {
    copy_v2_v2(c, &ptc->x);
    madd_v2_v2fl(sco, c, average_fac);
  }
  if (ptd) {
    copy_v2_v2(d, &ptd->x);
    madd_v2_v2fl(sco, d, average_fac);
  }

  /* Based on influence factor, blend between original and optimal smoothed coordinate */
  interp_v2_v2v2(c, c, sco, inf);
  copy_v2_v2(&ptc->x, c);
}

static void gp_stroke_arrow_calc_points_segment(float stroke_points[8],
                                                const float ref_point[2],
                                                const float dir_cw[2],
                                                const float dir_ccw[2],
                                                const float lenght,
                                                const float sign)
{
  stroke_points[0] = ref_point[0] + dir_cw[0] * lenght * sign;
  stroke_points[1] = ref_point[1] + dir_cw[1] * lenght * sign;
  stroke_points[2] = ref_point[0] + dir_ccw[0] * lenght * sign;
  stroke_points[3] = ref_point[1] + dir_ccw[1] * lenght * sign;
}

static void gp_stroke_arrow_calc_points(tGPspoint *point,
                                        const float stroke_dir[2],
                                        float corner[2],
                                        float stroke_points[8],
                                        const int arrow_style)
{
  const int arrow_lenght = 8;
  float norm_dir[2];
  copy_v2_v2(norm_dir, stroke_dir);
  normalize_v2(norm_dir);
  const float inv_norm_dir_clockwise[2] = {norm_dir[1], -norm_dir[0]};
  const float inv_norm_dir_counterclockwise[2] = {-norm_dir[1], norm_dir[0]};

  switch (arrow_style) {
    case GP_STROKE_ARROWSTYLE_OPEN:
      mul_v2_fl(norm_dir, arrow_lenght);
      stroke_points[0] = corner[0] + inv_norm_dir_clockwise[0] * arrow_lenght + norm_dir[0];
      stroke_points[1] = corner[1] + inv_norm_dir_clockwise[1] * arrow_lenght + norm_dir[1];
      stroke_points[2] = corner[0] + inv_norm_dir_counterclockwise[0] * arrow_lenght + norm_dir[0];
      stroke_points[3] = corner[1] + inv_norm_dir_counterclockwise[1] * arrow_lenght + norm_dir[1];
      break;
    case GP_STROKE_ARROWSTYLE_SEGMENT:
      gp_stroke_arrow_calc_points_segment(stroke_points,
                                          corner,
                                          inv_norm_dir_clockwise,
                                          inv_norm_dir_counterclockwise,
                                          arrow_lenght,
                                          1.0f);
      break;
    case GP_STROKE_ARROWSTYLE_CLOSED:
      mul_v2_fl(norm_dir, arrow_lenght);
      if (point != NULL) {
        add_v2_v2(&point->x, norm_dir);
        copy_v2_v2(corner, &point->x);
      }
      gp_stroke_arrow_calc_points_segment(stroke_points,
                                          corner,
                                          inv_norm_dir_clockwise,
                                          inv_norm_dir_counterclockwise,
                                          arrow_lenght,
                                          -1.0f);
      stroke_points[4] = corner[0] - norm_dir[0];
      stroke_points[5] = corner[1] - norm_dir[1];
      break;
    case GP_STROKE_ARROWSTYLE_SQUARE:
      mul_v2_fl(norm_dir, arrow_lenght * 1.5f);
      if (point != NULL) {
        add_v2_v2(&point->x, norm_dir);
        copy_v2_v2(corner, &point->x);
      }
      gp_stroke_arrow_calc_points_segment(stroke_points,
                                          corner,
                                          inv_norm_dir_clockwise,
                                          inv_norm_dir_counterclockwise,
                                          arrow_lenght * 0.75f,
                                          -1.0f);
      stroke_points[4] = stroke_points[0] - norm_dir[0];
      stroke_points[5] = stroke_points[1] - norm_dir[1];
      stroke_points[6] = stroke_points[2] - norm_dir[0];
      stroke_points[7] = stroke_points[3] - norm_dir[1];
      break;
    default:
      break;
  }
}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gp_stroke_addpoint(tGPsdata *p, const float mval[2], float pressure, double curtime)
{
  bGPdata *gpd = p->gpd;
  tGPspoint *pt;
  ToolSettings *ts = p->scene->toolsettings;

  /* check painting mode */
  if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    /* straight lines only - i.e. only store start and end point in buffer */
    if (gpd->runtime.sbuffer_used == 0) {
      /* first point in buffer (start point) */
      pt = (tGPspoint *)(gpd->runtime.sbuffer);

      /* store settings */
      copy_v2_v2(&pt->x, mval);
      /* T44932 - Pressure vals are unreliable, so ignore for now */
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
      copy_v2_v2(&pt->x, mval);
      /* T44932 - Pressure vals are unreliable, so ignore for now */
      pt->pressure = 1.0f;
      pt->strength = 1.0f;
      pt->time = (float)(curtime - p->inittime);

      /* now the buffer has 2 points (and shouldn't be allowed to get any larger) */
      gpd->runtime.sbuffer_used = 2;

      /* Arrows. */
      if (gpd->runtime.sbuffer_sflag & (GP_STROKE_USE_ARROW_START | GP_STROKE_USE_ARROW_END)) {
        /* Store start and end point coords for arrows. */
        float end[2];
        copy_v2_v2(end, &pt->x);
        pt = ((tGPspoint *)(gpd->runtime.sbuffer));
        float start[2];
        copy_v2_v2(start, &pt->x);

        /* Arrow end corner. */
        if (gpd->runtime.sbuffer_sflag & GP_STROKE_USE_ARROW_END) {
          pt++;
          float e_heading[2] = {start[0] - end[0], start[1] - end[1]};
          /* Calculate points for ending arrow. */
          gp_stroke_arrow_calc_points(
              pt, e_heading, end, gpd->runtime.arrow_end, gpd->runtime.arrow_end_style);
        }
        /* Arrow start corner. */
        if (gpd->runtime.sbuffer_sflag & GP_STROKE_USE_ARROW_START) {
          float s_heading[2] = {end[0] - start[0], end[1] - start[1]};
          /* Calculate points for starting arrow. */
          gp_stroke_arrow_calc_points(
              NULL, s_heading, start, gpd->runtime.arrow_start, gpd->runtime.arrow_start_style);
        }
      }
    }

    /* can keep carrying on this way :) */
    return GP_STROKEADD_NORMAL;
  }
  else if (p->paintmode == GP_PAINTMODE_DRAW) { /* normal drawing */
    /* check if still room in buffer or add more */
    gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
        gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

    /* get pointer to destination point */
    pt = ((tGPspoint *)(gpd->runtime.sbuffer) + gpd->runtime.sbuffer_used);

    /* store settings */
    copy_v2_v2(&pt->x, mval);
    pt->pressure = pressure;
    /* unused for annotations, but initialise for easier conversions to GP Object */
    pt->strength = 1.0f;

    /* point time */
    pt->time = (float)(curtime - p->inittime);

    /* increment counters */
    gpd->runtime.sbuffer_used++;

    /* Don't smooth if stabilizer is on. */
    if ((p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) == 0) {
      /* smooth while drawing previous points with a reduction factor for previous */
      for (int s = 0; s < 3; s++) {
        gp_smooth_buffer(p, 0.5f * ((3.0f - s) / 3.0f), gpd->runtime.sbuffer_used - s);
      }
    }

    return GP_STROKEADD_NORMAL;
  }
  else if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
    /* get pointer to destination point */
    pt = (tGPspoint *)gpd->runtime.sbuffer;

    /* store settings */
    copy_v2_v2(&pt->x, mval);
    /* T44932 - Pressure vals are unreliable, so ignore for now */
    pt->pressure = 1.0f;
    pt->strength = 1.0f;
    pt->time = (float)(curtime - p->inittime);

    /* if there's stroke for this poly line session add (or replace last) point
     * to stroke. This allows to draw lines more interactively (see new segment
     * during mouse slide, e.g.)
     */
    if (gp_stroke_added_check(p)) {
      bGPDstroke *gps = p->gpf->strokes.last;
      bGPDspoint *pts;

      /* first time point is adding to temporary buffer -- need to allocate new point in stroke */
      if (gpd->runtime.sbuffer_used == 0) {
        gps->points = MEM_reallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
        gps->totpoints++;
      }

      pts = &gps->points[gps->totpoints - 1];

      /* special case for poly lines: normally,
       * depth is needed only when creating new stroke from buffer,
       * but poly lines are converting to stroke instantly,
       * so initialize depth buffer before converting coordinates
       */
      if (gpencil_project_check(p)) {
        View3D *v3d = p->area->spacedata.first;

        view3d_region_operator_needs_opengl(p->win, p->region);
        ED_view3d_autodist_init(p->depsgraph,
                                p->region,
                                v3d,
                                (ts->annotate_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 1 : 0);
      }

      /* convert screen-coordinates to appropriate coordinates (and store them) */
      gp_stroke_convertcoords(p, &pt->x, &pts->x, NULL);

      /* copy pressure and time */
      pts->pressure = pt->pressure;
      pts->strength = pt->strength;
      pts->time = pt->time;
      gps->tot_triangles = 0;
    }

    /* increment counters */
    if (gpd->runtime.sbuffer_used == 0) {
      gpd->runtime.sbuffer_used++;
    }

    return GP_STROKEADD_NORMAL;
  }

  /* return invalid state for now... */
  return GP_STROKEADD_INVALID;
}

static void gp_stroke_arrow_init_point_default(bGPDspoint *pt)
{
  pt->pressure = 1.0f;
  pt->strength = 1.0f;
  pt->time = 1.0f;
}

static void gp_stroke_arrow_init_conv_point(bGPDspoint *pt, const float point[3])
{
  copy_v3_v3(&pt->x, point);
  gp_stroke_arrow_init_point_default(pt);
}

static void gp_stroke_arrow_init_point(
    tGPsdata *p, tGPspoint *ptc, bGPDspoint *pt, const float co[8], const int co_idx)
{
  /* Note: provided co_idx should be always pair number as it's [x1, y1, x2, y2, x3, y3]. */
  float real_co[2] = {co[co_idx], co[co_idx + 1]};
  copy_v2_v2(&ptc->x, real_co);
  gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
  gp_stroke_arrow_init_point_default(pt);
}

static void gp_stroke_arrow_allocate(bGPDstroke *gps, const int totpoints)
{
  /* Copy appropriate settings for stroke. */
  gps->totpoints = totpoints;
  /* Allocate enough memory for a continuous array for storage points. */
  gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
}

static void gp_arrow_create_open(tGPsdata *p,
                                 tGPspoint *ptc,
                                 bGPDspoint *pt,
                                 const float corner_point[3],
                                 const float arrow_points[8])
{
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 0);
  pt++;
  gp_stroke_arrow_init_conv_point(pt, corner_point);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 2);
}

static void gp_arrow_create_segm(tGPsdata *p,
                                 tGPspoint *ptc,
                                 bGPDspoint *pt,
                                 const float arrow_points[8])
{
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 0);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 2);
}

static void gp_arrow_create_closed(tGPsdata *p,
                                   tGPspoint *ptc,
                                   bGPDspoint *pt,
                                   const float arrow_points[8])
{
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 0);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 2);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 4);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 0);
}

static void gp_arrow_create_square(tGPsdata *p,
                                   tGPspoint *ptc,
                                   bGPDspoint *pt,
                                   const float corner_point[3],
                                   const float arrow_points[8])
{
  gp_stroke_arrow_init_conv_point(pt, corner_point);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 0);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 4);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 6);
  pt++;
  gp_stroke_arrow_init_point(p, ptc, pt, arrow_points, 2);
  pt++;
  gp_stroke_arrow_init_conv_point(pt, corner_point);
}

static void gp_arrow_create(tGPsdata *p,
                            tGPspoint *ptc,
                            bGPDspoint *pt,
                            bGPDstroke *arrow_stroke,
                            const float arrow_points[8],
                            const int style)
{
  float corner_conv[3];
  copy_v3_v3(corner_conv, &pt->x);

  switch (style) {
    case GP_STROKE_ARROWSTYLE_SEGMENT:
      gp_arrow_create_segm(p, ptc, pt, arrow_points);
      break;
    case GP_STROKE_ARROWSTYLE_CLOSED:
      gp_arrow_create_closed(p, ptc, pt, arrow_points);
      break;
    case GP_STROKE_ARROWSTYLE_OPEN:
      gp_arrow_create_open(p, ptc, pt, corner_conv, arrow_points);
      break;
    case GP_STROKE_ARROWSTYLE_SQUARE:
      gp_arrow_create_square(p, ptc, pt, corner_conv, arrow_points);
      break;
    default:
      break;
  }
  /* Link stroke to frame. */
  BLI_addtail(&p->gpf->strokes, arrow_stroke);
}

/* make a new stroke from the buffer data */
static void gp_stroke_newfrombuffer(tGPsdata *p)
{
  bGPdata *gpd = p->gpd;
  bGPDlayer *gpl = p->gpl;
  bGPDstroke *gps;
  bGPDspoint *pt;
  tGPspoint *ptc;
  ToolSettings *ts = p->scene->toolsettings;

  int i, totelem;
  /* Since strokes are so fine, when using their depth we need a margin
   * otherwise they might get missed. */
  int depth_margin = (ts->annotate_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 4 : 0;

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
    if (G.debug & G_DEBUG) {
      printf("Error: No valid points in stroke buffer to convert (tot=%d)\n",
             gpd->runtime.sbuffer_used);
    }
    return;
  }

  /* special case for poly line -- for already added stroke during session
   * coordinates are getting added to stroke immediately to allow more
   * interactive behavior
   */
  if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
    if (gp_stroke_added_check(p)) {
      return;
    }
  }

  /* allocate memory for a new stroke */
  gps = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");

  /* copy appropriate settings for stroke */
  gps->totpoints = totelem;
  gps->thickness = gpl->thickness;
  gps->fill_opacity_fac = 1.0f;
  gps->hardeness = 1.0f;
  copy_v2_fl(gps->aspect_ratio, 1.0f);
  gps->uv_scale = 1.0f;
  gps->flag = gpd->runtime.sbuffer_sflag;
  gps->inittime = p->inittime;
  gps->tot_triangles = 0;

  /* allocate enough memory for a continuous array for storage points */
  gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
  gps->tot_triangles = 0;

  /* set pointer to first non-initialized point */
  pt = gps->points + (gps->totpoints - totelem);

  /* copy points from the buffer to the stroke */
  if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    /* straight lines only -> only endpoints */
    {
      /* first point */
      ptc = gpd->runtime.sbuffer;

      /* convert screen-coordinates to appropriate coordinates (and store them) */
      gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);

      /* copy pressure and time */
      pt->pressure = ptc->pressure;
      pt->strength = ptc->strength;
      CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt->time = ptc->time;

      pt++;
    }

    if (totelem == 2) {
      bGPdata_Runtime runtime = gpd->runtime;

      /* Last point if applicable. */
      ptc = ((tGPspoint *)runtime.sbuffer) + (runtime.sbuffer_used - 1);

      /* Convert screen-coordinates to appropriate coordinates (and store them). */
      gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);

      /* Copy pressure and time. */
      pt->pressure = ptc->pressure;
      pt->strength = ptc->strength;
      CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt->time = ptc->time;

      /** Create arrow strokes. **/
      /* End arrow stroke. */
      if ((runtime.sbuffer_sflag & GP_STROKE_USE_ARROW_END) &&
          (runtime.arrow_end_style != GP_STROKE_ARROWSTYLE_NONE)) {
        int totarrowpoints = runtime.arrow_end_style;

        /* Setting up arrow stroke. */
        bGPDstroke *e_arrow_gps = BKE_gpencil_stroke_duplicate(gps, false);
        gp_stroke_arrow_allocate(e_arrow_gps, totarrowpoints);

        /* Set pointer to first non-initialized point. */
        pt = e_arrow_gps->points + (e_arrow_gps->totpoints - totarrowpoints);

        /* End point. */
        ptc = ((tGPspoint *)runtime.sbuffer) + (runtime.sbuffer_used - 1);
        gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
        gp_stroke_arrow_init_point_default(pt);

        /* Fill and convert arrow points to create arrow shape. */
        gp_arrow_create(p, ptc, pt, e_arrow_gps, runtime.arrow_end, runtime.arrow_end_style);
      }
      /* Start arrow stroke. */
      if ((runtime.sbuffer_sflag & GP_STROKE_USE_ARROW_START) &&
          (runtime.arrow_start_style != GP_STROKE_ARROWSTYLE_NONE)) {
        int totarrowpoints = runtime.arrow_start_style;

        /* Setting up arrow stroke. */
        bGPDstroke *s_arrow_gps = BKE_gpencil_stroke_duplicate(gps, false);
        gp_stroke_arrow_allocate(s_arrow_gps, totarrowpoints);

        /* Set pointer to first non-initialized point. */
        pt = s_arrow_gps->points + (s_arrow_gps->totpoints - totarrowpoints);

        /* Start point. */
        ptc = runtime.sbuffer;
        gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
        gp_stroke_arrow_init_point_default(pt);

        /* Fill and convert arrow points to create arrow shape. */
        gp_arrow_create(p, ptc, pt, s_arrow_gps, runtime.arrow_start, runtime.arrow_start_style);
      }
    }
  }
  else if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
    /* first point */
    ptc = gpd->runtime.sbuffer;

    /* convert screen-coordinates to appropriate coordinates (and store them) */
    gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);

    /* copy pressure and time */
    pt->pressure = ptc->pressure;
    pt->strength = ptc->strength;
    pt->time = ptc->time;
  }
  else {
    float *depth_arr = NULL;

    /* get an array of depths, far depths are blended */
    if (gpencil_project_check(p)) {
      int mval_i[2], mval_prev[2] = {0};
      int interp_depth = 0;
      int found_depth = 0;

      depth_arr = MEM_mallocN(sizeof(float) * gpd->runtime.sbuffer_used, "depth_points");

      for (i = 0, ptc = gpd->runtime.sbuffer; i < gpd->runtime.sbuffer_used; i++, ptc++, pt++) {
        round_v2i_v2fl(mval_i, &ptc->x);

        if ((ED_view3d_autodist_depth(p->region, mval_i, depth_margin, depth_arr + i) == 0) &&
            (i && (ED_view3d_autodist_depth_seg(
                       p->region, mval_i, mval_prev, depth_margin + 1, depth_arr + i) == 0))) {
          interp_depth = true;
        }
        else {
          found_depth = true;
        }

        copy_v2_v2_int(mval_prev, mval_i);
      }

      if (found_depth == false) {
        /* eeh... not much we can do.. :/, ignore depth in this case, use the 3D cursor */
        for (i = gpd->runtime.sbuffer_used - 1; i >= 0; i--) {
          depth_arr[i] = 0.9999f;
        }
      }
      else {
        if (ts->annotate_v3d_align & GP_PROJECT_DEPTH_STROKE_ENDPOINTS) {
          /* remove all info between the valid endpoints */
          int first_valid = 0;
          int last_valid = 0;

          for (i = 0; i < gpd->runtime.sbuffer_used; i++) {
            if (depth_arr[i] != FLT_MAX) {
              break;
            }
          }
          first_valid = i;

          for (i = gpd->runtime.sbuffer_used - 1; i >= 0; i--) {
            if (depth_arr[i] != FLT_MAX) {
              break;
            }
          }
          last_valid = i;

          /* invalidate non-endpoints, so only blend between first and last */
          for (i = first_valid + 1; i < last_valid; i++) {
            depth_arr[i] = FLT_MAX;
          }

          interp_depth = true;
        }

        if (interp_depth) {
          interp_sparse_array(depth_arr, gpd->runtime.sbuffer_used, FLT_MAX);
        }
      }
    }

    pt = gps->points;

    /* convert all points (normal behavior) */
    for (i = 0, ptc = gpd->runtime.sbuffer; i < gpd->runtime.sbuffer_used && ptc;
         i++, ptc++, pt++) {
      /* convert screen-coordinates to appropriate coordinates (and store them) */
      gp_stroke_convertcoords(p, &ptc->x, &pt->x, depth_arr ? depth_arr + i : NULL);

      /* copy pressure and time */
      pt->pressure = ptc->pressure;
      pt->strength = ptc->strength;
      CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt->time = ptc->time;
    }

    if (depth_arr) {
      MEM_freeN(depth_arr);
    }
  }

  /* add stroke to frame */
  BLI_addtail(&p->gpf->strokes, gps);
  gp_stroke_added_enable(p);
}

/* --- 'Eraser' for 'Paint' Tool ------ */

/* helper to free a stroke
 * NOTE: gps->dvert and gps->triangles should be NULL, but check anyway for good measure
 */
static void gp_free_stroke(bGPDframe *gpf, bGPDstroke *gps)
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
}

/* which which point is infront (result should only be used for comparison) */
static float view3d_point_depth(const RegionView3D *rv3d, const float co[3])
{
  if (rv3d->is_persp) {
    return ED_view3d_calc_zfac(rv3d, co, NULL);
  }
  else {
    return -dot_v3v3(rv3d->viewinv[2], co);
  }
}

/* only erase stroke points that are visible (3d view) */
static bool gp_stroke_eraser_is_occluded(tGPsdata *p,
                                         const bGPDspoint *pt,
                                         const int x,
                                         const int y)
{
  if ((p->area->spacetype == SPACE_VIEW3D) && (p->flags & GP_PAINTFLAG_V3D_ERASER_DEPTH)) {
    RegionView3D *rv3d = p->region->regiondata;
    const int mval_i[2] = {x, y};
    float mval_3d[3];

    if (ED_view3d_autodist_simple(p->region, mval_i, mval_3d, 0, NULL)) {
      const float depth_mval = view3d_point_depth(rv3d, mval_3d);
      const float depth_pt = view3d_point_depth(rv3d, &pt->x);

      if (depth_pt > depth_mval) {
        return true;
      }
    }
  }
  return false;
}

/* eraser tool - evaluation per stroke */
/* TODO: this could really do with some optimization (KD-Tree/BVH?) */
static void gp_stroke_eraser_dostroke(tGPsdata *p,
                                      bGPDframe *gpf,
                                      bGPDstroke *gps,
                                      const float mval[2],
                                      const int radius,
                                      const rcti *rect)
{
  bGPDspoint *pt1, *pt2;
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int mval_i[2];
  round_v2i_v2fl(mval_i, mval);

  if (gps->totpoints == 0) {
    /* just free stroke */
    gp_free_stroke(gpf, gps);
  }
  else if (gps->totpoints == 1) {
    /* only process if it hasn't been masked out... */
    if (!(p->flags & GP_PAINTFLAG_SELECTMASK) || (gps->points->flag & GP_SPOINT_SELECT)) {
      gp_point_to_xy(&p->gsc, gps, gps->points, &pc1[0], &pc1[1]);

      /* do boundbox check first */
      if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
        /* only check if point is inside */
        if (len_v2v2_int(mval_i, pc1) <= radius) {
          /* free stroke */
          gp_free_stroke(gpf, gps);
        }
      }
    }
  }
  else {
    /* Perform culling? */
    bool do_cull = false;

    /* Clear Tags
     *
     * Note: It's better this way, as we are sure that
     * we don't miss anything, though things will be
     * slightly slower as a result
     */
    for (i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      pt->flag &= ~GP_SPOINT_TAG;
    }

    /* First Pass: Loop over the points in the stroke
     *   1) Thin out parts of the stroke under the brush
     *   2) Tag "too thin" parts for removal (in second pass)
     */
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

      /* only process if it hasn't been masked out... */
      if ((p->flags & GP_PAINTFLAG_SELECTMASK) && !(gps->points->flag & GP_SPOINT_SELECT)) {
        continue;
      }

      gp_point_to_xy(&p->gsc, gps, pt1, &pc1[0], &pc1[1]);
      gp_point_to_xy(&p->gsc, gps, pt2, &pc2[0], &pc2[1]);

      /* Check that point segment of the boundbox of the eraser stroke */
      if (((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          ((!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1])) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1]))) {
        /* Check if point segment of stroke had anything to do with
         * eraser region  (either within stroke painted, or on its lines)
         *  - this assumes that linewidth is irrelevant
         */
        if (gp_stroke_inside_circle(mval, radius, pc1[0], pc1[1], pc2[0], pc2[1])) {
          if ((gp_stroke_eraser_is_occluded(p, pt1, pc1[0], pc1[1]) == false) ||
              (gp_stroke_eraser_is_occluded(p, pt2, pc2[0], pc2[1]) == false)) {
            /* Edge is affected - Check individual points now */
            if (len_v2v2_int(mval_i, pc1) <= radius) {
              pt1->flag |= GP_SPOINT_TAG;
            }
            if (len_v2v2_int(mval_i, pc2) <= radius) {
              pt2->flag |= GP_SPOINT_TAG;
            }
            do_cull = true;
          }
        }
      }
    }

    /* Second Pass: Remove any points that are tagged */
    if (do_cull) {
      gp_stroke_delete_tagged_points(gpf, gps, gps->next, GP_SPOINT_TAG, false, 0);
    }
  }
}

/* erase strokes which fall under the eraser strokes */
static void gp_stroke_doeraser(tGPsdata *p)
{
  bGPDframe *gpf = p->gpf;
  bGPDstroke *gps, *gpn;
  rcti rect;

  /* rect is rectangle of eraser */
  rect.xmin = p->mval[0] - p->radius;
  rect.ymin = p->mval[1] - p->radius;
  rect.xmax = p->mval[0] + p->radius;
  rect.ymax = p->mval[1] + p->radius;

  if (p->area->spacetype == SPACE_VIEW3D) {
    if (p->flags & GP_PAINTFLAG_V3D_ERASER_DEPTH) {
      View3D *v3d = p->area->spacedata.first;
      view3d_region_operator_needs_opengl(p->win, p->region);
      ED_view3d_autodist_init(p->depsgraph, p->region, v3d, 0);
    }
  }

  /* loop over strokes of active layer only (session init already took care of ensuring validity),
   * checking segments for intersections to remove
   */
  for (gps = gpf->strokes.first; gps; gps = gpn) {
    gpn = gps->next;
    /* Not all strokes in the datablock may be valid in the current editor/context
     * (e.g. 2D space strokes in the 3D view, if the same datablock is shared)
     */
    if (ED_gpencil_stroke_can_use_direct(p->area, gps)) {
      gp_stroke_eraser_dostroke(p, gpf, gps, p->mval, p->radius, &rect);
    }
  }
}

/* ******************************************* */
/* Sketching Operator */

/* clear the session buffers (call this before AND after a paint operation) */
static void gp_session_validatebuffer(tGPsdata *p)
{
  bGPdata *gpd = p->gpd;

  gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, true);

  /* reset flags */
  gpd->runtime.sbuffer_sflag = 0;

  /* reset inittime */
  p->inittime = 0.0;
}

/* (re)init new painting data */
static bool gp_session_initdata(bContext *C, tGPsdata *p)
{
  Main *bmain = CTX_data_main(C);
  bGPdata **gpd_ptr = NULL;
  ScrArea *curarea = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  /* make sure the active view (at the starting time) is a 3d-view */
  if (curarea == NULL) {
    p->status = GP_STATUS_ERROR;
    if (G.debug & G_DEBUG) {
      printf("Error: No active view for painting\n");
    }
    return 0;
  }

  /* pass on current scene and window */
  p->bmain = CTX_data_main(C);
  p->scene = CTX_data_scene(C);
  p->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  p->win = CTX_wm_window(C);

  unit_m4(p->imat);
  unit_m4(p->mat);

  switch (curarea->spacetype) {
    /* supported views first */
    case SPACE_VIEW3D: {
      /* View3D *v3d = curarea->spacedata.first; */
      /* RegionView3D *rv3d = region->regiondata; */

      /* set current area
       * - must verify that region data is 3D-view (and not something else)
       */
      /* CAUTION: If this is the "toolbar", then this will change on the first stroke */
      p->area = curarea;
      p->region = region;
      p->align_flag = &ts->annotate_v3d_align;

      if (region->regiondata == NULL) {
        p->status = GP_STATUS_ERROR;
        if (G.debug & G_DEBUG) {
          printf(
              "Error: 3D-View active region doesn't have any region data, so cannot be "
              "drawable\n");
        }
        return 0;
      }
      break;
    }
    case SPACE_NODE: {
      /* SpaceNode *snode = curarea->spacedata.first; */

      /* set current area */
      p->area = curarea;
      p->region = region;
      p->v2d = &region->v2d;
      p->align_flag = &ts->gpencil_v2d_align;
      break;
    }
    case SPACE_SEQ: {
      SpaceSeq *sseq = curarea->spacedata.first;

      /* set current area */
      p->area = curarea;
      p->region = region;
      p->v2d = &region->v2d;
      p->align_flag = &ts->gpencil_seq_align;

      /* check that gpencil data is allowed to be drawn */
      if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
        p->status = GP_STATUS_ERROR;
        if (G.debug & G_DEBUG) {
          printf("Error: In active view (sequencer), active mode doesn't support Grease Pencil\n");
        }
        return 0;
      }
      break;
    }
    case SPACE_IMAGE: {
      /* SpaceImage *sima = curarea->spacedata.first; */

      /* set the current area */
      p->area = curarea;
      p->region = region;
      p->v2d = &region->v2d;
      p->align_flag = &ts->gpencil_ima_align;
      break;
    }
    case SPACE_CLIP: {
      SpaceClip *sc = curarea->spacedata.first;
      MovieClip *clip = ED_space_clip_get_clip(sc);

      if (clip == NULL) {
        p->status = GP_STATUS_ERROR;
        return false;
      }

      /* set the current area */
      p->area = curarea;
      p->region = region;
      p->v2d = &region->v2d;
      p->align_flag = &ts->gpencil_v2d_align;

      invert_m4_m4(p->imat, sc->unistabmat);

      /* custom color for new layer */
      p->custom_color[0] = 1.0f;
      p->custom_color[1] = 0.0f;
      p->custom_color[2] = 0.5f;
      p->custom_color[3] = 0.9f;

      if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
        int framenr = ED_space_clip_get_clip_frame_number(sc);
        MovieTrackingTrack *track = BKE_tracking_track_get_active(&clip->tracking);
        MovieTrackingMarker *marker = track ? BKE_tracking_marker_get(track, framenr) : NULL;

        if (marker) {
          p->imat[3][0] -= marker->pos[0];
          p->imat[3][1] -= marker->pos[1];
        }
        else {
          p->status = GP_STATUS_ERROR;
          return false;
        }
      }

      invert_m4_m4(p->mat, p->imat);
      copy_m4_m4(p->gsc.mat, p->mat);
      break;
    }
    /* unsupported views */
    default: {
      p->status = GP_STATUS_ERROR;
      if (G.debug & G_DEBUG) {
        printf("Error: Annotations are not supported in this editor\n");
      }
      return 0;
    }
  }

  /* get gp-data */
  gpd_ptr = ED_annotation_data_get_pointers(C, &p->ownerPtr);
  if ((gpd_ptr == NULL) || !ED_gpencil_data_owner_is_annotation(&p->ownerPtr)) {
    p->status = GP_STATUS_ERROR;
    if (G.debug & G_DEBUG) {
      printf("Error: Current context doesn't allow for any Annotation data\n");
    }
    return 0;
  }
  else {
    /* if no existing GPencil block exists, add one */
    if (*gpd_ptr == NULL) {
      bGPdata *gpd = BKE_gpencil_data_addnew(bmain, "Annotations");
      *gpd_ptr = gpd;

      /* mark datablock as being used for annotations */
      gpd->flag |= GP_DATA_ANNOTATIONS;
    }
    p->gpd = *gpd_ptr;
  }

  if (ED_gpencil_session_active() == 0) {
    /* initialize undo stack,
     * also, existing undo stack would make buffer drawn
     */
    gpencil_undo_init(p->gpd);
  }

  /* clear out buffer (stored in gp-data), in case something contaminated it */
  gp_session_validatebuffer(p);

  return 1;
}

/* init new painting session */
static tGPsdata *gp_session_initpaint(bContext *C)
{
  tGPsdata *p = NULL;

  /* create new context data */
  p = MEM_callocN(sizeof(tGPsdata), "Annotation Drawing Data");

  /* Try to initialize context data
   * WARNING: This may not always succeed (e.g. using GP in an annotation-only context)
   */
  if (gp_session_initdata(C, p) == 0) {
    /* Invalid state - Exit
     * NOTE: It should be safe to just free the data, since failing context checks should
     * only happen when no data has been allocated.
     */
    MEM_freeN(p);
    return NULL;
  }

  /* Radius for eraser circle is defined in userprefs */
  /* NOTE: we do this here, so that if we exit immediately,
   *       erase size won't get lost
   */
  p->radius = U.gp_eraser;

  /* return context data for running paint operator */
  return p;
}

/* cleanup after a painting session */
static void gp_session_cleanup(tGPsdata *p)
{
  bGPdata *gpd = (p) ? p->gpd : NULL;

  /* error checking */
  if (gpd == NULL) {
    return;
  }

  /* free stroke buffer */
  if (gpd->runtime.sbuffer) {
    /* printf("\t\tGP - free sbuffer\n"); */
    MEM_freeN(gpd->runtime.sbuffer);
    gpd->runtime.sbuffer = NULL;
  }

  /* clear flags */
  gpd->runtime.sbuffer_used = 0;
  gpd->runtime.sbuffer_size = 0;
  gpd->runtime.sbuffer_sflag = 0;
  p->inittime = 0.0;
}

static void gp_session_free(tGPsdata *p)
{
  MEM_freeN(p);
}

/* init new stroke */
static void gp_paint_initstroke(tGPsdata *p, eGPencil_PaintModes paintmode, Depsgraph *depsgraph)
{
  Scene *scene = p->scene;
  ToolSettings *ts = scene->toolsettings;

  /* get active layer (or add a new one if non-existent) */
  p->gpl = BKE_gpencil_layer_active_get(p->gpd);
  if (p->gpl == NULL) {
    /* tag for annotations */
    p->gpd->flag |= GP_DATA_ANNOTATIONS;
    p->gpl = BKE_gpencil_layer_addnew(p->gpd, DATA_("Note"), true);

    if (p->custom_color[3]) {
      copy_v3_v3(p->gpl->color, p->custom_color);
    }
  }
  if (p->gpl->flag & GP_LAYER_LOCKED) {
    p->status = GP_STATUS_ERROR;
    if (G.debug & G_DEBUG) {
      printf("Error: Cannot paint on locked layer\n");
    }
    return;
  }

  /* get active frame (add a new one if not matching frame) */
  if (paintmode == GP_PAINTMODE_ERASER) {
    /* Eraser mode:
     * 1) Only allow erasing on the active layer (unlike for 3d-art Grease Pencil),
     *    since we won't be exposing layer locking in the UI
     * 2) Ensure that p->gpf refers to the frame used for the active layer
     *    (to avoid problems with other tools which expect it to exist)
     */
    bool has_layer_to_erase = false;

    if (BKE_gpencil_layer_is_editable(p->gpl)) {
      /* Ensure that there's stuff to erase here (not including selection mask below)... */
      if (p->gpl->actframe && p->gpl->actframe->strokes.first) {
        has_layer_to_erase = true;
      }
    }

    /* Ensure active frame is set correctly... */
    p->gpf = p->gpl->actframe;

    if (has_layer_to_erase == false) {
      p->status = GP_STATUS_CAPTURE;
      // if (G.debug & G_DEBUG)
      printf("Error: Eraser will not be affecting anything (gpencil_paint_init)\n");
      return;
    }
  }
  else {
    /* Drawing Modes - Add a new frame if needed on the active layer */
    short add_frame_mode = GP_GETFRAME_ADD_NEW;

    if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) {
      add_frame_mode = GP_GETFRAME_ADD_COPY;
    }
    else {
      add_frame_mode = GP_GETFRAME_ADD_NEW;
    }

    p->gpf = BKE_gpencil_layer_frame_get(p->gpl, CFRA, add_frame_mode);

    if (p->gpf == NULL) {
      p->status = GP_STATUS_ERROR;
      if (G.debug & G_DEBUG) {
        printf("Error: No frame created (gpencil_paint_init)\n");
      }
      return;
    }
    else {
      p->gpf->flag |= GP_FRAME_PAINT;
    }
  }

  /* set 'eraser' for this stroke if using eraser */
  p->paintmode = paintmode;
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    p->gpd->runtime.sbuffer_sflag |= GP_STROKE_ERASER;

    /* check if we should respect depth while erasing */
    if (p->area->spacetype == SPACE_VIEW3D) {
      if (p->gpl->flag & GP_LAYER_NO_XRAY) {
        p->flags |= GP_PAINTFLAG_V3D_ERASER_DEPTH;
      }
    }
  }
  else {
    /* disable eraser flags - so that we can switch modes during a session */
    p->gpd->runtime.sbuffer_sflag &= ~GP_STROKE_ERASER;

    if (p->area->spacetype == SPACE_VIEW3D) {
      if (p->gpl->flag & GP_LAYER_NO_XRAY) {
        p->flags &= ~GP_PAINTFLAG_V3D_ERASER_DEPTH;
      }
    }
  }

  /* set 'initial run' flag, which is only used to denote when a new stroke is starting */
  p->flags |= GP_PAINTFLAG_FIRSTRUN;

  /* when drawing in the camera view, in 2D space, set the subrect */
  p->subrect = NULL;
  if ((*p->align_flag & GP_PROJECT_VIEWSPACE) == 0) {
    if (p->area->spacetype == SPACE_VIEW3D) {
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
    switch (p->area->spacetype) {
      case SPACE_VIEW3D: {
        p->gpd->runtime.sbuffer_sflag |= GP_STROKE_3DSPACE;
        break;
      }
      case SPACE_NODE:
      case SPACE_SEQ:
      case SPACE_IMAGE:
      case SPACE_CLIP: {
        p->gpd->runtime.sbuffer_sflag |= GP_STROKE_2DSPACE;
        break;
      }
    }
  }
}

/* finish off a stroke (clears buffer, but doesn't finish the paint operation) */
static void gp_paint_strokeend(tGPsdata *p)
{
  ToolSettings *ts = p->scene->toolsettings;
  /* for surface sketching, need to set the right OpenGL context stuff so that
   * the conversions will project the values correctly...
   */
  if (gpencil_project_check(p)) {
    View3D *v3d = p->area->spacedata.first;

    /* need to restore the original projection settings before packing up */
    view3d_region_operator_needs_opengl(p->win, p->region);
    ED_view3d_autodist_init(
        p->depsgraph, p->region, v3d, (ts->annotate_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 1 : 0);
  }

  /* check if doing eraser or not */
  if ((p->gpd->runtime.sbuffer_sflag & GP_STROKE_ERASER) == 0) {
    /* transfer stroke to frame */
    gp_stroke_newfrombuffer(p);
  }

  /* clean up buffer now */
  gp_session_validatebuffer(p);
}

/* finish off stroke painting operation */
static void gp_paint_cleanup(tGPsdata *p)
{
  /* p->gpd==NULL happens when stroke failed to initialize,
   * for example when GP is hidden in current space (sergey)
   */
  if (p->gpd) {
    /* finish off a stroke */
    gp_paint_strokeend(p);
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
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    GPU_line_smooth(true);
    GPU_blend(true);
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

    immUniformColor4ub(255, 100, 100, 20);
    imm_draw_circle_fill_2d(shdr_pos, x, y, p->radius, 40);

    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

    immUniformColor4f(1.0f, 0.39f, 0.39f, 0.78f);
    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniform1f("dash_width", 12.0f);
    immUniform1f("dash_factor", 0.5f);

    imm_draw_circle_wire_2d(shdr_pos,
                            x,
                            y,
                            p->radius,
                            /* XXX Dashed shader gives bad results with sets of small segments
                             * currently, temp hack around the issue. :( */
                            max_ii(8, p->radius / 2)); /* was fixed 40 */

    immUnbindProgram();

    GPU_blend(false);
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
    /* enable cursor */
    p->erasercursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                               RGN_TYPE_ANY,
                                               NULL, /* XXX */
                                               gpencil_draw_eraser,
                                               p);
  }
}
static void gpencil_draw_stabilizer(bContext *C, int x, int y, void *p_ptr)
{
  ARegion *region = CTX_wm_region(C);
  tGPsdata *p = (tGPsdata *)p_ptr;
  bGPdata_Runtime runtime = p->gpd->runtime;
  const tGPspoint *points = runtime.sbuffer;
  int totpoints = runtime.sbuffer_used;
  if (totpoints < 2) {
    return;
  }
  const tGPspoint *pt = &points[totpoints - 1];

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  GPU_line_smooth(true);
  GPU_blend(true);
  GPU_line_width(1.25f);
  const float color[3] = {1.0f, 0.39f, 0.39f};

  /* default radius and color */
  float darkcolor[3];
  const float radius = 4.0f;

  /* Inner Ring: Color from UI panel */
  immUniformColor4f(color[0], color[1], color[2], 0.8f);
  imm_draw_circle_wire_2d(pos, x, y, radius, 40);

  /* Outer Ring: Dark color for contrast on light backgrounds (e.g. gray on white) */
  mul_v3_v3fl(darkcolor, color, 0.40f);
  immUniformColor4f(darkcolor[0], darkcolor[1], darkcolor[2], 0.8f);
  imm_draw_circle_wire_2d(pos, x, y, radius + 1, 40);

  /* Rope Simple. */
  immUniformColor4f(color[0], color[1], color[2], 0.8f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, pt->x + region->winrct.xmin, pt->y + region->winrct.ymin);
  immVertex2f(pos, x, y);
  immEnd();

  /* Returns back all GPU settings */
  GPU_blend(false);
  GPU_line_smooth(false);

  immUnbindProgram();
}

/* Turn *stabilizer* brush cursor in 3D view on/off */
static void gpencil_draw_toggle_stabilizer_cursor(tGPsdata *p, short enable)
{
  if (p->stabilizer_cursor && !enable) {
    /* clear cursor */
    WM_paint_cursor_end(p->stabilizer_cursor);
    p->stabilizer_cursor = NULL;
  }
  else if (enable && !p->stabilizer_cursor) {
    /* enable cursor */
    p->stabilizer_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, NULL, gpencil_draw_stabilizer, p);
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

  /* restore cursor to indicate end of drawing */
  WM_cursor_modal_restore(CTX_wm_window(C));

  /* don't assume that operator data exists at all */
  if (p) {
    /* check size of buffer before cleanup, to determine if anything happened here */
    if (p->paintmode == GP_PAINTMODE_ERASER) {
      /* turn off radial brush cursor */
      gpencil_draw_toggle_eraser_cursor(p, false);
    }
    else if (p->paintmode == GP_PAINTMODE_DRAW) {
      gpencil_draw_toggle_stabilizer_cursor(p, false);
    }

    /* always store the new eraser size to be used again next time
     * NOTE: Do this even when not in eraser mode, as eraser may
     *       have been toggled at some point.
     */
    U.gp_eraser = p->radius;

    /* clear undo stack */
    gpencil_undo_finish();

    /* cleanup */
    gp_paint_cleanup(p);
    gp_session_cleanup(p);
    gp_session_free(p);
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

  /* check context */
  p = op->customdata = gp_session_initpaint(C);
  if ((p == NULL) || (p->status == GP_STATUS_ERROR)) {
    /* something wasn't set correctly in context */
    gpencil_draw_exit(C, op);
    return 0;
  }

  /* init painting data */
  gp_paint_initstroke(p, paintmode, CTX_data_ensure_evaluated_depsgraph(C));
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

/* ensure that the correct cursor icon is set */
static void gpencil_draw_cursor_set(tGPsdata *p)
{
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    WM_cursor_modal_set(p->win, WM_CURSOR_ERASER);
  }
  else {
    WM_cursor_modal_set(p->win, WM_CURSOR_PAINT_BRUSH);
  }
}

/* update UI indicators of status, including cursor and header prints */
static void gpencil_draw_status_indicators(bContext *C, tGPsdata *p)
{
  /* header prints */
  switch (p->status) {
    case GP_STATUS_PAINTING:
      switch (p->paintmode) {
        case GP_PAINTMODE_DRAW_POLY:
          /* Provide usage tips, since this is modal, and unintuitive without hints */
          ED_workspace_status_text(
              C,
              TIP_("Annotation Create Poly: LMB click to place next stroke vertex | "
                   "ESC/Enter to end  (or click outside this area)"));
          break;
        default:
          /* Do nothing - the others are self explanatory, exit quickly once the mouse is
           * released Showing any text would just be annoying as it would flicker.
           */
          break;
      }
      break;

    case GP_STATUS_IDLING:
      /* print status info */
      switch (p->paintmode) {
        case GP_PAINTMODE_ERASER:
          ED_workspace_status_text(C,
                                   TIP_("Annotation Eraser: Hold and drag LMB or RMB to erase | "
                                        "ESC/Enter to end  (or click outside this area)"));
          break;
        case GP_PAINTMODE_DRAW_STRAIGHT:
          ED_workspace_status_text(C,
                                   TIP_("Annotation Line Draw: Hold and drag LMB to draw | "
                                        "ESC/Enter to end  (or click outside this area)"));
          break;
        case GP_PAINTMODE_DRAW:
          ED_workspace_status_text(C,
                                   TIP_("Annotation Freehand Draw: Hold and drag LMB to draw | "
                                        "E/ESC/Enter to end  (or click outside this area)"));
          break;
        case GP_PAINTMODE_DRAW_POLY:
          ED_workspace_status_text(
              C,
              TIP_("Annotation Create Poly: LMB click to place next stroke vertex | "
                   "ESC/Enter to end  (or click outside this area)"));
          break;

        default: /* unhandled future cases */
          ED_workspace_status_text(
              C, TIP_("Annotation Session: ESC/Enter to end   (or click outside this area)"));
          break;
      }
      break;

    case GP_STATUS_ERROR:
    case GP_STATUS_DONE:
    case GP_STATUS_CAPTURE:
      /* clear status string */
      ED_workspace_status_text(C, NULL);
      break;
  }
}

/* ------------------------------- */

/* create a new stroke point at the point indicated by the painting context */
static void gpencil_draw_apply(wmOperator *op, tGPsdata *p, Depsgraph *depsgraph)
{
  /* handle drawing/erasing -> test for erasing first */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    /* do 'live' erasing now */
    gp_stroke_doeraser(p);

    /* store used values */
    p->mvalo[0] = p->mval[0];
    p->mvalo[1] = p->mval[1];
    p->opressure = p->pressure;
  }
  /* Only add current point to buffer if mouse moved
   * (even though we got an event, it might be just noise). */
  else if (gp_stroke_filtermval(p, p->mval, p->mvalo)) {
    /* If lazy mouse, interpolate the last and current mouse positions. */
    if (p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) {
      float now_mouse[2];
      float last_mouse[2];
      copy_v2_v2(now_mouse, p->mval);
      copy_v2_v2(last_mouse, p->mvalo);
      interp_v2_v2v2(now_mouse, now_mouse, last_mouse, min_ff(p->stabilizer_factor, .995f));
      copy_v2_v2(p->mval, now_mouse);
    }

    /* try to add point */
    short ok = gp_stroke_addpoint(p, p->mval, p->pressure, p->curtime);

    /* handle errors while adding point */
    if ((ok == GP_STROKEADD_FULL) || (ok == GP_STROKEADD_OVERFLOW)) {
      /* finish off old stroke */
      gp_paint_strokeend(p);
      /* And start a new one!!! Else, projection errors! */
      gp_paint_initstroke(p, p->paintmode, depsgraph);

      /* start a new stroke, starting from previous point */
      /* XXX Must manually reset inittime... */
      /* XXX We only need to reuse previous point if overflow! */
      if (ok == GP_STROKEADD_OVERFLOW) {
        p->inittime = p->ocurtime;
        gp_stroke_addpoint(p, p->mvalo, p->opressure, p->ocurtime);
      }
      else {
        p->inittime = p->curtime;
      }
      gp_stroke_addpoint(p, p->mval, p->pressure, p->curtime);
    }
    else if (ok == GP_STROKEADD_INVALID) {
      /* the painting operation cannot continue... */
      BKE_report(op->reports, RPT_ERROR, "Cannot paint stroke");
      p->status = GP_STATUS_ERROR;

      if (G.debug & G_DEBUG) {
        printf("Error: Grease-Pencil Paint - Add Point Invalid\n");
      }
      return;
    }

    /* store used values */
    p->mvalo[0] = p->mval[0];
    p->mvalo[1] = p->mval[1];
    p->opressure = p->pressure;
    p->ocurtime = p->curtime;
  }
}

/* handle draw event */
static void annotation_draw_apply_event(
    wmOperator *op, const wmEvent *event, Depsgraph *depsgraph, float x, float y)
{
  tGPsdata *p = op->customdata;
  PointerRNA itemptr;
  float mousef[2];

  /* convert from window-space to area-space mouse coordinates
   * add any x,y override position for fake events
   */
  p->mval[0] = (float)event->mval[0] - x;
  p->mval[1] = (float)event->mval[1] - y;

  /* Key to toggle stabilization. */
  if (event->shift > 0 && p->paintmode == GP_PAINTMODE_DRAW) {
    /* Using permanent stabilization, shift will deactivate the flag. */
    if (p->flags & (GP_PAINTFLAG_USE_STABILIZER)) {
      if (p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) {
        gpencil_draw_toggle_stabilizer_cursor(p, false);
        p->flags &= ~GP_PAINTFLAG_USE_STABILIZER_TEMP;
      }
    }
    /* Not using any stabilization flag. Activate temporal one. */
    else if ((p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) == 0) {
      p->flags |= GP_PAINTFLAG_USE_STABILIZER_TEMP;
      gpencil_draw_toggle_stabilizer_cursor(p, true);
    }
  }
  /* verify key status for straight lines */
  else if ((event->ctrl > 0) || (event->alt > 0)) {
    if (p->straight[0] == 0) {
      int dx = abs((int)(p->mval[0] - p->mvalo[0]));
      int dy = abs((int)(p->mval[1] - p->mvalo[1]));
      if ((dx > 0) || (dy > 0)) {
        /* check mouse direction to replace the other coordinate with previous values */
        if (dx >= dy) {
          /* horizontal */
          p->straight[0] = 1;
          p->straight[1] = p->mval[1]; /* save y */
        }
        else {
          /* vertical */
          p->straight[0] = 2;
          p->straight[1] = p->mval[0]; /* save x */
        }
      }
    }
  }
  else {
    p->straight[0] = 0;
    /* We were using shift while having permanent stabilization actived,
       so activate the temp flag back again. */
    if (p->flags & GP_PAINTFLAG_USE_STABILIZER) {
      if ((p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) == 0) {
        gpencil_draw_toggle_stabilizer_cursor(p, true);
        p->flags |= GP_PAINTFLAG_USE_STABILIZER_TEMP;
      }
    }
    /* We are using the temporal stabilizer flag atm,
       but shift is not pressed as well as the permanent flag is not used,
       so we don't need the cursor anymore. */
    else if (p->flags & GP_PAINTFLAG_USE_STABILIZER_TEMP) {
      /* Reset temporal stabilizer flag and remove cursor. */
      p->flags &= ~GP_PAINTFLAG_USE_STABILIZER_TEMP;
      gpencil_draw_toggle_stabilizer_cursor(p, false);
    }
  }

  p->curtime = PIL_check_seconds_timer();

  /* handle pressure sensitivity (which is supplied by tablets or otherwise 1.0) */
  p->pressure = event->tablet.pressure;

  /* Hack for pressure sensitive eraser on D+RMB when using a tablet:
   * The pen has to float over the tablet surface, resulting in
   * zero pressure (T47101). Ignore pressure values if floating
   * (i.e. "effectively zero" pressure), and only when the "active"
   * end is the stylus (i.e. the default when not eraser)
   */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    if ((event->tablet.active != EVT_TABLET_ERASER) && (p->pressure < 0.001f)) {
      p->pressure = 1.0f;
    }
  }

  /* special exception for start of strokes (i.e. maybe for just a dot) */
  if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
    p->flags &= ~GP_PAINTFLAG_FIRSTRUN;

    p->mvalo[0] = p->mval[0];
    p->mvalo[1] = p->mval[1];
    p->opressure = p->pressure;
    p->inittime = p->ocurtime = p->curtime;
    p->straight[0] = 0;
    p->straight[1] = 0;

    /* special exception here for too high pressure values on first touch in
     * windows for some tablets, then we just skip first touch...
     */
    if ((event->tablet.active != EVT_TABLET_NONE) && (p->pressure >= 0.99f)) {
      return;
    }
  }

  /* check if alt key is pressed and limit to straight lines */
  if ((p->paintmode != GP_PAINTMODE_ERASER) && (p->straight[0] != 0)) {
    if (p->straight[0] == 1) {
      /* horizontal */
      p->mval[1] = p->straight[1]; /* replace y */
    }
    else {
      /* vertical */
      p->mval[0] = p->straight[1]; /* replace x */
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
  gpencil_draw_apply(op, p, depsgraph);

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

  /* printf("GPencil - Starting Re-Drawing\n"); */

  /* try to initialize context data needed while drawing */
  if (!gpencil_draw_init(C, op, NULL)) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    /* printf("\tGP - no valid data\n"); */
    return OPERATOR_CANCELLED;
  }
  else {
    p = op->customdata;
  }

  /* printf("\tGP - Start redrawing stroke\n"); */

  /* loop over the stroke RNA elements recorded (i.e. progress of mouse movement),
   * setting the relevant values in context at each step, then applying
   */
  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    float mousef[2];

    /* printf("\t\tGP - stroke elem\n"); */

    /* get relevant data for this point from stroke */
    RNA_float_get_array(&itemptr, "mouse", mousef);
    p->mval[0] = (int)mousef[0];
    p->mval[1] = (int)mousef[1];
    p->pressure = RNA_float_get(&itemptr, "pressure");
    p->curtime = (double)RNA_float_get(&itemptr, "time") + p->inittime;

    if (RNA_boolean_get(&itemptr, "is_start")) {
      /* if first-run flag isn't set already (i.e. not true first stroke),
       * then we must terminate the previous one first before continuing
       */
      if ((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) {
        /* TODO: both of these ops can set error-status, but we probably don't need to worry */
        gp_paint_strokeend(p);
        gp_paint_initstroke(p, p->paintmode, depsgraph);
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
    gpencil_draw_apply(op, p, depsgraph);
  }
  RNA_END;

  /* printf("\tGP - done\n"); */

  /* cleanup */
  gpencil_draw_exit(C, op);

  /* refreshes */
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

/* ------------------------------- */

/* start of interactive drawing part of operator */
static int gpencil_draw_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPsdata *p = NULL;

  /* support for tablets eraser pen */
  if (gpencil_is_tablet_eraser_active(event)) {
    RNA_enum_set(op->ptr, "mode", GP_PAINTMODE_ERASER);
  }

  if (G.debug & G_DEBUG) {
    printf("GPencil - Starting Drawing\n");
  }

  /* try to initialize context data needed while drawing */
  if (!gpencil_draw_init(C, op, event)) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    if (G.debug & G_DEBUG) {
      printf("\tGP - no valid data\n");
    }
    return OPERATOR_CANCELLED;
  }
  else {
    p = op->customdata;
  }

  /* if empty erase capture and finish */
  if (p->status == GP_STATUS_CAPTURE) {
    gpencil_draw_exit(C, op);

    BKE_report(op->reports, RPT_ERROR, "Nothing to erase");
    return OPERATOR_FINISHED;
  }

  /* TODO: set any additional settings that we can take from the events?
   * TODO? if tablet is erasing, force eraser to be on? */

  /* TODO: move cursor setting stuff to stroke-start so that paintmode can be changed midway...
   */

  /* if eraser is on, draw radial aid */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    gpencil_draw_toggle_eraser_cursor(p, true);
  }
  else if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    if (RNA_enum_get(op->ptr, "arrowstyle_start") != GP_STROKE_ARROWSTYLE_NONE) {
      p->gpd->runtime.sbuffer_sflag |= GP_STROKE_USE_ARROW_START;
      p->gpd->runtime.arrow_start_style = RNA_enum_get(op->ptr, "arrowstyle_start");
    }
    if (RNA_enum_get(op->ptr, "arrowstyle_end") != GP_STROKE_ARROWSTYLE_NONE) {
      p->gpd->runtime.sbuffer_sflag |= GP_STROKE_USE_ARROW_END;
      p->gpd->runtime.arrow_end_style = RNA_enum_get(op->ptr, "arrowstyle_end");
    }
  }
  else if (p->paintmode == GP_PAINTMODE_DRAW) {
    p->stabilizer_factor = RNA_float_get(op->ptr, "stabilizer_factor");
    p->stabilizer_radius = RNA_int_get(op->ptr, "stabilizer_radius");
    if (RNA_boolean_get(op->ptr, "use_stabilizer")) {
      p->flags |= GP_PAINTFLAG_USE_STABILIZER | GP_PAINTFLAG_USE_STABILIZER_TEMP;
      gpencil_draw_toggle_stabilizer_cursor(p, true);
    }
    else if (event->shift > 0) {
      p->flags |= GP_PAINTFLAG_USE_STABILIZER_TEMP;
      gpencil_draw_toggle_stabilizer_cursor(p, true);
    }
  }
  /* set cursor
   * NOTE: This may change later (i.e. intentionally via brush toggle,
   *       or unintentionally if the user scrolls outside the area)...
   */
  gpencil_draw_cursor_set(p);

  /* only start drawing immediately if we're allowed to do so... */
  if (RNA_boolean_get(op->ptr, "wait_for_input") == false) {
    /* hotkey invoked - start drawing */
    /* printf("\tGP - set first spot\n"); */
    p->status = GP_STATUS_PAINTING;

    /* handle the initial drawing - i.e. for just doing a simple dot */
    annotation_draw_apply_event(op, event, CTX_data_ensure_evaluated_depsgraph(C), 0.0f, 0.0f);
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
  }
  else {
    /* toolbar invoked - don't start drawing yet... */
    /* printf("\tGP - hotkey invoked... waiting for click-drag\n"); */
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
  }

  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
  /* add a modal handler for this operator, so that we can then draw continuous strokes */
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/* gpencil modal operator stores area, which can be removed while using it (like fullscreen) */
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

  /* printf("\t\tGP - start stroke\n"); */

  /* we may need to set up paint env again if we're resuming */
  /* XXX: watch it with the paintmode! in future,
   *      it'd be nice to allow changing paint-mode when in sketching-sessions */

  if (gp_session_initdata(C, p)) {
    gp_paint_initstroke(p, p->paintmode, CTX_data_ensure_evaluated_depsgraph(C));
  }

  if (p->status != GP_STATUS_ERROR) {
    p->status = GP_STATUS_PAINTING;
    op->flag &= ~OP_IS_MODAL_CURSOR_REGION;
  }

  return op->customdata;
}

static void gpencil_stroke_end(wmOperator *op)
{
  tGPsdata *p = op->customdata;

  gp_paint_cleanup(p);

  gpencil_undo_push(p->gpd);

  gp_session_cleanup(p);

  p->status = GP_STATUS_IDLING;
  op->flag |= OP_IS_MODAL_CURSOR_REGION;

  p->gpd = NULL;
  p->gpl = NULL;
  p->gpf = NULL;
}

/* add events for missing mouse movements when the artist draw very fast */
static void annotation_add_missing_events(bContext *C,
                                          wmOperator *op,
                                          const wmEvent *event,
                                          tGPsdata *p)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  float pt[2], a[2], b[2];
  float factor = 10.0f;

  copy_v2_v2(a, p->mvalo);
  b[0] = (float)event->mval[0] + 1.0f;
  b[1] = (float)event->mval[1] + 1.0f;

  /* get distance in pixels */
  float dist = len_v2v2(a, b);

  /* for very small distances, add a half way point */
  if (dist <= 2.0f) {
    interp_v2_v2v2(pt, a, b, 0.5f);
    sub_v2_v2v2(pt, b, pt);
    /* create fake event */
    annotation_draw_apply_event(op, event, depsgraph, pt[0], pt[1]);
  }
  else if (dist >= factor) {
    int slices = 2 + (int)((dist - 1.0) / factor);
    float n = 1.0f / slices;
    for (int i = 1; i < slices; i++) {
      interp_v2_v2v2(pt, a, b, n * i);
      sub_v2_v2v2(pt, b, pt);
      /* create fake event */
      annotation_draw_apply_event(op, event, depsgraph, pt[0], pt[1]);
    }
  }
}

/* events handling during interactive drawing part of operator */
static int gpencil_draw_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPsdata *p = op->customdata;
  /* default exit state - pass through to support MMB view nav, etc. */
  int estate = OPERATOR_PASS_THROUGH;

  /* if (event->type == NDOF_MOTION)
   *    return OPERATOR_PASS_THROUGH;
   * -------------------------------
   * [mce] Not quite what I was looking
   * for, but a good start! GP continues to
   * draw on the screen while the 3D mouse
   * moves the viewpoint. Problem is that
   * the stroke is converted to 3D only after
   * it is finished. This approach should work
   * better in tools that immediately apply
   * in 3D space.
   */

  if (p->status == GP_STATUS_IDLING) {
    ARegion *region = CTX_wm_region(C);
    p->region = region;
  }

  /* We don't pass on key events, GP is used with key-modifiers -
   * prevents Dkey to insert drivers. */
  if (ISKEYBOARD(event->type)) {
    if (ELEM(event->type,
             EVT_LEFTARROWKEY,
             EVT_DOWNARROWKEY,
             EVT_RIGHTARROWKEY,
             EVT_UPARROWKEY,
             EVT_ZKEY)) {
      /* allow some keys:
       *   - for frame changing [#33412]
       *   - for undo (during sketching sessions)
       */
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
      /* allow numpad keys so that camera/view manipulations can still take place
       * - PAD0 in particular is really important for Grease Pencil drawing,
       *   as animators may be working "to camera", so having this working
       *   is essential for ensuring that they can quickly return to that view
       */
    }
    else if ((event->type == EVT_BKEY) && (event->val == KM_RELEASE)) {
      /* Add Blank Frame
       * - Since this operator is non-modal, we can just call it here, and keep going...
       * - This operator is especially useful when animating
       */
      WM_operator_name_call(C, "GPENCIL_OT_blank_frame_add", WM_OP_EXEC_DEFAULT, NULL);
      estate = OPERATOR_RUNNING_MODAL;
    }
    else {
      estate = OPERATOR_RUNNING_MODAL;
    }
  }

  // printf("\tGP - handle modal event...\n");

  /* Exit painting mode (and/or end current stroke)
   *
   * NOTE: cannot do RIGHTMOUSE (as is standard for canceling)
   * as that would break polyline T32647.
   */
  if (event->val == KM_PRESS &&
      ELEM(event->type, EVT_RETKEY, EVT_PADENTER, EVT_ESCKEY, EVT_SPACEKEY, EVT_EKEY)) {
    /* exit() ends the current stroke before cleaning up */
    /* printf("\t\tGP - end of paint op + end of stroke\n"); */
    p->status = GP_STATUS_DONE;
    estate = OPERATOR_FINISHED;
  }

  /* toggle painting mode upon mouse-button movement
   *  - LEFTMOUSE  = standard drawing (all) / straight line drawing (all) / polyline (toolbox
   * only)
   *  - RIGHTMOUSE = polyline (hotkey) / eraser (all)
   *    (Disabling RIGHTMOUSE case here results in bugs like [#32647])
   * also making sure we have a valid event value, to not exit too early
   */
  if (ELEM(event->type, LEFTMOUSE, RIGHTMOUSE) && (ELEM(event->val, KM_PRESS, KM_RELEASE))) {
    /* if painting, end stroke */
    if (p->status == GP_STATUS_PAINTING) {
      int sketch = 0;

      /* basically, this should be mouse-button up = end stroke
       * BUT, polyline drawing is an exception -- all knots should be added during one session
       */
      sketch |= (p->paintmode == GP_PAINTMODE_DRAW_POLY);

      if (sketch) {
        /* end stroke only, and then wait to resume painting soon */
        /* printf("\t\tGP - end stroke only\n"); */
        gpencil_stroke_end(op);

        /* If eraser mode is on, turn it off after the stroke finishes
         * NOTE: This just makes it nicer to work with drawing sessions
         */
        if (p->paintmode == GP_PAINTMODE_ERASER) {
          p->paintmode = RNA_enum_get(op->ptr, "mode");

          /* if the original mode was *still* eraser,
           * we'll let it say for now, since this gives
           * users an opportunity to have visual feedback
           * when adjusting eraser size
           */
          if (p->paintmode != GP_PAINTMODE_ERASER) {
            /* turn off cursor...
             * NOTE: this should be enough for now
             *       Just hiding this makes it seem like
             *       you can paint again...
             */
            gpencil_draw_toggle_eraser_cursor(p, false);
          }
        }

        /* we've just entered idling state, so this event was processed (but no others yet) */
        estate = OPERATOR_RUNNING_MODAL;

        /* stroke could be smoothed, send notifier to refresh screen */
        WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
      }
      else {
        /* printf("\t\tGP - end of stroke + op\n"); */
        p->status = GP_STATUS_DONE;
        estate = OPERATOR_FINISHED;
      }
    }
    else if (event->val == KM_PRESS) {
      bool in_bounds = false;

      /* Check if we're outside the bounds of the active region
       * NOTE: An exception here is that if launched from the toolbar,
       *       whatever region we're now in should become the new region
       */
      if ((p->region) && (p->region->regiontype == RGN_TYPE_TOOLS)) {
        /* Change to whatever region is now under the mouse */
        ARegion *current_region = BKE_area_find_region_xy(
            p->area, RGN_TYPE_ANY, event->x, event->y);

        if (G.debug & G_DEBUG) {
          printf("found alternative region %p (old was %p) - at %d %d (area: %d %d -> %d %d)\n",
                 current_region,
                 p->region,
                 event->x,
                 event->y,
                 p->area->totrct.xmin,
                 p->area->totrct.ymin,
                 p->area->totrct.xmax,
                 p->area->totrct.ymax);
        }

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

          if (G.debug & G_DEBUG) {
            printf("%s: Region under cursor is out of bounds, so cannot be drawn on\n", __func__);
          }
        }
      }
      else if (p->region) {
        /* Perform bounds check. */
        const rcti *region_rect = ED_region_visible_rect(p->region);
        in_bounds = BLI_rcti_isect_pt_v(region_rect, event->mval);
      }
      else {
        /* No region */
        p->status = GP_STATUS_ERROR;
        estate = OPERATOR_CANCELLED;

        if (G.debug & G_DEBUG) {
          printf("%s: No active region found in GP Paint session data\n", __func__);
        }
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
    }
  }

  /* handle mode-specific events */
  if (p->status == GP_STATUS_PAINTING) {
    /* handle painting mouse-movements? */
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) || (p->flags & GP_PAINTFLAG_FIRSTRUN)) {
      /* handle drawing event */
      if ((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) {
        annotation_add_missing_events(C, op, event, p);
      }

      /* TODO(sergey): Possibly evaluating dependency graph from modal operator? */
      annotation_draw_apply_event(op, event, CTX_data_ensure_evaluated_depsgraph(C), 0.0f, 0.0f);

      /* finish painting operation if anything went wrong just now */
      if (p->status == GP_STATUS_ERROR) {
        printf("\t\t\t\tGP - add error done!\n");
        estate = OPERATOR_CANCELLED;
      }
      else {
        /* event handled, so just tag as running modal */
        /* printf("\t\t\t\tGP - add point handled!\n"); */
        estate = OPERATOR_RUNNING_MODAL;
      }
    }
    /* eraser size */
    else if ((p->paintmode == GP_PAINTMODE_ERASER) &&
             ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE, EVT_PADPLUSKEY, EVT_PADMINUS)) {
      /* just resize the brush (local version)
       * TODO: fix the hardcoded size jumps (set to make a visible difference) and hardcoded keys
       */
      /* printf("\t\tGP - resize eraser\n"); */
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

  /* gpencil modal operator stores area, which can be removed while using it (like fullscreen) */
  if (0 == gpencil_area_exists(C, p->area)) {
    estate = OPERATOR_CANCELLED;
  }
  else {
    /* update status indicators - cursor, header, etc. */
    gpencil_draw_status_indicators(C, p);
    /* cursor may have changed outside our control - T44084 */
    gpencil_draw_cursor_set(p);
  }

  /* process last operations before exiting */
  switch (estate) {
    case OPERATOR_FINISHED:
      /* one last flush before we're done */
      gpencil_draw_exit(C, op);
      WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
      break;

    case OPERATOR_CANCELLED:
      gpencil_draw_exit(C, op);
      break;

    case OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH:
      /* event doesn't need to be handled */
#if 0
      printf("unhandled event -> %d (mmb? = %d | mmv? = %d)\n",
             event->type,
             event->type == MIDDLEMOUSE,
             event->type == MOUSEMOVE);
#endif
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
    {GP_PAINTMODE_DRAW_POLY,
     "DRAW_POLY",
     0,
     "Draw Poly Line",
     "Click to place endpoints of straight line segments (connected)"},
    {GP_PAINTMODE_ERASER, "ERASER", 0, "Eraser", "Erase Annotation strokes"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem arrow_types[] = {
    {GP_STROKE_ARROWSTYLE_NONE, "NONE", 0, "None", "Don't use any arrow/style in corner"},
    {GP_STROKE_ARROWSTYLE_CLOSED, "ARROW", 0, "Arrow", "Use closed arrow style"},
    {GP_STROKE_ARROWSTYLE_OPEN, "ARROW_OPEN", 0, "Open Arrow", "Use open arrow style"},
    {GP_STROKE_ARROWSTYLE_SEGMENT,
     "ARROW_OPEN_INVERTED",
     0,
     "Segment",
     "Use perpendicular segment style"},
    {GP_STROKE_ARROWSTYLE_SQUARE, "DIAMOND", 0, "Square", "Use square style"},
    {0, NULL, 0, NULL, NULL},
};

void GPENCIL_OT_annotate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Annotation Draw";
  ot->idname = "GPENCIL_OT_annotate";
  ot->description = "Make annotations on the active data";

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

  /* properties */
  prop = RNA_def_enum(
      ot->srna, "arrowstyle_start", arrow_types, 0, "Start Arrow Style", "Stroke start style");
  prop = RNA_def_enum(
      ot->srna, "arrowstyle_end", arrow_types, 0, "End Arrow Style", "Stroke end style");
  prop = RNA_def_boolean(ot->srna,
                         "use_stabilizer",
                         false,
                         "Stabilize Stroke",
                         "Helper to draw smooth and clean lines. Press Shift for an invert effect "
                         "(even if this option is not active)");
  prop = RNA_def_float(ot->srna,
                       "stabilizer_factor",
                       0.75f,
                       0.0f,
                       1.0f,
                       "Stabilizer Stroke Factor",
                       "Higher values gives a smoother stroke",
                       0.0f,
                       1.0f);
  prop = RNA_def_int(ot->srna,
                     "stabilizer_radius",
                     35,
                     0,
                     200,
                     "Stabilizer Stroke Radius",
                     "Minimun distance from last point before stroke continues",
                     1,
                     100);
  RNA_def_property_subtype(prop, PROP_PIXEL);

  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* NOTE: wait for input is enabled by default,
   * so that all UI code can work properly without needing users to know about this */
  prop = RNA_def_boolean(ot->srna,
                         "wait_for_input",
                         true,
                         "Wait for Input",
                         "Wait for first click instead of painting immediately");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

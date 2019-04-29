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
 * The Original Code is Copyright (C) Blender Foundation
 */

/** \file
 * \ingroup spgraph
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_anim_api.h"

#include "graph_intern.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* *************************** */
/* Utility Drawing Defines */

/* determine the alpha value that should be used when
 * drawing components for some F-Curve (fcu)
 * - selected F-Curves should be more visible than partially visible ones
 */
static float fcurve_display_alpha(FCurve *fcu)
{
  return (fcu->flag & FCURVE_SELECTED) ? 1.0f : U.fcu_inactive_alpha;
}

/* *************************** */
/* F-Curve Modifier Drawing */

/* Envelope -------------- */

/* TODO: draw a shaded poly showing the region of influence too!!! */
static void draw_fcurve_modifier_controls_envelope(FModifier *fcm, View2D *v2d)
{
  FMod_Envelope *env = (FMod_Envelope *)fcm->data;
  FCM_EnvelopeData *fed;
  const float fac = 0.05f * BLI_rctf_size_x(&v2d->cur);
  int i;

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniformColor3f(0.0f, 0.0f, 0.0f);
  immUniform1f("dash_width", 10.0f);
  immUniform1f("dash_factor", 0.5f);

  /* draw two black lines showing the standard reference levels */

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(shdr_pos, v2d->cur.xmin, env->midval + env->min);
  immVertex2f(shdr_pos, v2d->cur.xmax, env->midval + env->min);

  immVertex2f(shdr_pos, v2d->cur.xmin, env->midval + env->max);
  immVertex2f(shdr_pos, v2d->cur.xmax, env->midval + env->max);
  immEnd();

  immUnbindProgram();

  if (env->totvert > 0) {
    /* set size of vertices (non-adjustable for now) */
    GPU_point_size(2.0f);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    /* for now, point color is fixed, and is white */
    immUniformColor3f(1.0f, 1.0f, 1.0f);

    immBeginAtMost(GPU_PRIM_POINTS, env->totvert * 2);

    for (i = 0, fed = env->data; i < env->totvert; i++, fed++) {
      /* only draw if visible
       * - min/max here are fixed, not relative
       */
      if (IN_RANGE(fed->time, (v2d->cur.xmin - fac), (v2d->cur.xmax + fac))) {
        immVertex2f(shdr_pos, fed->time, fed->min);
        immVertex2f(shdr_pos, fed->time, fed->max);
      }
    }

    immEnd();

    immUnbindProgram();
  }
}

/* *************************** */
/* F-Curve Drawing */

/* Points ---------------- */

/* helper func - set color to draw F-Curve data with */
static void set_fcurve_vertex_color(FCurve *fcu, bool sel)
{
  float color[4];
  float diff;

  /* Set color of curve vertex based on state of curve (i.e. 'Edit' Mode) */
  if ((fcu->flag & FCURVE_PROTECTED) == 0) {
    /* Curve's points ARE BEING edited */
    UI_GetThemeColor3fv(sel ? TH_VERTEX_SELECT : TH_VERTEX, color);
  }
  else {
    /* Curve's points CANNOT BE edited */
    UI_GetThemeColor3fv(sel ? TH_TEXT_HI : TH_TEXT, color);
  }

  /* Fade the 'intensity' of the vertices based on the selection of the curves too
   * - Only fade by 50% the amount the curves were faded by, so that the points
   *   still stand out for easier selection
   */
  diff = 1.0f - fcurve_display_alpha(fcu);
  color[3] = 1.0f - (diff * 0.5f);
  CLAMP(color[3], 0.2f, 1.0f);

  immUniformColor4fv(color);
}

static void draw_fcurve_selected_keyframe_vertices(
    FCurve *fcu, View2D *v2d, bool edit, bool sel, unsigned pos)
{
  const float fac = 0.05f * BLI_rctf_size_x(&v2d->cur);

  set_fcurve_vertex_color(fcu, sel);

  immBeginAtMost(GPU_PRIM_POINTS, fcu->totvert);

  BezTriple *bezt = fcu->bezt;
  for (int i = 0; i < fcu->totvert; i++, bezt++) {
    /* As an optimization step, only draw those in view
     * - We apply a correction factor to ensure that points
     *   don't pop in/out due to slight twitches of view size.
     */
    if (IN_RANGE(bezt->vec[1][0], (v2d->cur.xmin - fac), (v2d->cur.xmax + fac))) {
      if (edit) {
        /* 'Keyframe' vertex only, as handle lines and handles have already been drawn
         * - only draw those with correct selection state for the current drawing color
         * -
         */
        if ((bezt->f2 & SELECT) == sel) {
          immVertex2fv(pos, bezt->vec[1]);
        }
      }
      else {
        /* no check for selection here, as curve is not editable... */
        /* XXX perhaps we don't want to even draw points?   maybe add an option for that later */
        immVertex2fv(pos, bezt->vec[1]);
      }
    }
  }

  immEnd();
}

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_keyframe_vertices(FCurve *fcu, View2D *v2d, bool edit, unsigned pos)
{
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

  immUniform1f("size", UI_GetThemeValuef(TH_VERTEX_SIZE) * U.pixelsize);

  draw_fcurve_selected_keyframe_vertices(fcu, v2d, edit, false, pos);
  draw_fcurve_selected_keyframe_vertices(fcu, v2d, edit, true, pos);

  immUnbindProgram();
}

/* helper func - draw handle vertices only for an F-Curve (if it is not protected) */
static void draw_fcurve_selected_handle_vertices(
    FCurve *fcu, View2D *v2d, bool sel, bool sel_handle_only, unsigned pos)
{
  (void)v2d; /* TODO: use this to draw only points in view */

  /* set handle color */
  float hcolor[3];
  UI_GetThemeColor3fv(sel ? TH_HANDLE_VERTEX_SELECT : TH_HANDLE_VERTEX, hcolor);
  immUniform4f("outlineColor", hcolor[0], hcolor[1], hcolor[2], 1.0f);
  immUniformColor3fvAlpha(hcolor, 0.01f); /* almost invisible - only keep for smoothness */

  immBeginAtMost(GPU_PRIM_POINTS, fcu->totvert * 2);

  BezTriple *bezt = fcu->bezt;
  BezTriple *prevbezt = NULL;
  for (int i = 0; i < fcu->totvert; i++, prevbezt = bezt, bezt++) {
    /* Draw the editmode handles for a bezier curve (others don't have handles)
     * if their selection status matches the selection status we're drawing for
     * - first handle only if previous beztriple was bezier-mode
     * - second handle only if current beztriple is bezier-mode
     *
     * Also, need to take into account whether the keyframe was selected
     * if a Graph Editor option to only show handles of selected keys is on.
     */
    if (!sel_handle_only || BEZT_ISSEL_ANY(bezt)) {
      if ((!prevbezt && (bezt->ipo == BEZT_IPO_BEZ)) ||
          (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ))) {
        if ((bezt->f1 & SELECT) == sel
            /* && v2d->cur.xmin < bezt->vec[0][0] < v2d->cur.xmax) */) {
          immVertex2fv(pos, bezt->vec[0]);
        }
      }

      if (bezt->ipo == BEZT_IPO_BEZ) {
        if ((bezt->f3 & SELECT) == sel
            /* && v2d->cur.xmin < bezt->vec[2][0] < v2d->cur.xmax) */) {
          immVertex2fv(pos, bezt->vec[2]);
        }
      }
    }
  }

  immEnd();
}

/* helper func - draw handle vertices only for an F-Curve (if it is not protected) */
static void draw_fcurve_handle_vertices(FCurve *fcu,
                                        View2D *v2d,
                                        bool sel_handle_only,
                                        unsigned pos)
{
  /* smooth outlines for more consistent appearance */
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);

  /* set handle size */
  immUniform1f("size", (1.4f * UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE)) * U.pixelsize);
  immUniform1f("outlineWidth", 1.5f * U.pixelsize);

  draw_fcurve_selected_handle_vertices(fcu, v2d, false, sel_handle_only, pos);
  draw_fcurve_selected_handle_vertices(fcu, v2d, true, sel_handle_only, pos);

  immUnbindProgram();
}

static void draw_fcurve_vertices(ARegion *ar, FCurve *fcu, bool do_handles, bool sel_handle_only)
{
  View2D *v2d = &ar->v2d;

  /* only draw points if curve is visible
   * - Draw unselected points before selected points as separate passes
   *    to make sure in the case of overlapping points that the selected is always visible
   * - Draw handles before keyframes, so that keyframes will overlap handles
   *   (keyframes are more important for users).
   */

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_blend(true);
  GPU_enable_program_point_size();

  /* draw the two handles first (if they're shown, the curve doesn't
   * have just a single keyframe, and the curve is being edited) */
  if (do_handles) {
    draw_fcurve_handle_vertices(fcu, v2d, sel_handle_only, pos);
  }

  /* draw keyframes over the handles */
  draw_fcurve_keyframe_vertices(fcu, v2d, !(fcu->flag & FCURVE_PROTECTED), pos);

  GPU_disable_program_point_size();
  GPU_blend(false);
}

/* Handles ---------------- */

static bool draw_fcurve_handles_check(SpaceGraph *sipo, FCurve *fcu)
{
  /* don't draw handle lines if handles are not to be shown */
  if (
      /* handles shouldn't be shown anywhere */
      (sipo->flag & SIPO_NOHANDLES) ||
      /* keyframes aren't editable */
      (fcu->flag & FCURVE_PROTECTED) ||
#if 0 /* handles can still be selected and handle types set, better draw - campbell */
      /* editing the handles here will cause weird/incorrect interpolation issues */
      (fcu->flag & FCURVE_INT_VALUES) ||
#endif
      /* group that curve belongs to is not editable */
      ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
      /* Do not show handles if there is only 1 keyframe,
       * otherwise they all clump together in an ugly ball. */
      (fcu->totvert <= 1)) {
    return false;
  }
  else {
    return true;
  }
}

/* draw lines for F-Curve handles only (this is only done in EditMode)
 * note: draw_fcurve_handles_check must be checked before running this. */
static void draw_fcurve_handles(SpaceGraph *sipo, FCurve *fcu)
{
  int sel, b;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

  immBeginAtMost(GPU_PRIM_LINES, 4 * 2 * fcu->totvert);

  /* slightly hacky, but we want to draw unselected points before selected ones
   * so that selected points are clearly visible
   */
  for (sel = 0; sel < 2; sel++) {
    BezTriple *bezt = fcu->bezt, *prevbezt = NULL;
    int basecol = (sel) ? TH_HANDLE_SEL_FREE : TH_HANDLE_FREE;
    const float *fp;
    unsigned char col[4];

    for (b = 0; b < fcu->totvert; b++, prevbezt = bezt, bezt++) {
      /* if only selected keyframes can get their handles shown,
       * check that keyframe is selected
       */
      if (sipo->flag & SIPO_SELVHANDLESONLY) {
        if (BEZT_ISSEL_ANY(bezt) == 0) {
          continue;
        }
      }

      /* draw handle with appropriate set of colors if selection is ok */
      if ((bezt->f2 & SELECT) == sel) {
        fp = bezt->vec[0];

        /* only draw first handle if previous segment had handles */
        if ((!prevbezt && (bezt->ipo == BEZT_IPO_BEZ)) ||
            (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ))) {
          UI_GetThemeColor3ubv(basecol + bezt->h1, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp);
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp + 3);
        }

        /* only draw second handle if this segment is bezier */
        if (bezt->ipo == BEZT_IPO_BEZ) {
          UI_GetThemeColor3ubv(basecol + bezt->h2, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp + 3);
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp + 6);
        }
      }
      else {
        /* only draw first handle if previous segment was had handles, and selection is ok */
        if (((bezt->f1 & SELECT) == sel) && ((!prevbezt && (bezt->ipo == BEZT_IPO_BEZ)) ||
                                             (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ)))) {
          fp = bezt->vec[0];
          UI_GetThemeColor3ubv(basecol + bezt->h1, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp);
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp + 3);
        }

        /* only draw second handle if this segment is bezier, and selection is ok */
        if (((bezt->f3 & SELECT) == sel) && (bezt->ipo == BEZT_IPO_BEZ)) {
          fp = bezt->vec[1];
          UI_GetThemeColor3ubv(basecol + bezt->h2, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp);
          immAttr4ubv(color, col);
          immVertex2fv(pos, fp + 3);
        }
      }
    }
  }

  immEnd();
  immUnbindProgram();
}

/* Samples ---------------- */

/* helper func - draw sample-range marker for an F-Curve as a cross
 * NOTE: the caller MUST HAVE GL_LINE_SMOOTH & GL_BLEND ENABLED, otherwise, the controls don't
 * have a consistent appearance (due to off-pixel alignments)...
 */
static void draw_fcurve_sample_control(
    float x, float y, float xscale, float yscale, float hsize, unsigned int pos)
{
  /* adjust view transform before starting */
  GPU_matrix_push();
  GPU_matrix_translate_2f(x, y);
  GPU_matrix_scale_2f(1.0f / xscale * hsize, 1.0f / yscale * hsize);

  /* draw X shape */
  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, -0.7f, -0.7f);
  immVertex2f(pos, +0.7f, +0.7f);

  immVertex2f(pos, -0.7f, +0.7f);
  immVertex2f(pos, +0.7f, -0.7f);
  immEnd();

  /* restore view transform */
  GPU_matrix_pop();
}

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_samples(SpaceGraph *sipo, ARegion *ar, FCurve *fcu)
{
  FPoint *first, *last;
  float hsize, xscale, yscale;

  /* get view settings */
  hsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
  UI_view2d_scale_get(&ar->v2d, &xscale, &yscale);

  /* get verts */
  first = fcu->fpt;
  last = (first) ? (first + (fcu->totvert - 1)) : (NULL);

  /* draw */
  if (first && last) {
    /* anti-aliased lines for more consistent appearance */
    if ((sipo->flag & SIPO_BEAUTYDRAW_OFF) == 0) {
      GPU_line_smooth(true);
    }
    GPU_blend(true);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    immUniformThemeColor((fcu->flag & FCURVE_SELECTED) ? TH_TEXT_HI : TH_TEXT);

    draw_fcurve_sample_control(first->vec[0], first->vec[1], xscale, yscale, hsize, pos);
    draw_fcurve_sample_control(last->vec[0], last->vec[1], xscale, yscale, hsize, pos);

    immUnbindProgram();

    GPU_blend(false);
    if ((sipo->flag & SIPO_BEAUTYDRAW_OFF) == 0) {
      GPU_line_smooth(false);
    }
  }
}

/* Curve ---------------- */

/* Helper func - just draw the F-Curve by sampling the visible region
 * (for drawing curves with modifiers). */
static void draw_fcurve_curve(
    bAnimContext *ac, ID *id, FCurve *fcu_, View2D *v2d, View2DGrid *grid, unsigned int pos)
{
  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  float samplefreq;
  float stime, etime;
  float unitFac, offset;
  float dx, dy;
  short mapping_flag = ANIM_get_normalization_flags(ac);
  int i, n;

  /* when opening a blend file on a different sized screen or while dragging the toolbar this can
   * happen best just bail out in this case. */
  UI_view2d_grid_size(grid, &dx, &dy);
  if (dx <= 0.0f) {
    return;
  }

  /* disable any drivers */
  FCurve fcurve_for_draw = *fcu_;
  fcurve_for_draw.driver = NULL;

  /* compute unit correction factor */
  unitFac = ANIM_unit_mapping_get_factor(ac->scene, id, &fcurve_for_draw, mapping_flag, &offset);

  /* Note about sampling frequency:
   * Ideally, this is chosen such that we have 1-2 pixels = 1 segment
   * which means that our curves can be as smooth as possible. However,
   * this does mean that curves may not be fully accurate (i.e. if they have
   * sudden spikes which happen at the sampling point, we may have problems).
   * Also, this may introduce lower performance on less densely detailed curves,
   * though it is impossible to predict this from the modifiers!
   *
   * If the automatically determined sampling frequency is likely to cause an infinite
   * loop (i.e. too close to 0), then clamp it to a determined "safe" value. The value
   *  chosen here is just the coarsest value which still looks reasonable...
   */
  /* grid->dx represents the number of 'frames' between gridlines,
   * but we divide by U.v2d_min_gridsize to get pixels-steps */
  /* TODO: perhaps we should have 1.0 frames
   * as upper limit so that curves don't get too distorted? */
  samplefreq = dx / (U.v2d_min_gridsize * U.pixelsize);

  if (sipo->flag & SIPO_BEAUTYDRAW_OFF) {
    /* Low Precision = coarse lower-bound clamping
     *
     * Although the "Beauty Draw" flag was originally for AA'd
     * line drawing, the sampling rate here has a much greater
     * impact on performance (e.g. for T40372)!
     *
     * This one still amounts to 10 sample-frames for each 1-frame interval
     * which should be quite a decent approximation in many situations.
     */
    if (samplefreq < 0.1f) {
      samplefreq = 0.1f;
    }
  }
  else {
    /* "Higher Precision" but slower - especially on larger windows (e.g. T40372) */
    if (samplefreq < 0.00001f) {
      samplefreq = 0.00001f;
    }
  }

  /* the start/end times are simply the horizontal extents of the 'cur' rect */
  stime = v2d->cur.xmin;
  etime = v2d->cur.xmax + samplefreq; /* + samplefreq here so that last item gets included... */

  /* at each sampling interval, add a new vertex
   * - apply the unit correction factor to the calculated values so that
   *   the displayed values appear correctly in the viewport
   */

  n = (etime - stime) / samplefreq + 0.5f;

  if (n > 0) {
    immBegin(GPU_PRIM_LINE_STRIP, (n + 1));

    for (i = 0; i <= n; i++) {
      float ctime = stime + i * samplefreq;
      immVertex2f(pos, ctime, (evaluate_fcurve(&fcurve_for_draw, ctime) + offset) * unitFac);
    }

    immEnd();
  }
}

/* helper func - draw a samples-based F-Curve */
static void draw_fcurve_curve_samples(
    bAnimContext *ac, ID *id, FCurve *fcu, View2D *v2d, const uint shdr_pos)
{
  FPoint *prevfpt = fcu->fpt;
  FPoint *fpt = prevfpt + 1;
  float fac, v[2];
  int b = fcu->totvert;
  float unit_scale, offset;
  short mapping_flag = ANIM_get_normalization_flags(ac);
  int count = fcu->totvert;

  if (prevfpt->vec[0] > v2d->cur.xmin) {
    count++;
  }

  if ((prevfpt + b - 1)->vec[0] < v2d->cur.xmax) {
    count++;
  }

  /* apply unit mapping */
  GPU_matrix_push();
  unit_scale = ANIM_unit_mapping_get_factor(ac->scene, id, fcu, mapping_flag, &offset);
  GPU_matrix_scale_2f(1.0f, unit_scale);
  GPU_matrix_translate_2f(0.0f, offset);

  immBegin(GPU_PRIM_LINE_STRIP, count);

  /* extrapolate to left? - left-side of view comes before first keyframe? */
  if (prevfpt->vec[0] > v2d->cur.xmin) {
    v[0] = v2d->cur.xmin;

    /* y-value depends on the interpolation */
    if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) ||
        (fcu->totvert == 1)) {
      /* just extend across the first keyframe's value */
      v[1] = prevfpt->vec[1];
    }
    else {
      /* extrapolate linear doesn't use the handle, use the next points center instead */
      fac = (prevfpt->vec[0] - fpt->vec[0]) / (prevfpt->vec[0] - v[0]);
      if (fac) {
        fac = 1.0f / fac;
      }
      v[1] = prevfpt->vec[1] - fac * (prevfpt->vec[1] - fpt->vec[1]);
    }

    immVertex2fv(shdr_pos, v);
  }

  /* loop over samples, drawing segments */
  /* draw curve between first and last keyframe (if there are enough to do so) */
  while (b--) {
    /* Linear interpolation: just add one point (which should add a new line segment) */
    immVertex2fv(shdr_pos, prevfpt->vec);

    /* get next pointers */
    if (b > 0) {
      prevfpt++;
    }
  }

  /* extrapolate to right? (see code for left-extrapolation above too) */
  if (prevfpt->vec[0] < v2d->cur.xmax) {
    v[0] = v2d->cur.xmax;

    /* y-value depends on the interpolation */
    if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) ||
        (fcu->totvert == 1)) {
      /* based on last keyframe's value */
      v[1] = prevfpt->vec[1];
    }
    else {
      /* extrapolate linear doesn't use the handle, use the previous points center instead */
      fpt = prevfpt - 1;
      fac = (prevfpt->vec[0] - fpt->vec[0]) / (prevfpt->vec[0] - v[0]);
      if (fac) {
        fac = 1.0f / fac;
      }
      v[1] = prevfpt->vec[1] - fac * (prevfpt->vec[1] - fpt->vec[1]);
    }

    immVertex2fv(shdr_pos, v);
  }

  immEnd();

  GPU_matrix_pop();
}

/* helper func - check if the F-Curve only contains easily drawable segments
 * (i.e. no easing equation interpolations)
 */
static bool fcurve_can_use_simple_bezt_drawing(FCurve *fcu)
{
  BezTriple *bezt;
  int i;

  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    if (ELEM(bezt->ipo, BEZT_IPO_CONST, BEZT_IPO_LIN, BEZT_IPO_BEZ) == false) {
      return false;
    }
  }

  return true;
}

/* helper func - draw one repeat of an F-Curve (using Bezier curve approximations) */
static void draw_fcurve_curve_bezts(
    bAnimContext *ac, ID *id, FCurve *fcu, View2D *v2d, unsigned int pos)
{
  BezTriple *prevbezt = fcu->bezt;
  BezTriple *bezt = prevbezt + 1;
  float v1[2], v2[2], v3[2], v4[2];
  float *fp, data[120];
  float fac = 0.0f;
  int b = fcu->totvert - 1;
  int resol;
  float unit_scale, offset;
  short mapping_flag = ANIM_get_normalization_flags(ac);

  /* apply unit mapping */
  GPU_matrix_push();
  unit_scale = ANIM_unit_mapping_get_factor(ac->scene, id, fcu, mapping_flag, &offset);
  GPU_matrix_scale_2f(1.0f, unit_scale);
  GPU_matrix_translate_2f(0.0f, offset);

  /* For now, this assumes the worst case scenario, where all the keyframes have
   * bezier interpolation, and are drawn at full res.
   * This is tricky to optimize, but maybe can be improved at some point... */
  immBeginAtMost(GPU_PRIM_LINE_STRIP, (b * 32 + 3));

  /* extrapolate to left? */
  if (prevbezt->vec[1][0] > v2d->cur.xmin) {
    /* left-side of view comes before first keyframe, so need to extend as not cyclic */
    v1[0] = v2d->cur.xmin;

    /* y-value depends on the interpolation */
    if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (prevbezt->ipo == BEZT_IPO_CONST) ||
        (fcu->totvert == 1)) {
      /* just extend across the first keyframe's value */
      v1[1] = prevbezt->vec[1][1];
    }
    else if (prevbezt->ipo == BEZT_IPO_LIN) {
      /* extrapolate linear dosnt use the handle, use the next points center instead */
      fac = (prevbezt->vec[1][0] - bezt->vec[1][0]) / (prevbezt->vec[1][0] - v1[0]);
      if (fac) {
        fac = 1.0f / fac;
      }
      v1[1] = prevbezt->vec[1][1] - fac * (prevbezt->vec[1][1] - bezt->vec[1][1]);
    }
    else {
      /* based on angle of handle 1 (relative to keyframe) */
      fac = (prevbezt->vec[0][0] - prevbezt->vec[1][0]) / (prevbezt->vec[1][0] - v1[0]);
      if (fac) {
        fac = 1.0f / fac;
      }
      v1[1] = prevbezt->vec[1][1] - fac * (prevbezt->vec[0][1] - prevbezt->vec[1][1]);
    }

    immVertex2fv(pos, v1);
  }

  /* if only one keyframe, add it now */
  if (fcu->totvert == 1) {
    v1[0] = prevbezt->vec[1][0];
    v1[1] = prevbezt->vec[1][1];
    immVertex2fv(pos, v1);
  }

  /* draw curve between first and last keyframe (if there are enough to do so) */
  /* TODO: optimize this to not have to calc stuff out of view too? */
  while (b--) {
    if (prevbezt->ipo == BEZT_IPO_CONST) {
      /* Constant-Interpolation: draw segment between previous keyframe and next,
       * but holding same value */
      v1[0] = prevbezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      immVertex2fv(pos, v1);

      v1[0] = bezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      immVertex2fv(pos, v1);
    }
    else if (prevbezt->ipo == BEZT_IPO_LIN) {
      /* Linear interpolation: just add one point (which should add a new line segment) */
      v1[0] = prevbezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      immVertex2fv(pos, v1);
    }
    else if (prevbezt->ipo == BEZT_IPO_BEZ) {
      /* Bezier-Interpolation: draw curve as series of segments between keyframes
       * - resol determines number of points to sample in between keyframes
       */

      /* resol depends on distance between points
       * (not just horizontal) OR is a fixed high res */
      /* TODO: view scale should factor into this someday too... */
      if (fcu->driver) {
        resol = 32;
      }
      else {
        resol = (int)(5.0f * len_v2v2(bezt->vec[1], prevbezt->vec[1]));
      }

      if (resol < 2) {
        /* only draw one */
        v1[0] = prevbezt->vec[1][0];
        v1[1] = prevbezt->vec[1][1];
        immVertex2fv(pos, v1);
      }
      else {
        /* clamp resolution to max of 32 */
        /* NOTE: higher values will crash */
        if (resol > 32) {
          resol = 32;
        }

        v1[0] = prevbezt->vec[1][0];
        v1[1] = prevbezt->vec[1][1];
        v2[0] = prevbezt->vec[2][0];
        v2[1] = prevbezt->vec[2][1];

        v3[0] = bezt->vec[0][0];
        v3[1] = bezt->vec[0][1];
        v4[0] = bezt->vec[1][0];
        v4[1] = bezt->vec[1][1];

        correct_bezpart(v1, v2, v3, v4);

        BKE_curve_forward_diff_bezier(v1[0], v2[0], v3[0], v4[0], data, resol, sizeof(float) * 3);
        BKE_curve_forward_diff_bezier(
            v1[1], v2[1], v3[1], v4[1], data + 1, resol, sizeof(float) * 3);

        for (fp = data; resol; resol--, fp += 3) {
          immVertex2fv(pos, fp);
        }
      }
    }

    /* get next pointers */
    prevbezt = bezt;
    bezt++;

    /* last point? */
    if (b == 0) {
      v1[0] = prevbezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      immVertex2fv(pos, v1);
    }
  }

  /* extrapolate to right? (see code for left-extrapolation above too) */
  if (prevbezt->vec[1][0] < v2d->cur.xmax) {
    v1[0] = v2d->cur.xmax;

    /* y-value depends on the interpolation */
    if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) ||
        (prevbezt->ipo == BEZT_IPO_CONST) || (fcu->totvert == 1)) {
      /* based on last keyframe's value */
      v1[1] = prevbezt->vec[1][1];
    }
    else if (prevbezt->ipo == BEZT_IPO_LIN) {
      /* extrapolate linear dosnt use the handle, use the previous points center instead */
      bezt = prevbezt - 1;
      fac = (prevbezt->vec[1][0] - bezt->vec[1][0]) / (prevbezt->vec[1][0] - v1[0]);
      if (fac) {
        fac = 1.0f / fac;
      }
      v1[1] = prevbezt->vec[1][1] - fac * (prevbezt->vec[1][1] - bezt->vec[1][1]);
    }
    else {
      /* based on angle of handle 1 (relative to keyframe) */
      fac = (prevbezt->vec[2][0] - prevbezt->vec[1][0]) / (prevbezt->vec[1][0] - v1[0]);
      if (fac) {
        fac = 1.0f / fac;
      }
      v1[1] = prevbezt->vec[1][1] - fac * (prevbezt->vec[2][1] - prevbezt->vec[1][1]);
    }

    immVertex2fv(pos, v1);
  }

  immEnd();

  GPU_matrix_pop();
}

/* Debugging -------------------------------- */

/* Draw indicators which show the value calculated from the driver,
 * and how this is mapped to the value that comes out of it. This
 * is handy for helping users better understand how to interpret
 * the graphs, and also facilitates debugging.
 */
static void graph_draw_driver_debug(bAnimContext *ac, ID *id, FCurve *fcu)
{
  ChannelDriver *driver = fcu->driver;
  View2D *v2d = &ac->ar->v2d;
  short mapping_flag = ANIM_get_normalization_flags(ac);
  float offset;
  float unitfac = ANIM_unit_mapping_get_factor(ac->scene, id, fcu, mapping_flag, &offset);

  /* for now, only show when debugging driver... */
  //if ((driver->flag & DRIVER_FLAG_SHOWDEBUG) == 0)
  //  return;

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */

  /* No curve to modify/visualize the result?
   * => We still want to show the 1-1 default...
   */
  if ((fcu->totvert == 0) && BLI_listbase_is_empty(&fcu->modifiers)) {
    float t;

    /* draw with thin dotted lines in style of what curve would have been */
    immUniformColor3fv(fcu->color);

    immUniform1f("dash_width", 40.0f);
    immUniform1f("dash_factor", 0.5f);
    GPU_line_width(2.0f);

    /* draw 1-1 line, stretching just past the screen limits
     * NOTE: we need to scale the y-values to be valid for the units
     */
    immBegin(GPU_PRIM_LINES, 2);

    t = v2d->cur.xmin;
    immVertex2f(shdr_pos, t, (t + offset) * unitfac);

    t = v2d->cur.xmax;
    immVertex2f(shdr_pos, t, (t + offset) * unitfac);

    immEnd();
  }

  /* draw driver only if actually functional */
  if ((driver->flag & DRIVER_FLAG_INVALID) == 0) {
    /* grab "coordinates" for driver outputs */
    float x = driver->curval;
    float y = fcu->curval * unitfac;

    /* only draw indicators if the point is in range*/
    if (x >= v2d->cur.xmin) {
      float co[2];

      /* draw dotted lines leading towards this point from both axes ....... */
      immUniformColor3f(0.9f, 0.9f, 0.9f);
      immUniform1f("dash_width", 10.0f);
      immUniform1f("dash_factor", 0.5f);

      immBegin(GPU_PRIM_LINES, (y >= v2d->cur.ymin) ? 4 : 2);

      /* x-axis lookup */
      co[0] = x;

      if (y >= v2d->cur.ymin) {
        co[1] = v2d->cur.ymin - 1.0f;
        immVertex2fv(shdr_pos, co);

        co[1] = y;
        immVertex2fv(shdr_pos, co);
      }

      /* y-axis lookup */
      co[1] = y;

      co[0] = v2d->cur.xmin - 1.0f;
      immVertex2fv(shdr_pos, co);

      co[0] = x;
      immVertex2fv(shdr_pos, co);

      immEnd();

      immUnbindProgram();

      /* GPU_PRIM_POINTS do not survive dashed line geometry shader... */
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      /* x marks the spot .................................................... */
      /* -> outer frame */
      immUniformColor3f(0.9f, 0.9f, 0.9f);
      GPU_point_size(7.0);

      immBegin(GPU_PRIM_POINTS, 1);
      immVertex2f(shdr_pos, x, y);
      immEnd();

      /* inner frame */
      immUniformColor3f(0.9f, 0.0f, 0.0f);
      GPU_point_size(3.0);

      immBegin(GPU_PRIM_POINTS, 1);
      immVertex2f(shdr_pos, x, y);
      immEnd();
    }
  }

  immUnbindProgram();
}

/* Public Curve-Drawing API  ---------------- */

/* Draw the 'ghost' F-Curves (i.e. snapshots of the curve)
 * NOTE: unit mapping has already been applied to the values, so do not try and apply again
 */
void graph_draw_ghost_curves(bAnimContext *ac, SpaceGraph *sipo, ARegion *ar)
{
  FCurve *fcu;

  /* draw with thick dotted lines */
  GPU_line_width(3.0f);

  /* anti-aliased lines for less jagged appearance */
  if ((sipo->flag & SIPO_BEAUTYDRAW_OFF) == 0) {
    GPU_line_smooth(true);
  }
  GPU_blend(true);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniform1f("dash_width", 20.0f);
  immUniform1f("dash_factor", 0.5f);

  /* the ghost curves are simply sampled F-Curves stored in sipo->runtime.ghost_curves */
  for (fcu = sipo->runtime.ghost_curves.first; fcu; fcu = fcu->next) {
    /* set whatever color the curve has set
     * - this is set by the function which creates these
     * - draw with a fixed opacity of 2
     */
    immUniformColor3fvAlpha(fcu->color, 0.5f);

    /* simply draw the stored samples */
    draw_fcurve_curve_samples(ac, NULL, fcu, &ar->v2d, shdr_pos);
  }

  immUnbindProgram();

  if ((sipo->flag & SIPO_BEAUTYDRAW_OFF) == 0) {
    GPU_line_smooth(false);
  }
  GPU_blend(false);
}

/* This is called twice from space_graph.c -> graph_main_region_draw()
 * Unselected then selected F-Curves are drawn so that they do not occlude each other.
 */
void graph_draw_curves(
    bAnimContext *ac, SpaceGraph *sipo, ARegion *ar, View2DGrid *grid, short sel)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* build list of curves to draw */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE);
  filter |= ((sel) ? (ANIMFILTER_SEL) : (ANIMFILTER_UNSEL));
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* for each curve:
   * draw curve, then handle-lines, and finally vertices in this order so that
   * the data will be layered correctly
   */
  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;
    FModifier *fcm = find_active_fmodifier(&fcu->modifiers);
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    /* map keyframes for drawing if scaled F-Curve */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 0);
    }

    /* draw curve:
     * - curve line may be result of one or more destructive modifiers or just the raw data,
     *   so we need to check which method should be used
     * - controls from active modifier take precedence over keyframes
     *   (XXX! editing tools need to take this into account!)
     */

    /* 1) draw curve line */
    if (((fcu->modifiers.first) || (fcu->flag & FCURVE_INT_VALUES)) ||
        (((fcu->bezt) || (fcu->fpt)) && (fcu->totvert))) {
      /* set color/drawing style for curve itself */
      /* draw active F-Curve thicker than the rest to make it stand out */
      if (fcu->flag & FCURVE_ACTIVE) {
        GPU_line_width(2.5);
      }
      else {
        GPU_line_width(1.0);
      }

      /* anti-aliased lines for less jagged appearance */
      if ((sipo->flag & SIPO_BEAUTYDRAW_OFF) == 0) {
        GPU_line_smooth(true);
      }
      GPU_blend(true);

      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

      float viewport_size[4];
      GPU_viewport_size_get_f(viewport_size);
      immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

      immUniform1i("colors_len", 0); /* Simple dashes. */

      if (BKE_fcurve_is_protected(fcu)) {
        /* protected curves (non editable) are drawn with dotted lines */
        immUniform1f("dash_width", 4.0f);
        immUniform1f("dash_factor", 0.5f);
      }
      else {
        immUniform1f("dash_factor", 2.0f); /* solid line */
      }

      if (((fcu->grp) && (fcu->grp->flag & AGRP_MUTED)) || (fcu->flag & FCURVE_MUTED)) {
        /* muted curves are drawn in a grayish hue */
        /* XXX should we have some variations? */
        immUniformThemeColorShade(TH_HEADER, 50);
      }
      else {
        /* set whatever color the curve has set
         * - unselected curves draw less opaque to help distinguish the selected ones
         */
        immUniformColor3fvAlpha(fcu->color, fcurve_display_alpha(fcu));
      }

      /* draw F-Curve */
      if ((fcu->modifiers.first) || (fcu->flag & FCURVE_INT_VALUES)) {
        /* draw a curve affected by modifiers or only allowed to have integer values
         * by sampling it at various small-intervals over the visible region
         */
        draw_fcurve_curve(ac, ale->id, fcu, &ar->v2d, grid, shdr_pos);
      }
      else if (((fcu->bezt) || (fcu->fpt)) && (fcu->totvert)) {
        /* just draw curve based on defined data (i.e. no modifiers) */
        if (fcu->bezt) {
          if (fcurve_can_use_simple_bezt_drawing(fcu)) {
            draw_fcurve_curve_bezts(ac, ale->id, fcu, &ar->v2d, shdr_pos);
          }
          else {
            draw_fcurve_curve(ac, ale->id, fcu, &ar->v2d, grid, shdr_pos);
          }
        }
        else if (fcu->fpt) {
          draw_fcurve_curve_samples(ac, ale->id, fcu, &ar->v2d, shdr_pos);
        }
      }

      immUnbindProgram();

      if ((sipo->flag & SIPO_BEAUTYDRAW_OFF) == 0) {
        GPU_line_smooth(false);
      }
      GPU_blend(false);
    }

    /* 2) draw handles and vertices as appropriate based on active
     * - If the option to only show controls if the F-Curve is selected is enabled,
     *   we must obey this.
     */
    if (!(sipo->flag & SIPO_SELCUVERTSONLY) || (fcu->flag & FCURVE_SELECTED)) {
      if (!fcurve_are_keyframes_usable(fcu) && !(fcu->fpt && fcu->totvert)) {
        /* only draw controls if this is the active modifier */
        if ((fcu->flag & FCURVE_ACTIVE) && (fcm)) {
          switch (fcm->type) {
            case FMODIFIER_TYPE_ENVELOPE: /* envelope */
              draw_fcurve_modifier_controls_envelope(fcm, &ar->v2d);
              break;
          }
        }
      }
      else if (((fcu->bezt) || (fcu->fpt)) && (fcu->totvert)) {
        short mapping_flag = ANIM_get_normalization_flags(ac);
        float offset;
        float unit_scale = ANIM_unit_mapping_get_factor(
            ac->scene, ale->id, fcu, mapping_flag, &offset);

        /* apply unit-scaling to all values via OpenGL */
        GPU_matrix_push();
        GPU_matrix_scale_2f(1.0f, unit_scale);
        GPU_matrix_translate_2f(0.0f, offset);

        /* Set this once and for all -
         * all handles and handle-verts should use the same thickness. */
        GPU_line_width(1.0);

        if (fcu->bezt) {
          bool do_handles = draw_fcurve_handles_check(sipo, fcu);

          if (do_handles) {
            /* only draw handles/vertices on keyframes */
            GPU_blend(true);
            draw_fcurve_handles(sipo, fcu);
            GPU_blend(false);
          }

          draw_fcurve_vertices(ar, fcu, do_handles, (sipo->flag & SIPO_SELVHANDLESONLY));
        }
        else {
          /* samples: only draw two indicators at either end as indicators */
          draw_fcurve_samples(sipo, ar, fcu);
        }

        GPU_matrix_pop();
      }
    }

    /* 3) draw driver debugging stuff */
    if ((ac->datatype == ANIMCONT_DRIVERS) && (fcu->flag & FCURVE_ACTIVE)) {
      graph_draw_driver_debug(ac, ale->id, fcu);
    }

    /* undo mapping of keyframes for drawing if scaled F-Curve */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0);
    }
  }

  /* free list of curves */
  ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************* */
/* Channel List */

/* left hand part */
void graph_draw_channel_names(bContext *C, bAnimContext *ac, ARegion *ar)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  View2D *v2d = &ar->v2d;
  float y = 0.0f, height;
  size_t items;
  int i = 0;

  /* build list of channels to draw */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Update max-extent of channels here (taking into account scrollers):
   * - this is done to allow the channel list to be scrollable, but must be done here
   *   to avoid regenerating the list again and/or also because channels list is drawn first
   * - offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for
   *   start of list offset, and the second is as a correction for the scrollers.
   */
  height = (float)((items * ACHANNEL_STEP(ac)) + (ACHANNEL_HEIGHT(ac) * 2));
  UI_view2d_totRect_set(v2d, BLI_rcti_size_x(&ar->v2d.mask), height);

  /* loop through channels, and set up drawing depending on their type  */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t channel_index = 0;

    y = (float)ACHANNEL_FIRST(ac);

    for (ale = anim_data.first, i = 0; ale; ale = ale->next, i++) {
      const float yminc = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
      const float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF(ac));

      /* check if visible */
      if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        ANIM_channel_draw(ac, ale, yminc, ymaxc, channel_index);
      }

      /* adjust y-position for next one */
      y -= ACHANNEL_STEP(ac);
      channel_index++;
    }
  }
  { /* second pass: widgets */
    uiBlock *block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
    size_t channel_index = 0;

    y = (float)ACHANNEL_FIRST(ac);

    /* set blending again, as may not be set in previous step */
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);

    for (ale = anim_data.first, i = 0; ale; ale = ale->next, i++) {
      const float yminc = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
      const float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF(ac));

      /* check if visible */
      if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        rctf channel_rect;
        BLI_rctf_init(&channel_rect, 0, v2d->cur.xmax - V2D_SCROLL_WIDTH, yminc, ymaxc);
        ANIM_channel_draw_widgets(C, ac, ale, block, &channel_rect, channel_index);
      }

      /* adjust y-position for next one */
      y -= ACHANNEL_STEP(ac);
      channel_index++;
    }

    UI_block_end(C, block);
    UI_block_draw(C, block);

    GPU_blend(false);
  }

  /* free tempolary channels */
  ANIM_animdata_freelist(&anim_data);
}

/* SPDX-FileCopyrightText: Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_anim_api.h"

#include "graph_intern.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

static void graph_draw_driver_debug(bAnimContext *ac, ID *id, FCurve *fcu);

/* -------------------------------------------------------------------- */
/** \name Utility Drawing Defines
 * \{ */

/* determine the alpha value that should be used when
 * drawing components for some F-Curve (fcu)
 * - selected F-Curves should be more visible than partially visible ones
 */
static float fcurve_display_alpha(FCurve *fcu)
{
  return (fcu->flag & FCURVE_SELECTED) ? 1.0f : U.fcu_inactive_alpha;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FCurve Modifier Drawing
 * \{ */

/* Envelope -------------- */

/* TODO: draw a shaded poly showing the region of influence too!!! */
/**
 * \param adt_nla_remap: Send nullptr if no NLA remapping necessary.
 */
static void draw_fcurve_modifier_controls_envelope(FModifier *fcm,
                                                   View2D *v2d,
                                                   AnimData *adt_nla_remap)
{
  FMod_Envelope *env = (FMod_Envelope *)fcm->data;
  FCM_EnvelopeData *fed;
  const float fac = 0.05f * BLI_rctf_size_x(&v2d->cur);
  int i;

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniformColor3f(0.0f, 0.0f, 0.0f);
  immUniform1f("dash_width", 10.0f);
  immUniform1f("udash_factor", 0.5f);

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

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    /* for now, point color is fixed, and is white */
    immUniformColor3f(1.0f, 1.0f, 1.0f);

    immBeginAtMost(GPU_PRIM_POINTS, env->totvert * 2);

    for (i = 0, fed = env->data; i < env->totvert; i++, fed++) {
      const float env_scene_time = BKE_nla_tweakedit_remap(
          adt_nla_remap, fed->time, NLATIME_CONVERT_MAP);

      /* only draw if visible
       * - min/max here are fixed, not relative
       */
      if (IN_RANGE(env_scene_time, (v2d->cur.xmin - fac), (v2d->cur.xmax + fac))) {
        immVertex2f(shdr_pos, env_scene_time, fed->min);
        immVertex2f(shdr_pos, env_scene_time, fed->max);
      }
    }

    immEnd();

    immUnbindProgram();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FCurve Modifier Drawing
 * \{ */

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
    UI_GetThemeColor3fv(TH_VERTEX, color);
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

/* Draw a cross at the given position. Shader must already be bound.
 * NOTE: the caller MUST HAVE GL_LINE_SMOOTH & GL_BLEND ENABLED, otherwise the controls don't
 * have a consistent appearance (due to off-pixel alignments).
 */
static void draw_cross(float position[2], float scale[2], uint attr_id)
{
  GPU_matrix_push();
  GPU_matrix_translate_2fv(position);
  GPU_matrix_scale_2f(1.0f / scale[0], 1.0f / scale[1]);

  /* Draw X shape. */
  const float line_length = 0.7f;
  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(attr_id, -line_length, -line_length);
  immVertex2f(attr_id, +line_length, +line_length);

  immVertex2f(attr_id, -line_length, +line_length);
  immVertex2f(attr_id, +line_length, -line_length);
  immEnd();

  GPU_matrix_pop();
}

static void draw_fcurve_selected_keyframe_vertices(FCurve *fcu, View2D *v2d, bool sel, uint pos)
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
      /* 'Keyframe' vertex only, as handle lines and handles have already been drawn
       * - only draw those with correct selection state for the current drawing color
       * -
       */
      if ((bezt->f2 & SELECT) == sel) {
        immVertex2fv(pos, bezt->vec[1]);
      }
    }
  }

  immEnd();
}

static void draw_locked_keyframe_vertices(FCurve *fcu,
                                          View2D *v2d,
                                          const uint attr_id,
                                          const float unit_scale)
{
  const float correction_factor = 0.05f * BLI_rctf_size_x(&v2d->cur);

  /* get view settings */
  const float vertex_size = UI_GetThemeValuef(TH_VERTEX_SIZE);
  float scale[2];
  UI_view2d_scale_get(v2d, &scale[0], &scale[1]);
  scale[0] /= vertex_size;
  /* Dividing by the unit scale is needed to display euler correctly (internally they are radians
   * but displayed as degrees) and all curves when normalization is turned on. */
  scale[1] = scale[1] / vertex_size * unit_scale;

  set_fcurve_vertex_color(fcu, false);

  for (int i = 0; i < fcu->totvert; i++) {
    BezTriple *bezt = &fcu->bezt[i];
    if (!IN_RANGE(bezt->vec[1][0],
                  (v2d->cur.xmin - correction_factor),
                  (v2d->cur.xmax + correction_factor)))
    {
      continue;
    }
    float position[2] = {bezt->vec[1][0], bezt->vec[1][1]};
    draw_cross(position, scale, attr_id);
  }
}

/**
 * Draw the extra indicator for the active point.
 */
static void draw_fcurve_active_vertex(const FCurve *fcu, const View2D *v2d, const uint pos)
{
  const int active_keyframe_index = BKE_fcurve_active_keyframe_index(fcu);
  if (!(fcu->flag & FCURVE_ACTIVE) || active_keyframe_index == FCURVE_ACTIVE_KEYFRAME_NONE) {
    return;
  }

  const float fac = 0.05f * BLI_rctf_size_x(&v2d->cur);
  const BezTriple *bezt = &fcu->bezt[active_keyframe_index];

  if (!IN_RANGE(bezt->vec[1][0], (v2d->cur.xmin - fac), (v2d->cur.xmax + fac))) {
    return;
  }
  if (!(bezt->f2 & SELECT)) {
    return;
  }

  immBegin(GPU_PRIM_POINTS, 1);
  immUniformThemeColor(TH_VERTEX_ACTIVE);
  immVertex2fv(pos, bezt->vec[1]);
  immEnd();
}

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_keyframe_vertices(
    FCurve *fcu, View2D *v2d, bool edit, const uint pos, const float unit_scale)
{
  if (edit) {
    immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

    immUniform1f("size", UI_GetThemeValuef(TH_VERTEX_SIZE) * UI_SCALE_FAC);

    draw_fcurve_selected_keyframe_vertices(fcu, v2d, false, pos);
    draw_fcurve_selected_keyframe_vertices(fcu, v2d, true, pos);
    draw_fcurve_active_vertex(fcu, v2d, pos);

    immUnbindProgram();
  }
  else {
    if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
      GPU_line_smooth(true);
    }
    GPU_blend(GPU_BLEND_ALPHA);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    draw_locked_keyframe_vertices(fcu, v2d, pos, unit_scale);

    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
    if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
      GPU_line_smooth(false);
    }
  }
}

/* helper func - draw handle vertices only for an F-Curve (if it is not protected) */
static void draw_fcurve_selected_handle_vertices(
    FCurve *fcu, View2D *v2d, bool sel, bool sel_handle_only, uint pos)
{
  (void)v2d; /* TODO: use this to draw only points in view */

  /* set handle color */
  float hcolor[3];
  UI_GetThemeColor3fv(sel ? TH_HANDLE_VERTEX_SELECT : TH_HANDLE_VERTEX, hcolor);
  immUniform4f("outlineColor", hcolor[0], hcolor[1], hcolor[2], 1.0f);
  immUniformColor3fvAlpha(hcolor, 0.01f); /* almost invisible - only keep for smoothness */

  immBeginAtMost(GPU_PRIM_POINTS, fcu->totvert * 2);

  BezTriple *bezt = fcu->bezt;
  BezTriple *prevbezt = nullptr;
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
            /* && v2d->cur.xmin < bezt->vec[0][0] < v2d->cur.xmax) */)
        {
          immVertex2fv(pos, bezt->vec[0]);
        }
      }

      if (bezt->ipo == BEZT_IPO_BEZ) {
        if ((bezt->f3 & SELECT) == sel
            /* && v2d->cur.xmin < bezt->vec[2][0] < v2d->cur.xmax) */)
        {
          immVertex2fv(pos, bezt->vec[2]);
        }
      }
    }
  }

  immEnd();
}

/**
 * Draw the extra handles for the active point.
 */
static void draw_fcurve_active_handle_vertices(const FCurve *fcu,
                                               const bool sel_handle_only,
                                               const uint pos)
{
  const int active_keyframe_index = BKE_fcurve_active_keyframe_index(fcu);
  if (!(fcu->flag & FCURVE_ACTIVE) || active_keyframe_index == FCURVE_ACTIVE_KEYFRAME_NONE) {
    return;
  }

  const BezTriple *bezt = &fcu->bezt[active_keyframe_index];

  if (sel_handle_only && !BEZT_ISSEL_ANY(bezt)) {
    return;
  }

  float active_col[4];
  UI_GetThemeColor4fv(TH_VERTEX_ACTIVE, active_col);
  immUniform4fv("outlineColor", active_col);
  immUniformColor3fvAlpha(active_col, 0.01f); /* Almost invisible - only keep for smoothness. */
  immBeginAtMost(GPU_PRIM_POINTS, 2);

  const BezTriple *left_bezt = active_keyframe_index > 0 ? &fcu->bezt[active_keyframe_index - 1] :
                                                           bezt;
  if (left_bezt->ipo == BEZT_IPO_BEZ && (bezt->f1 & SELECT)) {
    immVertex2fv(pos, bezt->vec[0]);
  }
  if (bezt->ipo == BEZT_IPO_BEZ && (bezt->f3 & SELECT)) {
    immVertex2fv(pos, bezt->vec[2]);
  }
  immEnd();
}

/* helper func - draw handle vertices only for an F-Curve (if it is not protected) */
static void draw_fcurve_handle_vertices(FCurve *fcu, View2D *v2d, bool sel_handle_only, uint pos)
{
  /* smooth outlines for more consistent appearance */
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);

  /* set handle size */
  immUniform1f("size", (1.4f * UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE)) * UI_SCALE_FAC);
  immUniform1f("outlineWidth", 1.5f * UI_SCALE_FAC);

  draw_fcurve_selected_handle_vertices(fcu, v2d, false, sel_handle_only, pos);
  draw_fcurve_selected_handle_vertices(fcu, v2d, true, sel_handle_only, pos);
  draw_fcurve_active_handle_vertices(fcu, sel_handle_only, pos);

  immUnbindProgram();
}

static void draw_fcurve_vertices(
    ARegion *region, FCurve *fcu, bool do_handles, bool sel_handle_only, const float unit_scale)
{
  View2D *v2d = &region->v2d;

  /* only draw points if curve is visible
   * - Draw unselected points before selected points as separate passes
   *    to make sure in the case of overlapping points that the selected is always visible
   * - Draw handles before keyframes, so that keyframes will overlap handles
   *   (keyframes are more important for users).
   */

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_program_point_size(true);

  /* draw the two handles first (if they're shown, the curve doesn't
   * have just a single keyframe, and the curve is being edited) */
  if (do_handles) {
    draw_fcurve_handle_vertices(fcu, v2d, sel_handle_only, pos);
  }

  /* draw keyframes over the handles */
  draw_fcurve_keyframe_vertices(fcu, v2d, !(fcu->flag & FCURVE_PROTECTED), pos, unit_scale);

  GPU_program_point_size(false);
  GPU_blend(GPU_BLEND_NONE);
}

/* Handles ---------------- */

static bool draw_fcurve_handles_check(SpaceGraph *sipo, FCurve *fcu)
{
  /* don't draw handle lines if handles are not to be shown */
  if (/* handles shouldn't be shown anywhere */
      (sipo->flag & SIPO_NOHANDLES) ||
      /* keyframes aren't editable */
      (fcu->flag & FCURVE_PROTECTED) ||
#if 0
      /* handles can still be selected and handle types set, better draw - campbell */
      /* editing the handles here will cause weird/incorrect interpolation issues */
      (fcu->flag & FCURVE_INT_VALUES) ||
#endif
      /* group that curve belongs to is not editable */
      ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)))
  {
    return false;
  }
  return true;
}

/* draw lines for F-Curve handles only (this is only done in EditMode)
 * NOTE: draw_fcurve_handles_check must be checked before running this. */
static void draw_fcurve_handles(SpaceGraph *sipo, FCurve *fcu)
{
  int sel, b;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
  if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
    GPU_line_smooth(true);
  }
  GPU_blend(GPU_BLEND_ALPHA);

  immBeginAtMost(GPU_PRIM_LINES, 4 * 2 * fcu->totvert);

  /* slightly hacky, but we want to draw unselected points before selected ones
   * so that selected points are clearly visible
   */
  for (sel = 0; sel < 2; sel++) {
    BezTriple *bezt = fcu->bezt, *prevbezt = nullptr;
    int basecol = (sel) ? TH_HANDLE_SEL_FREE : TH_HANDLE_FREE;
    uchar col[4];

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
        /* only draw first handle if previous segment had handles */
        if ((!prevbezt && (bezt->ipo == BEZT_IPO_BEZ)) ||
            (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ))) {
          UI_GetThemeColor3ubv(basecol + bezt->h1, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[0]);
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[1]);
        }

        /* only draw second handle if this segment is bezier */
        if (bezt->ipo == BEZT_IPO_BEZ) {
          UI_GetThemeColor3ubv(basecol + bezt->h2, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[1]);
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[2]);
        }
      }
      else {
        /* only draw first handle if previous segment was had handles, and selection is ok */
        if (((bezt->f1 & SELECT) == sel) && ((!prevbezt && (bezt->ipo == BEZT_IPO_BEZ)) ||
                                             (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ))))
        {
          UI_GetThemeColor3ubv(basecol + bezt->h1, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[0]);
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[1]);
        }

        /* only draw second handle if this segment is bezier, and selection is ok */
        if (((bezt->f3 & SELECT) == sel) && (bezt->ipo == BEZT_IPO_BEZ)) {
          UI_GetThemeColor3ubv(basecol + bezt->h2, col);
          col[3] = fcurve_display_alpha(fcu) * 255;
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[0]);
          immAttr4ubv(color, col);
          immVertex2fv(pos, bezt->vec[1]);
        }
      }
    }
  }

  immEnd();
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
  if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
    GPU_line_smooth(false);
  }
}

/* Samples ---------------- */

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_samples(ARegion *region, FCurve *fcu, const float unit_scale)
{
  FPoint *first, *last;
  float scale[2];

  /* get view settings */
  const float hsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
  UI_view2d_scale_get(&region->v2d, &scale[0], &scale[1]);

  scale[0] /= hsize;
  scale[1] /= hsize / unit_scale;

  /* get verts */
  first = fcu->fpt;
  last = (first) ? (first + (fcu->totvert - 1)) : (nullptr);

  /* draw */
  if (first && last) {
    /* anti-aliased lines for more consistent appearance */
    if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
      GPU_line_smooth(true);
    }
    GPU_blend(GPU_BLEND_ALPHA);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    immUniformThemeColor((fcu->flag & FCURVE_SELECTED) ? TH_TEXT_HI : TH_TEXT);

    draw_cross(first->vec, scale, pos);
    draw_cross(last->vec, scale, pos);

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
      GPU_line_smooth(false);
    }
  }
}

/* Curve ---------------- */

/* Helper func - just draw the F-Curve by sampling the visible region
 * (for drawing curves with modifiers). */
static void draw_fcurve_curve(bAnimContext *ac,
                              ID *id,
                              FCurve *fcu_,
                              View2D *v2d,
                              uint pos,
                              const bool use_nla_remap,
                              const bool draw_extrapolation)
{
  short mapping_flag = ANIM_get_normalization_flags(ac);

  /* when opening a blend file on a different sized screen or while dragging the toolbar this can
   * happen best just bail out in this case. */
  if (UI_view2d_scale_get_x(v2d) <= 0.0f) {
    return;
  }

  /* disable any drivers */
  FCurve fcurve_for_draw = *fcu_;
  fcurve_for_draw.driver = nullptr;

  /* compute unit correction factor */
  float offset;
  float unitFac = ANIM_unit_mapping_get_factor(
      ac->scene, id, &fcurve_for_draw, mapping_flag, &offset);

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
   * chosen here is just the coarsest value which still looks reasonable.
   */

  /* TODO: perhaps we should have 1.0 frames
   * as upper limit so that curves don't get too distorted? */
  float pixels_per_sample = 1.5f;
  float samplefreq = pixels_per_sample / UI_view2d_scale_get_x(v2d);

  if (!(U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING)) {
    /* Low Precision = coarse lower-bound clamping
     *
     * Although the "Beauty Draw" flag was originally for AA'd
     * line drawing, the sampling rate here has a much greater
     * impact on performance (e.g. for #40372)!
     *
     * This one still amounts to 10 sample-frames for each 1-frame interval
     * which should be quite a decent approximation in many situations.
     */
    if (samplefreq < 0.1f) {
      samplefreq = 0.1f;
    }
  }
  else {
    /* "Higher Precision" but slower - especially on larger windows (e.g. #40372) */
    if (samplefreq < 0.00001f) {
      samplefreq = 0.00001f;
    }
  }

  /* the start/end times are simply the horizontal extents of the 'cur' rect */
  float stime = v2d->cur.xmin;
  float etime = v2d->cur.xmax;

  AnimData *adt = use_nla_remap ? BKE_animdata_from_id(id) : nullptr;

  /* If not drawing extrapolation, then change fcurve drawing bounds to its keyframe bounds clamped
   * by graph editor bounds. */
  if (!draw_extrapolation) {
    float fcu_start = 0;
    float fcu_end = 0;
    BKE_fcurve_calc_range(fcu_, &fcu_start, &fcu_end, false);

    fcu_start = BKE_nla_tweakedit_remap(adt, fcu_start, NLATIME_CONVERT_MAP);
    fcu_end = BKE_nla_tweakedit_remap(adt, fcu_end, NLATIME_CONVERT_MAP);

    /* Account for reversed NLA strip effect. */
    if (fcu_end < fcu_start) {
      SWAP(float, fcu_start, fcu_end);
    }

    /* Clamp to graph editor rendering bounds. */
    stime = max_ff(stime, fcu_start);
    etime = min_ff(etime, fcu_end);
  }

  const int total_samples = roundf((etime - stime) / samplefreq);
  if (total_samples <= 0) {
    return;
  }

  /* NLA remapping is linear so we don't have to remap per iteration. */
  const float eval_start = BKE_nla_tweakedit_remap(adt, stime, NLATIME_CONVERT_UNMAP);
  const float eval_freq = BKE_nla_tweakedit_remap(adt, stime + samplefreq, NLATIME_CONVERT_UNMAP) -
                          eval_start;
  const float eval_end = BKE_nla_tweakedit_remap(adt, etime, NLATIME_CONVERT_UNMAP);

  immBegin(GPU_PRIM_LINE_STRIP, (total_samples + 1));

  /* At each sampling interval, add a new vertex.
   *
   * Apply the unit correction factor to the calculated values so that the displayed values appear
   * correctly in the viewport.
   */
  for (int i = 0; i < total_samples; i++) {
    const float ctime = stime + i * samplefreq;
    float eval_time = eval_start + i * eval_freq;

    /* Prevent drawing past bounds, due to floating point problems.
     * User-wise, prevent visual flickering.
     *
     * This is to cover the case where:
     * eval_start + total_samples * eval_freq > eval_end
     * due to floating point problems.
     */
    if (eval_time > eval_end) {
      eval_time = eval_end;
    }

    immVertex2f(pos, ctime, (evaluate_fcurve(&fcurve_for_draw, eval_time) + offset) * unitFac);
  }

  /* Ensure we include end boundary point.
   * User-wise, prevent visual flickering.
   *
   * This is to cover the case where:
   * eval_start + total_samples * eval_freq < eval_end
   * due to floating point problems.
   */
  immVertex2f(pos, etime, (evaluate_fcurve(&fcurve_for_draw, eval_end) + offset) * unitFac);

  immEnd();
}

/* helper func - draw a samples-based F-Curve */
static void draw_fcurve_curve_samples(bAnimContext *ac,
                                      ID *id,
                                      FCurve *fcu,
                                      View2D *v2d,
                                      const uint shdr_pos,
                                      const bool draw_extrapolation)
{
  if (!draw_extrapolation && fcu->totvert == 1) {
    return;
  }

  FPoint *prevfpt = fcu->fpt;
  FPoint *fpt = prevfpt + 1;
  float fac, v[2];
  int b = fcu->totvert;
  float unit_scale, offset;
  short mapping_flag = ANIM_get_normalization_flags(ac);
  int count = fcu->totvert;

  const bool extrap_left = draw_extrapolation && prevfpt->vec[0] > v2d->cur.xmin;
  if (extrap_left) {
    count++;
  }

  const bool extrap_right = draw_extrapolation && (prevfpt + b - 1)->vec[0] < v2d->cur.xmax;
  if (extrap_right) {
    count++;
  }

  /* apply unit mapping */
  GPU_matrix_push();
  unit_scale = ANIM_unit_mapping_get_factor(ac->scene, id, fcu, mapping_flag, &offset);
  GPU_matrix_scale_2f(1.0f, unit_scale);
  GPU_matrix_translate_2f(0.0f, offset);

  immBegin(GPU_PRIM_LINE_STRIP, count);

  /* extrapolate to left? - left-side of view comes before first keyframe? */
  if (extrap_left) {
    v[0] = v2d->cur.xmin;

    /* y-value depends on the interpolation */
    if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) ||
        (fcu->totvert == 1))
    {
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
  if (extrap_right) {
    v[0] = v2d->cur.xmax;

    /* y-value depends on the interpolation */
    if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) ||
        (fcu->totvert == 1))
    {
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

static int calculate_bezt_draw_resolution(BezTriple *bezt,
                                          BezTriple *prevbezt,
                                          const blender::float2 resolution_scale)
{
  const int resolution_x = int((bezt->vec[1][0] - prevbezt->vec[1][0]) * resolution_scale[0]);
  /* Include the handles in the resolution calculation to cover the case where keys have the same
   * y-value, but their handles are offset to create an arc. */
  const float min_y = min_ffff(
      bezt->vec[1][1], bezt->vec[2][1], prevbezt->vec[1][1], prevbezt->vec[0][1]);
  const float max_y = max_ffff(
      bezt->vec[1][1], bezt->vec[2][1], prevbezt->vec[1][1], prevbezt->vec[0][1]);
  const int resolution_y = int((max_y - min_y) * resolution_scale[1]);
  /* Using a simple sum instead of calculating the diagonal. This gives a slightly higher
   * resolution but it does compensate for the fact that bezier curves can create long arcs between
   * keys. */
  return resolution_x + resolution_y;
}

/**
 * Add points on the bezier between \param prevbezt and \param bezt to \param curve_vertices. The
 * amount of points added is based on the given \param resolution.
 */
static void add_bezt_vertices(BezTriple *bezt,
                              BezTriple *prevbezt,
                              int resolution,
                              blender::Vector<blender::float2> &curve_vertices)
{
  if (resolution < 2) {
    curve_vertices.append({prevbezt->vec[1][0], prevbezt->vec[1][1]});
    return;
  }

  /* If the resolution goes too high the line will not end exactly at the keyframe. Probably due to
   * accumulating floating point issues in BKE_curve_forward_diff_bezier.*/
  resolution = min_ii(64, resolution);

  float prev_key[2], prev_handle[2], bez_handle[2], bez_key[2];
  /* Allocation needs +1 on resolution because BKE_curve_forward_diff_bezier uses it to iterate
   * inclusively. */
  float *bezier_diff_points = static_cast<float *>(
      MEM_mallocN(sizeof(float) * ((resolution + 1) * 2), "Draw bezt data"));

  prev_key[0] = prevbezt->vec[1][0];
  prev_key[1] = prevbezt->vec[1][1];
  prev_handle[0] = prevbezt->vec[2][0];
  prev_handle[1] = prevbezt->vec[2][1];

  bez_handle[0] = bezt->vec[0][0];
  bez_handle[1] = bezt->vec[0][1];
  bez_key[0] = bezt->vec[1][0];
  bez_key[1] = bezt->vec[1][1];

  BKE_fcurve_correct_bezpart(prev_key, prev_handle, bez_handle, bez_key);

  BKE_curve_forward_diff_bezier(prev_key[0],
                                prev_handle[0],
                                bez_handle[0],
                                bez_key[0],
                                bezier_diff_points,
                                resolution,
                                sizeof(float[2]));
  BKE_curve_forward_diff_bezier(prev_key[1],
                                prev_handle[1],
                                bez_handle[1],
                                bez_key[1],
                                bezier_diff_points + 1,
                                resolution,
                                sizeof(float[2]));

  for (float *fp = bezier_diff_points; resolution; resolution--, fp += 2) {
    const float x = *fp;
    const float y = *(fp + 1);
    curve_vertices.append({x, y});
  }
  MEM_freeN(bezier_diff_points);
}

/** Get the first and last index to the bezt array that are just outside min and max. */
static blender::int2 get_bounding_bezt_indices(FCurve *fcu, const float min, const float max)
{
  bool replace;
  int first, last;
  first = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, min, fcu->totvert, &replace);
  first = clamp_i(first - 1, 0, fcu->totvert - 1);

  last = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, max, fcu->totvert, &replace);
  last = replace ? last + 1 : last;
  last = clamp_i(last, 0, fcu->totvert - 1);
  return {first, last};
}

static void add_extrapolation_point_left(FCurve *fcu,
                                         const float v2d_xmin,
                                         blender::Vector<blender::float2> &curve_vertices)
{
  /* left-side of view comes before first keyframe, so need to extend as not cyclic */
  float vertex_position[2];
  vertex_position[0] = v2d_xmin;
  BezTriple *bezt = &fcu->bezt[0];

  /* y-value depends on the interpolation */
  if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (bezt->ipo == BEZT_IPO_CONST) ||
      (bezt->ipo == BEZT_IPO_LIN && fcu->totvert == 1))
  {
    /* just extend across the first keyframe's value */
    vertex_position[1] = bezt->vec[1][1];
  }
  else if (bezt->ipo == BEZT_IPO_LIN) {
    BezTriple *next_bezt = bezt + 1;
    /* extrapolate linear doesn't use the handle, use the next points center instead */
    float fac = (bezt->vec[1][0] - next_bezt->vec[1][0]) / (bezt->vec[1][0] - vertex_position[0]);
    if (fac) {
      fac = 1.0f / fac;
    }
    vertex_position[1] = bezt->vec[1][1] - fac * (bezt->vec[1][1] - next_bezt->vec[1][1]);
  }
  else {
    /* based on angle of handle 1 (relative to keyframe) */
    float fac = (bezt->vec[0][0] - bezt->vec[1][0]) / (bezt->vec[1][0] - vertex_position[0]);
    if (fac) {
      fac = 1.0f / fac;
    }
    vertex_position[1] = bezt->vec[1][1] - fac * (bezt->vec[0][1] - bezt->vec[1][1]);
  }

  curve_vertices.append(vertex_position);
}

static void add_extrapolation_point_right(FCurve *fcu,
                                          const float v2d_xmax,
                                          blender::Vector<blender::float2> &curve_vertices)
{
  float vertex_position[2];
  vertex_position[0] = v2d_xmax;
  BezTriple *bezt = &fcu->bezt[fcu->totvert - 1];

  /* y-value depends on the interpolation. */
  if ((fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) ||
      (bezt->ipo == BEZT_IPO_CONST) || (bezt->ipo == BEZT_IPO_LIN && fcu->totvert == 1))
  {
    /* based on last keyframe's value */
    vertex_position[1] = bezt->vec[1][1];
  }
  else if (bezt->ipo == BEZT_IPO_LIN) {
    /* Extrapolate linear doesn't use the handle, use the previous points center instead. */
    BezTriple *prev_bezt = bezt - 1;
    float fac = (bezt->vec[1][0] - prev_bezt->vec[1][0]) / (bezt->vec[1][0] - vertex_position[0]);
    if (fac) {
      fac = 1.0f / fac;
    }
    vertex_position[1] = bezt->vec[1][1] - fac * (bezt->vec[1][1] - prev_bezt->vec[1][1]);
  }
  else {
    /* Based on angle of handle 1 (relative to keyframe). */
    float fac = (bezt->vec[2][0] - bezt->vec[1][0]) / (bezt->vec[1][0] - vertex_position[0]);
    if (fac) {
      fac = 1.0f / fac;
    }
    vertex_position[1] = bezt->vec[1][1] - fac * (bezt->vec[2][1] - bezt->vec[1][1]);
  }

  curve_vertices.append(vertex_position);
}

static blender::float2 calculate_resolution_scale(View2D *v2d)
{
  /* The resolution for bezier forward diff in frame/value space. This ensures a constant
   * resolution in screenspace. */
  const int window_width = BLI_rcti_size_x(&v2d->mask);
  const int window_height = BLI_rcti_size_y(&v2d->mask);
  const float points_per_pixel = 0.25f;

  const float v2d_frame_range = BLI_rctf_size_x(&v2d->cur);
  const float v2d_value_range = BLI_rctf_size_y(&v2d->cur);
  const blender::float2 resolution_scale = {(window_width * points_per_pixel) / v2d_frame_range,
                                            (window_height * points_per_pixel) / v2d_value_range};
  return resolution_scale;
}

/* Helper function - draw one repeat of an F-Curve (using Bezier curve approximations). */
static void draw_fcurve_curve_bezts(
    bAnimContext *ac, ID *id, FCurve *fcu, View2D *v2d, uint pos, const bool draw_extrapolation)
{
  using namespace blender;
  if (!draw_extrapolation && fcu->totvert == 1) {
    return;
  }

  /* Apply unit mapping. */
  GPU_matrix_push();
  float offset;
  short mapping_flag = ANIM_get_normalization_flags(ac);
  const float unit_scale = ANIM_unit_mapping_get_factor(ac->scene, id, fcu, mapping_flag, &offset);
  GPU_matrix_scale_2f(1.0f, unit_scale);
  GPU_matrix_translate_2f(0.0f, offset);

  Vector<float2> curve_vertices;

  /* Extrapolate to the left? */
  if (draw_extrapolation && fcu->bezt[0].vec[1][0] > v2d->cur.xmin) {
    add_extrapolation_point_left(fcu, v2d->cur.xmin, curve_vertices);
  }

  const int2 bounding_indices = get_bounding_bezt_indices(fcu, v2d->cur.xmin, v2d->cur.xmax);

  /* This happens if there is only 1 frame in the curve or the view is only showing the
   * extrapolation zone of the curve. */
  if (bounding_indices[0] == bounding_indices[1]) {
    BezTriple *bezt = &fcu->bezt[bounding_indices[0]];
    curve_vertices.append({bezt->vec[1][0], bezt->vec[1][1]});
  }

  const blender::float2 resolution_scale = calculate_resolution_scale(v2d);
  /* Draw curve between first and last keyframe (if there are enough to do so). */
  for (int i = bounding_indices[0] + 1; i <= bounding_indices[1]; i++) {
    BezTriple *prevbezt = &fcu->bezt[i - 1];
    BezTriple *bezt = &fcu->bezt[i];

    if (prevbezt->ipo == BEZT_IPO_CONST) {
      /* Constant-Interpolation: draw segment between previous keyframe and next,
       * but holding same value */
      curve_vertices.append({prevbezt->vec[1][0], prevbezt->vec[1][1]});
      curve_vertices.append({bezt->vec[1][0], prevbezt->vec[1][1]});
    }
    else if (prevbezt->ipo == BEZT_IPO_LIN) {
      /* Linear interpolation: just add one point (which should add a new line segment) */
      curve_vertices.append({prevbezt->vec[1][0], prevbezt->vec[1][1]});
    }
    else if (prevbezt->ipo == BEZT_IPO_BEZ) {
      const int resolution = calculate_bezt_draw_resolution(bezt, prevbezt, resolution_scale);
      add_bezt_vertices(bezt, prevbezt, resolution, curve_vertices);
    }

    /* Last point? */
    if (i == bounding_indices[1]) {
      curve_vertices.append({bezt->vec[1][0], bezt->vec[1][1]});
    }
  }

  /* Extrapolate to the right? (see code for left-extrapolation above too) */
  if (draw_extrapolation && fcu->bezt[fcu->totvert - 1].vec[1][0] < v2d->cur.xmax) {
    add_extrapolation_point_right(fcu, v2d->cur.xmax, curve_vertices);
  }

  if (curve_vertices.size() < 2) {
    GPU_matrix_pop();
    return;
  }

  immBegin(GPU_PRIM_LINE_STRIP, curve_vertices.size());
  for (const float2 vertex : curve_vertices) {
    immVertex2fv(pos, vertex);
  }
  immEnd();

  GPU_matrix_pop();
}

static void draw_fcurve(bAnimContext *ac, SpaceGraph *sipo, ARegion *region, bAnimListElem *ale)
{
  FCurve *fcu = (FCurve *)ale->key_data;
  FModifier *fcm = find_active_fmodifier(&fcu->modifiers);
  AnimData *adt = ANIM_nla_mapping_get(ac, ale);

  /* map keyframes for drawing if scaled F-Curve */
  ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, false);

  /* draw curve:
   * - curve line may be result of one or more destructive modifiers or just the raw data,
   *   so we need to check which method should be used
   * - controls from active modifier take precedence over keyframes
   *   (XXX! editing tools need to take this into account!)
   */

  /* 1) draw curve line */
  if (((fcu->modifiers.first) || (fcu->flag & FCURVE_INT_VALUES)) ||
      (((fcu->bezt) || (fcu->fpt)) && (fcu->totvert)))
  {
    /* set color/drawing style for curve itself */
    /* draw active F-Curve thicker than the rest to make it stand out */
    if (fcu->flag & FCURVE_ACTIVE && !BKE_fcurve_is_protected(fcu)) {
      GPU_line_width(2.5);
    }
    else {
      GPU_line_width(1.0);
    }

    /* anti-aliased lines for less jagged appearance */
    if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
      GPU_line_smooth(true);
    }
    GPU_blend(GPU_BLEND_ALPHA);

    const uint shdr_pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);

    if (BKE_fcurve_is_protected(fcu)) {
      /* Protected curves (non editable) are drawn with dotted lines. */
      immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
      immUniform2f(
          "viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);
      immUniform1i("colors_len", 0); /* Simple dashes. */
      immUniform1f("dash_width", 16.0f * U.scale_factor);
      immUniform1f("udash_factor", 0.35f * U.scale_factor);
    }
    else {
      immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
      immUniform2fv("viewportSize", &viewport_size[2]);
      immUniform1f("lineWidth", GPU_line_width_get());
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

    const bool draw_extrapolation = (sipo->flag & SIPO_NO_DRAW_EXTRAPOLATION) == 0;
    /* draw F-Curve */
    if ((fcu->modifiers.first) || (fcu->flag & FCURVE_INT_VALUES)) {
      /* draw a curve affected by modifiers or only allowed to have integer values
       * by sampling it at various small-intervals over the visible region
       */
      if (adt) {
        /* We have to do this mapping dance since the keyframes were remapped but the F-modifier
         * evaluations are not.
         *
         * So we undo the keyframe remapping and instead remap the evaluation time when drawing the
         * curve itself. Afterward, we go back and redo the keyframe remapping so the controls are
         * drawn properly. */
        ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, false);
        draw_fcurve_curve(ac, ale->id, fcu, &region->v2d, shdr_pos, true, draw_extrapolation);
        ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, false);
      }
      else {
        draw_fcurve_curve(ac, ale->id, fcu, &region->v2d, shdr_pos, false, draw_extrapolation);
      }
    }
    else if (((fcu->bezt) || (fcu->fpt)) && (fcu->totvert)) {
      /* just draw curve based on defined data (i.e. no modifiers) */
      if (fcu->bezt) {
        if (fcurve_can_use_simple_bezt_drawing(fcu)) {
          draw_fcurve_curve_bezts(ac, ale->id, fcu, &region->v2d, shdr_pos, draw_extrapolation);
        }
        else {
          draw_fcurve_curve(ac, ale->id, fcu, &region->v2d, shdr_pos, false, draw_extrapolation);
        }
      }
      else if (fcu->fpt) {
        draw_fcurve_curve_samples(ac, ale->id, fcu, &region->v2d, shdr_pos, draw_extrapolation);
      }
    }

    immUnbindProgram();

    if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
      GPU_line_smooth(false);
    }
    GPU_blend(GPU_BLEND_NONE);
  }

  /* 2) draw handles and vertices as appropriate based on active
   * - If the option to only show controls if the F-Curve is selected is enabled,
   *   we must obey this.
   */
  if (!(U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) ||
      (fcu->flag & FCURVE_SELECTED)) {
    if (!BKE_fcurve_are_keyframes_usable(fcu) && !(fcu->fpt && fcu->totvert)) {
      /* only draw controls if this is the active modifier */
      if ((fcu->flag & FCURVE_ACTIVE) && (fcm)) {
        switch (fcm->type) {
          case FMODIFIER_TYPE_ENVELOPE: /* envelope */
            draw_fcurve_modifier_controls_envelope(fcm, &region->v2d, adt);
            break;
        }
      }
    }
    else if (((fcu->bezt) || (fcu->fpt)) && (fcu->totvert)) {
      short mapping_flag = ANIM_get_normalization_flags(ac);
      float offset;
      const float unit_scale = ANIM_unit_mapping_get_factor(
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
          draw_fcurve_handles(sipo, fcu);
        }

        draw_fcurve_vertices(
            region, fcu, do_handles, (sipo->flag & SIPO_SELVHANDLESONLY), unit_scale);
      }
      else {
        /* samples: only draw two indicators at either end as indicators */
        draw_fcurve_samples(region, fcu, unit_scale);
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
    ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, false);
  }
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
  View2D *v2d = &ac->region->v2d;
  short mapping_flag = ANIM_get_normalization_flags(ac);
  float offset;
  float unitfac = ANIM_unit_mapping_get_factor(ac->scene, id, fcu, mapping_flag, &offset);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */

  /* No curve to modify/visualize the result?
   * => We still want to show the 1-1 default...
   */
  if ((fcu->totvert == 0) && BLI_listbase_is_empty(&fcu->modifiers)) {
    float t;

    /* draw with thin dotted lines in style of what curve would have been */
    immUniformColor3fv(fcu->color);

    immUniform1f("dash_width", 40.0f);
    immUniform1f("udash_factor", 0.5f);
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

    /* Only draw indicators if the point is in range. */
    if (x >= v2d->cur.xmin) {
      float co[2];

      /* draw dotted lines leading towards this point from both axes ....... */
      immUniformColor3f(0.9f, 0.9f, 0.9f);
      immUniform1f("dash_width", 10.0f);
      immUniform1f("udash_factor", 0.5f);
      GPU_line_width(1.0f);

      immBegin(GPU_PRIM_LINES, (y <= v2d->cur.ymax) ? 4 : 2);

      /* x-axis lookup */
      co[0] = x;

      if (y <= v2d->cur.ymax) {
        co[1] = v2d->cur.ymax + 1.0f;
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
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

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

void graph_draw_ghost_curves(bAnimContext *ac, SpaceGraph *sipo, ARegion *region)
{
  FCurve *fcu;

  /* draw with thick dotted lines */
  GPU_line_width(3.0f);

  /* anti-aliased lines for less jagged appearance */
  if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
    GPU_line_smooth(true);
  }
  GPU_blend(GPU_BLEND_ALPHA);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniform1f("dash_width", 20.0f);
  immUniform1f("udash_factor", 0.5f);

  /* Don't draw extrapolation on sampled ghost curves because it doesn't
   * match the curves they're ghosting anyway.
   * See issue #109920 for details. */
  const bool draw_extrapolation = false;
  /* the ghost curves are simply sampled F-Curves stored in sipo->runtime.ghost_curves */
  for (fcu = static_cast<FCurve *>(sipo->runtime.ghost_curves.first); fcu; fcu = fcu->next) {
    /* set whatever color the curve has set
     * - this is set by the function which creates these
     * - draw with a fixed opacity of 2
     */
    immUniformColor3fvAlpha(fcu->color, 0.5f);

    /* simply draw the stored samples */
    draw_fcurve_curve_samples(ac, nullptr, fcu, &region->v2d, shdr_pos, draw_extrapolation);
  }

  immUnbindProgram();

  if (U.animation_flag & USER_ANIM_HIGH_QUALITY_DRAWING) {
    GPU_line_smooth(false);
  }
  GPU_blend(GPU_BLEND_NONE);
}

void graph_draw_curves(bAnimContext *ac, SpaceGraph *sipo, ARegion *region, short sel)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  int filter;

  /* build list of curves to draw */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY);
  filter |= ((sel) ? (ANIMFILTER_SEL) : (ANIMFILTER_UNSEL));
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* for each curve:
   * draw curve, then handle-lines, and finally vertices in this order so that
   * the data will be layered correctly
   */
  bAnimListElem *ale_active_fcurve = nullptr;
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale; ale = ale->next) {
    const FCurve *fcu = (FCurve *)ale->key_data;
    if (fcu->flag & FCURVE_ACTIVE) {
      ale_active_fcurve = ale;
      continue;
    }
    draw_fcurve(ac, sipo, region, ale);
  }

  /* Draw the active FCurve last so that it (especially the active keyframe)
   * shows on top of the other curves. */
  if (ale_active_fcurve != nullptr) {
    draw_fcurve(ac, sipo, region, ale_active_fcurve);
  }

  /* free list of curves */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Channel List
 * \{ */

void graph_draw_channel_names(bContext *C, bAnimContext *ac, ARegion *region)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  int filter;

  View2D *v2d = &region->v2d;
  float height;
  size_t items;

  /* build list of channels to draw */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS |
            ANIMFILTER_FCURVESONLY);
  items = ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Update max-extent of channels here (taking into account scrollers):
   * - this is done to allow the channel list to be scrollable, but must be done here
   *   to avoid regenerating the list again and/or also because channels list is drawn first */
  height = ANIM_UI_get_channels_total_height(v2d, items);
  v2d->tot.ymin = -height;
  const float channel_step = ANIM_UI_get_channel_step();

  /* Loop through channels, and set up drawing depending on their type. */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t channel_index = 0;
    float ymax = ANIM_UI_get_first_channel_top(v2d);

    for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
         ale = ale->next, ymax -= channel_step, channel_index++)
    {
      const float ymin = ymax - ANIM_UI_get_channel_height();

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        ANIM_channel_draw(ac, ale, ymin, ymax, channel_index);
      }
    }
  }
  { /* second pass: widgets */
    uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
    size_t channel_index = 0;
    float ymax = ANIM_UI_get_first_channel_top(v2d);

    /* set blending again, as may not be set in previous step */
    GPU_blend(GPU_BLEND_ALPHA);

    for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
         ale = ale->next, ymax -= channel_step, channel_index++)
    {
      const float ymin = ymax - ANIM_UI_get_channel_height();

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        rctf channel_rect;
        BLI_rctf_init(&channel_rect, 0, v2d->cur.xmax - V2D_SCROLL_WIDTH, ymin, ymax);
        ANIM_channel_draw_widgets(C, ac, ale, block, &channel_rect, channel_index);
      }
    }

    UI_block_end(C, block);
    UI_block_draw(C, block);

    GPU_blend(GPU_BLEND_NONE);
  }

  /* Free temporary channels. */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

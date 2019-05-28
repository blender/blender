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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h" /* SELECT */
#include "DNA_space_types.h"

#include "ED_clip.h"
#include "ED_mask.h" /* own include */
#include "ED_space_api.h"

#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_draw.h"
#include "GPU_shader.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "DEG_depsgraph_query.h"

#include "mask_intern.h" /* own include */

static void mask_spline_color_get(MaskLayer *masklay,
                                  MaskSpline *spline,
                                  const bool is_sel,
                                  unsigned char r_rgb[4])
{
  if (is_sel) {
    if (masklay->act_spline == spline) {
      r_rgb[0] = r_rgb[1] = r_rgb[2] = 255;
    }
    else {
      r_rgb[0] = 255;
      r_rgb[1] = r_rgb[2] = 0;
    }
  }
  else {
    r_rgb[0] = 128;
    r_rgb[1] = r_rgb[2] = 0;
  }

  r_rgb[3] = 255;
}

static void mask_spline_feather_color_get(MaskLayer *UNUSED(masklay),
                                          MaskSpline *UNUSED(spline),
                                          const bool is_sel,
                                          unsigned char r_rgb[4])
{
  if (is_sel) {
    r_rgb[1] = 255;
    r_rgb[0] = r_rgb[2] = 0;
  }
  else {
    r_rgb[1] = 128;
    r_rgb[0] = r_rgb[2] = 0;
  }

  r_rgb[3] = 255;
}

static void mask_point_undistort_pos(SpaceClip *sc, float r_co[2], const float co[2])
{
  BKE_mask_coord_to_movieclip(sc->clip, &sc->user, r_co, co);
  ED_clip_point_undistorted_pos(sc, r_co, r_co);
  BKE_mask_coord_from_movieclip(sc->clip, &sc->user, r_co, r_co);
}

static void draw_single_handle(const MaskLayer *mask_layer,
                               const MaskSplinePoint *point,
                               const eMaskWhichHandle which_handle,
                               const int draw_type,
                               const float handle_size,
                               const float point_pos[2],
                               const float handle_pos[2])
{
  const BezTriple *bezt = &point->bezt;
  char handle_type;

  if (which_handle == MASK_WHICH_HANDLE_STICK || which_handle == MASK_WHICH_HANDLE_LEFT) {
    handle_type = bezt->h1;
  }
  else {
    handle_type = bezt->h2;
  }

  if (handle_type == HD_VECT) {
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  const unsigned char rgb_gray[4] = {0x60, 0x60, 0x60, 0xff};

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3ubv(rgb_gray);

  /* this could be split into its own loop */
  if (draw_type == MASK_DT_OUTLINE) {
    GPU_line_width(3.0f);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos, point_pos);
    immVertex2fv(pos, handle_pos);
    immEnd();
  }

  switch (handle_type) {
    case HD_FREE:
      immUniformThemeColor(TH_HANDLE_FREE);
      break;
    case HD_AUTO:
      immUniformThemeColor(TH_HANDLE_AUTO);
      break;
    case HD_ALIGN:
    case HD_ALIGN_DOUBLESIDE:
      immUniformThemeColor(TH_HANDLE_ALIGN);
      break;
  }

  GPU_line_width(1.0f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2fv(pos, point_pos);
  immVertex2fv(pos, handle_pos);
  immEnd();
  immUnbindProgram();

  /* draw handle points */
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);
  immUniform1f("size", handle_size);
  immUniform1f("outlineWidth", 1.5f);

  float point_color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; /* active color by default */
  if (MASKPOINT_ISSEL_HANDLE(point, which_handle)) {
    if (point != mask_layer->act_point) {
      UI_GetThemeColor3fv(TH_HANDLE_VERTEX_SELECT, point_color);
    }
  }
  else {
    UI_GetThemeColor3fv(TH_HANDLE_VERTEX, point_color);
  }

  immUniform4fv("outlineColor", point_color);
  immUniformColor3fvAlpha(point_color, 0.25f);

  immBegin(GPU_PRIM_POINTS, 1);
  immVertex2fv(pos, handle_pos);
  immEnd();

  immUnbindProgram();
}

/* return non-zero if spline is selected */
static void draw_spline_points(const bContext *C,
                               MaskLayer *masklay,
                               MaskSpline *spline,
                               const char draw_flag,
                               const char draw_type)
{
  const bool is_spline_sel = (spline->flag & SELECT) &&
                             (masklay->restrictflag & MASK_RESTRICT_SELECT) == 0;
  const bool is_smooth = (draw_flag & MASK_DRAWFLAG_SMOOTH) != 0;

  unsigned char rgb_spline[4];
  MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);
  SpaceClip *sc = CTX_wm_space_clip(C);
  bool undistort = false;

  int tot_feather_point;
  float(*feather_points)[2], (*fp)[2];
  float min[2], max[2];

  if (!spline->tot_point) {
    return;
  }

  if (sc) {
    undistort = sc->clip && (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT);
  }

  /* TODO, add this to sequence editor */
  float handle_size = 2.0f * UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE) * U.pixelsize;

  mask_spline_color_get(masklay, spline, is_spline_sel, rgb_spline);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
  immUniform1f("size", 0.7f * handle_size);

  /* feather points */
  feather_points = fp = BKE_mask_spline_feather_points(spline, &tot_feather_point);
  for (int i = 0; i < spline->tot_point; i++) {

    /* watch it! this is intentionally not the deform array, only check for sel */
    MaskSplinePoint *point = &spline->points[i];

    for (int j = 0; j <= point->tot_uw; j++) {
      float feather_point[2];
      bool sel = false;

      copy_v2_v2(feather_point, *fp);

      if (undistort) {
        mask_point_undistort_pos(sc, feather_point, feather_point);
      }

      if (j == 0) {
        sel = MASKPOINT_ISSEL_ANY(point);
      }
      else {
        sel = (point->uw[j - 1].flag & SELECT) != 0;
      }

      if (sel) {
        if (point == masklay->act_point) {
          immUniformColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
          immUniformThemeColorShadeAlpha(TH_HANDLE_VERTEX_SELECT, 0, 255);
        }
      }
      else {
        immUniformThemeColorShadeAlpha(TH_HANDLE_VERTEX, 0, 255);
      }

      immBegin(GPU_PRIM_POINTS, 1);
      immVertex2fv(pos, feather_point);
      immEnd();

      fp++;
    }
  }
  MEM_freeN(feather_points);

  immUnbindProgram();

  if (is_smooth) {
    GPU_line_smooth(true);
  }

  /* control points */
  INIT_MINMAX2(min, max);
  for (int i = 0; i < spline->tot_point; i++) {

    /* watch it! this is intentionally not the deform array, only check for sel */
    MaskSplinePoint *point = &spline->points[i];
    MaskSplinePoint *point_deform = &points_array[i];
    BezTriple *bezt = &point_deform->bezt;

    float vert[2];

    copy_v2_v2(vert, bezt->vec[1]);

    if (undistort) {
      mask_point_undistort_pos(sc, vert, vert);
    }

    /* draw handle segment */
    if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
      float handle[2];
      BKE_mask_point_handle(point_deform, MASK_WHICH_HANDLE_STICK, handle);
      if (undistort) {
        mask_point_undistort_pos(sc, handle, handle);
      }
      draw_single_handle(
          masklay, point, MASK_WHICH_HANDLE_STICK, draw_type, handle_size, vert, handle);
    }
    else {
      float handle_left[2], handle_right[2];
      BKE_mask_point_handle(point_deform, MASK_WHICH_HANDLE_LEFT, handle_left);
      BKE_mask_point_handle(point_deform, MASK_WHICH_HANDLE_RIGHT, handle_right);
      if (undistort) {
        mask_point_undistort_pos(sc, handle_left, handle_left);
        mask_point_undistort_pos(sc, handle_left, handle_left);
      }
      draw_single_handle(
          masklay, point, MASK_WHICH_HANDLE_LEFT, draw_type, handle_size, vert, handle_left);
      draw_single_handle(
          masklay, point, MASK_WHICH_HANDLE_RIGHT, draw_type, handle_size, vert, handle_right);
    }

    /* bind program in loop so it does not interfere with draw_single_handle */
    immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

    /* draw CV point */
    if (MASKPOINT_ISSEL_KNOT(point)) {
      if (point == masklay->act_point) {
        immUniformColor3f(1.0f, 1.0f, 1.0f);
      }
      else {
        immUniformThemeColorShadeAlpha(TH_HANDLE_VERTEX_SELECT, 0, 255);
      }
    }
    else {
      immUniformThemeColorShadeAlpha(TH_HANDLE_VERTEX, 0, 255);
    }

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex2fv(pos, vert);
    immEnd();

    immUnbindProgram();

    minmax_v2v2_v2(min, max, vert);
  }

  if (is_smooth) {
    GPU_line_smooth(false);
  }

  if (is_spline_sel) {
    float x = (min[0] + max[0]) * 0.5f;
    float y = (min[1] + max[1]) * 0.5f;

    immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);
    immUniform1f("outlineWidth", 1.5f);

    if (masklay->act_spline == spline) {
      immUniformColor3f(1.0f, 1.0f, 1.0f);
    }
    else {
      immUniformColor3f(1.0f, 1.0f, 0.0f);
    }

    immUniform4f("outlineColor", 0.0f, 0.0f, 0.0f, 1.0f);
    immUniform1f("size", 12.0f);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex2f(pos, x, y);
    immEnd();

    immUnbindProgram();
  }
}

static void mask_color_active_tint(unsigned char r_rgb[4],
                                   const unsigned char rgb[4],
                                   const bool is_active)
{
  if (!is_active) {
    r_rgb[0] = (unsigned char)((((int)(rgb[0])) + 128) / 2);
    r_rgb[1] = (unsigned char)((((int)(rgb[1])) + 128) / 2);
    r_rgb[2] = (unsigned char)((((int)(rgb[2])) + 128) / 2);
    r_rgb[3] = rgb[3];
  }
  else {
    *(unsigned int *)r_rgb = *(const unsigned int *)rgb;
  }
}

static void mask_draw_array(unsigned int pos,
                            GPUPrimType prim_type,
                            const float (*points)[2],
                            unsigned int vertex_len)
{
  immBegin(prim_type, vertex_len);
  for (unsigned int i = 0; i < vertex_len; ++i) {
    immVertex2fv(pos, points[i]);
  }
  immEnd();
}

static void mask_draw_curve_type(const bContext *C,
                                 MaskSpline *spline,
                                 float (*orig_points)[2],
                                 int tot_point,
                                 const bool is_feather,
                                 const bool is_active,
                                 const unsigned char rgb_spline[4],
                                 const char draw_type)
{
  const GPUPrimType draw_method = (spline->flag & MASK_SPLINE_CYCLIC) ? GPU_PRIM_LINE_LOOP :
                                                                        GPU_PRIM_LINE_STRIP;
  const unsigned char rgb_black[4] = {0x00, 0x00, 0x00, 0xff};
  unsigned char rgb_tmp[4];
  SpaceClip *sc = CTX_wm_space_clip(C);
  float(*points)[2] = orig_points;

  if (sc) {
    const bool undistort = sc->clip && (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT);

    if (undistort) {
      points = MEM_callocN(2 * tot_point * sizeof(float), "undistorthed mask curve");

      for (int i = 0; i < tot_point; i++) {
        mask_point_undistort_pos(sc, points[i], orig_points[i]);
      }
    }
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  switch (draw_type) {

    case MASK_DT_OUTLINE:
      /* TODO(merwin): use fancy line shader here
       * probably better with geometry shader (after core profile switch)
       */
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      GPU_line_width(3.0f);

      mask_color_active_tint(rgb_tmp, rgb_black, is_active);
      immUniformColor4ubv(rgb_tmp);
      mask_draw_array(pos, draw_method, points, tot_point);

      GPU_line_width(1.0f);

      mask_color_active_tint(rgb_tmp, rgb_spline, is_active);
      immUniformColor4ubv(rgb_tmp);
      mask_draw_array(pos, draw_method, points, tot_point);

      immUnbindProgram();
      break;

    case MASK_DT_BLACK:
    case MASK_DT_WHITE:
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      GPU_line_width(1.0f);

      if (draw_type == MASK_DT_BLACK) {
        rgb_tmp[0] = rgb_tmp[1] = rgb_tmp[2] = 0;
      }
      else {
        rgb_tmp[0] = rgb_tmp[1] = rgb_tmp[2] = 255;
      }
      /* alpha values seem too low but gl draws many points that compensate for it */
      if (is_feather) {
        rgb_tmp[3] = 64;
      }
      else {
        rgb_tmp[3] = 128;
      }

      if (is_feather) {
        rgb_tmp[0] = (unsigned char)(((short)rgb_tmp[0] + (short)rgb_spline[0]) / 2);
        rgb_tmp[1] = (unsigned char)(((short)rgb_tmp[1] + (short)rgb_spline[1]) / 2);
        rgb_tmp[2] = (unsigned char)(((short)rgb_tmp[2] + (short)rgb_spline[2]) / 2);
      }

      mask_color_active_tint(rgb_tmp, rgb_tmp, is_active);
      immUniformColor4ubv(rgb_tmp);
      mask_draw_array(pos, draw_method, points, tot_point);

      immUnbindProgram();
      break;

    case MASK_DT_DASH: {
      float colors[8];

      mask_color_active_tint(rgb_tmp, rgb_spline, is_active);
      rgba_uchar_to_float(colors, rgb_tmp);
      mask_color_active_tint(rgb_tmp, rgb_black, is_active);
      rgba_uchar_to_float(colors + 4, rgb_tmp);

      immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

      float viewport_size[4];
      GPU_viewport_size_get_f(viewport_size);
      immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

      immUniform1i("colors_len", 2); /* "advanced" mode */
      immUniformArray4fv("colors", colors, 2);
      immUniform1f("dash_width", 4.0f);
      GPU_line_width(1.0f);

      mask_draw_array(pos, draw_method, points, tot_point);

      immUnbindProgram();
      break;
    }

    default:
      BLI_assert(false);
  }

  if (points != orig_points) {
    MEM_freeN(points);
  }
}

static void draw_spline_curve(const bContext *C,
                              MaskLayer *masklay,
                              MaskSpline *spline,
                              const char draw_flag,
                              const char draw_type,
                              const bool is_active,
                              const int width,
                              const int height)
{
  const unsigned int resol = max_ii(BKE_mask_spline_feather_resolution(spline, width, height),
                                    BKE_mask_spline_resolution(spline, width, height));

  unsigned char rgb_tmp[4];

  const bool is_spline_sel = (spline->flag & SELECT) &&
                             (masklay->restrictflag & MASK_RESTRICT_SELECT) == 0;
  const bool is_smooth = (draw_flag & MASK_DRAWFLAG_SMOOTH) != 0;
  const bool is_fill = (spline->flag & MASK_SPLINE_NOFILL) == 0;

  unsigned int tot_diff_point;
  float(*diff_points)[2];

  unsigned int tot_feather_point;
  float(*feather_points)[2];

  diff_points = BKE_mask_spline_differentiate_with_resolution(spline, &tot_diff_point, resol);

  if (!diff_points) {
    return;
  }

  if (is_smooth) {
    GPU_line_smooth(true);
  }

  feather_points = BKE_mask_spline_feather_differentiated_points_with_resolution(
      spline, &tot_feather_point, resol, (is_fill != false));

  /* draw feather */
  mask_spline_feather_color_get(masklay, spline, is_spline_sel, rgb_tmp);
  mask_draw_curve_type(
      C, spline, feather_points, tot_feather_point, true, is_active, rgb_tmp, draw_type);

  if (!is_fill) {
    const float *fp = &diff_points[0][0];
    float *fp_feather = &feather_points[0][0];

    BLI_assert(tot_diff_point == tot_feather_point);

    for (int i = 0; i < tot_diff_point; i++, fp += 2, fp_feather += 2) {
      float tvec[2];
      sub_v2_v2v2(tvec, fp, fp_feather);
      add_v2_v2v2(fp_feather, fp, tvec);
    }

    /* same as above */
    mask_draw_curve_type(
        C, spline, feather_points, tot_feather_point, true, is_active, rgb_tmp, draw_type);
  }

  MEM_freeN(feather_points);

  /* draw main curve */
  mask_spline_color_get(masklay, spline, is_spline_sel, rgb_tmp);
  mask_draw_curve_type(
      C, spline, diff_points, tot_diff_point, false, is_active, rgb_tmp, draw_type);
  MEM_freeN(diff_points);

  if (is_smooth) {
    GPU_line_smooth(false);
  }
}

static void draw_masklays(const bContext *C,
                          Mask *mask,
                          const char draw_flag,
                          const char draw_type,
                          const int width,
                          const int height)
{
  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_program_point_size(true);

  MaskLayer *masklay;
  int i;

  for (masklay = mask->masklayers.first, i = 0; masklay; masklay = masklay->next, i++) {
    MaskSpline *spline;
    const bool is_active = (i == mask->masklay_act);

    if (masklay->restrictflag & MASK_RESTRICT_VIEW) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {

      /* draw curve itself first... */
      draw_spline_curve(C, masklay, spline, draw_flag, draw_type, is_active, width, height);

      if (!(masklay->restrictflag & MASK_RESTRICT_SELECT)) {
        /* ...and then handles over the curve so they're nicely visible */
        draw_spline_points(C, masklay, spline, draw_flag, draw_type);
      }

      /* show undeform for testing */
      if (0) {
        void *back = spline->points_deform;

        spline->points_deform = NULL;
        draw_spline_curve(C, masklay, spline, draw_flag, draw_type, is_active, width, height);
        draw_spline_points(C, masklay, spline, draw_flag, draw_type);
        spline->points_deform = back;
      }
    }
  }

  GPU_program_point_size(false);
  GPU_blend(false);
}

void ED_mask_draw(const bContext *C, const char draw_flag, const char draw_type)
{
  ScrArea *sa = CTX_wm_area(C);
  Mask *mask = CTX_data_edit_mask(C);
  int width, height;

  if (!mask) {
    return;
  }

  ED_mask_get_size(sa, &width, &height);

  draw_masklays(C, mask, draw_flag, draw_type, width, height);
}

static float *mask_rasterize(Mask *mask, const int width, const int height)
{
  MaskRasterHandle *handle;
  float *buffer = MEM_mallocN(sizeof(float) * height * width, "rasterized mask buffer");

  /* Initialize rasterization handle. */
  handle = BKE_maskrasterize_handle_new();
  BKE_maskrasterize_handle_init(handle, mask, width, height, true, true, true);

  BKE_maskrasterize_buffer(handle, width, height, buffer);

  /* Free memory. */
  BKE_maskrasterize_handle_free(handle);

  return buffer;
}

/* sets up the opengl context.
 * width, height are to match the values from ED_mask_get_size() */
void ED_mask_draw_region(
    Depsgraph *depsgraph,
    Mask *mask_,
    ARegion *ar,
    const char draw_flag,
    const char draw_type,
    const char overlay_mode,
    /* convert directly into aspect corrected vars */
    const int width_i,
    const int height_i,
    const float aspx,
    const float aspy,
    const bool do_scale_applied,
    const bool do_draw_cb,
    /* optional - only used by clip */
    float stabmat[4][4],
    /* optional - only used when do_post_draw is set or called from clip editor */
    const bContext *C)
{
  struct View2D *v2d = &ar->v2d;
  Mask *mask_eval = (Mask *)DEG_get_evaluated_id(depsgraph, &mask_->id);

  /* aspect always scales vertically in movie and image spaces */
  const float width = width_i, height = (float)height_i * (aspy / aspx);

  int x, y;
  /* int w, h; */
  float zoomx, zoomy;

  /* frame image */
  float maxdim;
  float xofs, yofs;

  /* find window pixel coordinates of origin */
  UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x, &y);

  /* w = BLI_rctf_size_x(&v2d->tot); */
  /* h = BLI_rctf_size_y(&v2d->tot); */

  zoomx = (float)(BLI_rcti_size_x(&ar->winrct) + 1) / BLI_rctf_size_x(&ar->v2d.cur);
  zoomy = (float)(BLI_rcti_size_y(&ar->winrct) + 1) / BLI_rctf_size_y(&ar->v2d.cur);

  if (do_scale_applied) {
    zoomx /= width;
    zoomy /= height;
  }

  x += v2d->tot.xmin * zoomx;
  y += v2d->tot.ymin * zoomy;

  /* frame the image */
  maxdim = max_ff(width, height);
  if (width == height) {
    xofs = yofs = 0;
  }
  else if (width < height) {
    xofs = ((height - width) / -2.0f) * zoomx;
    yofs = 0.0f;
  }
  else { /* (width > height) */
    xofs = 0.0f;
    yofs = ((width - height) / -2.0f) * zoomy;
  }

  if (draw_flag & MASK_DRAWFLAG_OVERLAY) {
    float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float *buffer = mask_rasterize(mask_eval, width, height);

    if (overlay_mode != MASK_OVERLAY_ALPHACHANNEL) {
      /* More blending types could be supported in the future. */
      GPU_blend(true);
      GPU_blend_set_func(GPU_DST_COLOR, GPU_ZERO);
    }

    GPU_matrix_push();
    GPU_matrix_translate_2f(x, y);
    GPU_matrix_scale_2f(zoomx, zoomy);
    if (stabmat) {
      GPU_matrix_mul(stabmat);
    }
    IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
    GPU_shader_uniform_vector(
        state.shader, GPU_shader_get_uniform_ensure(state.shader, "shuffle"), 4, 1, red);
    immDrawPixelsTex(
        &state, 0.0f, 0.0f, width, height, GL_RED, GL_FLOAT, GL_NEAREST, buffer, 1.0f, 1.0f, NULL);

    GPU_matrix_pop();

    if (overlay_mode != MASK_OVERLAY_ALPHACHANNEL) {
      GPU_blend(false);
    }

    MEM_freeN(buffer);
  }

  /* apply transformation so mask editing tools will assume drawing from the
   * origin in normalized space */
  GPU_matrix_push();
  GPU_matrix_translate_2f(x + xofs, y + yofs);
  GPU_matrix_scale_2f(zoomx, zoomy);
  if (stabmat) {
    GPU_matrix_mul(stabmat);
  }
  GPU_matrix_scale_2f(maxdim, maxdim);

  if (do_draw_cb) {
    ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);
  }

  /* draw! */
  draw_masklays(C, mask_eval, draw_flag, draw_type, width, height);

  if (do_draw_cb) {
    ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
  }

  GPU_matrix_pop();
}

void ED_mask_draw_frames(Mask *mask, ARegion *ar, const int cfra, const int sfra, const int efra)
{
  const float framelen = ar->winx / (float)(efra - sfra + 1);

  MaskLayer *masklay = BKE_mask_layer_active(mask);

  if (masklay) {
    unsigned int num_lines = BLI_listbase_count(&masklay->splines_shapes);

    if (num_lines > 0) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      immUniformColor4ub(255, 175, 0, 255);

      immBegin(GPU_PRIM_LINES, 2 * num_lines);

      for (MaskLayerShape *masklay_shape = masklay->splines_shapes.first; masklay_shape;
           masklay_shape = masklay_shape->next) {
        int frame = masklay_shape->frame;

        /* draw_keyframe(i, CFRA, sfra, framelen, 1); */
        int height = (frame == cfra) ? 22 : 10;
        int x = (frame - sfra) * framelen;
        immVertex2i(pos, x, 0);
        immVertex2i(pos, x, height);
      }
      immEnd();
      immUnbindProgram();
    }
  }
}

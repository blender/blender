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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spimage
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "PIL_time.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_moviecache.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_paint.h"

#include "BIF_glutil.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLF_api.h"

#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_mask.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "image_intern.h"

static void draw_render_info(
    const bContext *C, Scene *scene, Image *ima, ARegion *region, float zoomx, float zoomy)
{
  Render *re = RE_GetSceneRender(scene);
  Scene *stats_scene = ED_render_job_get_scene(C);
  if (stats_scene == NULL) {
    stats_scene = CTX_data_scene(C);
  }

  RenderResult *rr = BKE_image_acquire_renderresult(stats_scene, ima);

  if (rr && rr->text) {
    float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};
    ED_region_info_draw(region, rr->text, fill_color, true);
  }

  BKE_image_release_renderresult(stats_scene, ima);

  if (re) {
    int total_tiles;
    bool need_free_tiles;
    rcti *tiles = RE_engine_get_current_tiles(re, &total_tiles, &need_free_tiles);

    if (total_tiles) {
      /* find window pixel coordinates of origin */
      int x, y;
      UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);

      GPU_matrix_push();
      GPU_matrix_translate_2f(x, y);
      GPU_matrix_scale_2f(zoomx, zoomy);

      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      immUniformThemeColor(TH_FACE_SELECT);

      GPU_line_width(1.0f);

      rcti *tile = tiles;
      for (int i = 0; i < total_tiles; i++, tile++) {
        immDrawBorderCorners(pos, tile, zoomx, zoomy);
      }

      immUnbindProgram();

      if (need_free_tiles) {
        MEM_freeN(tiles);
      }

      GPU_matrix_pop();
    }
  }
}

void ED_image_draw_info(Scene *scene,
                        ARegion *region,
                        bool color_manage,
                        bool use_default_view,
                        int channels,
                        int x,
                        int y,
                        const uchar cp[4],
                        const float fp[4],
                        const float linearcol[4],
                        const int *zp,
                        const float *zpf)
{
  rcti color_rect;
  char str[256];
  int dx = 6;
  /* local coordinate visible rect inside region, to accommodate overlapping ui */
  const rcti *rect = ED_region_visible_rect(region);
  const int ymin = rect->ymin;
  const int dy = ymin + 0.3f * UI_UNIT_Y;

  /* text colors */
  /* XXX colored text not allowed in Blender UI */
#if 0
  uchar red[3] = {255, 50, 50};
  uchar green[3] = {0, 255, 0};
  uchar blue[3] = {100, 100, 255};
#else
  const uchar red[3] = {255, 255, 255};
  const uchar green[3] = {255, 255, 255};
  const uchar blue[3] = {255, 255, 255};
#endif
  float hue = 0, sat = 0, val = 0, lum = 0, u = 0, v = 0;
  float col[4], finalcol[4];

  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* noisy, high contrast make impossible to read if lower alpha is used. */
  immUniformColor4ub(0, 0, 0, 190);
  immRecti(pos, 0, ymin, BLI_rcti_size_x(&region->winrct) + 1, ymin + UI_UNIT_Y);

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);

  BLF_size(blf_mono_font, 11.0f * U.pixelsize, U.dpi);

  BLF_color3ub(blf_mono_font, 255, 255, 255);
  SNPRINTF(str, "X:%-4d  Y:%-4d |", x, y);
  BLF_position(blf_mono_font, dx, dy, 0);
  BLF_draw(blf_mono_font, str, sizeof(str));
  dx += BLF_width(blf_mono_font, str, sizeof(str));

  if (zp) {
    BLF_color3ub(blf_mono_font, 255, 255, 255);
    SNPRINTF(str, " Z:%-.4f |", 0.5f + 0.5f * (((float)*zp) / (float)0x7fffffff));
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));
  }
  if (zpf) {
    BLF_color3ub(blf_mono_font, 255, 255, 255);
    SNPRINTF(str, " Z:%-.3f |", *zpf);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));
  }

  if (channels == 1 && (cp != NULL || fp != NULL)) {
    if (fp != NULL) {
      SNPRINTF(str, " Val:%-.3f |", fp[0]);
    }
    else if (cp != NULL) {
      SNPRINTF(str, " Val:%-.3f |", cp[0] / 255.0f);
    }
    BLF_color3ub(blf_mono_font, 255, 255, 255);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));
  }

  if (channels >= 3) {
    BLF_color3ubv(blf_mono_font, red);
    if (fp) {
      SNPRINTF(str, "  R:%-.5f", fp[0]);
    }
    else if (cp) {
      SNPRINTF(str, "  R:%-3d", cp[0]);
    }
    else {
      STRNCPY(str, "  R:-");
    }
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    BLF_color3ubv(blf_mono_font, green);
    if (fp) {
      SNPRINTF(str, "  G:%-.5f", fp[1]);
    }
    else if (cp) {
      SNPRINTF(str, "  G:%-3d", cp[1]);
    }
    else {
      STRNCPY(str, "  G:-");
    }
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    BLF_color3ubv(blf_mono_font, blue);
    if (fp) {
      SNPRINTF(str, "  B:%-.5f", fp[2]);
    }
    else if (cp) {
      SNPRINTF(str, "  B:%-3d", cp[2]);
    }
    else {
      STRNCPY(str, "  B:-");
    }
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    if (channels == 4) {
      BLF_color3ub(blf_mono_font, 255, 255, 255);
      if (fp) {
        SNPRINTF(str, "  A:%-.4f", fp[3]);
      }
      else if (cp) {
        SNPRINTF(str, "  A:%-3d", cp[3]);
      }
      else {
        STRNCPY(str, "- ");
      }
      BLF_position(blf_mono_font, dx, dy, 0);
      BLF_draw(blf_mono_font, str, sizeof(str));
      dx += BLF_width(blf_mono_font, str, sizeof(str));
    }

    if (color_manage) {
      float rgba[4];

      copy_v3_v3(rgba, linearcol);
      if (channels == 3) {
        rgba[3] = 1.0f;
      }
      else {
        rgba[3] = linearcol[3];
      }

      if (use_default_view) {
        IMB_colormanagement_pixel_to_display_space_v4(rgba, rgba, NULL, &scene->display_settings);
      }
      else {
        IMB_colormanagement_pixel_to_display_space_v4(
            rgba, rgba, &scene->view_settings, &scene->display_settings);
      }

      SNPRINTF(str, "  |  CM  R:%-.4f  G:%-.4f  B:%-.4f", rgba[0], rgba[1], rgba[2]);
      BLF_position(blf_mono_font, dx, dy, 0);
      BLF_draw(blf_mono_font, str, sizeof(str));
      dx += BLF_width(blf_mono_font, str, sizeof(str));
    }
  }

  /* color rectangle */
  if (channels == 1) {
    if (fp) {
      col[0] = col[1] = col[2] = fp[0];
    }
    else if (cp) {
      col[0] = col[1] = col[2] = (float)cp[0] / 255.0f;
    }
    else {
      col[0] = col[1] = col[2] = 0.0f;
    }
    col[3] = 1.0f;
  }
  else if (channels == 3) {
    copy_v3_v3(col, linearcol);
    col[3] = 1.0f;
  }
  else if (channels == 4) {
    copy_v4_v4(col, linearcol);
  }
  else {
    BLI_assert(0);
    zero_v4(col);
  }

  if (color_manage) {
    if (use_default_view) {
      IMB_colormanagement_pixel_to_display_space_v4(finalcol, col, NULL, &scene->display_settings);
    }
    else {
      IMB_colormanagement_pixel_to_display_space_v4(
          finalcol, col, &scene->view_settings, &scene->display_settings);
    }
  }
  else {
    copy_v4_v4(finalcol, col);
  }

  GPU_blend(GPU_BLEND_NONE);
  dx += 0.25f * UI_UNIT_X;

  BLI_rcti_init(&color_rect,
                dx,
                dx + (1.5f * UI_UNIT_X),
                ymin + 0.15f * UI_UNIT_Y,
                ymin + 0.85f * UI_UNIT_Y);

  /* BLF uses immediate mode too, so we must reset our vertex format */
  pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  if (channels == 4) {
    rcti color_rect_half;
    int color_quater_x, color_quater_y;

    color_rect_half = color_rect;
    color_rect_half.xmax = BLI_rcti_cent_x(&color_rect);
    /* what color ??? */
    immRecti(pos, color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);

    color_rect_half = color_rect;
    color_rect_half.xmin = BLI_rcti_cent_x(&color_rect);

    color_quater_x = BLI_rcti_cent_x(&color_rect_half);
    color_quater_y = BLI_rcti_cent_y(&color_rect_half);

    immUniformColor3ub(UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK);
    immRecti(pos,
             color_rect_half.xmin,
             color_rect_half.ymin,
             color_rect_half.xmax,
             color_rect_half.ymax);

    immUniformColor3ub(UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT);
    immRecti(pos, color_quater_x, color_quater_y, color_rect_half.xmax, color_rect_half.ymax);
    immRecti(pos, color_rect_half.xmin, color_rect_half.ymin, color_quater_x, color_quater_y);

    if (fp != NULL || cp != NULL) {
      GPU_blend(GPU_BLEND_ALPHA);
      immUniformColor3fvAlpha(finalcol, fp ? fp[3] : (cp[3] / 255.0f));
      immRecti(pos, color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
      GPU_blend(GPU_BLEND_NONE);
    }
  }
  else {
    immUniformColor3fv(finalcol);
    immRecti(pos, color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
  }
  immUnbindProgram();

  /* draw outline */
  pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3ub(128, 128, 128);
  imm_draw_box_wire_2d(pos, color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
  immUnbindProgram();

  dx += 1.75f * UI_UNIT_X;

  BLF_color3ub(blf_mono_font, 255, 255, 255);
  if (channels == 1) {
    if (fp) {
      rgb_to_hsv(fp[0], fp[0], fp[0], &hue, &sat, &val);
      rgb_to_yuv(fp[0], fp[0], fp[0], &lum, &u, &v, BLI_YUV_ITU_BT709);
    }
    else if (cp) {
      rgb_to_hsv(
          (float)cp[0] / 255.0f, (float)cp[0] / 255.0f, (float)cp[0] / 255.0f, &hue, &sat, &val);
      rgb_to_yuv((float)cp[0] / 255.0f,
                 (float)cp[0] / 255.0f,
                 (float)cp[0] / 255.0f,
                 &lum,
                 &u,
                 &v,
                 BLI_YUV_ITU_BT709);
    }

    SNPRINTF(str, "V:%-.4f", val);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "   L:%-.4f", lum);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
  }
  else if (channels >= 3) {
    rgb_to_hsv(finalcol[0], finalcol[1], finalcol[2], &hue, &sat, &val);
    rgb_to_yuv(finalcol[0], finalcol[1], finalcol[2], &lum, &u, &v, BLI_YUV_ITU_BT709);

    SNPRINTF(str, "H:%-.4f", hue);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "  S:%-.4f", sat);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "  V:%-.4f", val);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "   L:%-.4f", lum);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw(blf_mono_font, str, sizeof(str));
  }
}
void draw_image_sample_line(SpaceImage *sima)
{
  if (sima->sample_line_hist.flag & HISTO_FLAG_SAMPLELINE) {
    Histogram *hist = &sima->sample_line_hist;

    GPUVertFormat *format = immVertexFormat();
    uint shdr_dashed_pos = GPU_vertformat_attr_add(
        format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

    immUniform1i("colors_len", 2); /* Advanced dashes. */
    immUniformArray4fv(
        "colors", (float *)(float[][4]){{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, 2);
    immUniform1f("dash_width", 2.0f);
    immUniform1f("dash_factor", 0.5f);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(shdr_dashed_pos, hist->co[0]);
    immVertex2fv(shdr_dashed_pos, hist->co[1]);
    immEnd();

    immUnbindProgram();
  }
}

void draw_image_main_helpers(const bContext *C, ARegion *region)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = ED_space_image(sima);

  const bool show_viewer = (ima && ima->source == IMA_SRC_VIEWER) != 0;
  const bool show_render = (show_viewer && ima->type == IMA_TYPE_R_RESULT) != 0;
  if (ima && show_render) {
    float zoomx, zoomy;
    ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);
    draw_render_info(C, sima->iuser.scene, ima, region, zoomx, zoomy);
  }
}

bool ED_space_image_show_cache(const SpaceImage *sima)
{
  Image *image = ED_space_image(sima);
  Mask *mask = NULL;
  if (sima->mode == SI_MODE_MASK) {
    mask = ED_space_image_get_mask(sima);
  }
  if (image == NULL && mask == NULL) {
    return false;
  }
  if (mask == NULL) {
    return ELEM(image->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE);
  }
  return true;
}

bool ED_space_image_show_cache_and_mval_over(const SpaceImage *sima,
                                             ARegion *region,
                                             const int mval[2])
{
  const rcti *rect_visible = ED_region_visible_rect(region);
  if (mval[1] > rect_visible->ymin + (16 * UI_DPI_FAC)) {
    return false;
  }
  return ED_space_image_show_cache(sima);
}

void draw_image_cache(const bContext *C, ARegion *region)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  Image *image = ED_space_image(sima);
  float x, cfra = CFRA, sfra = SFRA, efra = EFRA, framelen = region->winx / (efra - sfra + 1);
  Mask *mask = NULL;

  if (!ED_space_image_show_cache(sima)) {
    return;
  }

  if (sima->mode == SI_MODE_MASK) {
    mask = ED_space_image_get_mask(sima);
  }

  /* Local coordinate visible rect inside region, to accommodate overlapping ui. */
  const rcti *rect_visible = ED_region_visible_rect(region);
  const int region_bottom = rect_visible->ymin;

  GPU_blend(GPU_BLEND_ALPHA);

  /* Draw cache background. */
  ED_region_cache_draw_background(region);

  /* Draw cached segments. */
  if (image != NULL && image->cache != NULL &&
      ELEM(image->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    int num_segments = 0;
    int *points = NULL;

    IMB_moviecache_get_cache_segments(image->cache, IMB_PROXY_NONE, 0, &num_segments, &points);
    ED_region_cache_draw_cached_segments(
        region, num_segments, points, sfra + sima->iuser.offset, efra + sima->iuser.offset);
  }

  GPU_blend(GPU_BLEND_NONE);

  /* Draw current frame. */
  x = (cfra - sfra) / (efra - sfra + 1) * region->winx;

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColor(TH_CFRAME);
  immRecti(pos, x, region_bottom, x + ceilf(framelen), region_bottom + 8 * UI_DPI_FAC);
  immUnbindProgram();

  ED_region_cache_draw_curfra_label(cfra, x, region_bottom + 8.0f * UI_DPI_FAC);

  if (mask != NULL) {
    ED_mask_draw_frames(mask, region, cfra, sfra, efra);
  }
}

float ED_space_image_zoom_level(const View2D *v2d, const int grid_dimension)
{
  /* UV-space length per pixel */
  float xzoom = (v2d->cur.xmax - v2d->cur.xmin) / ((float)(v2d->mask.xmax - v2d->mask.xmin));
  float yzoom = (v2d->cur.ymax - v2d->cur.ymin) / ((float)(v2d->mask.ymax - v2d->mask.ymin));

  /* Zoom_factor for UV/Image editor is calculated based on:
   * - Default grid size on startup, which is 256x256 pixels
   * - How blend factor for grid lines is set up in the fragment shader `grid_frag.glsl`. */
  float zoom_factor;
  zoom_factor = (xzoom + yzoom) / 2.0f; /* Average for accuracy. */
  zoom_factor *= 256.0f / (powf(grid_dimension, 2));
  return zoom_factor;
}

void ED_space_image_grid_steps(SpaceImage *sima,
                               float grid_steps[SI_GRID_STEPS_LEN],
                               const int grid_dimension)
{
  if (sima->flag & SI_CUSTOM_GRID) {
    for (int step = 0; step < SI_GRID_STEPS_LEN; step++) {
      grid_steps[step] = powf(1, step) * (1.0f / ((float)sima->custom_grid_subdiv));
    }
  }
  else {
    for (int step = 0; step < SI_GRID_STEPS_LEN; step++) {
      grid_steps[step] = powf(grid_dimension, step) *
                         (1.0f / (powf(grid_dimension, SI_GRID_STEPS_LEN)));
    }
  }
}

float ED_space_image_increment_snap_value(const int grid_dimesnions,
                                          const float grid_steps[SI_GRID_STEPS_LEN],
                                          const float zoom_factor)
{
  /* Small offset on each grid_steps[] so that snapping value doesn't change until grid lines are
   * significantly visible.
   * `Offset = 3/4 * (grid_steps[i] - (grid_steps[i] / grid_dimesnsions))`
   *
   * Refer `grid_frag.glsl` to find out when grid lines actually start appearing */

  for (int step = 0; step < SI_GRID_STEPS_LEN; step++) {
    float offset = (3.0f / 4.0f) * (grid_steps[step] - (grid_steps[step] / grid_dimesnions));

    if ((grid_steps[step] - offset) > zoom_factor) {
      return grid_steps[step];
    }
  }

  /* Fallback */
  return grid_steps[0];
}

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
  RenderData *rd = RE_engine_get_render_data(re);
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

      if (rd->mode & R_BORDER) {
        /* TODO: round or floor instead of casting to int */
        GPU_matrix_translate_2f((int)(-rd->border.xmin * rd->xsch * rd->size * 0.01f),
                                (int)(-rd->border.ymin * rd->ysch * rd->size * 0.01f));
      }

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

/* used by node view too */
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
                        int *zp,
                        float *zpf)
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
  uchar red[3] = {255, 255, 255};
  uchar green[3] = {255, 255, 255};
  uchar blue[3] = {255, 255, 255};
#endif
  float hue = 0, sat = 0, val = 0, lum = 0, u = 0, v = 0;
  float col[4], finalcol[4];

  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* noisy, high contrast make impossible to read if lower alpha is used. */
  immUniformColor4ub(0, 0, 0, 190);
  immRecti(pos, 0, ymin, BLI_rcti_size_x(&region->winrct) + 1, ymin + UI_UNIT_Y);

  immUnbindProgram();

  GPU_blend(false);

  BLF_size(blf_mono_font, 11 * U.pixelsize, U.dpi);

  BLF_color3ub(blf_mono_font, 255, 255, 255);
  SNPRINTF(str, "X:%-4d  Y:%-4d |", x, y);
  BLF_position(blf_mono_font, dx, dy, 0);
  BLF_draw_ascii(blf_mono_font, str, sizeof(str));
  dx += BLF_width(blf_mono_font, str, sizeof(str));

  if (zp) {
    BLF_color3ub(blf_mono_font, 255, 255, 255);
    SNPRINTF(str, " Z:%-.4f |", 0.5f + 0.5f * (((float)*zp) / (float)0x7fffffff));
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));
  }
  if (zpf) {
    BLF_color3ub(blf_mono_font, 255, 255, 255);
    SNPRINTF(str, " Z:%-.3f |", *zpf);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
      BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
      BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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

  GPU_blend(false);
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

    GPU_blend(true);
    immUniformColor3fvAlpha(finalcol, fp ? fp[3] : (cp[3] / 255.0f));
    immRecti(pos, color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
    GPU_blend(false);
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
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "   L:%-.4f", lum);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
  }
  else if (channels >= 3) {
    rgb_to_hsv(finalcol[0], finalcol[1], finalcol[2], &hue, &sat, &val);
    rgb_to_yuv(finalcol[0], finalcol[1], finalcol[2], &lum, &u, &v, BLI_YUV_ITU_BT709);

    SNPRINTF(str, "H:%-.4f", hue);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "  S:%-.4f", sat);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "  V:%-.4f", val);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
    dx += BLF_width(blf_mono_font, str, sizeof(str));

    SNPRINTF(str, "   L:%-.4f", lum);
    BLF_position(blf_mono_font, dx, dy, 0);
    BLF_draw_ascii(blf_mono_font, str, sizeof(str));
  }
}

/* image drawing */
static void sima_draw_zbuf_pixels(
    float x1, float y1, int rectx, int recty, int *rect, float zoomx, float zoomy)
{
  float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};

  /* Slowwww */
  int *recti = MEM_mallocN(rectx * recty * sizeof(int), "temp");
  for (int a = rectx * recty - 1; a >= 0; a--) {
    /* zbuffer values are signed, so we need to shift color range */
    recti[a] = rect[a] * 0.5f + 0.5f;
  }

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
  GPU_shader_uniform_vector(
      state.shader, GPU_shader_get_uniform(state.shader, "shuffle"), 4, 1, red);

  immDrawPixelsTex(
      &state, x1, y1, rectx, recty, GL_RED, GL_INT, GL_NEAREST, recti, zoomx, zoomy, NULL);

  MEM_freeN(recti);
}

static void sima_draw_zbuffloat_pixels(Scene *scene,
                                       float x1,
                                       float y1,
                                       int rectx,
                                       int recty,
                                       float *rect_float,
                                       float zoomx,
                                       float zoomy)
{
  float bias, scale, *rectf, clip_end;
  int a;
  float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};

  if (scene->camera && scene->camera->type == OB_CAMERA) {
    bias = ((Camera *)scene->camera->data)->clip_start;
    clip_end = ((Camera *)scene->camera->data)->clip_end;
    scale = 1.0f / (clip_end - bias);
  }
  else {
    bias = 0.1f;
    scale = 0.01f;
    clip_end = 100.0f;
  }

  rectf = MEM_mallocN(rectx * recty * sizeof(float), "temp");
  for (a = rectx * recty - 1; a >= 0; a--) {
    if (rect_float[a] > clip_end) {
      rectf[a] = 0.0f;
    }
    else if (rect_float[a] < bias) {
      rectf[a] = 1.0f;
    }
    else {
      rectf[a] = 1.0f - (rect_float[a] - bias) * scale;
      rectf[a] *= rectf[a];
    }
  }

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
  GPU_shader_uniform_vector(
      state.shader, GPU_shader_get_uniform(state.shader, "shuffle"), 4, 1, red);

  immDrawPixelsTex(
      &state, x1, y1, rectx, recty, GL_RED, GL_FLOAT, GL_NEAREST, rectf, zoomx, zoomy, NULL);

  MEM_freeN(rectf);
}

static void draw_udim_label(ARegion *region, float fx, float fy, const char *label)
{
  if (label == NULL || !label[0]) {
    return;
  }

  /* find window pixel coordinates of origin */
  int x, y;
  UI_view2d_view_to_region(&region->v2d, fx, fy, &x, &y);

  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  int textwidth = BLF_width(blf_mono_font, label, strlen(label)) + 10;
  float stepx = BLI_rcti_size_x(&region->v2d.mask) / BLI_rctf_size_x(&region->v2d.cur);
  float opacity;
  if (textwidth < 0.5f * (stepx - 10)) {
    opacity = 1.0f;
  }
  else if (textwidth < (stepx - 10)) {
    opacity = 2.0f - 2.0f * (textwidth / (stepx - 10));
  }
  else {
    opacity = 0.0f;
  }
  BLF_color4ub(blf_mono_font, 220, 220, 220, 150 * opacity);
  BLF_position(blf_mono_font, (int)(x + 10), (int)(y + 10), 0);
  BLF_draw_ascii(blf_mono_font, label, strlen(label));

  GPU_blend(false);
}

static void draw_image_buffer(const bContext *C,
                              SpaceImage *sima,
                              ARegion *region,
                              Scene *scene,
                              ImBuf *ibuf,
                              float fx,
                              float fy,
                              float zoomx,
                              float zoomy)
{
  /* Image are still drawn in display space. */
  glDisable(GL_FRAMEBUFFER_SRGB);

  int x, y;
  int sima_flag = sima->flag & ED_space_image_get_display_channel_mask(ibuf);

  /* find window pixel coordinates of origin */
  UI_view2d_view_to_region(&region->v2d, fx, fy, &x, &y);

  /* this part is generic image display */
  if (sima_flag & SI_SHOW_ZBUF && (ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels == 1))) {
    if (ibuf->zbuf) {
      sima_draw_zbuf_pixels(x, y, ibuf->x, ibuf->y, ibuf->zbuf, zoomx, zoomy);
    }
    else if (ibuf->zbuf_float) {
      sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->zbuf_float, zoomx, zoomy);
    }
    else if (ibuf->channels == 1) {
      sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->rect_float, zoomx, zoomy);
    }
  }
  else {
    int clip_max_x, clip_max_y;
    UI_view2d_view_to_region(
        &region->v2d, region->v2d.cur.xmax, region->v2d.cur.ymax, &clip_max_x, &clip_max_y);

    if (sima_flag & SI_USE_ALPHA) {
      imm_draw_box_checker_2d(x, y, x + ibuf->x * zoomx, y + ibuf->y * zoomy);

      GPU_blend(true);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    }

    /* If RGBA display with color management */
    if ((sima_flag & (SI_SHOW_R | SI_SHOW_G | SI_SHOW_B | SI_SHOW_ALPHA)) == 0) {

      ED_draw_imbuf_ctx_clipping(
          C, ibuf, x, y, GL_NEAREST, 0, 0, clip_max_x, clip_max_y, zoomx, zoomy);
    }
    else {
      float shuffle[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      uchar *display_buffer;
      void *cache_handle;
      ColorManagedViewSettings *view_settings;
      ColorManagedDisplaySettings *display_settings;

      if (sima_flag & SI_SHOW_R) {
        shuffle[0] = 1.0f;
      }
      else if (sima_flag & SI_SHOW_G) {
        shuffle[1] = 1.0f;
      }
      else if (sima_flag & SI_SHOW_B) {
        shuffle[2] = 1.0f;
      }
      else if (sima_flag & SI_SHOW_ALPHA) {
        shuffle[3] = 1.0f;
      }

      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
      GPU_shader_uniform_vector(
          state.shader, GPU_shader_get_uniform(state.shader, "shuffle"), 4, 1, shuffle);

      IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);
      display_buffer = IMB_display_buffer_acquire(
          ibuf, view_settings, display_settings, &cache_handle);

      if (display_buffer) {
        immDrawPixelsTex_clipping(&state,
                                  x,
                                  y,
                                  ibuf->x,
                                  ibuf->y,
                                  GL_RGBA,
                                  GL_UNSIGNED_BYTE,
                                  GL_NEAREST,
                                  display_buffer,
                                  0,
                                  0,
                                  clip_max_x,
                                  clip_max_y,
                                  zoomx,
                                  zoomy,
                                  NULL);
      }

      IMB_display_buffer_release(cache_handle);
    }

    if (sima_flag & SI_USE_ALPHA) {
      GPU_blend(false);
    }
  }

  glEnable(GL_FRAMEBUFFER_SRGB);
}

static void draw_image_buffer_repeated(const bContext *C,
                                       SpaceImage *sima,
                                       ARegion *region,
                                       Scene *scene,
                                       ImBuf *ibuf,
                                       float zoomx,
                                       float zoomy)
{
  const double time_current = PIL_check_seconds_timer();

  const int xmax = ceil(region->v2d.cur.xmax);
  const int ymax = ceil(region->v2d.cur.ymax);
  const int xmin = floor(region->v2d.cur.xmin);
  const int ymin = floor(region->v2d.cur.ymin);

  for (int x = xmin; x < xmax; x++) {
    for (int y = ymin; y < ymax; y++) {
      draw_image_buffer(C, sima, region, scene, ibuf, x, y, zoomx, zoomy);

      /* only draw until running out of time */
      if ((PIL_check_seconds_timer() - time_current) > 0.25) {
        return;
      }
    }
  }
}

/* draw uv edit */

/* draw grease pencil */
void draw_image_grease_pencil(bContext *C, bool onlyv2d)
{
  /* draw in View2D space? */
  if (onlyv2d) {
    /* draw grease-pencil ('image' strokes) */
    ED_annotation_draw_2dimage(C);
  }
  else {
    /* assume that UI_view2d_restore(C) has been called... */
    // SpaceImage *sima = (SpaceImage *)CTX_wm_space_data(C);

    /* draw grease-pencil ('screen' strokes) */
    ED_annotation_draw_view2d(C, 0);
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

static void draw_image_paint_helpers(
    const bContext *C, ARegion *region, Scene *scene, float zoomx, float zoomy)
{
  Brush *brush;
  int x, y;
  ImBuf *ibuf;

  brush = BKE_paint_brush(&scene->toolsettings->imapaint.paint);

  if (brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE) && brush->clone.image) {
    ibuf = BKE_image_acquire_ibuf(brush->clone.image, NULL, NULL);

    if (ibuf) {
      void *cache_handle = NULL;
      float col[4] = {1.0f, 1.0f, 1.0f, brush->clone.alpha};
      UI_view2d_view_to_region(
          &region->v2d, brush->clone.offset[0], brush->clone.offset[1], &x, &y);

      uchar *display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);

      if (!display_buffer) {
        BKE_image_release_ibuf(brush->clone.image, ibuf, NULL);
        IMB_display_buffer_release(cache_handle);
        return;
      }

      GPU_blend(true);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
      immDrawPixelsTex(&state,
                       x,
                       y,
                       ibuf->x,
                       ibuf->y,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       GL_NEAREST,
                       display_buffer,
                       zoomx,
                       zoomy,
                       col);

      GPU_blend(false);

      BKE_image_release_ibuf(brush->clone.image, ibuf, NULL);
      IMB_display_buffer_release(cache_handle);
    }
  }
}

static void draw_udim_tile_grid(uint pos_attr,
                                uint color_attr,
                                ARegion *region,
                                int x,
                                int y,
                                float stepx,
                                float stepy,
                                const float color[3])
{
  float x1, y1;
  UI_view2d_view_to_region_fl(&region->v2d, x, y, &x1, &y1);
  int gridpos[5][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}};
  for (int i = 0; i < 4; i++) {
    immAttr3fv(color_attr, color);
    immVertex2f(pos_attr, x1 + gridpos[i][0] * stepx, y1 + gridpos[i][1] * stepy);
    immAttr3fv(color_attr, color);
    immVertex2f(pos_attr, x1 + gridpos[i + 1][0] * stepx, y1 + gridpos[i + 1][1] * stepy);
  }
}

static void draw_udim_tile_grids(ARegion *region, SpaceImage *sima, Image *ima)
{
  int num_tiles;
  if (ima != NULL) {
    num_tiles = BLI_listbase_count(&ima->tiles);

    if (ima->source != IMA_SRC_TILED) {
      return;
    }
  }
  else {
    num_tiles = sima->tile_grid_shape[0] * sima->tile_grid_shape[1];
  }

  float stepx = BLI_rcti_size_x(&region->v2d.mask) / BLI_rctf_size_x(&region->v2d.cur);
  float stepy = BLI_rcti_size_y(&region->v2d.mask) / BLI_rctf_size_y(&region->v2d.cur);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
  immBegin(GPU_PRIM_LINES, 8 * num_tiles);

  float theme_color[3], selected_color[3];
  UI_GetThemeColorShade3fv(TH_BACK, 60.0f, theme_color);
  UI_GetThemeColor3fv(TH_FACE_SELECT, selected_color);

  if (ima != NULL) {
    ImageTile *cur_tile = BLI_findlink(&ima->tiles, ima->active_tile_index);

    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      if (tile != cur_tile) {
        int x = (tile->tile_number - 1001) % 10;
        int y = (tile->tile_number - 1001) / 10;
        draw_udim_tile_grid(pos, color, region, x, y, stepx, stepy, theme_color);
      }
    }

    if (cur_tile != NULL) {
      int cur_x = (cur_tile->tile_number - 1001) % 10;
      int cur_y = (cur_tile->tile_number - 1001) / 10;
      draw_udim_tile_grid(pos, color, region, cur_x, cur_y, stepx, stepy, selected_color);
    }
  }
  else {
    for (int y = 0; y < sima->tile_grid_shape[1]; y++) {
      for (int x = 0; x < sima->tile_grid_shape[0]; x++) {
        draw_udim_tile_grid(pos, color, region, x, y, stepx, stepy, theme_color);
      }
    }
  }

  immEnd();
  immUnbindProgram();
}

/* draw main image region */

void draw_image_main(const bContext *C, ARegion *region)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  Image *ima;
  ImBuf *ibuf;
  float zoomx, zoomy;
  bool show_viewer, show_render, show_paint, show_stereo3d, show_multilayer;
  void *lock;

  /* XXX can we do this in refresh? */
#if 0
  what_image(sima);

  if (sima->image) {
    ED_image_get_aspect(sima->image, &xuser_asp, &yuser_asp);

    /* UGLY hack? until now iusers worked fine... but for flipbook viewer we need this */
    if (sima->image->type == IMA_TYPE_COMPOSITE) {
      ImageUser *iuser = ntree_get_active_iuser(scene->nodetree);
      if (iuser) {
        BKE_image_user_calc_imanr(iuser, scene->r.cfra, 0);
        sima->iuser = *iuser;
      }
    }
    /* and we check for spare */
    ibuf = ED_space_image_buffer(sima);
  }
#endif

  /* retrieve the image and information about it */
  ima = ED_space_image(sima);
  ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);

  /* Tag image as in active use for garbage collector. */
  if (ima) {
    BKE_image_tag_time(ima);
  }

  show_viewer = (ima && ima->source == IMA_SRC_VIEWER) != 0;
  show_render = (show_viewer && ima->type == IMA_TYPE_R_RESULT) != 0;
  show_paint = (ima && (sima->mode == SI_MODE_PAINT) && (show_viewer == false) &&
                (show_render == false));
  show_stereo3d = (ima && BKE_image_is_stereo(ima) && (sima->iuser.flag & IMA_SHOW_STEREO));
  show_multilayer = ima && BKE_image_is_multilayer(ima);

  if (show_viewer) {
    /* use locked draw for drawing viewer image buffer since the compositor
     * is running in separated thread and compositor could free this buffers.
     * other images are not modifying in such a way so they does not require
     * lock (sergey)
     */
    BLI_thread_lock(LOCK_DRAW_IMAGE);
  }

  if (show_stereo3d) {
    if (show_multilayer) {
      /* update multiindex and pass for the current eye */
      BKE_image_multilayer_index(ima->rr, &sima->iuser);
    }
    else {
      BKE_image_multiview_index(ima, &sima->iuser);
    }
  }

  ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);

  int main_w = 0;
  int main_h = 0;

  /* draw the image or grid */
  if (ibuf == NULL) {
    if (ima != NULL) {
      LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
        int x = (tile->tile_number - 1001) % 10;
        int y = (tile->tile_number - 1001) / 10;
        ED_region_grid_draw(region, zoomx, zoomy, x, y);
      }
    }
    else {
      for (int y = 0; y < sima->tile_grid_shape[1]; y++) {
        for (int x = 0; x < sima->tile_grid_shape[0]; x++) {
          ED_region_grid_draw(region, zoomx, zoomy, x, y);
        }
      }
    }
  }
  else {
    if (sima->flag & SI_DRAW_TILE) {
      draw_image_buffer_repeated(C, sima, region, scene, ibuf, zoomx, zoomy);
    }
    else {
      main_w = ibuf->x;
      main_h = ibuf->y;

      draw_image_buffer(C, sima, region, scene, ibuf, 0.0f, 0.0f, zoomx, zoomy);
      if (ima->source == IMA_SRC_TILED) {
        ImageTile *tile = BKE_image_get_tile(ima, 0);
        char label[sizeof(tile->label)];
        BKE_image_get_tile_label(ima, tile, label, sizeof(label));
        draw_udim_label(region, 0.0f, 0.0f, label);
      }
    }

    if (sima->flag & SI_DRAW_METADATA) {
      int x, y;
      rctf frame;

      BLI_rctf_init(&frame, 0.0f, ibuf->x, 0.0f, ibuf->y);
      UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);

      ED_region_image_metadata_draw(x, y, ibuf, &frame, zoomx, zoomy);
    }
  }

  ED_space_image_release_buffer(sima, ibuf, lock);

  if (ima != NULL && ima->source == IMA_SRC_TILED) {
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      if (tile->tile_number == 1001) {
        continue;
      }

      ibuf = ED_space_image_acquire_buffer(sima, &lock, tile->tile_number);
      if (ibuf != NULL) {
        int x_pos = (tile->tile_number - 1001) % 10;
        int y_pos = (tile->tile_number - 1001) / 10;
        char label[sizeof(tile->label)];
        BKE_image_get_tile_label(ima, tile, label, sizeof(label));

        float tile_zoomx = (zoomx * main_w) / ibuf->x;
        float tile_zoomy = (zoomy * main_h) / ibuf->y;
        draw_image_buffer(C, sima, region, scene, ibuf, x_pos, y_pos, tile_zoomx, tile_zoomy);
        draw_udim_label(region, x_pos, y_pos, label);
      }
      ED_space_image_release_buffer(sima, ibuf, lock);
    }
  }

  draw_udim_tile_grids(region, sima, ima);

  /* paint helpers */
  if (show_paint) {
    draw_image_paint_helpers(C, region, scene, zoomx, zoomy);
  }

  if (show_viewer) {
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }

  /* render info */
  if (ima && show_render) {
    draw_render_info(C, sima->iuser.scene, ima, region, zoomx, zoomy);
  }
}

bool ED_space_image_show_cache(SpaceImage *sima)
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

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

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

  GPU_blend(false);

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

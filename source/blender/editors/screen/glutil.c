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
 * \ingroup edscr
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "BIF_glutil.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"

/* ******************************************** */

static int get_cached_work_texture(int *r_w, int *r_h)
{
  static GLint texid = -1;
  static int tex_w = 256;
  static int tex_h = 256;

  if (texid == -1) {
    glGenTextures(1, (GLuint *)&texid);

    glBindTexture(GL_TEXTURE_2D, texid);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
  }

  *r_w = tex_w;
  *r_h = tex_h;
  return texid;
}

static void immDrawPixelsTexSetupAttributes(IMMDrawPixelsTexState *state)
{
  GPUVertFormat *vert_format = immVertexFormat();
  state->pos = GPU_vertformat_attr_add(vert_format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  state->texco = GPU_vertformat_attr_add(
      vert_format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
}

/* To be used before calling immDrawPixelsTex
 * Default shader is GPU_SHADER_2D_IMAGE_COLOR
 * You can still set uniforms with :
 * GPU_shader_uniform_int(shader, GPU_shader_get_uniform_ensure(shader, "name"), 0);
 * */
IMMDrawPixelsTexState immDrawPixelsTexSetup(int builtin)
{
  IMMDrawPixelsTexState state;
  immDrawPixelsTexSetupAttributes(&state);

  state.shader = GPU_shader_get_builtin_shader(builtin);

  /* Shader will be unbind by immUnbindProgram in immDrawPixelsTexScaled_clipping */
  immBindBuiltinProgram(builtin);
  immUniform1i("image", 0);
  state.do_shader_unbind = true;

  return state;
}

/* Use the currently bound shader.
 *
 * Use immDrawPixelsTexSetup to bind the shader you
 * want before calling immDrawPixelsTex.
 *
 * If using a special shader double check it uses the same
 * attributes "pos" "texCoord" and uniform "image".
 *
 * If color is NULL then use white by default
 *
 * Be also aware that this function unbinds the shader when
 * it's finished.
 * */
void immDrawPixelsTexScaled_clipping(IMMDrawPixelsTexState *state,
                                     float x,
                                     float y,
                                     int img_w,
                                     int img_h,
                                     int format,
                                     int type,
                                     int zoomfilter,
                                     void *rect,
                                     float scaleX,
                                     float scaleY,
                                     float clip_min_x,
                                     float clip_min_y,
                                     float clip_max_x,
                                     float clip_max_y,
                                     float xzoom,
                                     float yzoom,
                                     float color[4])
{
  unsigned char *uc_rect = (unsigned char *)rect;
  const float *f_rect = (float *)rect;
  int subpart_x, subpart_y, tex_w, tex_h;
  int seamless, offset_x, offset_y, nsubparts_x, nsubparts_y;
  int texid = get_cached_work_texture(&tex_w, &tex_h);
  int components;
  const bool use_clipping = ((clip_min_x < clip_max_x) && (clip_min_y < clip_max_y));
  float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  GLint unpack_row_length;
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_length);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texid);

  /* don't want nasty border artifacts */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, zoomfilter);

  /* setup seamless 2=on, 0=off */
  seamless = ((tex_w < img_w || tex_h < img_h) && tex_w > 2 && tex_h > 2) ? 2 : 0;

  offset_x = tex_w - seamless;
  offset_y = tex_h - seamless;

  nsubparts_x = (img_w + (offset_x - 1)) / (offset_x);
  nsubparts_y = (img_h + (offset_y - 1)) / (offset_y);

  if (format == GL_RGBA) {
    components = 4;
  }
  else if (format == GL_RGB) {
    components = 3;
  }
  else if (format == GL_RED) {
    components = 1;
  }
  else {
    BLI_assert(!"Incompatible format passed to glaDrawPixelsTexScaled");
    return;
  }

  if (type == GL_FLOAT) {
    /* need to set internal format to higher range float */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tex_w, tex_h, 0, format, GL_FLOAT, NULL);
  }
  else {
    /* switch to 8bit RGBA for byte buffer */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, format, GL_UNSIGNED_BYTE, NULL);
  }

  unsigned int pos = state->pos, texco = state->texco;

  /* optional */
  /* NOTE: Shader could be null for GLSL OCIO drawing, it is fine, since
   * it does not need color.
   */
  if (state->shader != NULL && GPU_shader_get_uniform_ensure(state->shader, "color") != -1) {
    immUniformColor4fv((color) ? color : white);
  }

  for (subpart_y = 0; subpart_y < nsubparts_y; subpart_y++) {
    for (subpart_x = 0; subpart_x < nsubparts_x; subpart_x++) {
      int remainder_x = img_w - subpart_x * offset_x;
      int remainder_y = img_h - subpart_y * offset_y;
      int subpart_w = (remainder_x < tex_w) ? remainder_x : tex_w;
      int subpart_h = (remainder_y < tex_h) ? remainder_y : tex_h;
      int offset_left = (seamless && subpart_x != 0) ? 1 : 0;
      int offset_bot = (seamless && subpart_y != 0) ? 1 : 0;
      int offset_right = (seamless && remainder_x > tex_w) ? 1 : 0;
      int offset_top = (seamless && remainder_y > tex_h) ? 1 : 0;
      float rast_x = x + subpart_x * offset_x * xzoom;
      float rast_y = y + subpart_y * offset_y * yzoom;
      /* check if we already got these because we always get 2 more when doing seamless */
      if (subpart_w <= seamless || subpart_h <= seamless) {
        continue;
      }

      if (use_clipping) {
        if (rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX < clip_min_x ||
            rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY < clip_min_y) {
          continue;
        }
        if (rast_x + (float)offset_left * xzoom > clip_max_x ||
            rast_y + (float)offset_bot * yzoom > clip_max_y) {
          continue;
        }
      }

      if (type == GL_FLOAT) {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        subpart_w,
                        subpart_h,
                        format,
                        GL_FLOAT,
                        &f_rect[((size_t)subpart_y) * offset_y * img_w * components +
                                subpart_x * offset_x * components]);

        /* add an extra border of pixels so linear looks ok at edges of full image */
        if (subpart_w < tex_w) {
          glTexSubImage2D(GL_TEXTURE_2D,
                          0,
                          subpart_w,
                          0,
                          1,
                          subpart_h,
                          format,
                          GL_FLOAT,
                          &f_rect[((size_t)subpart_y) * offset_y * img_w * components +
                                  (subpart_x * offset_x + subpart_w - 1) * components]);
        }
        if (subpart_h < tex_h) {
          glTexSubImage2D(
              GL_TEXTURE_2D,
              0,
              0,
              subpart_h,
              subpart_w,
              1,
              format,
              GL_FLOAT,
              &f_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components +
                      subpart_x * offset_x * components]);
        }
        if (subpart_w < tex_w && subpart_h < tex_h) {
          glTexSubImage2D(
              GL_TEXTURE_2D,
              0,
              subpart_w,
              subpart_h,
              1,
              1,
              format,
              GL_FLOAT,
              &f_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components +
                      (subpart_x * offset_x + subpart_w - 1) * components]);
        }
      }
      else {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        subpart_w,
                        subpart_h,
                        format,
                        GL_UNSIGNED_BYTE,
                        &uc_rect[((size_t)subpart_y) * offset_y * img_w * components +
                                 subpart_x * offset_x * components]);

        if (subpart_w < tex_w) {
          glTexSubImage2D(GL_TEXTURE_2D,
                          0,
                          subpart_w,
                          0,
                          1,
                          subpart_h,
                          format,
                          GL_UNSIGNED_BYTE,
                          &uc_rect[((size_t)subpart_y) * offset_y * img_w * components +
                                   (subpart_x * offset_x + subpart_w - 1) * components]);
        }
        if (subpart_h < tex_h) {
          glTexSubImage2D(
              GL_TEXTURE_2D,
              0,
              0,
              subpart_h,
              subpart_w,
              1,
              format,
              GL_UNSIGNED_BYTE,
              &uc_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components +
                       subpart_x * offset_x * components]);
        }
        if (subpart_w < tex_w && subpart_h < tex_h) {
          glTexSubImage2D(
              GL_TEXTURE_2D,
              0,
              subpart_w,
              subpart_h,
              1,
              1,
              format,
              GL_UNSIGNED_BYTE,
              &uc_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components +
                       (subpart_x * offset_x + subpart_w - 1) * components]);
        }
      }

      immBegin(GPU_PRIM_TRI_FAN, 4);
      immAttr2f(texco, (float)(0 + offset_left) / tex_w, (float)(0 + offset_bot) / tex_h);
      immVertex2f(pos, rast_x + (float)offset_left * xzoom, rast_y + (float)offset_bot * yzoom);

      immAttr2f(texco, (float)(subpart_w - offset_right) / tex_w, (float)(0 + offset_bot) / tex_h);
      immVertex2f(pos,
                  rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX,
                  rast_y + (float)offset_bot * yzoom);

      immAttr2f(texco,
                (float)(subpart_w - offset_right) / tex_w,
                (float)(subpart_h - offset_top) / tex_h);
      immVertex2f(pos,
                  rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX,
                  rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);

      immAttr2f(texco, (float)(0 + offset_left) / tex_w, (float)(subpart_h - offset_top) / tex_h);
      immVertex2f(pos,
                  rast_x + (float)offset_left * xzoom,
                  rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);
      immEnd();

      /* NOTE: Weirdly enough this is only required on macOS. Without this there is some sort of
       * bleeding of data is happening from tiles which are drawn later on.
       * This doesn't seem to be too slow,
       * but still would be nice to have fast and nice solution. */
#ifdef __APPLE__
      GPU_flush();
#endif
    }
  }

  if (state->do_shader_unbind) {
    immUnbindProgram();
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length);
}

void immDrawPixelsTexScaled(IMMDrawPixelsTexState *state,
                            float x,
                            float y,
                            int img_w,
                            int img_h,
                            int format,
                            int type,
                            int zoomfilter,
                            void *rect,
                            float scaleX,
                            float scaleY,
                            float xzoom,
                            float yzoom,
                            float color[4])
{
  immDrawPixelsTexScaled_clipping(state,
                                  x,
                                  y,
                                  img_w,
                                  img_h,
                                  format,
                                  type,
                                  zoomfilter,
                                  rect,
                                  scaleX,
                                  scaleY,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  xzoom,
                                  yzoom,
                                  color);
}

void immDrawPixelsTex(IMMDrawPixelsTexState *state,
                      float x,
                      float y,
                      int img_w,
                      int img_h,
                      int format,
                      int type,
                      int zoomfilter,
                      void *rect,
                      float xzoom,
                      float yzoom,
                      float color[4])
{
  immDrawPixelsTexScaled_clipping(state,
                                  x,
                                  y,
                                  img_w,
                                  img_h,
                                  format,
                                  type,
                                  zoomfilter,
                                  rect,
                                  1.0f,
                                  1.0f,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  xzoom,
                                  yzoom,
                                  color);
}

void immDrawPixelsTex_clipping(IMMDrawPixelsTexState *state,
                               float x,
                               float y,
                               int img_w,
                               int img_h,
                               int format,
                               int type,
                               int zoomfilter,
                               void *rect,
                               float clip_min_x,
                               float clip_min_y,
                               float clip_max_x,
                               float clip_max_y,
                               float xzoom,
                               float yzoom,
                               float color[4])
{
  immDrawPixelsTexScaled_clipping(state,
                                  x,
                                  y,
                                  img_w,
                                  img_h,
                                  format,
                                  type,
                                  zoomfilter,
                                  rect,
                                  1.0f,
                                  1.0f,
                                  clip_min_x,
                                  clip_min_y,
                                  clip_max_x,
                                  clip_max_y,
                                  xzoom,
                                  yzoom,
                                  color);
}

/* *************** glPolygonOffset hack ************* */

float bglPolygonOffsetCalc(const float winmat[16], float viewdist, float dist)
{
  if (winmat[15] > 0.5f) {
#if 1
    return 0.00001f * dist * viewdist;  // ortho tweaking
#else
    static float depth_fac = 0.0f;
    if (depth_fac == 0.0f) {
      int depthbits;
      glGetIntegerv(GL_DEPTH_BITS, &depthbits);
      depth_fac = 1.0f / (float)((1 << depthbits) - 1);
    }
    offs = (-1.0 / winmat[10]) * dist * depth_fac;

    UNUSED_VARS(viewdist);
#endif
  }
  else {
    /* This adjustment effectively results in reducing the Z value by 0.25%.
     *
     * winmat[14] actually evaluates to `-2 * far * near / (far - near)`,
     * is very close to -0.2 with default clip range,
     * and is used as the coefficient multiplied by `w / z`,
     * thus controlling the z dependent part of the depth value.
     */
    return winmat[14] * -0.0025f * dist;
  }
}

/**
 * \note \a viewdist is only for ortho at the moment.
 */
void bglPolygonOffset(float viewdist, float dist)
{
  static float winmat[16], offset = 0.0f;

  if (dist != 0.0f) {
    // glEnable(GL_POLYGON_OFFSET_FILL);
    // glPolygonOffset(-1.0, -1.0);

    /* hack below is to mimic polygon offset */
    GPU_matrix_projection_get(winmat);

    /* dist is from camera to center point */

    float offs = bglPolygonOffsetCalc(winmat, viewdist, dist);

    winmat[14] -= offs;
    offset += offs;
  }
  else {
    winmat[14] += offset;
    offset = 0.0;
  }

  GPU_matrix_projection_set(winmat);
}

/* **** Color management helper functions for GLSL display/transform ***** */

/* Draw given image buffer on a screen using GLSL for display transform */
void ED_draw_imbuf_clipping(ImBuf *ibuf,
                            float x,
                            float y,
                            int zoomfilter,
                            ColorManagedViewSettings *view_settings,
                            ColorManagedDisplaySettings *display_settings,
                            float clip_min_x,
                            float clip_min_y,
                            float clip_max_x,
                            float clip_max_y,
                            float zoom_x,
                            float zoom_y)
{
  bool force_fallback = false;
  bool need_fallback = true;

  /* Early out */
  if (ibuf->rect == NULL && ibuf->rect_float == NULL) {
    return;
  }

  /* Single channel images could not be transformed using GLSL yet */
  force_fallback |= ibuf->channels == 1;

  /* If user decided not to use GLSL, fallback to glaDrawPixelsAuto */
  force_fallback |= (ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL);

  /* Try to draw buffer using GLSL display transform */
  if (force_fallback == false) {
    int ok;

    IMMDrawPixelsTexState state = {0};
    /* We want GLSL state to be fully handled by OCIO. */
    state.do_shader_unbind = false;
    immDrawPixelsTexSetupAttributes(&state);

    if (ibuf->rect_float) {
      if (ibuf->float_colorspace) {
        ok = IMB_colormanagement_setup_glsl_draw_from_space(
            view_settings, display_settings, ibuf->float_colorspace, ibuf->dither, true);
      }
      else {
        ok = IMB_colormanagement_setup_glsl_draw(
            view_settings, display_settings, ibuf->dither, true);
      }
    }
    else {
      ok = IMB_colormanagement_setup_glsl_draw_from_space(
          view_settings, display_settings, ibuf->rect_colorspace, ibuf->dither, false);
    }

    if (ok) {
      if (ibuf->rect_float) {
        int format = 0;

        if (ibuf->channels == 3) {
          format = GL_RGB;
        }
        else if (ibuf->channels == 4) {
          format = GL_RGBA;
        }
        else {
          BLI_assert(!"Incompatible number of channels for GLSL display");
        }

        if (format != 0) {
          immDrawPixelsTex_clipping(&state,
                                    x,
                                    y,
                                    ibuf->x,
                                    ibuf->y,
                                    format,
                                    GL_FLOAT,
                                    zoomfilter,
                                    ibuf->rect_float,
                                    clip_min_x,
                                    clip_min_y,
                                    clip_max_x,
                                    clip_max_y,
                                    zoom_x,
                                    zoom_y,
                                    NULL);
        }
      }
      else if (ibuf->rect) {
        /* ibuf->rect is always RGBA */
        immDrawPixelsTex_clipping(&state,
                                  x,
                                  y,
                                  ibuf->x,
                                  ibuf->y,
                                  GL_RGBA,
                                  GL_UNSIGNED_BYTE,
                                  zoomfilter,
                                  ibuf->rect,
                                  clip_min_x,
                                  clip_min_y,
                                  clip_max_x,
                                  clip_max_y,
                                  zoom_x,
                                  zoom_y,
                                  NULL);
      }

      IMB_colormanagement_finish_glsl_draw();

      need_fallback = false;
    }
  }

  /* In case GLSL failed or not usable, fallback to glaDrawPixelsAuto */
  if (need_fallback) {
    unsigned char *display_buffer;
    void *cache_handle;

    display_buffer = IMB_display_buffer_acquire(
        ibuf, view_settings, display_settings, &cache_handle);

    if (display_buffer) {
      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
      immDrawPixelsTex_clipping(&state,
                                x,
                                y,
                                ibuf->x,
                                ibuf->y,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                zoomfilter,
                                display_buffer,
                                clip_min_x,
                                clip_min_y,
                                clip_max_x,
                                clip_max_y,
                                zoom_x,
                                zoom_y,
                                NULL);
    }

    IMB_display_buffer_release(cache_handle);
  }
}

void ED_draw_imbuf(ImBuf *ibuf,
                   float x,
                   float y,
                   int zoomfilter,
                   ColorManagedViewSettings *view_settings,
                   ColorManagedDisplaySettings *display_settings,
                   float zoom_x,
                   float zoom_y)
{
  ED_draw_imbuf_clipping(ibuf,
                         x,
                         y,
                         zoomfilter,
                         view_settings,
                         display_settings,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         zoom_x,
                         zoom_y);
}

void ED_draw_imbuf_ctx_clipping(const bContext *C,
                                ImBuf *ibuf,
                                float x,
                                float y,
                                int zoomfilter,
                                float clip_min_x,
                                float clip_min_y,
                                float clip_max_x,
                                float clip_max_y,
                                float zoom_x,
                                float zoom_y)
{
  ColorManagedViewSettings *view_settings;
  ColorManagedDisplaySettings *display_settings;

  IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);

  ED_draw_imbuf_clipping(ibuf,
                         x,
                         y,
                         zoomfilter,
                         view_settings,
                         display_settings,
                         clip_min_x,
                         clip_min_y,
                         clip_max_x,
                         clip_max_y,
                         zoom_x,
                         zoom_y);
}

void ED_draw_imbuf_ctx(
    const bContext *C, ImBuf *ibuf, float x, float y, int zoomfilter, float zoom_x, float zoom_y)
{
  ED_draw_imbuf_ctx_clipping(C, ibuf, x, y, zoomfilter, 0.0f, 0.0f, 0.0f, 0.0f, zoom_x, zoom_y);
}

int ED_draw_imbuf_method(ImBuf *ibuf)
{
  if (U.image_draw_method == IMAGE_DRAW_METHOD_AUTO) {
    /* Use faster GLSL when CPU to GPU transfer is unlikely to be a bottleneck,
     * otherwise do color management on CPU side. */
    const size_t threshold = 2048 * 2048 * 4 * sizeof(float);
    const size_t data_size = (ibuf->rect_float) ? sizeof(float) : sizeof(uchar);
    const size_t size = ibuf->x * ibuf->y * ibuf->channels * data_size;

    return (size > threshold) ? IMAGE_DRAW_METHOD_2DTEXTURE : IMAGE_DRAW_METHOD_GLSL;
  }
  else {
    return U.image_draw_method;
  }
}

/* don't move to GPU_immediate_util.h because this uses user-prefs
 * and isn't very low level */
void immDrawBorderCorners(unsigned int pos, const rcti *border, float zoomx, float zoomy)
{
  float delta_x = 4.0f * UI_DPI_FAC / zoomx;
  float delta_y = 4.0f * UI_DPI_FAC / zoomy;

  delta_x = min_ff(delta_x, border->xmax - border->xmin);
  delta_y = min_ff(delta_y, border->ymax - border->ymin);

  /* left bottom corner */
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos, border->xmin, border->ymin + delta_y);
  immVertex2f(pos, border->xmin, border->ymin);
  immVertex2f(pos, border->xmin + delta_x, border->ymin);
  immEnd();

  /* left top corner */
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos, border->xmin, border->ymax - delta_y);
  immVertex2f(pos, border->xmin, border->ymax);
  immVertex2f(pos, border->xmin + delta_x, border->ymax);
  immEnd();

  /* right bottom corner */
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos, border->xmax - delta_x, border->ymin);
  immVertex2f(pos, border->xmax, border->ymin);
  immVertex2f(pos, border->xmax, border->ymin + delta_y);
  immEnd();

  /* right top corner */
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos, border->xmax - delta_x, border->ymax);
  immVertex2f(pos, border->xmax, border->ymax);
  immVertex2f(pos, border->xmax, border->ymax - delta_y);
  immEnd();
}

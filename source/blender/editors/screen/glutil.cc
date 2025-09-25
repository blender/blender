/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cstdio>
#include <cstring>

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_utildefines.h"

#include "BIF_glutil.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "GPU_context.hh"
#include "GPU_immediate.hh"
#include "GPU_texture.hh"

/* ******************************************** */

static void immDrawPixelsTexSetupAttributes(IMMDrawPixelsTexState *state)
{
  GPUVertFormat *vert_format = immVertexFormat();
  state->pos = GPU_vertformat_attr_add(
      vert_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  state->texco = GPU_vertformat_attr_add(
      vert_format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);
}

IMMDrawPixelsTexState immDrawPixelsTexSetup(int builtin)
{
  IMMDrawPixelsTexState state;
  immDrawPixelsTexSetupAttributes(&state);

  state.shader = GPU_shader_get_builtin_shader(GPUBuiltinShader(builtin));

  /* Shader will be unbind by immUnbindProgram in a `immDrawPixelsTex` function. */
  immBindBuiltinProgram(GPUBuiltinShader(builtin));
  state.do_shader_unbind = true;

  return state;
}

void immDrawPixelsTexScaledFullSize(const IMMDrawPixelsTexState *state,
                                    const float x,
                                    const float y,
                                    const int img_w,
                                    const int img_h,
                                    const blender::gpu::TextureFormat gpu_format,
                                    const bool use_filter,
                                    const void *rect,
                                    const float scaleX,
                                    const float scaleY,
                                    const float xzoom,
                                    const float yzoom,
                                    const float color[4])
{
  static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const float draw_width = img_w * scaleX * xzoom;
  const float draw_height = img_h * scaleY * yzoom;
  /* Down-scaling with regular bi-linear interpolation (i.e. #GL_LINEAR) doesn't give good
   * filtering results. Mipmaps can be used to get better results (i.e. #GL_LINEAR_MIPMAP_LINEAR),
   * so always use mipmaps when filtering. */
  const bool use_mipmap = use_filter && ((draw_width < img_w) || (draw_height < img_h));
  const int mip_len = use_mipmap ? 9999 : 1;

  blender::gpu::Texture *tex = GPU_texture_create_2d(
      "immDrawPixels", img_w, img_h, mip_len, gpu_format, GPU_TEXTURE_USAGE_SHADER_READ, nullptr);

  const bool use_float_data = ELEM(gpu_format,
                                   blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
                                   blender::gpu::TextureFormat::SFLOAT_16_16_16,
                                   blender::gpu::TextureFormat::SFLOAT_16);
  eGPUDataFormat gpu_data_format = (use_float_data) ? GPU_DATA_FLOAT : GPU_DATA_UBYTE;
  GPU_texture_update(tex, gpu_data_format, rect);

  GPU_texture_filter_mode(tex, use_filter);
  if (use_mipmap) {
    GPU_texture_update_mipmap_chain(tex);
    GPU_texture_mipmap_mode(tex, true, true);
  }
  GPU_texture_extend_mode(tex, GPU_SAMPLER_EXTEND_MODE_EXTEND);

  GPU_texture_bind(tex, 0);

  /* optional */
  /* NOTE: Shader could be null for GLSL OCIO drawing, it is fine, since
   * it does not need color.
   */
  if (state->shader != nullptr && GPU_shader_get_uniform(state->shader, "color") != -1) {
    immUniformColor4fv((color) ? color : white);
  }

  uint pos = state->pos, texco = state->texco;

  immBegin(GPU_PRIM_TRI_FAN, 4);
  immAttr2f(texco, 0.0f, 0.0f);
  immVertex2f(pos, x, y);

  immAttr2f(texco, 1.0f, 0.0f);
  immVertex2f(pos, x + draw_width, y);

  immAttr2f(texco, 1.0f, 1.0f);
  immVertex2f(pos, x + draw_width, y + draw_height);

  immAttr2f(texco, 0.0f, 1.0f);
  immVertex2f(pos, x, y + draw_height);
  immEnd();

  if (state->do_shader_unbind) {
    immUnbindProgram();
  }

  GPU_texture_unbind(tex);
  GPU_texture_free(tex);
}

void immDrawPixelsTexTiled_scaling_clipping(IMMDrawPixelsTexState *state,
                                            float x,
                                            float y,
                                            int img_w,
                                            int img_h,
                                            blender::gpu::TextureFormat gpu_format,
                                            bool use_filter,
                                            const void *rect,
                                            float scaleX,
                                            float scaleY,
                                            float clip_min_x,
                                            float clip_min_y,
                                            float clip_max_x,
                                            float clip_max_y,
                                            float xzoom,
                                            float yzoom,
                                            const float color[4])
{
  int subpart_x, subpart_y, tex_w = 256, tex_h = 256;
  if (ELEM(GPU_backend_get_type(), GPU_BACKEND_METAL, GPU_BACKEND_VULKAN)) {
    /* NOTE: These backend will keep all temporary texture memory within a command
     * submission in-flight, so using a partial tile size does not provide any tangible memory
     * reduction, but does incur additional API overhead and significant cache inefficiency on
     * specific platforms.
     *
     * The Metal API also provides smart resource paging such that the application can
     * still efficiently swap memory, even if system is low in physical memory. */
    tex_w = img_w;
    tex_h = img_h;
  }
  int seamless, offset_x, offset_y, nsubparts_x, nsubparts_y;
  int components;
  const bool use_clipping = ((clip_min_x < clip_max_x) && (clip_min_y < clip_max_y));
  const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  if (ELEM(gpu_format,
           blender::gpu::TextureFormat::UNORM_8_8_8_8,
           blender::gpu::TextureFormat::SFLOAT_16_16_16_16))
  {
    components = 4;
  }
  else if (ELEM(gpu_format, blender::gpu::TextureFormat::SFLOAT_16_16_16)) {
    components = 3;
  }
  else if (ELEM(gpu_format,
                blender::gpu::TextureFormat::UNORM_8,
                blender::gpu::TextureFormat::SFLOAT_16))
  {
    components = 1;
  }
  else {
    BLI_assert_msg(0, "Incompatible format passed to immDrawPixels");
    return;
  }

  const bool use_float_data = ELEM(gpu_format,
                                   blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
                                   blender::gpu::TextureFormat::SFLOAT_16_16_16,
                                   blender::gpu::TextureFormat::SFLOAT_16);
  eGPUDataFormat gpu_data = (use_float_data) ? GPU_DATA_FLOAT : GPU_DATA_UBYTE;
  size_t stride = components * ((use_float_data) ? sizeof(float) : sizeof(uchar));

  blender::gpu::Texture *tex = GPU_texture_create_2d(
      "immDrawPixels", tex_w, tex_h, 1, gpu_format, GPU_TEXTURE_USAGE_SHADER_READ, nullptr);

  GPU_texture_filter_mode(tex, use_filter);
  GPU_texture_extend_mode(tex, GPU_SAMPLER_EXTEND_MODE_EXTEND);

  GPU_texture_bind(tex, 0);

  /* setup seamless 2=on, 0=off */
  seamless = ((tex_w < img_w || tex_h < img_h) && tex_w > 2 && tex_h > 2) ? 2 : 0;

  offset_x = tex_w - seamless;
  offset_y = tex_h - seamless;

  nsubparts_x = (img_w + (offset_x - 1)) / (offset_x);
  nsubparts_y = (img_h + (offset_y - 1)) / (offset_y);

  /* optional */
  /* NOTE: Shader could be null for GLSL OCIO drawing, it is fine, since
   * it does not need color.
   */
  if (state->shader != nullptr && GPU_shader_get_uniform(state->shader, "color") != -1) {
    immUniformColor4fv((color) ? color : white);
  }

  GPU_unpack_row_length_set(img_w);

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

      int right = subpart_w - offset_right;
      int top = subpart_h - offset_top;
      int bottom = 0 + offset_bot;
      int left = 0 + offset_left;

      if (use_clipping) {
        if (rast_x + right * xzoom * scaleX < clip_min_x ||
            rast_y + top * yzoom * scaleY < clip_min_y)
        {
          continue;
        }
        if (rast_x + left * xzoom > clip_max_x || rast_y + bottom * yzoom > clip_max_y) {
          continue;
        }
      }

      {
        int src_y = subpart_y * offset_y;
        int src_x = subpart_x * offset_x;

#define DATA(_y, _x) (static_cast<const char *>(rect) + stride * (size_t(_y) * img_w + (_x)))
        {
          const void *data = DATA(src_y, src_x);
          GPU_texture_update_sub(tex, gpu_data, data, 0, 0, 0, subpart_w, subpart_h, 0);
        }
        /* Add an extra border of pixels so linear interpolation looks ok
         * at edges of full image. */
        if (subpart_w < tex_w) {
          const void *data = DATA(src_y, src_x + subpart_w - 1);
          const int offset[2] = {subpart_w, 0};
          const int extent[2] = {1, subpart_h};
          GPU_texture_update_sub(tex, gpu_data, data, UNPACK2(offset), 0, UNPACK2(extent), 0);
        }
        if (subpart_h < tex_h) {
          const void *data = DATA(src_y + subpart_h - 1, src_x);
          const int offset[2] = {0, subpart_h};
          const int extent[2] = {subpart_w, 1};
          GPU_texture_update_sub(tex, gpu_data, data, UNPACK2(offset), 0, UNPACK2(extent), 0);
        }

        if (subpart_w < tex_w && subpart_h < tex_h) {
          const void *data = DATA(src_y + subpart_h - 1, src_x + subpart_w - 1);
          const int offset[2] = {subpart_w, subpart_h};
          const int extent[2] = {1, 1};
          GPU_texture_update_sub(tex, gpu_data, data, UNPACK2(offset), 0, UNPACK2(extent), 0);
        }
#undef DATA
      }

      uint pos = state->pos, texco = state->texco;

      immBegin(GPU_PRIM_TRI_FAN, 4);
      immAttr2f(texco, left / float(tex_w), bottom / float(tex_h));
      immVertex2f(pos, rast_x + offset_left * xzoom, rast_y + offset_bot * yzoom);

      immAttr2f(texco, right / float(tex_w), bottom / float(tex_h));
      immVertex2f(pos, rast_x + right * xzoom * scaleX, rast_y + offset_bot * yzoom);

      immAttr2f(texco, right / float(tex_w), top / float(tex_h));
      immVertex2f(pos, rast_x + right * xzoom * scaleX, rast_y + top * yzoom * scaleY);

      immAttr2f(texco, left / float(tex_w), top / float(tex_h));
      immVertex2f(pos, rast_x + offset_left * xzoom, rast_y + top * yzoom * scaleY);
      immEnd();
    }
  }

  if (state->do_shader_unbind) {
    immUnbindProgram();
  }

  GPU_texture_unbind(tex);
  GPU_texture_free(tex);

  /* Restore default. */
  GPU_unpack_row_length_set(0);
}

void immDrawPixelsTexTiled_scaling(IMMDrawPixelsTexState *state,
                                   float x,
                                   float y,
                                   int img_w,
                                   int img_h,
                                   blender::gpu::TextureFormat gpu_format,
                                   bool use_filter,
                                   const void *rect,
                                   float scaleX,
                                   float scaleY,
                                   float xzoom,
                                   float yzoom,
                                   const float color[4])
{
  immDrawPixelsTexTiled_scaling_clipping(state,
                                         x,
                                         y,
                                         img_w,
                                         img_h,
                                         gpu_format,
                                         use_filter,
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

void immDrawPixelsTexTiled(IMMDrawPixelsTexState *state,
                           float x,
                           float y,
                           int img_w,
                           int img_h,
                           blender::gpu::TextureFormat gpu_format,
                           bool use_filter,
                           const void *rect,
                           float xzoom,
                           float yzoom,
                           const float color[4])
{
  immDrawPixelsTexTiled_scaling_clipping(state,
                                         x,
                                         y,
                                         img_w,
                                         img_h,
                                         gpu_format,
                                         use_filter,
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

void immDrawPixelsTexTiled_clipping(IMMDrawPixelsTexState *state,
                                    float x,
                                    float y,
                                    int img_w,
                                    int img_h,
                                    blender::gpu::TextureFormat gpu_format,
                                    bool use_filter,
                                    const void *rect,
                                    float clip_min_x,
                                    float clip_min_y,
                                    float clip_max_x,
                                    float clip_max_y,
                                    float xzoom,
                                    float yzoom,
                                    const float color[4])
{
  immDrawPixelsTexTiled_scaling_clipping(state,
                                         x,
                                         y,
                                         img_w,
                                         img_h,
                                         gpu_format,
                                         use_filter,
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

/* **** Color management helper functions for GLSL display/transform ***** */

void ED_draw_imbuf_clipping(ImBuf *ibuf,
                            float x,
                            float y,
                            bool use_filter,
                            const ColorManagedViewSettings *view_settings,
                            const ColorManagedDisplaySettings *display_settings,
                            float clip_min_x,
                            float clip_min_y,
                            float clip_max_x,
                            float clip_max_y,
                            float zoom_x,
                            float zoom_y)
{
  using namespace blender::gpu;

  bool force_fallback = false;
  bool need_fallback = true;

  /* Early out */
  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    return;
  }

  /* Single channel images could not be transformed using GLSL yet */
  force_fallback |= ibuf->channels == 1;

  /* If user decided not to use GLSL, fallback to glaDrawPixelsAuto */
  force_fallback |= (ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL);

  /* Try to draw buffer using GLSL display transform */
  if (force_fallback == false) {
    int ok;

    IMMDrawPixelsTexState state = {nullptr};
    /* We want GLSL state to be fully handled by OCIO. */
    state.do_shader_unbind = false;
    immDrawPixelsTexSetupAttributes(&state);

    if (ibuf->float_buffer.data) {
      if (ibuf->float_buffer.colorspace) {
        ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings,
                                                            display_settings,
                                                            ibuf->float_buffer.colorspace,
                                                            ibuf->dither,
                                                            true,
                                                            false);
      }
      else {
        ok = IMB_colormanagement_setup_glsl_draw(
            view_settings, display_settings, ibuf->dither, true);
      }
    }
    else {
      ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings,
                                                          display_settings,
                                                          ibuf->byte_buffer.colorspace,
                                                          ibuf->dither,
                                                          false,
                                                          false);
    }

    if (ok) {
      if (ibuf->float_buffer.data) {
        TextureFormat format = TextureFormat::Invalid;

        if (ibuf->channels == 3) {
          format = TextureFormat::SFLOAT_16_16_16;
        }
        else if (ibuf->channels == 4) {
          format = TextureFormat::SFLOAT_16_16_16_16;
        }
        else {
          BLI_assert_msg(0, "Incompatible number of channels for GLSL display");
        }

        if (format != TextureFormat::Invalid) {
          immDrawPixelsTexTiled_clipping(&state,
                                         x,
                                         y,
                                         ibuf->x,
                                         ibuf->y,
                                         format,
                                         use_filter,
                                         ibuf->float_buffer.data,
                                         clip_min_x,
                                         clip_min_y,
                                         clip_max_x,
                                         clip_max_y,
                                         zoom_x,
                                         zoom_y,
                                         nullptr);
        }
      }
      else if (ibuf->byte_buffer.data) {
        /* ibuf->rect is always RGBA */
        immDrawPixelsTexTiled_clipping(&state,
                                       x,
                                       y,
                                       ibuf->x,
                                       ibuf->y,
                                       blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                       use_filter,
                                       ibuf->byte_buffer.data,
                                       clip_min_x,
                                       clip_min_y,
                                       clip_max_x,
                                       clip_max_y,
                                       zoom_x,
                                       zoom_y,
                                       nullptr);
      }

      IMB_colormanagement_finish_glsl_draw();

      need_fallback = false;
    }
  }

  /* In case GLSL failed or not usable, fallback to glaDrawPixelsAuto */
  if (need_fallback) {
    uchar *display_buffer;
    void *cache_handle;

    display_buffer = IMB_display_buffer_acquire(
        ibuf, view_settings, display_settings, &cache_handle);

    if (display_buffer) {
      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
      immDrawPixelsTexTiled_clipping(&state,
                                     x,
                                     y,
                                     ibuf->x,
                                     ibuf->y,
                                     blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                     use_filter,
                                     display_buffer,
                                     clip_min_x,
                                     clip_min_y,
                                     clip_max_x,
                                     clip_max_y,
                                     zoom_x,
                                     zoom_y,
                                     nullptr);
    }

    IMB_display_buffer_release(cache_handle);
  }
}

void ED_draw_imbuf(ImBuf *ibuf,
                   float x,
                   float y,
                   bool use_filter,
                   const ColorManagedViewSettings *view_settings,
                   const ColorManagedDisplaySettings *display_settings,
                   float zoom_x,
                   float zoom_y)
{
  ED_draw_imbuf_clipping(ibuf,
                         x,
                         y,
                         use_filter,
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
                                bool use_filter,
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
                         use_filter,
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
    const bContext *C, ImBuf *ibuf, float x, float y, bool use_filter, float zoom_x, float zoom_y)
{
  ED_draw_imbuf_ctx_clipping(C, ibuf, x, y, use_filter, 0.0f, 0.0f, 0.0f, 0.0f, zoom_x, zoom_y);
}

int ED_draw_imbuf_method(const ImBuf *ibuf)
{
  if (U.image_draw_method == IMAGE_DRAW_METHOD_AUTO) {
    /* Use faster GLSL when CPU to GPU transfer is unlikely to be a bottleneck,
     * otherwise do color management on CPU side. */
    const size_t threshold = sizeof(float[4]) * 2048 * 2048;
    const size_t data_size = (ibuf->float_buffer.data) ? sizeof(float) : sizeof(uchar);
    const size_t size = size_t(ibuf->x) * size_t(ibuf->y) * size_t(ibuf->channels) * data_size;

    return (size > threshold) ? IMAGE_DRAW_METHOD_2DTEXTURE : IMAGE_DRAW_METHOD_GLSL;
  }
  return U.image_draw_method;
}

void immDrawBorderCorners(uint pos, const rcti *border, float zoomx, float zoomy)
{
  float delta_x = 4.0f * UI_SCALE_FAC / zoomx;
  float delta_y = 4.0f * UI_SCALE_FAC / zoomy;

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

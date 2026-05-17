/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#include "GPU_shader_builtin.hh"
#include "GPU_texture.hh"

namespace blender {

struct rcti;

struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct ImBuf;
struct bContext;
namespace gpu {
class Shader;
}  // namespace gpu

/**
 * Utility to render a quad using pixel bitmap
 * data in system memory.
 */
class PixelBitmapDrawer {
  gpu::Shader *shader;
  unsigned int pos;
  unsigned int texco;

 public:
  /**
   * Prepares for drawing, using one of built-in shaders
   * (e.g. GPU_SHADER_3D_IMAGE_COLOR).
   *
   * Before drawing additional uniforms can be set on the shader.
   */
  PixelBitmapDrawer(GPUBuiltinShader builtin_shader);

  /**
   * Prepares for drawing, with shader set up by the caller.
   * The shader is expected to use `pos` and `texCoord` attributes,
   * and `image` texture.
   */
  PixelBitmapDrawer();

  gpu::Shader *shader_get()
  {
    return shader;
  }

  /**
   * Draws pixel data on a rectangle.
   *
   * The model-view and projection matrices are assumed to define a 1-to-1 mapping to screen space.
   *
   * If color is null, white is used.
   */
  void draw(float x,
            float y,
            int img_w,
            int img_h,
            gpu::TextureFormat gpu_format,
            bool use_filter,
            const void *rect,
            float scale_x,
            float scale_y,
            const float color[4]);

 private:
  void init_vertex_attributes();
};

/**
 * Draw the image buffer, with the given display transform parameters.
 */
void ED_draw_imbuf(const ImBuf *ibuf,
                   float x,
                   float y,
                   bool use_filter,
                   const ColorManagedViewSettings *view_settings,
                   const ColorManagedDisplaySettings *display_settings,
                   float zoom_x,
                   float zoom_y);
/**
 * Draw the image buffer, using display transform parameters from the context.
 */
void ED_draw_imbuf_ctx(const bContext *C,
                       const ImBuf *ibuf,
                       float x,
                       float y,
                       bool use_filter,
                       float zoom_x,
                       float zoom_y);

/**
 * Don't move to `GPU_immediate_util.hh`
 * because this uses user-preferences and isn't very low level.
 */
void immDrawBorderCorners(unsigned int pos, const rcti *border, float zoomx, float zoomy);

}  // namespace blender

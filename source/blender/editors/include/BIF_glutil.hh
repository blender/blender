/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

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

struct IMMDrawPixelsTexState {
  gpu::Shader *shader;
  unsigned int pos;
  unsigned int texco;
  bool do_shader_unbind;
};

/* To be used before calling immDrawPixels
 * Default shader is GPU_SHADER_2D_IMAGE_COLOR
 * Returns a shader to be able to set uniforms */
/**
 * To be used before calling #immDrawPixels
 * Default shader is #GPU_SHADER_2D_IMAGE_COLOR
 * You can still set uniforms with:
 * `GPU_shader_uniform_*(shader, "name", value);`
 */
IMMDrawPixelsTexState immDrawPixelsTexSetup(int builtin);

/**
 * Draws pixel data on a rectangle.
 *
 * Use the currently bound shader.
 *
 * Use #immDrawPixelsTexSetup to bind the shader you want before calling #immDrawPixels.
 *
 * If using a special shader double check it uses the same attributes "pos" "texCoord" and uniform
 * "image".
 *
 * If color is NULL then use white by default
 *
 * Unless `state->do_shader_unbind` is explicitly set to `false`, the shader is unbound when
 * finished.
 *
 * The model-view and projection matrices are assumed to define a 1-to-1 mapping to screen space.
 */
void immDrawPixels(const IMMDrawPixelsTexState *state,
                   float x,
                   float y,
                   int img_w,
                   int img_h,
                   gpu::TextureFormat gpu_format,
                   bool use_filter,
                   const void *rect,
                   float scale_x,
                   float scale_y,
                   const float color[4]);

/* Image buffer drawing functions, with display transform
 *
 * The view and display settings can either be specified manually,
 * or retrieved from the context with the '_ctx' variations.*/

void ED_draw_imbuf(const ImBuf *ibuf,
                   float x,
                   float y,
                   bool use_filter,
                   const ColorManagedViewSettings *view_settings,
                   const ColorManagedDisplaySettings *display_settings,
                   float zoom_x,
                   float zoom_y);
/**
 * Draw given image buffer on a screen using GLSL for display transform.
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

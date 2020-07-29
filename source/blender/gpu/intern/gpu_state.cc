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
 */

/** \file
 * \ingroup gpu
 */

#ifndef GPU_STANDALONE
#  include "DNA_userdef_types.h"
#  define PIXELSIZE (U.pixelsize)
#else
#  define PIXELSIZE (1.0f)
#endif

#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_state.h"

static GLenum gpu_get_gl_blendfunction(eGPUBlendFunction blend)
{
  switch (blend) {
    case GPU_ONE:
      return GL_ONE;
    case GPU_SRC_ALPHA:
      return GL_SRC_ALPHA;
    case GPU_ONE_MINUS_SRC_ALPHA:
      return GL_ONE_MINUS_SRC_ALPHA;
    case GPU_DST_COLOR:
      return GL_DST_COLOR;
    case GPU_ZERO:
      return GL_ZERO;
    default:
      BLI_assert(!"Unhandled blend mode");
      return GL_ZERO;
  }
}

void GPU_blend(bool enable)
{
  if (enable) {
    glEnable(GL_BLEND);
  }
  else {
    glDisable(GL_BLEND);
  }
}

void GPU_blend_set_func(eGPUBlendFunction sfactor, eGPUBlendFunction dfactor)
{
  glBlendFunc(gpu_get_gl_blendfunction(sfactor), gpu_get_gl_blendfunction(dfactor));
}

void GPU_blend_set_func_separate(eGPUBlendFunction src_rgb,
                                 eGPUBlendFunction dst_rgb,
                                 eGPUBlendFunction src_alpha,
                                 eGPUBlendFunction dst_alpha)
{
  glBlendFuncSeparate(gpu_get_gl_blendfunction(src_rgb),
                      gpu_get_gl_blendfunction(dst_rgb),
                      gpu_get_gl_blendfunction(src_alpha),
                      gpu_get_gl_blendfunction(dst_alpha));
}

void GPU_face_culling(eGPUFaceCull culling)
{
  if (culling == GPU_CULL_NONE) {
    glDisable(GL_CULL_FACE);
  }
  else {
    glEnable(GL_CULL_FACE);
    glCullFace((culling == GPU_CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
}

void GPU_front_facing(bool invert)
{
  glFrontFace((invert) ? GL_CW : GL_CCW);
}

void GPU_provoking_vertex(eGPUProvokingVertex vert)
{
  glProvokingVertex((vert == GPU_VERTEX_FIRST) ? GL_FIRST_VERTEX_CONVENTION :
                                                 GL_LAST_VERTEX_CONVENTION);
}

void GPU_depth_range(float near, float far)
{
  /* glDepthRangef is only for OpenGL 4.1 or higher */
  glDepthRange(near, far);
}

void GPU_depth_test(bool enable)
{
  if (enable) {
    glEnable(GL_DEPTH_TEST);
  }
  else {
    glDisable(GL_DEPTH_TEST);
  }
}

bool GPU_depth_test_enabled()
{
  return glIsEnabled(GL_DEPTH_TEST);
}

void GPU_line_smooth(bool enable)
{
  if (enable && ((G.debug & G_DEBUG_GPU) == 0)) {
    glEnable(GL_LINE_SMOOTH);
  }
  else {
    glDisable(GL_LINE_SMOOTH);
  }
}

void GPU_line_width(float width)
{
  float max_size = GPU_max_line_width();
  float final_size = width * PIXELSIZE;
  /* Fix opengl errors on certain platform / drivers. */
  CLAMP(final_size, 1.0f, max_size);
  glLineWidth(final_size);
}

void GPU_point_size(float size)
{
  glPointSize(size * PIXELSIZE);
}

void GPU_polygon_smooth(bool enable)
{
  if (enable && ((G.debug & G_DEBUG_GPU) == 0)) {
    glEnable(GL_POLYGON_SMOOTH);
  }
  else {
    glDisable(GL_POLYGON_SMOOTH);
  }
}

/* Programmable point size
 * - shaders set their own point size when enabled
 * - use glPointSize when disabled */
void GPU_program_point_size(bool enable)
{
  if (enable) {
    glEnable(GL_PROGRAM_POINT_SIZE);
  }
  else {
    glDisable(GL_PROGRAM_POINT_SIZE);
  }
}

void GPU_scissor_test(bool enable)
{
  if (enable) {
    glEnable(GL_SCISSOR_TEST);
  }
  else {
    glDisable(GL_SCISSOR_TEST);
  }
}

void GPU_scissor(int x, int y, int width, int height)
{
  glScissor(x, y, width, height);
}

void GPU_viewport(int x, int y, int width, int height)
{
  glViewport(x, y, width, height);
}

void GPU_scissor_get_f(float coords[4])
{
  glGetFloatv(GL_SCISSOR_BOX, coords);
}

void GPU_scissor_get_i(int coords[4])
{
  glGetIntegerv(GL_SCISSOR_BOX, coords);
}

void GPU_viewport_size_get_f(float coords[4])
{
  glGetFloatv(GL_VIEWPORT, coords);
}

void GPU_viewport_size_get_i(int coords[4])
{
  glGetIntegerv(GL_VIEWPORT, coords);
}

void GPU_flush(void)
{
  glFlush();
}

void GPU_finish(void)
{
  glFinish();
}

void GPU_unpack_row_length_set(uint len)
{
  glPixelStorei(GL_UNPACK_ROW_LENGTH, len);
}

void GPU_logic_op_xor_set(bool enable)
{
  if (enable) {
    glLogicOp(GL_XOR);
    glEnable(GL_COLOR_LOGIC_OP);
  }
  else {
    glDisable(GL_COLOR_LOGIC_OP);
  }
}

void GPU_color_mask(bool r, bool g, bool b, bool a)
{
  glColorMask(r, g, b, a);
}

void GPU_depth_mask(bool depth)
{
  glDepthMask(depth);
}

bool GPU_depth_mask_get(void)
{
  GLint mask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &mask);
  return mask == GL_TRUE;
}

void GPU_stencil_mask(uint stencil)
{
  glStencilMask(stencil);
}

void GPU_clip_distances(int distances_new)
{
  static int distances_enabled = 0;
  for (int i = 0; i < distances_new; i++) {
    glEnable(GL_CLIP_DISTANCE0 + i);
  }
  for (int i = distances_new; i < distances_enabled; i++) {
    glDisable(GL_CLIP_DISTANCE0 + i);
  }
  distances_enabled = distances_new;
}

bool GPU_mipmap_enabled(void)
{
  /* TODO(fclem) this used to be a userdef option. */
  return true;
}

/** \name GPU Push/Pop State
 * \{ */

#define STATE_STACK_DEPTH 16

typedef struct {
  eGPUAttrMask mask;

  /* GL_BLEND_BIT */
  uint is_blend : 1;

  /* GL_DEPTH_BUFFER_BIT */
  uint is_depth_test : 1;
  int depth_func;
  double depth_clear_value;
  bool depth_write_mask;

  /* GL_SCISSOR_BIT */
  int scissor_box[4];
  uint is_scissor_test : 1;

  /* GL_VIEWPORT_BIT */
  int viewport[4];
  double near_far[2];
} GPUAttrValues;

typedef struct {
  GPUAttrValues attr_stack[STATE_STACK_DEPTH];
  uint top;
} GPUAttrStack;

static GPUAttrStack state = {
    {},
    0,
};

#define AttrStack state
#define Attr state.attr_stack[state.top]

/**
 * Replacement for glPush/PopAttributes
 *
 * We don't need to cover all the options of legacy OpenGL
 * but simply the ones used by Blender.
 */
void gpuPushAttr(eGPUAttrMask mask)
{
  Attr.mask = mask;

  if ((mask & GPU_DEPTH_BUFFER_BIT) != 0) {
    Attr.is_depth_test = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_DEPTH_FUNC, &Attr.depth_func);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &Attr.depth_clear_value);
    glGetBooleanv(GL_DEPTH_WRITEMASK, (GLboolean *)&Attr.depth_write_mask);
  }

  if ((mask & GPU_SCISSOR_BIT) != 0) {
    Attr.is_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, (GLint *)&Attr.scissor_box);
  }

  if ((mask & GPU_VIEWPORT_BIT) != 0) {
    glGetDoublev(GL_DEPTH_RANGE, (GLdouble *)&Attr.near_far);
    glGetIntegerv(GL_VIEWPORT, (GLint *)&Attr.viewport);
  }

  if ((mask & GPU_BLEND_BIT) != 0) {
    Attr.is_blend = glIsEnabled(GL_BLEND);
  }

  BLI_assert(AttrStack.top < STATE_STACK_DEPTH);
  AttrStack.top++;
}

static void restore_mask(GLenum cap, const bool value)
{
  if (value) {
    glEnable(cap);
  }
  else {
    glDisable(cap);
  }
}

void gpuPopAttr(void)
{
  BLI_assert(AttrStack.top > 0);
  AttrStack.top--;

  GLint mask = Attr.mask;

  if ((mask & GPU_DEPTH_BUFFER_BIT) != 0) {
    restore_mask(GL_DEPTH_TEST, Attr.is_depth_test);
    glDepthFunc(Attr.depth_func);
    glClearDepth(Attr.depth_clear_value);
    glDepthMask(Attr.depth_write_mask);
  }

  if ((mask & GPU_VIEWPORT_BIT) != 0) {
    glViewport(Attr.viewport[0], Attr.viewport[1], Attr.viewport[2], Attr.viewport[3]);
    glDepthRange(Attr.near_far[0], Attr.near_far[1]);
  }

  if ((mask & GPU_SCISSOR_BIT) != 0) {
    restore_mask(GL_SCISSOR_TEST, Attr.is_scissor_test);
    glScissor(Attr.scissor_box[0], Attr.scissor_box[1], Attr.scissor_box[2], Attr.scissor_box[3]);
  }

  if ((mask & GPU_BLEND_BIT) != 0) {
    restore_mask(GL_BLEND, Attr.is_blend);
  }
}

#undef Attr
#undef AttrStack

/* Default OpenGL State
 *
 * This is called on startup, for opengl offscreen render.
 * Generally we should always return to this state when
 * temporarily modifying the state for drawing, though that are (undocumented)
 * exceptions that we should try to get rid of. */

void GPU_state_init(void)
{
  GPU_program_point_size(false);

  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_COLOR_LOGIC_OP);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_DITHER);

  glDepthFunc(GL_LEQUAL);
  glDepthRange(0.0, 1.0);

  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  /* Is default but better be explicit. */
  glEnable(GL_MULTISAMPLE);

  /* This is a bit dangerous since addons could change this. */
  glEnable(GL_PRIMITIVE_RESTART);
  glPrimitiveRestartIndex((GLuint)0xFFFFFFFF);

  /* TODO: Should become default. But needs at least GL 4.3 */
  if (GLEW_ARB_ES3_compatibility) {
    /* Takes predecence over GL_PRIMITIVE_RESTART */
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
  }
}

/** \} */

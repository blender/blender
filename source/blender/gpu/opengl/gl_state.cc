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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup gpu
 */

#include "BLI_math_base.h"

#include "GPU_extensions.h"

#include "glew-mx.h"

#include "gl_state.hh"

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name GLStateManager
 * \{ */

GLStateManager::GLStateManager(void) : GPUStateManager()
{
  /* Set other states that never change. */
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_PRIMITIVE_RESTART);

  glDisable(GL_DITHER);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  glPrimitiveRestartIndex((GLuint)0xFFFFFFFF);
  /* TODO: Should become default. But needs at least GL 4.3 */
  if (GLEW_ARB_ES3_compatibility) {
    /* Takes predecence over GL_PRIMITIVE_RESTART */
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
  }

  /* Force update using default state. */
  current_ = ~state;
  current_mutable_ = ~mutable_state;
  set_state(state);
  set_mutable_state(mutable_state);
}

void GLStateManager::set_state(const GPUState &state)
{
  GPUState changed = state ^ current_;

  if (changed.blend != 0) {
    set_blend(state.blend);
  }
  if (changed.write_mask != 0) {
    set_write_mask(state.write_mask);
  }
  if (changed.depth_test != 0) {
    set_depth_test(state.depth_test);
  }
  if (changed.stencil_test != 0 || changed.stencil_op != 0) {
    set_stencil_test(state.stencil_test, state.stencil_op);
    set_stencil_mask(state.stencil_test, mutable_state);
  }
  if (changed.clip_distances != 0) {
    set_clip_distances(state.clip_distances, current_.clip_distances);
  }
  if (changed.culling_test != 0) {
    set_backface_culling(state.culling_test);
  }
  if (changed.logic_op_xor != 0) {
    set_logic_op(state.logic_op_xor);
  }
  if (changed.invert_facing != 0) {
    set_facing(state.invert_facing);
  }
  if (changed.provoking_vert != 0) {
    set_provoking_vert(state.provoking_vert);
  }
  if (changed.shadow_bias != 0) {
    set_shadow_bias(state.shadow_bias);
  }

  /* TODO remove */
  if (changed.polygon_smooth) {
    if (state.polygon_smooth) {
      glEnable(GL_POLYGON_SMOOTH);
    }
    else {
      glDisable(GL_POLYGON_SMOOTH);
    }
  }
  if (changed.line_smooth) {
    if (state.line_smooth) {
      glEnable(GL_LINE_SMOOTH);
    }
    else {
      glDisable(GL_LINE_SMOOTH);
    }
  }

  current_ = state;
}

void GLStateManager::set_mutable_state(const GPUStateMutable &state)
{
  GPUStateMutable changed = state ^ current_mutable_;

  if ((changed.viewport_rect[0] != 0) || (changed.viewport_rect[1] != 0) ||
      (changed.viewport_rect[2] != 0) || (changed.viewport_rect[3] != 0)) {
    glViewport(UNPACK4(state.viewport_rect));
  }

  if ((changed.scissor_rect[0] != 0) || (changed.scissor_rect[1] != 0) ||
      (changed.scissor_rect[2] != 0) || (changed.scissor_rect[3] != 0)) {
    if ((state.scissor_rect[2] > 0)) {
      glScissor(UNPACK4(state.scissor_rect));
      glEnable(GL_SCISSOR_TEST);
    }
    else {
      glDisable(GL_SCISSOR_TEST);
    }
  }

  /* TODO remove, should be uniform. */
  if (changed.point_size != 0) {
    if (state.point_size > 0.0f) {
      glEnable(GL_PROGRAM_POINT_SIZE);
      glPointSize(state.point_size);
    }
    else {
      glDisable(GL_PROGRAM_POINT_SIZE);
    }
  }

  if (changed.line_width != 0) {
    /* TODO remove, should use wide line shader. */
    glLineWidth(clamp_f(state.line_width, 1.0f, GPU_max_line_width()));
  }

  if (changed.depth_range[0] != 0 || changed.depth_range[1] != 0) {
    /* TODO remove, should modify the projection matrix instead. */
    glDepthRange(UNPACK2(state.depth_range));
  }

  if (changed.stencil_compare_mask != 0 || changed.stencil_reference != 0 ||
      changed.stencil_write_mask != 0) {
    set_stencil_mask(current_.stencil_test, state);
  }

  current_mutable_ = state;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State set functions
 * \{ */

void GLStateManager::set_write_mask(const eGPUWriteMask value)
{
  glDepthMask((value & GPU_WRITE_DEPTH) != 0);
  glColorMask((value & GPU_WRITE_RED) != 0,
              (value & GPU_WRITE_GREEN) != 0,
              (value & GPU_WRITE_BLUE) != 0,
              (value & GPU_WRITE_ALPHA) != 0);

  if (value == GPU_WRITE_NONE) {
    glEnable(GL_RASTERIZER_DISCARD);
  }
  else {
    glDisable(GL_RASTERIZER_DISCARD);
  }
}

void GLStateManager::set_depth_test(const eGPUDepthTest value)
{
  GLenum func;
  switch (value) {
    case GPU_DEPTH_LESS:
      func = GL_LESS;
      break;
    case GPU_DEPTH_LESS_EQUAL:
      func = GL_LEQUAL;
      break;
    case GPU_DEPTH_EQUAL:
      func = GL_EQUAL;
      break;
    case GPU_DEPTH_GREATER:
      func = GL_GREATER;
      break;
    case GPU_DEPTH_GREATER_EQUAL:
      func = GL_GEQUAL;
      break;
    case GPU_DEPTH_ALWAYS:
    default:
      func = GL_ALWAYS;
      break;
  }

  if (value != GPU_DEPTH_NONE) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(func);
  }
  else {
    glDisable(GL_DEPTH_TEST);
  }
}

void GLStateManager::set_stencil_test(const eGPUStencilTest test, const eGPUStencilOp operation)
{
  switch (operation) {
    case GPU_STENCIL_OP_REPLACE:
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
      break;
    case GPU_STENCIL_OP_NONE:
    default:
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  }

  if (test != GPU_STENCIL_NONE) {
    glEnable(GL_STENCIL_TEST);
  }
  else {
    glDisable(GL_STENCIL_TEST);
  }
}

void GLStateManager::set_stencil_mask(const eGPUStencilTest test, const GPUStateMutable state)
{
  GLenum func;
  switch (test) {
    case GPU_STENCIL_NEQUAL:
      func = GL_NOTEQUAL;
      break;
    case GPU_STENCIL_EQUAL:
      func = GL_EQUAL;
      break;
    case GPU_STENCIL_ALWAYS:
      func = GL_ALWAYS;
      break;
    case GPU_STENCIL_NONE:
    default:
      glStencilMask(0x00);
      glStencilFunc(GL_ALWAYS, 0x00, 0x00);
      return;
  }

  glStencilMask(state.stencil_write_mask);
  glStencilFunc(func, state.stencil_reference, state.stencil_compare_mask);
}

void GLStateManager::set_clip_distances(const int new_dist_len, const int old_dist_len)
{
  for (int i = 0; i < new_dist_len; i++) {
    glEnable(GL_CLIP_DISTANCE0 + i);
  }
  for (int i = new_dist_len; i < old_dist_len; i++) {
    glDisable(GL_CLIP_DISTANCE0 + i);
  }
}

void GLStateManager::set_logic_op(const bool enable)
{
  if (enable) {
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);
  }
  else {
    glDisable(GL_COLOR_LOGIC_OP);
  }
}

void GLStateManager::set_facing(const bool invert)
{
  glFrontFace((invert) ? GL_CW : GL_CCW);
}

void GLStateManager::set_backface_culling(const eGPUFaceCullTest test)
{
  if (test != GPU_CULL_NONE) {
    glEnable(GL_CULL_FACE);
    glCullFace((test == GPU_CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
  else {
    glDisable(GL_CULL_FACE);
  }
}

void GLStateManager::set_provoking_vert(const eGPUProvokingVertex vert)
{
  GLenum value = (vert == GPU_VERTEX_FIRST) ? GL_FIRST_VERTEX_CONVENTION :
                                              GL_LAST_VERTEX_CONVENTION;
  glProvokingVertex(value);
}

void GLStateManager::set_shadow_bias(const bool enable)
{
  if (enable) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_POLYGON_OFFSET_LINE);
    /* 2.0 Seems to be the lowest possible slope bias that works in every case. */
    glPolygonOffset(2.0f, 1.0f);
  }
  else {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
  }
}

void GLStateManager::set_blend(const eGPUBlend value)
{
  /**
   * Factors to the equation.
   * SRC is fragment shader output.
   * DST is framebuffer color.
   * final.rgb = SRC.rgb * src_rgb + DST.rgb * dst_rgb;
   * final.a = SRC.a * src_alpha + DST.a * dst_alpha;
   **/
  GLenum src_rgb, src_alpha, dst_rgb, dst_alpha;
  switch (value) {
    default:
    case GPU_BLEND_ALPHA: {
      src_rgb = GL_SRC_ALPHA;
      dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
      src_alpha = GL_ONE;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ALPHA_PREMULT: {
      src_rgb = GL_ONE;
      dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
      src_alpha = GL_ONE;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ADDITIVE: {
      /* Do not let alpha accumulate but premult the source RGB by it. */
      src_rgb = GL_SRC_ALPHA;
      dst_rgb = GL_ONE;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE;
      break;
    }
    case GPU_BLEND_SUBTRACT:
    case GPU_BLEND_ADDITIVE_PREMULT: {
      /* Let alpha accumulate. */
      src_rgb = GL_ONE;
      dst_rgb = GL_ONE;
      src_alpha = GL_ONE;
      dst_alpha = GL_ONE;
      break;
    }
    case GPU_BLEND_MULTIPLY: {
      src_rgb = GL_DST_COLOR;
      dst_rgb = GL_ZERO;
      src_alpha = GL_DST_ALPHA;
      dst_alpha = GL_ZERO;
      break;
    }
    case GPU_BLEND_INVERT: {
      src_rgb = GL_ONE_MINUS_DST_COLOR;
      dst_rgb = GL_ZERO;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE;
      break;
    }
    case GPU_BLEND_OIT: {
      src_rgb = GL_ONE;
      dst_rgb = GL_ONE;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_BACKGROUND: {
      src_rgb = GL_ONE_MINUS_DST_ALPHA;
      dst_rgb = GL_SRC_ALPHA;
      src_alpha = GL_ZERO;
      dst_alpha = GL_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_CUSTOM: {
      src_rgb = GL_ONE;
      dst_rgb = GL_SRC1_COLOR;
      src_alpha = GL_ONE;
      dst_alpha = GL_SRC1_ALPHA;
      break;
    }
  }

  if (value != GPU_BLEND_NONE) {
    glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
    glEnable(GL_BLEND);
  }
  else {
    glDisable(GL_BLEND);
  }
}

/** \} */

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

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_state.h"

#include "gpu_context_private.hh"

#include "gpu_state_private.hh"

using namespace blender::gpu;

#define SET_STATE(_prefix, _state, _value) \
  do { \
    GPUStateManager *stack = GPU_context_active_get()->state_manager; \
    auto &state_object = stack->_prefix##state; \
    state_object._state = _value; \
  } while (0)

#define SET_IMMUTABLE_STATE(_state, _value) SET_STATE(, _state, _value)
#define SET_MUTABLE_STATE(_state, _value) SET_STATE(mutable_, _state, _value)

/* -------------------------------------------------------------------- */
/** \name Immutable state Setters
 * \{ */

void GPU_blend(eGPUBlend blend)
{
  SET_IMMUTABLE_STATE(blend, blend);
}

void GPU_face_culling(eGPUFaceCullTest culling)
{
  SET_IMMUTABLE_STATE(culling_test, culling);
}

void GPU_front_facing(bool invert)
{
  SET_IMMUTABLE_STATE(invert_facing, invert);
}

void GPU_provoking_vertex(eGPUProvokingVertex vert)
{
  SET_IMMUTABLE_STATE(provoking_vert, vert);
}

/* TODO explicit depth test. */
void GPU_depth_test(bool enable)
{
  SET_IMMUTABLE_STATE(depth_test, (enable) ? GPU_DEPTH_LESS_EQUAL : GPU_DEPTH_NONE);
}

void GPU_line_smooth(bool enable)
{
  SET_IMMUTABLE_STATE(line_smooth, enable);
}

void GPU_polygon_smooth(bool enable)
{
  SET_IMMUTABLE_STATE(polygon_smooth, enable);
}

void GPU_logic_op_xor_set(bool enable)
{
  SET_IMMUTABLE_STATE(logic_op_xor, enable);
}

void GPU_write_mask(eGPUWriteMask mask)
{
  SET_IMMUTABLE_STATE(write_mask, mask);
}

void GPU_color_mask(bool r, bool g, bool b, bool a)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->state;
  eGPUWriteMask write_mask = state.write_mask;
  SET_FLAG_FROM_TEST(write_mask, r, GPU_WRITE_RED);
  SET_FLAG_FROM_TEST(write_mask, g, GPU_WRITE_GREEN);
  SET_FLAG_FROM_TEST(write_mask, b, GPU_WRITE_BLUE);
  SET_FLAG_FROM_TEST(write_mask, a, GPU_WRITE_ALPHA);
  state.write_mask = write_mask;
}

void GPU_depth_mask(bool depth)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->state;
  eGPUWriteMask write_mask = state.write_mask;
  SET_FLAG_FROM_TEST(write_mask, depth, GPU_WRITE_DEPTH);
  state.write_mask = write_mask;
}

void GPU_clip_distances(int distances_enabled)
{
  SET_IMMUTABLE_STATE(clip_distances, distances_enabled);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mutable State Setters
 * \{ */

void GPU_depth_range(float near, float far)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->mutable_state;
  copy_v2_fl2(state.depth_range, near, far);
}

void GPU_line_width(float width)
{
  SET_MUTABLE_STATE(line_width, width * PIXELSIZE);
}

void GPU_point_size(float size)
{
  SET_MUTABLE_STATE(point_size, size * PIXELSIZE);
}

/* Programmable point size
 * - shaders set their own point size when enabled
 * - use glPointSize when disabled */
/* TODO remove and use program point size everywhere */
void GPU_program_point_size(bool enable)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->mutable_state;
  /* Set point size sign negative to disable. */
  state.point_size = fabsf(state.point_size) * (enable ? 1 : -1);
}

void GPU_scissor_test(bool enable)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->mutable_state;
  /* Set point size sign negative to disable. */
  state.scissor_rect[2] = abs(state.scissor_rect[2]) * (enable ? 1 : -1);
}

void GPU_scissor(int x, int y, int width, int height)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->mutable_state;
  int scissor_rect[4] = {x, y, width, height};
  copy_v4_v4_int(state.scissor_rect, scissor_rect);
}

void GPU_viewport(int x, int y, int width, int height)
{
  GPUStateManager *stack = GPU_context_active_get()->state_manager;
  auto &state = stack->mutable_state;
  int viewport_rect[4] = {x, y, width, height};
  copy_v4_v4_int(state.viewport_rect, viewport_rect);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State Getters
 * \{ */

eGPUBlend GPU_blend_get()
{
  GPUState &state = GPU_context_active_get()->state_manager->state;
  return state.blend;
}

eGPUWriteMask GPU_write_mask_get()
{
  GPUState &state = GPU_context_active_get()->state_manager->state;
  return state.write_mask;
}

bool GPU_depth_test_enabled()
{
  GPUState &state = GPU_context_active_get()->state_manager->state;
  return state.depth_test != GPU_DEPTH_NONE;
}

void GPU_scissor_get(int coords[4])
{
  GPUStateMutable &state = GPU_context_active_get()->state_manager->mutable_state;
  copy_v4_v4_int(coords, state.scissor_rect);
}

void GPU_viewport_size_get_f(float coords[4])
{
  GPUStateMutable &state = GPU_context_active_get()->state_manager->mutable_state;
  for (int i = 0; i < 4; i++) {
    coords[i] = state.viewport_rect[i];
  }
}

void GPU_viewport_size_get_i(int coords[4])
{
  GPUStateMutable &state = GPU_context_active_get()->state_manager->mutable_state;
  copy_v4_v4_int(coords, state.viewport_rect);
}

bool GPU_depth_mask_get(void)
{
  GPUState &state = GPU_context_active_get()->state_manager->state;
  return (state.write_mask & GPU_WRITE_DEPTH) != 0;
}

bool GPU_mipmap_enabled(void)
{
  /* TODO(fclem) this used to be a userdef option. */
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Utils
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default OpenGL State
 *
 * This is called on startup, for opengl offscreen render.
 * Generally we should always return to this state when
 * temporarily modifying the state for drawing, though that are (undocumented)
 * exceptions that we should try to get rid of.
 * \{ */

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

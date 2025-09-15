/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "GPU_state.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"

#include "gpu_state_private.hh"

using namespace blender::gpu;

#define SET_STATE(_prefix, _state, _value) \
  do { \
    StateManager *stack = Context::get()->state_manager; \
    auto &state_object = stack->_prefix##state; \
    state_object._state = (_value); \
  } while (0)

#define SET_IMMUTABLE_STATE(_state, _value) SET_STATE(, _state, _value)
#define SET_MUTABLE_STATE(_state, _value) SET_STATE(mutable_, _state, _value)

/* -------------------------------------------------------------------- */
/** \name Immutable state Setters
 * \{ */

void GPU_blend(GPUBlend blend)
{
  SET_IMMUTABLE_STATE(blend, blend);
}

void GPU_face_culling(GPUFaceCullTest culling)
{
  SET_IMMUTABLE_STATE(culling_test, culling);
}

GPUFaceCullTest GPU_face_culling_get()
{
  GPUState &state = Context::get()->state_manager->state;
  return (GPUFaceCullTest)state.culling_test;
}

void GPU_front_facing(bool invert)
{
  SET_IMMUTABLE_STATE(invert_facing, invert);
}

void GPU_provoking_vertex(GPUProvokingVertex vert)
{
  SET_IMMUTABLE_STATE(provoking_vert, vert);
}

void GPU_depth_test(GPUDepthTest test)
{
  SET_IMMUTABLE_STATE(depth_test, test);
}

void GPU_stencil_test(GPUStencilTest test)
{
  SET_IMMUTABLE_STATE(stencil_test, test);
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

void GPU_write_mask(GPUWriteMask mask)
{
  SET_IMMUTABLE_STATE(write_mask, mask);
}

void GPU_color_mask(bool r, bool g, bool b, bool a)
{
  StateManager *stack = Context::get()->state_manager;
  auto &state = stack->state;
  uint32_t write_mask = state.write_mask;
  SET_FLAG_FROM_TEST(write_mask, r, uint32_t(GPU_WRITE_RED));
  SET_FLAG_FROM_TEST(write_mask, g, uint32_t(GPU_WRITE_GREEN));
  SET_FLAG_FROM_TEST(write_mask, b, uint32_t(GPU_WRITE_BLUE));
  SET_FLAG_FROM_TEST(write_mask, a, uint32_t(GPU_WRITE_ALPHA));
  state.write_mask = write_mask;
}

void GPU_depth_mask(bool depth)
{
  StateManager *stack = Context::get()->state_manager;
  auto &state = stack->state;
  uint32_t write_mask = state.write_mask;
  SET_FLAG_FROM_TEST(write_mask, depth, uint32_t(GPU_WRITE_DEPTH));
  state.write_mask = write_mask;
}

void GPU_shadow_offset(bool enable)
{
  SET_IMMUTABLE_STATE(shadow_bias, enable);
}

void GPU_clip_distances(int distances_enabled)
{
  SET_IMMUTABLE_STATE(clip_distances, distances_enabled);
}

void GPU_state_set(GPUWriteMask write_mask,
                   GPUBlend blend,
                   GPUFaceCullTest culling_test,
                   GPUDepthTest depth_test,
                   GPUStencilTest stencil_test,
                   GPUStencilOp stencil_op,
                   GPUProvokingVertex provoking_vert)
{
  StateManager *stack = Context::get()->state_manager;
  auto &state = stack->state;
  state.write_mask = uint32_t(write_mask);
  state.blend = uint32_t(blend);
  state.culling_test = uint32_t(culling_test);
  state.depth_test = uint32_t(depth_test);
  state.stencil_test = uint32_t(stencil_test);
  state.stencil_op = uint32_t(stencil_op);
  state.provoking_vert = uint32_t(provoking_vert);
}

void GPU_clip_control_unit_range(bool enable)
{
  SET_IMMUTABLE_STATE(clip_control, enable);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mutable State Setters
 * \{ */

void GPU_depth_range(float near, float far)
{
  StateManager *stack = Context::get()->state_manager;
  auto &state = stack->mutable_state;
  copy_v2_fl2(state.depth_range, near, far);
}

void GPU_line_width(float width)
{
  width = max_ff(1.0f, width * PIXELSIZE);
  SET_MUTABLE_STATE(line_width, width);
}

void GPU_point_size(float size)
{
  StateManager *stack = Context::get()->state_manager;
  auto &state = stack->mutable_state;
  /* Keep the sign of point_size since it represents the enable state. */
  state.point_size = size * ((state.point_size > 0.0) ? 1.0f : -1.0f);
}

void GPU_program_point_size(bool enable)
{
  StateManager *stack = Context::get()->state_manager;
  auto &state = stack->mutable_state;
  /* Set point size sign negative to disable. */
  state.point_size = fabsf(state.point_size) * (enable ? 1 : -1);
}

void GPU_scissor_test(bool enable)
{
  Context::get()->active_fb->scissor_test_set(enable);
}

void GPU_scissor(int x, int y, int width, int height)
{
  int scissor_rect[4] = {x, y, width, height};
  Context::get()->active_fb->scissor_set(scissor_rect);
}

void GPU_viewport(int x, int y, int width, int height)
{
  int viewport_rect[4] = {x, y, width, height};
  Context::get()->active_fb->viewport_set(viewport_rect);
}

void GPU_stencil_reference_set(uint reference)
{
  SET_MUTABLE_STATE(stencil_reference, uint8_t(reference));
}

void GPU_stencil_write_mask_set(uint write_mask)
{
  SET_MUTABLE_STATE(stencil_write_mask, uint8_t(write_mask));
}

void GPU_stencil_compare_mask_set(uint compare_mask)
{
  SET_MUTABLE_STATE(stencil_compare_mask, uint8_t(compare_mask));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State Getters
 * \{ */

GPUBlend GPU_blend_get()
{
  GPUState &state = Context::get()->state_manager->state;
  return (GPUBlend)state.blend;
}

GPUWriteMask GPU_write_mask_get()
{
  GPUState &state = Context::get()->state_manager->state;
  return (GPUWriteMask)state.write_mask;
}

uint GPU_stencil_mask_get()
{
  const GPUStateMutable &state = Context::get()->state_manager->mutable_state;
  return state.stencil_write_mask;
}

GPUDepthTest GPU_depth_test_get()
{
  GPUState &state = Context::get()->state_manager->state;
  return (GPUDepthTest)state.depth_test;
}

GPUStencilTest GPU_stencil_test_get()
{
  GPUState &state = Context::get()->state_manager->state;
  return (GPUStencilTest)state.stencil_test;
}

float GPU_line_width_get()
{
  const GPUStateMutable &state = Context::get()->state_manager->mutable_state;
  return state.line_width;
}

bool GPU_line_smooth_get()
{
  const GPUState &state = Context::get()->state_manager->state;
  return bool(state.line_smooth);
}

void GPU_scissor_get(int coords[4])
{
  Context::get()->active_fb->scissor_get(coords);
}

void GPU_viewport_size_get_f(float coords[4])
{
  int viewport[4];
  Context::get()->active_fb->viewport_get(viewport);
  for (int i = 0; i < 4; i++) {
    coords[i] = viewport[i];
  }
}

void GPU_viewport_size_get_i(int coords[4])
{
  Context::get()->active_fb->viewport_get(coords);
}

bool GPU_depth_mask_get()
{
  const GPUState &state = Context::get()->state_manager->state;
  return (state.write_mask & GPU_WRITE_DEPTH) != 0;
}

bool GPU_mipmap_enabled()
{
  /* TODO(fclem): this used to be a userdef option. */
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Utils
 * \{ */

void GPU_flush()
{
  Context::get()->flush();
}

void GPU_finish()
{
  Context::get()->finish();
}

void GPU_apply_state()
{
  Context::get()->state_manager->apply_state();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Synchronization Utils
 * \{ */

void GPU_memory_barrier(GPUBarrier barrier)
{
  Context::get()->state_manager->issue_barrier(barrier);
}

GPUFence *GPU_fence_create()
{
  Fence *fence = GPUBackend::get()->fence_alloc();
  return wrap(fence);
}

void GPU_fence_free(GPUFence *fence)
{
  delete unwrap(fence);
}

void GPU_fence_signal(GPUFence *fence)
{
  unwrap(fence)->signal();
}

void GPU_fence_wait(GPUFence *fence)
{
  unwrap(fence)->wait();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default State
 * \{ */

StateManager::StateManager()
{
  /* Set default state. */
  state.write_mask = GPU_WRITE_COLOR;
  state.blend = GPU_BLEND_NONE;
  state.culling_test = GPU_CULL_NONE;
  state.depth_test = GPU_DEPTH_NONE;
  state.stencil_test = GPU_STENCIL_NONE;
  state.stencil_op = GPU_STENCIL_OP_NONE;
  state.provoking_vert = GPU_VERTEX_LAST;
  state.logic_op_xor = false;
  state.invert_facing = false;
  state.shadow_bias = false;
  state.clip_distances = 0;
  state.clip_control = false;
  state.polygon_smooth = false;
  state.line_smooth = false;

  mutable_state.depth_range[0] = 0.0f;
  mutable_state.depth_range[1] = 1.0f;
  mutable_state.point_size = -1.0f; /* Negative is not using point size. */
  mutable_state.line_width = 1.0f;
  mutable_state.stencil_write_mask = 0x00;
  mutable_state.stencil_compare_mask = 0x00;
  mutable_state.stencil_reference = 0x00;

  image_formats.fill(TextureWriteFormat::Invalid);
}

/** \} */

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#include "BKE_global.h"
#include "GPU_compute.h"

#include "draw_debug.hh"
#include "draw_defines.h"
#include "draw_manager.h"
#include "draw_manager.hh"
#include "draw_pass.hh"
#include "draw_shader.h"

namespace blender::draw {

Manager::~Manager()
{
  for (GPUTexture *texture : acquired_textures) {
    /* Decrease refcount and free if 0. */
    GPU_texture_free(texture);
  }
}

void Manager::begin_sync()
{
  /* TODO: This means the reference is kept until further redraw or manager tear-down. Instead,
   * they should be released after each draw loop. But for now, mimics old DRW behavior. */
  for (GPUTexture *texture : acquired_textures) {
    /* Decrease refcount and free if 0. */
    GPU_texture_free(texture);
  }

  acquired_textures.clear();

#ifdef DEBUG
  /* Detect uninitialized data. */
  memset(matrix_buf.data(), 0xF0, resource_len_ * sizeof(*matrix_buf.data()));
  memset(bounds_buf.data(), 0xF0, resource_len_ * sizeof(*bounds_buf.data()));
  memset(infos_buf.data(), 0xF0, resource_len_ * sizeof(*infos_buf.data()));
#endif
  resource_len_ = 0;
  attribute_len_ = 0;
  /* TODO(fclem): Resize buffers if too big, but with an hysteresis threshold. */

  object_active = DST.draw_ctx.obact;

  /* Init the 0 resource. */
  resource_handle(float4x4::identity());
}

void Manager::end_sync()
{
  GPU_debug_group_begin("Manager.end_sync");

  matrix_buf.push_update();
  bounds_buf.push_update();
  infos_buf.push_update();
  attributes_buf.push_update();
  attributes_buf_legacy.push_update();

  /* Useful for debugging the following resource finalize. But will trigger the drawing of the GPU
   * debug draw/print buffers for every frame. Not nice for performance. */
  // debug_bind();

  /* Dispatch compute to finalize the resources on GPU. Save a bit of CPU time. */
  uint thread_groups = divide_ceil_u(resource_len_, DRW_FINALIZE_GROUP_SIZE);
  GPUShader *shader = DRW_shader_draw_resource_finalize_get();
  GPU_shader_bind(shader);
  GPU_shader_uniform_1i(shader, "resource_len", resource_len_);
  GPU_storagebuf_bind(matrix_buf, GPU_shader_get_ssbo(shader, "matrix_buf"));
  GPU_storagebuf_bind(bounds_buf, GPU_shader_get_ssbo(shader, "bounds_buf"));
  GPU_storagebuf_bind(infos_buf, GPU_shader_get_ssbo(shader, "infos_buf"));
  GPU_compute_dispatch(shader, thread_groups, 1, 1);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);

  GPU_debug_group_end();
}

void Manager::debug_bind()
{
#ifdef DEBUG
  if (DST.debug == nullptr) {
    return;
  }
  GPU_storagebuf_bind(drw_debug_gpu_draw_buf_get(), DRW_DEBUG_DRAW_SLOT);
  GPU_storagebuf_bind(drw_debug_gpu_print_buf_get(), DRW_DEBUG_PRINT_SLOT);
#  ifndef DISABLE_DEBUG_SHADER_PRINT_BARRIER
  /* Add a barrier to allow multiple shader writing to the same buffer. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
#  endif
#endif
}

void Manager::resource_bind()
{
  GPU_storagebuf_bind(matrix_buf, DRW_OBJ_MAT_SLOT);
  GPU_storagebuf_bind(infos_buf, DRW_OBJ_INFOS_SLOT);
  GPU_storagebuf_bind(attributes_buf, DRW_OBJ_ATTR_SLOT);
  /* 2 is the hardcoded location of the uniform attr UBO. */
  /* TODO(@fclem): Remove this workaround. */
  GPU_uniformbuf_bind(attributes_buf_legacy, 2);
}

void Manager::submit(PassSimple &pass, View &view)
{
  view.bind();

  debug_bind();

  command::RecordingState state;
  state.inverted_view = view.is_inverted();

  pass.draw_commands_buf_.bind(state, pass.headers_, pass.commands_);

  resource_bind();

  pass.submit(state);

  state.cleanup();
}

void Manager::submit(PassMain &pass, View &view)
{
  view.bind();

  debug_bind();

  bool freeze_culling = (U.experimental.use_viewport_debug && DST.draw_ctx.v3d &&
                         (DST.draw_ctx.v3d->debug_flag & V3D_DEBUG_FREEZE_CULLING) != 0);

  view.compute_visibility(bounds_buf, resource_len_, freeze_culling);

  command::RecordingState state;
  state.inverted_view = view.is_inverted();

  pass.draw_commands_buf_.bind(state, pass.headers_, pass.commands_, view.visibility_buf_);

  resource_bind();

  pass.submit(state);

  state.cleanup();
}

void Manager::submit(PassSortable &pass, View &view)
{
  pass.sort();

  this->submit(static_cast<PassMain &>(pass), view);
}

void Manager::submit(PassSimple &pass)
{
  debug_bind();

  command::RecordingState state;

  pass.draw_commands_buf_.bind(state, pass.headers_, pass.commands_);

  resource_bind();

  pass.submit(state);

  state.cleanup();
}

Manager::SubmitDebugOutput Manager::submit_debug(PassSimple &pass, View &view)
{
  submit(pass, view);

  pass.draw_commands_buf_.resource_id_buf_.read();

  Manager::SubmitDebugOutput output;
  output.resource_id = {pass.draw_commands_buf_.resource_id_buf_.data(),
                        pass.draw_commands_buf_.resource_id_count_};
  /* There is no visibility data for PassSimple. */
  output.visibility = {(uint *)view.visibility_buf_.data(), 0};
  return output;
}

Manager::SubmitDebugOutput Manager::submit_debug(PassMain &pass, View &view)
{
  submit(pass, view);

  GPU_finish();

  pass.draw_commands_buf_.resource_id_buf_.read();
  view.visibility_buf_.read();

  Manager::SubmitDebugOutput output;
  output.resource_id = {pass.draw_commands_buf_.resource_id_buf_.data(),
                        pass.draw_commands_buf_.resource_id_count_};
  output.visibility = {(uint *)view.visibility_buf_.data(), divide_ceil_u(resource_len_, 32)};
  return output;
}

Manager::DataDebugOutput Manager::data_debug()
{
  matrix_buf.read();
  bounds_buf.read();
  infos_buf.read();

  Manager::DataDebugOutput output;
  output.matrices = {matrix_buf.data(), resource_len_};
  output.bounds = {bounds_buf.data(), resource_len_};
  output.infos = {infos_buf.data(), resource_len_};
  return output;
}

}  // namespace blender::draw

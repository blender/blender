/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"

#include "BLI_math_base.h"
#include "GPU_compute.hh"

#include "draw_defines.hh"
#include "draw_manager.hh"
#include "draw_manager_c.hh"
#include "draw_pass.hh"
#include "draw_shader.hh"

namespace blender::draw {

std::atomic<uint32_t> Manager::global_sync_counter_ = 1;

Manager::~Manager()
{
  for (GPUTexture *texture : acquired_textures) {
    /* Decrease refcount and free if 0. */
    GPU_texture_free(texture);
  }
}

void Manager::begin_sync()
{
  /* Add 2 to always have a non-null number even in case of overflow. */
  sync_counter_ = (global_sync_counter_ += 2);

  matrix_buf.swap();
  bounds_buf.swap();
  infos_buf.swap();

  matrix_buf.current().trim_to_next_power_of_2(resource_len_);
  bounds_buf.current().trim_to_next_power_of_2(resource_len_);
  infos_buf.current().trim_to_next_power_of_2(resource_len_);
  attributes_buf.trim_to_next_power_of_2(attribute_len_);

  /* TODO: This means the reference is kept until further redraw or manager tear-down. Instead,
   * they should be released after each draw loop. But for now, mimics old DRW behavior. */
  for (GPUTexture *texture : acquired_textures) {
    /* Decrease refcount and free if 0. */
    GPU_texture_free(texture);
  }

  acquired_textures.clear();
  layer_attributes.clear();

/* For some reason, if this uninitialized data pattern was enabled (ie release asserts enabled),
 * The viewport just gives up rendering objects on ARM64 devices. Possibly Mesa GLOn12-related. */
#if !defined(NDEBUG) && !defined(_M_ARM64)
  /* Detect uninitialized data. */
  memset(matrix_buf.current().data(),
         0xF0,
         matrix_buf.current().size() * sizeof(*matrix_buf.current().data()));
  memset(bounds_buf.current().data(),
         0xF0,
         matrix_buf.current().size() * sizeof(*bounds_buf.current().data()));
  memset(infos_buf.current().data(),
         0xF0,
         matrix_buf.current().size() * sizeof(*infos_buf.current().data()));
#endif
  resource_len_ = 0;
  attribute_len_ = 0;
  /* TODO(fclem): Resize buffers if too big, but with an hysteresis threshold. */

  object_active = DST.draw_ctx.obact;

  /* Init the 0 resource. */
  resource_handle(float4x4::identity());
}

void Manager::sync_layer_attributes()
{
  /* Sort the attribute IDs - the shaders use binary search. */
  Vector<uint32_t> id_list;

  id_list.reserve(layer_attributes.size());

  for (uint32_t id : layer_attributes.keys()) {
    id_list.append(id);
  }

  std::sort(id_list.begin(), id_list.end());

  /* Look up the attributes. */
  int count = 0, size = layer_attributes_buf.end() - layer_attributes_buf.begin();

  for (uint32_t id : id_list) {
    if (layer_attributes_buf[count].sync(
            DST.draw_ctx.scene, DST.draw_ctx.view_layer, layer_attributes.lookup(id)))
    {
      /* Check if the buffer is full. */
      if (++count == size) {
        break;
      }
    }
  }

  layer_attributes_buf[0].buffer_length = count;
}

void Manager::end_sync()
{
  GPU_debug_group_begin("Manager.end_sync");

  sync_layer_attributes();

  matrix_buf.current().push_update();
  bounds_buf.current().push_update();
  infos_buf.current().push_update();
  attributes_buf.push_update();
  layer_attributes_buf.push_update();
  attributes_buf_legacy.push_update();

  /* Useful for debugging the following resource finalize. But will trigger the drawing of the GPU
   * debug draw/print buffers for every frame. Not nice for performance. */
  // debug_bind();

  /* Dispatch compute to finalize the resources on GPU. Save a bit of CPU time. */
  uint thread_groups = divide_ceil_u(resource_len_, DRW_FINALIZE_GROUP_SIZE);
  GPUShader *shader = DRW_shader_draw_resource_finalize_get();
  GPU_shader_bind(shader);
  GPU_shader_uniform_1i(shader, "resource_len", resource_len_);
  GPU_storagebuf_bind(matrix_buf.current(), GPU_shader_get_ssbo_binding(shader, "matrix_buf"));
  GPU_storagebuf_bind(bounds_buf.current(), GPU_shader_get_ssbo_binding(shader, "bounds_buf"));
  GPU_storagebuf_bind(infos_buf.current(), GPU_shader_get_ssbo_binding(shader, "infos_buf"));
  GPU_compute_dispatch(shader, thread_groups, 1, 1);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);

  GPU_debug_group_end();
}

void Manager::debug_bind()
{
#ifdef WITH_DRAW_DEBUG
  if (DST.debug == nullptr) {
    return;
  }
  GPU_storagebuf_bind(drw_debug_gpu_draw_buf_get(), DRW_DEBUG_DRAW_SLOT);
#  ifndef DISABLE_DEBUG_SHADER_PRINT_BARRIER
  /* Add a barrier to allow multiple shader writing to the same buffer. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
#  endif
#endif
}

void Manager::resource_bind()
{
  GPU_storagebuf_bind(matrix_buf.current(), DRW_OBJ_MAT_SLOT);
  GPU_storagebuf_bind(infos_buf.current(), DRW_OBJ_INFOS_SLOT);
  GPU_storagebuf_bind(attributes_buf, DRW_OBJ_ATTR_SLOT);
  GPU_uniformbuf_bind(layer_attributes_buf, DRW_LAYER_ATTR_UBO_SLOT);
  /* 2 is the hardcoded location of the uniform attr UBO. */
  /* TODO(@fclem): Remove this workaround. */
  GPU_uniformbuf_bind(attributes_buf_legacy, 2);
}

uint64_t Manager::fingerprint_get()
{
  /* Covers new sync cycle, added resources and different #Manager. */
  return sync_counter_ | (uint64_t(resource_len_) << 32);
}

ResourceHandleRange Manager::resource_handle_for_sculpt(const ObjectRef &ref)
{
  /* TODO(fclem): Deduplicate with other engine. */
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(*ref.object);
  const blender::Bounds<float3> bounds = bke::pbvh::bounds_get(pbvh);
  const float3 center = math::midpoint(bounds.min, bounds.max);
  const float3 half_extent = bounds.max - center;
  return resource_handle(ref, nullptr, &center, &half_extent);
}

void Manager::compute_visibility(View &view)
{
  bool freeze_culling = (USER_EXPERIMENTAL_TEST(&U, use_viewport_debug) && DST.draw_ctx.v3d &&
                         (DST.draw_ctx.v3d->debug_flag & V3D_DEBUG_FREEZE_CULLING) != 0);

  BLI_assert_msg(view.manager_fingerprint_ != this->fingerprint_get(),
                 "Resources did not changed, no need to update");

  view.manager_fingerprint_ = this->fingerprint_get();

  view.bind();
  view.compute_visibility(
      bounds_buf.current(), infos_buf.current(), resource_len_, freeze_culling);
}

void Manager::ensure_visibility(View &view)
{
  if (view.manager_fingerprint_ != this->fingerprint_get()) {
    compute_visibility(view);
  }
}

void Manager::generate_commands(PassMain &pass, View &view)
{
  BLI_assert_msg((pass.manager_fingerprint_ != this->fingerprint_get()) ||
                     (pass.view_fingerprint_ != view.fingerprint_get()),
                 "Resources and view did not changed no need to update");
  BLI_assert_msg((view.manager_fingerprint_ == this->fingerprint_get()) &&
                     (view.fingerprint_get() != 0),
                 "Resources or view changed, but compute_visibility was not called");

  pass.manager_fingerprint_ = this->fingerprint_get();
  pass.view_fingerprint_ = view.fingerprint_get();

  pass.draw_commands_buf_.generate_commands(pass.headers_,
                                            pass.commands_,
                                            view.get_visibility_buffer(),
                                            view.visibility_word_per_draw(),
                                            view.view_len_,
                                            pass.use_custom_ids);
}

void Manager::generate_commands(PassSortable &pass, View &view)
{
  pass.sort();
  generate_commands(static_cast<PassMain &>(pass), view);
}

void Manager::generate_commands(PassSimple &pass)
{
  BLI_assert_msg(pass.manager_fingerprint_ != this->fingerprint_get(),
                 "Resources did not changed since last generate_command, no need to update");
  pass.manager_fingerprint_ = this->fingerprint_get();

  pass.draw_commands_buf_.generate_commands(pass.headers_, pass.commands_, pass.sub_passes_);
}

void Manager::submit_only(PassMain &pass, View &view)
{
  BLI_assert_msg(view.manager_fingerprint_ != 0, "compute_visibility was not called on this view");
  BLI_assert_msg(view.manager_fingerprint_ == this->fingerprint_get(),
                 "Resources changed since last compute_visibility");
  BLI_assert_msg(pass.manager_fingerprint_ != 0, "generate_command was not called on this pass");
  BLI_assert_msg(pass.manager_fingerprint_ == this->fingerprint_get(),
                 "Resources changed since last generate_command");
  /* The function generate_commands needs to be called for each view this pass is going to be
   * submitted with. This is because the commands are stored inside the pass and not per view. */
  BLI_assert_msg(pass.view_fingerprint_ == view.fingerprint_get(),
                 "View have changed since last generate_commands or "
                 "submitting with a different view");

  debug_bind();

  command::RecordingState state;
  state.inverted_view = view.is_inverted();

  view.bind();
  pass.draw_commands_buf_.bind(state);

  resource_bind();

  pass.submit(state);

  state.cleanup();
}

void Manager::submit(PassMain &pass, View &view)
{
  if (view.manager_fingerprint_ != this->fingerprint_get()) {
    compute_visibility(view);
  }

  if (pass.manager_fingerprint_ != this->fingerprint_get() ||
      pass.view_fingerprint_ != view.fingerprint_get())
  {
    generate_commands(pass, view);
  }

  this->submit_only(pass, view);
}

void Manager::submit(PassSortable &pass, View &view)
{
  pass.sort();

  this->submit(static_cast<PassMain &>(pass), view);
}

void Manager::submit(PassSimple &pass, bool inverted_view)
{
  if (!pass.has_generated_commands()) {
    generate_commands(pass);
  }

  debug_bind();

  command::RecordingState state;
  state.inverted_view = inverted_view;

  pass.draw_commands_buf_.bind(state);

  resource_bind();

  pass.submit(state);

  state.cleanup();
}

void Manager::submit(PassSimple &pass, View &view)
{
  debug_bind();

  view.bind();

  this->submit(pass, view.is_inverted());
}

Manager::SubmitDebugOutput Manager::submit_debug(PassSimple &pass, View &view)
{
  submit(pass, view);

  pass.draw_commands_buf_.resource_id_buf_.read();

  Manager::SubmitDebugOutput output;
  output.resource_id = {pass.draw_commands_buf_.resource_id_buf_.data(),
                        pass.draw_commands_buf_.resource_id_count_};
  /* There is no visibility data for PassSimple. */
  output.visibility = {(uint *)view.get_visibility_buffer().data(), 0};
  return output;
}

Manager::SubmitDebugOutput Manager::submit_debug(PassMain &pass, View &view)
{
  submit(pass, view);

  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);

  pass.draw_commands_buf_.resource_id_buf_.read();
  view.get_visibility_buffer().read();

  Manager::SubmitDebugOutput output;
  output.resource_id = {pass.draw_commands_buf_.resource_id_buf_.data(),
                        pass.draw_commands_buf_.resource_id_count_};
  output.visibility = {(uint *)view.get_visibility_buffer().data(),
                       divide_ceil_u(resource_len_, 32)};
  return output;
}

Manager::DataDebugOutput Manager::data_debug()
{
  matrix_buf.current().read();
  bounds_buf.current().read();
  infos_buf.current().read();

  Manager::DataDebugOutput output;
  output.matrices = {matrix_buf.current().data(), resource_len_};
  output.bounds = {bounds_buf.current().data(), resource_len_};
  output.infos = {infos_buf.current().data(), resource_len_};
  return output;
}

}  // namespace blender::draw

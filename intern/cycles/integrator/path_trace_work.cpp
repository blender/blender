/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device/device.h"

#include "integrator/path_trace_display.h"
#include "integrator/path_trace_work.h"
#include "integrator/path_trace_work_cpu.h"
#include "integrator/path_trace_work_gpu.h"
#include "render/buffers.h"
#include "render/film.h"
#include "render/scene.h"

#include "kernel/kernel_types.h"

CCL_NAMESPACE_BEGIN

unique_ptr<PathTraceWork> PathTraceWork::create(Device *device,
                                                Film *film,
                                                DeviceScene *device_scene,
                                                bool *cancel_requested_flag)
{
  if (device->info.type == DEVICE_CPU) {
    return make_unique<PathTraceWorkCPU>(device, film, device_scene, cancel_requested_flag);
  }

  return make_unique<PathTraceWorkGPU>(device, film, device_scene, cancel_requested_flag);
}

PathTraceWork::PathTraceWork(Device *device,
                             Film *film,
                             DeviceScene *device_scene,
                             bool *cancel_requested_flag)
    : device_(device),
      film_(film),
      device_scene_(device_scene),
      buffers_(make_unique<RenderBuffers>(device)),
      effective_buffer_params_(buffers_->params),
      cancel_requested_flag_(cancel_requested_flag)
{
}

PathTraceWork::~PathTraceWork()
{
}

RenderBuffers *PathTraceWork::get_render_buffers()
{
  return buffers_.get();
}

void PathTraceWork::set_effective_buffer_params(const BufferParams &effective_full_params,
                                                const BufferParams &effective_big_tile_params,
                                                const BufferParams &effective_buffer_params)
{
  effective_full_params_ = effective_full_params;
  effective_big_tile_params_ = effective_big_tile_params;
  effective_buffer_params_ = effective_buffer_params;
}

bool PathTraceWork::has_multiple_works() const
{
  /* Assume if there are multiple works working on the same big tile none of the works gets the
   * entire big tile to work on. */
  return !(effective_big_tile_params_.width == effective_buffer_params_.width &&
           effective_big_tile_params_.height == effective_buffer_params_.height &&
           effective_big_tile_params_.full_x == effective_buffer_params_.full_x &&
           effective_big_tile_params_.full_y == effective_buffer_params_.full_y);
}

void PathTraceWork::copy_to_render_buffers(RenderBuffers *render_buffers)
{
  copy_render_buffers_from_device();

  const int64_t width = effective_buffer_params_.width;
  const int64_t height = effective_buffer_params_.height;
  const int64_t pass_stride = effective_buffer_params_.pass_stride;
  const int64_t row_stride = width * pass_stride;
  const int64_t data_size = row_stride * height * sizeof(float);

  const int64_t offset_y = effective_buffer_params_.full_y - effective_big_tile_params_.full_y;
  const int64_t offset_in_floats = offset_y * row_stride;

  const float *src = buffers_->buffer.data();
  float *dst = render_buffers->buffer.data() + offset_in_floats;

  memcpy(dst, src, data_size);
}

void PathTraceWork::copy_from_render_buffers(const RenderBuffers *render_buffers)
{
  const int64_t width = effective_buffer_params_.width;
  const int64_t height = effective_buffer_params_.height;
  const int64_t pass_stride = effective_buffer_params_.pass_stride;
  const int64_t row_stride = width * pass_stride;
  const int64_t data_size = row_stride * height * sizeof(float);

  const int64_t offset_y = effective_buffer_params_.full_y - effective_big_tile_params_.full_y;
  const int64_t offset_in_floats = offset_y * row_stride;

  const float *src = render_buffers->buffer.data() + offset_in_floats;
  float *dst = buffers_->buffer.data();

  memcpy(dst, src, data_size);

  copy_render_buffers_to_device();
}

void PathTraceWork::copy_from_denoised_render_buffers(const RenderBuffers *render_buffers)
{
  const int64_t width = effective_buffer_params_.width;
  const int64_t offset_y = effective_buffer_params_.full_y - effective_big_tile_params_.full_y;
  const int64_t offset = offset_y * width;

  render_buffers_host_copy_denoised(
      buffers_.get(), effective_buffer_params_, render_buffers, effective_buffer_params_, offset);

  copy_render_buffers_to_device();
}

bool PathTraceWork::get_render_tile_pixels(const PassAccessor &pass_accessor,
                                           const PassAccessor::Destination &destination)
{
  const int offset_y = (effective_buffer_params_.full_y + effective_buffer_params_.window_y) -
                       (effective_big_tile_params_.full_y + effective_big_tile_params_.window_y);
  const int width = effective_buffer_params_.width;

  PassAccessor::Destination slice_destination = destination;
  slice_destination.offset += offset_y * width;

  return pass_accessor.get_render_tile_pixels(buffers_.get(), slice_destination);
}

bool PathTraceWork::set_render_tile_pixels(PassAccessor &pass_accessor,
                                           const PassAccessor::Source &source)
{
  const int offset_y = effective_buffer_params_.full_y - effective_big_tile_params_.full_y;
  const int width = effective_buffer_params_.width;

  PassAccessor::Source slice_source = source;
  slice_source.offset += offset_y * width;

  return pass_accessor.set_render_tile_pixels(buffers_.get(), slice_source);
}

PassAccessor::PassAccessInfo PathTraceWork::get_display_pass_access_info(PassMode pass_mode) const
{
  const KernelFilm &kfilm = device_scene_->data.film;
  const KernelBackground &kbackground = device_scene_->data.background;

  const BufferParams &params = buffers_->params;

  const BufferPass *display_pass = params.get_actual_display_pass(film_->get_display_pass());

  PassAccessor::PassAccessInfo pass_access_info;
  pass_access_info.type = display_pass->type;
  pass_access_info.offset = PASS_UNUSED;

  if (pass_mode == PassMode::DENOISED) {
    pass_access_info.mode = PassMode::DENOISED;
    pass_access_info.offset = params.get_pass_offset(pass_access_info.type, PassMode::DENOISED);
  }

  if (pass_access_info.offset == PASS_UNUSED) {
    pass_access_info.mode = PassMode::NOISY;
    pass_access_info.offset = params.get_pass_offset(pass_access_info.type);
  }

  pass_access_info.use_approximate_shadow_catcher = kfilm.use_approximate_shadow_catcher;
  pass_access_info.use_approximate_shadow_catcher_background =
      kfilm.use_approximate_shadow_catcher && !kbackground.transparent;

  pass_access_info.show_active_pixels = film_->get_show_active_pixels();

  return pass_access_info;
}

PassAccessor::Destination PathTraceWork::get_display_destination_template(
    const PathTraceDisplay *display) const
{
  PassAccessor::Destination destination(film_->get_display_pass());

  const int2 display_texture_size = display->get_texture_size();
  const int texture_x = effective_buffer_params_.full_x - effective_full_params_.full_x +
                        effective_buffer_params_.window_x;
  const int texture_y = effective_buffer_params_.full_y - effective_full_params_.full_y +
                        effective_buffer_params_.window_y;

  destination.offset = texture_y * display_texture_size.x + texture_x;
  destination.stride = display_texture_size.x;

  return destination;
}

CCL_NAMESPACE_END

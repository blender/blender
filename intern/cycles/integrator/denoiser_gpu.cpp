/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "integrator/denoiser_gpu.h"

#include "device/denoise.h"
#include "device/device.h"
#include "device/memory.h"
#include "device/queue.h"
#include "integrator/pass_accessor_gpu.h"
#include "session/buffers.h"
#include "util/log.h"
#include "util/progress.h"

CCL_NAMESPACE_BEGIN

DenoiserGPU::DenoiserGPU(Device *path_trace_device, const DenoiseParams &params)
    : Denoiser(path_trace_device, params)
{
}

DenoiserGPU::~DenoiserGPU()
{
  /* Explicit implementation, to allow forward declaration of Device in the header. */
}

bool DenoiserGPU::denoise_buffer(const BufferParams &buffer_params,
                                 RenderBuffers *render_buffers,
                                 const int num_samples,
                                 bool allow_inplace_modification)
{
  Device *denoiser_device = get_denoiser_device();
  if (!denoiser_device) {
    return false;
  }

  DenoiseTask task;
  task.params = params_;
  task.num_samples = num_samples;
  task.buffer_params = buffer_params;
  task.allow_inplace_modification = allow_inplace_modification;

  RenderBuffers local_render_buffers(denoiser_device);
  bool local_buffer_used = false;

  if (denoiser_device == render_buffers->buffer.device) {
    /* The device can access an existing buffer pointer. */
    local_buffer_used = false;
    task.render_buffers = render_buffers;
  }
  else {
    VLOG_WORK << "Creating temporary buffer on denoiser device.";

    /* Create buffer which is available by the device used by denoiser. */

    /* TODO(sergey): Optimize data transfers. For example, only copy denoising related passes,
     * ignoring other light ad data passes. */

    local_buffer_used = true;

    render_buffers->copy_from_device();

    local_render_buffers.reset(buffer_params);

    /* NOTE: The local buffer is allocated for an exact size of the effective render size, while
     * the input render buffer is allocated for the lowest resolution divider possible. So it is
     * important to only copy actually needed part of the input buffer. */
    memcpy(local_render_buffers.buffer.data(),
           render_buffers->buffer.data(),
           sizeof(float) * local_render_buffers.buffer.size());

    denoiser_queue_->copy_to_device(local_render_buffers.buffer);

    task.render_buffers = &local_render_buffers;
    task.allow_inplace_modification = true;
  }

  const bool denoise_result = denoise_buffer(task);

  if (local_buffer_used) {
    local_render_buffers.copy_from_device();

    render_buffers_host_copy_denoised(
        render_buffers, buffer_params, &local_render_buffers, local_render_buffers.params);

    render_buffers->copy_to_device();
  }

  return denoise_result;
}

Device *DenoiserGPU::ensure_denoiser_device(Progress *progress)
{
  Device *denoiser_device = Denoiser::ensure_denoiser_device(progress);
  if (!denoiser_device) {
    return nullptr;
  }

  if (!denoiser_queue_) {
    denoiser_queue_ = denoiser_device->gpu_queue_create();
    if (!denoiser_queue_) {
      return nullptr;
    }
  }

  return denoiser_device;
}

DenoiserGPU::DenoiseContext::DenoiseContext(Device *device, const DenoiseTask &task)
    : denoise_params(task.params),
      render_buffers(task.render_buffers),
      buffer_params(task.buffer_params),
      guiding_buffer(device, "denoiser guiding passes buffer", true),
      num_samples(task.num_samples)
{
  num_input_passes = 1;
  if (denoise_params.use_pass_albedo) {
    num_input_passes += 1;
    use_pass_albedo = true;
    pass_denoising_albedo = buffer_params.get_pass_offset(PASS_DENOISING_ALBEDO);
    if (denoise_params.use_pass_normal) {
      num_input_passes += 1;
      use_pass_normal = true;
      pass_denoising_normal = buffer_params.get_pass_offset(PASS_DENOISING_NORMAL);
    }
  }

  if (denoise_params.temporally_stable) {
    prev_output.device_pointer = render_buffers->buffer.device_pointer;

    prev_output.offset = buffer_params.get_pass_offset(PASS_DENOISING_PREVIOUS);

    prev_output.stride = buffer_params.stride;
    prev_output.pass_stride = buffer_params.pass_stride;

    num_input_passes += 1;
    use_pass_motion = true;
    pass_motion = buffer_params.get_pass_offset(PASS_MOTION);
  }

  use_guiding_passes = (num_input_passes - 1) > 0;

  if (use_guiding_passes) {
    if (task.allow_inplace_modification) {
      guiding_params.device_pointer = render_buffers->buffer.device_pointer;

      guiding_params.pass_albedo = pass_denoising_albedo;
      guiding_params.pass_normal = pass_denoising_normal;
      guiding_params.pass_flow = pass_motion;

      guiding_params.stride = buffer_params.stride;
      guiding_params.pass_stride = buffer_params.pass_stride;
    }
    else {
      guiding_params.pass_stride = 0;
      if (use_pass_albedo) {
        guiding_params.pass_albedo = guiding_params.pass_stride;
        guiding_params.pass_stride += 3;
      }
      if (use_pass_normal) {
        guiding_params.pass_normal = guiding_params.pass_stride;
        guiding_params.pass_stride += 3;
      }
      if (use_pass_motion) {
        guiding_params.pass_flow = guiding_params.pass_stride;
        guiding_params.pass_stride += 2;
      }

      guiding_params.stride = buffer_params.width;

      guiding_buffer.alloc_to_device(buffer_params.width * buffer_params.height *
                                     guiding_params.pass_stride);
      guiding_params.device_pointer = guiding_buffer.device_pointer;
    }
  }

  pass_sample_count = buffer_params.get_pass_offset(PASS_SAMPLE_COUNT);
}

bool DenoiserGPU::denoise_filter_color_postprocess(const DenoiseContext &context,
                                                   const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &context.num_samples,
                             &pass.noisy_offset,
                             &pass.denoised_offset,
                             &context.pass_sample_count,
                             &pass.num_components,
                             &pass.use_compositing);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS, work_size, args);
}

bool DenoiserGPU::denoise_filter_color_preprocess(const DenoiseContext &context,
                                                  const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &pass.denoised_offset);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_PREPROCESS, work_size, args);
}

bool DenoiserGPU::denoise_filter_guiding_set_fake_albedo(const DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.guiding_params.device_pointer,
                             &context.guiding_params.pass_stride,
                             &context.guiding_params.pass_albedo,
                             &buffer_params.width,
                             &buffer_params.height);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_GUIDING_SET_FAKE_ALBEDO, work_size, args);
}

void DenoiserGPU::denoise_color_read(const DenoiseContext &context, const DenoisePass &pass)
{
  PassAccessor::PassAccessInfo pass_access_info;
  pass_access_info.type = pass.type;
  pass_access_info.mode = PassMode::NOISY;
  pass_access_info.offset = pass.noisy_offset;

  /* Denoiser operates on passes which are used to calculate the approximation, and is never used
   * on the approximation. The latter is not even possible because OptiX does not support
   * denoising of semi-transparent pixels. */
  pass_access_info.use_approximate_shadow_catcher = false;
  pass_access_info.use_approximate_shadow_catcher_background = false;
  pass_access_info.show_active_pixels = false;

  /* TODO(sergey): Consider adding support of actual exposure, to avoid clamping in extreme cases.
   */
  const PassAccessorGPU pass_accessor(
      denoiser_queue_.get(), pass_access_info, 1.0f, context.num_samples);

  PassAccessor::Destination destination(pass_access_info.type);
  destination.d_pixels = context.render_buffers->buffer.device_pointer +
                         pass.denoised_offset * sizeof(float);
  destination.num_components = 3;
  destination.pixel_stride = context.buffer_params.pass_stride;

  BufferParams buffer_params = context.buffer_params;
  buffer_params.window_x = 0;
  buffer_params.window_y = 0;
  buffer_params.window_width = buffer_params.width;
  buffer_params.window_height = buffer_params.height;

  pass_accessor.get_render_tile_pixels(context.render_buffers, buffer_params, destination);
}

void DenoiserGPU::denoise_pass(DenoiseContext &context, PassType pass_type)
{
  const BufferParams &buffer_params = context.buffer_params;

  const DenoisePass pass(pass_type, buffer_params);

  if (pass.noisy_offset == PASS_UNUSED) {
    return;
  }
  if (pass.denoised_offset == PASS_UNUSED) {
    LOG(DFATAL) << "Missing denoised pass " << pass_type_as_string(pass_type);
    return;
  }

  if (pass.use_denoising_albedo) {
    if (context.albedo_replaced_with_fake) {
      LOG(ERROR) << "Pass which requires albedo is denoised after fake albedo has been set.";
      return;
    }
  }
  else if (context.use_guiding_passes && !context.albedo_replaced_with_fake) {
    context.albedo_replaced_with_fake = true;
    if (!denoise_filter_guiding_set_fake_albedo(context)) {
      LOG(ERROR) << "Error replacing real albedo with the fake one.";
      return;
    }
  }

  /* Read and preprocess noisy color input pass. */
  denoise_color_read(context, pass);
  if (!denoise_filter_color_preprocess(context, pass)) {
    LOG(ERROR) << "Error converting denoising passes to RGB buffer.";
    return;
  }

  if (!denoise_run(context, pass)) {
    LOG(ERROR) << "Error running denoiser.";
    return;
  }

  /* Store result in the combined pass of the render buffer.
   *
   * This will scale the denoiser result up to match the number of, possibly per-pixel, samples. */
  if (!denoise_filter_color_postprocess(context, pass)) {
    LOG(ERROR) << "Error copying denoiser result to the denoised pass.";
    return;
  }

  denoiser_queue_->synchronize();
}

CCL_NAMESPACE_END

/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/denoiser_gpu.h"

#include "device/denoise.h"
#include "device/device.h"
#include "device/memory.h"
#include "device/queue.h"

#include "integrator/pass_accessor_gpu.h"

#include "session/buffers.h"

#include "util/log.h"

CCL_NAMESPACE_BEGIN

DenoiserGPU::DenoiserGPU(Device *denoiser_device, const DenoiseParams &params)
    : Denoiser(denoiser_device, params)
{
  denoiser_queue_ = denoiser_device->gpu_queue_create();
  DCHECK(denoiser_queue_);
}

DenoiserGPU::~DenoiserGPU()  // NOLINT
{
  /* Explicit implementation, to allow forward declaration of Device in the header. */
}

bool DenoiserGPU::denoise_buffer(const BufferParams &buffer_params,
                                 const BufferParams &denoised_buffer_params,
                                 RenderBuffers *render_buffers,
                                 const int num_samples,
                                 const bool allow_inplace_modification,
                                 const float2 pixel_jitter)
{
  Device *denoiser_device = get_denoiser_device();
  if (!denoiser_device) {
    return false;
  }

  RenderBuffers local_render_buffers(denoiser_device);
  bool local_buffer_used = false;

  if (denoiser_device == render_buffers->buffer.device) {
    /* The device can access an existing buffer pointer. */
    local_buffer_used = false;
  }
  else {
    LOG_DEBUG << "Creating temporary buffer on denoiser device.";

    /* Create buffer which is available by the device used by denoiser. */

    /* TODO(sergey): Optimize data transfers. For example, only copy denoising related passes,
     * ignoring other light ad data passes. */

    local_buffer_used = true;

    render_buffers->copy_from_device();

    local_render_buffers.reset(denoised_buffer_params);

    /* NOTE: The local buffer is allocated for an exact size of the effective render size, while
     * the input render buffer is allocated for the lowest resolution divider possible. So it is
     * important to only copy actually needed part of the input buffer. */
    memcpy(local_render_buffers.buffer.data(),
           render_buffers->buffer.data(),
           sizeof(float) * local_render_buffers.buffer.size());

    denoiser_queue_->copy_to_device(local_render_buffers.buffer);
  }

  {
    DenoiseContext context(denoiser_device,
                           params_,
                           buffer_params,
                           denoised_buffer_params,
                           local_buffer_used ? &local_render_buffers : render_buffers,
                           num_samples,
                           local_buffer_used || allow_inplace_modification,
                           pixel_jitter);

    if (!denoise_ensure(context)) {
      return false;
    }

    if (!denoise_filter_guiding_preprocess(context)) {
      LOG_ERROR << "Error preprocessing guiding passes.";
      return false;
    }

    /* Passes which will use real albedo when it is available. */
    if (!denoise_pass(context, PASS_COMBINED)) {
      return false;
    }
    if (!denoise_pass(context, PASS_SHADOW_CATCHER_MATTE)) {
      return false;
    }

    /* Passes which do not need albedo and hence if real is present it needs to become fake. */
    if (!denoise_pass(context, PASS_SHADOW_CATCHER)) {
      return false;
    }
  }

  if (local_buffer_used) {
    local_render_buffers.copy_from_device();

    render_buffers_host_copy_denoised(render_buffers,
                                      denoised_buffer_params,
                                      &local_render_buffers,
                                      local_render_buffers.params);

    render_buffers->copy_to_device();
  }

  return true;
}

bool DenoiserGPU::denoise_ensure(DenoiseContext &context)
{
  if (!denoise_create_if_needed(context)) {
    LOG_ERROR << "GPU denoiser creation has failed.";
    return false;
  }

  if (!denoise_configure_if_needed(context)) {
    LOG_ERROR << "GPU denoiser configuration has failed.";
    return false;
  }

  return true;
}

bool DenoiserGPU::denoise_filter_guiding_preprocess(DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  /* Delay the allocation of the guiding buffer to first use, in case it's not actually needed
   * (e.g. with the DLSS denoiser, which overrides this implementation). */
  if (context.use_guiding_passes && !context.guiding_params.device_pointer) {
    context.guiding_buffer.alloc_to_device(buffer_params.width * buffer_params.height *
                                           context.guiding_params.pass_stride);
    context.guiding_params.device_pointer = context.guiding_buffer.device_pointer;
  }

  const int work_size = buffer_params.width * buffer_params.height;

  const DeviceKernelArguments args(&context.guiding_params.device_pointer,
                                   &context.guiding_params.pass_stride,
                                   &context.guiding_params.pass_albedo,
                                   &context.guiding_params.pass_normal,
                                   &context.guiding_params.pass_flow,
                                   &context.render_buffers->buffer.device_pointer,
                                   &buffer_params.offset,
                                   &buffer_params.stride,
                                   &buffer_params.pass_stride,
                                   &context.pass_sample_count,
                                   &context.pass_denoising_albedo,
                                   &context.pass_denoising_normal,
                                   &context.pass_motion,
                                   &buffer_params.full_x,
                                   &buffer_params.full_y,
                                   &buffer_params.width,
                                   &buffer_params.height,
                                   &context.num_samples);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS, work_size, args) &&
         denoise_filter_guiding_flip_y(context);
}

DenoiserGPU::DenoiseContext::DenoiseContext(Device *device,
                                            const DenoiseParams &params,
                                            const BufferParams &buffer_params,
                                            const BufferParams &denoised_buffer_params,
                                            RenderBuffers *render_buffers,
                                            const int num_samples,
                                            const bool allow_inplace_modification,
                                            const float2 pixel_jitter)
    : denoise_params(params),
      render_buffers(render_buffers),
      buffer_params(buffer_params),
      denoised_buffer_params(denoised_buffer_params),
      guiding_buffer(device, "denoiser guiding passes buffer", true),
      use_guiding_passes(params.passes != DENOISER_PASS_NONE),
      num_samples(num_samples),
      pixel_jitter(pixel_jitter)
{
  pass_motion = buffer_params.get_pass_offset(PASS_MOTION);
  pass_sample_count = buffer_params.get_pass_offset(PASS_SAMPLE_COUNT);

  if (params.passes & DENOISER_PASS_ALBEDO) {
    pass_denoising_albedo = buffer_params.get_pass_offset(PASS_DENOISING_ALBEDO);
  }
  if (params.passes & DENOISER_PASS_NORMAL) {
    pass_denoising_normal = buffer_params.get_pass_offset(PASS_DENOISING_NORMAL);
  }

  if (params.temporally_stable) {
    prev_output.device_pointer = render_buffers->buffer.device_pointer;

    prev_output.offset = buffer_params.get_pass_offset(PASS_DENOISING_PREVIOUS);

    prev_output.stride = buffer_params.stride;
    prev_output.pass_stride = buffer_params.pass_stride;
  }

  if (use_guiding_passes) {
    if (allow_inplace_modification) {
      guiding_params.device_pointer = render_buffers->buffer.device_pointer;

      guiding_params.pass_albedo = pass_denoising_albedo;
      guiding_params.pass_normal = pass_denoising_normal;
      guiding_params.pass_flow = pass_motion;

      guiding_params.stride = buffer_params.stride;
      guiding_params.pass_stride = buffer_params.pass_stride;
    }
    else {
      guiding_params.pass_stride = 0;
      if (params.passes & DENOISER_PASS_ALBEDO) {
        guiding_params.pass_albedo = guiding_params.pass_stride;
        guiding_params.pass_stride += 3;
      }
      if (params.passes & DENOISER_PASS_NORMAL) {
        guiding_params.pass_normal = guiding_params.pass_stride;
        guiding_params.pass_stride += 3;
      }
      if (params.passes & DENOISER_PASS_MOTION) {
        guiding_params.pass_flow = guiding_params.pass_stride;
        guiding_params.pass_stride += 2;
      }

      guiding_params.stride = buffer_params.width;
    }
  }
}

bool DenoiserGPU::denoise_filter_color_postprocess(const DenoiseContext &context,
                                                   const DenoisePass &pass)
{
  if (!denoise_filter_color_flip_y(context, context.denoised_buffer_params, pass)) {
    return false;
  }

  const BufferParams &buffer_params = context.denoised_buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  const DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                                   &buffer_params.full_x,
                                   &buffer_params.full_y,
                                   &buffer_params.width,
                                   &buffer_params.height,
                                   &buffer_params.offset,
                                   &buffer_params.stride,
                                   &context.buffer_params.full_x,
                                   &context.buffer_params.full_y,
                                   &context.buffer_params.offset,
                                   &context.buffer_params.stride,
                                   &buffer_params.pass_stride,
                                   &context.num_samples,
                                   &pass.noisy_offset,
                                   &pass.denoised_offset,
                                   &context.pass_sample_count,
                                   &pass.num_components,
                                   &pass.use_compositing,
                                   &params_.upscale_factor);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS, work_size, args);
}

bool DenoiserGPU::denoise_filter_color_preprocess(const DenoiseContext &context,
                                                  const DenoisePass &pass)
{
  if (context.denoise_params.type != DENOISER_OPTIX) {
    /* Pass preprocessing is used to clamp values for the OptiX denoiser.
     * Clamping is not necessary for other denoisers, so just skip this preprocess step. */
    return true;
  }

  if (!denoise_filter_color_flip_y(context, context.buffer_params, pass)) {
    return false;
  }

  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  const DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
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

bool DenoiserGPU::denoise_filter_color_flip_y(const DenoiseContext &context,
                                              const BufferParams &buffer_params,
                                              const DenoisePass &pass)
{
  if (context.denoise_params.type != DENOISER_OPTIX || context.denoise_params.temporally_stable) {
    /* Flipping the image is used to improve result quality with the OptiX denoiser.
     * It is not necessary for other denoisers, so just skip this preprocess step. */
    return true;
  }

  const int work_size = buffer_params.width * buffer_params.height / 2;

  const DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                                   &buffer_params.full_x,
                                   &buffer_params.full_y,
                                   &buffer_params.width,
                                   &buffer_params.height,
                                   &buffer_params.offset,
                                   &buffer_params.stride,
                                   &buffer_params.pass_stride,
                                   &pass.denoised_offset);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_FLIP_Y, work_size, args);
}

bool DenoiserGPU::denoise_filter_guiding_flip_y(const DenoiseContext &context)
{
  if (context.denoise_params.type != DENOISER_OPTIX || context.denoise_params.temporally_stable) {
    /* Flipping the image is used to improve result quality with the OptiX denoiser.
     * It is not necessary for other denoisers, so just skip this preprocess step. */
    return true;
  }

  const BufferParams &buffer_params = context.buffer_params;

  const int guiding_offset = 0;

  const int work_size = buffer_params.width * buffer_params.height / 2;

  const int guiding_passes[] = {context.guiding_params.pass_albedo,
                                context.guiding_params.pass_normal};
  for (const int guiding_pass : guiding_passes) {
    if (guiding_pass == PASS_UNUSED) {
      continue;
    }

    const DeviceKernelArguments args(&context.guiding_params.device_pointer,
                                     &guiding_offset,
                                     &guiding_offset,
                                     &buffer_params.width,
                                     &buffer_params.height,
                                     &guiding_offset,
                                     &context.guiding_params.stride,
                                     &context.guiding_params.pass_stride,
                                     &guiding_pass);

    if (!denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_FLIP_Y, work_size, args)) {
      return false;
    }
  }
  return true;
}

bool DenoiserGPU::denoise_filter_guiding_set_fake_albedo(DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  if (context.use_guiding_passes && !context.guiding_params.device_pointer) {
    context.guiding_buffer.alloc_to_device(buffer_params.width * buffer_params.height *
                                           context.guiding_params.pass_stride);
    context.guiding_params.device_pointer = context.guiding_buffer.device_pointer;
  }

  const int work_size = buffer_params.width * buffer_params.height;

  const DeviceKernelArguments args(&context.guiding_params.device_pointer,
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

  PassAccessor::Destination destination(pass_access_info.type, pass_access_info.mode);
  destination.d_pixels = context.render_buffers->buffer.device_pointer;
  destination.num_components = 3;
  destination.pixel_offset = pass.denoised_offset;
  destination.pixel_stride = context.buffer_params.pass_stride;

  BufferParams buffer_params = context.buffer_params;
  buffer_params.window_x = 0;
  buffer_params.window_y = 0;
  buffer_params.window_width = buffer_params.width;
  buffer_params.window_height = buffer_params.height;

  pass_accessor.get_render_tile_pixels(context.render_buffers, buffer_params, destination);
}

bool DenoiserGPU::denoise_pass(DenoiseContext &context, PassType pass_type)
{
  const BufferParams &buffer_params = context.buffer_params;

  const DenoisePass pass(pass_type, buffer_params);

  if (pass.noisy_offset == PASS_UNUSED) {
    return true;
  }
  if (pass.denoised_offset == PASS_UNUSED) {
    LOG_DFATAL << "Missing denoised pass " << pass_type_as_string(pass_type);
    return false;
  }

  if (pass.use_denoising_albedo) {
    if (context.albedo_replaced_with_fake) {
      LOG_ERROR << "Pass which requires albedo is denoised after fake albedo has been set.";
      return false;
    }
  }
  else if (context.use_guiding_passes && !context.albedo_replaced_with_fake) {
    context.albedo_replaced_with_fake = true;
    if (!denoise_filter_guiding_set_fake_albedo(context)) {
      LOG_ERROR << "Error replacing real albedo with the fake one.";
      return false;
    }
  }

  /* Read and preprocess noisy color input pass. */
  denoise_color_read(context, pass);
  if (!denoise_filter_color_preprocess(context, pass)) {
    LOG_ERROR << "Error converting denoising passes to RGB buffer.";
    return false;
  }

  if (!denoise_run(context, pass)) {
    LOG_ERROR << "Error running denoiser.";
    return false;
  }

  /* Store result in the combined pass of the render buffer.
   *
   * This will scale the denoiser result up to match the number of, possibly per-pixel, samples. */
  if (!denoise_filter_color_postprocess(context, pass)) {
    LOG_ERROR << "Error copying denoiser result to the denoised pass.";
    return false;
  }

  return denoiser_queue_->synchronize();
}

CCL_NAMESPACE_END

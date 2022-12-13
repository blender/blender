/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "integrator/denoiser_gpu.h"

#include "device/denoise.h"
#include "device/device.h"
#include "device/memory.h"
#include "device/queue.h"
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

CCL_NAMESPACE_END

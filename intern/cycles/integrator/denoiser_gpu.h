/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "integrator/denoiser.h"

CCL_NAMESPACE_BEGIN

/* Implementation of Denoiser which uses a device-specific denoising implementation, running on a
 * GPU device queue. It makes sure the to-be-denoised buffer is available on the denoising device
 * and invokes denoising kernels via the device queue API. */
class DenoiserGPU : public Denoiser {
 public:
  DenoiserGPU(Device *path_trace_device, const DenoiseParams &params);
  ~DenoiserGPU();

  virtual bool denoise_buffer(const BufferParams &buffer_params,
                              RenderBuffers *render_buffers,
                              const int num_samples,
                              bool allow_inplace_modification) override;

 protected:
  /* All the parameters needed to perform buffer denoising on a device.
   * Is not really a task in its canonical terms (as in, is not an asynchronous running task). Is
   * more like a wrapper for all the arguments and parameters needed to perform denoising. Is a
   * single place where they are all listed, so that it's not required to modify all device methods
   * when these parameters do change. */
  class DenoiseTask {
   public:
    DenoiseParams params;

    int num_samples;

    RenderBuffers *render_buffers;
    BufferParams buffer_params;

    /* Allow to do in-place modification of the input passes (scaling them down i.e.). This will
     * lower the memory footprint of the denoiser but will make input passes "invalid" (from path
     * tracer) point of view. */
    bool allow_inplace_modification;
  };

  /* Returns true if task is fully handled. */
  virtual bool denoise_buffer(const DenoiseTask & /*task*/) = 0;

  virtual Device *ensure_denoiser_device(Progress *progress) override;

  unique_ptr<DeviceQueue> denoiser_queue_;
};

CCL_NAMESPACE_END

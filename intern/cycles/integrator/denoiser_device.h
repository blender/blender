/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "integrator/denoiser.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

/* Denoiser which uses device-specific denoising implementation, such as OptiX denoiser which are
 * implemented as a part of a driver of specific device.
 *
 * This implementation makes sure the to-be-denoised buffer is available on the denoising device
 * and invoke denoising kernel via device API. */
class DeviceDenoiser : public Denoiser {
 public:
  DeviceDenoiser(Device *path_trace_device, const DenoiseParams &params);
  ~DeviceDenoiser();

  virtual bool denoise_buffer(const BufferParams &buffer_params,
                              RenderBuffers *render_buffers,
                              const int num_samples,
                              bool allow_inplace_modification) override;
};

CCL_NAMESPACE_END

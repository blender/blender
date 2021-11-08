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

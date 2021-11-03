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
#include "util/thread.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

/* Implementation of denoising API which uses OpenImageDenoise library. */
class OIDNDenoiser : public Denoiser {
 public:
  /* Forwardly declared state which might be using compile-flag specific fields, such as
   * OpenImageDenoise device and filter handles. */
  class State;

  OIDNDenoiser(Device *path_trace_device, const DenoiseParams &params);

  virtual bool denoise_buffer(const BufferParams &buffer_params,
                              RenderBuffers *render_buffers,
                              const int num_samples,
                              bool allow_inplace_modification) override;

 protected:
  virtual uint get_device_type_mask() const override;

  /* We only perform one denoising at a time, since OpenImageDenoise itself is multithreaded.
   * Use this mutex whenever images are passed to the OIDN and needs to be denoised. */
  static thread_mutex mutex_;
};

CCL_NAMESPACE_END

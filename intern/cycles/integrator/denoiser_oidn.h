/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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
  virtual Device *ensure_denoiser_device(Progress *progress) override;

  /* We only perform one denoising at a time, since OpenImageDenoise itself is multithreaded.
   * Use this mutex whenever images are passed to the OIDN and needs to be denoised. */
  static thread_mutex mutex_;
};

CCL_NAMESPACE_END

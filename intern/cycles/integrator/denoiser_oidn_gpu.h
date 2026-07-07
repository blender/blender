/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(WITH_OPENIMAGEDENOISE)

#  include "integrator/denoiser_gpu.h"
#  include "integrator/denoiser_oidn_base.h"
#  include "util/openimagedenoise.h"  // IWYU pragma: keep

CCL_NAMESPACE_BEGIN

/* Implementation of a GPU denoiser which uses OpenImageDenoise library. */
class OIDNDenoiserGPU : public DenoiserGPU {
 public:
  class State;

  OIDNDenoiserGPU(Device *denoiser_device, const DenoiseParams &params);

  bool denoise_buffer(const BufferParams &buffer_params,
                      RenderBuffers *render_buffers,
                      const int num_samples,
                      bool allow_inplace_modification) override;

  static bool is_device_supported(const DeviceInfo &device);

 protected:
  enum class ExecMode {
    SYNC,
    ASYNC,
  };

  uint get_device_type_mask() const override;

  /* Create OIDN denoiser descriptor if needed.
   * Will do nothing if the current OIDN descriptor is usable for the given parameters.
   * If the OIDN denoiser descriptor did re-allocate here it is left unconfigured. */
  bool denoise_create_if_needed(DenoiseContext &context) override;

  /* Configure existing OIDN denoiser descriptor for the use for the given task. */
  bool denoise_configure_if_needed(DenoiseContext &context) override;

  /* Run configured denoiser. */
  bool denoise_run(const DenoiseContext &context, const DenoisePass &pass) override;

  bool commit_and_execute_filter(OIDNFilter filter, ExecMode mode = ExecMode::SYNC);

  void set_filter_pass(OIDNFilter filter,
                       const char *name,
                       device_ptr ptr,
                       const int format,
                       const int width,
                       const int height,
                       const size_t offset_in_bytes,
                       const size_t pixel_stride_in_bytes,
                       size_t row_stride_in_bytes);

  OIDNDenoiserBase base_;

  /* Filter memory usage limit if we ran out of memory with OIDN's default limit. */
  int max_mem_ = 768;
};

CCL_NAMESPACE_END

#endif

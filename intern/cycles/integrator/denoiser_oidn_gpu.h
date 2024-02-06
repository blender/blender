/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(WITH_OPENIMAGEDENOISE)

#  include "integrator/denoiser_gpu.h"
#  include "util/thread.h"
#  include "util/unique_ptr.h"

typedef struct OIDNDeviceImpl *OIDNDevice;
typedef struct OIDNFilterImpl *OIDNFilter;
typedef struct OIDNBufferImpl *OIDNBuffer;

CCL_NAMESPACE_BEGIN

/* Implementation of a GPU denoiser which uses OpenImageDenoise library. */
class OIDNDenoiserGPU : public DenoiserGPU {
  friend class OIDNDenoiseContext;

 public:
  /* Forwardly declared state which might be using compile-flag specific fields, such as
   * OpenImageDenoise device and filter handles. */
  class State;

  OIDNDenoiserGPU(Device *path_trace_device, const DenoiseParams &params);
  ~OIDNDenoiserGPU();

  virtual bool denoise_buffer(const BufferParams &buffer_params,
                              RenderBuffers *render_buffers,
                              const int num_samples,
                              bool allow_inplace_modification) override;

  static bool is_device_supported(const DeviceInfo &device);

 protected:
  virtual uint get_device_type_mask() const override;

  /* Create OIDN denoiser descriptor if needed.
   * Will do nothing if the current OIDN descriptor is usable for the given parameters.
   * If the OIDN denoiser descriptor did re-allocate here it is left unconfigured. */
  virtual bool denoise_create_if_needed(DenoiseContext &context) override;

  /* Configure existing OIDN denoiser descriptor for the use for the given task. */
  virtual bool denoise_configure_if_needed(DenoiseContext &context) override;

  /* Run configured denoiser. */
  virtual bool denoise_run(const DenoiseContext &context, const DenoisePass &pass) override;

  OIDNFilter create_filter();

  OIDNDevice oidn_device_ = nullptr;
  OIDNFilter oidn_filter_ = nullptr;
  OIDNFilter albedo_filter_ = nullptr;
  OIDNFilter normal_filter_ = nullptr;

  bool is_configured_ = false;
  int2 configured_size_ = make_int2(0, 0);

  bool use_pass_albedo_ = false;
  bool use_pass_normal_ = false;
  DenoiserQuality quality_ = DENOISER_QUALITY_HIGH;

  int max_mem_ = 3000;
};

CCL_NAMESPACE_END

#endif

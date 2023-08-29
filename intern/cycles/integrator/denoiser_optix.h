/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPTIX

#  include "integrator/denoiser_gpu.h"

#  include "device/optix/util.h"

CCL_NAMESPACE_BEGIN

/* Implementation of denoising API which uses the OptiX denoiser. */
class OptiXDenoiser : public DenoiserGPU {
 public:
  OptiXDenoiser(Device *path_trace_device, const DenoiseParams &params);
  ~OptiXDenoiser();

 protected:
  virtual uint get_device_type_mask() const override;

 private:
  virtual bool denoise_buffer(const DenoiseTask &task) override;

  /* Read guiding passes from the render buffers, preprocess them in a way which is expected by
   * OptiX and store in the guiding passes memory within the given context.
   *
   * Pre-processing of the guiding passes is to only happen once per context lifetime. DO not
   * preprocess them for every pass which is being denoised. */
  bool denoise_filter_guiding_preprocess(const DenoiseContext &context);

  /* Set fake albedo pixels in the albedo guiding pass storage.
   * After this point only passes which do not need albedo for denoising can be processed. */
  bool denoise_filter_guiding_set_fake_albedo(const DenoiseContext &context);
  /* Make sure the OptiX denoiser is created and configured. */
  bool denoise_ensure(DenoiseContext &context);

  /* Create OptiX denoiser descriptor if needed.
   * Will do nothing if the current OptiX descriptor is usable for the given parameters.
   * If the OptiX denoiser descriptor did re-allocate here it is left unconfigured. */
  bool denoise_create_if_needed(DenoiseContext &context);

  /* Configure existing OptiX denoiser descriptor for the use for the given task. */
  bool denoise_configure_if_needed(DenoiseContext &context);

  /* Run configured denoiser. */
  bool denoise_run(const DenoiseContext &context, const DenoisePass &pass) override;

  OptixDenoiser optix_denoiser_ = nullptr;

  /* Configuration size, as provided to `optixDenoiserSetup`.
   * If the `optixDenoiserSetup()` was never used on the current `optix_denoiser` the
   * `is_configured` will be false. */
  bool is_configured_ = false;
  int2 configured_size_ = make_int2(0, 0);

  /* OptiX denoiser state and scratch buffers, stored in a single memory buffer.
   * The memory layout goes as following: [denoiser state][scratch buffer]. */
  device_only_memory<unsigned char> state_;
  OptixDenoiserSizes sizes_ = {};

  bool use_pass_albedo_ = false;
  bool use_pass_normal_ = false;
  bool use_pass_motion_ = false;
};

CCL_NAMESPACE_END

#endif

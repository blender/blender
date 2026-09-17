/* SPDX-FileCopyrightText: 2025 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_DLSS

#  include "integrator/denoiser_gpu.h"

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_CUDADevice;

CCL_NAMESPACE_BEGIN

/* Implementation of denoising API which uses DLSS. */
class DLSSDenoiser : public DenoiserGPU {
 public:
  DLSSDenoiser(Device *denoiser_device, const DenoiseParams &params);
  ~DLSSDenoiser();

  static bool is_device_supported(const DeviceInfo &device);

 private:
  bool denoise_create_if_needed(DenoiseContext &context) override;

  bool denoise_configure_if_needed(DenoiseContext &context) override;

  bool denoise_filter_color_preprocess(const DenoiseContext &context,
                                       const DenoisePass &pass) override;
  bool denoise_filter_color_postprocess(const DenoiseContext &context,
                                        const DenoisePass &pass) override;

  bool denoise_filter_guiding_preprocess(DenoiseContext &context) override;

  bool denoise_run(const DenoiseContext &context, const DenoisePass &pass) override;

  NVSDK_NGX_Handle *handle_ = nullptr;
  NVSDK_NGX_CUDADevice *ngx_device_ = nullptr;

  struct CUDATexture {
    void init(Device *device, int width, int height, int num_components);
    void destroy();

    void *array = nullptr;
    uint64_t texture_handle = 0;
    uint64_t surface_handle = 0;
  };
  CUDATexture tex_color_;
  CUDATexture tex_depth_;
  CUDATexture tex_diffuse_albedo_;
  CUDATexture tex_specular_albedo_;
  CUDATexture tex_normal_roughness_;
  CUDATexture tex_motion_;
  CUDATexture tex_specular_motion_;
  CUDATexture tex_output_;

  int last_width_ = 0;
  int last_height_ = 0;
  float last_upscale_factor_ = 0.0f;
};

CCL_NAMESPACE_END

#endif

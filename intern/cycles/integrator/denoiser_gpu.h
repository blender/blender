/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "integrator/denoiser.h"

#include "session/buffers.h"

CCL_NAMESPACE_BEGIN

/* Implementation of Denoiser which uses a device-specific denoising implementation, running on a
 * GPU device queue. It makes sure the to-be-denoised buffer is available on the denoising device
 * and invokes denoising kernels via the device queue API. */
class DenoiserGPU : public Denoiser {
 public:
  DenoiserGPU(Device *denoiser_device, const DenoiseParams &params);
  ~DenoiserGPU() override;

  bool denoise_buffer(const BufferParams &buffer_params,
                      const BufferParams &denoised_buffer_params,
                      RenderBuffers *render_buffers,
                      int num_samples,
                      bool allow_inplace_modification,
                      float2 pixel_jitter) override;

 protected:
  class DenoisePass;
  class DenoiseContext;

  /* Make sure the GPU denoiser is created and configured. */
  virtual bool denoise_ensure(DenoiseContext &context);

  /* Create GPU denoiser descriptor if needed.
   * Will do nothing if the current GPU descriptor is usable for the given parameters.
   * If the GPU denoiser descriptor did re-allocate here it is left unconfigured. */
  virtual bool denoise_create_if_needed(DenoiseContext &context) = 0;

  /* Configure existing GPU denoiser descriptor for the use for the given task. */
  virtual bool denoise_configure_if_needed(DenoiseContext &context) = 0;

  /* Read input color pass from the render buffer into the memory which corresponds to the noisy
   * input within the given context. Pixels are scaled to the number of samples, but are not
   * preprocessed yet. */
  void denoise_color_read(const DenoiseContext &context, const DenoisePass &pass);

  /* Run corresponding filter kernels, preparing data for the denoiser or copying data from the
   * denoiser result to the render buffer. */
  virtual bool denoise_filter_color_preprocess(const DenoiseContext &context,
                                               const DenoisePass &pass);
  virtual bool denoise_filter_color_postprocess(const DenoiseContext &context,
                                                const DenoisePass &pass);
  bool denoise_filter_color_flip_y(const DenoiseContext &context,
                                   const BufferParams &buffer_params,
                                   const DenoisePass &pass);
  bool denoise_filter_guiding_flip_y(const DenoiseContext &context);
  bool denoise_filter_guiding_set_fake_albedo(DenoiseContext &context);

  /* Read guiding passes from the render buffers, preprocess them in a way which is expected by
   * the GPU denoiser and store in the guiding passes memory within the given context.
   *
   * Pre-processing of the guiding passes is to only happen once per context lifetime. DO not
   * preprocess them for every pass which is being denoised. */
  virtual bool denoise_filter_guiding_preprocess(DenoiseContext &context);

  bool denoise_pass(DenoiseContext &context, PassType pass_type);

  /* Returns true if task is fully handled. */
  virtual bool denoise_run(const DenoiseContext &context, const DenoisePass &pass) = 0;

  unique_ptr<DeviceQueue> denoiser_queue_;

  class DenoisePass {
   public:
    DenoisePass(const PassType type, const BufferParams &buffer_params) : type(type)
    {
      noisy_offset = buffer_params.get_pass_offset(type, PassMode::NOISY);
      denoised_offset = buffer_params.get_pass_offset(type, PassMode::DENOISED);

      const PassInfo pass_info = Pass::get_info(type);
      num_components = pass_info.num_components;
      use_compositing = pass_info.use_compositing;
      use_denoising_albedo = pass_info.use_denoising_albedo;
    }

    PassType type;

    int noisy_offset;
    int denoised_offset;

    int num_components;
    int use_compositing;
    bool use_denoising_albedo;
  };

  class DenoiseContext {
   public:
    explicit DenoiseContext(Device *device,
                            const DenoiseParams &params,
                            const BufferParams &buffer_params,
                            const BufferParams &denoised_buffer_params,
                            RenderBuffers *render_buffers,
                            int num_samples,
                            bool allow_inplace_modification,
                            float2 pixel_jitter);

    const DenoiseParams &denoise_params;

    RenderBuffers *render_buffers = nullptr;
    const BufferParams &buffer_params;
    const BufferParams &denoised_buffer_params;

    /* Previous output. */
    struct {
      device_ptr device_pointer = 0;

      int offset = PASS_UNUSED;

      int stride = -1;
      int pass_stride = -1;
    } prev_output;

    /* Device-side storage of the guiding passes. */
    device_only_memory<float> guiding_buffer;

    struct {
      device_ptr device_pointer = 0;

      /* NOTE: Are only initialized when the corresponding guiding pass is enabled. */
      int pass_albedo = PASS_UNUSED;
      int pass_normal = PASS_UNUSED;
      int pass_flow = PASS_UNUSED;

      int stride = -1;
      int pass_stride = -1;
    } guiding_params;

    const bool use_guiding_passes = false;

    int num_samples = 0;

    int pass_sample_count = PASS_UNUSED;

    /* NOTE: Are only initialized when the corresponding guiding pass is enabled. */
    int pass_denoising_albedo = PASS_UNUSED;
    int pass_denoising_normal = PASS_UNUSED;
    int pass_motion = PASS_UNUSED;

    /* For passes which don't need albedo channel for denoising we replace the actual albedo with
     * the (0.5, 0.5, 0.5). This flag indicates that the real albedo pass has been replaced with
     * the fake values and denoising of passes which do need albedo can no longer happen. */
    bool albedo_replaced_with_fake = false;

    /* Sub-pixel jitter offset of the current frame. This can be used for upscaling. */
    float2 pixel_jitter;
  };
};

CCL_NAMESPACE_END

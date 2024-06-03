/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "integrator/denoiser.h"

CCL_NAMESPACE_BEGIN

/* Implementation of Denoiser which uses a device-specific denoising implementation, running on a
 * GPU device queue. It makes sure the to-be-denoised buffer is available on the denoising device
 * and invokes denoising kernels via the device queue API. */
class DenoiserGPU : public Denoiser {
 public:
  DenoiserGPU(Device *denoiser_device, const DenoiseParams &params);
  ~DenoiserGPU();

  virtual bool denoise_buffer(const BufferParams &buffer_params,
                              RenderBuffers *render_buffers,
                              const int num_samples,
                              bool allow_inplace_modification) override;

 protected:
  class DenoisePass;
  class DenoiseContext;

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
  bool denoise_filter_color_preprocess(const DenoiseContext &context, const DenoisePass &pass);
  bool denoise_filter_color_postprocess(const DenoiseContext &context, const DenoisePass &pass);
  bool denoise_filter_guiding_set_fake_albedo(const DenoiseContext &context);

  /* Read guiding passes from the render buffers, preprocess them in a way which is expected by
   * the GPU denoiser and store in the guiding passes memory within the given context.
   *
   * Pre-processing of the guiding passes is to only happen once per context lifetime. DO not
   * preprocess them for every pass which is being denoised. */
  bool denoise_filter_guiding_preprocess(const DenoiseContext &context);

  void denoise_pass(DenoiseContext &context, PassType pass_type);

  /* Returns true if task is fully handled. */
  virtual bool denoise_buffer(const DenoiseTask &task);
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
    explicit DenoiseContext(Device *device, const DenoiseTask &task);

    const DenoiseParams &denoise_params;

    RenderBuffers *render_buffers = nullptr;
    const BufferParams &buffer_params;

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

    /* Number of input passes. Including the color and extra auxiliary passes. */
    int num_input_passes = 0;
    bool use_guiding_passes = false;
    bool use_pass_albedo = false;
    bool use_pass_normal = false;
    bool use_pass_motion = false;

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
  };
};

CCL_NAMESPACE_END

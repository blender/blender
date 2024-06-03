/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/denoiser_oidn.h"

#include <array>

#include "device/device.h"
#include "device/queue.h"
#include "integrator/pass_accessor_cpu.h"
#include "session/buffers.h"
#include "util/array.h"
#include "util/log.h"
#include "util/openimagedenoise.h"
#include "util/path.h"

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/kernel.h"

CCL_NAMESPACE_BEGIN

thread_mutex OIDNDenoiser::mutex_;

OIDNDenoiser::OIDNDenoiser(Device *denoiser_device, const DenoiseParams &params)
    : Denoiser(denoiser_device, params)
{
  DCHECK_EQ(params.type, DENOISER_OPENIMAGEDENOISE);

#ifndef WITH_OPENIMAGEDENOISE
  (void)progress;
  set_error("Failed to denoise, build has no OpenImageDenoise support");
  return nullptr;
#else
  if (!openimagedenoise_supported()) {
    set_error("OpenImageDenoiser is not supported on this CPU: missing SSE 4.1 support");
  }
#endif
}

#ifdef WITH_OPENIMAGEDENOISE
static bool oidn_progress_monitor_function(void *user_ptr, double /*n*/)
{
  OIDNDenoiser *oidn_denoiser = reinterpret_cast<OIDNDenoiser *>(user_ptr);
  return !oidn_denoiser->is_cancelled();
}

class OIDNPass {
 public:
  OIDNPass() = default;

  OIDNPass(const BufferParams &buffer_params,
           const char *name,
           PassType type,
           PassMode mode = PassMode::NOISY)
      : name(name), type(type), mode(mode)
  {
    offset = buffer_params.get_pass_offset(type, mode);
    need_scale = (type == PASS_DENOISING_ALBEDO || type == PASS_DENOISING_NORMAL);

    const PassInfo pass_info = Pass::get_info(type);
    num_components = pass_info.num_components;
    use_compositing = pass_info.use_compositing;
    use_denoising_albedo = pass_info.use_denoising_albedo;
  }

  inline operator bool() const
  {
    return name[0] != '\0';
  }

  /* Name of an image which will be passed to the OIDN library.
   * Should be one of the following: color, albedo, normal, output.
   * The albedo and normal images are optional. */
  const char *name = "";

  PassType type = PASS_NONE;
  PassMode mode = PassMode::NOISY;
  int num_components = -1;
  bool use_compositing = false;
  bool use_denoising_albedo = true;

  /* Offset of beginning of this pass in the render buffers. */
  int offset = -1;

  /* Denotes whether the data is to be scaled down with the number of passes.
   * Is required for albedo and normal passes. The color pass OIDN will perform auto-exposure, so
   * scaling is not needed for the color pass unless adaptive sampling is used.
   *
   * NOTE: Do not scale the output pass, as that requires to be a pointer in the original buffer.
   * All the scaling on the output needed for integration with adaptive sampling will happen
   * outside of generic pass handling. */
  bool need_scale = false;

  /* The content of the pass has been pre-filtered. */
  bool is_filtered = false;

  /* For the scaled passes, the data which holds values of scaled pixels. */
  array<float> scaled_buffer;
};

class OIDNDenoiseContext {
 public:
  OIDNDenoiseContext(OIDNDenoiser *denoiser,
                     const DenoiseParams &denoise_params,
                     const BufferParams &buffer_params,
                     RenderBuffers *render_buffers,
                     const int num_samples,
                     const bool allow_inplace_modification)
      : denoiser_(denoiser),
        denoise_params_(denoise_params),
        buffer_params_(buffer_params),
        render_buffers_(render_buffers),
        num_samples_(num_samples),
        allow_inplace_modification_(allow_inplace_modification),
        pass_sample_count_(buffer_params_.get_pass_offset(PASS_SAMPLE_COUNT))
  {
    if (denoise_params_.use_pass_albedo) {
      oidn_albedo_pass_ = OIDNPass(buffer_params_, "albedo", PASS_DENOISING_ALBEDO);
    }

    if (denoise_params_.use_pass_normal) {
      oidn_normal_pass_ = OIDNPass(buffer_params_, "normal", PASS_DENOISING_NORMAL);
    }

    const char *custom_weight_path = getenv("CYCLES_OIDN_CUSTOM_WEIGHTS");
    if (custom_weight_path) {
      if (!path_read_binary(custom_weight_path, custom_weights)) {
        fprintf(stderr, "Cycles: Failed to load custom OIDN weights!");
      }
    }
  }

  bool need_denoising() const
  {
    if (buffer_params_.width == 0 && buffer_params_.height == 0) {
      return false;
    }

    return true;
  }

  /* Make the guiding passes available by a sequential denoising of various passes. */
  void read_guiding_passes()
  {
    read_guiding_pass(oidn_albedo_pass_);
    read_guiding_pass(oidn_normal_pass_);
  }

  void denoise_pass(const PassType pass_type)
  {
    OIDNPass oidn_color_pass(buffer_params_, "color", pass_type);
    if (oidn_color_pass.offset == PASS_UNUSED) {
      return;
    }

    if (oidn_color_pass.use_denoising_albedo) {
      if (albedo_replaced_with_fake_) {
        LOG(ERROR) << "Pass which requires albedo is denoised after fake albedo has been set.";
        return;
      }
    }

    OIDNPass oidn_output_pass(buffer_params_, "output", pass_type, PassMode::DENOISED);
    if (oidn_output_pass.offset == PASS_UNUSED) {
      LOG(DFATAL) << "Missing denoised pass " << pass_type_as_string(pass_type);
      return;
    }

    OIDNPass oidn_color_access_pass = read_input_pass(oidn_color_pass, oidn_output_pass);

    oidn::DeviceRef oidn_device = oidn::newDevice(oidn::DeviceType::CPU);
    oidn_device.set("setAffinity", false);
    oidn_device.commit();

    /* Create a filter for denoising a beauty (color) image using prefiltered auxiliary images too.
     */
    oidn::FilterRef oidn_filter = oidn_device.newFilter("RT");
    set_input_pass(oidn_filter, oidn_color_access_pass);
    set_guiding_passes(oidn_filter, oidn_color_pass);
    set_output_pass(oidn_filter, oidn_output_pass);
    oidn_filter.setProgressMonitorFunction(oidn_progress_monitor_function, denoiser_);
    oidn_filter.set("hdr", true);
    oidn_filter.set("srgb", false);
    if (custom_weights.size()) {
      oidn_filter.setData("weights", custom_weights.data(), custom_weights.size());
    }
    set_quality(oidn_filter);

    if (denoise_params_.prefilter == DENOISER_PREFILTER_NONE ||
        denoise_params_.prefilter == DENOISER_PREFILTER_ACCURATE)
    {
      oidn_filter.set("cleanAux", true);
    }
    oidn_filter.commit();

    filter_guiding_pass_if_needed(oidn_device, oidn_albedo_pass_);
    filter_guiding_pass_if_needed(oidn_device, oidn_normal_pass_);

    /* Filter the beauty image. */
    oidn_filter.execute();

    /* Check for errors. */
    const char *error_message;
    const oidn::Error error = oidn_device.getError(error_message);
    if (error != oidn::Error::None && error != oidn::Error::Cancelled) {
      denoiser_->set_error("OpenImageDenoise error: " + string(error_message));
    }

    postprocess_output(oidn_color_pass, oidn_output_pass);
  }

 protected:
  void filter_guiding_pass_if_needed(oidn::DeviceRef &oidn_device, OIDNPass &oidn_pass)
  {
    if (denoise_params_.prefilter != DENOISER_PREFILTER_ACCURATE || !oidn_pass ||
        oidn_pass.is_filtered)
    {
      return;
    }

    oidn::FilterRef oidn_filter = oidn_device.newFilter("RT");
    set_pass(oidn_filter, oidn_pass);
    set_output_pass(oidn_filter, oidn_pass);
    set_quality(oidn_filter);
    oidn_filter.commit();
    oidn_filter.execute();

    oidn_pass.is_filtered = true;
  }

  /* Make pixels of a guiding pass available by the denoiser. */
  void read_guiding_pass(OIDNPass &oidn_pass)
  {
    if (!oidn_pass) {
      return;
    }

    DCHECK(!oidn_pass.use_compositing);

    if (denoise_params_.prefilter != DENOISER_PREFILTER_ACCURATE &&
        !is_pass_scale_needed(oidn_pass))
    {
      /* Pass data is available as-is from the render buffers. */
      return;
    }

    if (allow_inplace_modification_) {
      scale_pass_in_render_buffers(oidn_pass);
      return;
    }

    read_pass_pixels_into_buffer(oidn_pass);
  }

  /* Special reader of the input pass.
   * To save memory it will read pixels into the output, and let the denoiser to perform an
   * in-place operation. */
  OIDNPass read_input_pass(OIDNPass &oidn_input_pass, const OIDNPass &oidn_output_pass)
  {
    const bool use_compositing = oidn_input_pass.use_compositing;

    /* Simple case: no compositing is involved, no scaling is needed.
     * The pass pixels will be referenced as-is, without extra processing. */
    if (!use_compositing && !is_pass_scale_needed(oidn_input_pass)) {
      return oidn_input_pass;
    }

    float *buffer_data = render_buffers_->buffer.data();
    float *pass_data = buffer_data + oidn_output_pass.offset;

    PassAccessor::Destination destination(pass_data, 3);
    destination.pixel_stride = buffer_params_.pass_stride;

    read_pass_pixels(oidn_input_pass, destination);

    OIDNPass oidn_input_pass_at_output = oidn_input_pass;
    oidn_input_pass_at_output.offset = oidn_output_pass.offset;

    return oidn_input_pass_at_output;
  }

  /* Read pass pixels using PassAccessor into the given destination. */
  void read_pass_pixels(const OIDNPass &oidn_pass, const PassAccessor::Destination &destination)
  {
    PassAccessor::PassAccessInfo pass_access_info;
    pass_access_info.type = oidn_pass.type;
    pass_access_info.mode = oidn_pass.mode;
    pass_access_info.offset = oidn_pass.offset;

    /* Denoiser operates on passes which are used to calculate the approximation, and is never used
     * on the approximation. The latter is not even possible because OIDN does not support
     * denoising of semi-transparent pixels. */
    pass_access_info.use_approximate_shadow_catcher = false;
    pass_access_info.use_approximate_shadow_catcher_background = false;
    pass_access_info.show_active_pixels = false;

    /* OIDN will perform an auto-exposure, so it is not required to know exact exposure configured
     * by users. What is important is to use same exposure for read and write access of the pass
     * pixels. */
    const PassAccessorCPU pass_accessor(pass_access_info, 1.0f, num_samples_);

    BufferParams buffer_params = buffer_params_;
    buffer_params.window_x = 0;
    buffer_params.window_y = 0;
    buffer_params.window_width = buffer_params.width;
    buffer_params.window_height = buffer_params.height;

    pass_accessor.get_render_tile_pixels(render_buffers_, buffer_params, destination);
  }

  /* Read pass pixels using PassAccessor into a temporary buffer which is owned by the pass.. */
  void read_pass_pixels_into_buffer(OIDNPass &oidn_pass)
  {
    VLOG_WORK << "Allocating temporary buffer for pass " << oidn_pass.name << " ("
              << pass_type_as_string(oidn_pass.type) << ")";

    const int64_t width = buffer_params_.width;
    const int64_t height = buffer_params_.height;

    array<float> &scaled_buffer = oidn_pass.scaled_buffer;
    scaled_buffer.resize(width * height * 3);

    const PassAccessor::Destination destination(scaled_buffer.data(), 3);

    read_pass_pixels(oidn_pass, destination);
  }

  /* Set OIDN image to reference pixels from the given render buffer pass.
   * No transform to the pixels is done, no additional memory is used. */
  void set_pass_referenced(oidn::FilterRef &oidn_filter,
                           const char *name,
                           const OIDNPass &oidn_pass)
  {
    const int64_t x = buffer_params_.full_x;
    const int64_t y = buffer_params_.full_y;
    const int64_t width = buffer_params_.width;
    const int64_t height = buffer_params_.height;
    const int64_t offset = buffer_params_.offset;
    const int64_t stride = buffer_params_.stride;
    const int64_t pass_stride = buffer_params_.pass_stride;

    const int64_t pixel_index = offset + x + y * stride;
    const int64_t buffer_offset = pixel_index * pass_stride;

    float *buffer_data = render_buffers_->buffer.data();

    oidn_filter.setImage(name,
                         buffer_data + buffer_offset + oidn_pass.offset,
                         oidn::Format::Float3,
                         width,
                         height,
                         0,
                         pass_stride * sizeof(float),
                         stride * pass_stride * sizeof(float));
  }

  void set_pass_from_buffer(oidn::FilterRef &oidn_filter, const char *name, OIDNPass &oidn_pass)
  {
    const int64_t width = buffer_params_.width;
    const int64_t height = buffer_params_.height;

    oidn_filter.setImage(
        name, oidn_pass.scaled_buffer.data(), oidn::Format::Float3, width, height, 0, 0, 0);
  }

  void set_pass(oidn::FilterRef &oidn_filter, OIDNPass &oidn_pass)
  {
    set_pass(oidn_filter, oidn_pass.name, oidn_pass);
  }
  void set_pass(oidn::FilterRef &oidn_filter, const char *name, OIDNPass &oidn_pass)
  {
    if (oidn_pass.scaled_buffer.empty()) {
      set_pass_referenced(oidn_filter, name, oidn_pass);
    }
    else {
      set_pass_from_buffer(oidn_filter, name, oidn_pass);
    }
  }

  void set_input_pass(oidn::FilterRef &oidn_filter, OIDNPass &oidn_pass)
  {
    set_pass_referenced(oidn_filter, oidn_pass.name, oidn_pass);
  }

  void set_guiding_passes(oidn::FilterRef &oidn_filter, OIDNPass &oidn_pass)
  {
    if (oidn_albedo_pass_) {
      if (oidn_pass.use_denoising_albedo) {
        set_pass(oidn_filter, oidn_albedo_pass_);
      }
      else {
        /* NOTE: OpenImageDenoise library implicitly expects albedo pass when normal pass has been
         * provided. */
        set_fake_albedo_pass(oidn_filter);
      }
    }

    if (oidn_normal_pass_) {
      set_pass(oidn_filter, oidn_normal_pass_);
    }
  }

  void set_fake_albedo_pass(oidn::FilterRef &oidn_filter)
  {
    const int64_t width = buffer_params_.width;
    const int64_t height = buffer_params_.height;

    if (!albedo_replaced_with_fake_) {
      const int64_t num_pixel_components = width * height * 3;
      oidn_albedo_pass_.scaled_buffer.resize(num_pixel_components);

      for (int i = 0; i < num_pixel_components; ++i) {
        oidn_albedo_pass_.scaled_buffer[i] = 0.5f;
      }

      albedo_replaced_with_fake_ = true;
    }

    set_pass(oidn_filter, oidn_albedo_pass_);
  }

  void set_output_pass(oidn::FilterRef &oidn_filter, OIDNPass &oidn_pass)
  {
    set_pass(oidn_filter, "output", oidn_pass);
  }

  void set_quality(oidn::FilterRef &oidn_filter)
  {
#  if OIDN_VERSION_MAJOR >= 2
    switch (denoise_params_.quality) {
      case DENOISER_QUALITY_FAST:
#    if OIDN_VERSION >= 20300
        oidn_filter.set("quality", OIDN_QUALITY_FAST);
        break;
#    endif
      case DENOISER_QUALITY_BALANCED:
        oidn_filter.set("quality", OIDN_QUALITY_BALANCED);
        break;
      case DENOISER_QUALITY_HIGH:
      default:
        oidn_filter.set("quality", OIDN_QUALITY_HIGH);
    }
#  endif
  }

  /* Scale output pass to match adaptive sampling per-pixel scale, as well as bring alpha channel
   * back. */
  void postprocess_output(const OIDNPass &oidn_input_pass, const OIDNPass &oidn_output_pass)
  {
    kernel_assert(oidn_input_pass.num_components == oidn_output_pass.num_components);

    const int64_t x = buffer_params_.full_x;
    const int64_t y = buffer_params_.full_y;
    const int64_t width = buffer_params_.width;
    const int64_t height = buffer_params_.height;
    const int64_t offset = buffer_params_.offset;
    const int64_t stride = buffer_params_.stride;
    const int64_t pass_stride = buffer_params_.pass_stride;
    const int64_t row_stride = stride * pass_stride;

    const int64_t pixel_offset = offset + x + y * stride;
    const int64_t buffer_offset = (pixel_offset * pass_stride);

    float *buffer_data = render_buffers_->buffer.data();

    const bool has_pass_sample_count = (pass_sample_count_ != PASS_UNUSED);
    const bool need_scale = has_pass_sample_count || oidn_input_pass.use_compositing;

    for (int y = 0; y < height; ++y) {
      float *buffer_row = buffer_data + buffer_offset + y * row_stride;
      for (int x = 0; x < width; ++x) {
        float *buffer_pixel = buffer_row + x * pass_stride;
        float *denoised_pixel = buffer_pixel + oidn_output_pass.offset;

        if (need_scale) {
          const float pixel_scale = has_pass_sample_count ?
                                        __float_as_uint(buffer_pixel[pass_sample_count_]) :
                                        num_samples_;

          denoised_pixel[0] = denoised_pixel[0] * pixel_scale;
          denoised_pixel[1] = denoised_pixel[1] * pixel_scale;
          denoised_pixel[2] = denoised_pixel[2] * pixel_scale;
        }

        if (oidn_output_pass.num_components == 3) {
          /* Pass without alpha channel. */
        }
        else if (!oidn_input_pass.use_compositing) {
          /* Currently compositing passes are either 3-component (derived by dividing light passes)
           * or do not have transparency (shadow catcher). Implicitly rely on this logic, as it
           * simplifies logic and avoids extra memory allocation. */
          const float *noisy_pixel = buffer_pixel + oidn_input_pass.offset;
          denoised_pixel[3] = noisy_pixel[3];
        }
        else {
          /* Assigning to zero since this is a default alpha value for 3-component passes, and it
           * is an opaque pixel for 4 component passes. */
          denoised_pixel[3] = 0;
        }
      }
    }
  }

  bool is_pass_scale_needed(OIDNPass &oidn_pass) const
  {
    if (pass_sample_count_ != PASS_UNUSED) {
      /* With adaptive sampling pixels will have different number of samples in them, so need to
       * always scale the pass to make pixels uniformly sampled. */
      return true;
    }

    if (!oidn_pass.need_scale) {
      return false;
    }

    if (num_samples_ == 1) {
      /* If the avoid scaling if there is only one sample, to save up time (so we don't divide
       * buffer by 1). */
      return false;
    }

    return true;
  }

  void scale_pass_in_render_buffers(OIDNPass &oidn_pass)
  {
    const int64_t x = buffer_params_.full_x;
    const int64_t y = buffer_params_.full_y;
    const int64_t width = buffer_params_.width;
    const int64_t height = buffer_params_.height;
    const int64_t offset = buffer_params_.offset;
    const int64_t stride = buffer_params_.stride;
    const int64_t pass_stride = buffer_params_.pass_stride;
    const int64_t row_stride = stride * pass_stride;

    const int64_t pixel_offset = offset + x + y * stride;
    const int64_t buffer_offset = (pixel_offset * pass_stride);

    float *buffer_data = render_buffers_->buffer.data();

    const bool has_pass_sample_count = (pass_sample_count_ != PASS_UNUSED);

    for (int y = 0; y < height; ++y) {
      float *buffer_row = buffer_data + buffer_offset + y * row_stride;
      for (int x = 0; x < width; ++x) {
        float *buffer_pixel = buffer_row + x * pass_stride;
        float *pass_pixel = buffer_pixel + oidn_pass.offset;

        const float pixel_scale = 1.0f / (has_pass_sample_count ?
                                              __float_as_uint(buffer_pixel[pass_sample_count_]) :
                                              num_samples_);

        pass_pixel[0] = pass_pixel[0] * pixel_scale;
        pass_pixel[1] = pass_pixel[1] * pixel_scale;
        pass_pixel[2] = pass_pixel[2] * pixel_scale;
      }
    }
  }

  OIDNDenoiser *denoiser_ = nullptr;

  const DenoiseParams &denoise_params_;
  const BufferParams &buffer_params_;
  RenderBuffers *render_buffers_ = nullptr;
  int num_samples_ = 0;
  bool allow_inplace_modification_ = false;
  int pass_sample_count_ = PASS_UNUSED;

  vector<uint8_t> custom_weights;

  /* Optional albedo and normal passes, reused by denoising of different pass types. */
  OIDNPass oidn_albedo_pass_;
  OIDNPass oidn_normal_pass_;

  /* For passes which don't need albedo channel for denoising we replace the actual albedo with
   * the (0.5, 0.5, 0.5). This flag indicates that the real albedo pass has been replaced with
   * the fake values and denoising of passes which do need albedo can no longer happen. */
  bool albedo_replaced_with_fake_ = false;
};

static unique_ptr<DeviceQueue> create_device_queue(const RenderBuffers *render_buffers)
{
  Device *device = render_buffers->buffer.device;
  if (device->info.has_gpu_queue) {
    return device->gpu_queue_create();
  }
  return nullptr;
}

static void copy_render_buffers_from_device(unique_ptr<DeviceQueue> &queue,
                                            RenderBuffers *render_buffers)
{
  if (queue) {
    queue->copy_from_device(render_buffers->buffer);
    queue->synchronize();
  }
  else {
    render_buffers->copy_from_device();
  }
}

static void copy_render_buffers_to_device(unique_ptr<DeviceQueue> &queue,
                                          RenderBuffers *render_buffers)
{
  if (queue) {
    queue->copy_to_device(render_buffers->buffer);
    queue->synchronize();
  }
  else {
    render_buffers->copy_to_device();
  }
}

#endif

bool OIDNDenoiser::denoise_buffer(const BufferParams &buffer_params,
                                  RenderBuffers *render_buffers,
                                  const int num_samples,
                                  bool allow_inplace_modification)
{
  DCHECK(openimagedenoise_supported())
      << "OpenImageDenoise is not supported on this platform or build.";

#ifdef WITH_OPENIMAGEDENOISE
  thread_scoped_lock lock(mutex_);

  /* Make sure the host-side data is available for denoising. */
  unique_ptr<DeviceQueue> queue = create_device_queue(render_buffers);
  copy_render_buffers_from_device(queue, render_buffers);

  OIDNDenoiseContext context(
      this, params_, buffer_params, render_buffers, num_samples, allow_inplace_modification);

  if (context.need_denoising()) {
    context.read_guiding_passes();

    const std::array<PassType, 3> passes = {
        {/* Passes which will use real albedo when it is available. */
         PASS_COMBINED,
         PASS_SHADOW_CATCHER_MATTE,

         /* Passes which do not need albedo and hence if real is present it needs to become fake.
          */
         PASS_SHADOW_CATCHER}};

    for (const PassType pass_type : passes) {
      context.denoise_pass(pass_type);
      if (is_cancelled()) {
        return false;
      }
    }

    /* TODO: It may be possible to avoid this copy, but we have to ensure that when other code
     * copies data from the device it doesn't overwrite the denoiser buffers. */
    copy_render_buffers_to_device(queue, render_buffers);
  }
#else
  (void)buffer_params;
  (void)render_buffers;
  (void)num_samples;
  (void)allow_inplace_modification;
#endif

  /* This code is not supposed to run when compiled without OIDN support, so can assume if we made
   * it up here all passes are properly denoised. */
  return true;
}

uint OIDNDenoiser::get_device_type_mask() const
{
  return DEVICE_MASK_CPU;
}

CCL_NAMESPACE_END

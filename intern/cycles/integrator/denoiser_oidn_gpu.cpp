/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if defined(WITH_OPENIMAGEDENOISE)

#  include "integrator/denoiser_oidn_gpu.h"

#  include <array>

#  include "device/device.h"
#  include "device/queue.h"
#  include "integrator/pass_accessor_cpu.h"
#  include "session/buffers.h"
#  include "util/array.h"
#  include "util/log.h"
#  include "util/openimagedenoise.h"

#  include "kernel/device/cpu/compat.h"
#  include "kernel/device/cpu/kernel.h"

#  if OIDN_VERSION_MAJOR < 2
#    define oidnSetFilterBool oidnSetFilter1b
#    define oidnSetFilterInt oidnSetFilter1i
#    define oidnExecuteFilterAsync oidnExecuteFilter
#  endif

CCL_NAMESPACE_BEGIN

/* Ideally, this would be dynamic and adaptively change when the runtime runs out of memory.  */
constexpr int prefilter_max_mem = 1024;

thread_mutex OIDNDenoiserGPU::mutex_;
bool OIDNDenoiserGPU::is_device_type_supported(const DeviceType &type)
{
  switch (type) {
#  ifdef OIDN_DEVICE_SYCL
    /* Assume all devices with Cycles support are also supported by OIDN2. */
    case DEVICE_ONEAPI:
      return true;
#  endif
    default:
      return false;
  }
}

bool OIDNDenoiserGPU::is_device_supported(const DeviceInfo &device)
{
  /* Currently falls back to checking just the device type, can be improved. */
  return is_device_type_supported(device.type);
}

OIDNDenoiserGPU::OIDNDenoiserGPU(Device *path_trace_device, const DenoiseParams &params)
    : DenoiserGPU(path_trace_device, params)
{
  DCHECK_EQ(params.type, DENOISER_OPENIMAGEDENOISE);
}

OIDNDenoiserGPU::~OIDNDenoiserGPU()
{
  if (albedo_filter_) {
    oidnReleaseFilter(albedo_filter_);
  }
  if (normal_filter_) {
    oidnReleaseFilter(normal_filter_);
  }
  if (oidn_filter_) {
    oidnReleaseFilter(oidn_filter_);
  }
  if (oidn_device_) {
    oidnReleaseDevice(oidn_device_);
  }
}

bool OIDNDenoiserGPU::denoise_buffer(const BufferParams &buffer_params,
                                     RenderBuffers *render_buffers,
                                     const int num_samples,
                                     bool allow_inplace_modification)
{
  return DenoiserGPU::denoise_buffer(
      buffer_params, render_buffers, num_samples, allow_inplace_modification);
}

uint OIDNDenoiserGPU::get_device_type_mask() const
{
  uint device_mask = 0;
#  ifdef OIDN_DEVICE_SYCL
  device_mask |= DEVICE_MASK_ONEAPI;
#  endif
  return device_mask;
}

OIDNFilter OIDNDenoiserGPU::create_filter()
{
  const char *error_message = nullptr;
  OIDNFilter filter = oidnNewFilter(oidn_device_, "RT");
  if (filter == nullptr) {
    OIDNError err = oidnGetDeviceError(oidn_device_, (const char **)&error_message);
    if (OIDN_ERROR_NONE != err) {
      LOG(ERROR) << "OIDN error: " << error_message;
      denoiser_device_->set_error(error_message);
    }
  }
  return filter;
}

bool OIDNDenoiserGPU::denoise_create_if_needed(DenoiseContext &context)
{
  const bool recreate_denoiser = (oidn_device_ == nullptr) || (oidn_filter_ == nullptr) ||
                                 (use_pass_albedo_ != context.use_pass_albedo) ||
                                 (use_pass_normal_ != context.use_pass_normal);
  if (!recreate_denoiser) {
    return true;
  }

  /* Destroy existing handle before creating new one. */
  if (oidn_filter_) {
    oidnReleaseFilter(oidn_filter_);
  }

  if (oidn_device_) {
    oidnReleaseDevice(oidn_device_);
  }

  switch (denoiser_device_->info.type) {
#  if defined(OIDN_DEVICE_SYCL)
    case DEVICE_ONEAPI:
      oidn_device_ = oidnNewDevice(OIDN_DEVICE_TYPE_SYCL);
      denoiser_queue_->init_execution();
      break;
#  endif
    default:
      break;
  }
  if (!oidn_device_) {
    denoiser_device_->set_error("Failed to create OIDN device");
    return false;
  }

  oidnCommitDevice(oidn_device_);

  oidn_filter_ = create_filter();
  if (oidn_filter_ == nullptr) {
    return false;
  }

  oidnSetFilterBool(oidn_filter_, "hdr", true);
  oidnSetFilterBool(oidn_filter_, "srgb", false);
  oidnSetFilterInt(oidn_filter_, "maxMemoryMB", max_mem_);
  if (params_.prefilter == DENOISER_PREFILTER_NONE ||
      params_.prefilter == DENOISER_PREFILTER_ACCURATE)
  {
    oidnSetFilterInt(oidn_filter_, "cleanAux", true);
  }

  if (context.use_pass_albedo) {
    albedo_filter_ = create_filter();
    if (albedo_filter_ == nullptr) {
      oidnSetFilterInt(oidn_filter_, "maxMemoryMB", prefilter_max_mem);
      return false;
    }
  }

  if (context.use_pass_normal) {
    normal_filter_ = create_filter();
    if (normal_filter_ == nullptr) {
      oidnSetFilterInt(oidn_filter_, "maxMemoryMB", prefilter_max_mem);
      return false;
    }
  }

  /* OIDN denoiser handle was created with the requested number of input passes. */
  use_pass_albedo_ = context.use_pass_albedo;
  use_pass_normal_ = context.use_pass_normal;

  /* OIDN denoiser has been created, but it needs configuration. */
  is_configured_ = false;
  return true;
}

bool OIDNDenoiserGPU::denoise_configure_if_needed(DenoiseContext &context)
{
  /* Limit maximum tile size denoiser can be invoked with. */
  const int2 size = make_int2(context.buffer_params.width, context.buffer_params.height);

  if (is_configured_ && (configured_size_.x == size.x && configured_size_.y == size.y)) {
    return true;
  }

  is_configured_ = true;
  configured_size_ = size;

  return true;
}

bool OIDNDenoiserGPU::denoise_run(const DenoiseContext &context, const DenoisePass &pass)
{
  /* Color pass. */
  const int64_t pass_stride_in_bytes = context.buffer_params.pass_stride * sizeof(float);

  oidnSetSharedFilterImage(oidn_filter_,
                           "color",
                           (void *)context.render_buffers->buffer.device_pointer,
                           OIDN_FORMAT_FLOAT3,
                           context.buffer_params.width,
                           context.buffer_params.height,
                           pass.denoised_offset * sizeof(float),
                           pass_stride_in_bytes,
                           pass_stride_in_bytes * context.buffer_params.stride);
  oidnSetSharedFilterImage(oidn_filter_,
                           "output",
                           (void *)context.render_buffers->buffer.device_pointer,
                           OIDN_FORMAT_FLOAT3,
                           context.buffer_params.width,
                           context.buffer_params.height,
                           pass.denoised_offset * sizeof(float),
                           pass_stride_in_bytes,
                           pass_stride_in_bytes * context.buffer_params.stride);

  /* Optional albedo and color passes. */
  if (context.num_input_passes > 1) {
    const device_ptr d_guiding_buffer = context.guiding_params.device_pointer;
    const int64_t pixel_stride_in_bytes = context.guiding_params.pass_stride * sizeof(float);
    const int64_t row_stride_in_bytes = context.guiding_params.stride * pixel_stride_in_bytes;

    if (context.use_pass_albedo) {
      if (params_.prefilter == DENOISER_PREFILTER_NONE) {
        oidnSetSharedFilterImage(oidn_filter_,
                                 "albedo",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_albedo * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);
      }
      else {
        oidnSetSharedFilterImage(albedo_filter_,
                                 "color",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_albedo * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);
        oidnSetSharedFilterImage(albedo_filter_,
                                 "output",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_albedo * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);
        oidnCommitFilter(albedo_filter_);
        oidnExecuteFilterAsync(albedo_filter_);

        oidnSetSharedFilterImage(oidn_filter_,
                                 "albedo",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_albedo * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);
      }
    }

    if (context.use_pass_normal) {
      if (params_.prefilter == DENOISER_PREFILTER_NONE) {
        oidnSetSharedFilterImage(oidn_filter_,
                                 "normal",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_normal * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);
      }
      else {
        oidnSetSharedFilterImage(normal_filter_,
                                 "color",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_normal * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);

        oidnSetSharedFilterImage(normal_filter_,
                                 "output",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_normal * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);

        oidnCommitFilter(normal_filter_);
        oidnExecuteFilterAsync(normal_filter_);

        oidnSetSharedFilterImage(oidn_filter_,
                                 "normal",
                                 (void *)d_guiding_buffer,
                                 OIDN_FORMAT_FLOAT3,
                                 context.buffer_params.width,
                                 context.buffer_params.height,
                                 context.guiding_params.pass_normal * sizeof(float),
                                 pixel_stride_in_bytes,
                                 row_stride_in_bytes);
      }
    }
  }

  oidnCommitFilter(oidn_filter_);
  oidnExecuteFilter(oidn_filter_);

  const char *out_message = nullptr;
  OIDNError err = oidnGetDeviceError(oidn_device_, (const char **)&out_message);
  if (OIDN_ERROR_NONE != err) {
    /* If OIDN runs out of memory, reduce mem limit and retry */
    while (err == OIDN_ERROR_OUT_OF_MEMORY && max_mem_ > 200) {
      max_mem_ = max_mem_ / 2;
      oidnSetFilterInt(oidn_filter_, "maxMemoryMB", max_mem_);
      oidnCommitFilter(oidn_filter_);
      oidnExecuteFilter(oidn_filter_);
      err = oidnGetDeviceError(oidn_device_, &out_message);
    }
    if (out_message) {
      LOG(ERROR) << "OIDN error: " << out_message;
      denoiser_device_->set_error(out_message);
    }
    else {
      LOG(ERROR) << "OIDN error: unspecified";
      denoiser_device_->set_error("Unspecified OIDN error");
    }
    return false;
  }
  return true;
}

CCL_NAMESPACE_END

#endif
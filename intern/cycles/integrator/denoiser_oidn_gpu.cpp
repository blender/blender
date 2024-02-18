/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if defined(WITH_OPENIMAGEDENOISE)

#  include "integrator/denoiser_oidn_gpu.h"

#  include <array>

#  include "device/device.h"
#  include "device/oneapi/device_impl.h"
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

bool OIDNDenoiserGPU::is_device_supported(const DeviceInfo &device)
{
  int device_type = OIDN_DEVICE_TYPE_DEFAULT;
  switch (device.type) {
#  ifdef OIDN_DEVICE_SYCL
    case DEVICE_ONEAPI:
      device_type = OIDN_DEVICE_TYPE_SYCL;
      break;
#  endif
#  ifdef OIDN_DEVICE_HIP
    case DEVICE_HIP:
      device_type = OIDN_DEVICE_TYPE_HIP;
      break;
#  endif
#  ifdef OIDN_DEVICE_CUDA
    case DEVICE_CUDA:
    case DEVICE_OPTIX:
      device_type = OIDN_DEVICE_TYPE_CUDA;
      break;
#  endif
#  ifdef OIDN_DEVICE_METAL
    case DEVICE_METAL: {
      int num_devices = oidnGetNumPhysicalDevices();
      for (int i = 0; i < num_devices; i++) {
        if (oidnGetPhysicalDeviceUInt(i, "type") == OIDN_DEVICE_TYPE_METAL) {
          const char *name = oidnGetPhysicalDeviceString(i, "name");
          if (device.id.find(name) != std::string::npos) {
            return true;
          }
        }
      }
      return false;
    }
#  endif
    case DEVICE_CPU:
      /* This is the GPU denoiser - CPU devices shouldn't end up here. */
      assert(0);
    default:
      return false;
  }

  /* Match GPUs by their PCI ID. */
  const int num_devices = oidnGetNumPhysicalDevices();
  for (int i = 0; i < num_devices; i++) {
    if (oidnGetPhysicalDeviceInt(i, "type") == device_type) {
      if (oidnGetPhysicalDeviceBool(i, "pciAddressSupported")) {
        unsigned int pci_domain = oidnGetPhysicalDeviceInt(i, "pciDomain");
        unsigned int pci_bus = oidnGetPhysicalDeviceInt(i, "pciBus");
        unsigned int pci_device = oidnGetPhysicalDeviceInt(i, "pciDevice");
        string pci_id = string_printf("%04x:%02x:%02x", pci_domain, pci_bus, pci_device);
        if (device.id.find(pci_id) != string::npos) {
          return true;
        }
      }
    }
  }

  return false;
}

OIDNDenoiserGPU::OIDNDenoiserGPU(Device *path_trace_device, const DenoiseParams &params)
    : DenoiserGPU(path_trace_device, params)
{
  DCHECK_EQ(params.type, DENOISER_OPENIMAGEDENOISE);
}

OIDNDenoiserGPU::~OIDNDenoiserGPU()
{
  release_all_resources();
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
#  ifdef OIDN_DEVICE_METAL
  device_mask |= DEVICE_MASK_METAL;
#  endif
#  ifdef OIDN_DEVICE_CUDA
  device_mask |= DEVICE_MASK_CUDA;
  device_mask |= DEVICE_MASK_OPTIX;
#  endif
#  ifdef OIDN_DEVICE_HIP
  device_mask |= DEVICE_MASK_HIP;
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

#  if OIDN_VERSION_MAJOR >= 2
  switch (quality_) {
    case DENOISER_QUALITY_BALANCED:
      oidnSetFilterInt(filter, "quality", OIDN_QUALITY_BALANCED);
      break;
    case DENOISER_QUALITY_HIGH:
    default:
      oidnSetFilterInt(filter, "quality", OIDN_QUALITY_HIGH);
  }
#  endif

  return filter;
}

bool OIDNDenoiserGPU::commit_and_execute_filter(OIDNFilter filter, ExecMode mode)
{
  const char *error_message = nullptr;
  OIDNError err = OIDN_ERROR_NONE;

  for (;;) {
    oidnCommitFilter(filter);
    if (mode == ExecMode::ASYNC) {
      oidnExecuteFilterAsync(filter);
    }
    else {
      oidnExecuteFilter(filter);
    }

    /* If OIDN runs out of memory, reduce mem limit and retry */
    err = oidnGetDeviceError(oidn_device_, (const char **)&error_message);
    if (err != OIDN_ERROR_OUT_OF_MEMORY || max_mem_ < 200) {
      break;
    }
    max_mem_ = max_mem_ / 2;
    oidnSetFilterInt(filter, "maxMemoryMB", max_mem_);
  }

  if (err != OIDN_ERROR_NONE) {
    if (error_message == nullptr) {
      error_message = "Unspecified OIDN error";
    }
    LOG(ERROR) << "OIDN error: " << error_message;
    denoiser_device_->set_error(error_message);
    return false;
  }
  return true;
}

bool OIDNDenoiserGPU::denoise_create_if_needed(DenoiseContext &context)
{
  const bool recreate_denoiser = (oidn_device_ == nullptr) || (oidn_filter_ == nullptr) ||
                                 (use_pass_albedo_ != context.use_pass_albedo) ||
                                 (use_pass_normal_ != context.use_pass_normal) ||
                                 (quality_ != params_.quality);
  if (!recreate_denoiser) {
    return true;
  }

  /* Destroy existing handles before creating new ones. */
  release_all_resources();

  switch (denoiser_device_->info.type) {
#  if defined(OIDN_DEVICE_SYCL) && defined(WITH_ONEAPI)
    case DEVICE_ONEAPI:
      oidn_device_ = oidnNewSYCLDevice(
          (const sycl::queue *)reinterpret_cast<OneapiDevice *>(denoiser_device_)->sycl_queue(),
          1);
      break;
#  endif
#  if defined(OIDN_DEVICE_METAL) && defined(WITH_METAL)
    case DEVICE_METAL: {
      denoiser_queue_->init_execution();
      const MTLCommandQueue_id queue = (const MTLCommandQueue_id)denoiser_queue_->native_queue();
      oidn_device_ = oidnNewMetalDevice(&queue, 1);
    } break;
#  endif
#  if defined(OIDN_DEVICE_CUDA) && defined(WITH_CUDA)
    case DEVICE_CUDA:
    case DEVICE_OPTIX: {
      /* Directly using the stream from the DeviceQueue returns "invalid resource handle". */
      cudaStream_t stream = nullptr;
      oidn_device_ = oidnNewCUDADevice(&denoiser_device_->info.num, &stream, 1);
      break;
    }
#  endif
#  if defined(OIDN_DEVICE_HIP) && defined(WITH_HIP)
    case DEVICE_HIP: {
      hipStream_t stream = nullptr;
      oidn_device_ = oidnNewHIPDevice(&denoiser_device_->info.num, &stream, 1);
      break;
    }
#  endif
    default:
      break;
  }

  if (!oidn_device_) {
    denoiser_device_->set_error("Failed to create OIDN device");
    return false;
  }

  if (denoiser_queue_) {
    denoiser_queue_->init_execution();
  }

  oidnCommitDevice(oidn_device_);

  quality_ = params_.quality;

  oidn_filter_ = create_filter();
  if (oidn_filter_ == nullptr) {
    return false;
  }

  oidnSetFilterBool(oidn_filter_, "hdr", true);
  oidnSetFilterBool(oidn_filter_, "srgb", false);

  if (context.use_pass_albedo) {
    albedo_filter_ = create_filter();
    if (albedo_filter_ == nullptr) {
      return false;
    }
  }

  if (context.use_pass_normal) {
    normal_filter_ = create_filter();
    if (normal_filter_ == nullptr) {
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

  set_filter_pass(oidn_filter_,
                  "color",
                  context.render_buffers->buffer.device_pointer,
                  OIDN_FORMAT_FLOAT3,
                  context.buffer_params.width,
                  context.buffer_params.height,
                  pass.denoised_offset * sizeof(float),
                  pass_stride_in_bytes,
                  pass_stride_in_bytes * context.buffer_params.stride);

  set_filter_pass(oidn_filter_,
                  "output",
                  context.render_buffers->buffer.device_pointer,
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
      set_filter_pass(oidn_filter_,
                      "albedo",
                      d_guiding_buffer,
                      OIDN_FORMAT_FLOAT3,
                      context.buffer_params.width,
                      context.buffer_params.height,
                      context.guiding_params.pass_albedo * sizeof(float),
                      pixel_stride_in_bytes,
                      row_stride_in_bytes);

      if (params_.prefilter == DENOISER_PREFILTER_ACCURATE) {
        set_filter_pass(albedo_filter_,
                        "albedo",
                        d_guiding_buffer,
                        OIDN_FORMAT_FLOAT3,
                        context.buffer_params.width,
                        context.buffer_params.height,
                        context.guiding_params.pass_albedo * sizeof(float),
                        pixel_stride_in_bytes,
                        row_stride_in_bytes);

        set_filter_pass(albedo_filter_,
                        "output",
                        d_guiding_buffer,
                        OIDN_FORMAT_FLOAT3,
                        context.buffer_params.width,
                        context.buffer_params.height,
                        context.guiding_params.pass_albedo * sizeof(float),
                        pixel_stride_in_bytes,
                        row_stride_in_bytes);

        if (!commit_and_execute_filter(albedo_filter_, ExecMode::ASYNC)) {
          return false;
        }
      }
    }

    if (context.use_pass_normal) {
      set_filter_pass(oidn_filter_,
                      "normal",
                      d_guiding_buffer,
                      OIDN_FORMAT_FLOAT3,
                      context.buffer_params.width,
                      context.buffer_params.height,
                      context.guiding_params.pass_normal * sizeof(float),
                      pixel_stride_in_bytes,
                      row_stride_in_bytes);

      if (params_.prefilter == DENOISER_PREFILTER_ACCURATE) {
        set_filter_pass(normal_filter_,
                        "normal",
                        d_guiding_buffer,
                        OIDN_FORMAT_FLOAT3,
                        context.buffer_params.width,
                        context.buffer_params.height,
                        context.guiding_params.pass_normal * sizeof(float),
                        pixel_stride_in_bytes,
                        row_stride_in_bytes);

        set_filter_pass(normal_filter_,
                        "output",
                        d_guiding_buffer,
                        OIDN_FORMAT_FLOAT3,
                        context.buffer_params.width,
                        context.buffer_params.height,
                        context.guiding_params.pass_normal * sizeof(float),
                        pixel_stride_in_bytes,
                        row_stride_in_bytes);

        if (!commit_and_execute_filter(normal_filter_, ExecMode::ASYNC)) {
          return false;
        }
      }
    }
  }

  oidnSetFilterInt(oidn_filter_, "cleanAux", params_.prefilter != DENOISER_PREFILTER_FAST);
  return commit_and_execute_filter(oidn_filter_);
}

void OIDNDenoiserGPU::set_filter_pass(OIDNFilter filter,
                                      const char *name,
                                      device_ptr ptr,
                                      int format,
                                      int width,
                                      int height,
                                      size_t offset_in_bytes,
                                      size_t pixel_stride_in_bytes,
                                      size_t row_stride_in_bytes)
{
#  if defined(OIDN_DEVICE_METAL) && defined(WITH_METAL)
  if (denoiser_device_->info.type == DEVICE_METAL) {
    void *mtl_buffer = denoiser_device_->get_native_buffer(ptr);
    OIDNBuffer oidn_buffer = oidnNewSharedBufferFromMetal(oidn_device_, mtl_buffer);

    oidnSetFilterImage(filter,
                       name,
                       oidn_buffer,
                       (OIDNFormat)format,
                       width,
                       height,
                       offset_in_bytes,
                       pixel_stride_in_bytes,
                       row_stride_in_bytes);

    oidnReleaseBuffer(oidn_buffer);
  }
  else
#  endif
  {
    oidnSetSharedFilterImage(filter,
                             name,
                             (void *)ptr,
                             (OIDNFormat)format,
                             width,
                             height,
                             offset_in_bytes,
                             pixel_stride_in_bytes,
                             row_stride_in_bytes);
  }
}

void OIDNDenoiserGPU::release_all_resources()
{
  if (albedo_filter_) {
    oidnReleaseFilter(albedo_filter_);
    albedo_filter_ = nullptr;
  }
  if (normal_filter_) {
    oidnReleaseFilter(normal_filter_);
    normal_filter_ = nullptr;
  }
  if (oidn_filter_) {
    oidnReleaseFilter(oidn_filter_);
    oidn_filter_ = nullptr;
  }
  if (oidn_device_) {
    oidnReleaseDevice(oidn_device_);
    oidn_device_ = nullptr;
  }
}

CCL_NAMESPACE_END

#endif

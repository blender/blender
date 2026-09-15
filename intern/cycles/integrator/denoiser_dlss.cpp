/* SPDX-FileCopyrightText: 2025 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_DLSS

#  include "integrator/denoiser_dlss.h"
#  include "integrator/pass_accessor_gpu.h"

#  include "device/cuda/device_impl.h"

#  include "util/path.h"

#  define NVSDK_NGX_HEADER_ONLY
#  include <nvsdk_ngx.h>
#  include <nvsdk_ngx_defs.h>
#  include <nvsdk_ngx_defs_dlssd.h>

CCL_NAMESPACE_BEGIN

/* Application ID for Blender from NVIDIA. */
static const int NGX_APPLICATION_ID = 100334311;

void DLSSDenoiser::CUDATexture::init(Device *device, int width, int height, int num_components)
{
  CUDA_ARRAY_DESCRIPTOR desc = {};
  desc.Width = width;
  desc.Height = height;
  desc.Format = CU_AD_FORMAT_FLOAT;
  desc.NumChannels = num_components;

  cuda_device_assert(device, cuArrayCreate((CUarray *)&array, &desc));

  CUDA_TEXTURE_DESC tex_desc = {};
  tex_desc.addressMode[0] = CU_TR_ADDRESS_MODE_CLAMP;
  tex_desc.addressMode[1] = CU_TR_ADDRESS_MODE_CLAMP;
  tex_desc.addressMode[2] = CU_TR_ADDRESS_MODE_CLAMP;
  tex_desc.flags = CU_TRSF_NORMALIZED_COORDINATES;

  CUDA_RESOURCE_DESC res_desc = {};
  res_desc.resType = CU_RESOURCE_TYPE_ARRAY;
  res_desc.res.array.hArray = (CUarray)array;

  cuda_device_assert(
      device, cuTexObjectCreate((CUtexObject *)&texture_handle, &res_desc, &tex_desc, nullptr));
  cuda_device_assert(device, cuSurfObjectCreate((CUsurfObject *)&surface_handle, &res_desc));
}
void DLSSDenoiser::CUDATexture::destroy()
{
  cuSurfObjectDestroy((CUsurfObject)surface_handle);
  surface_handle = 0;
  cuTexObjectDestroy((CUtexObject)texture_handle);
  texture_handle = 0;

  cuArrayDestroy((CUarray)array);
  array = 0;
}

DLSSDenoiser::DLSSDenoiser(Device *denoiser_device, const DenoiseParams &params)
    : DenoiserGPU(denoiser_device, params)
{
  CUDADevice *const cuda_device = static_cast<CUDADevice *>(denoiser_device_);
  const CUDAContextScope scope(cuda_device);

  /* Path to optionally search for features, in addition to the executable directory and driver. */
  const wstring app_path = string_to_wstring(path_get());
  /* Path to write NGX logs to. */
  const wstring app_data_path = string_to_wstring(path_cache_get());

  const wchar_t *const app_paths[] = {app_path.c_str()};

  NVSDK_NGX_FeatureCommonInfo feature_info = {};
  feature_info.PathListInfo.Path = app_paths;
  feature_info.PathListInfo.Length = 1;
  feature_info.LoggingInfo.LoggingCallback =
      [](const char *message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature) {
        switch (loggingLevel) {
          case NVSDK_NGX_LOGGING_LEVEL_OFF:
          case NVSDK_NGX_LOGGING_LEVEL_NUM:
            assert(false);
            break;
          case NVSDK_NGX_LOGGING_LEVEL_ON:
            LOG_INFO << message;
            break;
          case NVSDK_NGX_LOGGING_LEVEL_VERBOSE:
            LOG_INFO << message;
            break;
        }
      };
  feature_info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;

  /* Create a fixed handle for this particular CUDA context and queue combination, since NGX uses
   * it for lookup internally. */
  ngx_device_ = new NVSDK_NGX_CUDADevice{
      cuda_device->cuContext, static_cast<CUDADeviceQueue *>(denoiser_queue_.get())->stream()};

  const NVSDK_NGX_Result result = NVSDK_NGX_CUDA_Init1(
      NGX_APPLICATION_ID, app_data_path.c_str(), ngx_device_, &feature_info);

  if (result == NVSDK_NGX_Result_FAIL_FeatureNotSupported) {
    delete ngx_device_;
    ngx_device_ = nullptr;
    set_error("Failed to load NGX driver");
  }
  else if (NVSDK_NGX_FAILED(result)) {
    set_error("Failed to initialize NGX driver");
  }
}

DLSSDenoiser::~DLSSDenoiser()
{
  CUDADevice *const cuda_device = static_cast<CUDADevice *>(denoiser_device_);
  const CUDAContextScope scope(cuda_device);

  tex_color_.destroy();
  tex_depth_.destroy();
  tex_diffuse_albedo_.destroy();
  tex_specular_albedo_.destroy();
  tex_normal_roughness_.destroy();
  tex_motion_.destroy();
  tex_specular_motion_.destroy();
  tex_output_.destroy();

  if (ngx_device_ == nullptr) {
    return;
  }

  if (handle_ != nullptr) {
    NVSDK_NGX_CUDA_ReleaseFeature(handle_);
  }

  const NVSDK_NGX_Result result = NVSDK_NGX_CUDA_Shutdown1(ngx_device_);

  if (NVSDK_NGX_FAILED(result)) {
    set_error("Failed to shutdown NGX driver");
  }

  delete ngx_device_;
}

bool DLSSDenoiser::is_device_supported(const DeviceInfo &device)
{
  if (device.type != DEVICE_CUDA && device.type != DEVICE_OPTIX) {
    return false;
  }

  /* 'NVSDK_NGX_CUDA_GetFeatureRequirements' is an expensive call, so cache the result (since
   * 'is_device_supported' is called a lot). */
  static ccl::map<int, NVSDK_NGX_Feature_Support_Result> supported_cache;
  if (const auto it = supported_cache.find(device.num); it != supported_cache.end()) {
    return it->second == NVSDK_NGX_FeatureSupportResult_Supported;
  }

  /* Path to write NGX logs to. */
  const wstring app_data_path = string_to_wstring(path_cache_get());

  CUdevice cuDevice = 0;
  cuDeviceGet(&cuDevice, device.num);

  /* Query driver whether the DLSS-RR feature is supported on this particular device. */
  NVSDK_NGX_FeatureDiscoveryInfo discovery_info = {};
  discovery_info.SDKVersion = NVSDK_NGX_Version_API;
  discovery_info.FeatureID = NVSDK_NGX_Feature_RayReconstruction;
  discovery_info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
  discovery_info.Identifier.v.ApplicationId = NGX_APPLICATION_ID;
  discovery_info.ApplicationDataPath = app_data_path.c_str();

  NVSDK_NGX_FeatureRequirement requirement = {NVSDK_NGX_FeatureSupportResult_Supported};

  const NVSDK_NGX_Result result = NVSDK_NGX_CUDA_GetFeatureRequirements(
      cuDevice, &discovery_info, &requirement);

  if (NVSDK_NGX_SUCCEED(result)) {
    supported_cache.emplace(device.num, requirement.FeatureSupported);
    return requirement.FeatureSupported == NVSDK_NGX_FeatureSupportResult_Supported;
  }
  else {
    return false;
  }
}

bool DLSSDenoiser::denoise_create_if_needed(DenoiseContext &context)
{
  const bool recreate_denoiser = last_width_ != context.denoised_buffer_params.width ||
                                 last_height_ != context.denoised_buffer_params.height ||
                                 last_upscale_factor_ != context.denoise_params.upscale_factor;
  if (handle_ != nullptr && !recreate_denoiser) {
    return true;
  }

  CUDADevice *const cuda_device = static_cast<CUDADevice *>(denoiser_device_);
  const CUDAContextScope scope(cuda_device);

  if (handle_ != nullptr) {
    denoiser_queue_->synchronize();

    NVSDK_NGX_CUDA_ReleaseFeature(handle_);
    handle_ = nullptr;
  }

  tex_color_.destroy();
  tex_depth_.destroy();
  tex_diffuse_albedo_.destroy();
  tex_specular_albedo_.destroy();
  tex_normal_roughness_.destroy();
  tex_motion_.destroy();
  tex_specular_motion_.destroy();
  tex_output_.destroy();

  /* Feature creation fails below these dimensions.
   * Avoid hard error that stops rendering and only disable denoising for very small viewports. */
  if (context.denoised_buffer_params.width < 32 || context.denoised_buffer_params.height < 32) {
    last_width_ = 0;
    last_height_ = 0;
    return false;
  }

  NVSDK_NGX_Parameter *params = nullptr;
  if (NVSDK_NGX_FAILED(NVSDK_NGX_CUDA_AllocateParameters(&params))) {
    return false;
  }

  /* Input params (with resolution divider applied). */
  params->Set(NVSDK_NGX_Parameter_Width, context.buffer_params.width);
  params->Set(NVSDK_NGX_Parameter_Height, context.buffer_params.height);
  /* Output params. */
  params->Set(NVSDK_NGX_Parameter_OutWidth, context.denoised_buffer_params.width);
  params->Set(NVSDK_NGX_Parameter_OutHeight, context.denoised_buffer_params.height);

  /* Usually the DLSS quality mode is set first and then DLSS should be queried for the optimal
   * upscale factor to go along with it. In Cycles the upscale factor is already determined before
   * the denoiser is initialized though, so we do it backwards and instead try to find a
   * reasonable quality mode to match the denoiser settings here. */
  NVSDK_NGX_PerfQuality_Value perf_quality_value = NVSDK_NGX_PerfQuality_Value_DLAA;
  if (context.denoise_params.upscale_factor > 1) {
    switch (context.denoise_params.quality) {
      default:
      case DENOISER_QUALITY_HIGH:
        perf_quality_value = NVSDK_NGX_PerfQuality_Value_MaxQuality;
        break;
      case DENOISER_QUALITY_BALANCED:
        perf_quality_value = NVSDK_NGX_PerfQuality_Value_Balanced;
        break;
      case DENOISER_QUALITY_FAST:
        perf_quality_value = NVSDK_NGX_PerfQuality_Value_MaxPerf;
        if (context.denoise_params.upscale_factor >= 3) {
          perf_quality_value = NVSDK_NGX_PerfQuality_Value_UltraPerformance;
        }
        break;
    }
  }
  params->Set(NVSDK_NGX_Parameter_PerfQualityValue, perf_quality_value);

  params->Set(NVSDK_NGX_Parameter_DLSS_Denoise_Mode, NVSDK_NGX_DLSS_Denoise_Mode_DLUnified);
  params->Set(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags,
              NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes);
  params->Set(NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, 0);
  params->Set(NVSDK_NGX_Parameter_Use_HW_Depth, NVSDK_NGX_DLSS_Depth_Type_Linear);
  /* Normals and roughness are packed into one texture in 'denoise_filter_guiding_preprocess'. */
  params->Set(NVSDK_NGX_Parameter_DLSS_Roughness_Mode, NVSDK_NGX_DLSS_Roughness_Mode_Packed);

  const NVSDK_NGX_Result result = NVSDK_NGX_CUDA_CreateFeature1(
      ngx_device_, NVSDK_NGX_Feature_RayReconstruction, params, &handle_);

  NVSDK_NGX_CUDA_DestroyParameters(params);

  if (NVSDK_NGX_FAILED(result)) {
    set_error("Failed to create DLSS instance");
    return false;
  }

  /* DLSS requires inputs and outputs in separate CUDA textures/surfaces, while Cycles stores them
   * in an interleaved buffer, so need to create these temporary textures and convert between the
   * two storage variants (in 'denoise_filter_color_preprocess',
   * 'denoise_filter_guiding_preprocess' and 'denoise_filter_color_postprocess'). */
  tex_color_.init(cuda_device, context.buffer_params.width, context.buffer_params.height, 4);
  tex_depth_.init(cuda_device, context.buffer_params.width, context.buffer_params.height, 1);
  tex_diffuse_albedo_.init(
      cuda_device, context.buffer_params.width, context.buffer_params.height, 4);
  tex_specular_albedo_.init(
      cuda_device, context.buffer_params.width, context.buffer_params.height, 4);
  tex_normal_roughness_.init(
      cuda_device, context.buffer_params.width, context.buffer_params.height, 4);
  tex_motion_.init(cuda_device, context.buffer_params.width, context.buffer_params.height, 2);
  tex_specular_motion_.init(
      cuda_device, context.buffer_params.width, context.buffer_params.height, 2);

  tex_output_.init(
      cuda_device, context.denoised_buffer_params.width, context.denoised_buffer_params.height, 4);

  last_width_ = context.denoised_buffer_params.width;
  last_height_ = context.denoised_buffer_params.height;
  last_upscale_factor_ = context.denoise_params.upscale_factor;

  return !cuda_device->have_error();
}

bool DLSSDenoiser::denoise_configure_if_needed(DenoiseContext & /*context*/)
{
  return true;
}

bool DLSSDenoiser::denoise_filter_color_preprocess(const DenoiseContext &context,
                                                   const DenoisePass &pass)
{
  if (pass.type != PASS_COMBINED) {
    return true;
  }

  /* Input params (with resolution divider applied). */
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  const DeviceKernelArguments args(&tex_color_.surface_handle,
                                   &context.render_buffers->buffer.device_pointer,
                                   &buffer_params.full_x,
                                   &buffer_params.full_y,
                                   &buffer_params.width,
                                   &buffer_params.height,
                                   &buffer_params.offset,
                                   &buffer_params.stride,
                                   &buffer_params.pass_stride,
                                   &pass.denoised_offset);

  return denoiser_queue_->enqueue(
      DEVICE_KERNEL_FILTER_COLOR_PREPROCESS_TO_SURFACE, work_size, args);
}
bool DLSSDenoiser::denoise_filter_color_postprocess(const DenoiseContext &context,
                                                    const DenoisePass &pass)
{
  if (pass.type != PASS_COMBINED) {
    return true;
  }

  /* Output params. */
  const BufferParams &buffer_params = context.denoised_buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  const DeviceKernelArguments args(&tex_output_.surface_handle,
                                   &context.render_buffers->buffer.device_pointer,
                                   &buffer_params.full_x,
                                   &buffer_params.full_y,
                                   &buffer_params.width,
                                   &buffer_params.height,
                                   &buffer_params.offset,
                                   &buffer_params.stride,
                                   &context.buffer_params.full_x,
                                   &context.buffer_params.full_y,
                                   &context.buffer_params.offset,
                                   &context.buffer_params.stride,
                                   &buffer_params.pass_stride,
                                   &context.num_samples,
                                   &pass.noisy_offset,
                                   &pass.denoised_offset,
                                   &context.pass_sample_count,
                                   &pass.num_components,
                                   &pass.use_compositing,
                                   &params_.upscale_factor);

  return denoiser_queue_->enqueue(
      DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS_FROM_SURFACE, work_size, args);
}

bool DLSSDenoiser::denoise_filter_guiding_preprocess(DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  const int pass_depth = context.buffer_params.get_pass_offset(PASS_DENOISING_DEPTH);
  const int pass_specular_albedo = context.buffer_params.get_pass_offset(
      PASS_DENOISING_SPECULAR_ALBEDO);
  const int pass_roughness = context.buffer_params.get_pass_offset(PASS_DENOISING_ROUGHNESS);
  const int pass_specular_motion = context.buffer_params.get_pass_offset(
      PASS_DENOISING_SPECULAR_MOTION);

  const DeviceKernelArguments args(&tex_depth_.surface_handle,
                                   &tex_diffuse_albedo_.surface_handle,
                                   &tex_specular_albedo_.surface_handle,
                                   &tex_normal_roughness_.surface_handle,
                                   &tex_motion_.surface_handle,
                                   &tex_specular_motion_.surface_handle,
                                   &context.render_buffers->buffer.device_pointer,
                                   &buffer_params.offset,
                                   &buffer_params.stride,
                                   &buffer_params.pass_stride,
                                   &context.pass_sample_count,
                                   &pass_depth,
                                   &context.pass_denoising_albedo,
                                   &pass_specular_albedo,
                                   &context.pass_denoising_normal,
                                   &pass_roughness,
                                   &context.pass_motion,
                                   &pass_specular_motion,
                                   &buffer_params.full_x,
                                   &buffer_params.full_y,
                                   &buffer_params.width,
                                   &buffer_params.height,
                                   &context.num_samples);

  return denoiser_queue_->enqueue(
      DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS_TO_SURFACE, work_size, args);
}

bool DLSSDenoiser::denoise_run(const DenoiseContext &context, const DenoisePass &pass)
{
  if (pass.type != PASS_COMBINED) {
    return true;
  }

  NVSDK_NGX_Parameter *params = nullptr;
  if (NVSDK_NGX_FAILED(NVSDK_NGX_CUDA_AllocateParameters(&params))) {
    return false;
  }

  CUDADevice *const cuda_device = static_cast<CUDADevice *>(denoiser_device_);
  const CUDAContextScope scope(cuda_device);

  params->Set(NVSDK_NGX_Parameter_Reset, 0);

  params->Set(NVSDK_NGX_Parameter_Jitter_Offset_X, context.pixel_jitter.x);
  params->Set(NVSDK_NGX_Parameter_Jitter_Offset_Y, context.pixel_jitter.y);

  params->Set(NVSDK_NGX_Parameter_Color, &tex_color_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_Depth, &tex_depth_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_DiffuseAlbedo, &tex_diffuse_albedo_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_SpecularAlbedo, &tex_specular_albedo_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_GBuffer_Normals, &tex_normal_roughness_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_GBuffer_Roughness, &tex_normal_roughness_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_MotionVectors, &tex_motion_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_GBuffer_SpecularMvec, &tex_specular_motion_.texture_handle);
  params->Set(NVSDK_NGX_Parameter_Output, &tex_output_.surface_handle);

  params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width,
              context.buffer_params.width);
  params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height,
              context.buffer_params.height);

  params->Set(NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis, 1);

  const NVSDK_NGX_Result result = NVSDK_NGX_CUDA_EvaluateFeature(handle_, params, nullptr);

  NVSDK_NGX_CUDA_DestroyParameters(params);

  return NVSDK_NGX_SUCCEED(result);
}

CCL_NAMESPACE_END

#endif

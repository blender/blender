/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPTIX

#  include "integrator/denoiser_optix.h"
#  include "integrator/pass_accessor_gpu.h"

#  include "device/optix/device_impl.h"
#  include "device/optix/queue.h"

#  include <optix_denoiser_tiling.h>

CCL_NAMESPACE_BEGIN

OptiXDenoiser::OptiXDenoiser(Device *denoiser_device, const DenoiseParams &params)
    : DenoiserGPU(denoiser_device, params), state_(denoiser_device, "__denoiser_state", true)
{
}

OptiXDenoiser::~OptiXDenoiser()
{
  /* It is important that the OptixDenoiser handle is destroyed before the OptixDeviceContext
   * handle, which is guaranteed since the local denoising device owning the OptiX device context
   * is deleted as part of the Denoiser class destructor call after this. */
  if (optix_denoiser_ != nullptr) {
    optixDenoiserDestroy(optix_denoiser_);
  }
}

uint OptiXDenoiser::get_device_type_mask() const
{
  return DEVICE_MASK_OPTIX;
}

bool OptiXDenoiser::is_device_supported(const DeviceInfo &device)
{
  if (device.type == DEVICE_OPTIX) {
    return device.denoisers & DENOISER_OPTIX;
  }
  return false;
}

bool OptiXDenoiser::denoise_buffer(const DenoiseTask &task)
{
  OptiXDevice *const optix_device = static_cast<OptiXDevice *>(denoiser_device_);

  const CUDAContextScope scope(optix_device);

  return DenoiserGPU::denoise_buffer(task);
}

bool OptiXDenoiser::denoise_create_if_needed(DenoiseContext &context)
{
  const bool recreate_denoiser = (optix_denoiser_ == nullptr) ||
                                 (use_pass_albedo_ != context.use_pass_albedo) ||
                                 (use_pass_normal_ != context.use_pass_normal) ||
                                 (use_pass_motion_ != context.use_pass_motion);
  if (!recreate_denoiser) {
    return true;
  }

  /* Destroy existing handle before creating new one. */
  if (optix_denoiser_) {
    optixDenoiserDestroy(optix_denoiser_);
  }

  /* Create OptiX denoiser handle on demand when it is first used. */
  OptixDenoiserOptions denoiser_options = {};
  denoiser_options.guideAlbedo = context.use_pass_albedo;
  denoiser_options.guideNormal = context.use_pass_normal;

  OptixDenoiserModelKind model = OPTIX_DENOISER_MODEL_KIND_AOV;
  if (context.use_pass_motion) {
    model = OPTIX_DENOISER_MODEL_KIND_TEMPORAL;
  }

  const OptixResult result = optixDenoiserCreate(
      static_cast<OptiXDevice *>(denoiser_device_)->context,
      model,
      &denoiser_options,
      &optix_denoiser_);

  if (result != OPTIX_SUCCESS) {
    set_error("Failed to create OptiX denoiser");
    return false;
  }

  /* OptiX denoiser handle was created with the requested number of input passes. */
  use_pass_albedo_ = context.use_pass_albedo;
  use_pass_normal_ = context.use_pass_normal;
  use_pass_motion_ = context.use_pass_motion;

  /* OptiX denoiser has been created, but it needs configuration. */
  is_configured_ = false;

  return true;
}

bool OptiXDenoiser::denoise_configure_if_needed(DenoiseContext &context)
{
  /* Limit maximum tile size denoiser can be invoked with. */
  const int2 tile_size = make_int2(min(context.buffer_params.width, 4096),
                                   min(context.buffer_params.height, 4096));

  if (is_configured_ && (configured_size_.x == tile_size.x && configured_size_.y == tile_size.y)) {
    return true;
  }

  optix_device_assert(
      denoiser_device_,
      optixDenoiserComputeMemoryResources(optix_denoiser_, tile_size.x, tile_size.y, &sizes_));

  const bool tiled = tile_size.x < context.buffer_params.width ||
                     tile_size.y < context.buffer_params.height;

  /* Allocate denoiser state if tile size has changed since last setup. */
  state_.device = denoiser_device_;
  state_.alloc_to_device(sizes_.stateSizeInBytes + sizes_.withOverlapScratchSizeInBytes);

  /* Initialize denoiser state for the current tile size. */
  const OptixResult result = optixDenoiserSetup(
      optix_denoiser_,
      0, /* Work around bug in r495 drivers that causes artifacts when denoiser setup is called
          * on a stream that is not the default stream. */
      tile_size.x + (tiled ? sizes_.overlapWindowSizeInPixels * 2 : 0),
      tile_size.y + (tiled ? sizes_.overlapWindowSizeInPixels * 2 : 0),
      state_.device_pointer,
      sizes_.stateSizeInBytes,
      state_.device_pointer + sizes_.stateSizeInBytes,
      sizes_.withOverlapScratchSizeInBytes);
  if (result != OPTIX_SUCCESS) {
    set_error("Failed to set up OptiX denoiser");
    return false;
  }

  cuda_device_assert(denoiser_device_, cuCtxSynchronize());

  is_configured_ = true;
  configured_size_ = tile_size;

  return true;
}

bool OptiXDenoiser::denoise_run(const DenoiseContext &context, const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;
  const int width = buffer_params.width;
  const int height = buffer_params.height;

  /* Set up input and output layer information. */
  OptixImage2D color_layer = {0};
  OptixImage2D albedo_layer = {0};
  OptixImage2D normal_layer = {0};
  OptixImage2D flow_layer = {0};

  OptixImage2D output_layer = {0};
  OptixImage2D prev_output_layer = {0};

  /* Color pass. */
  {
    const int pass_denoised = pass.denoised_offset;
    const int64_t pass_stride_in_bytes = context.buffer_params.pass_stride * sizeof(float);

    color_layer.data = context.render_buffers->buffer.device_pointer +
                       pass_denoised * sizeof(float);
    color_layer.width = width;
    color_layer.height = height;
    color_layer.rowStrideInBytes = pass_stride_in_bytes * context.buffer_params.stride;
    color_layer.pixelStrideInBytes = pass_stride_in_bytes;
    color_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
  }

  /* Previous output. */
  if (context.prev_output.offset != PASS_UNUSED) {
    const int64_t pass_stride_in_bytes = context.prev_output.pass_stride * sizeof(float);

    prev_output_layer.data = context.prev_output.device_pointer +
                             context.prev_output.offset * sizeof(float);
    prev_output_layer.width = width;
    prev_output_layer.height = height;
    prev_output_layer.rowStrideInBytes = pass_stride_in_bytes * context.prev_output.stride;
    prev_output_layer.pixelStrideInBytes = pass_stride_in_bytes;
    prev_output_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
  }

  /* Optional albedo and color passes. */
  if (context.num_input_passes > 1) {
    const device_ptr d_guiding_buffer = context.guiding_params.device_pointer;
    const int64_t pixel_stride_in_bytes = context.guiding_params.pass_stride * sizeof(float);
    const int64_t row_stride_in_bytes = context.guiding_params.stride * pixel_stride_in_bytes;

    if (context.use_pass_albedo) {
      albedo_layer.data = d_guiding_buffer + context.guiding_params.pass_albedo * sizeof(float);
      albedo_layer.width = width;
      albedo_layer.height = height;
      albedo_layer.rowStrideInBytes = row_stride_in_bytes;
      albedo_layer.pixelStrideInBytes = pixel_stride_in_bytes;
      albedo_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    if (context.use_pass_normal) {
      normal_layer.data = d_guiding_buffer + context.guiding_params.pass_normal * sizeof(float);
      normal_layer.width = width;
      normal_layer.height = height;
      normal_layer.rowStrideInBytes = row_stride_in_bytes;
      normal_layer.pixelStrideInBytes = pixel_stride_in_bytes;
      normal_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    if (context.use_pass_motion) {
      flow_layer.data = d_guiding_buffer + context.guiding_params.pass_flow * sizeof(float);
      flow_layer.width = width;
      flow_layer.height = height;
      flow_layer.rowStrideInBytes = row_stride_in_bytes;
      flow_layer.pixelStrideInBytes = pixel_stride_in_bytes;
      flow_layer.format = OPTIX_PIXEL_FORMAT_FLOAT2;
    }
  }

  /* Denoise in-place of the noisy input in the render buffers. */
  output_layer = color_layer;

  OptixDenoiserGuideLayer guide_layers = {};
  guide_layers.albedo = albedo_layer;
  guide_layers.normal = normal_layer;
  guide_layers.flow = flow_layer;

  OptixDenoiserLayer image_layers = {};
  image_layers.input = color_layer;
  image_layers.previousOutput = prev_output_layer;
  image_layers.output = output_layer;

  /* Finally run denoising. */
  OptixDenoiserParams params = {}; /* All parameters are disabled/zero. */

  optix_device_assert(denoiser_device_,
                      optixUtilDenoiserInvokeTiled(
                          optix_denoiser_,
                          static_cast<OptiXDeviceQueue *>(denoiser_queue_.get())->stream(),
                          &params,
                          state_.device_pointer,
                          sizes_.stateSizeInBytes,
                          &guide_layers,
                          &image_layers,
                          1,
                          state_.device_pointer + sizes_.stateSizeInBytes,
                          sizes_.withOverlapScratchSizeInBytes,
                          sizes_.overlapWindowSizeInPixels,
                          configured_size_.x,
                          configured_size_.y));

  return true;
}

CCL_NAMESPACE_END

#endif

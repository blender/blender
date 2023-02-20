/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifdef WITH_OPTIX

#  include "integrator/denoiser_optix.h"
#  include "integrator/pass_accessor_gpu.h"

#  include "device/optix/device_impl.h"
#  include "device/optix/queue.h"

#  include <optix_denoiser_tiling.h>

CCL_NAMESPACE_BEGIN

#  if OPTIX_ABI_VERSION >= 60
using ::optixUtilDenoiserInvokeTiled;
#  else
// A minimal copy of functionality `optix_denoiser_tiling.h` which allows to fix integer overflow
// issues without bumping SDK or driver requirement.
//
// The original code is Copyright NVIDIA Corporation, BSD-3-Clause.
static OptixResult optixUtilDenoiserSplitImage(const OptixImage2D &input,
                                               const OptixImage2D &output,
                                               unsigned int overlapWindowSizeInPixels,
                                               unsigned int tileWidth,
                                               unsigned int tileHeight,
                                               std::vector<OptixUtilDenoiserImageTile> &tiles)
{
  if (tileWidth == 0 || tileHeight == 0)
    return OPTIX_ERROR_INVALID_VALUE;

  unsigned int inPixelStride = optixUtilGetPixelStride(input);
  unsigned int outPixelStride = optixUtilGetPixelStride(output);

  int inp_w = std::min(tileWidth + 2 * overlapWindowSizeInPixels, input.width);
  int inp_h = std::min(tileHeight + 2 * overlapWindowSizeInPixels, input.height);
  int inp_y = 0, copied_y = 0;

  do {
    int inputOffsetY = inp_y == 0 ? 0 :
                                    std::max((int)overlapWindowSizeInPixels,
                                             inp_h - ((int)input.height - inp_y));
    int copy_y = inp_y == 0 ? std::min(input.height, tileHeight + overlapWindowSizeInPixels) :
                              std::min(tileHeight, input.height - copied_y);

    int inp_x = 0, copied_x = 0;
    do {
      int inputOffsetX = inp_x == 0 ? 0 :
                                      std::max((int)overlapWindowSizeInPixels,
                                               inp_w - ((int)input.width - inp_x));
      int copy_x = inp_x == 0 ? std::min(input.width, tileWidth + overlapWindowSizeInPixels) :
                                std::min(tileWidth, input.width - copied_x);

      OptixUtilDenoiserImageTile tile;
      tile.input.data = input.data + (size_t)(inp_y - inputOffsetY) * input.rowStrideInBytes +
                        +(size_t)(inp_x - inputOffsetX) * inPixelStride;
      tile.input.width = inp_w;
      tile.input.height = inp_h;
      tile.input.rowStrideInBytes = input.rowStrideInBytes;
      tile.input.pixelStrideInBytes = input.pixelStrideInBytes;
      tile.input.format = input.format;

      tile.output.data = output.data + (size_t)inp_y * output.rowStrideInBytes +
                         (size_t)inp_x * outPixelStride;
      tile.output.width = copy_x;
      tile.output.height = copy_y;
      tile.output.rowStrideInBytes = output.rowStrideInBytes;
      tile.output.pixelStrideInBytes = output.pixelStrideInBytes;
      tile.output.format = output.format;

      tile.inputOffsetX = inputOffsetX;
      tile.inputOffsetY = inputOffsetY;
      tiles.push_back(tile);

      inp_x += inp_x == 0 ? tileWidth + overlapWindowSizeInPixels : tileWidth;
      copied_x += copy_x;
    } while (inp_x < static_cast<int>(input.width));

    inp_y += inp_y == 0 ? tileHeight + overlapWindowSizeInPixels : tileHeight;
    copied_y += copy_y;
  } while (inp_y < static_cast<int>(input.height));

  return OPTIX_SUCCESS;
}

static OptixResult optixUtilDenoiserInvokeTiled(OptixDenoiser denoiser,
                                                CUstream stream,
                                                const OptixDenoiserParams *params,
                                                CUdeviceptr denoiserState,
                                                size_t denoiserStateSizeInBytes,
                                                const OptixDenoiserGuideLayer *guideLayer,
                                                const OptixDenoiserLayer *layers,
                                                unsigned int numLayers,
                                                CUdeviceptr scratch,
                                                size_t scratchSizeInBytes,
                                                unsigned int overlapWindowSizeInPixels,
                                                unsigned int tileWidth,
                                                unsigned int tileHeight)
{
  if (!guideLayer || !layers)
    return OPTIX_ERROR_INVALID_VALUE;

  std::vector<std::vector<OptixUtilDenoiserImageTile>> tiles(numLayers);
  std::vector<std::vector<OptixUtilDenoiserImageTile>> prevTiles(numLayers);
  for (unsigned int l = 0; l < numLayers; l++) {
    if (const OptixResult res = ccl::optixUtilDenoiserSplitImage(layers[l].input,
                                                                 layers[l].output,
                                                                 overlapWindowSizeInPixels,
                                                                 tileWidth,
                                                                 tileHeight,
                                                                 tiles[l]))
      return res;

    if (layers[l].previousOutput.data) {
      OptixImage2D dummyOutput = layers[l].previousOutput;
      if (const OptixResult res = ccl::optixUtilDenoiserSplitImage(layers[l].previousOutput,
                                                                   dummyOutput,
                                                                   overlapWindowSizeInPixels,
                                                                   tileWidth,
                                                                   tileHeight,
                                                                   prevTiles[l]))
        return res;
    }
  }

  std::vector<OptixUtilDenoiserImageTile> albedoTiles;
  if (guideLayer->albedo.data) {
    OptixImage2D dummyOutput = guideLayer->albedo;
    if (const OptixResult res = ccl::optixUtilDenoiserSplitImage(guideLayer->albedo,
                                                                 dummyOutput,
                                                                 overlapWindowSizeInPixels,
                                                                 tileWidth,
                                                                 tileHeight,
                                                                 albedoTiles))
      return res;
  }

  std::vector<OptixUtilDenoiserImageTile> normalTiles;
  if (guideLayer->normal.data) {
    OptixImage2D dummyOutput = guideLayer->normal;
    if (const OptixResult res = ccl::optixUtilDenoiserSplitImage(guideLayer->normal,
                                                                 dummyOutput,
                                                                 overlapWindowSizeInPixels,
                                                                 tileWidth,
                                                                 tileHeight,
                                                                 normalTiles))
      return res;
  }
  std::vector<OptixUtilDenoiserImageTile> flowTiles;
  if (guideLayer->flow.data) {
    OptixImage2D dummyOutput = guideLayer->flow;
    if (const OptixResult res = ccl::optixUtilDenoiserSplitImage(guideLayer->flow,
                                                                 dummyOutput,
                                                                 overlapWindowSizeInPixels,
                                                                 tileWidth,
                                                                 tileHeight,
                                                                 flowTiles))
      return res;
  }

  for (size_t t = 0; t < tiles[0].size(); t++) {
    std::vector<OptixDenoiserLayer> tlayers;
    for (unsigned int l = 0; l < numLayers; l++) {
      OptixDenoiserLayer layer = {};
      layer.input = (tiles[l])[t].input;
      layer.output = (tiles[l])[t].output;
      if (layers[l].previousOutput.data)
        layer.previousOutput = (prevTiles[l])[t].input;
      tlayers.push_back(layer);
    }

    OptixDenoiserGuideLayer gl = {};
    if (guideLayer->albedo.data)
      gl.albedo = albedoTiles[t].input;

    if (guideLayer->normal.data)
      gl.normal = normalTiles[t].input;

    if (guideLayer->flow.data)
      gl.flow = flowTiles[t].input;

    if (const OptixResult res = optixDenoiserInvoke(denoiser,
                                                    stream,
                                                    params,
                                                    denoiserState,
                                                    denoiserStateSizeInBytes,
                                                    &gl,
                                                    &tlayers[0],
                                                    numLayers,
                                                    (tiles[0])[t].inputOffsetX,
                                                    (tiles[0])[t].inputOffsetY,
                                                    scratch,
                                                    scratchSizeInBytes))
      return res;
  }
  return OPTIX_SUCCESS;
}
#  endif

OptiXDenoiser::OptiXDenoiser(Device *path_trace_device, const DenoiseParams &params)
    : DenoiserGPU(path_trace_device, params), state_(path_trace_device, "__denoiser_state", true)
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

class OptiXDenoiser::DenoiseContext {
 public:
  explicit DenoiseContext(OptiXDevice *device, const DenoiseTask &task)
      : denoise_params(task.params),
        render_buffers(task.render_buffers),
        buffer_params(task.buffer_params),
        guiding_buffer(device, "denoiser guiding passes buffer", true),
        num_samples(task.num_samples)
  {
    num_input_passes = 1;
    if (denoise_params.use_pass_albedo) {
      num_input_passes += 1;
      use_pass_albedo = true;
      pass_denoising_albedo = buffer_params.get_pass_offset(PASS_DENOISING_ALBEDO);
      if (denoise_params.use_pass_normal) {
        num_input_passes += 1;
        use_pass_normal = true;
        pass_denoising_normal = buffer_params.get_pass_offset(PASS_DENOISING_NORMAL);
      }
    }

    if (denoise_params.temporally_stable) {
      prev_output.device_pointer = render_buffers->buffer.device_pointer;

      prev_output.offset = buffer_params.get_pass_offset(PASS_DENOISING_PREVIOUS);

      prev_output.stride = buffer_params.stride;
      prev_output.pass_stride = buffer_params.pass_stride;

      num_input_passes += 1;
      use_pass_motion = true;
      pass_motion = buffer_params.get_pass_offset(PASS_MOTION);
    }

    use_guiding_passes = (num_input_passes - 1) > 0;

    if (use_guiding_passes) {
      if (task.allow_inplace_modification) {
        guiding_params.device_pointer = render_buffers->buffer.device_pointer;

        guiding_params.pass_albedo = pass_denoising_albedo;
        guiding_params.pass_normal = pass_denoising_normal;
        guiding_params.pass_flow = pass_motion;

        guiding_params.stride = buffer_params.stride;
        guiding_params.pass_stride = buffer_params.pass_stride;
      }
      else {
        guiding_params.pass_stride = 0;
        if (use_pass_albedo) {
          guiding_params.pass_albedo = guiding_params.pass_stride;
          guiding_params.pass_stride += 3;
        }
        if (use_pass_normal) {
          guiding_params.pass_normal = guiding_params.pass_stride;
          guiding_params.pass_stride += 3;
        }
        if (use_pass_motion) {
          guiding_params.pass_flow = guiding_params.pass_stride;
          guiding_params.pass_stride += 2;
        }

        guiding_params.stride = buffer_params.width;

        guiding_buffer.alloc_to_device(buffer_params.width * buffer_params.height *
                                       guiding_params.pass_stride);
        guiding_params.device_pointer = guiding_buffer.device_pointer;
      }
    }

    pass_sample_count = buffer_params.get_pass_offset(PASS_SAMPLE_COUNT);
  }

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

class OptiXDenoiser::DenoisePass {
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
  bool use_compositing;
  bool use_denoising_albedo;
};

bool OptiXDenoiser::denoise_buffer(const DenoiseTask &task)
{
  OptiXDevice *const optix_device = static_cast<OptiXDevice *>(denoiser_device_);

  const CUDAContextScope scope(optix_device);

  DenoiseContext context(optix_device, task);

  if (!denoise_ensure(context)) {
    return false;
  }

  if (!denoise_filter_guiding_preprocess(context)) {
    LOG(ERROR) << "Error preprocessing guiding passes.";
    return false;
  }

  /* Passes which will use real albedo when it is available. */
  denoise_pass(context, PASS_COMBINED);
  denoise_pass(context, PASS_SHADOW_CATCHER_MATTE);

  /* Passes which do not need albedo and hence if real is present it needs to become fake. */
  denoise_pass(context, PASS_SHADOW_CATCHER);

  return true;
}

bool OptiXDenoiser::denoise_filter_guiding_preprocess(const DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.guiding_params.device_pointer,
                             &context.guiding_params.pass_stride,
                             &context.guiding_params.pass_albedo,
                             &context.guiding_params.pass_normal,
                             &context.guiding_params.pass_flow,
                             &context.render_buffers->buffer.device_pointer,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &context.pass_sample_count,
                             &context.pass_denoising_albedo,
                             &context.pass_denoising_normal,
                             &context.pass_motion,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &context.num_samples);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS, work_size, args);
}

bool OptiXDenoiser::denoise_filter_guiding_set_fake_albedo(const DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.guiding_params.device_pointer,
                             &context.guiding_params.pass_stride,
                             &context.guiding_params.pass_albedo,
                             &buffer_params.width,
                             &buffer_params.height);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_GUIDING_SET_FAKE_ALBEDO, work_size, args);
}

void OptiXDenoiser::denoise_pass(DenoiseContext &context, PassType pass_type)
{
  const BufferParams &buffer_params = context.buffer_params;

  const DenoisePass pass(pass_type, buffer_params);

  if (pass.noisy_offset == PASS_UNUSED) {
    return;
  }
  if (pass.denoised_offset == PASS_UNUSED) {
    LOG(DFATAL) << "Missing denoised pass " << pass_type_as_string(pass_type);
    return;
  }

  if (pass.use_denoising_albedo) {
    if (context.albedo_replaced_with_fake) {
      LOG(ERROR) << "Pass which requires albedo is denoised after fake albedo has been set.";
      return;
    }
  }
  else if (context.use_guiding_passes && !context.albedo_replaced_with_fake) {
    context.albedo_replaced_with_fake = true;
    if (!denoise_filter_guiding_set_fake_albedo(context)) {
      LOG(ERROR) << "Error replacing real albedo with the fake one.";
      return;
    }
  }

  /* Read and preprocess noisy color input pass. */
  denoise_color_read(context, pass);
  if (!denoise_filter_color_preprocess(context, pass)) {
    LOG(ERROR) << "Error converting denoising passes to RGB buffer.";
    return;
  }

  if (!denoise_run(context, pass)) {
    LOG(ERROR) << "Error running OptiX denoiser.";
    return;
  }

  /* Store result in the combined pass of the render buffer.
   *
   * This will scale the denoiser result up to match the number of, possibly per-pixel, samples. */
  if (!denoise_filter_color_postprocess(context, pass)) {
    LOG(ERROR) << "Error copying denoiser result to the denoised pass.";
    return;
  }

  denoiser_queue_->synchronize();
}

void OptiXDenoiser::denoise_color_read(const DenoiseContext &context, const DenoisePass &pass)
{
  PassAccessor::PassAccessInfo pass_access_info;
  pass_access_info.type = pass.type;
  pass_access_info.mode = PassMode::NOISY;
  pass_access_info.offset = pass.noisy_offset;

  /* Denoiser operates on passes which are used to calculate the approximation, and is never used
   * on the approximation. The latter is not even possible because OptiX does not support
   * denoising of semi-transparent pixels. */
  pass_access_info.use_approximate_shadow_catcher = false;
  pass_access_info.use_approximate_shadow_catcher_background = false;
  pass_access_info.show_active_pixels = false;

  /* TODO(sergey): Consider adding support of actual exposure, to avoid clamping in extreme cases.
   */
  const PassAccessorGPU pass_accessor(
      denoiser_queue_.get(), pass_access_info, 1.0f, context.num_samples);

  PassAccessor::Destination destination(pass_access_info.type);
  destination.d_pixels = context.render_buffers->buffer.device_pointer +
                         pass.denoised_offset * sizeof(float);
  destination.num_components = 3;
  destination.pixel_stride = context.buffer_params.pass_stride;

  BufferParams buffer_params = context.buffer_params;
  buffer_params.window_x = 0;
  buffer_params.window_y = 0;
  buffer_params.window_width = buffer_params.width;
  buffer_params.window_height = buffer_params.height;

  pass_accessor.get_render_tile_pixels(context.render_buffers, buffer_params, destination);
}

bool OptiXDenoiser::denoise_filter_color_preprocess(const DenoiseContext &context,
                                                    const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &pass.denoised_offset);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_PREPROCESS, work_size, args);
}

bool OptiXDenoiser::denoise_filter_color_postprocess(const DenoiseContext &context,
                                                     const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &context.num_samples,
                             &pass.noisy_offset,
                             &pass.denoised_offset,
                             &context.pass_sample_count,
                             &pass.num_components,
                             &pass.use_compositing);

  return denoiser_queue_->enqueue(DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS, work_size, args);
}

bool OptiXDenoiser::denoise_ensure(DenoiseContext &context)
{
  if (!denoise_create_if_needed(context)) {
    LOG(ERROR) << "OptiX denoiser creation has failed.";
    return false;
  }

  if (!denoise_configure_if_needed(context)) {
    LOG(ERROR) << "OptiX denoiser configuration has failed.";
    return false;
  }

  return true;
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

  OptixDenoiserModelKind model = OPTIX_DENOISER_MODEL_KIND_HDR;
  if (context.use_pass_motion) {
    model = OPTIX_DENOISER_MODEL_KIND_TEMPORAL;
  }

  const OptixResult result = optixDenoiserCreate(
      static_cast<OptiXDevice *>(denoiser_device_)->context,
      model,
      &denoiser_options,
      &optix_denoiser_);

  if (result != OPTIX_SUCCESS) {
    denoiser_device_->set_error("Failed to create OptiX denoiser");
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

  /* Allocate denoiser state if tile size has changed since last setup. */
  state_.device = denoiser_device_;
  state_.alloc_to_device(sizes_.stateSizeInBytes + sizes_.withOverlapScratchSizeInBytes);

  /* Initialize denoiser state for the current tile size. */
  const OptixResult result = optixDenoiserSetup(
      optix_denoiser_,
      0, /* Work around bug in r495 drivers that causes artifacts when denoiser setup is called
          * on a stream that is not the default stream. */
      tile_size.x + sizes_.overlapWindowSizeInPixels * 2,
      tile_size.y + sizes_.overlapWindowSizeInPixels * 2,
      state_.device_pointer,
      sizes_.stateSizeInBytes,
      state_.device_pointer + sizes_.stateSizeInBytes,
      sizes_.withOverlapScratchSizeInBytes);
  if (result != OPTIX_SUCCESS) {
    denoiser_device_->set_error("Failed to set up OptiX denoiser");
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
                      ccl::optixUtilDenoiserInvokeTiled(
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

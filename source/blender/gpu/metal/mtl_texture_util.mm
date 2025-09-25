/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "DNA_userdef_types.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_platform.hh"
#include "GPU_state.hh"

#include "mtl_backend.hh"
#include "mtl_context.hh"
#include "mtl_texture.hh"

/* Utility file for secondary functionality which supports mtl_texture.mm. */

extern char datatoc_compute_texture_update_msl[];
extern char datatoc_compute_texture_read_msl[];

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Texture Utility Functions
 * \{ */

MTLPixelFormat gpu_texture_format_to_metal(TextureFormat tex_format)
{
#define CASE(a, b, c, blender_enum, d, mtl_pixel_enum, f, g, h) \
  case TextureFormat::blender_enum: \
    return MTLPixelFormat##mtl_pixel_enum;

  switch (tex_format) {
    GPU_TEXTURE_FORMAT_EXPAND(CASE)
    case TextureFormat::Invalid:
      break;
  }
#undef CASE
  BLI_assert_msg(false, "Unrecognised GPU pixel format!\n");
  return MTLPixelFormatRGBA8Unorm;
}

size_t get_mtl_format_bytesize(MTLPixelFormat tex_format)
{
  switch (tex_format) {
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Snorm:
    case MTLPixelFormatRGB10A2Uint:
    case MTLPixelFormatRGB10A2Unorm:
      return 4;
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
      return 16;
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
      return 8;
    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG8Snorm:
    case MTLPixelFormatRG8Unorm_sRGB:
      return 2;
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRG32Float:
      return 8;
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG16Snorm:
      return 4;
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
      return 1;
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
      return 4;
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatR16Unorm:
      return 2;
    case MTLPixelFormatRG11B10Float:
      return 4;
    case MTLPixelFormatDepth32Float_Stencil8:
      return 8;
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatDepth24Unorm_Stencil8:
      return 4;
    case MTLPixelFormatDepth16Unorm:
      return 2;
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC1_RGBA_sRGB:
      return 1; /* NOTE: not quite correct (BC1 is 0.5 BPP). */
    case MTLPixelFormatBC2_RGBA:
    case MTLPixelFormatBC2_RGBA_sRGB:
    case MTLPixelFormatBC3_RGBA:
    case MTLPixelFormatBC3_RGBA_sRGB:
      return 1;

    default:
      BLI_assert_msg(false, "Unrecognised GPU pixel format!\n");
      return 1;
  }
}

int get_mtl_format_num_components(MTLPixelFormat tex_format)
{
  switch (tex_format) {
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Snorm:
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatRGB10A2Uint:
    case MTLPixelFormatRGB10A2Unorm:
    case MTLPixelFormatBC1_RGBA_sRGB:
    case MTLPixelFormatBC2_RGBA_sRGB:
    case MTLPixelFormatBC3_RGBA_sRGB:
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC2_RGBA:
    case MTLPixelFormatBC3_RGBA:
      return 4;

    case MTLPixelFormatRG11B10Float:
      return 3;

    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRG32Float:
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatDepth32Float_Stencil8:
    case MTLPixelFormatRG16Snorm:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG8Snorm:
      return 2;

    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Unorm:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatDepth16Unorm:
    case MTLPixelFormatDepth24Unorm_Stencil8:
      /* Treating this format as single-channel for direct data copies -- Stencil component is not
       * addressable. */
      return 1;

    default:
      BLI_assert_msg(false, "Unrecognised GPU pixel format!\n");
      return 1;
  }
}

bool mtl_format_supports_blending(MTLPixelFormat format)
{
  /* Add formats as needed -- Verify platforms. */
  const MTLCapabilities &capabilities = MTLBackend::get_capabilities();

  if (capabilities.supports_family_mac1 || capabilities.supports_family_mac_catalyst1) {

    switch (format) {
      case MTLPixelFormatA8Unorm:
      case MTLPixelFormatR8Uint:
      case MTLPixelFormatR8Sint:
      case MTLPixelFormatR16Uint:
      case MTLPixelFormatR16Sint:
      case MTLPixelFormatRG32Uint:
      case MTLPixelFormatRG32Sint:
      case MTLPixelFormatRGBA8Uint:
      case MTLPixelFormatRGBA8Sint:
      case MTLPixelFormatRGBA32Uint:
      case MTLPixelFormatRGBA32Sint:
      case MTLPixelFormatDepth16Unorm:
      case MTLPixelFormatDepth32Float:
      case MTLPixelFormatInvalid:
      case MTLPixelFormatBGR10A2Unorm:
      case MTLPixelFormatRGB10A2Uint:
        return false;
      default:
        return true;
    }
  }
  else {
    switch (format) {
      case MTLPixelFormatA8Unorm:
      case MTLPixelFormatR8Uint:
      case MTLPixelFormatR8Sint:
      case MTLPixelFormatR16Uint:
      case MTLPixelFormatR16Sint:
      case MTLPixelFormatRG32Uint:
      case MTLPixelFormatRG32Sint:
      case MTLPixelFormatRGBA8Uint:
      case MTLPixelFormatRGBA8Sint:
      case MTLPixelFormatRGBA32Uint:
      case MTLPixelFormatRGBA32Sint:
      case MTLPixelFormatRGBA32Float:
      case MTLPixelFormatDepth16Unorm:
      case MTLPixelFormatDepth32Float:
      case MTLPixelFormatInvalid:
      case MTLPixelFormatBGR10A2Unorm:
      case MTLPixelFormatRGB10A2Uint:
        return false;
      default:
        return true;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture data upload routines
 * \{ */

id<MTLComputePipelineState> gpu::MTLTexture::mtl_texture_update_impl(
    TextureUpdateRoutineSpecialisation specialization_params,
    blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
        &specialization_cache,
    GPUTextureType texture_type)
{
  /* Check whether the Kernel exists. */
  id<MTLComputePipelineState> *result = specialization_cache.lookup_ptr(specialization_params);
  if (result != nullptr) {
    return *result;
  }

  id<MTLComputePipelineState> return_pso = nil;
  @autoreleasepool {

    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /** SOURCE. **/
    NSString *tex_update_kernel_src = [NSString
        stringWithUTF8String:datatoc_compute_texture_update_msl];

    /* Prepare options and specializations. */
    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion2_2;
    options.preprocessorMacros = @{
      @"INPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialization_params.input_data_type.c_str()],
      @"OUTPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialization_params.output_data_type.c_str()],
      @"COMPONENT_COUNT_INPUT" :
          [NSNumber numberWithInt:specialization_params.component_count_input],
      @"COMPONENT_COUNT_OUTPUT" :
          [NSNumber numberWithInt:specialization_params.component_count_output],
      @"TEX_TYPE" : [NSNumber numberWithInt:((int)(texture_type))],
      @"IS_TEXTURE_CLEAR" :
          [NSNumber numberWithInt:((int)(specialization_params.is_clear ? 1 : 0))]
    };

    /* Prepare shader library for conversion routine. */
    NSError *error = nullptr;
    id<MTLLibrary> temp_lib = [[ctx->device newLibraryWithSource:tex_update_kernel_src
                                                         options:options
                                                           error:&error] autorelease];
    if (error) {
      /* Only exit out if genuine error and not warning. */
      if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
          NSNotFound)
      {
        NSLog(@"Compile Error - Metal Shader Library error %@ ", error);
        BLI_assert(false);
        return nil;
      }
    }

    /* Fetch compute function. */
    BLI_assert(temp_lib != nil);
    id<MTLFunction> temp_compute_function = [[temp_lib
        newFunctionWithName:@"compute_texture_update"] autorelease];
    BLI_assert(temp_compute_function);

    /* Otherwise, bake new Kernel. */
    id<MTLComputePipelineState> compute_pso = [ctx->device
        newComputePipelineStateWithFunction:temp_compute_function
                                      error:&error];
    if (error || compute_pso == nil) {
      NSLog(@"Failed to prepare texture_update MTLComputePipelineState %@", error);
      BLI_assert(false);
    }

    /* Store PSO. */
    specialization_cache.add_new(specialization_params, compute_pso);
    return_pso = compute_pso;
  }

  BLI_assert(return_pso != nil);
  return return_pso;
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_1d_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialization,
                                 mtl_context->get_texture_utils().texture_1d_update_compute_psos,
                                 GPU_TEXTURE_1D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_1d_array_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(
      specialization,
      mtl_context->get_texture_utils().texture_1d_array_update_compute_psos,
      GPU_TEXTURE_1D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_2d_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialization,
                                 mtl_context->get_texture_utils().texture_2d_update_compute_psos,
                                 GPU_TEXTURE_2D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_2d_array_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(
      specialization,
      mtl_context->get_texture_utils().texture_2d_array_update_compute_psos,
      GPU_TEXTURE_2D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_3d_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialization,
                                 mtl_context->get_texture_utils().texture_3d_update_compute_psos,
                                 GPU_TEXTURE_3D);
}

/* TODO(Metal): Data upload routine kernel for texture cube and texture cube array.
 * Currently does not appear to be hit. */

gpu::Shader *gpu::MTLTexture::depth_2d_update_sh_get(
    DepthTextureUpdateRoutineSpecialisation specialization)
{

  /* Check whether the Kernel exists. */
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);

  gpu::Shader **result = mtl_context->get_texture_utils().depth_2d_update_shaders.lookup_ptr(
      specialization);
  if (result != nullptr) {
    return *result;
  }

  const char *depth_2d_info_variant = nullptr;
  switch (specialization.data_mode) {
    case MTL_DEPTH_UPDATE_MODE_FLOAT:
      depth_2d_info_variant = "depth_2d_update_float";
      break;
    case MTL_DEPTH_UPDATE_MODE_INT24:
      depth_2d_info_variant = "depth_2d_update_int24";
      break;
    case MTL_DEPTH_UPDATE_MODE_INT32:
      depth_2d_info_variant = "depth_2d_update_int32";
      break;
    default:
      BLI_assert(false && "Invalid format mode\n");
      return nullptr;
  }

  gpu::Shader *shader = GPU_shader_create_from_info_name(depth_2d_info_variant);
  mtl_context->get_texture_utils().depth_2d_update_shaders.add_new(specialization, shader);
  return shader;
}

gpu::Shader *gpu::MTLTexture::fullscreen_blit_sh_get()
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  if (mtl_context->get_texture_utils().fullscreen_blit_shader == nullptr) {
    gpu::Shader *shader = GPU_shader_create_from_info_name("fullscreen_blit");

    mtl_context->get_texture_utils().fullscreen_blit_shader = shader;
  }
  return mtl_context->get_texture_utils().fullscreen_blit_shader;
}

/* Special routine for updating 2D depth textures using the rendering pipeline. */
void gpu::MTLTexture::update_sub_depth_2d(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  /* Verify we are in a valid configuration. */
  BLI_assert(ELEM(format_,
                  TextureFormat::SFLOAT_32_DEPTH,
                  TextureFormat::UNORM_16_DEPTH,
                  TextureFormat::SFLOAT_32_DEPTH_UINT_8));
  BLI_assert(validate_data_format(format_, type));
  BLI_assert(ELEM(type, GPU_DATA_FLOAT, GPU_DATA_UINT_24_8_DEPRECATED, GPU_DATA_UINT));

  /* Determine whether we are in GPU_DATA_UINT_24_8_DEPRECATED or GPU_DATA_FLOAT mode. */
  bool is_float = (type == GPU_DATA_FLOAT);
  TextureFormat format = (is_float) ? TextureFormat::SFLOAT_32 : TextureFormat::SINT_32;

  /* Shader key - Add parameters here for different configurations. */
  DepthTextureUpdateRoutineSpecialisation specialization;
  switch (type) {
    case GPU_DATA_FLOAT:
      specialization.data_mode = MTL_DEPTH_UPDATE_MODE_FLOAT;
      break;

    case GPU_DATA_UINT_24_8_DEPRECATED:
      specialization.data_mode = MTL_DEPTH_UPDATE_MODE_INT24;
      break;

    case GPU_DATA_UINT:
      specialization.data_mode = MTL_DEPTH_UPDATE_MODE_INT32;
      break;

    default:
      BLI_assert_msg(false, "Unsupported eGPUDataFormat being passed to depth texture update\n");
      return;
  }

  /* Push contents into an r32_tex and render contents to depth using a shader. */
  gpu::Texture *r32_tex_tmp = GPU_texture_create_2d("depth_intermediate_copy_tex",
                                                    w_,
                                                    h_,
                                                    1,
                                                    format,
                                                    GPU_TEXTURE_USAGE_SHADER_READ |
                                                        GPU_TEXTURE_USAGE_ATTACHMENT,
                                                    nullptr);
  GPU_texture_filter_mode(r32_tex_tmp, false);
  GPU_texture_extend_mode(r32_tex_tmp, GPU_SAMPLER_EXTEND_MODE_EXTEND);
  gpu::MTLTexture *mtl_tex = static_cast<gpu::MTLTexture *>(r32_tex_tmp);
  mtl_tex->update_sub(mip, offset, extent, type, data);

  gpu::FrameBuffer *restore_fb = GPU_framebuffer_active_get();
  gpu::FrameBuffer *depth_fb_temp = GPU_framebuffer_create("depth_intermediate_copy_fb");
  GPU_framebuffer_texture_attach(depth_fb_temp, this, 0, mip);
  GPU_framebuffer_bind(depth_fb_temp);
  if (extent[0] == w_ && extent[1] == h_) {
    /* Skip load if the whole texture is being updated. */
    GPU_framebuffer_clear_depth(depth_fb_temp, 0.0);
    GPU_framebuffer_clear_stencil(depth_fb_temp, 0);
  }

  gpu::Shader *depth_2d_update_sh = depth_2d_update_sh_get(specialization);
  BLI_assert(depth_2d_update_sh != nullptr);
  Batch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, depth_2d_update_sh);

  GPU_batch_texture_bind(quad, "source_data", r32_tex_tmp);
  GPU_batch_uniform_1i(quad, "mip", mip);
  GPU_batch_uniform_2f(quad, "extent", (float)extent[0], (float)extent[1]);
  GPU_batch_uniform_2f(quad, "offset", (float)offset[0], (float)offset[1]);
  GPU_batch_uniform_2f(quad, "size", (float)w_, (float)h_);

  bool depth_write_prev = GPU_depth_mask_get();
  uint stencil_mask_prev = GPU_stencil_mask_get();
  GPUDepthTest depth_test_prev = GPU_depth_test_get();
  GPUStencilTest stencil_test_prev = GPU_stencil_test_get();
  GPU_scissor_test(true);
  GPU_scissor(offset[0], offset[1], extent[0], extent[1]);

  GPU_stencil_write_mask_set(0xFF);
  GPU_stencil_reference_set(0);
  GPU_stencil_test(GPU_STENCIL_ALWAYS);
  GPU_depth_mask(true);
  GPU_depth_test(GPU_DEPTH_ALWAYS);

  GPU_batch_draw(quad);

  GPU_depth_mask(depth_write_prev);
  GPU_stencil_write_mask_set(stencil_mask_prev);
  GPU_stencil_test(stencil_test_prev);
  GPU_depth_test(depth_test_prev);

  if (restore_fb != nullptr) {
    GPU_framebuffer_bind(restore_fb);
  }
  else {
    GPU_framebuffer_restore();
  }
  GPU_framebuffer_free(depth_fb_temp);
  GPU_texture_free(r32_tex_tmp);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture data read routines
 * \{ */

id<MTLComputePipelineState> gpu::MTLTexture::mtl_texture_read_impl(
    TextureReadRoutineSpecialisation specialization_params,
    blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
        &specialization_cache,
    GPUTextureType texture_type)
{
  /* Check whether the Kernel exists. */
  id<MTLComputePipelineState> *result = specialization_cache.lookup_ptr(specialization_params);
  if (result != nullptr) {
    return *result;
  }

  id<MTLComputePipelineState> return_pso = nil;
  @autoreleasepool {

    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /** SOURCE. **/
    NSString *tex_update_kernel_src = [NSString
        stringWithUTF8String:datatoc_compute_texture_read_msl];

    /* Defensive Debug Checks. */
    int64_t depth_scale_factor = 1;
    if (specialization_params.depth_format_mode > 0) {
      BLI_assert(specialization_params.component_count_input == 1);
      BLI_assert(specialization_params.component_count_output == 1);
      switch (specialization_params.depth_format_mode) {
        case 1:
          /* FLOAT */
          depth_scale_factor = 1;
          break;
        case 2:
          /* D24 uint */
          depth_scale_factor = 0xFFFFFFu;
          break;
        case 4:
          /* D32 uint */
          depth_scale_factor = 0xFFFFFFFFu;
          break;
        default:
          BLI_assert_msg(0, "Unrecognized mode");
          break;
      }
    }

    /* Prepare options and specializations. */
    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion2_2;
    options.preprocessorMacros = @{
      @"INPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialization_params.input_data_type.c_str()],
      @"OUTPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialization_params.output_data_type.c_str()],
      @"COMPONENT_COUNT_INPUT" :
          [NSNumber numberWithInt:specialization_params.component_count_input],
      @"COMPONENT_COUNT_OUTPUT" :
          [NSNumber numberWithInt:specialization_params.component_count_output],
      @"WRITE_COMPONENT_COUNT" :
          [NSNumber numberWithInt:min_ii(specialization_params.component_count_input,
                                         specialization_params.component_count_output)],
      @"IS_DEPTH_FORMAT" :
          [NSNumber numberWithInt:((specialization_params.depth_format_mode > 0) ? 1 : 0)],
      @"DEPTH_SCALE_FACTOR" : [NSNumber numberWithLongLong:depth_scale_factor],
      @"TEX_TYPE" : [NSNumber numberWithInt:((int)(texture_type))],
      @"IS_DEPTHSTENCIL_24_8" :
          [NSNumber numberWithInt:(specialization_params.depth_format_mode == 2) ? 1 : 0]
    };

    /* Prepare shader library for conversion routine. */
    NSError *error = nullptr;
    id<MTLLibrary> temp_lib = [[ctx->device newLibraryWithSource:tex_update_kernel_src
                                                         options:options
                                                           error:&error] autorelease];
    if (error) {
      /* Only exit out if genuine error and not warning. */
      if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
          NSNotFound)
      {
        NSLog(@"Compile Error - Metal Shader Library error %@ ", error);
        BLI_assert(false);
        return nil;
      }
    }

    /* Fetch compute function. */
    BLI_assert(temp_lib != nil);
    id<MTLFunction> temp_compute_function = [[temp_lib newFunctionWithName:@"compute_texture_read"]
        autorelease];
    BLI_assert(temp_compute_function);

    /* Otherwise, bake new Kernel. */
    id<MTLComputePipelineState> compute_pso = [ctx->device
        newComputePipelineStateWithFunction:temp_compute_function
                                      error:&error];
    if (error || compute_pso == nil) {
      NSLog(@"Failed to prepare texture_read MTLComputePipelineState %@", error);
      BLI_assert(false);
      return nil;
    }

    /* Store PSO. */
    specialization_cache.add_new(specialization_params, compute_pso);
    return_pso = compute_pso;
  }

  BLI_assert(return_pso != nil);
  return return_pso;
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_2d_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_2d_read_compute_psos,
                               GPU_TEXTURE_2D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_2d_array_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_2d_array_read_compute_psos,
                               GPU_TEXTURE_2D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_1d_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_1d_read_compute_psos,
                               GPU_TEXTURE_1D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_1d_array_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_1d_array_read_compute_psos,
                               GPU_TEXTURE_1D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_3d_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_3d_read_compute_psos,
                               GPU_TEXTURE_3D);
}

/** \} */

}  // namespace blender::gpu

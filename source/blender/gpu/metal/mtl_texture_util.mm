/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "DNA_userdef_types.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_capabilities.h"
#include "GPU_framebuffer.h"
#include "GPU_platform.h"
#include "GPU_state.h"

#include "mtl_backend.hh"
#include "mtl_context.hh"
#include "mtl_texture.hh"

/* Utility file for secondary functionality which supports mtl_texture.mm. */

extern char datatoc_compute_texture_update_msl[];
extern char datatoc_depth_2d_update_vert_glsl[];
extern char datatoc_depth_2d_update_float_frag_glsl[];
extern char datatoc_depth_2d_update_int24_frag_glsl[];
extern char datatoc_depth_2d_update_int32_frag_glsl[];
extern char datatoc_compute_texture_read_msl[];
extern char datatoc_gpu_shader_fullscreen_blit_vert_glsl[];
extern char datatoc_gpu_shader_fullscreen_blit_frag_glsl[];

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Texture Utility Functions
 * \{ */

MTLPixelFormat gpu_texture_format_to_metal(eGPUTextureFormat tex_format)
{

  switch (tex_format) {
    /* Formats texture & renderbuffer. */
    case GPU_RGBA8UI:
      return MTLPixelFormatRGBA8Uint;
    case GPU_RGBA8I:
      return MTLPixelFormatRGBA8Sint;
    case GPU_RGBA8:
      return MTLPixelFormatRGBA8Unorm;
    case GPU_RGBA32UI:
      return MTLPixelFormatRGBA32Uint;
    case GPU_RGBA32I:
      return MTLPixelFormatRGBA32Sint;
    case GPU_RGBA32F:
      return MTLPixelFormatRGBA32Float;
    case GPU_RGBA16UI:
      return MTLPixelFormatRGBA16Uint;
    case GPU_RGBA16I:
      return MTLPixelFormatRGBA16Sint;
    case GPU_RGBA16F:
      return MTLPixelFormatRGBA16Float;
    case GPU_RGBA16:
      return MTLPixelFormatRGBA16Unorm;
    case GPU_RG8UI:
      return MTLPixelFormatRG8Uint;
    case GPU_RG8I:
      return MTLPixelFormatRG8Sint;
    case GPU_RG8:
      return MTLPixelFormatRG8Unorm;
    case GPU_RG32UI:
      return MTLPixelFormatRG32Uint;
    case GPU_RG32I:
      return MTLPixelFormatRG32Sint;
    case GPU_RG32F:
      return MTLPixelFormatRG32Float;
    case GPU_RG16UI:
      return MTLPixelFormatRG16Uint;
    case GPU_RG16I:
      return MTLPixelFormatRG16Sint;
    case GPU_RG16F:
      return MTLPixelFormatRG16Float;
    case GPU_RG16:
      return MTLPixelFormatRG16Float;
    case GPU_R8UI:
      return MTLPixelFormatR8Uint;
    case GPU_R8I:
      return MTLPixelFormatR8Sint;
    case GPU_R8:
      return MTLPixelFormatR8Unorm;
    case GPU_R32UI:
      return MTLPixelFormatR32Uint;
    case GPU_R32I:
      return MTLPixelFormatR32Sint;
    case GPU_R32F:
      return MTLPixelFormatR32Float;
    case GPU_R16UI:
      return MTLPixelFormatR16Uint;
    case GPU_R16I:
      return MTLPixelFormatR16Sint;
    case GPU_R16F:
      return MTLPixelFormatR16Float;
    case GPU_R16:
      return MTLPixelFormatR16Snorm;

    /* Special formats texture & renderbuffer. */
    case GPU_R11F_G11F_B10F:
      return MTLPixelFormatRG11B10Float;
    case GPU_DEPTH32F_STENCIL8:
      return MTLPixelFormatDepth32Float_Stencil8;
    case GPU_DEPTH24_STENCIL8: {
      BLI_assert(false && "GPU_DEPTH24_STENCIL8 not supported by Apple Silicon.");
      return MTLPixelFormatDepth24Unorm_Stencil8;
    }
    case GPU_SRGB8_A8:
      return MTLPixelFormatRGBA8Unorm_sRGB;
    case GPU_RGB16F:
      return MTLPixelFormatRGBA16Float;

    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
      return MTLPixelFormatDepth32Float;
    case GPU_DEPTH_COMPONENT16:
      return MTLPixelFormatDepth16Unorm;

    default:
      BLI_assert(!"Unrecognised GPU pixel format!\n");
      return MTLPixelFormatRGBA8Unorm;
  }
}

int get_mtl_format_bytesize(MTLPixelFormat tex_format)
{

  switch (tex_format) {
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatRGBA8Unorm:
      return 4;
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
      return 16;
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Unorm:
      return 8;
    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
    case MTLPixelFormatRG8Unorm:
      return 2;
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRG32Float:
      return 8;
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
      return 4;
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR8Unorm:
      return 1;
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
      return 4;
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Snorm:
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

    default:
      BLI_assert(!"Unrecognised GPU pixel format!\n");
      return 1;
  }
}

int get_mtl_format_num_components(MTLPixelFormat tex_format)
{

  switch (tex_format) {
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
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
      return 2;

    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatDepth16Unorm:
    case MTLPixelFormatDepth24Unorm_Stencil8:
      /* Treating this format as single-channel for direct data copies -- Stencil component is not
       * addressable. */
      return 1;

    default:
      BLI_assert(!"Unrecognised GPU pixel format!\n");
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
    TextureUpdateRoutineSpecialisation specialisation_params,
    blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
        &specialisation_cache,
    eGPUTextureType texture_type)
{
  /* Check whether the Kernel exists. */
  id<MTLComputePipelineState> *result = specialisation_cache.lookup_ptr(specialisation_params);
  if (result != nullptr) {
    return *result;
  }

  id<MTLComputePipelineState> return_pso = nil;
  @autoreleasepool {

    /* Fetch active context. */
    MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
    BLI_assert(ctx);

    /** SOURCE. **/
    NSString *tex_update_kernel_src = [NSString
        stringWithUTF8String:datatoc_compute_texture_update_msl];

    /* Prepare options and specializations. */
    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion2_2;
    options.preprocessorMacros = @{
      @"INPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialisation_params.input_data_type.c_str()],
      @"OUTPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialisation_params.output_data_type.c_str()],
      @"COMPONENT_COUNT_INPUT" :
          [NSNumber numberWithInt:specialisation_params.component_count_input],
      @"COMPONENT_COUNT_OUTPUT" :
          [NSNumber numberWithInt:specialisation_params.component_count_output],
      @"TEX_TYPE" : [NSNumber numberWithInt:((int)(texture_type))]
    };

    /* Prepare shader library for conversion routine. */
    NSError *error = NULL;
    id<MTLLibrary> temp_lib = [[ctx->device newLibraryWithSource:tex_update_kernel_src
                                                         options:options
                                                           error:&error] autorelease];
    if (error) {
      NSLog(@"Compile Error - Metal Shader Library error %@ ", error);
      BLI_assert(false);
      return nullptr;
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
    [compute_pso retain];
    specialisation_cache.add_new(specialisation_params, compute_pso);
    return_pso = compute_pso;
  }

  BLI_assert(return_pso != nil);
  return return_pso;
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_1d_get_kernel(
    TextureUpdateRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialisation,
                                 mtl_context->get_texture_utils().texture_1d_update_compute_psos,
                                 GPU_TEXTURE_1D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_1d_array_get_kernel(
    TextureUpdateRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(
      specialisation,
      mtl_context->get_texture_utils().texture_1d_array_update_compute_psos,
      GPU_TEXTURE_1D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_2d_get_kernel(
    TextureUpdateRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialisation,
                                 mtl_context->get_texture_utils().texture_2d_update_compute_psos,
                                 GPU_TEXTURE_2D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_2d_array_get_kernel(
    TextureUpdateRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(
      specialisation,
      mtl_context->get_texture_utils().texture_2d_array_update_compute_psos,
      GPU_TEXTURE_2D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_update_3d_get_kernel(
    TextureUpdateRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialisation,
                                 mtl_context->get_texture_utils().texture_3d_update_compute_psos,
                                 GPU_TEXTURE_3D);
}

/* TODO(Metal): Data upload routine kernel for texture cube and texture cube array.
 * Currently does not appear to be hit. */

GPUShader *gpu::MTLTexture::depth_2d_update_sh_get(
    DepthTextureUpdateRoutineSpecialisation specialisation)
{

  /* Check whether the Kernel exists. */
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);

  GPUShader **result = mtl_context->get_texture_utils().depth_2d_update_shaders.lookup_ptr(
      specialisation);
  if (result != nullptr) {
    return *result;
  }

  const char *fragment_source = nullptr;
  switch (specialisation.data_mode) {
    case MTL_DEPTH_UPDATE_MODE_FLOAT:
      fragment_source = datatoc_depth_2d_update_float_frag_glsl;
      break;
    case MTL_DEPTH_UPDATE_MODE_INT24:
      fragment_source = datatoc_depth_2d_update_int24_frag_glsl;
      break;
    case MTL_DEPTH_UPDATE_MODE_INT32:
      fragment_source = datatoc_depth_2d_update_int32_frag_glsl;
      break;
    default:
      BLI_assert(false && "Invalid format mode\n");
      return nullptr;
  }

  GPUShader *shader = GPU_shader_create(datatoc_depth_2d_update_vert_glsl,
                                        fragment_source,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        "depth_2d_update_sh_get");
  mtl_context->get_texture_utils().depth_2d_update_shaders.add_new(specialisation, shader);
  return shader;
}

GPUShader *gpu::MTLTexture::fullscreen_blit_sh_get()
{

  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  if (mtl_context->get_texture_utils().fullscreen_blit_shader == nullptr) {
    const char *vertex_source = datatoc_gpu_shader_fullscreen_blit_vert_glsl;
    const char *fragment_source = datatoc_gpu_shader_fullscreen_blit_frag_glsl;
    GPUShader *shader = GPU_shader_create(
        vertex_source, fragment_source, nullptr, nullptr, nullptr, "fullscreen_blit");
    mtl_context->get_texture_utils().fullscreen_blit_shader = shader;
  }
  return mtl_context->get_texture_utils().fullscreen_blit_shader;
}

/* Special routine for updating 2D depth textures using the rendering pipeline. */
void gpu::MTLTexture::update_sub_depth_2d(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  /* Verify we are in a valid configuration. */
  BLI_assert(ELEM(this->format_,
                  GPU_DEPTH_COMPONENT24,
                  GPU_DEPTH_COMPONENT32F,
                  GPU_DEPTH_COMPONENT16,
                  GPU_DEPTH24_STENCIL8,
                  GPU_DEPTH32F_STENCIL8));
  BLI_assert(validate_data_format_mtl(this->format_, type));
  BLI_assert(ELEM(type, GPU_DATA_FLOAT, GPU_DATA_UINT_24_8, GPU_DATA_UINT));

  /* Determine whether we are in GPU_DATA_UINT_24_8 or GPU_DATA_FLOAT mode. */
  bool is_float = (type == GPU_DATA_FLOAT);
  eGPUTextureFormat format = (is_float) ? GPU_R32F : GPU_R32I;

  /* Shader key - Add parameters here for different configurations. */
  DepthTextureUpdateRoutineSpecialisation specialisation;
  switch (type) {
    case GPU_DATA_FLOAT:
      specialisation.data_mode = MTL_DEPTH_UPDATE_MODE_FLOAT;
      break;

    case GPU_DATA_UINT_24_8:
      specialisation.data_mode = MTL_DEPTH_UPDATE_MODE_INT24;
      break;

    case GPU_DATA_UINT:
      specialisation.data_mode = MTL_DEPTH_UPDATE_MODE_INT32;
      break;

    default:
      BLI_assert(false && "Unsupported eGPUDataFormat being passed to depth texture update\n");
      return;
  }

  /* Push contents into an r32_tex and render contents to depth using a shader. */
  GPUTexture *r32_tex_tmp = GPU_texture_create_2d(
      "depth_intermediate_copy_tex", this->w_, this->h_, 1, format, nullptr);
  GPU_texture_filter_mode(r32_tex_tmp, false);
  GPU_texture_wrap_mode(r32_tex_tmp, false, true);
  gpu::MTLTexture *mtl_tex = static_cast<gpu::MTLTexture *>(unwrap(r32_tex_tmp));
  mtl_tex->update_sub(mip, offset, extent, type, data);

  GPUFrameBuffer *restore_fb = GPU_framebuffer_active_get();
  GPUFrameBuffer *depth_fb_temp = GPU_framebuffer_create("depth_intermediate_copy_fb");
  GPU_framebuffer_texture_attach(depth_fb_temp, wrap(static_cast<Texture *>(this)), 0, mip);
  GPU_framebuffer_bind(depth_fb_temp);
  if (extent[0] == this->w_ && extent[1] == this->h_) {
    /* Skip load if the whole texture is being updated. */
    GPU_framebuffer_clear_depth(depth_fb_temp, 0.0);
    GPU_framebuffer_clear_stencil(depth_fb_temp, 0);
  }

  GPUShader *depth_2d_update_sh = depth_2d_update_sh_get(specialisation);
  BLI_assert(depth_2d_update_sh != nullptr);
  GPUBatch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, depth_2d_update_sh);

  GPU_batch_texture_bind(quad, "source_data", r32_tex_tmp);
  GPU_batch_uniform_1i(quad, "mip", mip);
  GPU_batch_uniform_2f(quad, "extent", (float)extent[0], (float)extent[1]);
  GPU_batch_uniform_2f(quad, "offset", (float)offset[0], (float)offset[1]);
  GPU_batch_uniform_2f(quad, "size", (float)this->w_, (float)this->h_);

  bool depth_write_prev = GPU_depth_mask_get();
  uint stencil_mask_prev = GPU_stencil_mask_get();
  eGPUDepthTest depth_test_prev = GPU_depth_test_get();
  eGPUStencilTest stencil_test_prev = GPU_stencil_test_get();
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
/** \name Texture data read  routines
 * \{ */

id<MTLComputePipelineState> gpu::MTLTexture::mtl_texture_read_impl(
    TextureReadRoutineSpecialisation specialisation_params,
    blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
        &specialisation_cache,
    eGPUTextureType texture_type)
{
  /* Check whether the Kernel exists. */
  id<MTLComputePipelineState> *result = specialisation_cache.lookup_ptr(specialisation_params);
  if (result != nullptr) {
    return *result;
  }

  id<MTLComputePipelineState> return_pso = nil;
  @autoreleasepool {

    /* Fetch active context. */
    MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
    BLI_assert(ctx);

    /** SOURCE. **/
    NSString *tex_update_kernel_src = [NSString
        stringWithUTF8String:datatoc_compute_texture_read_msl];

    /* Defensive Debug Checks. */
    long long int depth_scale_factor = 1;
    if (specialisation_params.depth_format_mode > 0) {
      BLI_assert(specialisation_params.component_count_input == 1);
      BLI_assert(specialisation_params.component_count_output == 1);
      switch (specialisation_params.depth_format_mode) {
        case 1:
          /* FLOAT */
          depth_scale_factor = 1;
          break;
        case 2:
          /* D24 unsigned int */
          depth_scale_factor = 0xFFFFFFu;
          break;
        case 4:
          /* D32 unsigned int */
          depth_scale_factor = 0xFFFFFFFFu;
          break;
        default:
          BLI_assert_msg(0, "Unrecognised mode");
          break;
      }
    }

    /* Prepare options and specializations. */
    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion2_2;
    options.preprocessorMacros = @{
      @"INPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialisation_params.input_data_type.c_str()],
      @"OUTPUT_DATA_TYPE" :
          [NSString stringWithUTF8String:specialisation_params.output_data_type.c_str()],
      @"COMPONENT_COUNT_INPUT" :
          [NSNumber numberWithInt:specialisation_params.component_count_input],
      @"COMPONENT_COUNT_OUTPUT" :
          [NSNumber numberWithInt:specialisation_params.component_count_output],
      @"WRITE_COMPONENT_COUNT" :
          [NSNumber numberWithInt:min_ii(specialisation_params.component_count_input,
                                         specialisation_params.component_count_output)],
      @"IS_DEPTH_FORMAT" :
          [NSNumber numberWithInt:((specialisation_params.depth_format_mode > 0) ? 1 : 0)],
      @"DEPTH_SCALE_FACTOR" : [NSNumber numberWithLongLong:depth_scale_factor],
      @"TEX_TYPE" : [NSNumber numberWithInt:((int)(texture_type))]
    };

    /* Prepare shader library for conversion routine. */
    NSError *error = NULL;
    id<MTLLibrary> temp_lib = [[ctx->device newLibraryWithSource:tex_update_kernel_src
                                                         options:options
                                                           error:&error] autorelease];
    if (error) {
      NSLog(@"Compile Error - Metal Shader Library error %@ ", error);
      BLI_assert(false);
      return nil;
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
    [compute_pso retain];
    specialisation_cache.add_new(specialisation_params, compute_pso);
    return_pso = compute_pso;
  }

  BLI_assert(return_pso != nil);
  return return_pso;
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_2d_get_kernel(
    TextureReadRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialisation,
                               mtl_context->get_texture_utils().texture_2d_read_compute_psos,
                               GPU_TEXTURE_2D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_2d_array_get_kernel(
    TextureReadRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialisation,
                               mtl_context->get_texture_utils().texture_2d_array_read_compute_psos,
                               GPU_TEXTURE_2D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_1d_get_kernel(
    TextureReadRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialisation,
                               mtl_context->get_texture_utils().texture_1d_read_compute_psos,
                               GPU_TEXTURE_1D);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_1d_array_get_kernel(
    TextureReadRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialisation,
                               mtl_context->get_texture_utils().texture_1d_array_read_compute_psos,
                               GPU_TEXTURE_1D_ARRAY);
}

id<MTLComputePipelineState> gpu::MTLTexture::texture_read_3d_get_kernel(
    TextureReadRoutineSpecialisation specialisation)
{
  MTLContext *mtl_context = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialisation,
                               mtl_context->get_texture_utils().texture_3d_read_compute_psos,
                               GPU_TEXTURE_3D);
}

/** \} */

}  // namespace blender::gpu

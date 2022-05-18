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
#include "mtl_common.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_texture.hh"

#include "GHOST_C-api.h"

/* Debug assistance. */
/* Capture texture update routine for analysis in XCode GPU Frame Debugger. */
#define DEBUG_TEXTURE_UPDATE_CAPTURE false

/* Capture texture read routine for analysis in XCode GPU Frame Debugger. */
#define DEBUG_TEXTURE_READ_CAPTURE false

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

void gpu::MTLTexture::mtl_texture_init()
{
  BLI_assert(MTLContext::get() != nullptr);

  /* Status. */
  this->is_baked_ = false;
  this->is_dirty_ = false;
  this->resource_mode_ = MTL_TEXTURE_MODE_DEFAULT;
  this->mtl_max_mips_ = 1;

  /* Metal properties. */
  this->texture_ = nil;
  this->texture_buffer_ = nil;
  this->mip_swizzle_view_ = nil;

  /* Binding information. */
  this->is_bound_ = false;

  /* VBO. */
  this->vert_buffer_ = nullptr;
  this->vert_buffer_mtl_ = nil;
  this->vert_buffer_offset_ = -1;

  /* Default Swizzle. */
  this->tex_swizzle_mask_[0] = 'r';
  this->tex_swizzle_mask_[1] = 'g';
  this->tex_swizzle_mask_[2] = 'b';
  this->tex_swizzle_mask_[3] = 'a';
  this->mtl_swizzle_mask_ = MTLTextureSwizzleChannelsMake(
      MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha);

  /* TODO(Metal): Find a way of specifying texture usage externally. */
  this->gpu_image_usage_flags_ = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
}

gpu::MTLTexture::MTLTexture(const char *name) : Texture(name)
{
  /* Common Initialization. */
  mtl_texture_init();
}

gpu::MTLTexture::MTLTexture(const char *name,
                            eGPUTextureFormat format,
                            eGPUTextureType type,
                            id<MTLTexture> metal_texture)
    : Texture(name)
{
  /* Common Initialization. */
  mtl_texture_init();

  /* Prep texture from METAL handle. */
  BLI_assert(metal_texture != nil);
  BLI_assert(type == GPU_TEXTURE_2D);
  this->type_ = type;
  init_2D(metal_texture.width, metal_texture.height, 0, 1, format);

  /* Assign MTLTexture. */
  this->texture_ = metal_texture;
  [this->texture_ retain];

  /* Flag as Baked. */
  this->is_baked_ = true;
  this->is_dirty_ = false;
  this->resource_mode_ = MTL_TEXTURE_MODE_EXTERNAL;
}

gpu::MTLTexture::~MTLTexture()
{
  /* Unbind if bound. */
  if (this->is_bound_) {
    MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
    if (ctx != nullptr) {
      ctx->state_manager->texture_unbind(this);
    }
  }

  /* Free memory. */
  this->reset();
}

/** \} */

/* -------------------------------------------------------------------- */
void gpu::MTLTexture::bake_mip_swizzle_view()
{
  if (texture_view_dirty_flags_) {
    /* if a texture view was previously created we release it. */
    if (this->mip_swizzle_view_ != nil) {
      [this->mip_swizzle_view_ release];
    }

    /* Determine num slices */
    int num_slices = 1;
    switch (this->type_) {
      case GPU_TEXTURE_1D_ARRAY:
        num_slices = this->h_;
        break;
      case GPU_TEXTURE_2D_ARRAY:
        num_slices = this->d_;
        break;
      case GPU_TEXTURE_CUBE:
        num_slices = 6;
        break;
      case GPU_TEXTURE_CUBE_ARRAY:
        /* d_ is equal to array levels * 6, including face count. */
        num_slices = this->d_;
        break;
      default:
        num_slices = 1;
        break;
    }

    int range_len = min_ii((this->mip_texture_max_level_ - this->mip_texture_base_level_) + 1,
                           this->texture_.mipmapLevelCount);
    BLI_assert(range_len > 0);
    BLI_assert(mip_texture_base_level_ < this->texture_.mipmapLevelCount);
    BLI_assert(this->mip_texture_base_layer_ < num_slices);
    this->mip_swizzle_view_ = [this->texture_
        newTextureViewWithPixelFormat:this->texture_.pixelFormat
                          textureType:this->texture_.textureType
                               levels:NSMakeRange(this->mip_texture_base_level_, range_len)
                               slices:NSMakeRange(this->mip_texture_base_layer_, num_slices)
                              swizzle:this->mtl_swizzle_mask_];
    MTL_LOG_INFO(
        "Updating texture view - MIP TEXTURE BASE LEVEL: %d, MAX LEVEL: %d (Range len: %d)\n",
        this->mip_texture_base_level_,
        min_ii(this->mip_texture_max_level_, this->texture_.mipmapLevelCount),
        range_len);
    [this->mip_swizzle_view_ retain];
    this->mip_swizzle_view_.label = [this->texture_ label];
    texture_view_dirty_flags_ = TEXTURE_VIEW_NOT_DIRTY;
  }
}

/** \name Operations
 * \{ */

id<MTLTexture> gpu::MTLTexture::get_metal_handle()
{

  /* ensure up to date and baked. */
  this->ensure_baked();

  /* Verify VBO texture shares same buffer. */
  if (this->resource_mode_ == MTL_TEXTURE_MODE_VBO) {
    int r_offset = -1;

    /* TODO(Metal): Fetch buffer from MTLVertBuf when implemented. */
    id<MTLBuffer> buf = nil; /*vert_buffer_->get_metal_buffer(&r_offset);*/
    BLI_assert(this->vert_buffer_mtl_ != nil);
    BLI_assert(buf == this->vert_buffer_mtl_ && r_offset == this->vert_buffer_offset_);

    UNUSED_VARS(buf);
    UNUSED_VARS_NDEBUG(r_offset);
  }

  if (this->is_baked_) {
    /* For explicit texture views, ensure we always return the texture view. */
    if (this->resource_mode_ == MTL_TEXTURE_MODE_TEXTURE_VIEW) {
      BLI_assert(this->mip_swizzle_view_ && "Texture view should always have a valid handle.");
    }

    if (this->mip_swizzle_view_ != nil || texture_view_dirty_flags_) {
      bake_mip_swizzle_view();
      return this->mip_swizzle_view_;
    }
    return this->texture_;
  }
  return nil;
}

id<MTLTexture> gpu::MTLTexture::get_metal_handle_base()
{

  /* ensure up to date and baked. */
  this->ensure_baked();

  /* For explicit texture views, always return the texture view. */
  if (this->resource_mode_ == MTL_TEXTURE_MODE_TEXTURE_VIEW) {
    BLI_assert(this->mip_swizzle_view_ && "Texture view should always have a valid handle.");
    if (this->mip_swizzle_view_ != nil || texture_view_dirty_flags_) {
      bake_mip_swizzle_view();
    }
    return this->mip_swizzle_view_;
  }

  /* Return base handle. */
  if (this->is_baked_) {
    return this->texture_;
  }
  return nil;
}

void gpu::MTLTexture::blit(id<MTLBlitCommandEncoder> blit_encoder,
                           unsigned int src_x_offset,
                           unsigned int src_y_offset,
                           unsigned int src_z_offset,
                           unsigned int src_slice,
                           unsigned int src_mip,
                           gpu::MTLTexture *dest,
                           unsigned int dst_x_offset,
                           unsigned int dst_y_offset,
                           unsigned int dst_z_offset,
                           unsigned int dst_slice,
                           unsigned int dst_mip,
                           unsigned int width,
                           unsigned int height,
                           unsigned int depth)
{

  BLI_assert(this && dest);
  BLI_assert(width > 0 && height > 0 && depth > 0);
  MTLSize src_size = MTLSizeMake(width, height, depth);
  MTLOrigin src_origin = MTLOriginMake(src_x_offset, src_y_offset, src_z_offset);
  MTLOrigin dst_origin = MTLOriginMake(dst_x_offset, dst_y_offset, dst_z_offset);

  if (this->format_get() != dest->format_get()) {
    MTL_LOG_WARNING(
        "[Warning] gpu::MTLTexture: Cannot copy between two textures of different types using a "
        "blit encoder. TODO: Support this operation\n");
    return;
  }

  /* TODO(Metal): Verify if we want to use the one with modified base-level/texture view
   * or not. */
  [blit_encoder copyFromTexture:this->get_metal_handle_base()
                    sourceSlice:src_slice
                    sourceLevel:src_mip
                   sourceOrigin:src_origin
                     sourceSize:src_size
                      toTexture:dest->get_metal_handle_base()
               destinationSlice:dst_slice
               destinationLevel:dst_mip
              destinationOrigin:dst_origin];
}

void gpu::MTLTexture::blit(gpu::MTLTexture *dst,
                           unsigned int src_x_offset,
                           unsigned int src_y_offset,
                           unsigned int dst_x_offset,
                           unsigned int dst_y_offset,
                           unsigned int src_mip,
                           unsigned int dst_mip,
                           unsigned int dst_slice,
                           int width,
                           int height)
{
  BLI_assert(this->type_get() == dst->type_get());

  GPUShader *shader = fullscreen_blit_sh_get();
  BLI_assert(shader != nullptr);
  BLI_assert(GPU_context_active_get());

  /* Fetch restore framebuffer and blit target framebuffer from destination texture. */
  GPUFrameBuffer *restore_fb = GPU_framebuffer_active_get();
  GPUFrameBuffer *blit_target_fb = dst->get_blit_framebuffer(dst_slice, dst_mip);
  BLI_assert(blit_target_fb);
  GPU_framebuffer_bind(blit_target_fb);

  /* Execute graphics draw call to perform the blit. */
  GPUBatch *quad = GPU_batch_preset_quad();

  GPU_batch_set_shader(quad, shader);

  float w = dst->width_get();
  float h = dst->height_get();

  GPU_shader_uniform_2f(shader, "fullscreen", w, h);
  GPU_shader_uniform_2f(shader, "src_offset", src_x_offset, src_y_offset);
  GPU_shader_uniform_2f(shader, "dst_offset", dst_x_offset, dst_y_offset);
  GPU_shader_uniform_2f(shader, "size", width, height);

  GPU_shader_uniform_1i(shader, "mip", src_mip);
  GPU_batch_texture_bind(quad, "imageTexture", wrap(this));

  /* Caching previous pipeline state. */
  bool depth_write_prev = GPU_depth_mask_get();
  uint stencil_mask_prev = GPU_stencil_mask_get();
  eGPUStencilTest stencil_test_prev = GPU_stencil_test_get();
  eGPUFaceCullTest culling_test_prev = GPU_face_culling_get();
  eGPUBlend blend_prev = GPU_blend_get();
  eGPUDepthTest depth_test_prev = GPU_depth_test_get();
  GPU_scissor_test(false);

  /* Apply state for blit draw call. */
  GPU_stencil_write_mask_set(0xFF);
  GPU_stencil_reference_set(0);
  GPU_face_culling(GPU_CULL_NONE);
  GPU_stencil_test(GPU_STENCIL_ALWAYS);
  GPU_depth_mask(false);
  GPU_blend(GPU_BLEND_NONE);
  GPU_depth_test(GPU_DEPTH_ALWAYS);

  GPU_batch_draw(quad);

  /* restoring old pipeline state. */
  GPU_depth_mask(depth_write_prev);
  GPU_stencil_write_mask_set(stencil_mask_prev);
  GPU_stencil_test(stencil_test_prev);
  GPU_face_culling(culling_test_prev);
  GPU_depth_mask(depth_write_prev);
  GPU_blend(blend_prev);
  GPU_depth_test(depth_test_prev);

  if (restore_fb != nullptr) {
    GPU_framebuffer_bind(restore_fb);
  }
  else {
    GPU_framebuffer_restore();
  }
}

GPUFrameBuffer *gpu::MTLTexture::get_blit_framebuffer(unsigned int dst_slice, unsigned int dst_mip)
{

  /* Check if layer has changed. */
  bool update_attachments = false;
  if (!this->blit_fb_) {
    this->blit_fb_ = GPU_framebuffer_create("gpu_blit");
    update_attachments = true;
  }

  /* Check if current blit FB has the correct attachment properties. */
  if (this->blit_fb_) {
    if (this->blit_fb_slice_ != dst_slice || this->blit_fb_mip_ != dst_mip) {
      update_attachments = true;
    }
  }

  if (update_attachments) {
    if (format_flag_ & GPU_FORMAT_DEPTH || format_flag_ & GPU_FORMAT_STENCIL) {
      /* DEPTH TEX */
      GPU_framebuffer_ensure_config(
          &this->blit_fb_,
          {GPU_ATTACHMENT_TEXTURE_LAYER_MIP(wrap(static_cast<Texture *>(this)),
                                            static_cast<int>(dst_slice),
                                            static_cast<int>(dst_mip)),
           GPU_ATTACHMENT_NONE});
    }
    else {
      /* COLOR TEX */
      GPU_framebuffer_ensure_config(
          &this->blit_fb_,
          {GPU_ATTACHMENT_NONE,
           GPU_ATTACHMENT_TEXTURE_LAYER_MIP(wrap(static_cast<Texture *>(this)),
                                            static_cast<int>(dst_slice),
                                            static_cast<int>(dst_mip))});
    }
    this->blit_fb_slice_ = dst_slice;
    this->blit_fb_mip_ = dst_mip;
  }

  BLI_assert(this->blit_fb_);
  return this->blit_fb_;
}

MTLSamplerState gpu::MTLTexture::get_sampler_state()
{
  MTLSamplerState sampler_state;
  sampler_state.state = this->sampler_state;
  /* Add more parameters as needed */
  return sampler_state;
}

void gpu::MTLTexture::update_sub(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  /* Fetch active context. */
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(ctx);

  /* Do not update texture view. */
  BLI_assert(this->resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW);

  /* Ensure mipmaps. */
  this->ensure_mipmaps(mip);

  /* Ensure texture is baked. */
  this->ensure_baked();

  /* Safety checks. */
#if TRUST_NO_ONE
  BLI_assert(mip >= this->mip_min_ && mip <= this->mip_max_);
  BLI_assert(mip < this->texture_.mipmapLevelCount);
  BLI_assert(this->texture_.mipmapLevelCount >= this->mip_max_);
#endif

  /* DEPTH FLAG - Depth formats cannot use direct BLIT - pass off to their own routine which will
   * do a depth-only render. */
  bool is_depth_format = (this->format_flag_ & GPU_FORMAT_DEPTH);
  if (is_depth_format) {
    switch (this->type_) {

      case GPU_TEXTURE_2D: {
        update_sub_depth_2d(mip, offset, extent, type, data);
        return;
      }
      default:
        MTL_LOG_ERROR(
            "[Error] gpu::MTLTexture::update_sub not yet supported for other depth "
            "configurations\n");
        return;
        return;
    }
  }

  @autoreleasepool {
    /* Determine totalsize of INPUT Data. */
    int num_channels = to_component_len(this->format_);
    int input_bytes_per_pixel = num_channels * to_bytesize(type);
    int totalsize = 0;

    /* If unpack row length is used, size of input data uses the unpack row length, rather than the
     * image length. */
    int expected_update_w = ((ctx->pipeline_state.unpack_row_length == 0) ?
                                 extent[0] :
                                 ctx->pipeline_state.unpack_row_length);

    /* Ensure calculated total size isn't larger than remaining image data size */
    switch (this->dimensions_count()) {
      case 1:
        totalsize = input_bytes_per_pixel * max_ii(expected_update_w, 1);
        break;
      case 2:
        totalsize = input_bytes_per_pixel * max_ii(expected_update_w, 1) * max_ii(extent[1], 1);
        break;
      case 3:
        totalsize = input_bytes_per_pixel * max_ii(expected_update_w, 1) * max_ii(extent[1], 1) *
                    max_ii(extent[2], 1);
        break;
      default:
        BLI_assert(false);
        break;
    }

    /* When unpack row length is used, provided data does not necessarily contain padding for last
     * row, so we only include up to the end of updated data. */
    if (ctx->pipeline_state.unpack_row_length > 0) {
      BLI_assert(ctx->pipeline_state.unpack_row_length >= extent[0]);
      totalsize -= (ctx->pipeline_state.unpack_row_length - extent[0]) * input_bytes_per_pixel;
    }

    /* Check */
    BLI_assert(totalsize > 0);

    /* Determine expected destination data size. */
    MTLPixelFormat destination_format = gpu_texture_format_to_metal(this->format_);
    int expected_dst_bytes_per_pixel = get_mtl_format_bytesize(destination_format);
    int destination_num_channels = get_mtl_format_num_components(destination_format);
    int destination_totalsize = 0;
    switch (this->dimensions_count()) {
      case 1:
        destination_totalsize = expected_dst_bytes_per_pixel * max_ii(expected_update_w, 1);
        break;
      case 2:
        destination_totalsize = expected_dst_bytes_per_pixel * max_ii(expected_update_w, 1) *
                                max_ii(extent[1], 1);
        break;
      case 3:
        destination_totalsize = expected_dst_bytes_per_pixel * max_ii(expected_update_w, 1) *
                                max_ii(extent[1], 1) * max_ii(extent[2], 1);
        break;
      default:
        BLI_assert(false);
        break;
    }

    /* Prepare specialisation struct (For texture update routine). */
    TextureUpdateRoutineSpecialisation compute_specialisation_kernel = {
        tex_data_format_to_msl_type_str(type),              /* INPUT DATA FORMAT */
        tex_data_format_to_msl_texture_template_type(type), /* TEXTURE DATA FORMAT */
        num_channels,
        destination_num_channels};

    /* Determine whether we can do direct BLIT or not. */
    bool can_use_direct_blit = true;
    if (expected_dst_bytes_per_pixel != input_bytes_per_pixel ||
        num_channels != destination_num_channels) {
      can_use_direct_blit = false;
    }

#if MTL_VALIDATION_CRASH_DEPTH_1_1_1_WA
    if (this->type_ == GPU_TEXTURE_2D || this->type_ == GPU_TEXTURE_2D_ARRAY) {
      /* Workaround for crash in validation layer when blitting to depth2D target with
       * dimensions (1, 1, 1); */
      if (extent[0] == 1 && extent[1] == 1 && extent[2] == 1 && totalsize == 4) {
        can_use_direct_blit = false;
      }
    }
#endif

    if (this->format_ == GPU_SRGB8_A8 && !can_use_direct_blit) {
      MTL_LOG_WARNING(
          "SRGB data upload does not work correctly using compute upload. "
          "texname '%s'\n",
          this->name_);
    }

    /* Safety Checks. */
    if (type == GPU_DATA_UINT_24_8 || type == GPU_DATA_10_11_11_REV) {
      BLI_assert(can_use_direct_blit &&
                 "Special input data type must be a 1-1 mapping with destination texture as it "
                 "cannot easily be split");
    }

    /* Debug and verification. */
    if (!can_use_direct_blit) {
      MTL_LOG_WARNING(
          "gpu::MTLTexture::update_sub supplied bpp is %d bytes (%d components per "
          "pixel), but backing texture bpp is %d bytes (%d components per pixel) "
          "(TODO(Metal): Channel Conversion needed) (w: %d, h: %d, d: %d)\n",
          input_bytes_per_pixel,
          num_channels,
          expected_dst_bytes_per_pixel,
          destination_num_channels,
          w_,
          h_,
          d_);

      /* Check mip compatibility. */
      if (mip != 0) {
        MTL_LOG_ERROR(
            "[Error]: Updating texture layers other than mip=0 when data is mismatched is not "
            "possible in METAL on macOS using texture->write\n");
        return;
      }

      /* Check Format write-ability. */
      if (mtl_format_get_writeable_view_format(destination_format) == MTLPixelFormatInvalid) {
        MTL_LOG_ERROR(
            "[Error]: Updating texture -- destination MTLPixelFormat '%d' does not support write "
            "operations, and no suitable TextureView format exists.\n",
            *(int *)(&destination_format));
        return;
      }
    }

    /* Debug hook for performing GPU capture of routine. */
    bool DO_CAPTURE = false;
#if DEBUG_TEXTURE_UPDATE_CAPTURE == 1
    DO_CAPTURE = true;
    if (DO_CAPTURE) {
      MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
      MTLCaptureDescriptor *capture_descriptor = [[MTLCaptureDescriptor alloc] init];
      capture_descriptor.captureObject = ctx->device;
      NSError *error;
      if (![capture_manager startCaptureWithDescriptor:capture_descriptor error:&error]) {
        NSString *error_str = [NSString stringWithFormat:@"%@", error];
        const char *error_c_str = [error_str UTF8String];
        MTL_LOG_ERROR("Failed to start capture. Error: %s\n", error_c_str);
      }
    }
#endif

    /* Fetch or Create command buffer. */
    id<MTLCommandBuffer> cmd_buffer = ctx->get_active_command_buffer();
    bool own_command_buffer = false;
    if (cmd_buffer == nil || DO_CAPTURE) {
      cmd_buffer = [ctx->queue commandBuffer];
      own_command_buffer = true;
    }
    else {
      /* Finish graphics work. */
      ctx->end_render_pass();
    }

    /* Prepare staging buffer for data. */
    id<MTLBuffer> staging_buffer = nil;
    unsigned long long staging_buffer_offset = 0;

    /* Fetch allocation from scratch buffer. */
    MTLTemporaryBufferRange allocation; /* TODO(Metal): Metal Memory manager. */
    /* = ctx->get_memory_manager().scratch_buffer_allocate_range_aligned(totalsize, 256);*/
    memcpy(allocation.host_ptr, data, totalsize);
    staging_buffer = allocation.metal_buffer;
    if (own_command_buffer) {
      if (allocation.requires_flush()) {
        [staging_buffer didModifyRange:NSMakeRange(allocation.buffer_offset, allocation.size)];
      }
    }
    staging_buffer_offset = allocation.buffer_offset;

    /* Common Properties. */
    MTLPixelFormat compatible_write_format = mtl_format_get_writeable_view_format(
        destination_format);

    /* Some texture formats are not writeable so we need to use a texture view. */
    if (compatible_write_format == MTLPixelFormatInvalid) {
      MTL_LOG_ERROR("Cannot use compute update blit with texture-view format: %d\n",
                    *((int *)&compatible_write_format));
      return;
    }
    id<MTLTexture> texture_handle = ((compatible_write_format == destination_format)) ?
                                        this->texture_ :
                                        [this->texture_
                                            newTextureViewWithPixelFormat:compatible_write_format];

    /* Prepare encoders */
    id<MTLBlitCommandEncoder> blit_encoder = nil;
    id<MTLComputeCommandEncoder> compute_encoder = nil;
    if (can_use_direct_blit) {
      blit_encoder = [cmd_buffer blitCommandEncoder];
      BLI_assert(blit_encoder != nil);
    }
    else {
      compute_encoder = [cmd_buffer computeCommandEncoder];
      BLI_assert(compute_encoder != nil);
    }

    switch (this->type_) {

      /* 1D */
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_1D_ARRAY: {
        if (can_use_direct_blit) {
          /* Use Blit based update. */
          int bytes_per_row = expected_dst_bytes_per_pixel *
                              ((ctx->pipeline_state.unpack_row_length == 0) ?
                                   extent[0] :
                                   ctx->pipeline_state.unpack_row_length);
          int bytes_per_image = bytes_per_row;
          int max_array_index = ((this->type_ == GPU_TEXTURE_1D_ARRAY) ? extent[1] : 1);
          for (int array_index = 0; array_index < max_array_index; array_index++) {

            int buffer_array_offset = staging_buffer_offset + (bytes_per_image * array_index);
            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:buffer_array_offset
                       sourceBytesPerRow:bytes_per_row
                     sourceBytesPerImage:bytes_per_image
                              sourceSize:MTLSizeMake(extent[0], 1, 1)
                               toTexture:texture_handle
                        destinationSlice:((this->type_ == GPU_TEXTURE_1D_ARRAY) ?
                                              (array_index + offset[1]) :
                                              0)
                        destinationLevel:mip
                       destinationOrigin:MTLOriginMake(offset[0], 0, 0)];
          }
        }
        else {
          /* Use Compute Based update. */
          if (this->type_ == GPU_TEXTURE_1D) {
            id<MTLComputePipelineState> pso = texture_update_1d_get_kernel(
                compute_specialisation_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], 1, 1},
                                          {offset[0], 0, 0},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};
            [compute_encoder setComputePipelineState:pso];
            [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
            [compute_encoder setBuffer:staging_buffer offset:staging_buffer_offset atIndex:1];
            [compute_encoder setTexture:texture_handle atIndex:0];
            [compute_encoder
                      dispatchThreads:MTLSizeMake(extent[0], 1, 1) /* Width, Height, Layer */
                threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
          }
          else if (this->type_ == GPU_TEXTURE_1D_ARRAY) {
            id<MTLComputePipelineState> pso = texture_update_1d_array_get_kernel(
                compute_specialisation_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], extent[1], 1},
                                          {offset[0], offset[1], 0},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};
            [compute_encoder setComputePipelineState:pso];
            [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
            [compute_encoder setBuffer:staging_buffer offset:staging_buffer_offset atIndex:1];
            [compute_encoder setTexture:texture_handle atIndex:0];
            [compute_encoder
                      dispatchThreads:MTLSizeMake(extent[0], extent[1], 1) /* Width, layers, nil */
                threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          }
        }
      } break;

      /* 2D */
      case GPU_TEXTURE_2D:
      case GPU_TEXTURE_2D_ARRAY: {
        if (can_use_direct_blit) {
          /* Use Blit encoder update. */
          int bytes_per_row = expected_dst_bytes_per_pixel *
                              ((ctx->pipeline_state.unpack_row_length == 0) ?
                                   extent[0] :
                                   ctx->pipeline_state.unpack_row_length);
          int bytes_per_image = bytes_per_row * extent[1];

          int texture_array_relative_offset = 0;
          int base_slice = (this->type_ == GPU_TEXTURE_2D_ARRAY) ? offset[2] : 0;
          int final_slice = base_slice + ((this->type_ == GPU_TEXTURE_2D_ARRAY) ? extent[2] : 1);

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {

            if (array_slice > 0) {
              BLI_assert(this->type_ == GPU_TEXTURE_2D_ARRAY);
              BLI_assert(array_slice < this->d_);
            }

            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:staging_buffer_offset + texture_array_relative_offset
                       sourceBytesPerRow:bytes_per_row
                     sourceBytesPerImage:bytes_per_image
                              sourceSize:MTLSizeMake(extent[0], extent[1], 1)
                               toTexture:texture_handle
                        destinationSlice:array_slice
                        destinationLevel:mip
                       destinationOrigin:MTLOriginMake(offset[0], offset[1], 0)];

            texture_array_relative_offset += bytes_per_image;
          }
        }
        else {
          /* Use Compute texture update. */
          if (this->type_ == GPU_TEXTURE_2D) {
            id<MTLComputePipelineState> pso = texture_update_2d_get_kernel(
                compute_specialisation_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], extent[1], 1},
                                          {offset[0], offset[1], 0},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};
            [compute_encoder setComputePipelineState:pso];
            [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
            [compute_encoder setBuffer:staging_buffer offset:staging_buffer_offset atIndex:1];
            [compute_encoder setTexture:texture_handle atIndex:0];
            [compute_encoder
                      dispatchThreads:MTLSizeMake(
                                          extent[0], extent[1], 1) /* Width, Height, Layer */
                threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          }
          else if (this->type_ == GPU_TEXTURE_2D_ARRAY) {
            id<MTLComputePipelineState> pso = texture_update_2d_array_get_kernel(
                compute_specialisation_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], extent[1], extent[2]},
                                          {offset[0], offset[1], offset[2]},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};
            [compute_encoder setComputePipelineState:pso];
            [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
            [compute_encoder setBuffer:staging_buffer offset:staging_buffer_offset atIndex:1];
            [compute_encoder setTexture:texture_handle atIndex:0];
            [compute_encoder dispatchThreads:MTLSizeMake(extent[0],
                                                         extent[1],
                                                         extent[2]) /* Width, Height, Layer */
                       threadsPerThreadgroup:MTLSizeMake(4, 4, 4)];
          }
        }

      } break;

      /* 3D */
      case GPU_TEXTURE_3D: {
        if (can_use_direct_blit) {
          int bytes_per_row = expected_dst_bytes_per_pixel *
                              ((ctx->pipeline_state.unpack_row_length == 0) ?
                                   extent[0] :
                                   ctx->pipeline_state.unpack_row_length);
          int bytes_per_image = bytes_per_row * extent[1];
          [blit_encoder copyFromBuffer:staging_buffer
                          sourceOffset:staging_buffer_offset
                     sourceBytesPerRow:bytes_per_row
                   sourceBytesPerImage:bytes_per_image
                            sourceSize:MTLSizeMake(extent[0], extent[1], extent[2])
                             toTexture:texture_handle
                      destinationSlice:0
                      destinationLevel:mip
                     destinationOrigin:MTLOriginMake(offset[0], offset[1], offset[2])];
        }
        else {
          id<MTLComputePipelineState> pso = texture_update_3d_get_kernel(
              compute_specialisation_kernel);
          TextureUpdateParams params = {mip,
                                        {extent[0], extent[1], extent[2]},
                                        {offset[0], offset[1], offset[2]},
                                        ((ctx->pipeline_state.unpack_row_length == 0) ?
                                             extent[0] :
                                             ctx->pipeline_state.unpack_row_length)};
          [compute_encoder setComputePipelineState:pso];
          [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
          [compute_encoder setBuffer:staging_buffer offset:staging_buffer_offset atIndex:1];
          [compute_encoder setTexture:texture_handle atIndex:0];
          [compute_encoder
                    dispatchThreads:MTLSizeMake(
                                        extent[0], extent[1], extent[2]) /* Width, Height, Depth */
              threadsPerThreadgroup:MTLSizeMake(4, 4, 4)];
        }
      } break;

      /* CUBE */
      case GPU_TEXTURE_CUBE: {
        if (can_use_direct_blit) {
          int bytes_per_row = expected_dst_bytes_per_pixel *
                              ((ctx->pipeline_state.unpack_row_length == 0) ?
                                   extent[0] :
                                   ctx->pipeline_state.unpack_row_length);
          int bytes_per_image = bytes_per_row * extent[1];

          int texture_array_relative_offset = 0;

          /* Iterate over all cube faces in range (offset[2], offset[2] + extent[2]). */
          for (int i = 0; i < extent[2]; i++) {
            int face_index = offset[2] + i;

            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:staging_buffer_offset + texture_array_relative_offset
                       sourceBytesPerRow:bytes_per_row
                     sourceBytesPerImage:bytes_per_image
                              sourceSize:MTLSizeMake(extent[0], extent[1], 1)
                               toTexture:texture_handle
                        destinationSlice:face_index /* = cubeFace+arrayIndex*6 */
                        destinationLevel:mip
                       destinationOrigin:MTLOriginMake(offset[0], offset[1], 0)];
            texture_array_relative_offset += bytes_per_image;
          }
        }
        else {
          MTL_LOG_ERROR(
              "TODO(Metal): Support compute texture update for GPU_TEXTURE_CUBE %d, %d, %d\n",
              w_,
              h_,
              d_);
        }
      } break;

      case GPU_TEXTURE_CUBE_ARRAY: {
        if (can_use_direct_blit) {

          int bytes_per_row = expected_dst_bytes_per_pixel *
                              ((ctx->pipeline_state.unpack_row_length == 0) ?
                                   extent[0] :
                                   ctx->pipeline_state.unpack_row_length);
          int bytes_per_image = bytes_per_row * extent[1];

          /* Upload to all faces between offset[2] (which is zero in most cases) AND extent[2]. */
          int texture_array_relative_offset = 0;
          for (int i = 0; i < extent[2]; i++) {
            int face_index = offset[2] + i;
            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:staging_buffer_offset + texture_array_relative_offset
                       sourceBytesPerRow:bytes_per_row
                     sourceBytesPerImage:bytes_per_image
                              sourceSize:MTLSizeMake(extent[0], extent[1], 1)
                               toTexture:texture_handle
                        destinationSlice:face_index /* = cubeFace+arrayIndex*6. */
                        destinationLevel:mip
                       destinationOrigin:MTLOriginMake(offset[0], offset[1], 0)];
            texture_array_relative_offset += bytes_per_image;
          }
        }
        else {
          MTL_LOG_ERROR(
              "TODO(Metal): Support compute texture update for GPU_TEXTURE_CUBE_ARRAY %d, %d, "
              "%d\n",
              w_,
              h_,
              d_);
        }
      } break;

      case GPU_TEXTURE_BUFFER: {
        /* TODO(Metal): Support Data upload to TEXTURE BUFFER
         * Data uploads generally happen via GPUVertBuf instead. */
        BLI_assert(false);
      } break;

      case GPU_TEXTURE_ARRAY:
        /* Not an actual format - modifier flag for others. */
        return;
    }

    /* Finalize Blit Encoder. */
    if (can_use_direct_blit) {

      /* Textures which use MTLStorageModeManaged need to have updated contents
       * synced back to CPU to avoid an automatic flush overwriting contents. */
      if (texture_.storageMode == MTLStorageModeManaged) {
        [blit_encoder synchronizeResource:texture_buffer_];
      }

      /* End Encoding. */
      [blit_encoder endEncoding];
    }
    else {

      /* End Encoding. */
      [compute_encoder endEncoding];

      /* Textures which use MTLStorageModeManaged need to have updated contents
       * synced back to CPU to avoid an automatic flush overwriting contents. */
      if (texture_.storageMode == MTLStorageModeManaged) {
        blit_encoder = [cmd_buffer blitCommandEncoder];
        [blit_encoder synchronizeResource:texture_buffer_];
        [blit_encoder endEncoding];
      }
    }

    if (own_command_buffer) {
      [cmd_buffer commit];
    }

#if DEBUG_TEXTURE_UPDATE_CAPTURE == 1
    if (DO_CAPTURE) {
      [cmd_buffer waitUntilCompleted];
      MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
      [capture_manager stopCapture];
    }
#endif
  }
}

void gpu::MTLTexture::ensure_mipmaps(int miplvl)
{

  /* Do not update texture view. */
  BLI_assert(this->resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW);

  /* Clamp level to maximum. */
  int effective_h = (this->type_ == GPU_TEXTURE_1D_ARRAY) ? 0 : this->h_;
  int effective_d = (this->type_ != GPU_TEXTURE_3D) ? 0 : this->d_;
  int max_dimension = max_iii(this->w_, effective_h, effective_d);
  int max_miplvl = floor(log2(max_dimension));
  miplvl = min_ii(max_miplvl, miplvl);

  /* Increase mipmap level. */
  if (mipmaps_ < miplvl) {
    mipmaps_ = miplvl;

    /* Check if baked. */
    if (this->is_baked_ && mipmaps_ > mtl_max_mips_) {
      this->is_dirty_ = true;
      MTL_LOG_WARNING("Texture requires regenerating due to increase in mip-count\n");
    }
  }
  this->mip_range_set(0, mipmaps_);
}

void gpu::MTLTexture::generate_mipmap(void)
{
  /* Fetch Active Context. */
  MTLContext *ctx = reinterpret_cast<MTLContext *>(GPU_context_active_get());
  BLI_assert(ctx);

  if (!ctx->device) {
    MTL_LOG_ERROR("Cannot Generate mip-maps -- metal device invalid\n");
    BLI_assert(false);
    return;
  }

  /* Ensure mipmaps. */
  this->ensure_mipmaps(9999);

  /* Ensure texture is baked. */
  this->ensure_baked();
  BLI_assert(this->is_baked_ && this->texture_ && "MTLTexture is not valid");

  if (this->mipmaps_ == 1 || this->mtl_max_mips_ == 1) {
    MTL_LOG_WARNING("Call to generate mipmaps on texture with 'mipmaps_=1\n'");
    return;
  }

  /* Verify if we can perform mipmap generation. */
  if (this->format_ == GPU_DEPTH_COMPONENT32F || this->format_ == GPU_DEPTH_COMPONENT24 ||
      this->format_ == GPU_DEPTH_COMPONENT16 || this->format_ == GPU_DEPTH32F_STENCIL8 ||
      this->format_ == GPU_DEPTH24_STENCIL8) {
    MTL_LOG_WARNING("Cannot generate mipmaps for textures using DEPTH formats\n");
    return;
  }

  @autoreleasepool {

    id<MTLCommandBuffer> cmd_buffer = ctx->get_active_command_buffer();
    bool own_command_buffer = false;
    if (cmd_buffer == nil) {
      cmd_buffer = [ctx->queue commandBuffer];
      own_command_buffer = true;
    }
    else {
      /* End active graphics work. */
      ctx->end_render_pass();
    }

    id<MTLBlitCommandEncoder> enc = [cmd_buffer blitCommandEncoder];
#if MTL_DEBUG_COMMAND_BUFFER_EXECUTION
    [enc insertDebugSignpost:@"Generate MipMaps"];
#endif
    [enc generateMipmapsForTexture:this->texture_];
    [enc endEncoding];

    if (own_command_buffer) {
      [cmd_buffer commit];
    }
  }
  return;
}

void gpu::MTLTexture::copy_to(Texture *dst)
{
  /* Safety Checks. */
  gpu::MTLTexture *mt_src = this;
  gpu::MTLTexture *mt_dst = static_cast<gpu::MTLTexture *>(dst);
  BLI_assert((mt_dst->w_ == mt_src->w_) && (mt_dst->h_ == mt_src->h_) &&
             (mt_dst->d_ == mt_src->d_));
  BLI_assert(mt_dst->format_ == mt_src->format_);
  BLI_assert(mt_dst->type_ == mt_src->type_);

  UNUSED_VARS_NDEBUG(mt_src);

  /* Fetch active context. */
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(ctx);

  /* Ensure texture is baked. */
  this->ensure_baked();

  @autoreleasepool {
    /* End render pass. */
    ctx->end_render_pass();

    /* Setup blit encoder. */
    id<MTLCommandBuffer> cmd_buffer = ctx->get_active_command_buffer();
    BLI_assert(cmd_buffer != nil);
    id<MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
    BLI_assert(blit_encoder != nil);

    /* TODO(Metal): Consider supporting multiple mip levels IF the GL implementation
     * follows, currently it does not. */
    int mip = 0;

    /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
    int extent[3] = {1, 1, 1};
    this->mip_size_get(mip, extent);

    switch (mt_dst->type_) {
      case GPU_TEXTURE_2D_ARRAY:
      case GPU_TEXTURE_CUBE_ARRAY:
      case GPU_TEXTURE_3D: {
        /* Do full texture copy for 3D textures */
        BLI_assert(mt_dst->d_ == this->d_);
        [blit_encoder copyFromTexture:this->get_metal_handle_base()
                            toTexture:mt_dst->get_metal_handle_base()];
      } break;
      default: {
        int slice = 0;
        this->blit(blit_encoder,
                   0,
                   0,
                   0,
                   slice,
                   mip,
                   mt_dst,
                   0,
                   0,
                   0,
                   slice,
                   mip,
                   extent[0],
                   extent[1],
                   extent[2]);
      } break;
    }

    /* End encoding */
    [blit_encoder endEncoding];
  }
}

void gpu::MTLTexture::clear(eGPUDataFormat data_format, const void *data)
{
  /* Ensure texture is baked. */
  this->ensure_baked();

  /* Create clear framebuffer. */
  GPUFrameBuffer *prev_fb = GPU_framebuffer_active_get();
  FrameBuffer *fb = reinterpret_cast<FrameBuffer *>(this->get_blit_framebuffer(0, 0));
  fb->bind(true);
  fb->clear_attachment(this->attachment_type(0), data_format, data);
  GPU_framebuffer_bind(prev_fb);
}

static MTLTextureSwizzle swizzle_to_mtl(const char swizzle)
{
  switch (swizzle) {
    default:
    case 'x':
    case 'r':
      return MTLTextureSwizzleRed;
    case 'y':
    case 'g':
      return MTLTextureSwizzleGreen;
    case 'z':
    case 'b':
      return MTLTextureSwizzleBlue;
    case 'w':
    case 'a':
      return MTLTextureSwizzleAlpha;
    case '0':
      return MTLTextureSwizzleZero;
    case '1':
      return MTLTextureSwizzleOne;
  }
}

void gpu::MTLTexture::swizzle_set(const char swizzle_mask[4])
{
  if (memcmp(this->tex_swizzle_mask_, swizzle_mask, 4) != 0) {
    memcpy(this->tex_swizzle_mask_, swizzle_mask, 4);

    /* Creating the swizzle mask and flagging as dirty if changed. */
    MTLTextureSwizzleChannels new_swizzle_mask = MTLTextureSwizzleChannelsMake(
        swizzle_to_mtl(swizzle_mask[0]),
        swizzle_to_mtl(swizzle_mask[1]),
        swizzle_to_mtl(swizzle_mask[2]),
        swizzle_to_mtl(swizzle_mask[3]));

    this->mtl_swizzle_mask_ = new_swizzle_mask;
    this->texture_view_dirty_flags_ |= TEXTURE_VIEW_SWIZZLE_DIRTY;
  }
}

void gpu::MTLTexture::mip_range_set(int min, int max)
{
  BLI_assert(min <= max && min >= 0 && max <= mipmaps_);

  /* NOTE:
   * - mip_min_ and mip_max_ are used to Clamp LODs during sampling.
   * - Given functions like Framebuffer::recursive_downsample modifies the mip range
   *   between each layer, we do not want to be re-baking the texture.
   * - For the time being, we are going to just need to generate a FULL mipmap chain
   *   as we do not know ahead of time whether mipmaps will be used.
   *
   *   TODO(Metal): Add texture initialization flag to determine whether mipmaps are used
   *   or not. Will be important for saving memory for big textures. */
  this->mip_min_ = min;
  this->mip_max_ = max;

  if ((this->type_ == GPU_TEXTURE_1D || this->type_ == GPU_TEXTURE_1D_ARRAY ||
       this->type_ == GPU_TEXTURE_BUFFER) &&
      max > 1) {

    MTL_LOG_ERROR(
        " MTLTexture of type TEXTURE_1D_ARRAY or TEXTURE_BUFFER cannot have a mipcount "
        "greater than 1\n");
    this->mip_min_ = 0;
    this->mip_max_ = 0;
    this->mipmaps_ = 0;
    BLI_assert(false);
  }

  /* Mip range for texture view. */
  this->mip_texture_base_level_ = this->mip_min_;
  this->mip_texture_max_level_ = this->mip_max_;
  texture_view_dirty_flags_ |= TEXTURE_VIEW_MIP_DIRTY;
}

void *gpu::MTLTexture::read(int mip, eGPUDataFormat type)
{
  /* Prepare Array for return data. */
  BLI_assert(!(format_flag_ & GPU_FORMAT_COMPRESSED));
  BLI_assert(mip <= mipmaps_);
  BLI_assert(validate_data_format_mtl(this->format_, type));

  /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
  int extent[3] = {1, 1, 1};
  this->mip_size_get(mip, extent);

  size_t sample_len = extent[0] * extent[1] * extent[2];
  size_t sample_size = to_bytesize(format_, type);
  size_t texture_size = sample_len * sample_size;
  int num_channels = to_component_len(this->format_);

  void *data = MEM_mallocN(texture_size + 8, "GPU_texture_read");

  /* Ensure texture is baked. */
  if (this->is_baked_) {
    this->read_internal(
        mip, 0, 0, 0, extent[0], extent[1], extent[2], type, num_channels, texture_size + 8, data);
  }
  else {
    /* Clear return values? */
    MTL_LOG_WARNING("MTLTexture::read - reading from texture with no image data\n");
  }

  return data;
}

/* Fetch the raw buffer data from a texture and copy to CPU host ptr. */
void gpu::MTLTexture::read_internal(int mip,
                                    int x_off,
                                    int y_off,
                                    int z_off,
                                    int width,
                                    int height,
                                    int depth,
                                    eGPUDataFormat desired_output_format,
                                    int num_output_components,
                                    int debug_data_size,
                                    void *r_data)
{
  /* Verify textures are baked. */
  if (!this->is_baked_) {
    MTL_LOG_WARNING("gpu::MTLTexture::read_internal - Trying to read from a non-baked texture!\n");
    return;
  }
  /* Fetch active context. */
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(ctx);

  /* Calculate Desired output size. */
  int num_channels = to_component_len(this->format_);
  BLI_assert(num_output_components <= num_channels);
  unsigned int desired_output_bpp = num_output_components * to_bytesize(desired_output_format);

  /* Calculate Metal data output for trivial copy. */
  unsigned int image_bpp = get_mtl_format_bytesize(this->texture_.pixelFormat);
  unsigned int image_components = get_mtl_format_num_components(this->texture_.pixelFormat);
  bool is_depth_format = (this->format_flag_ & GPU_FORMAT_DEPTH);

  /* Verify if we need to use compute read. */
  eGPUDataFormat data_format = to_mtl_internal_data_format(this->format_get());
  bool format_conversion_needed = (data_format != desired_output_format);
  bool can_use_simple_read = (desired_output_bpp == image_bpp) && (!format_conversion_needed) &&
                             (num_output_components == image_components);

  /* Depth must be read using the compute shader -- Some safety checks to verify that params are
   * correct. */
  if (is_depth_format) {
    can_use_simple_read = false;
    /* TODO(Metal): Stencil data write not yet supported, so force components to one. */
    image_components = 1;
    BLI_assert(num_output_components == 1);
    BLI_assert(image_components == 1);
    BLI_assert(data_format == GPU_DATA_FLOAT || data_format == GPU_DATA_UINT_24_8);
    BLI_assert(validate_data_format_mtl(this->format_, data_format));
  }

  /* SPECIAL Workaround for R11G11B10 textures requesting a read using: GPU_DATA_10_11_11_REV. */
  if (desired_output_format == GPU_DATA_10_11_11_REV) {
    BLI_assert(this->format_ == GPU_R11F_G11F_B10F);

    /* override parameters - we'll be able to use simple copy, as bpp will match at 4 bytes. */
    image_bpp = sizeof(int);
    image_components = 1;
    desired_output_bpp = sizeof(int);
    num_output_components = 1;

    data_format = GPU_DATA_INT;
    format_conversion_needed = false;
    can_use_simple_read = true;
  }

  /* Determine size of output data. */
  unsigned int bytes_per_row = desired_output_bpp * width;
  unsigned int bytes_per_image = bytes_per_row * height;
  unsigned int total_bytes = bytes_per_image * depth;

  if (can_use_simple_read) {
    /* DEBUG check that if direct copy is being used, then both the expected output size matches
     * the METAL texture size. */
    BLI_assert(
        ((num_output_components * to_bytesize(desired_output_format)) == desired_output_bpp) &&
        (desired_output_bpp == image_bpp));
  }
  /* DEBUG check that the allocated data size matches the bytes we expect. */
  BLI_assert(total_bytes <= debug_data_size);

  /* Fetch allocation from scratch buffer. */
  id<MTLBuffer> destination_buffer = nil;
  unsigned int destination_offset = 0;
  void *destination_buffer_host_ptr = nullptr;

  /* TODO(Metal): Optimize buffer allocation. */
  MTLResourceOptions bufferOptions = MTLResourceStorageModeManaged;
  destination_buffer = [ctx->device newBufferWithLength:max_ii(total_bytes, 256)
                                                options:bufferOptions];
  destination_offset = 0;
  destination_buffer_host_ptr = (void *)((unsigned char *)([destination_buffer contents]) +
                                         destination_offset);

  /* Prepare specialisation struct (For non-trivial texture read routine). */
  int depth_format_mode = 0;
  if (is_depth_format) {
    depth_format_mode = 1;
    switch (desired_output_format) {
      case GPU_DATA_FLOAT:
        depth_format_mode = 1;
        break;
      case GPU_DATA_UINT_24_8:
        depth_format_mode = 2;
        break;
      case GPU_DATA_UINT:
        depth_format_mode = 4;
        break;
      default:
        BLI_assert(false && "Unhandled depth read format case");
        break;
    }
  }

  TextureReadRoutineSpecialisation compute_specialisation_kernel = {
      tex_data_format_to_msl_texture_template_type(data_format), /* TEXTURE DATA TYPE */
      tex_data_format_to_msl_type_str(desired_output_format),    /* OUTPUT DATA TYPE */
      num_channels,                                              /* TEXTURE COMPONENT COUNT */
      num_output_components,                                     /* OUTPUT DATA COMPONENT COUNT */
      depth_format_mode};

  bool copy_successful = false;
  @autoreleasepool {

    bool DO_CAPTURE = false;
#if DEBUG_TEXTURE_READ_CAPTURE == 1
    DO_CAPTURE = true;
    if (DO_CAPTURE) {
      MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
      MTLCaptureDescriptor *capture_descriptor = [[MTLCaptureDescriptor alloc] init];
      capture_descriptor.captureObject = ctx->device;
      NSError *error;
      if (![capture_manager startCaptureWithDescriptor:capture_descriptor error:&error]) {
        NSString *error_str = [NSString stringWithFormat:@"%@", error];
        const char *error_c_str = [error_str UTF8String];
        MTL_LOG_ERROR("Failed to start capture. Error: %s\n", error_c_str);
      }
    }
#endif

    /* TODO(Metal): Verify whether we need some form of barrier here to ensure reads
     * happen after work with associated texture is finished. */
    GPU_finish();

    /* Fetch or Create command buffer. */
    id<MTLCommandBuffer> cmd_buffer = ctx->get_active_command_buffer();
    bool own_command_buffer = false;
    if (cmd_buffer == nil || DO_CAPTURE || true) {
      cmd_buffer = [ctx->queue commandBuffer];
      own_command_buffer = true;
    }
    else {
      /* End any graphics workloads. */
      ctx->end_render_pass();
    }

    /* Texture View for SRGB special case. */
    id<MTLTexture> read_texture = this->texture_;
    if (this->format_ == GPU_SRGB8_A8) {
      read_texture = [this->texture_ newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm];
    }

    /* Perform per-texture type read. */
    switch (this->type_) {
      case GPU_TEXTURE_2D: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = [cmd_buffer blitCommandEncoder];
#if MTL_DEBUG_COMMAND_BUFFER_EXECUTION
          [enc insertDebugSignpost:@"GPUTextureRead"];
#endif
          [enc copyFromTexture:read_texture
                           sourceSlice:0
                           sourceLevel:mip
                          sourceOrigin:MTLOriginMake(x_off, y_off, 0)
                            sourceSize:MTLSizeMake(width, height, 1)
                              toBuffer:destination_buffer
                     destinationOffset:destination_offset
                destinationBytesPerRow:bytes_per_row
              destinationBytesPerImage:bytes_per_image];
          [enc synchronizeResource:destination_buffer];
          [enc endEncoding];
          copy_successful = true;
        }
        else {

          /* Use Compute READ. */
          id<MTLComputeCommandEncoder> compute_encoder = [cmd_buffer computeCommandEncoder];
          id<MTLComputePipelineState> pso = texture_read_2d_get_kernel(
              compute_specialisation_kernel);
          TextureReadParams params = {
              mip,
              {width, height, 1},
              {x_off, y_off, 0},
          };
          [compute_encoder setComputePipelineState:pso];
          [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
          [compute_encoder setBuffer:destination_buffer offset:destination_offset atIndex:1];
          [compute_encoder setTexture:read_texture atIndex:0];
          [compute_encoder dispatchThreads:MTLSizeMake(width, height, 1) /* Width, Height, Layer */
                     threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          [compute_encoder endEncoding];

          /* Use Blit encoder to synchronize results back to CPU. */
          id<MTLBlitCommandEncoder> enc = [cmd_buffer blitCommandEncoder];
#if MTL_DEBUG_COMMAND_BUFFER_EXECUTION
          [enc insertDebugSignpost:@"GPUTextureRead-syncResource"];
#endif
          [enc synchronizeResource:destination_buffer];
          [enc endEncoding];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_2D_ARRAY: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = [cmd_buffer blitCommandEncoder];
#if MTL_DEBUG_COMMAND_BUFFER_EXECUTION
          [enc insertDebugSignpost:@"GPUTextureRead"];
#endif
          int base_slice = z_off;
          int final_slice = base_slice + depth;
          int texture_array_relative_offset = 0;

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {
            [enc copyFromTexture:read_texture
                             sourceSlice:0
                             sourceLevel:mip
                            sourceOrigin:MTLOriginMake(x_off, y_off, 0)
                              sourceSize:MTLSizeMake(width, height, 1)
                                toBuffer:destination_buffer
                       destinationOffset:destination_offset + texture_array_relative_offset
                  destinationBytesPerRow:bytes_per_row
                destinationBytesPerImage:bytes_per_image];
            [enc synchronizeResource:destination_buffer];

            texture_array_relative_offset += bytes_per_image;
          }
          [enc endEncoding];
          copy_successful = true;
        }
        else {

          /* Use Compute READ */
          id<MTLComputeCommandEncoder> compute_encoder = [cmd_buffer computeCommandEncoder];
          id<MTLComputePipelineState> pso = texture_read_2d_array_get_kernel(
              compute_specialisation_kernel);
          TextureReadParams params = {
              mip,
              {width, height, depth},
              {x_off, y_off, z_off},
          };
          [compute_encoder setComputePipelineState:pso];
          [compute_encoder setBytes:&params length:sizeof(params) atIndex:0];
          [compute_encoder setBuffer:destination_buffer offset:destination_offset atIndex:1];
          [compute_encoder setTexture:read_texture atIndex:0];
          [compute_encoder
                    dispatchThreads:MTLSizeMake(width, height, depth) /* Width, Height, Layer */
              threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          [compute_encoder endEncoding];

          /* Use Blit encoder to synchronize results back to CPU. */
          id<MTLBlitCommandEncoder> enc = [cmd_buffer blitCommandEncoder];
#if MTL_DEBUG_COMMAND_BUFFER_EXECUTION
          [enc insertDebugSignpost:@"GPUTextureRead-syncResource"];
#endif
          [enc synchronizeResource:destination_buffer];
          [enc endEncoding];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_CUBE_ARRAY: {
        if (can_use_simple_read) {
          id<MTLBlitCommandEncoder> enc = [cmd_buffer blitCommandEncoder];
#if MTL_DEBUG_COMMAND_BUFFER_EXECUTION
          [enc insertDebugSignpost:@"GPUTextureRead"];
#endif
          int base_slice = z_off;
          int final_slice = base_slice + depth;
          int texture_array_relative_offset = 0;

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {
            [enc copyFromTexture:read_texture
                             sourceSlice:array_slice
                             sourceLevel:mip
                            sourceOrigin:MTLOriginMake(x_off, y_off, 0)
                              sourceSize:MTLSizeMake(width, height, 1)
                                toBuffer:destination_buffer
                       destinationOffset:destination_offset + texture_array_relative_offset
                  destinationBytesPerRow:bytes_per_row
                destinationBytesPerImage:bytes_per_image];
            [enc synchronizeResource:destination_buffer];

            texture_array_relative_offset += bytes_per_image;
          }
          MTL_LOG_INFO("Copying texture data to buffer GPU_TEXTURE_CUBE_ARRAY\n");
          [enc endEncoding];
          copy_successful = true;
        }
        else {
          MTL_LOG_ERROR("TODO(Metal): unsupported compute copy of texture cube array");
        }
      } break;

      default:
        MTL_LOG_WARNING(
            "[Warning] gpu::MTLTexture::read_internal simple-copy not yet supported for texture "
            "type: %d\n",
            (int)this->type_);
        break;
    }

    if (copy_successful) {
      /* Ensure GPU copy from texture to host-accessible buffer is complete. */
      if (own_command_buffer) {
        [cmd_buffer commit];
        [cmd_buffer waitUntilCompleted];
      }
      else {
        /* Ensure GPU copy commands have completed. */
        GPU_finish();
      }

#if DEBUG_TEXTURE_READ_CAPTURE == 1
      if (DO_CAPTURE) {
        MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
        [capture_manager stopCapture];
      }
#endif

      /* Copy data from Shared Memory into ptr. */
      memcpy(r_data, destination_buffer_host_ptr, total_bytes);
      MTL_LOG_INFO("gpu::MTLTexture::read_internal success! %d bytes read\n", total_bytes);
    }
    else {
      MTL_LOG_WARNING(
          "[Warning] gpu::MTLTexture::read_internal not yet supported for this config -- data "
          "format different (src %d bytes, dst %d bytes) (src format: %d, dst format: %d), or "
          "varying component counts (src %d, dst %d)\n",
          image_bpp,
          desired_output_bpp,
          (int)data_format,
          (int)desired_output_format,
          image_components,
          num_output_components);
    }
  }
}

/* Remove once no longer required -- will just return 0 for now in MTL path. */
uint gpu::MTLTexture::gl_bindcode_get(void) const
{
  return 0;
}

bool gpu::MTLTexture::init_internal(void)
{
  if (this->format_ == GPU_DEPTH24_STENCIL8) {
    /* Apple Silicon requires GPU_DEPTH32F_STENCIL8 instead of GPU_DEPTH24_STENCIL8. */
    this->format_ = GPU_DEPTH32F_STENCIL8;
  }

  this->prepare_internal();
  return true;
}

bool gpu::MTLTexture::init_internal(GPUVertBuf *vbo)
{
  /* Zero initialize. */
  this->prepare_internal();

  /* TODO(Metal): Add implementation for GPU Vert buf. */
  return false;
}

bool gpu::MTLTexture::init_internal(const GPUTexture *src, int mip_offset, int layer_offset)
{
  BLI_assert(src);

  /* Zero initialize. */
  this->prepare_internal();

  /* Flag as using texture view. */
  this->resource_mode_ = MTL_TEXTURE_MODE_TEXTURE_VIEW;
  this->source_texture_ = src;
  this->mip_texture_base_level_ = mip_offset;
  this->mip_texture_base_layer_ = layer_offset;

  /* Assign texture as view. */
  const gpu::MTLTexture *mtltex = static_cast<const gpu::MTLTexture *>(unwrap(src));
  this->texture_ = mtltex->texture_;
  BLI_assert(this->texture_);
  [this->texture_ retain];

  /* Flag texture as baked -- we do not need explicit initialization. */
  this->is_baked_ = true;
  this->is_dirty_ = false;

  /* Bake mip swizzle view. */
  bake_mip_swizzle_view();
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name METAL Resource creation and management
 * \{ */

bool gpu::MTLTexture::texture_is_baked()
{
  return this->is_baked_;
}

/* Prepare texture parameters after initialization, but before baking. */
void gpu::MTLTexture::prepare_internal()
{

  /* Derive implicit usage flags for Depth/Stencil attachments. */
  if (this->format_flag_ & GPU_FORMAT_DEPTH || this->format_flag_ & GPU_FORMAT_STENCIL) {
    this->gpu_image_usage_flags_ |= GPU_TEXTURE_USAGE_ATTACHMENT;
  }

  /* Derive maximum number of mip levels by default.
   * TODO(Metal): This can be removed if max mip counts are specified upfront. */
  if (this->type_ == GPU_TEXTURE_1D || this->type_ == GPU_TEXTURE_1D_ARRAY ||
      this->type_ == GPU_TEXTURE_BUFFER) {
    this->mtl_max_mips_ = 1;
  }
  else {
    int effective_h = (this->type_ == GPU_TEXTURE_1D_ARRAY) ? 0 : this->h_;
    int effective_d = (this->type_ != GPU_TEXTURE_3D) ? 0 : this->d_;
    int max_dimension = max_iii(this->w_, effective_h, effective_d);
    int max_miplvl = max_ii(floor(log2(max_dimension)) + 1, 1);
    this->mtl_max_mips_ = max_miplvl;
  }
}

void gpu::MTLTexture::ensure_baked()
{

  /* If properties have changed, re-bake. */
  bool copy_previous_contents = false;
  if (this->is_baked_ && this->is_dirty_) {
    copy_previous_contents = true;
    id<MTLTexture> previous_texture = this->texture_;
    [previous_texture retain];

    this->reset();
  }

  if (!this->is_baked_) {
    MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
    BLI_assert(ctx);

    /* Ensure texture mode is valid. */
    BLI_assert(this->resource_mode_ != MTL_TEXTURE_MODE_EXTERNAL);
    BLI_assert(this->resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW);
    BLI_assert(this->resource_mode_ != MTL_TEXTURE_MODE_VBO);

    /* Format and mip levels (TODO(Metal): Optimize mipmaps counts, specify up-front). */
    MTLPixelFormat mtl_format = gpu_texture_format_to_metal(this->format_);

    /* Create texture descriptor. */
    switch (this->type_) {

      /* 1D */
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_1D_ARRAY: {
        BLI_assert(this->w_ > 0);
        this->texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        this->texture_descriptor_.pixelFormat = mtl_format;
        this->texture_descriptor_.textureType = (this->type_ == GPU_TEXTURE_1D_ARRAY) ?
                                                    MTLTextureType1DArray :
                                                    MTLTextureType1D;
        this->texture_descriptor_.width = this->w_;
        this->texture_descriptor_.height = 1;
        this->texture_descriptor_.depth = 1;
        this->texture_descriptor_.arrayLength = (this->type_ == GPU_TEXTURE_1D_ARRAY) ? this->h_ :
                                                                                        1;
        this->texture_descriptor_.mipmapLevelCount = (this->mtl_max_mips_ > 0) ?
                                                         this->mtl_max_mips_ :
                                                         1;
        this->texture_descriptor_.usage =
            MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite |
            MTLTextureUsagePixelFormatView; /* TODO(Metal): Optimize usage flags. */
        this->texture_descriptor_.storageMode = MTLStorageModePrivate;
        this->texture_descriptor_.sampleCount = 1;
        this->texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        this->texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* 2D */
      case GPU_TEXTURE_2D:
      case GPU_TEXTURE_2D_ARRAY: {
        BLI_assert(this->w_ > 0 && this->h_ > 0);
        this->texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        this->texture_descriptor_.pixelFormat = mtl_format;
        this->texture_descriptor_.textureType = (this->type_ == GPU_TEXTURE_2D_ARRAY) ?
                                                    MTLTextureType2DArray :
                                                    MTLTextureType2D;
        this->texture_descriptor_.width = this->w_;
        this->texture_descriptor_.height = this->h_;
        this->texture_descriptor_.depth = 1;
        this->texture_descriptor_.arrayLength = (this->type_ == GPU_TEXTURE_2D_ARRAY) ? this->d_ :
                                                                                        1;
        this->texture_descriptor_.mipmapLevelCount = (this->mtl_max_mips_ > 0) ?
                                                         this->mtl_max_mips_ :
                                                         1;
        this->texture_descriptor_.usage =
            MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite |
            MTLTextureUsagePixelFormatView; /* TODO(Metal): Optimize usage flags. */
        this->texture_descriptor_.storageMode = MTLStorageModePrivate;
        this->texture_descriptor_.sampleCount = 1;
        this->texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        this->texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* 3D */
      case GPU_TEXTURE_3D: {
        BLI_assert(this->w_ > 0 && this->h_ > 0 && this->d_ > 0);
        this->texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        this->texture_descriptor_.pixelFormat = mtl_format;
        this->texture_descriptor_.textureType = MTLTextureType3D;
        this->texture_descriptor_.width = this->w_;
        this->texture_descriptor_.height = this->h_;
        this->texture_descriptor_.depth = this->d_;
        this->texture_descriptor_.arrayLength = 1;
        this->texture_descriptor_.mipmapLevelCount = (this->mtl_max_mips_ > 0) ?
                                                         this->mtl_max_mips_ :
                                                         1;
        this->texture_descriptor_.usage =
            MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite |
            MTLTextureUsagePixelFormatView; /* TODO(Metal): Optimize usage flags. */
        this->texture_descriptor_.storageMode = MTLStorageModePrivate;
        this->texture_descriptor_.sampleCount = 1;
        this->texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        this->texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* CUBE TEXTURES */
      case GPU_TEXTURE_CUBE:
      case GPU_TEXTURE_CUBE_ARRAY: {
        /* NOTE: For a cube-map 'Texture::d_' refers to total number of faces,
         * not just array slices. */
        BLI_assert(this->w_ > 0 && this->h_ > 0);
        this->texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        this->texture_descriptor_.pixelFormat = mtl_format;
        this->texture_descriptor_.textureType = (this->type_ == GPU_TEXTURE_CUBE_ARRAY) ?
                                                    MTLTextureTypeCubeArray :
                                                    MTLTextureTypeCube;
        this->texture_descriptor_.width = this->w_;
        this->texture_descriptor_.height = this->h_;
        this->texture_descriptor_.depth = 1;
        this->texture_descriptor_.arrayLength = (this->type_ == GPU_TEXTURE_CUBE_ARRAY) ?
                                                    this->d_ / 6 :
                                                    1;
        this->texture_descriptor_.mipmapLevelCount = (this->mtl_max_mips_ > 0) ?
                                                         this->mtl_max_mips_ :
                                                         1;
        this->texture_descriptor_.usage =
            MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite |
            MTLTextureUsagePixelFormatView; /* TODO(Metal): Optimize usage flags. */
        this->texture_descriptor_.storageMode = MTLStorageModePrivate;
        this->texture_descriptor_.sampleCount = 1;
        this->texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        this->texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* GPU_TEXTURE_BUFFER */
      case GPU_TEXTURE_BUFFER: {
        this->texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        this->texture_descriptor_.pixelFormat = mtl_format;
        this->texture_descriptor_.textureType = MTLTextureTypeTextureBuffer;
        this->texture_descriptor_.width = this->w_;
        this->texture_descriptor_.height = 1;
        this->texture_descriptor_.depth = 1;
        this->texture_descriptor_.arrayLength = 1;
        this->texture_descriptor_.mipmapLevelCount = (this->mtl_max_mips_ > 0) ?
                                                         this->mtl_max_mips_ :
                                                         1;
        this->texture_descriptor_.usage =
            MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite |
            MTLTextureUsagePixelFormatView; /* TODO(Metal): Optimize usage flags. */
        this->texture_descriptor_.storageMode = MTLStorageModePrivate;
        this->texture_descriptor_.sampleCount = 1;
        this->texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        this->texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      default: {
        MTL_LOG_ERROR("[METAL] Error: Cannot create texture with unknown type: %d\n", this->type_);
        return;
      } break;
    }

    /* Determine Resource Mode. */
    this->resource_mode_ = MTL_TEXTURE_MODE_DEFAULT;

    /* Create texture. */
    this->texture_ = [ctx->device newTextureWithDescriptor:this->texture_descriptor_];

    [this->texture_descriptor_ release];
    this->texture_descriptor_ = nullptr;
    this->texture_.label = [NSString stringWithUTF8String:this->get_name()];
    BLI_assert(this->texture_);
    this->is_baked_ = true;
    this->is_dirty_ = false;
  }

  /* Re-apply previous contents. */
  if (copy_previous_contents) {
    id<MTLTexture> previous_texture;
    /* TODO(Metal): May need to copy previous contents of texture into new texture. */
    /*[previous_texture release]; */
    UNUSED_VARS(previous_texture);
  }
}

void gpu::MTLTexture::reset()
{

  MTL_LOG_INFO("Texture %s reset. Size %d, %d, %d\n", this->get_name(), w_, h_, d_);
  /* Delete associated METAL resources. */
  if (this->texture_ != nil) {
    [this->texture_ release];
    this->texture_ = nil;
    this->is_baked_ = false;
    this->is_dirty_ = true;
  }

  if (this->mip_swizzle_view_ != nil) {
    [this->mip_swizzle_view_ release];
    this->mip_swizzle_view_ = nil;
  }

  if (this->texture_buffer_ != nil) {
    [this->texture_buffer_ release];
  }

  /* Blit framebuffer. */
  if (this->blit_fb_) {
    GPU_framebuffer_free(this->blit_fb_);
    this->blit_fb_ = nullptr;
  }

  BLI_assert(this->texture_ == nil);
  BLI_assert(this->mip_swizzle_view_ == nil);
}

/** \} */

}  // namespace blender::gpu

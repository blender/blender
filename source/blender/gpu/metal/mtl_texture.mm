/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"
#include "BLI_math_half.hh"

#include "DNA_userdef_types.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_platform.hh"
#include "GPU_state.hh"

#include "mtl_backend.hh"
#include "mtl_common.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_texture.hh"
#include "mtl_vertex_buffer.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

void gpu::MTLTexture::mtl_texture_init()
{
  BLI_assert(MTLContext::get() != nullptr);

  /* Status. */
  is_baked_ = false;
  is_dirty_ = false;
  resource_mode_ = MTL_TEXTURE_MODE_DEFAULT;
  mtl_max_mips_ = 1;

  /* Metal properties. */
  texture_ = nil;
  mip_swizzle_view_ = nil;

  /* VBO. */
  vert_buffer_ = nullptr;
  vert_buffer_mtl_ = nil;

  /* Default Swizzle. */
  tex_swizzle_mask_[0] = 'r';
  tex_swizzle_mask_[1] = 'g';
  tex_swizzle_mask_[2] = 'b';
  tex_swizzle_mask_[3] = 'a';
  mtl_swizzle_mask_ = MTLTextureSwizzleChannelsMake(
      MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha);
}

gpu::MTLTexture::MTLTexture(const char *name) : Texture(name)
{
  /* Common Initialization. */
  mtl_texture_init();
}

gpu::MTLTexture::MTLTexture(const char *name,
                            TextureFormat format,
                            GPUTextureType type,
                            id<MTLTexture> metal_texture)
    : Texture(name)
{
  /* Common Initialization. */
  mtl_texture_init();

  /* Prep texture from METAL handle. */
  BLI_assert(metal_texture != nil);
  BLI_assert(type == GPU_TEXTURE_2D);
  type_ = type;
  init_2D((int)metal_texture.width, (int)metal_texture.height, 0, 1, format);

  /* Assign MTLTexture. */
  texture_ = metal_texture;
  [texture_ retain];
  internal_gpu_image_usage_flags_ = gpu_usage_from_mtl(metal_texture.usage);
  gpu_image_usage_flags_ = internal_gpu_image_usage_flags_;

  /* Flag as Baked. */
  is_baked_ = true;
  is_dirty_ = false;
  resource_mode_ = MTL_TEXTURE_MODE_EXTERNAL;
}

gpu::MTLTexture::~MTLTexture()
{
  /* Unbind if bound. */
  MTLContext *ctx = MTLContext::get();
  if (ctx != nullptr) {
    ctx->state_manager->texture_unbind(this);
  }

  /* Free memory. */
  this->reset();
}

/** \} */

/* -------------------------------------------------------------------- */
void gpu::MTLTexture::bake_mip_swizzle_view()
{
  if (texture_view_dirty_flags_) {

    /* Optimization: only generate texture view for mipmapped textures if base level > 0
     * and max level does not match the existing number of mips.
     * Only apply this if mipmap is the only change, and we have not previously generated
     * a texture view. For textures which are created as views, this should also be skipped. */
    if (resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW &&
        texture_view_dirty_flags_ == TEXTURE_VIEW_MIP_DIRTY && mip_swizzle_view_ == nil)
    {

      if (mip_texture_base_level_ == 0 && mip_texture_max_level_ == mtl_max_mips_) {
        texture_view_dirty_flags_ = TEXTURE_VIEW_NOT_DIRTY;
        return;
      }
    }

    /* if a texture view was previously created we release it. */
    if (mip_swizzle_view_ != nil) {
      [mip_swizzle_view_ release];
      mip_swizzle_view_ = nil;
    }

    /* Use source texture to determine range limits. If we are using a GPU texture view, the range
     * check should only validate the range */
    const gpu::Texture *tex_view_src = this;
    if (resource_mode_ == MTL_TEXTURE_MODE_TEXTURE_VIEW) {
      tex_view_src = source_texture_;
    }

    /* Determine num slices */
    int max_slices = 1;
    int num_slices = 1;
    switch (type_) {
      case GPU_TEXTURE_1D_ARRAY:
        max_slices = tex_view_src->height_get();
        num_slices = h_;
        break;
      case GPU_TEXTURE_2D_ARRAY:
        max_slices = tex_view_src->depth_get();
        num_slices = d_;
        break;
      case GPU_TEXTURE_CUBE:
        max_slices = 6;
        num_slices = 6;
        break;
      case GPU_TEXTURE_CUBE_ARRAY:
        /* d_ is equal to array levels * 6, including face count. */
        max_slices = tex_view_src->depth_get();
        num_slices = d_;
        break;
      default:
        num_slices = 1;
        break;
    }

    /* Determine texture view format. If texture view is used as a stencil view, we instead provide
     * the equivalent format for performing stencil reads/samples. */
    MTLPixelFormat texture_view_pixel_format = gpu_texture_format_to_metal(format_);
    if (texture_view_stencil_) {
      switch (texture_view_pixel_format) {
        case MTLPixelFormatDepth24Unorm_Stencil8:
          texture_view_pixel_format = MTLPixelFormatX24_Stencil8;
          break;
        case MTLPixelFormatDepth32Float_Stencil8:
          texture_view_pixel_format = MTLPixelFormatX32_Stencil8;
          break;
        default:
          BLI_assert_msg(false, "Texture format does not support stencil views.");
          break;
      }
    }

    /* NOTE: Texture type for cube maps can be overridden as a 2D array. This is done
     * via modifying this textures type flags. */
    MTLTextureType texture_view_texture_type = to_metal_type(type_);

    /* Ensure we have texture view usage flagged.
     * NOTE: This check exists in high level GPU API, however does not cover internal Metal backend
     * uses of texture views such as when required to support SRGB enablement toggle during
     * rendering. */
    BLI_assert_msg(
        (texture_view_pixel_format == texture_.pixelFormat) ||
            (internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_FORMAT_VIEW),
        "Usage Flag GPU_TEXTURE_USAGE_FORMAT_VIEW must be specified if a texture view is "
        "created with a different format to its source texture.");

    int range_len = min_ii((mip_texture_max_level_ - mip_texture_base_level_) + 1,
                           (int)texture_.mipmapLevelCount - mip_texture_base_level_);
    BLI_assert(range_len > 0);
    BLI_assert(mip_texture_base_level_ < texture_.mipmapLevelCount);
    BLI_assert(mip_texture_base_layer_ < max_slices);
    UNUSED_VARS_NDEBUG(max_slices);
    mip_swizzle_view_ = [texture_
        newTextureViewWithPixelFormat:texture_view_pixel_format
                          textureType:texture_view_texture_type
                               levels:NSMakeRange(mip_texture_base_level_, range_len)
                               slices:NSMakeRange(mip_texture_base_layer_, num_slices)
                              swizzle:mtl_swizzle_mask_];
    MTL_LOG_DEBUG(
        "Updating texture view - MIP TEXTURE BASE LEVEL: %d, MAX LEVEL: %d (Range len: %d)",
        mip_texture_base_level_,
        min_ii(mip_texture_max_level_, (int)texture_.mipmapLevelCount),
        range_len);
#ifndef NDEBUG
    mip_swizzle_view_.label = [NSString
        stringWithFormat:
            @"MipSwizzleView_%s__format=%u_type=%u_baselevel=%u_numlevels=%u_swizzle='%c%c%c%c'",
            [[texture_ label] UTF8String],
            (uint)texture_view_pixel_format,
            (uint)texture_view_texture_type,
            (uint)mip_texture_base_level_,
            (uint)range_len,
            tex_swizzle_mask_[0],
            tex_swizzle_mask_[1],
            tex_swizzle_mask_[2],
            tex_swizzle_mask_[3]];
#else
    mip_swizzle_view_.label = [texture_ label];
#endif
    texture_view_dirty_flags_ = TEXTURE_VIEW_NOT_DIRTY;
  }
}

/** \name Operations
 * \{ */

id<MTLTexture> gpu::MTLTexture::get_metal_handle()
{

  /* Verify VBO texture shares same buffer. */
  if (resource_mode_ == MTL_TEXTURE_MODE_VBO) {
    id<MTLBuffer> buf = vert_buffer_->get_metal_buffer();

    /* Source vertex buffer has been re-generated, require re-initialization. */
    if (buf != vert_buffer_mtl_) {
      MTL_LOG_DEBUG(
          "MTLTexture '%p' using MTL_TEXTURE_MODE_VBO requires re-generation due to updated "
          "Vertex-Buffer.",
          this);
      /* Clear state. */
      this->reset();

      /* Re-initialize. */
      this->init_internal(vert_buffer_);

      /* Update for assertion check below. */
      buf = vert_buffer_->get_metal_buffer();
    }

    /* Ensure buffer is valid.
     * Fetch-vert buffer handle directly in-case it changed above. */
    BLI_assert(vert_buffer_mtl_ != nil);
    BLI_assert(vert_buffer_->get_metal_buffer() == vert_buffer_mtl_);
  }

  /* ensure up to date and baked. */
  this->ensure_baked();

  if (is_baked_) {
    /* For explicit texture views, ensure we always return the texture view. */
    if (resource_mode_ == MTL_TEXTURE_MODE_TEXTURE_VIEW) {
      BLI_assert_msg(mip_swizzle_view_, "Texture view should always have a valid handle.");
    }

    if (mip_swizzle_view_ != nil || texture_view_dirty_flags_) {
      bake_mip_swizzle_view();

      /* Optimization: If texture view does not change mip parameters, no texture view will be
       * baked. This is because texture views remove the ability to perform lossless compression.
       */
      if (mip_swizzle_view_ != nil) {
        return mip_swizzle_view_;
      }
    }
    return texture_;
  }
  return nil;
}

id<MTLTexture> gpu::MTLTexture::get_metal_handle_base()
{

  /* ensure up to date and baked. */
  this->ensure_baked();

  /* For explicit texture views, always return the texture view. */
  if (resource_mode_ == MTL_TEXTURE_MODE_TEXTURE_VIEW) {
    BLI_assert_msg(mip_swizzle_view_, "Texture view should always have a valid handle.");
    if (mip_swizzle_view_ != nil || texture_view_dirty_flags_) {
      bake_mip_swizzle_view();
    }
    BLI_assert(mip_swizzle_view_ != nil);
    return mip_swizzle_view_;
  }

  /* Return base handle. */
  if (is_baked_) {
    return texture_;
  }
  return nil;
}

void gpu::MTLTexture::blit(id<MTLBlitCommandEncoder> blit_encoder,
                           uint src_x_offset,
                           uint src_y_offset,
                           uint src_z_offset,
                           uint src_slice,
                           uint src_mip,
                           gpu::MTLTexture *dst,
                           uint dst_x_offset,
                           uint dst_y_offset,
                           uint dst_z_offset,
                           uint dst_slice,
                           uint dst_mip,
                           uint width,
                           uint height,
                           uint depth)
{

  BLI_assert(dst);
  BLI_assert(width > 0 && height > 0 && depth > 0);
  MTLSize src_size = MTLSizeMake(width, height, depth);
  MTLOrigin src_origin = MTLOriginMake(src_x_offset, src_y_offset, src_z_offset);
  MTLOrigin dst_origin = MTLOriginMake(dst_x_offset, dst_y_offset, dst_z_offset);

  if (this->format_get() != dst->format_get()) {
    MTL_LOG_WARNING(
        "gpu::MTLTexture: Cannot copy between two textures of different types using a "
        "blit encoder. TODO: Support this operation");
    return;
  }

  /* TODO(Metal): Verify if we want to use the one with modified base-level/texture view
   * or not. */
  [blit_encoder copyFromTexture:this->get_metal_handle_base()
                    sourceSlice:src_slice
                    sourceLevel:src_mip
                   sourceOrigin:src_origin
                     sourceSize:src_size
                      toTexture:dst->get_metal_handle_base()
               destinationSlice:dst_slice
               destinationLevel:dst_mip
              destinationOrigin:dst_origin];
}

void gpu::MTLTexture::blit(gpu::MTLTexture *dst,
                           uint src_x_offset,
                           uint src_y_offset,
                           uint dst_x_offset,
                           uint dst_y_offset,
                           uint src_mip,
                           uint dst_mip,
                           uint dst_slice,
                           int width,
                           int height)
{
  BLI_assert(this->type_get() == dst->type_get());

  gpu::Shader *shader = fullscreen_blit_sh_get();
  BLI_assert(shader != nullptr);
  BLI_assert(MTLContext::get());

  /* Fetch restore framebuffer and blit target framebuffer from destination texture. */
  gpu::FrameBuffer *restore_fb = GPU_framebuffer_active_get();
  gpu::FrameBuffer *blit_target_fb = dst->get_blit_framebuffer(dst_slice, dst_mip);
  BLI_assert(blit_target_fb);
  GPU_framebuffer_bind(blit_target_fb);

  /* Execute graphics draw call to perform the blit. */
  Batch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, shader);

  float w = dst->width_get();
  float h = dst->height_get();

  GPU_shader_uniform_2f(shader, "fullscreen", w, h);
  GPU_shader_uniform_2f(shader, "src_offset", src_x_offset, src_y_offset);
  GPU_shader_uniform_2f(shader, "dst_offset", dst_x_offset, dst_y_offset);
  GPU_shader_uniform_2f(shader, "size", width, height);

  GPU_shader_uniform_1i(shader, "mip", src_mip);
  GPU_batch_texture_bind(quad, "imageTexture", this);

  /* Caching previous pipeline state. */
  bool depth_write_prev = GPU_depth_mask_get();
  uint stencil_mask_prev = GPU_stencil_mask_get();
  GPUStencilTest stencil_test_prev = GPU_stencil_test_get();
  GPUFaceCullTest culling_test_prev = GPU_face_culling_get();
  GPUBlend blend_prev = GPU_blend_get();
  GPUDepthTest depth_test_prev = GPU_depth_test_get();
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

gpu::FrameBuffer *gpu::MTLTexture::get_blit_framebuffer(int dst_slice, uint dst_mip)
{

  /* Check if layer has changed. */
  bool update_attachments = false;
  if (!blit_fb_) {
    std::string fb_name = StringRefNull(this->name_) + "_blit_fb";
    blit_fb_ = GPU_framebuffer_create(fb_name.c_str());
    update_attachments = true;
  }

  /* Check if current blit FB has the correct attachment properties. */
  if (blit_fb_) {
    if (blit_fb_slice_ != dst_slice || blit_fb_mip_ != dst_mip) {
      update_attachments = true;
    }
  }

  if (update_attachments) {
    if (format_flag_ & GPU_FORMAT_DEPTH || format_flag_ & GPU_FORMAT_STENCIL) {
      /* DEPTH TEX */
      GPU_framebuffer_ensure_config(
          &blit_fb_,
          {GPU_ATTACHMENT_TEXTURE_LAYER_MIP(this, int(dst_slice), int(dst_mip)),
           GPU_ATTACHMENT_NONE});
    }
    else {
      /* COLOR TEX */
      GPU_framebuffer_ensure_config(
          &blit_fb_,
          {GPU_ATTACHMENT_NONE,
           GPU_ATTACHMENT_TEXTURE_LAYER_MIP(this, int(dst_slice), int(dst_mip))});
    }
    blit_fb_slice_ = dst_slice;
    blit_fb_mip_ = dst_mip;
  }

  BLI_assert(blit_fb_);
  return blit_fb_;
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
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  /* Do not update texture view. */
  BLI_assert(resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW);

  /* Ensure mipmaps. */
  this->ensure_mipmaps(mip);

  /* Ensure texture is baked. */
  this->ensure_baked();

  /* Safety checks. */
  BLI_assert(mip >= mip_min_ && mip <= mip_max_);
  BLI_assert(mip < texture_.mipmapLevelCount);
  BLI_assert(texture_.mipmapLevelCount >= mip_max_);

  std::unique_ptr<uint16_t, MEM_freeN_smart_ptr_deleter> clamped_half_buffer = nullptr;

  if (data != nullptr && type == GPU_DATA_FLOAT && is_half_float(format_)) {
    size_t pixel_count = max_ii(extent[0], 1) * max_ii(extent[1], 1) * max_ii(extent[2], 1);
    size_t total_component_count = to_component_len(format_) * pixel_count;

    clamped_half_buffer.reset(
        (uint16_t *)MEM_mallocN_aligned(sizeof(uint16_t) * total_component_count, 128, __func__));

    Span<float> src(static_cast<const float *>(data), total_component_count);
    MutableSpan<uint16_t> dst(static_cast<uint16_t *>(clamped_half_buffer.get()),
                              total_component_count);

    constexpr int64_t chunk_size = 4 * 1024 * 1024;

    threading::parallel_for(
        IndexRange(total_component_count), chunk_size, [&](const IndexRange range) {
          /* Doing float to half conversion manually to avoid implementation specific behavior
           * regarding Inf and NaNs. Use make finite version to avoid unexpected black pixels on
           * certain implementation. For platform parity we clamp these infinite values to finite
           * values. */
          blender::math::float_to_half_make_finite_array(
              src.slice(range).data(), dst.slice(range).data(), range.size());
        });
    data = clamped_half_buffer.get();
    type = GPU_DATA_HALF_FLOAT;
  }

  /* DEPTH FLAG - Depth formats cannot use direct BLIT - pass off to their own routine which will
   * do a depth-only render. */
  bool is_depth_format = (format_flag_ & GPU_FORMAT_DEPTH);
  if (is_depth_format) {
    switch (type_) {

      case GPU_TEXTURE_2D:
        update_sub_depth_2d(mip, offset, extent, type, data);
        return;
      default:
        MTL_LOG_ERROR(
            "gpu::MTLTexture::update_sub not yet supported for other depth "
            "configurations");
        return;
    }
  }

  const bool is_compressed = (format_flag_ & GPU_FORMAT_COMPRESSED);

  @autoreleasepool {
    /* Determine totalsize of INPUT Data. */
    int num_channels = to_component_len(format_);
    size_t input_bytes_per_pixel = to_bytesize(format_, type);
    size_t totalsize = 0;

    /* If unpack row length is used, size of input data uses the unpack row length, rather than the
     * image length. */
    size_t expected_update_w = ((ctx->pipeline_state.unpack_row_length == 0) ?
                                    extent[0] :
                                    ctx->pipeline_state.unpack_row_length);

    /* Ensure calculated total size isn't larger than remaining image data size. */
    if (is_compressed) {
      /* Calculate size requirement for incoming compressed texture data. */
      totalsize = ((expected_update_w + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(format_);
    }
    else {
      switch (this->dimensions_count()) {
        case 1:
          totalsize = input_bytes_per_pixel * max_ulul(expected_update_w, 1);
          break;
        case 2:
          totalsize = input_bytes_per_pixel * max_ulul(expected_update_w, 1) * (size_t)extent[1];
          break;
        case 3:
          totalsize = input_bytes_per_pixel * max_ulul(expected_update_w, 1) * (size_t)extent[1] *
                      (size_t)extent[2];
          break;
        default:
          BLI_assert(false);
          break;
      }
    }

    /* Early exit if update size is zero. update_sub sometimes has a zero-sized
     * extent when called from texture painting. */
    if (totalsize <= 0 || extent[0] <= 0) {
      MTL_LOG_WARNING(
          "MTLTexture::update_sub called with extent size of zero for one or more dimensions. "
          "(%d, %d, %d) - DimCount: %u",
          extent[0],
          extent[1],
          extent[2],
          this->dimensions_count());
      return;
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
    MTLPixelFormat destination_format = gpu_texture_format_to_metal(format_);
    size_t expected_dst_bytes_per_pixel = get_mtl_format_bytesize(destination_format);
    int destination_num_channels = get_mtl_format_num_components(destination_format);

    /* Prepare specialization struct (For texture update routine). */
    TextureUpdateRoutineSpecialisation compute_specialization_kernel = {
        tex_data_format_to_msl_type_str(type),              /* INPUT DATA FORMAT */
        tex_data_format_to_msl_texture_template_type(type), /* TEXTURE DATA FORMAT */
        num_channels,
        destination_num_channels,
        false /* Not a clear. */
    };

    /* Determine whether we can do direct BLIT or not. For compressed textures,
     * always assume a direct blit (input data pretends to be float, but it is
     * not). */
    bool can_use_direct_blit = true;
    if (!is_compressed && (expected_dst_bytes_per_pixel != input_bytes_per_pixel ||
                           num_channels != destination_num_channels))
    {
      can_use_direct_blit = false;
    }

    if (is_depth_format) {
      if (type_ == GPU_TEXTURE_2D || type_ == GPU_TEXTURE_2D_ARRAY) {
        /* Workaround for crash in validation layer when blitting to sampler2DDepth target with
         * dimensions (1, 1, 1); */
        if (extent[0] == 1 && extent[1] == 1 && extent[2] == 1 && totalsize == 4) {
          can_use_direct_blit = false;
        }
      }
    }

    if (format_ == TextureFormat::SRGBA_8_8_8_8 && !can_use_direct_blit) {
      MTL_LOG_WARNING(
          "SRGB data upload does not work correctly using compute upload. "
          "texname '%s'",
          name_);
    }

    /* Safety Checks. */
    if (type == GPU_DATA_UINT_24_8_DEPRECATED || type == GPU_DATA_10_11_11_REV ||
        type == GPU_DATA_2_10_10_10_REV || is_compressed)
    {
      BLI_assert(can_use_direct_blit &&
                 "Special input data type must be a 1-1 mapping with destination texture as it "
                 "cannot easily be split");
    }

    /* Debug and verification. */
    if (!can_use_direct_blit) {
      /* Check mip compatibility. */
      if (mip != 0) {
        MTL_LOG_ERROR(
            "Updating texture layers other than mip=0 when data is mismatched is not "
            "possible in METAL on macOS using texture->write\n");
        return;
      }

      /* Check Format write-ability. */
      if (mtl_format_get_writeable_view_format(destination_format) == MTLPixelFormatInvalid) {
        MTL_LOG_ERROR(
            "Updating texture -- destination MTLPixelFormat '%d' does not support write "
            "operations, and no suitable TextureView format exists.\n",
            *(int *)(&destination_format));
        return;
      }
    }

    /* Common Properties. */
    MTLPixelFormat compatible_write_format = mtl_format_get_writeable_view_format(
        destination_format);

    /* Some texture formats are not writeable so we need to use a texture view. */
    if (compatible_write_format == MTLPixelFormatInvalid) {
      MTL_LOG_ERROR("Cannot use compute update blit with texture-view format: %d\n",
                    *((int *)&compatible_write_format));
      return;
    }

    /* Fetch allocation from memory pool. */
    MTLBuffer *temp_allocation = MTLContext::get_global_memory_manager()->allocate_with_data(
        totalsize, true, data);
    id<MTLBuffer> staging_buffer = temp_allocation->get_metal_buffer();
    BLI_assert(staging_buffer != nil);

    /* Prepare command encoders. */
    id<MTLBlitCommandEncoder> blit_encoder = nil;
    id<MTLComputeCommandEncoder> compute_encoder = nil;
    id<MTLTexture> staging_texture = nil;
    id<MTLTexture> texture_handle = nil;

    /* Use staging texture. */
    bool use_staging_texture = false;

    if (can_use_direct_blit) {
      blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
      BLI_assert(blit_encoder != nil);

      /* If we need to use a texture view to write texture data as the source
       * format is unwritable, if our texture has not been initialized with
       * texture view support, use a staging texture. */
      if ((compatible_write_format != destination_format) &&
          !(internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_FORMAT_VIEW))
      {
        use_staging_texture = true;
      }
    }
    else {
      compute_encoder = ctx->main_command_buffer.ensure_begin_compute_encoder();
      BLI_assert(compute_encoder != nil);

      /* For compute, we should use a stating texture to avoid texture write usage,
       * if it has not been specified for the texture. Using shader-write disables
       * lossless texture compression, so this is best to avoid where possible. */
      if (!(internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_SHADER_WRITE)) {
        use_staging_texture = true;
      }
      if (compatible_write_format != destination_format) {
        if (!(internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_FORMAT_VIEW)) {
          use_staging_texture = true;
        }
      }
    }

    /* Allocate stating texture if needed. */
    if (use_staging_texture) {
      /* Create staging texture to avoid shader-write limiting optimization. */
      BLI_assert(texture_descriptor_ != nullptr);
      MTLTextureUsage original_usage = texture_descriptor_.usage;
      texture_descriptor_.usage = original_usage | MTLTextureUsageShaderWrite |
                                  MTLTextureUsagePixelFormatView;
      staging_texture = [ctx->device newTextureWithDescriptor:texture_descriptor_];
      staging_texture.label = @"Staging texture";
      texture_descriptor_.usage = original_usage;

      /* Create texture view if needed. */
      texture_handle = ((compatible_write_format == destination_format)) ?
                           [staging_texture retain] :
                           [staging_texture newTextureViewWithPixelFormat:compatible_write_format];
    }
    else {
      /* Use texture view. */
      if (compatible_write_format != destination_format) {
        BLI_assert(internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_FORMAT_VIEW);
        texture_handle = [texture_ newTextureViewWithPixelFormat:compatible_write_format];
      }
      else {
        texture_handle = texture_;
        [texture_handle retain];
      }
    }

    switch (type_) {

      /* 1D */
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_1D_ARRAY: {
        if (can_use_direct_blit) {
          /* Use Blit based update. */
          size_t bytes_per_row = expected_dst_bytes_per_pixel *
                                 ((ctx->pipeline_state.unpack_row_length == 0) ?
                                      extent[0] :
                                      ctx->pipeline_state.unpack_row_length);
          size_t bytes_per_image = bytes_per_row;
          if (is_compressed) {
            size_t block_size = to_block_size(format_);
            size_t blocks_x = divide_ceil_u(extent[0], 4);
            bytes_per_row = blocks_x * block_size;
            bytes_per_image = bytes_per_row;
          }
          int max_array_index = ((type_ == GPU_TEXTURE_1D_ARRAY) ? extent[1] : 1);
          for (int array_index = 0; array_index < max_array_index; array_index++) {

            size_t buffer_array_offset = (bytes_per_image * (size_t)array_index);
            [blit_encoder
                     copyFromBuffer:staging_buffer
                       sourceOffset:buffer_array_offset
                  sourceBytesPerRow:bytes_per_row
                sourceBytesPerImage:bytes_per_image
                         sourceSize:MTLSizeMake(extent[0], 1, 1)
                          toTexture:texture_handle
                   destinationSlice:((type_ == GPU_TEXTURE_1D_ARRAY) ? (array_index + offset[1]) :
                                                                       0)
                   destinationLevel:mip
                  destinationOrigin:MTLOriginMake(offset[0], 0, 0)];
          }
        }
        else {
          /* Use Compute Based update. */
          if (type_ == GPU_TEXTURE_1D) {
            id<MTLComputePipelineState> pso = texture_update_1d_get_kernel(
                compute_specialization_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], 1, 1},
                                          {offset[0], 0, 0},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};

            /* Bind resources via compute state for optimal state caching performance. */
            MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
            cs.bind_pso(pso);
            cs.bind_compute_bytes(&params, sizeof(params), 0);
            cs.bind_compute_buffer(staging_buffer, 0, 1);
            cs.bind_compute_texture(texture_handle, 0);
            [compute_encoder
                      dispatchThreads:MTLSizeMake(extent[0], 1, 1) /* Width, Height, Layer */
                threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
          }
          else if (type_ == GPU_TEXTURE_1D_ARRAY) {
            id<MTLComputePipelineState> pso = texture_update_1d_array_get_kernel(
                compute_specialization_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], extent[1], 1},
                                          {offset[0], offset[1], 0},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};

            /* Bind resources via compute state for optimal state caching performance. */
            MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
            cs.bind_pso(pso);
            cs.bind_compute_bytes(&params, sizeof(params), 0);
            cs.bind_compute_buffer(staging_buffer, 0, 1);
            cs.bind_compute_texture(texture_handle, 0);
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
          size_t bytes_per_row = expected_dst_bytes_per_pixel *
                                 ((ctx->pipeline_state.unpack_row_length == 0) ?
                                      extent[0] :
                                      ctx->pipeline_state.unpack_row_length);
          size_t bytes_per_image = bytes_per_row * extent[1];
          if (is_compressed) {
            size_t block_size = to_block_size(format_);
            size_t blocks_x = divide_ceil_u(extent[0], 4);
            size_t blocks_y = divide_ceil_u(extent[1], 4);
            bytes_per_row = blocks_x * block_size;
            bytes_per_image = bytes_per_row * blocks_y;
          }

          size_t texture_array_relative_offset = 0;
          int base_slice = (type_ == GPU_TEXTURE_2D_ARRAY) ? offset[2] : 0;
          int final_slice = base_slice + ((type_ == GPU_TEXTURE_2D_ARRAY) ? extent[2] : 1);

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {

            if (array_slice > 0) {
              BLI_assert(type_ == GPU_TEXTURE_2D_ARRAY);
              BLI_assert(array_slice < d_);
            }

            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:texture_array_relative_offset
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
          if (type_ == GPU_TEXTURE_2D) {
            id<MTLComputePipelineState> pso = texture_update_2d_get_kernel(
                compute_specialization_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], extent[1], 1},
                                          {offset[0], offset[1], 0},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};

            /* Bind resources via compute state for optimal state caching performance. */
            MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
            cs.bind_pso(pso);
            cs.bind_compute_bytes(&params, sizeof(params), 0);
            cs.bind_compute_buffer(staging_buffer, 0, 1);
            cs.bind_compute_texture(texture_handle, 0);
            [compute_encoder
                      dispatchThreads:MTLSizeMake(
                                          extent[0], extent[1], 1) /* Width, Height, Layer */
                threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          }
          else if (type_ == GPU_TEXTURE_2D_ARRAY) {
            id<MTLComputePipelineState> pso = texture_update_2d_array_get_kernel(
                compute_specialization_kernel);
            TextureUpdateParams params = {mip,
                                          {extent[0], extent[1], extent[2]},
                                          {offset[0], offset[1], offset[2]},
                                          ((ctx->pipeline_state.unpack_row_length == 0) ?
                                               extent[0] :
                                               ctx->pipeline_state.unpack_row_length)};

            /* Bind resources via compute state for optimal state caching performance. */
            MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
            cs.bind_pso(pso);
            cs.bind_compute_bytes(&params, sizeof(params), 0);
            cs.bind_compute_buffer(staging_buffer, 0, 1);
            cs.bind_compute_texture(texture_handle, 0);
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
          size_t bytes_per_row = expected_dst_bytes_per_pixel *
                                 ((ctx->pipeline_state.unpack_row_length == 0) ?
                                      extent[0] :
                                      ctx->pipeline_state.unpack_row_length);
          size_t bytes_per_image = bytes_per_row * extent[1];
          [blit_encoder copyFromBuffer:staging_buffer
                          sourceOffset:0
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
              compute_specialization_kernel);
          TextureUpdateParams params = {mip,
                                        {extent[0], extent[1], extent[2]},
                                        {offset[0], offset[1], offset[2]},
                                        ((ctx->pipeline_state.unpack_row_length == 0) ?
                                             extent[0] :
                                             ctx->pipeline_state.unpack_row_length)};

          /* Bind resources via compute state for optimal state caching performance. */
          MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
          cs.bind_pso(pso);
          cs.bind_compute_bytes(&params, sizeof(params), 0);
          cs.bind_compute_buffer(staging_buffer, 0, 1);
          cs.bind_compute_texture(texture_handle, 0);
          [compute_encoder
                    dispatchThreads:MTLSizeMake(
                                        extent[0], extent[1], extent[2]) /* Width, Height, Depth */
              threadsPerThreadgroup:MTLSizeMake(4, 4, 4)];
        }
      } break;

      /* CUBE */
      case GPU_TEXTURE_CUBE: {
        if (can_use_direct_blit) {
          size_t bytes_per_row = expected_dst_bytes_per_pixel *
                                 ((ctx->pipeline_state.unpack_row_length == 0) ?
                                      extent[0] :
                                      ctx->pipeline_state.unpack_row_length);
          size_t bytes_per_image = bytes_per_row * extent[1];
          size_t texture_array_relative_offset = 0;

          /* Iterate over all cube faces in range (offset[2], offset[2] + extent[2]). */
          for (int i = 0; i < extent[2]; i++) {
            int face_index = offset[2] + i;

            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:texture_array_relative_offset
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

          size_t bytes_per_row = expected_dst_bytes_per_pixel *
                                 ((ctx->pipeline_state.unpack_row_length == 0) ?
                                      extent[0] :
                                      ctx->pipeline_state.unpack_row_length);
          size_t bytes_per_image = bytes_per_row * extent[1];

          /* Upload to all faces between offset[2] (which is zero in most cases) AND extent[2]. */
          size_t texture_array_relative_offset = 0;
          for (int i = 0; i < extent[2]; i++) {
            int face_index = offset[2] + i;
            [blit_encoder copyFromBuffer:staging_buffer
                            sourceOffset:texture_array_relative_offset
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
         * Data uploads generally happen via VertBuf instead. */
        BLI_assert(false);
      } break;

      case GPU_TEXTURE_ARRAY:
        /* Not an actual format - modifier flag for others. */
        return;
    }

    /* If staging texture was used, copy contents to original texture. */
    if (use_staging_texture) {
      /* When using staging texture, copy results into existing texture. */
      BLI_assert(staging_texture != nil);
      blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();

      /* Copy modified staging texture region back to original texture.
       * Differing blit dimensions based on type. */
      switch (type_) {
        case GPU_TEXTURE_1D:
        case GPU_TEXTURE_1D_ARRAY: {
          int base_slice = (type_ == GPU_TEXTURE_1D_ARRAY) ? offset[1] : 0;
          int final_slice = base_slice + ((type_ == GPU_TEXTURE_1D_ARRAY) ? extent[1] : 1);
          for (int array_index = base_slice; array_index < final_slice; array_index++) {
            [blit_encoder copyFromTexture:staging_texture
                              sourceSlice:array_index
                              sourceLevel:mip
                             sourceOrigin:MTLOriginMake(offset[0], 0, 0)
                               sourceSize:MTLSizeMake(extent[0], 1, 1)
                                toTexture:texture_
                         destinationSlice:array_index
                         destinationLevel:mip
                        destinationOrigin:MTLOriginMake(offset[0], 0, 0)];
          }
        } break;
        case GPU_TEXTURE_2D:
        case GPU_TEXTURE_2D_ARRAY: {
          int base_slice = (type_ == GPU_TEXTURE_2D_ARRAY) ? offset[2] : 0;
          int final_slice = base_slice + ((type_ == GPU_TEXTURE_2D_ARRAY) ? extent[2] : 1);
          for (int array_index = base_slice; array_index < final_slice; array_index++) {
            [blit_encoder copyFromTexture:staging_texture
                              sourceSlice:array_index
                              sourceLevel:mip
                             sourceOrigin:MTLOriginMake(offset[0], offset[1], 0)
                               sourceSize:MTLSizeMake(extent[0], extent[1], 1)
                                toTexture:texture_
                         destinationSlice:array_index
                         destinationLevel:mip
                        destinationOrigin:MTLOriginMake(offset[0], offset[1], 0)];
          }
        } break;
        case GPU_TEXTURE_3D: {
          [blit_encoder copyFromTexture:staging_texture
                            sourceSlice:0
                            sourceLevel:mip
                           sourceOrigin:MTLOriginMake(offset[0], offset[1], offset[2])
                             sourceSize:MTLSizeMake(extent[0], extent[1], extent[2])
                              toTexture:texture_
                       destinationSlice:0
                       destinationLevel:mip
                      destinationOrigin:MTLOriginMake(offset[0], offset[1], offset[2])];
        } break;
        case GPU_TEXTURE_CUBE:
        case GPU_TEXTURE_CUBE_ARRAY: {
          /* Iterate over all cube faces in range (offset[2], offset[2] + extent[2]). */
          for (int i = 0; i < extent[2]; i++) {
            int face_index = offset[2] + i;
            [blit_encoder copyFromTexture:staging_texture
                              sourceSlice:face_index
                              sourceLevel:mip
                             sourceOrigin:MTLOriginMake(offset[0], offset[1], 0)
                               sourceSize:MTLSizeMake(extent[0], extent[1], 1)
                                toTexture:texture_
                         destinationSlice:face_index
                         destinationLevel:mip
                        destinationOrigin:MTLOriginMake(offset[0], offset[1], 0)];
          }
        } break;
        case GPU_TEXTURE_ARRAY:
        case GPU_TEXTURE_BUFFER:
          BLI_assert_unreachable();
          break;
      }

      [staging_texture release];
    }

    /* Finalize Blit Encoder. */
    if (can_use_direct_blit) {
      /* Textures which use MTLStorageModeManaged need to have updated contents
       * synced back to CPU to avoid an automatic flush overwriting contents. */
      if (texture_.storageMode == MTLStorageModeManaged) {
        [blit_encoder synchronizeResource:texture_];
      }
      [blit_encoder optimizeContentsForGPUAccess:texture_];
    }
    else {
      /* Textures which use MTLStorageModeManaged need to have updated contents
       * synced back to CPU to avoid an automatic flush overwriting contents. */
      blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
      if (texture_.storageMode == MTLStorageModeManaged) {

        [blit_encoder synchronizeResource:texture_];
      }
      [blit_encoder optimizeContentsForGPUAccess:texture_];
    }

    /* Decrement texture reference counts. This ensures temporary texture views are released. */
    [texture_handle release];

    ctx->main_command_buffer.submit(false);

    /* Release temporary staging buffer allocation.
     * NOTE: Allocation will be tracked with command submission and released once no longer in use.
     */
    temp_allocation->free();
  }
}

void MTLTexture::update_sub(int offset[3],
                            int extent[3],
                            eGPUDataFormat format,
                            GPUPixelBuffer *pixbuf)
{
  /* Update texture from pixel buffer. */
  BLI_assert(validate_data_format(format_, format));
  BLI_assert(pixbuf != nullptr);

  /* Fetch pixel buffer metal buffer. */
  MTLPixelBuffer *mtl_pix_buf = static_cast<MTLPixelBuffer *>(unwrap(pixbuf));
  id<MTLBuffer> buffer = mtl_pix_buf->get_metal_buffer();
  BLI_assert(buffer != nil);
  if (buffer == nil) {
    return;
  }

  /* Ensure texture is ready. */
  this->ensure_baked();
  BLI_assert(texture_ != nil);

  /* Calculate dimensions. */
  int num_image_channels = to_component_len(format_);

  size_t bits_per_pixel = num_image_channels * to_bytesize(format);
  size_t bytes_per_row = bits_per_pixel * extent[0];
  size_t bytes_per_image = bytes_per_row * extent[1];

  /* Currently only required for 2D textures. */
  if (type_ == GPU_TEXTURE_2D) {

    /* Create blit command encoder to copy data. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
    [blit_encoder copyFromBuffer:buffer
                    sourceOffset:0
               sourceBytesPerRow:bytes_per_row
             sourceBytesPerImage:bytes_per_image
                      sourceSize:MTLSizeMake(extent[0], extent[1], 1)
                       toTexture:texture_
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(offset[0], offset[1], 0)];

    if (texture_.storageMode == MTLStorageModeManaged) {
      [blit_encoder synchronizeResource:texture_];
    }
    [blit_encoder optimizeContentsForGPUAccess:texture_];
  }
  else {
    BLI_assert(false);
  }
}

void gpu::MTLTexture::ensure_mipmaps(int miplvl)
{

  /* Do not update texture view. */
  BLI_assert(resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW);

  /* Clamp level to maximum. */
  int effective_h = (type_ == GPU_TEXTURE_1D_ARRAY) ? 0 : h_;
  int effective_d = (type_ != GPU_TEXTURE_3D) ? 0 : d_;
  int max_dimension = max_iii(w_, effective_h, effective_d);
  int max_miplvl = floor(log2(max_dimension));
  miplvl = min_ii(max_miplvl, miplvl);

  /* Increase mipmap level. */
  if (mipmaps_ < miplvl) {
    mipmaps_ = miplvl;

    /* Check if baked. */
    if (is_baked_ && mipmaps_ > mtl_max_mips_) {
      BLI_assert_msg(false,
                     "Texture requires a higher mipmap level count. Please specify the required "
                     "amount upfront.");
      is_dirty_ = true;
      MTL_LOG_WARNING("Texture requires regenerating due to increase in mip-count");
    }
  }
  this->mip_range_set(0, mipmaps_);
}

void gpu::MTLTexture::generate_mipmap()
{
  /* Compressed textures allow users to provide their own custom mipmaps. And
   * we can't generate them at runtime anyway. */
  if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    return;
  }

  /* Fetch Active Context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  if (!ctx->device) {
    MTL_LOG_ERROR("Cannot Generate mip-maps -- metal device invalid\n");
    BLI_assert(false);
    return;
  }

  /* Ensure mipmaps. */
  this->ensure_mipmaps(mtl_max_mips_);

  /* Ensure texture is baked. */
  this->ensure_baked();
  BLI_assert_msg(is_baked_ && texture_, "MTLTexture is not valid");

  if (mipmaps_ == 1 || mtl_max_mips_ == 1) {
    /* Nothing to do. */
    return;
  }

  /* Verify if we can perform mipmap generation. */
  if (format_ == TextureFormat::SFLOAT_32_DEPTH || format_ == TextureFormat::UNORM_16_DEPTH ||
      format_ == TextureFormat::SFLOAT_32_DEPTH_UINT_8)
  {
    MTL_LOG_WARNING("Cannot generate mipmaps for textures using DEPTH formats");
    return;
  }

  @autoreleasepool {
    /* Fetch active BlitCommandEncoder. */
    id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
    if (G.debug & G_DEBUG_GPU) {
      [enc insertDebugSignpost:@"Generate MipMaps"];
    }
    [enc generateMipmapsForTexture:texture_];
    has_generated_mips_ = true;
  }
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
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  /* Ensure texture is baked. */
  this->ensure_baked();

  @autoreleasepool {
    /* Setup blit encoder. */
    id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
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
        BLI_assert(mt_dst->d_ == d_);
        [blit_encoder copyFromTexture:this->get_metal_handle_base()
                            toTexture:mt_dst->get_metal_handle_base()];
        [blit_encoder optimizeContentsForGPUAccess:mt_dst->get_metal_handle_base()];
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
  }
}

void gpu::MTLTexture::clear(eGPUDataFormat data_format, const void *data)
{
  /* Ensure texture is baked. */
  this->ensure_baked();

  /* If render-pass clear is not supported, use compute-based clear. */
  bool do_render_pass_clear = true;
  if (ELEM(type_, GPU_TEXTURE_1D, GPU_TEXTURE_1D_ARRAY)) {
    do_render_pass_clear = false;
  }
  /* If texture is buffer-backed, clear directly on buffer.
   * NOTE: This us currently only true for fallback atomic textures. */
  if (backing_buffer_ != nullptr) {
    uint channel_len = to_component_len(format_);
    uint channel_size = to_bytesize(data_format);
    bool fast_buf_clear = true;
    const uchar *val = reinterpret_cast<const uchar *>(data);
    for (int i = 1; i < channel_size * channel_len; i++) {
      fast_buf_clear = fast_buf_clear && (val[i] == val[0]);
    }
    if (fast_buf_clear) {
      /* Fetch active context. */
      MTLContext *ctx = MTLContext::get();
      BLI_assert(ctx);

      /* Begin compute encoder. */
      id<MTLBlitCommandEncoder> blit_encoder =
          ctx->main_command_buffer.ensure_begin_blit_encoder();
      [blit_encoder fillBuffer:backing_buffer_->get_metal_buffer()
                         range:NSMakeRange(0, backing_buffer_->get_size())
                         value:val[0]];
    }
    else {
      BLI_assert_msg(false,
                     "Non-repeating-byte-pattern clear for buffer-backed textures not supported!");
    }
    return;
  }

  if (do_render_pass_clear) {
    /* Create clear frame-buffer for fast clear. */
    gpu::FrameBuffer *prev_fb = GPU_framebuffer_active_get();
    FrameBuffer *fb = this->get_blit_framebuffer(-1, 0);
    fb->bind(true);
    fb->clear_attachment(this->attachment_type(0), data_format, data);
    GPU_framebuffer_bind(prev_fb);
  }
  else {
    /** Perform compute-based clear. */
    /* Prepare specialization struct (For texture clear routine). */
    int num_channels = to_component_len(format_);
    TextureUpdateRoutineSpecialisation compute_specialization_kernel = {
        tex_data_format_to_msl_type_str(data_format),              /* INPUT DATA FORMAT */
        tex_data_format_to_msl_texture_template_type(data_format), /* TEXTURE DATA FORMAT */
        num_channels,
        num_channels,
        true /* Operation is a clear. */
    };

    /* Determine size of source data clear. */
    uint clear_data_size = to_bytesize(format_, data_format);

    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /* Determine writeable texture handle. */
    id<MTLTexture> texture_handle = texture_;

    /* Begin compute encoder. */
    id<MTLComputeCommandEncoder> compute_encoder =
        ctx->main_command_buffer.ensure_begin_compute_encoder();

    /* Perform clear operation based on texture type. */
    switch (type_) {
      case GPU_TEXTURE_1D: {
        id<MTLComputePipelineState> pso = texture_update_1d_get_kernel(
            compute_specialization_kernel);
        TextureUpdateParams params = {0,
                                      {w_, 1, 1},
                                      {0, 0, 0},
                                      ((ctx->pipeline_state.unpack_row_length == 0) ?
                                           w_ :
                                           ctx->pipeline_state.unpack_row_length)};

        /* Bind resources via compute state for optimal state caching performance. */
        MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
        cs.bind_pso(pso);
        cs.bind_compute_bytes(&params, sizeof(params), 0);
        cs.bind_compute_bytes(data, clear_data_size, 1);
        cs.bind_compute_texture(texture_handle, 0);
        [compute_encoder dispatchThreads:MTLSizeMake(w_, 1, 1) /* Width, Height, Layer */
                   threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
      } break;
      case GPU_TEXTURE_1D_ARRAY: {
        id<MTLComputePipelineState> pso = texture_update_1d_array_get_kernel(
            compute_specialization_kernel);
        TextureUpdateParams params = {0,
                                      {w_, h_, 1},
                                      {0, 0, 0},
                                      ((ctx->pipeline_state.unpack_row_length == 0) ?
                                           w_ :
                                           ctx->pipeline_state.unpack_row_length)};

        /* Bind resources via compute state for optimal state caching performance. */
        MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
        cs.bind_pso(pso);
        cs.bind_compute_bytes(&params, sizeof(params), 0);
        cs.bind_compute_bytes(data, clear_data_size, 1);
        cs.bind_compute_texture(texture_handle, 0);
        [compute_encoder dispatchThreads:MTLSizeMake(w_, h_, 1) /* Width, layers, nil */
                   threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
      } break;
      default: {
        MTL_LOG_ERROR(
            "gpu::MTLTexture::clear requires compute pass for texture"
            "type: %d, but this is not yet supported",
            (int)type_);
      } break;
    }

    /* Textures which use MTLStorageModeManaged need to have updated contents
     * synced back to CPU to avoid an automatic flush overwriting contents. */
    id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
    if (texture_.storageMode == MTLStorageModeManaged) {
      [blit_encoder synchronizeResource:texture_];
    }
    [blit_encoder optimizeContentsForGPUAccess:texture_];
  }
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
  if (memcmp(tex_swizzle_mask_, swizzle_mask, 4) != 0) {
    memcpy(tex_swizzle_mask_, swizzle_mask, 4);

    /* Creating the swizzle mask and flagging as dirty if changed. */
    MTLTextureSwizzleChannels new_swizzle_mask = MTLTextureSwizzleChannelsMake(
        swizzle_to_mtl(swizzle_mask[0]),
        swizzle_to_mtl(swizzle_mask[1]),
        swizzle_to_mtl(swizzle_mask[2]),
        swizzle_to_mtl(swizzle_mask[3]));

    mtl_swizzle_mask_ = new_swizzle_mask;
    texture_view_dirty_flags_ |= TEXTURE_VIEW_SWIZZLE_DIRTY;
  }
}

void gpu::MTLTexture::mip_range_set(int min, int max)
{
  BLI_assert(min <= max && min >= 0 && max <= mipmaps_);

  /* NOTE:
   * - mip_min_ and mip_max_ are used to Clamp LODs during sampling.
   * - For the time being, we are going to just need to generate a FULL mipmap chain
   *   as we do not know ahead of time whether mipmaps will be used.
   *
   *   TODO(Metal): Add texture initialization flag to determine whether mipmaps are used
   *   or not. Will be important for saving memory for big textures. */
  mip_min_ = min;
  mip_max_ = max;

  if ((type_ == GPU_TEXTURE_1D || type_ == GPU_TEXTURE_1D_ARRAY || type_ == GPU_TEXTURE_BUFFER) &&
      max > 1)
  {

    MTL_LOG_ERROR(
        " MTLTexture of type TEXTURE_1D_ARRAY or TEXTURE_BUFFER cannot have a mipcount "
        "greater than 1\n");
    mip_min_ = 0;
    mip_max_ = 0;
    mipmaps_ = 0;
    BLI_assert(false);
  }

  /* Mip range for texture view. */
  mip_texture_base_level_ = mip_min_;
  mip_texture_max_level_ = mip_max_;
  texture_view_dirty_flags_ |= TEXTURE_VIEW_MIP_DIRTY;
}

void *gpu::MTLTexture::read(int mip, eGPUDataFormat type)
{
  /* Prepare Array for return data. */
  BLI_assert(!(format_flag_ & GPU_FORMAT_COMPRESSED));
  BLI_assert(mip <= mipmaps_);
  BLI_assert(validate_data_format(format_, type));

  /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
  int extent[3] = {1, 1, 1};
  this->mip_size_get(mip, extent);

  size_t sample_len = extent[0] * max_ii(extent[1], 1) * max_ii(extent[2], 1);
  size_t sample_size = to_bytesize(format_, type);
  size_t texture_size = sample_len * sample_size;
  int num_channels = to_component_len(format_);

  void *data = MEM_mallocN(texture_size + 8, "GPU_texture_read");

  /* Ensure texture is baked. */
  if (is_baked_) {
    this->read_internal(
        mip, 0, 0, 0, extent[0], extent[1], extent[2], type, num_channels, texture_size + 8, data);
  }
  else {
    /* Clear return values? */
    MTL_LOG_WARNING("MTLTexture::read - reading from texture with no image data");
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
                                    size_t debug_data_size,
                                    void *r_data)
{
  /* Verify textures are baked. */
  if (!is_baked_) {
    MTL_LOG_WARNING("gpu::MTLTexture::read_internal - Trying to read from a non-baked texture!");
    return;
  }
  /* Fetch active context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  /* Calculate Desired output size. */
  int num_channels = to_component_len(format_);
  BLI_assert(num_output_components <= num_channels);
  size_t desired_output_bpp = num_output_components * to_bytesize(desired_output_format);

  /* Calculate Metal data output for trivial copy. */
  size_t image_bpp = get_mtl_format_bytesize(texture_.pixelFormat);
  uint image_components = get_mtl_format_num_components(texture_.pixelFormat);
  bool is_depth_format = (format_flag_ & GPU_FORMAT_DEPTH);

  /* Verify if we need to use compute read. */
  eGPUDataFormat data_format = to_texture_data_format(this->format_get());
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
    BLI_assert(data_format == GPU_DATA_FLOAT || data_format == GPU_DATA_UINT_24_8_DEPRECATED);
    BLI_assert(validate_data_format(format_, data_format));
  }

  /* SPECIAL Workaround for R11G11B10, TextureFormat::UNORM_10_10_10_2,
   * TextureFormat::UINT_10_10_10_2 textures requesting a read using:
   * GPU_DATA_10_11_11_REV. */
  if (desired_output_format == GPU_DATA_10_11_11_REV ||
      desired_output_format == GPU_DATA_2_10_10_10_REV)
  {
    BLI_assert(format_ == TextureFormat::UFLOAT_11_11_10 ||
               format_ == TextureFormat::UNORM_10_10_10_2 ||
               format_ == TextureFormat::UINT_10_10_10_2);

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
  size_t bytes_per_row = desired_output_bpp * width;
  size_t bytes_per_image = bytes_per_row * height;
  size_t total_bytes = bytes_per_image * max_ii(depth, 1);

  if (can_use_simple_read) {
    /* DEBUG check that if direct copy is being used, then both the expected output size matches
     * the METAL texture size. */
    BLI_assert(
        ((num_output_components * to_bytesize(desired_output_format)) == desired_output_bpp) &&
        (desired_output_bpp == image_bpp));
  }
  /* DEBUG check that the allocated data size matches the bytes we expect. */
  BLI_assert(total_bytes <= debug_data_size);
  UNUSED_VARS_NDEBUG(debug_data_size);

  /* Fetch allocation from scratch buffer. */
  gpu::MTLBuffer *dest_buf = MTLContext::get_global_memory_manager()->allocate_aligned(
      total_bytes, 256, true);
  BLI_assert(dest_buf != nullptr);

  id<MTLBuffer> destination_buffer = dest_buf->get_metal_buffer();
  BLI_assert(destination_buffer != nil);
  void *destination_buffer_host_ptr = dest_buf->get_host_ptr();
  BLI_assert(destination_buffer_host_ptr != nullptr);

  /* Prepare specialization struct (For non-trivial texture read routine). */
  int depth_format_mode = 0;
  if (is_depth_format) {
    depth_format_mode = 1;
    switch (desired_output_format) {
      case GPU_DATA_FLOAT:
        depth_format_mode = 1;
        break;
      case GPU_DATA_UINT_24_8_DEPRECATED:
        depth_format_mode = 2;
        break;
      case GPU_DATA_UINT:
        depth_format_mode = 4;
        break;
      default:
        BLI_assert_msg(false, "Unhandled depth read format case");
        break;
    }
  }

  TextureReadRoutineSpecialisation compute_specialization_kernel = {
      tex_data_format_to_msl_texture_template_type(data_format), /* TEXTURE DATA TYPE */
      tex_data_format_to_msl_type_str(desired_output_format),    /* OUTPUT DATA TYPE */
      num_channels,                                              /* TEXTURE COMPONENT COUNT */
      num_output_components,                                     /* OUTPUT DATA COMPONENT COUNT */
      depth_format_mode};

  bool copy_successful = false;
  @autoreleasepool {

    /* TODO(Metal): Verify whether we need some form of barrier here to ensure reads
     * happen after work with associated texture is finished. */
    GPU_finish();

    /** Determine source read texture handle. */
    id<MTLTexture> read_texture = texture_;
    /* Use texture-view handle if reading from a GPU texture view. */
    if (resource_mode_ == MTL_TEXTURE_MODE_TEXTURE_VIEW) {
      read_texture = this->get_metal_handle();
    }
    /* Create Texture View for SRGB special case to bypass internal type conversion. */
    if (format_ == TextureFormat::SRGBA_8_8_8_8) {
      BLI_assert(internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_FORMAT_VIEW);
      read_texture = [read_texture newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm];
    }

    /* Perform per-texture type read. */
    switch (type_) {
      case GPU_TEXTURE_1D: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
          if (G.debug & G_DEBUG_GPU) {
            [enc insertDebugSignpost:@"GPUTextureRead1D"];
          }
          [enc copyFromTexture:read_texture
                           sourceSlice:0
                           sourceLevel:mip
                          sourceOrigin:MTLOriginMake(x_off, 0, 0)
                            sourceSize:MTLSizeMake(width, 1, 1)
                              toBuffer:destination_buffer
                     destinationOffset:0
                destinationBytesPerRow:bytes_per_row
              destinationBytesPerImage:bytes_per_image];
          copy_successful = true;
        }
        else {

          /* Use Compute READ. */
          id<MTLComputeCommandEncoder> compute_encoder =
              ctx->main_command_buffer.ensure_begin_compute_encoder();
          id<MTLComputePipelineState> pso = texture_read_1d_get_kernel(
              compute_specialization_kernel);
          TextureReadParams params = {
              mip,
              {width, 1, 1},
              {x_off, 0, 0},
          };

          /* Bind resources via compute state for optimal state caching performance. */
          MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
          cs.bind_pso(pso);
          cs.bind_compute_bytes(&params, sizeof(params), 0);
          cs.bind_compute_buffer(destination_buffer, 0, 1);
          cs.bind_compute_texture(read_texture, 0);
          [compute_encoder dispatchThreads:MTLSizeMake(width, 1, 1) /* Width, Height, Layer */
                     threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_1D_ARRAY: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
          if (G.debug & G_DEBUG_GPU) {
            [enc insertDebugSignpost:@"GPUTextureRead1DArray"];
          }

          int base_slice = y_off;
          int final_slice = base_slice + height;
          size_t texture_array_relative_offset = 0;

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {
            [enc copyFromTexture:read_texture
                             sourceSlice:base_slice
                             sourceLevel:mip
                            sourceOrigin:MTLOriginMake(x_off, 0, 0)
                              sourceSize:MTLSizeMake(width, 1, 1)
                                toBuffer:destination_buffer
                       destinationOffset:texture_array_relative_offset
                  destinationBytesPerRow:bytes_per_row
                destinationBytesPerImage:bytes_per_row];
            texture_array_relative_offset += bytes_per_row;
          }
          copy_successful = true;
        }
        else {
          /* Use Compute READ. */
          id<MTLComputeCommandEncoder> compute_encoder =
              ctx->main_command_buffer.ensure_begin_compute_encoder();
          id<MTLComputePipelineState> pso = texture_read_1d_array_get_kernel(
              compute_specialization_kernel);
          TextureReadParams params = {
              mip,
              {width, height, 1},
              {x_off, y_off, 0},
          };

          /* Bind resources via compute state for optimal state caching performance. */
          MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
          cs.bind_pso(pso);
          cs.bind_compute_bytes(&params, sizeof(params), 0);
          cs.bind_compute_buffer(destination_buffer, 0, 1);
          cs.bind_compute_texture(read_texture, 0);
          [compute_encoder dispatchThreads:MTLSizeMake(width, height, 1) /* Width, Height, Layer */
                     threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_2D: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
          if (G.debug & G_DEBUG_GPU) {
            [enc insertDebugSignpost:@"GPUTextureRead2D"];
          }
          [enc copyFromTexture:read_texture
                           sourceSlice:0
                           sourceLevel:mip
                          sourceOrigin:MTLOriginMake(x_off, y_off, 0)
                            sourceSize:MTLSizeMake(width, height, 1)
                              toBuffer:destination_buffer
                     destinationOffset:0
                destinationBytesPerRow:bytes_per_row
              destinationBytesPerImage:bytes_per_image];
          copy_successful = true;
        }
        else {

          /* Use Compute READ. */
          id<MTLComputeCommandEncoder> compute_encoder =
              ctx->main_command_buffer.ensure_begin_compute_encoder();
          id<MTLComputePipelineState> pso = texture_read_2d_get_kernel(
              compute_specialization_kernel);
          TextureReadParams params = {
              mip,
              {width, height, 1},
              {x_off, y_off, 0},
          };

          /* Bind resources via compute state for optimal state caching performance. */
          MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
          cs.bind_pso(pso);
          cs.bind_compute_bytes(&params, sizeof(params), 0);
          cs.bind_compute_buffer(destination_buffer, 0, 1);
          cs.bind_compute_texture(read_texture, 0);
          [compute_encoder dispatchThreads:MTLSizeMake(width, height, 1) /* Width, Height, Layer */
                     threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_2D_ARRAY: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
          if (G.debug & G_DEBUG_GPU) {
            [enc insertDebugSignpost:@"GPUTextureRead2DArray"];
          }
          int base_slice = z_off;
          int final_slice = base_slice + depth;
          size_t texture_array_relative_offset = 0;

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {
            [enc copyFromTexture:read_texture
                             sourceSlice:array_slice
                             sourceLevel:mip
                            sourceOrigin:MTLOriginMake(x_off, y_off, 0)
                              sourceSize:MTLSizeMake(width, height, 1)
                                toBuffer:destination_buffer
                       destinationOffset:texture_array_relative_offset
                  destinationBytesPerRow:bytes_per_row
                destinationBytesPerImage:bytes_per_image];
            texture_array_relative_offset += bytes_per_image;
          }
          copy_successful = true;
        }
        else {

          /* Use Compute READ */
          id<MTLComputeCommandEncoder> compute_encoder =
              ctx->main_command_buffer.ensure_begin_compute_encoder();
          id<MTLComputePipelineState> pso = texture_read_2d_array_get_kernel(
              compute_specialization_kernel);
          TextureReadParams params = {
              mip,
              {width, height, depth},
              {x_off, y_off, z_off},
          };

          /* Bind resources via compute state for optimal state caching performance. */
          MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
          cs.bind_pso(pso);
          cs.bind_compute_bytes(&params, sizeof(params), 0);
          cs.bind_compute_buffer(destination_buffer, 0, 1);
          cs.bind_compute_texture(read_texture, 0);
          [compute_encoder
                    dispatchThreads:MTLSizeMake(width, height, depth) /* Width, Height, Layer */
              threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_3D: {
        if (can_use_simple_read) {
          /* Use Blit Encoder READ. */
          id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
          if (G.debug & G_DEBUG_GPU) {
            [enc insertDebugSignpost:@"GPUTextureRead3D"];
          }
          [enc copyFromTexture:read_texture
                           sourceSlice:0
                           sourceLevel:mip
                          sourceOrigin:MTLOriginMake(x_off, y_off, z_off)
                            sourceSize:MTLSizeMake(width, height, depth)
                              toBuffer:destination_buffer
                     destinationOffset:0
                destinationBytesPerRow:bytes_per_row
              destinationBytesPerImage:bytes_per_image];
          copy_successful = true;
        }
        else {

          /* Use Compute READ. */
          id<MTLComputeCommandEncoder> compute_encoder =
              ctx->main_command_buffer.ensure_begin_compute_encoder();
          id<MTLComputePipelineState> pso = texture_read_3d_get_kernel(
              compute_specialization_kernel);
          TextureReadParams params = {
              mip,
              {width, height, depth},
              {x_off, y_off, z_off},
          };

          /* Bind resources via compute state for optimal state caching performance. */
          MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
          cs.bind_pso(pso);
          cs.bind_compute_bytes(&params, sizeof(params), 0);
          cs.bind_compute_buffer(destination_buffer, 0, 1);
          cs.bind_compute_texture(read_texture, 0);
          [compute_encoder
                    dispatchThreads:MTLSizeMake(width, height, depth) /* Width, Height, Layer */
              threadsPerThreadgroup:MTLSizeMake(4, 4, 4)];
          copy_successful = true;
        }
      } break;

      case GPU_TEXTURE_CUBE:
      case GPU_TEXTURE_CUBE_ARRAY: {
        BLI_assert_msg(z_off == 0 || type_ == GPU_TEXTURE_CUBE_ARRAY,
                       "z_off > 0 is only supported by TEXTURE CUBE ARRAY reads.");
        BLI_assert_msg(depth <= 6 || type_ == GPU_TEXTURE_CUBE_ARRAY,
                       "depth > 6 is only supported by TEXTURE CUBE ARRAY reads. ");
        if (can_use_simple_read) {
          id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
          if (G.debug & G_DEBUG_GPU) {
            [enc insertDebugSignpost:@"GPUTextureReadCubeArray"];
          }

          /* NOTE: Depth should have a minimum value of 1 as we read at least one slice. */
          int base_slice = z_off;
          int final_slice = base_slice + depth;
          size_t texture_array_relative_offset = 0;

          for (int array_slice = base_slice; array_slice < final_slice; array_slice++) {
            [enc copyFromTexture:read_texture
                             sourceSlice:array_slice
                             sourceLevel:mip
                            sourceOrigin:MTLOriginMake(x_off, y_off, 0)
                              sourceSize:MTLSizeMake(width, height, 1)
                                toBuffer:destination_buffer
                       destinationOffset:texture_array_relative_offset
                  destinationBytesPerRow:bytes_per_row
                destinationBytesPerImage:bytes_per_image];

            texture_array_relative_offset += bytes_per_image;
          }
          MTL_LOG_DEBUG("Copying texture data to buffer GPU_TEXTURE_CUBE_ARRAY");
          copy_successful = true;
        }
        else {
          MTL_LOG_ERROR("TODO(Metal): unsupported compute copy of texture cube array");
        }
      } break;

      default:
        MTL_LOG_ERROR(
            "gpu::MTLTexture::read_internal simple-copy not yet supported for texture "
            "type: %d",
            (int)type_);
        break;
    }

    if (copy_successful) {

      /* Use Blit encoder to synchronize results back to CPU. */
      if (dest_buf->get_resource_options() == MTLResourceStorageModeManaged) {
        id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
        if (G.debug & G_DEBUG_GPU) {
          [enc insertDebugSignpost:@"GPUTextureRead-syncResource"];
        }
        [enc synchronizeResource:destination_buffer];
      }

      /* Ensure GPU copy commands have completed. */
      GPU_finish();

      /* Copy data from Shared Memory into ptr. */
      memcpy(r_data, destination_buffer_host_ptr, total_bytes);
      MTL_LOG_DEBUG("gpu::MTLTexture::read_internal success! %lu bytes read", total_bytes);
    }
    else {
      MTL_LOG_WARNING(
          "gpu::MTLTexture::read_internal not yet supported for this config -- data "
          "format different (src %lu bytes, dst %lu bytes) (src format: %d, dst format: %d), or "
          "varying component counts (src %d, dst %d)",
          image_bpp,
          desired_output_bpp,
          (int)data_format,
          (int)desired_output_format,
          image_components,
          num_output_components);
    }

    /* Release destination buffer. */
    dest_buf->free();
  }
}

bool gpu::MTLTexture::init_internal()
{
  this->prepare_internal();
  /* TODO(jbakker): Other limit checks should be added as well. When a texture violates a limit it
   * is not backed by a texture and will crash when used. */
  const int limit = GPU_max_texture_3d_size();
  if ((type_ == GPU_TEXTURE_3D) && (w_ > limit || h_ > limit || d_ > limit)) {
    return false;
  }
  return true;
}

bool gpu::MTLTexture::init_internal(VertBuf *vbo)
{
  MTLPixelFormat mtl_format = gpu_texture_format_to_metal(this->format_);
  mtl_max_mips_ = 1;
  mipmaps_ = 0;
  this->mip_range_set(0, 0);

  /* Create texture from VertBuf's buffer. */
  MTLVertBuf *mtl_vbo = static_cast<MTLVertBuf *>(vbo);
  mtl_vbo->bind();
  mtl_vbo->flag_used();

  /* Get Metal Buffer. */
  id<MTLBuffer> source_buffer = mtl_vbo->get_metal_buffer();
  BLI_assert(source_buffer);

  /* Verify size. */
  if (w_ <= 0) {
    MTL_LOG_WARNING("Allocating texture buffer of width 0!");
    w_ = 1;
  }

  /* Verify Texture and vertex buffer alignment. */
  const GPUVertFormat *format = GPU_vertbuf_get_format(vbo);
  size_t bytes_per_pixel = get_mtl_format_bytesize(mtl_format);
  size_t bytes_per_row = bytes_per_pixel * w_;

  MTLContext *mtl_ctx = MTLContext::get();
  uint32_t align_requirement = uint32_t(
      [mtl_ctx->device minimumLinearTextureAlignmentForPixelFormat:mtl_format]);

  /* If stride is larger than bytes per pixel, but format has multiple attributes,
   * split attributes across several pixels. */
  if (format->stride > bytes_per_pixel && format->attr_len > 1) {

    /* We need to increase the number of pixels available to store additional attributes.
     * First ensure that the total stride of the vertex format fits uniformly into
     * multiple pixels. If these sizes are different, then attributes are of differing
     * sizes and this operation is unsupported. */
    if (bytes_per_pixel * format->attr_len != format->stride) {
      BLI_assert_msg(false,
                     "Cannot split attributes across multiple pixels as attribute format sizes do "
                     "not match.");
      return false;
    }

    /* Provide a single pixel per attribute. */
    /* Increase bytes per row to ensure there are enough bytes for all vertex attribute data. */
    bytes_per_row *= format->attr_len;
    BLI_assert(bytes_per_row == format->stride * w_);

    /* Multiply width of image to provide one attribute per pixel. */
    w_ *= format->attr_len;
    BLI_assert(bytes_per_row == bytes_per_pixel * w_);
    BLI_assert_msg(w_ == mtl_vbo->vertex_len * format->attr_len,
                   "Image should contain one pixel for each attribute in every vertex.");
  }
  else {
    /* Verify per-vertex size aligns with texture size. */
    BLI_assert(bytes_per_pixel == format->stride &&
               "Pixel format stride MUST match the texture format stride -- These being different "
               "is likely caused by Metal's VBO padding to a minimum of 4-bytes per-vertex."
               " If multiple attributes are used. Each attribute is to be packed into its own "
               "individual pixel when stride length is exceeded. ");
  }

  /* Create texture descriptor. */
  BLI_assert(type_ == GPU_TEXTURE_BUFFER);
  texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
  texture_descriptor_.pixelFormat = mtl_format;
  texture_descriptor_.textureType = MTLTextureTypeTextureBuffer;
  texture_descriptor_.width = w_;
  texture_descriptor_.height = 1;
  texture_descriptor_.depth = 1;
  texture_descriptor_.arrayLength = 1;
  texture_descriptor_.mipmapLevelCount = mtl_max_mips_;
  texture_descriptor_.usage =
      MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite |
      MTLTextureUsagePixelFormatView; /* TODO(Metal): Optimize usage flags. */
  texture_descriptor_.storageMode = [source_buffer storageMode];
  texture_descriptor_.sampleCount = 1;
  texture_descriptor_.cpuCacheMode = [source_buffer cpuCacheMode];
  texture_descriptor_.hazardTrackingMode = [source_buffer hazardTrackingMode];

  texture_ = [source_buffer
      newTextureWithDescriptor:texture_descriptor_
                        offset:0
                   bytesPerRow:ceil_to_multiple_ul(bytes_per_row, align_requirement)];
  aligned_w_ = bytes_per_row / bytes_per_pixel;

  BLI_assert(texture_);
  texture_.label = [NSString stringWithUTF8String:this->get_name()];
  is_baked_ = true;
  is_dirty_ = false;
  resource_mode_ = MTL_TEXTURE_MODE_VBO;

  /* Track Status. */
  vert_buffer_ = mtl_vbo;
  vert_buffer_mtl_ = source_buffer;

  return true;
}

bool gpu::MTLTexture::init_internal(gpu::Texture *src,
                                    int mip_offset,
                                    int layer_offset,
                                    bool use_stencil)
{
  BLI_assert(src);

  /* Zero initialize. */
  this->prepare_internal();

  /* Flag as using texture view. */
  resource_mode_ = MTL_TEXTURE_MODE_TEXTURE_VIEW;
  source_texture_ = src;
  mip_texture_base_level_ = mip_offset;
  mip_texture_base_layer_ = layer_offset;
  texture_view_dirty_flags_ |= TEXTURE_VIEW_MIP_DIRTY;

  /* Assign usage. */
  internal_gpu_image_usage_flags_ = GPU_texture_usage(src);

  /* Assign texture as view. */
  gpu::MTLTexture *mtltex = static_cast<gpu::MTLTexture *>(src);
  mtltex->ensure_baked();
  texture_ = mtltex->texture_;
  BLI_assert(texture_);
  [texture_ retain];

  /* Flag texture as baked -- we do not need explicit initialization. */
  is_baked_ = true;
  is_dirty_ = false;

  /* Stencil view support. */
  texture_view_stencil_ = false;
  if (use_stencil) {
    BLI_assert(ELEM(format_, TextureFormat::SFLOAT_32_DEPTH_UINT_8));
    texture_view_stencil_ = true;
  }

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
  return is_baked_;
}

/* Prepare texture parameters after initialization, but before baking. */
void gpu::MTLTexture::prepare_internal()
{
  /* Take a copy of the flags so that any modifications we make won't effect the texture
   * cache/pool match finding test. */
  internal_gpu_image_usage_flags_ = gpu_image_usage_flags_;

  /* Metal: Texture clearing is done using frame-buffer clear. This has no performance impact or
   * bandwidth implications for lossless compression and is considered best-practice.
   *
   * Attachment usage also required for depth-stencil attachment targets, for depth-update support.
   * NOTE: Emulated atomic textures cannot support render-target usage. For clearing, the backing
   * buffer is cleared instead.
   */
  if (!((internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATOMIC) &&
        !MTLBackend::get_capabilities().supports_texture_atomics))
  {
    /* Force attachment usage - see comment above. */
    internal_gpu_image_usage_flags_ |= GPU_TEXTURE_USAGE_ATTACHMENT;
  }

  /* Derive maximum number of mip levels by default.
   * TODO(Metal): This can be removed if max mip counts are specified upfront. */
  if (type_ == GPU_TEXTURE_1D || type_ == GPU_TEXTURE_1D_ARRAY || type_ == GPU_TEXTURE_BUFFER) {
    mtl_max_mips_ = 1;
  }
  else {
    /* Require correct explicit mipmap level counts. */
    mtl_max_mips_ = mipmaps_;
  }
}

void gpu::MTLTexture::ensure_baked()
{

  /* If properties have changed, re-bake. */
  id<MTLTexture> previous_texture = nil;
  bool copy_previous_contents = false;

  if (is_baked_ && is_dirty_) {
    copy_previous_contents = true;
    previous_texture = texture_;
    [previous_texture retain];
    this->reset();
  }

  if (!is_baked_) {
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /* Ensure texture mode is valid. */
    BLI_assert(resource_mode_ != MTL_TEXTURE_MODE_EXTERNAL);
    BLI_assert(resource_mode_ != MTL_TEXTURE_MODE_TEXTURE_VIEW);
    BLI_assert(resource_mode_ != MTL_TEXTURE_MODE_VBO);

    /* Format and mip levels (TODO(Metal): Optimize mipmaps counts, specify up-front). */
    MTLPixelFormat mtl_format = gpu_texture_format_to_metal(format_);

    /* SRGB textures require a texture view for reading data and when rendering with SRGB
     * disabled. Enabling the texture_view or texture_read usage flags disables lossless
     * compression, so the situations in which it is used should be limited. */
    if (format_ == TextureFormat::SRGBA_8_8_8_8) {
      internal_gpu_image_usage_flags_ |= GPU_TEXTURE_USAGE_FORMAT_VIEW;
    }

    /* Create texture descriptor. */
    switch (type_) {

      /* 1D */
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_1D_ARRAY: {
        BLI_assert(w_ > 0);
        texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        texture_descriptor_.pixelFormat = mtl_format;
        texture_descriptor_.textureType = (type_ == GPU_TEXTURE_1D_ARRAY) ? MTLTextureType1DArray :
                                                                            MTLTextureType1D;
        texture_descriptor_.width = w_;
        texture_descriptor_.height = 1;
        texture_descriptor_.depth = 1;
        texture_descriptor_.arrayLength = (type_ == GPU_TEXTURE_1D_ARRAY) ? h_ : 1;
        texture_descriptor_.mipmapLevelCount = (mtl_max_mips_ > 0) ? mtl_max_mips_ : 1;
        texture_descriptor_.usage = mtl_usage_from_gpu(internal_gpu_image_usage_flags_);
        texture_descriptor_.storageMode = MTLStorageModePrivate;
        texture_descriptor_.sampleCount = 1;
        texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* 2D */
      case GPU_TEXTURE_2D:
      case GPU_TEXTURE_2D_ARRAY: {
        BLI_assert(w_ > 0 && h_ > 0);
        texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        texture_descriptor_.pixelFormat = mtl_format;
        texture_descriptor_.textureType = (type_ == GPU_TEXTURE_2D_ARRAY) ? MTLTextureType2DArray :
                                                                            MTLTextureType2D;
        texture_descriptor_.width = w_;
        texture_descriptor_.height = h_;
        texture_descriptor_.depth = 1;
        texture_descriptor_.arrayLength = (type_ == GPU_TEXTURE_2D_ARRAY) ? d_ : 1;
        texture_descriptor_.mipmapLevelCount = (mtl_max_mips_ > 0) ? mtl_max_mips_ : 1;
        texture_descriptor_.usage = mtl_usage_from_gpu(internal_gpu_image_usage_flags_);
        texture_descriptor_.storageMode = MTLStorageModePrivate;
        texture_descriptor_.sampleCount = 1;
        texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* 3D */
      case GPU_TEXTURE_3D: {
        BLI_assert(w_ > 0 && h_ > 0 && d_ > 0);
        texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        texture_descriptor_.pixelFormat = mtl_format;
        texture_descriptor_.textureType = MTLTextureType3D;
        texture_descriptor_.width = w_;
        texture_descriptor_.height = h_;
        texture_descriptor_.depth = d_;
        texture_descriptor_.arrayLength = 1;
        texture_descriptor_.mipmapLevelCount = (mtl_max_mips_ > 0) ? mtl_max_mips_ : 1;
        texture_descriptor_.usage = mtl_usage_from_gpu(internal_gpu_image_usage_flags_);
        texture_descriptor_.storageMode = MTLStorageModePrivate;
        texture_descriptor_.sampleCount = 1;
        texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* CUBE TEXTURES */
      case GPU_TEXTURE_CUBE:
      case GPU_TEXTURE_CUBE_ARRAY: {
        /* NOTE: For a cube-map 'Texture::d_' refers to total number of faces,
         * not just array slices. */
        BLI_assert(w_ > 0 && h_ > 0);
        texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        texture_descriptor_.pixelFormat = mtl_format;
        texture_descriptor_.textureType = (type_ == GPU_TEXTURE_CUBE_ARRAY) ?
                                              MTLTextureTypeCubeArray :
                                              MTLTextureTypeCube;
        texture_descriptor_.width = w_;
        texture_descriptor_.height = h_;
        texture_descriptor_.depth = 1;
        texture_descriptor_.arrayLength = (type_ == GPU_TEXTURE_CUBE_ARRAY) ? d_ / 6 : 1;
        texture_descriptor_.mipmapLevelCount = (mtl_max_mips_ > 0) ? mtl_max_mips_ : 1;
        texture_descriptor_.usage = mtl_usage_from_gpu(internal_gpu_image_usage_flags_);
        texture_descriptor_.storageMode = MTLStorageModePrivate;
        texture_descriptor_.sampleCount = 1;
        texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      /* GPU_TEXTURE_BUFFER */
      case GPU_TEXTURE_BUFFER: {
        texture_descriptor_ = [[MTLTextureDescriptor alloc] init];
        texture_descriptor_.pixelFormat = mtl_format;
        texture_descriptor_.textureType = MTLTextureTypeTextureBuffer;
        texture_descriptor_.width = w_;
        texture_descriptor_.height = 1;
        texture_descriptor_.depth = 1;
        texture_descriptor_.arrayLength = 1;
        texture_descriptor_.mipmapLevelCount = (mtl_max_mips_ > 0) ? mtl_max_mips_ : 1;
        texture_descriptor_.usage = mtl_usage_from_gpu(internal_gpu_image_usage_flags_);
        texture_descriptor_.storageMode = MTLStorageModePrivate;
        texture_descriptor_.sampleCount = 1;
        texture_descriptor_.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        texture_descriptor_.hazardTrackingMode = MTLHazardTrackingModeDefault;
      } break;

      default: {
        MTL_LOG_ERROR("[METAL] Error: Cannot create texture with unknown type: %d\n", type_);
        return;
      } break;
    }

    /* Determine Resource Mode. */
    resource_mode_ = MTL_TEXTURE_MODE_DEFAULT;

    /* Override storage mode if memoryless attachments are being used.
     * NOTE: Memoryless textures can only be supported on TBDR GPUs. */
    if (internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_MEMORYLESS) {
      const bool is_tile_based_arch = (GPU_platform_architecture() == GPU_ARCHITECTURE_TBDR);
      if (is_tile_based_arch) {
        texture_descriptor_.storageMode = MTLStorageModeMemoryless;
      }
    }

    /** Atomic texture fallback.
     * If texture atomic operations are required and are not natively supported, we instead
     * allocate a buffer-backed 2D texture and perform atomic operations on this instead. Support
     * for 2D Array textures and 3D textures is achieved via packing layers into the 2D texture. */
    bool native_texture_atomics = MTLBackend::get_capabilities().supports_texture_atomics;
    if ((internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATOMIC) && !native_texture_atomics) {

      /* Validate format support. */
      BLI_assert_msg(ELEM(type_, GPU_TEXTURE_2D, GPU_TEXTURE_2D_ARRAY, GPU_TEXTURE_3D),
                     "Texture atomic fallback support is only available for GPU_TEXTURE_2D, "
                     "GPU_TEXTURE_2D_ARRAY and GPU_TEXTURE_3D.");

      /* Re-assign 2D resolution to encompass all texture layers.
       * Texture access is handled by remapping to a linear ID and using this in the destination
       * texture. based on original with: LinearPxID = x + y*layer_w + z*(layer_h*layer_w);
       * tx_2d.y = LinearPxID/2D_tex_width;
       * tx_2d.x = LinearPxID - (tx_2d.y*2D_tex_width); */
      if (ELEM(type_, GPU_TEXTURE_2D_ARRAY, GPU_TEXTURE_3D)) {
        /* Maximum 2D texture dimensions will be (16384, 16384) on all target platforms. */
        const uint max_width = 16384;
        const uint max_height = 16384;
        const uint pixels_res = w_ * h_ * d_;

        uint new_w = 0, new_h = 0;
        if (pixels_res <= max_width) {
          new_w = pixels_res;
          new_h = 1;
        }
        else {
          new_w = max_width;
          new_h = ((pixels_res % new_w) == 0) ? (pixels_res / new_w) : ((pixels_res / new_w) + 1);
        }

        texture_descriptor_.width = new_w;
        texture_descriptor_.height = new_h;

        UNUSED_VARS_NDEBUG(max_height);
        BLI_assert_msg(texture_descriptor_.width <= max_width &&
                           texture_descriptor_.height <= max_height,
                       "Atomic fallback support texture is too large.");
      }

      /* Allocate buffer for texture data. */
      size_t bytes_per_pixel = get_mtl_format_bytesize(mtl_format);
      size_t bytes_per_row = bytes_per_pixel * texture_descriptor_.width;
      size_t total_bytes = bytes_per_row * texture_descriptor_.height;

      backing_buffer_ = MTLContext::get_global_memory_manager()->allocate(
          total_bytes, (internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_HOST_READ));
      BLI_assert(backing_buffer_ != nullptr);

      /* NOTE: Fallback buffer-backed texture always set to Texture2D. */
      texture_descriptor_.textureType = MTLTextureType2D;
      texture_descriptor_.depth = 1;
      texture_descriptor_.arrayLength = 1;

      /* Write texture dimensions to metadata. This is required to remap 2D Array/3D sample
       * coordinates into 2D texture space. */
      tex_buffer_metadata_[0] = w_;
      tex_buffer_metadata_[1] = h_;
      tex_buffer_metadata_[2] = d_;

      /* Texture allocation with buffer as backing storage. Bytes per row must satisfy alignment
       * rules for device. */
      uint32_t align_requirement = uint32_t(
          [ctx->device minimumLinearTextureAlignmentForPixelFormat:mtl_format]);
      size_t aligned_bytes_per_row = ceil_to_multiple_ul(bytes_per_row, align_requirement);
      texture_ = [backing_buffer_->get_metal_buffer()
          newTextureWithDescriptor:texture_descriptor_
                            offset:0
                       bytesPerRow:aligned_bytes_per_row];
      /* Aligned width. */
      tex_buffer_metadata_[3] = bytes_per_row / bytes_per_pixel;

#ifndef NDEBUG
      texture_.label = [NSString
          stringWithFormat:@"AtomicBufferBackedTexture_%s", this->get_name()];
#endif
    }
    else {

      /* Standard texture allocation. */
      texture_ = [ctx->device newTextureWithDescriptor:texture_descriptor_];

#ifndef NDEBUG
      if (internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_MEMORYLESS) {
        texture_.label = [NSString stringWithFormat:@"MemorylessTexture_%s", this->get_name()];
      }
      else {
        texture_.label = [NSString stringWithFormat:@"Texture_%s", this->get_name()];
      }
#endif
    }

    BLI_assert(texture_);
    is_baked_ = true;
    is_dirty_ = false;
  }

  /* Re-apply previous contents. */
  if (copy_previous_contents) {
    /* TODO(Metal): May need to copy previous contents of texture into new texture. */
    [previous_texture release];
  }
}

void gpu::MTLTexture::reset()
{
  MTL_LOG_DEBUG("Texture %s reset. Size %d, %d, %d", this->get_name(), w_, h_, d_);
  /* Delete associated METAL resources. */
  if (texture_ != nil) {
    [texture_ release];
    texture_ = nil;
    is_baked_ = false;
    is_dirty_ = true;
  }

  /* Release backing Metal buffer, if used. */
  if (backing_buffer_ != nullptr) {
    backing_buffer_->free();
    backing_buffer_ = nullptr;
  }

  /* Release backing storage buffer, if used. */
  if (storage_buffer_ != nullptr) {
    delete storage_buffer_;
    storage_buffer_ = nullptr;
  }

  if (texture_no_srgb_ != nil) {
    [texture_no_srgb_ release];
    texture_no_srgb_ = nil;
  }

  if (mip_swizzle_view_ != nil) {
    [mip_swizzle_view_ release];
    mip_swizzle_view_ = nil;
  }

  /* Blit framebuffer. */
  if (blit_fb_) {
    GPU_framebuffer_free(blit_fb_);
    blit_fb_ = nullptr;
  }

  /* Descriptor. */
  if (texture_descriptor_ != nullptr) {
    [texture_descriptor_ release];
    texture_descriptor_ = nullptr;
  }

  /* Reset mipmap state. */
  has_generated_mips_ = false;

  BLI_assert(texture_ == nil);
  BLI_assert(mip_swizzle_view_ == nil);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Alias resource access to buffer backed content using Storage Buffer.
 * \{ */
MTLStorageBuf *gpu::MTLTexture::get_storagebuf()
{
  BLI_assert_msg(
      backing_buffer_ != nullptr,
      "Resource must have been created as a buffer backed resource to support SSBO wrapping.");
  /* Ensure texture resource is up to date. */
  this->ensure_baked();
  if (storage_buffer_ == nil) {
    BLI_assert(texture_ != nullptr);
    id<MTLBuffer> backing_buffer = [texture_ buffer];
    BLI_assert(backing_buffer != nil);
    storage_buffer_ = new MTLStorageBuf(this, [backing_buffer length]);
  }
  return storage_buffer_;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name SRGB Handling.
 * \{ */
bool MTLTexture::is_format_srgb()
{
  return (format_ == TextureFormat::SRGBA_8_8_8_8);
}

id<MTLTexture> MTLTexture::get_non_srgb_handle()
{
  id<MTLTexture> base_tex = get_metal_handle_base();
  BLI_assert(base_tex != nil);
  if (texture_no_srgb_ == nil) {
    texture_no_srgb_ = [base_tex newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm];
  }
  return texture_no_srgb_;
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Pixel Buffer
 * \{ */

MTLPixelBuffer::MTLPixelBuffer(size_t size) : PixelBuffer(size)
{
  /* Ensure buffer satisfies the alignment of 256 bytes for copying
   * data between buffers and textures. As specified in:
   * https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf */
  BLI_assert(size >= 256);
  buffer_ = nil;
}

MTLPixelBuffer::~MTLPixelBuffer()
{
  if (buffer_) {
    [buffer_ release];
    buffer_ = nil;
  }
}

void *MTLPixelBuffer::map()
{
  /* Duplicate the existing buffer and release original to ensure we do not directly modify data
   * in-flight on the GPU. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  MTLResourceOptions resource_options = ([ctx->device hasUnifiedMemory]) ?
                                            MTLResourceStorageModeShared :
                                            MTLResourceStorageModeManaged;

  if (buffer_ != nil) {
    id<MTLBuffer> new_buffer = [ctx->device newBufferWithBytes:[buffer_ contents]
                                                        length:size_
                                                       options:resource_options];
    [buffer_ release];
    buffer_ = new_buffer;
  }
  else {
    buffer_ = [ctx->device newBufferWithLength:size_ options:resource_options];
  }

  return [buffer_ contents];
}

void MTLPixelBuffer::unmap()
{
  if (buffer_ == nil) {
    return;
  }

  /* Ensure changes are synchronized. */
  if (buffer_.resourceOptions & MTLResourceStorageModeManaged) {
    [buffer_ didModifyRange:NSMakeRange(0, size_)];
  }
}

GPUPixelBufferNativeHandle MTLPixelBuffer::get_native_handle()
{
  GPUPixelBufferNativeHandle native_handle;

  /* Only supported with unified memory currently. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  if (![ctx->device hasUnifiedMemory]) {
    return native_handle;
  }

  /* Just get pointer to unified memory. No need to unmap. */
  map();
  native_handle.handle = reinterpret_cast<int64_t>(buffer_);
  native_handle.size = size_;

  return native_handle;
}

size_t MTLPixelBuffer::get_size()
{
  return size_;
}

id<MTLBuffer> MTLPixelBuffer::get_metal_buffer()
{
  return buffer_;
}

/** \} */

}  // namespace blender::gpu

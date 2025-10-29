/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"
#include "BLI_map.hh"
#include "GPU_texture.hh"
#include "MEM_guardedalloc.h"

#include "gpu_texture_private.hh"

#include <mutex>
#include <string>
#include <thread>

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLRenderPipelineState;

namespace blender::gpu {
class FrameBuffer;
}  // namespace blender::gpu

/* Texture Update system structs. */
struct TextureUpdateRoutineSpecialisation {

  /* The METAL type of data in input array, e.g. half, float, short, int */
  std::string input_data_type;

  /* The type of the texture data texture2d<T,..>, e.g. T=float, half, int etc. */
  std::string output_data_type;

  /* Number of image channels provided in input texture data array (min=1, max=4). */
  int component_count_input;

  /* Number of channels the destination texture has (min=1, max=4). */
  int component_count_output;

  /* Whether the update routine is a clear, and only the first texel of the input data buffer will
   * be read. */
  bool is_clear;

  bool operator==(const TextureUpdateRoutineSpecialisation &other) const
  {
    return ((input_data_type == other.input_data_type) &&
            (output_data_type == other.output_data_type) &&
            (component_count_input == other.component_count_input) &&
            (component_count_output == other.component_count_output) &&
            (is_clear == other.is_clear));
  }

  uint64_t hash() const
  {
    blender::DefaultHash<std::string> string_hasher;
    return (uint64_t)string_hasher(this->input_data_type + this->output_data_type +
                                   std::to_string((this->component_count_input << 9) |
                                                  (this->component_count_output << 5) |
                                                  (this->is_clear ? 1 : 0)));
  }
};

/* Type of data is being written to the depth target:
 * 0 = floating point (0.0 - 1.0)
 * 1 = 24 bit integer (0 - 2^24)
 * 2 = 32 bit integer (0 - 2^32) */

enum DepthTextureUpdateMode {
  MTL_DEPTH_UPDATE_MODE_FLOAT = 0,
  MTL_DEPTH_UPDATE_MODE_INT24 = 1,
  MTL_DEPTH_UPDATE_MODE_INT32 = 2
};

struct DepthTextureUpdateRoutineSpecialisation {
  DepthTextureUpdateMode data_mode;

  bool operator==(const DepthTextureUpdateRoutineSpecialisation &other) const
  {
    return ((data_mode == other.data_mode));
  }

  uint64_t hash() const
  {
    return (uint64_t)(this->data_mode);
  }
};

/* Texture Read system structs. */
struct TextureReadRoutineSpecialisation {
  std::string input_data_type;
  std::string output_data_type;
  int component_count_input;
  int component_count_output;

  /* Format for depth data.
   * 0 = Not a Depth format,
   * 1 = FLOAT DEPTH,
   * 2 = 24Bit Integer Depth,
   * 4 = 32bit Unsigned-Integer Depth. */
  int depth_format_mode;

  bool operator==(const TextureReadRoutineSpecialisation &other) const
  {
    return ((input_data_type == other.input_data_type) &&
            (output_data_type == other.output_data_type) &&
            (component_count_input == other.component_count_input) &&
            (component_count_output == other.component_count_output) &&
            (depth_format_mode == other.depth_format_mode));
  }

  uint64_t hash() const
  {
    blender::DefaultHash<std::string> string_hasher;
    return uint64_t(string_hasher(this->input_data_type + this->output_data_type +
                                  std::to_string((this->component_count_input << 8) +
                                                 this->component_count_output +
                                                 (this->depth_format_mode << 28))));
  }
};

namespace blender::gpu {

class MTLContext;
class MTLVertBuf;
class MTLStorageBuf;
class MTLBuffer;

/* Metal Texture internal implementation. */
static const int MTL_MAX_MIPMAP_COUNT = 15; /* Max: 16384x16384 */
static const int MTL_MAX_FBO_ATTACHED = 16;

/* Samplers */
struct MTLSamplerState {
  GPUSamplerState state;

  /* Mip min and mip max on sampler state always the same.
   * Level range now controlled with textureView to be consistent with GL baseLevel. */
  bool operator==(const MTLSamplerState &other) const
  {
    /* Add other parameters as needed. */
    return (this->state == other.state);
  }

  operator uint() const
  {
    uint integer_representation = 0;
    integer_representation |= this->state.filtering;
    integer_representation |= this->state.extend_x << 8;
    integer_representation |= this->state.extend_yz << 12;
    integer_representation |= this->state.custom_type << 16;
    integer_representation |= this->state.type << 24;
    return integer_representation;
  }

  operator uint64_t() const
  {
    uint64_t integer_representation = 0;
    integer_representation |= this->state.filtering;
    integer_representation |= this->state.extend_x << 8;
    integer_representation |= this->state.extend_yz << 12;
    integer_representation |= this->state.custom_type << 16;
    integer_representation |= this->state.type << 24;
    return integer_representation;
  }
};

const MTLSamplerState DEFAULT_SAMPLER_STATE = {GPUSamplerState::default_sampler() /*, 0, 9999 */};

class MTLTexture : public Texture {
  friend class MTLContext;
  friend class MTLStateManager;
  friend class MTLFrameBuffer;
  friend class MTLStorageBuf;

  /* Special case: The XR blitting function needs access to the Metal blit encoder and handles
   * for directly blitting from an UNORM to an SRGB texture without color space conversion. */
  friend void xr_blit(id<MTLTexture> metal_xr_texture, int ofsx, int ofsy, int width, int height);

 private:
  /* Where the textures data comes from. */
  enum {
    MTL_TEXTURE_MODE_DEFAULT,     /* Texture is self-initialized (Standard). */
    MTL_TEXTURE_MODE_EXTERNAL,    /* Texture source from external id<MTLTexture> handle */
    MTL_TEXTURE_MODE_VBO,         /* Texture source initialized from VBO */
    MTL_TEXTURE_MODE_TEXTURE_VIEW /* Texture is a view into an existing texture. */
  } resource_mode_;

  /* 'baking' refers to the generation of GPU-backed resources. This flag ensures GPU resources are
   * ready. Baking is generally deferred until as late as possible, to ensure all associated
   * resource state has been specified up-front. */
  bool is_baked_ = false;
  MTLTextureDescriptor *texture_descriptor_ = nullptr;
  id<MTLTexture> texture_ = nil;

  /* Texture Storage. */
  size_t aligned_w_ = 0;

  /* Storage buffer view.
   * Buffer backed textures can be wrapped with a storage buffer instance for direct data
   * reading/writing. Required for atomic operations on texture data when texture atomics are
   * unsupported.
   *
   * tex_buffer_metadata_ packs 4 parameters required by the shader to perform texture space
   * remapping: (x, y, z) = (width, height, depth/layers) (w) = aligned width. */
  MTLBuffer *backing_buffer_ = nullptr;
  MTLStorageBuf *storage_buffer_ = nullptr;
  int tex_buffer_metadata_[4];

  /* Blit Frame-buffer. */
  gpu::FrameBuffer *blit_fb_ = nullptr;
  uint blit_fb_slice_ = 0;
  uint blit_fb_mip_ = 0;

  /* Non-SRGB texture view, used for when a framebuffer is bound with SRGB disabled. */
  id<MTLTexture> texture_no_srgb_ = nil;

  /* Texture view properties */
  /* In Metal, we use texture views to either limit mipmap ranges,
   * , apply a swizzle mask, or both.
   *
   * We apply the mip limit in the view rather than in the sampler, as
   * certain effects and functionality such as textureSize rely on the base level
   * being modified.
   *
   * Texture views can also point to external textures, rather than the owned
   * texture if MTL_TEXTURE_MODE_TEXTURE_VIEW is used.
   * If this mode is used, source_texture points to a gpu::Texture from which
   * we pull their texture handle as a root.
   */
  const gpu::Texture *source_texture_ = nullptr;

  enum TextureViewDirtyState {
    TEXTURE_VIEW_NOT_DIRTY = 0,
    TEXTURE_VIEW_SWIZZLE_DIRTY = (1 << 0),
    TEXTURE_VIEW_MIP_DIRTY = (1 << 1)
  };
  id<MTLTexture> mip_swizzle_view_ = nil;
  char tex_swizzle_mask_[4];
  MTLTextureSwizzleChannels mtl_swizzle_mask_;
  bool mip_range_dirty_ = false;

  bool texture_view_stencil_ = false;
  int mip_texture_base_level_ = 0;
  int mip_texture_max_level_ = 1000;
  int mip_texture_base_layer_ = 0;
  int texture_view_dirty_flags_ = TEXTURE_VIEW_NOT_DIRTY;

  /* Max mip-maps for currently allocated texture resource. */
  int mtl_max_mips_ = 1;
  bool has_generated_mips_ = false;

  /* We may modify the requested usage flags so store them separately. */
  eGPUTextureUsage internal_gpu_image_usage_flags_;

  /* VBO. */
  MTLVertBuf *vert_buffer_;
  id<MTLBuffer> vert_buffer_mtl_;

  /* Whether the texture's properties or state has changed (e.g. mipmap range), and re-baking of
   * GPU resource is required. */
  bool is_dirty_;

 public:
  MTLTexture(const char *name);
  MTLTexture(const char *name,
             TextureFormat format,
             GPUTextureType type,
             id<MTLTexture> metal_texture);
  ~MTLTexture() override;

  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  void generate_mipmap() override;
  void copy_to(Texture *dst) override;
  void clear(eGPUDataFormat format, const void *data) override;
  void swizzle_set(const char swizzle_mask[4]) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat type) override;

  bool is_format_srgb();
  bool texture_is_baked();
  const char *get_name()
  {
    return name_;
  }

  bool has_custom_swizzle()
  {
    return (mtl_swizzle_mask_.red != MTLTextureSwizzleRed ||
            mtl_swizzle_mask_.green != MTLTextureSwizzleGreen ||
            mtl_swizzle_mask_.blue != MTLTextureSwizzleBlue ||
            mtl_swizzle_mask_.alpha != MTLTextureSwizzleAlpha);
  }

  id<MTLBuffer> get_vertex_buffer() const
  {
    if (resource_mode_ == MTL_TEXTURE_MODE_VBO) {
      return vert_buffer_mtl_;
    }
    return nil;
  }

  MTLStorageBuf *get_storagebuf();

  const int *get_texture_metadata_ptr() const
  {
    return tex_buffer_metadata_;
  }

  id<MTLTexture> get_metal_handle();
  id<MTLTexture> get_metal_handle_base();
  id<MTLTexture> get_non_srgb_handle();
  MTLSamplerState get_sampler_state();

 protected:
  bool init_internal() override;
  bool init_internal(VertBuf *vbo) override;
  bool init_internal(gpu::Texture *src,
                     int mip_offset,
                     int layer_offset,
                     bool use_stencil) override; /* Texture View */

 private:
  /* Common Constructor, default initialization. */
  void mtl_texture_init();

  /* Post-construction and member initialization, prior to baking.
   * Called during init_internal */
  void prepare_internal();

  /* Generate Metal GPU resources and upload data if needed */
  void ensure_baked();

  /* Delete associated Metal GPU resources. */
  void reset();
  void ensure_mipmaps(int miplvl);

  /* Flags a given mip level as being used. */
  void add_subresource(uint level);

  void read_internal(int mip,
                     int x_off,
                     int y_off,
                     int z_off,
                     int width,
                     int height,
                     int depth,
                     eGPUDataFormat desired_output_format,
                     int num_output_components,
                     size_t debug_data_size,
                     void *r_data);
  void bake_mip_swizzle_view();

  void blit(id<MTLBlitCommandEncoder> blit_encoder,
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
            uint depth);
  void blit(gpu::MTLTexture *dst,
            uint src_x_offset,
            uint src_y_offset,
            uint dst_x_offset,
            uint dst_y_offset,
            uint src_mip,
            uint dst_mip,
            uint dst_slice,
            int width,
            int height);
  gpu::FrameBuffer *get_blit_framebuffer(int dst_slice, uint dst_mip);
  /* Texture Update function Utilities. */
  /* Metal texture updating does not provide the same range of functionality for type conversion
   * and format compatibility as are available in OpenGL. To achieve the same level of
   * functionality, we need to instead use compute kernels to perform texture data conversions
   * where appropriate.
   * There are a number of different inputs which affect permutations and thus require different
   * shaders and PSOs, such as:
   *  - Texture format
   *  - Texture type (e.g. 2D, 3D, 2D Array, Depth etc;)
   *  - Source data format and component count (e.g. floating point)
   *
   * MECHANISM:
   *
   *  blender::map<INPUT DEFINES STRUCT, compute PSO> update_2d_array_kernel_psos;
   * - Generate compute shader with configured kernel below with variable parameters depending
   *   on input/output format configurations. Do not need to keep source or descriptors around,
   *   just PSO, as same input defines will always generate the same code.
   *
   * - IF datatype IS an exact match e.g. :
   *    - Per-component size matches (e.g. GPU_DATA_UBYTE)
   *                                OR GPU_DATA_10_11_11_REV && GPU_R11G11B10 (equiv)
   *                                OR D24S8 and GPU_DATA_UINT_24_8_DEPRECATED
   *    We can use BLIT ENCODER.
   *
   * OTHERWISE TRIGGER COMPUTE:
   *  - Compute sizes will vary. Threads per grid WILL match 'extent'.
   *    Dimensions will vary depending on texture type.
   *  - Will use setBytes with 'TextureUpdateParams' struct to pass in useful member params.
   */
  struct TextureUpdateParams {
    int mip_index;
    int extent[3];          /* Width, Height, Slice on 2D Array tex. */
    int offset[3];          /* Width, Height, Slice on 2D Array tex. */
    uint unpack_row_length; /* Number of pixels between bytes in input data. */
  };

  id<MTLComputePipelineState> texture_update_1d_get_kernel(
      TextureUpdateRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_update_1d_array_get_kernel(
      TextureUpdateRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_update_2d_get_kernel(
      TextureUpdateRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_update_2d_array_get_kernel(
      TextureUpdateRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_update_3d_get_kernel(
      TextureUpdateRoutineSpecialisation specialization);

  id<MTLComputePipelineState> mtl_texture_update_impl(
      TextureUpdateRoutineSpecialisation specialization_params,
      blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
          &specialization_cache,
      GPUTextureType texture_type);

  /* Depth Update Utilities */
  /* Depth texture updates are not directly supported with Blit operations, similarly, we cannot
   * use a compute shader to write to depth, so we must instead render to a depth target.
   * These processes use vertex/fragment shaders to render texture data from an intermediate
   * source, in order to prime the depth buffer. */
  gpu::Shader *depth_2d_update_sh_get(DepthTextureUpdateRoutineSpecialisation specialization);

  void update_sub_depth_2d(
      int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data);

  /* Texture Read function utilities -- Follows a similar mechanism to the updating routines */
  struct TextureReadParams {
    int mip_index;
    int extent[3]; /* Width, Height, Slice on 2D Array tex. */
    int offset[3]; /* Width, Height, Slice on 2D Array tex. */
  };

  id<MTLComputePipelineState> texture_read_1d_get_kernel(
      TextureReadRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_read_1d_array_get_kernel(
      TextureReadRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_read_2d_get_kernel(
      TextureReadRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_read_2d_array_get_kernel(
      TextureReadRoutineSpecialisation specialization);
  id<MTLComputePipelineState> texture_read_3d_get_kernel(
      TextureReadRoutineSpecialisation specialization);

  id<MTLComputePipelineState> mtl_texture_read_impl(
      TextureReadRoutineSpecialisation specialization_params,
      blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
          &specialization_cache,
      GPUTextureType texture_type);

  /* fullscreen blit utilities. */
  gpu::Shader *fullscreen_blit_sh_get();

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLTexture")
};

class MTLPixelBuffer : public PixelBuffer {
 private:
  id<MTLBuffer> buffer_ = nil;

 public:
  MTLPixelBuffer(size_t size);
  ~MTLPixelBuffer();

  void *map() override;
  void unmap() override;
  GPUPixelBufferNativeHandle get_native_handle() override;
  size_t get_size() override;

  id<MTLBuffer> get_metal_buffer();

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLPixelBuffer")
};

/* Utility */
MTLPixelFormat gpu_texture_format_to_metal(TextureFormat tex_format);
size_t get_mtl_format_bytesize(MTLPixelFormat tex_format);
int get_mtl_format_num_components(MTLPixelFormat tex_format);
bool mtl_format_supports_blending(MTLPixelFormat format);

/* The type used to define the per-component data in the input buffer. */
inline std::string tex_data_format_to_msl_type_str(eGPUDataFormat type)
{
  switch (type) {
    case GPU_DATA_FLOAT:
      return "float";
    case GPU_DATA_HALF_FLOAT:
      return "half";
    case GPU_DATA_INT:
      return "int";
    case GPU_DATA_UINT:
      return "uint";
    case GPU_DATA_UBYTE:
      return "uchar";
    case GPU_DATA_UINT_24_8_DEPRECATED:
      return "uint"; /* Problematic type - but will match alignment. */
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV:
      return "float"; /* Problematic type - each component will be read as a float. */
    default:
      BLI_assert(false);
      break;
  }
  return "";
}

/* The type T which goes into texture2d<T, access>. */
inline std::string tex_data_format_to_msl_texture_template_type(eGPUDataFormat type)
{
  switch (type) {
    case GPU_DATA_FLOAT:
      return "float";
    case GPU_DATA_HALF_FLOAT:
      return "half";
    case GPU_DATA_INT:
      return "int";
    case GPU_DATA_UINT:
      return "uint";
    case GPU_DATA_UBYTE:
      return "ushort";
    case GPU_DATA_UINT_24_8_DEPRECATED:
      return "uint"; /* Problematic type. */
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV:
      return "float"; /* Problematic type. */
    default:
      BLI_assert(false);
      break;
  }
  return "";
}

/* Fetch Metal texture type from GPU texture type. */
inline MTLTextureType to_metal_type(GPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
      return MTLTextureType1D;
    case GPU_TEXTURE_2D:
      return MTLTextureType2D;
    case GPU_TEXTURE_3D:
      return MTLTextureType3D;
    case GPU_TEXTURE_CUBE:
      return MTLTextureTypeCube;
    case GPU_TEXTURE_BUFFER:
      return MTLTextureTypeTextureBuffer;
    case GPU_TEXTURE_1D_ARRAY:
      return MTLTextureType1DArray;
    case GPU_TEXTURE_2D_ARRAY:
      return MTLTextureType2DArray;
    case GPU_TEXTURE_CUBE_ARRAY:
      return MTLTextureTypeCubeArray;
    default:
      BLI_assert_unreachable();
  }
  return MTLTextureType2D;
}

/* Determine whether format is writable or not. Use mtl_format_get_writeable_view_format(..) for
 * these. */
inline bool mtl_format_is_writable(MTLPixelFormat format)
{
  switch (format) {
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatBGRA8Unorm_sRGB:
    case MTLPixelFormatDepth16Unorm:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatDepth32Float_Stencil8:
    case MTLPixelFormatBGR10A2Unorm:
    case MTLPixelFormatDepth24Unorm_Stencil8:
      return false;
    default:
      return true;
  }
  return true;
}

/* For the cases where a texture format is unwritable, we can create a texture view of a similar
 * format */
inline MTLPixelFormat mtl_format_get_writeable_view_format(MTLPixelFormat format)
{
  switch (format) {
    case MTLPixelFormatRGBA8Unorm_sRGB:
      return MTLPixelFormatRGBA8Unorm;
    case MTLPixelFormatBGRA8Unorm_sRGB:
      return MTLPixelFormatBGRA8Unorm;
    case MTLPixelFormatDepth16Unorm:
      return MTLPixelFormatR16Unorm;
    case MTLPixelFormatDepth32Float:
      return MTLPixelFormatR32Float;
    case MTLPixelFormatDepth32Float_Stencil8:
      // return MTLPixelFormatRG32Float;
      /* No alternative mirror format. This should not be used for
       * manual data upload */
      return MTLPixelFormatInvalid;
    case MTLPixelFormatBGR10A2Unorm:
      // return MTLPixelFormatBGRA8Unorm;
      /* No alternative mirror format. This should not be used for
       * manual data upload */
      return MTLPixelFormatInvalid;
    case MTLPixelFormatDepth24Unorm_Stencil8:
      /* No direct format, but we'll just mirror the bytes -- `Uint`
       * should ensure bytes are not re-normalized or manipulated */
      // return MTLPixelFormatR32Uint;
      return MTLPixelFormatInvalid;
    default:
      return format;
  }
  return format;
}

inline MTLTextureUsage mtl_usage_from_gpu(eGPUTextureUsage usage)
{
  MTLTextureUsage mtl_usage = MTLTextureUsageUnknown;
  if (usage == GPU_TEXTURE_USAGE_GENERAL) {
    return MTLTextureUsageUnknown;
  }
  /* Host read implies general read support, as the compute-based host read routine requires
   * reading of texture data. */
  if (usage & GPU_TEXTURE_USAGE_SHADER_READ || usage & GPU_TEXTURE_USAGE_HOST_READ) {
    mtl_usage = mtl_usage | MTLTextureUsageShaderRead;
  }
  if (usage & GPU_TEXTURE_USAGE_SHADER_WRITE) {
    mtl_usage = mtl_usage | MTLTextureUsageShaderWrite;
  }
  if (usage & GPU_TEXTURE_USAGE_ATTACHMENT) {
    mtl_usage = mtl_usage | MTLTextureUsageRenderTarget;
  }
  if (usage & GPU_TEXTURE_USAGE_FORMAT_VIEW) {
    mtl_usage = mtl_usage | MTLTextureUsagePixelFormatView;
  }
#if defined(MAC_OS_VERSION_14_0)
  if (@available(macOS 14.0, *)) {
    if (usage & GPU_TEXTURE_USAGE_ATOMIC) {
      mtl_usage = mtl_usage | MTLTextureUsageShaderAtomic;
    }
  }
#endif
  return mtl_usage;
}

inline eGPUTextureUsage gpu_usage_from_mtl(MTLTextureUsage mtl_usage)
{
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
  if (mtl_usage == MTLTextureUsageUnknown) {
    return GPU_TEXTURE_USAGE_GENERAL;
  }
  if (mtl_usage & MTLTextureUsageShaderRead) {
    usage = usage | GPU_TEXTURE_USAGE_SHADER_READ;
  }
  if (mtl_usage & MTLTextureUsageShaderWrite) {
    usage = usage | GPU_TEXTURE_USAGE_SHADER_WRITE;
  }
  if (mtl_usage & MTLTextureUsageRenderTarget) {
    usage = usage | GPU_TEXTURE_USAGE_ATTACHMENT;
  }
  if (mtl_usage & MTLTextureUsagePixelFormatView) {
    usage = usage | GPU_TEXTURE_USAGE_FORMAT_VIEW;
  }
  return usage;
}

}  // namespace blender::gpu

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * A `GPUTexture` is a wrapper around backend specific texture objects.
 * It allows, creation of diverse texture format and types, update, read, reference counting,
 * internal sampler state tracking and texture binding.
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_state.h"

struct GPUVertBuf;

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Enums
 * \{ */

/**
 * A `eGPUSamplerState` specify the sampler state to bind a texture with.
 * One is stored inside `GPUTexture` for default parameters.
 *
 * Some sampler states commonly set:
 * - BORDER_COLOR is set to {0, 0, 0, 0}.
 * - MIN_LOD is set to -1000.
 * - MAX_LOD is set to 1000.
 * - LOD_BIAS is set to 0.0.
 */
/**
 * TODO(fclem): this enum needs to be split into multiple states. One for filtering. One for
 * extension / wrap mode etc...
 */
typedef enum eGPUSamplerState {
  /**
   * Default sampler state with all options off.
   * It means no filtering, no mipmap, clamp to edge texel, no compare.
   */
  GPU_SAMPLER_DEFAULT = 0,
  /**
   * Enables hardware linear filtering.
   * Enables linear interpolation between mips if GPU_SAMPLER_MIPMAP is also set.
   */
  GPU_SAMPLER_FILTER = (1 << 0),
  /**
   * Enables mipmap access through shader samplers.
   * Enables linear interpolation between mips if GPU_SAMPLER_FILTER is also set, otherwise the mip
   * interpolation will be set to nearest.
   */
  GPU_SAMPLER_MIPMAP = (1 << 1),
  /**
   * Sets texture coordinate extension to repeat in X, Y and Z direction.
   * If not set for some direction, either clamp to edge (texel) or border color (0,0,0,0) if
   * `GPU_SAMPLER_CLAMP_BORDER` is set.
   * If `GPU_SAMPLER_MIRROR_REPEAT` is set, any direction using `GPU_SAMPLER_REPEAT_*` will use a
   * mirrored repeat coordinate extension.
   */
  GPU_SAMPLER_REPEAT_S = (1 << 2),
  GPU_SAMPLER_REPEAT_T = (1 << 3),
  GPU_SAMPLER_REPEAT_R = (1 << 4),
  GPU_SAMPLER_REPEAT = (GPU_SAMPLER_REPEAT_S | GPU_SAMPLER_REPEAT_T | GPU_SAMPLER_REPEAT_R),
  /**
   * Clamp to border color instead of border texel.
   * Used for directions not using `GPU_SAMPLER_REPEAT_*`.
   */
  GPU_SAMPLER_CLAMP_BORDER = (1 << 5),
  /**
   * Enable compare mode for depth texture. The depth texture must then be bound to a shadow
   * sampler.
   */
  GPU_SAMPLER_COMPARE = (1 << 6),
  /** Enable Anisotropic filtering. This only has effect if `GPU_SAMPLER_MIPMAP` is set.
   * The filtered result is implementation dependent.
   * The maximum amount of samples is set
   */
  GPU_SAMPLER_ANISO = (1 << 7),
  /** Enable mirror repeat extension mode for directions using the `GPU_SAMPLER_REPEAT_*` flag. */
  GPU_SAMPLER_MIRROR_REPEAT = (1 << 8),

  /** Special icon sampler with custom lod bias and interpolation mode. */
  GPU_SAMPLER_ICON = (1 << 9),
} eGPUSamplerState;

/**
 * #GPU_SAMPLER_MAX is not a valid enum value, but only a limit.
 * It also creates a bad mask for the `NOT` operator in #ENUM_OPERATORS.
 */
#ifdef __cplusplus
static constexpr eGPUSamplerState GPU_SAMPLER_MAX = eGPUSamplerState(GPU_SAMPLER_ICON + 1);
#else
static const int GPU_SAMPLER_MAX = (GPU_SAMPLER_ICON + 1);
#endif

ENUM_OPERATORS(eGPUSamplerState, GPU_SAMPLER_ICON)

/**
 * Types of texture internal storage. Defines how the data is stored inside the video memory.
 * Be aware that some formats are not supported by render-buffers.
 */
/**
 * TODO(fclem): Enable and implement all of them. Encouraging new usage and avoiding missing cases
 * in switch statements.
 */
typedef enum eGPUTextureFormat {
  /* Formats texture & render-buffer. */
  GPU_RGBA8UI,
  GPU_RGBA8I,
  GPU_RGBA8,
  GPU_RGBA32UI,
  GPU_RGBA32I,
  GPU_RGBA32F,
  GPU_RGBA16UI,
  GPU_RGBA16I,
  GPU_RGBA16F,
  GPU_RGBA16,
  GPU_RG8UI,
  GPU_RG8I,
  GPU_RG8,
  GPU_RG32UI,
  GPU_RG32I,
  GPU_RG32F,
  GPU_RG16UI,
  GPU_RG16I,
  GPU_RG16F,
  GPU_RG16,
  GPU_R8UI,
  GPU_R8I,
  GPU_R8,
  GPU_R32UI,
  GPU_R32I,
  GPU_R32F,
  GPU_R16UI,
  GPU_R16I,
  GPU_R16F,
  GPU_R16, /* Max texture buffer format. */

  /* Special formats texture & render-buffer. */
  GPU_RGB10_A2,
  GPU_R11F_G11F_B10F,
  GPU_DEPTH32F_STENCIL8,
  GPU_DEPTH24_STENCIL8,
  GPU_SRGB8_A8,
#if 0
  GPU_RGB10_A2UI,
#endif

  /* Texture only format */
  GPU_RGB16F,
#if 0
  GPU_RGBA16_SNORM,
  GPU_RGBA8_SNORM,
  GPU_RGB32F,
  GPU_RGB32I,
  GPU_RGB32UI,
  GPU_RGB16_SNORM,
  GPU_RGB16I,
  GPU_RGB16UI,
  GPU_RGB16,
  GPU_RGB8_SNORM,
  GPU_RGB8,
  GPU_RGB8I,
  GPU_RGB8UI,
  GPU_RG16_SNORM,
  GPU_RG8_SNORM,
  GPU_R16_SNORM,
  GPU_R8_SNORM,
#endif

  /* Special formats texture only */
  GPU_SRGB8_A8_DXT1,
  GPU_SRGB8_A8_DXT3,
  GPU_SRGB8_A8_DXT5,
  GPU_RGBA8_DXT1,
  GPU_RGBA8_DXT3,
  GPU_RGBA8_DXT5,
#if 0
  GPU_SRGB8,
  GPU_RGB9_E5,
  GPU_COMPRESSED_RG_RGTC2,
  GPU_COMPRESSED_SIGNED_RG_RGTC2,
  GPU_COMPRESSED_RED_RGTC1,
  GPU_COMPRESSED_SIGNED_RED_RGTC1,
#endif

  /* Depth Formats */
  GPU_DEPTH_COMPONENT32F,
  GPU_DEPTH_COMPONENT24,
  GPU_DEPTH_COMPONENT16,
} eGPUTextureFormat;

/**
 * Types of data for data specification.
 * Used for formatting upload and download of data.
 * When used with textures, they need to match or be compatible with the `eGPUTextureFormat` used.
 * Check `validate_data_format` and `validate_data_format_mtl` for compatibility list.
 */
typedef enum eGPUDataFormat {
  GPU_DATA_FLOAT,
  GPU_DATA_HALF_FLOAT,
  GPU_DATA_INT,
  GPU_DATA_UINT,
  GPU_DATA_UBYTE,
  /** Special type used for depth-stencil textures. */
  GPU_DATA_UINT_24_8,
  /** Special type used for packed 32bit per pixel textures. Data is stored in reverse order. */
  GPU_DATA_10_11_11_REV,
  GPU_DATA_2_10_10_10_REV,
} eGPUDataFormat;

/**
 * Texture usage flags allow backend implementations to contextually optimize texture resources.
 * Any texture with an explicit flag should not perform operations which are not explicitly
 * specified in the usage flags. If usage is unknown upfront, then GPU_TEXTURE_USAGE_GENERAL can be
 * used.
 *
 * NOTE: These usage flags act as hints for the backend implementations. There may be no benefit in
 * some circumstances, and certain resource types may insert additional usage as required. However,
 * explicit usage can ensure that hardware features such as render target/texture compression can
 * be used. For explicit APIs such as Metal/Vulkan, texture usage needs to be specified up-front.
 */
typedef enum eGPUTextureUsage {
  /* Whether texture is sampled or read during a shader. */
  GPU_TEXTURE_USAGE_SHADER_READ = (1 << 0),
  /* Whether the texture is written to by a shader using imageStore. */
  GPU_TEXTURE_USAGE_SHADER_WRITE = (1 << 1),
  /* Whether a texture is used as an attachment in a frame-buffer. */
  GPU_TEXTURE_USAGE_ATTACHMENT = (1 << 2),
  /* Whether the texture is used as a texture view, uses mip-map layer adjustment,
   * OR, uses swizzle access masks. Mip-map base layer adjustment and texture channel swizzling
   * requires a texture view under-the-hood. */
  GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW = (1 << 3),
  /* Whether a texture can be allocated without any backing memory. It is used as an
   * attachment to store data, but is not needed by any future passes.
   * This usage mode should be used in scenarios where an attachment has no previous
   * contents and is not stored after a render pass. */
  GPU_TEXTURE_USAGE_MEMORYLESS = (1 << 4),
  /* Whether the texture needs to be read from by the CPU. */
  GPU_TEXTURE_USAGE_HOST_READ = (1 << 5),
  /* Create a texture whose usage cannot be defined prematurely.
   * This is unoptimized and should not be used. */
  GPU_TEXTURE_USAGE_GENERAL = 0xFF,
} eGPUTextureUsage;

ENUM_OPERATORS(eGPUTextureUsage, GPU_TEXTURE_USAGE_GENERAL);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

/** Opaque type hiding blender::gpu::Texture. */
typedef struct GPUTexture GPUTexture;

/**
 * \note \a data is expected to be float. If the \a format is not compatible with float data or if
 * the data is not in float format, use GPU_texture_update to upload the data with the right data
 * format.
 * NOTE: `_ex` variants of texture creation functions allow specification of explicit usage for
 * optimal performance. Using standard texture creation will use the `GPU_TEXTURE_USAGE_GENERAL`.
 *
 * Textures created via other means will either inherit usage from the source resource, or also
 * be initialized with `GPU_TEXTURE_USAGE_GENERAL`.
 *
 * flag. \a mips is the number of mip level to allocate. It must be >= 1.
 */
/* Extended texture functions. */
GPUTexture *GPU_texture_create_1d_ex(const char *name,
                                     int width,
                                     int mip_len,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage,
                                     const float *data);
GPUTexture *GPU_texture_create_1d_array_ex(const char *name,
                                           int width,
                                           int layer_len,
                                           int mip_len,
                                           eGPUTextureFormat format,
                                           eGPUTextureUsage usage,
                                           const float *data);
GPUTexture *GPU_texture_create_2d_ex(const char *name,
                                     int width,
                                     int height,
                                     int mip_len,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage,
                                     const float *data);
GPUTexture *GPU_texture_create_2d_array_ex(const char *name,
                                           int width,
                                           int height,
                                           int layer_len,
                                           int mip_len,
                                           eGPUTextureFormat format,
                                           eGPUTextureUsage usage,
                                           const float *data);
GPUTexture *GPU_texture_create_3d_ex(const char *name,
                                     int width,
                                     int height,
                                     int depth,
                                     int mip_len,
                                     eGPUTextureFormat format,
                                     eGPUDataFormat data_format,
                                     eGPUTextureUsage usage,
                                     const void *data);
GPUTexture *GPU_texture_create_cube_ex(const char *name,
                                       int width,
                                       int mip_len,
                                       eGPUTextureFormat format,
                                       eGPUTextureUsage usage,
                                       const float *data);
GPUTexture *GPU_texture_create_cube_array_ex(const char *name,
                                             int width,
                                             int layer_len,
                                             int mip_len,
                                             eGPUTextureFormat format,
                                             eGPUTextureUsage usage,
                                             const float *data);
/**
 * DDS texture loading. Return NULL if support is not available.
 * \a data should hold all the data for \a mip_len mipmaps.
 */
GPUTexture *GPU_texture_create_compressed_2d_ex(const char *name,
                                                int width,
                                                int height,
                                                int mip_len,
                                                eGPUTextureFormat format,
                                                eGPUTextureUsage usage,
                                                const void *data);

/* Standard texture functions. */
GPUTexture *GPU_texture_create_1d(
    const char *name, int width, int mip_len, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_1d_array(const char *name,
                                        int width,
                                        int layer_len,
                                        int mip_len,
                                        eGPUTextureFormat format,
                                        const float *data);
GPUTexture *GPU_texture_create_2d(const char *name,
                                  int width,
                                  int height,
                                  int mip_len,
                                  eGPUTextureFormat format,
                                  const float *data);
GPUTexture *GPU_texture_create_2d_array(const char *name,
                                        int width,
                                        int height,
                                        int layer_len,
                                        int mip_len,
                                        eGPUTextureFormat format,
                                        const float *data);
GPUTexture *GPU_texture_create_3d(const char *name,
                                  int width,
                                  int height,
                                  int depth,
                                  int mip_len,
                                  eGPUTextureFormat format,
                                  eGPUDataFormat data_format,
                                  const void *data);
GPUTexture *GPU_texture_create_cube(
    const char *name, int width, int mip_len, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_cube_array(const char *name,
                                          int width,
                                          int layer_len,
                                          int mip_len,
                                          eGPUTextureFormat format,
                                          const float *data);
/**
 * DDS texture loading. Return NULL if compressed texture support is not available.
 * \a data should hold all the data for \a mip_len mipmaps.
 * The data is expected to be in compressed form. This isn't going to compress un-compress data.
 */
GPUTexture *GPU_texture_create_compressed_2d(const char *name,
                                             int width,
                                             int height,
                                             int mip_len,
                                             eGPUTextureFormat format,
                                             const void *data);

/**
 * Create a buffer texture that allow access to a buffer \a vertex_buf through a sampler of type
 * `(FLOAT/INT/UINT)_BUFFER`.
 */
GPUTexture *GPU_texture_create_from_vertbuf(const char *name, struct GPUVertBuf *vertex_buf);

/**
 * Create an error texture that will bind an pink texture at draw time.
 * \a dimension is the number of number of dimension of the texture (1, 2, or 3).
 * \a array if set to true, will make the texture be an array (layered).
 */
GPUTexture *GPU_texture_create_error(int dimension, bool array);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Freeing
 * \{ */

/**
 * Add a reference to this texture for usage.
 * This internally increment the reference counter.
 * This avoids the texture being free between the time it is referenced by the drawing logic and
 * the time it is actually dereferenced.
 */
void GPU_texture_ref(GPUTexture *texture);

/**
 * This internally decrement the reference counter.
 * If the reference counter is 1 when calling this function the #GPUTexture will be freed.
 */
void GPU_texture_free(GPUTexture *texture);

#define GPU_TEXTURE_FREE_SAFE(texture) \
  do { \
    if (texture != NULL) { \
      GPU_texture_free(texture); \
      texture = NULL; \
    } \
  } while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Views
 * \{ */

/**
 * Create an alias of the source texture data. A view can cover the whole texture or only a range
 * of mip levels and/or array layer range.
 *
 * \a view_format is the format in which the view will interpret the data of \a source_texture . It
 * must match the format of \a source_texture in size (ex: RGBA8 can be reinterpreted as R32UI).
 * See https://www.khronos.org/opengl/wiki/Texture_Storage#View_texture_aliases for an exhaustive
 * list.
 *
 * \note If \a source_texture is freed, the texture view will continue to be valid.
 * \note If \a mip_start or \a mip_len is bigger than available mips they will be clamped to the
 * source texture available range.
 * \note If \a cube_as_array is true, then the created view will be a 2D array texture instead of a
 * cube-map texture or cube-map-array texture.
 *
 * TODO(fclem): Target conversion (ex: Texture 2D as Texture 2D Array) is not implemented yet.
 */
GPUTexture *GPU_texture_create_view(const char *name,
                                    const GPUTexture *source_texture,
                                    eGPUTextureFormat view_format,
                                    int mip_start,
                                    int mip_len,
                                    int layer_start,
                                    int layer_len,
                                    bool cube_as_array);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modify & Update
 * \{ */

/**
 * Makes data interpretation aware of the source layout.
 * Skipping pixels correctly when changing rows when doing partial update.
 * This affects `GPU_texture_update`, `GPU_texture_update_sub`, `GPU_texture_update_mipmap`.
 * TODO(fclem): replace this by pixel buffer updates using a custom utility to do the line shifting
 * like Cycles does.
 */
void GPU_unpack_row_length_set(uint len);

/**
 * Update the content of a texture's base mip-map level (mip 0).
 * \a data_format is the format of the \a data . It needs to be compatible with the internal
 * texture storage.
 * The \a data should be be the size of the entire mip 0 level.
 * \note This function only update the content of mip 0. Either specify other mips or use
 * `GPU_texture_generate_mipmap` to generate them if needed.
 */
void GPU_texture_update(GPUTexture *texture, eGPUDataFormat data_format, const void *data);

/**
 * Update the content of a region of a texture's base mip-map level (mip 0).
 * \a data_format is the format of the \a data . It needs to be compatible with the internal
 * texture storage.
 * The \a data should be be the size of the mip 0 level region.
 * \note This function only update the content of mip 0. Either specify other mips or use
 * `GPU_texture_generate_mipmap` to generate them if needed.
 *
 * \a offset_x , \a offset_y , \a offset_z specify the bottom left corner of the updated region.
 * \a width , \a height , \a depth specify the extent of the updated region.
 */
void GPU_texture_update_sub(GPUTexture *texture,
                            eGPUDataFormat data_format,
                            const void *pixels,
                            int offset_x,
                            int offset_y,
                            int offset_z,
                            int width,
                            int height,
                            int depth);

/**
 * Update the content of a texture's specific mip-map level.
 * \a data_format is the format of the \a pixels . It needs to be compatible with the internal
 * texture storage.
 * The \a data should be be the size of the entire \a mip_level .
 */
void GPU_texture_update_mipmap(GPUTexture *texture,
                               int mip_level,
                               eGPUDataFormat data_format,
                               const void *pixels);

/**
 * Fills the whole texture with the same data for all pixels.
 * \warning Only work for 2D texture for now.
 * \warning Only clears the MIP 0 of the texture.
 * \param data_format: data format of the pixel data.
 * \note The format is float for UNORM textures.
 * \param data: 1 pixel worth of data to fill the texture with.
 */
void GPU_texture_clear(GPUTexture *texture, eGPUDataFormat data_format, const void *data);

/**
 * Copy a \a src texture content to a similar \a dst texture. Only MIP 0 is copied.
 * Textures needs to match in size and format.
 */
void GPU_texture_copy(GPUTexture *dst, GPUTexture *src);

/**
 * Update the mip-map levels using the mip 0 data.
 * \note this doesn't work on depth or compressed textures.
 */
void GPU_texture_generate_mipmap(GPUTexture *texture);

/**
 * Read the content of a \a mip_level from a \a tex and returns a copy of its data.
 * \warning the texture must have been created using GPU_TEXTURE_USAGE_HOST_READ.
 * \note synchronisation of shader writes via `imageStore()` needs to be explicitly done using
 * `GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH)`.
 */
void *GPU_texture_read(GPUTexture *texture, eGPUDataFormat data_format, int mip_level);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

/**
 * Bind a texture to a texture sampling image units using the texture internal sampler state.
 */
void GPU_texture_bind(GPUTexture *texture, int unit);
/**
 * Bind a texture to a texture sampling image units using the explicit sampler state.
 */
void GPU_texture_bind_ex(GPUTexture *texture, eGPUSamplerState state, int unit, bool set_number);
/**
 * Unbind \a tex from a texture sampling image unit.
 * \note this isn't strictly required but it is better for debugging purpose.
 */
void GPU_texture_unbind(GPUTexture *texture);
/**
 * Unbind all texture from all texture sampling image units.
 */
void GPU_texture_unbind_all(void);

/**
 * Bind \a tex to an arbitrary load/store image unit.
 * It correspond to a `gpu::shader::ShaderCreateInfo::image()` declaration.
 * \note this overrides any previous bind on the same unit.
 */
void GPU_texture_image_bind(GPUTexture *texture, int unit);
/**
 * Unbind \a tex from an arbitrary load/store image unit.
 * \note this isn't strictly required but it is better for debugging purpose.
 */
void GPU_texture_image_unbind(GPUTexture *texture);
/**
 * Unbind all texture from all arbitrary load/store image units.
 */
void GPU_texture_image_unbind_all(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name State API
 * \{ */

/**
 * Set anisotropic filter usage. Filter sample count is determined globally by
 * `U.anisotropic_filter` and updated when `GPU_samplers_update` is called.
 */
void GPU_texture_anisotropic_filter(GPUTexture *texture, bool use_aniso);

/**
 * Set \a tex texture depth comparison mode. Only works on depth format.
 */
void GPU_texture_compare_mode(GPUTexture *texture, bool use_compare);

/**
 * Set \a tex texture filter usage.
 * If \a use_filter is true, the texture will use linear interpolation between neighboring texels.
 * \note Does not work on non-normalized integer textures.
 * \note Does not modify the mip-map usage state.
 */
void GPU_texture_filter_mode(GPUTexture *texture, bool use_filter);

/**
 * Set \a tex texture filter and mip-map usage.
 * If \a use_filter is true, the texture will use linear interpolation between neighboring texels.
 * If \a use_mipmap is true, the texture will use mip-mapping as anti-aliasing method.
 * If both are set to true, the texture will use linear interpolation between mip-map levels.
 * \note Does not work on non-normalized integer textures.
 */
void GPU_texture_mipmap_mode(GPUTexture *texture, bool use_mipmap, bool use_filter);

/**
 * Set \a tex texture sampling method for coordinates outside of the [0..1] uv range.
 *
 * If \a use_repeat is true, sampling the texture outside of the [0..1] uv range will repeat to
 * border color instead of the border texel value.
 *
 * If \a use_clamp is true, sampling the texture outside of the [0..1] uv range will clamp to the
 * closest border texel value. If set to false, it will use the values (0, 0, 0, 0) instead.
 */
void GPU_texture_wrap_mode(GPUTexture *texture, bool use_repeat, bool use_clamp);

/**
 * Set \a tex texture swizzle state for swizzling sample components.
 *
 * A texture sample always return 4 components in the shader. If the texture has less than 4
 * components, the missing ones are replaced by the matching values in the following vector
 * (0, 0, 0, 1).
 *
 * \a swizzle contains 1 char per component representing the source of the data for each of the
 * component of a sample value. The possible values for each of these 4 characters are:
 * - 'r' or 'x': use the texture first component.
 * - 'g' or 'y': use the texture second component.
 * - 'b' or 'z': use the texture third component.
 * - 'a' or 'w': use the texture fourth component.
 * - '0': will make the component value to always return 0.
 * - '1': will make the component value to always return 1.
 */
void GPU_texture_swizzle_set(GPUTexture *texture, const char swizzle[4]);

/**
 * Set a depth-stencil texture read mode.
 *
 * If \a use_stencil is true, the texture is expected to be bound to a UINT sampler and will return
 * the stencil value (in a range of [0..255]) as the first component.
 * If \a use_stencil is false, the texture is expected to be bound to a DEPTH sampler and will
 * return the normalized depth value (in a range of [0..1])  as the first component.
 */
void GPU_texture_stencil_texture_mode_set(GPUTexture *texture, bool use_stencil);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Introspection API
 * \{ */

/**
 * Return the number of dimensions of the texture ignoring dimension of layers (1, 2 or 3).
 * Cube textures are considered 2D.
 */
int GPU_texture_dimensions(const GPUTexture *texture);

/**
 * Return the width of \a tex .
 */
int GPU_texture_width(const GPUTexture *texture);

/**
 * Return the height of \a tex . Correspond to number of layers for 1D array texture.
 */
int GPU_texture_height(const GPUTexture *texture);

/**
 * Return the number of layers of \a tex . Return 1 if the texture is not layered.
 */
int GPU_texture_layer_count(const GPUTexture *texture);

/**
 * Return the number of mip-map level inside this texture.
 */
int GPU_texture_mip_count(const GPUTexture *texture);

/**
 * Return the texture format of \a tex .
 */
eGPUTextureFormat GPU_texture_format(const GPUTexture *texture);

/**
 * Return the usage flags of \a tex .
 */
eGPUTextureUsage GPU_texture_usage(const GPUTexture *texture);

/**
 * Return true if the texture is an array texture type (has layers).
 */
bool GPU_texture_array(const GPUTexture *texture);

/**
 * Return true if the texture is an cube-map texture type.
 */
bool GPU_texture_cube(const GPUTexture *texture);

/**
 * Return true if the texture format has a depth component.
 */
bool GPU_texture_depth(const GPUTexture *texture);

/**
 * Return true if the texture format has a stencil component.
 */
bool GPU_texture_stencil(const GPUTexture *texture);

/**
 * Return true if the texture format is an integer type (non-normalized integers).
 */
bool GPU_texture_integer(const GPUTexture *texture);

/**
 * Returns the pixel dimensions of a texture's mip-map level.
 * \a size is expected to be a pointer to a vector of dimension matching the texture's dimension
 * (including the array dimension).
 */
void GPU_texture_get_mipmap_size(GPUTexture *texture, int mip_level, int *size);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Python API & meta-data
 *
 * These are not intrinsic properties of a texture but they are stored inside the gpu::Texture
 * structure for tracking purpose.
 * \{ */

/**
 * Width & Height (of source data), optional.
 * WORKAROUND: Calling 'BKE_image_get_size' may free the texture. Store the source image size
 * (before down-scaling) inside the #GPUTexture to retrieve the original size later (Ref #59347).
 */
int GPU_texture_orig_width(const GPUTexture *texture);
int GPU_texture_orig_height(const GPUTexture *texture);
void GPU_texture_orig_size_set(GPUTexture *texture, int width, int height);

/**
 * Reference of a pointer that needs to be cleaned when deallocating the texture.
 * Points to #BPyGPUTexture.tex
 */
#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_texture_py_reference_get(GPUTexture *texture);
void GPU_texture_py_reference_set(GPUTexture *texture, void **py_ref);
#endif

/**
 * Return the backend handle of the texture.
 * \note This is a legacy feature only working on OpenGL backend. It will be removed once we remove
 * the python BGL module.
 */
int GPU_texture_opengl_bindcode(const GPUTexture *texture);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Returns the number of components in a texture format.
 */
size_t GPU_texture_component_len(eGPUTextureFormat format);

/**
 * Return the expected number of bytes for one pixel of \a data_format data.
 */
size_t GPU_texture_dataformat_size(eGPUDataFormat data_format);

/**
 * Return the texture format as a string for display purpose.
 * Example: `GPU_RGBA8` returns as `"RGBA8"`.
 */
const char *GPU_texture_format_description(eGPUTextureFormat format);

/**
 * Returns the memory usage of all currently allocated textures in bytes.
 * \note that does not mean all of the textures are inside VRAM. Drivers can swap the texture
 * memory back and forth depending on usage.
 */
unsigned int GPU_texture_memory_usage_get(void);

/**
 * Update sampler states depending on user settings.
 */
void GPU_samplers_update(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pixel Buffer
 *
 * Used for interfacing with other graphic APIs using graphic interoperability.
 * It can also be used for more efficient partial update from CPU side.
 * \{ */

/** Opaque type hiding blender::gpu::PixelBuffer. */
typedef struct GPUPixelBuffer GPUPixelBuffer;

/**
 * Creates a #GPUPixelBuffer object with \a byte_size worth of storage.
 */
GPUPixelBuffer *GPU_pixel_buffer_create(uint byte_size);

/**
 * Free a #GPUPixelBuffer object.
 * The object should be unmapped before being freed.
 */
void GPU_pixel_buffer_free(GPUPixelBuffer *pixel_buf);

/**
 * Maps a pixel buffer to RAM, giving back access rights to CPU.
 * The returned pointer is only valid until `GPU_pixel_buffer_unmap` is called.
 * A #GPUPixelBuffer needs to be unmapped before being used for GPU side operation (like texture
 * update through `GPU_texture_update_sub_from_pixel_buffer`).
 */
void *GPU_pixel_buffer_map(GPUPixelBuffer *pixel_buf);

/**
 * Unmap a pixel buffer from RAM, giving back access rights to GPU.
 * Any pointer previously acquired by `GPU_pixel_buffer_map` becomes invalid.
 */
void GPU_pixel_buffer_unmap(GPUPixelBuffer *pixel_buf);

/**
 * Return size in bytes of the \a pix_buf .
 */
uint GPU_pixel_buffer_size(GPUPixelBuffer *pixel_buf);

/**
 * Return the native handle of the \a pix_buf to use for graphic interoperability registration.
 */
int64_t GPU_pixel_buffer_get_native_handle(GPUPixelBuffer *pixel_buf);

/**
 * Update a sub-region of a texture using the data from a #GPUPixelBuffer as source data.
 * The \a pix_buf data is expected to be contiguous and big enough to fill the described
 * sub-region.
 */
void GPU_texture_update_sub_from_pixel_buffer(GPUTexture *texture,
                                              eGPUDataFormat data_format,
                                              GPUPixelBuffer *pixel_buf,
                                              int offset_x,
                                              int offset_y,
                                              int offset_z,
                                              int width,
                                              int height,
                                              int depth);
/** \} */

#ifdef __cplusplus
}
#endif

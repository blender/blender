/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_index_range.hh"

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_debug.hh"
#include "GPU_shader.hh"
#include "GPU_shader_builtin.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "gpu_capabilities_private.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_texture_private.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.mipmap"};

namespace blender {

namespace gpu {

static Shader *get_update_mipmap_shader(TextureFormat texture_format, bool is_layered)
{
  if (is_layered) {
    switch (texture_format) {
      case TextureFormat::UNORM_8:
        return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_UNORM_8_LAYERED);
      case TextureFormat::UNORM_8_8_8_8:
        return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_UNORM_8_8_8_8_LAYERED);
      case TextureFormat::SFLOAT_16:
        return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_16_LAYERED);
      case TextureFormat::SFLOAT_16_16_16_16:
        return GPU_shader_get_builtin_shader(
            GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_16_16_16_16_LAYERED);
      case TextureFormat::SFLOAT_32:
        return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32_LAYERED);
      case TextureFormat::SFLOAT_32_32_32_32:
        return GPU_shader_get_builtin_shader(
            GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32_32_32_32_LAYERED);
      case TextureFormat::SRGBA_8_8_8_8:
        return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SRGBA_8_8_8_8_LAYERED);
      default:
        break;
    }
    return nullptr;
  }

  switch (texture_format) {
    case TextureFormat::UNORM_8:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_UNORM_8);
    case TextureFormat::UNORM_8_8_8_8:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_UNORM_8_8_8_8);
    case TextureFormat::SFLOAT_16:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_16);
    case TextureFormat::SFLOAT_16_16_16_16:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_16_16_16_16);
    case TextureFormat::SFLOAT_32:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32);
    case TextureFormat::SFLOAT_32_32_32_32:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32_32_32_32);
    case TextureFormat::SRGBA_8_8_8_8:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SRGBA_8_8_8_8);
    default:
      break;
  }
  return nullptr;
}

constexpr int max_levels_per_dispatch = 2;

/**
 * Compute the number of work groups required to dispatch a single pass of the mipmap update
 * shader.
 *
 * \param mip_start: The first mipmap level that is processed by the dispatch.
 */
static uint mipmap_dispatch_group_len(Texture &texture, int mip_start)
{
  const int num_mipmaps = texture.mip_count();
  if (mip_start >= num_mipmaps - 1) {
    return 0;
  }

  int num_levels = min_ii(num_mipmaps - mip_start - 1, max_levels_per_dispatch);
  int3 mip_size(1, 1, 1);
  texture.mip_size_get(mip_start + num_levels, mip_size);

  if (num_levels == 1u) {
    /* Each thread writes one sample. */
    constexpr uint32_t warps = 4;
    const uint32_t samples = mip_size.x * mip_size.y;
    const uint32_t threads = warps * 32U;
    return divide_ceil_u(samples, threads);
  }
  else {
    /* Each workgroup handles a tile. */
    constexpr uint32_t TileWidth = 8;
    constexpr uint32_t TileHeight = 8;
    const uint32_t horizontalTiles = divide_ceil_u(mip_size.x, TileWidth);
    const uint32_t verticalTiles = divide_ceil_u(mip_size.y, TileHeight);
    return horizontalTiles * verticalTiles;
  }
}

static void update_mipmaps_layer(Texture &texture,
                                 Shader &shader,
                                 const int layer,
                                 const TextureFormat view_format)
{
  const int num_mipmaps = texture.mip_count();
  Vector<Texture *, 16> views;
  for (int mipmap : IndexRange(num_mipmaps)) {
    views.append(GPU_texture_create_view(
        __func__, &texture, view_format, mipmap, 1, layer, 1, false, false));
  }

  for (int mip_start = 0; mip_start < num_mipmaps - 1; mip_start += max_levels_per_dispatch) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    GPU_texture_image_bind(views[mip_start], 0);
    for (int mip_offset = 1; mip_offset <= max_levels_per_dispatch; mip_offset++) {
      GPU_texture_image_bind(views[min_ii(mip_start + mip_offset, views.size() - 1)], mip_offset);
    }
    int num_levels = min_ii(views.size() - mip_start - 1, max_levels_per_dispatch);
    GPU_shader_uniform_1i(&shader, "num_levels", num_levels);

    /* The number of work groups in a single dispatch is bounded by `GPU_max_work_group_count()`.
     * Split the work over multiple dispatches when it doesn't fit. */
    const uint group_len = mipmap_dispatch_group_len(texture, mip_start);
    const int max_group_count = max_ii(GPU_max_work_group_count(0), 1);
    for (uint group_offset = 0; group_offset < group_len; group_offset += max_group_count) {
      GPU_shader_uniform_1i(&shader, "group_offset", int(group_offset));
      GPU_compute_dispatch(&shader, min_uu(group_len - group_offset, max_group_count), 1, 1);
    }
  }

  for (Texture *view : views) {
    GPU_texture_free(view);
  }
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

static void update_mipmaps(Texture &texture, Shader &shader, const TextureFormat write_format)
{
  Context &context = *Context::get();
  Shader *prev_shader = context.shader;

  Texture *texture_ptr = &texture;
  int layer_count = texture.layer_count();
  int mip_count = texture.mip_count();
  const bool is_layered = texture.type_get() & GPU_TEXTURE_ARRAY;

  /* When the texture format can not be written to directly, use a temporary texture. */
  const bool use_temp_texture = write_format != texture.format_get();
  if (use_temp_texture) {
    if (is_layered) {
      texture_ptr = GPU_texture_create_2d_array(__func__,
                                                texture.width_get(),
                                                texture.height_get(),
                                                layer_count,
                                                texture.mip_count(),
                                                TextureFormat::UNORM_8_8_8_8,
                                                GPU_TEXTURE_USAGE_SHADER_READ |
                                                    GPU_TEXTURE_USAGE_SHADER_WRITE,
                                                nullptr);
    }
    else {
      texture_ptr = GPU_texture_create_2d(__func__,
                                          texture.width_get(),
                                          texture.height_get(),
                                          texture.mip_count(),
                                          TextureFormat::UNORM_8_8_8_8,
                                          GPU_TEXTURE_USAGE_SHADER_READ |
                                              GPU_TEXTURE_USAGE_SHADER_WRITE,
                                          nullptr);
    }
    texture.copy_to(texture_ptr, IndexRange(1));
  }

  GPU_shader_bind(&shader);
  for (int layer : IndexRange(layer_count)) {
    update_mipmaps_layer(*texture_ptr, shader, layer, write_format);
  }

  if (use_temp_texture) {
    /* Copy result (mip1 and higher) to original texture and free temporary resources. */
    texture_ptr->copy_to(&texture, IndexRange::from_begin_end(1, mip_count));
    GPU_texture_free(texture_ptr);
    texture_ptr = nullptr;
  }

  /* Clear all bound images.
   *
   * Current OpenGL API doesn't have a way to rebind the previous state as it only keeps track of
   * handles. Using a temporary state manager doesn't fit with Metal as the state is stored in
   * multiple places.
   *
   * To not over complicate the implementation for something that is not likely to happen it was
   * decided to unbind all images. When artifacts happen the calling code must be fixed. */
  context.state_manager->image_unbind_all();

  /* Reset original state. */
  if (prev_shader) {
    GPU_shader_bind(prev_shader);
  }
}

}  // namespace gpu

using namespace blender::gpu;

eGPUTextureUsage GPU_texture_mipmap_usage(TextureFormat /*format*/)
{
  /* Mipmaps generated with the compute shader need write support. */
  return GPU_TEXTURE_USAGE_SHADER_WRITE;
}

void GPU_texture_update_mipmap_chain(Texture *tex)
{
  BLI_assert(tex);

  const int num_mipmaps = tex->mip_count();
  /* Early exit - nothing to generate as texture only contains 1 mipmap level. */
  if (num_mipmaps == 1) {
    return;
  }

  bool use_compute_shaders = true;

  if ((tex->usage_get() & GPU_TEXTURE_USAGE_SHADER_WRITE) == 0) {
    CLOG_TRACE(&LOG,
               "Texture doesn't have `GPU_TEXTURE_USAGE_SHADER_WRITE` set. Fallback to backend "
               "implementation");
    use_compute_shaders = false;
  }

  if (use_compute_shaders) {
    const TextureFormat texture_format = tex->format_get();
    const bool is_layered = tex->type_get() & GPU_TEXTURE_ARRAY;
    /* For sRGB without direct write support, write into a temporary UNORM_8_8_8_8 texture. */
    const TextureFormat write_format = ((tex->format_flag_get() & GPU_FORMAT_SRGB) &&
                                        !GCaps.srgb_write_direct_support) ?
                                           TextureFormat::UNORM_8_8_8_8 :
                                           texture_format;
    /* For sRGB with direct write support, the shader is that same as UNORM_8_8_8_8 and
     * any conversion to/from sRGB happens automatically. */
    const TextureFormat shader_format = ((tex->format_flag_get() & GPU_FORMAT_SRGB) &&
                                         GCaps.srgb_write_direct_support) ?
                                            TextureFormat::UNORM_8_8_8_8 :
                                            texture_format;
    Shader *shader = get_update_mipmap_shader(shader_format, is_layered);
    if (shader) {
      GPU_debug_group_begin("Update Mipmaps");
      update_mipmaps(*tex, *shader, write_format);
      GPU_debug_group_end();
      return;
    }
    CLOG_INFO(&LOG,
              "No shader exists for updating mipmaps (format=%s). Fallback to backend "
              "implementation, this could lead to different results between platforms.",
              GPU_texture_format_name(texture_format));
  }

  /* No mipmap shader exists for this texture format. Fallback to backend implementation. */
  tex->generate_mipmap();
}

}  // namespace blender

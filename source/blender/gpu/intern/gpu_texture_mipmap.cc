/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_bit_span.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base_c.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_debug.hh"
#include "GPU_shader.hh"
#include "GPU_shader_builtin.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_storage_buffer.hh"
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

/* Shader and API chunk size are assumed to match. */
static_assert(GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE == MIPMAP_UPDATE_CHUNK_SIZE);
/* Chunk size is assumed power-of-two so footprint at every level is a whole
 * number of destination tiles or within a single tile. */
static_assert((GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE &
               (GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE - 1)) == 0);

static void update_mipmaps_layers(Texture &texture,
                                  Shader &shader,
                                  const IndexRange layers,
                                  const TextureFormat view_format,
                                  const BitSpan modified_chunks)
{
  const int num_mipmaps = texture.mip_count();
  const int layer_count = texture.layer_count();

  /* Create view for each mipmap level. Profiling shows this is the most expensive
   * host side work of GPU mipmap generation, so it may be worth trying to cache this.
   * But it may not be that significant compared to e.g. other texture painting
   * overhead, needs to be measured. */
  Vector<Texture *, 16> views;
  for (int mipmap : IndexRange(num_mipmaps)) {
    views.append(GPU_texture_create_view(
        __func__, &texture, view_format, mipmap, 1, 0, layer_count, false, false));
  }

  /* Check the bitmask has the correct size, or empty for full update. */
  const int2 chunks_size(
      divide_ceil_u(texture.width_get(), GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE),
      divide_ceil_u(texture.height_get(), GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE));
  BLI_assert(modified_chunks.is_empty() ||
             modified_chunks.size() == chunks_size.x * chunks_size.y);

  /* Turn bitmask into chunk coordinates. */
  Vector<int2> chunks;
  bits::foreach_1_index(modified_chunks, [&](const int64_t i) {
    chunks.append(int2(i % chunks_size.x, i / chunks_size.x));
  });
  StorageBuf *chunk_buf = GPU_storagebuf_create_ex(sizeof(int2) * max_ii(chunks.size(), 1),
                                                   chunks.is_empty() ? nullptr : chunks.data(),
                                                   GPU_USAGE_DYNAMIC,
                                                   __func__);
  GPU_storagebuf_bind(chunk_buf, GPU_shader_get_ssbo_binding(&shader, "chunk_list"));

  /* The views cover all layers, so only the destination layer of the dispatch varies. */
  const auto compute_dispatch_layers =
      [&](const int groups_x, const int groups_y, const int groups_z) {
        for (const int64_t layer : layers) {
          GPU_shader_uniform_1i(&shader, "dst_layer", int(layer));
          GPU_compute_dispatch(&shader, groups_x, groups_y, groups_z);
        }
      };

  constexpr int max_levels_per_dispatch = 2;

  for (int mip_start = 0; mip_start < num_mipmaps - 1; mip_start += max_levels_per_dispatch) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    GPU_texture_image_bind(views[mip_start], 0);
    for (int mip_offset = 1; mip_offset <= max_levels_per_dispatch; mip_offset++) {
      GPU_texture_image_bind(views[min_ii(mip_start + mip_offset, views.size() - 1)], mip_offset);
    }
    int num_levels = min_ii(views.size() - mip_start - 1, max_levels_per_dispatch);
    GPU_shader_uniform_1i(&shader, "num_levels", num_levels);

    int3 mip_size(1, 1, 1);
    texture.mip_size_get(mip_start + num_levels, mip_size);

    if (num_levels == 1u) {
      /* Each thread writes one sample. */
      constexpr uint32_t warps = 4;
      const uint32_t samples = mip_size.x * mip_size.y;
      const uint32_t threads = warps * 32U;
      const uint group_len = divide_ceil_u(samples, threads);

      /* The number of work groups in a single dispatch is bounded by `GPU_max_work_group_count()`.
       * Split the work over multiple dispatches when it doesn't fit. */
      const int max_group_count = max_ii(GPU_max_work_group_count(0), 1);
      for (uint group_offset = 0; group_offset < group_len; group_offset += max_group_count) {
        GPU_shader_uniform_1i(&shader, "group_offset", int(group_offset));
        compute_dispatch_layers(int(min_uu(group_len - group_offset, max_group_count)), 1, 1);
      }
      continue;
    }

    /* Each workgroup handles a tile. */
    const int2 tile_count(divide_ceil_u(mip_size.x, MIPMAP_UPDATE_TILE_SIZE),
                          divide_ceil_u(mip_size.y, MIPMAP_UPDATE_TILE_SIZE));

    if (!chunks.is_empty()) {
      /* Partial update. */
      const int dst_level = mip_start + num_levels;

      /* Add a margin for levels with odd dimensions, as neighboring chunks are affected then.
       * For the common case of power-of-two textures there is no margin needed because of the
       * simple box filter. */
      int2 margin(0, 0);
      for (const int src_level : IndexRange(dst_level)) {
        int3 src_size(1, 1, 1);
        texture.mip_size_get(src_level, src_size);
        margin.x |= (src_size.x > 1 && (src_size.x & 1));
        margin.y |= (src_size.y > 1 && (src_size.y & 1));
      }

      /* A tile grid is dispatched per chunk. It covers the chunk's footprint at dst_level,
       * plus the margin. */
      const int chunk_pixels_num = max_ii(GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE >> dst_level, 1);
      const auto chunk_tiles_num = [&](const int margin) {
        return margin ? int(divide_ceil_u(chunk_pixels_num + 2, MIPMAP_UPDATE_TILE_SIZE)) + 1 :
                        max_ii(chunk_pixels_num / MIPMAP_UPDATE_TILE_SIZE, 1);
      };
      const int2 sub_tiles(chunk_tiles_num(margin.x), chunk_tiles_num(margin.y));

      /* Check if we already cover the whole level anyway, if so fall through to full update. */
      const int64_t total_tiles = int64_t(tile_count.x) * tile_count.y;
      if (chunks.size() * sub_tiles.x * sub_tiles.y < total_tiles) {
        /* Partial update, one workgroup per tile in a chunk. */
        GPU_shader_uniform_1i(&shader, "partial_dst_level", dst_level);
        GPU_shader_uniform_1i(&shader, "margin_x", margin.x);
        GPU_shader_uniform_1i(&shader, "margin_y", margin.y);
        compute_dispatch_layers(sub_tiles.x, sub_tiles.y, chunks.size());
        continue;
      }
    }

    /* Full update, one workgroup per tile. */
    GPU_shader_uniform_1i(&shader, "partial_dst_level", -1);
    GPU_shader_uniform_1i(&shader, "margin_x", 0);
    GPU_shader_uniform_1i(&shader, "margin_y", 0);
    compute_dispatch_layers(tile_count.x, tile_count.y, 1);
  }

  for (Texture *view : views) {
    GPU_texture_free(view);
  }
  GPU_storagebuf_unbind(chunk_buf);
  GPU_storagebuf_free(chunk_buf);
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

static void update_mipmaps(Texture &texture,
                           Shader &shader,
                           const TextureFormat write_format,
                           const int partial_layer,
                           const BitSpan modified_chunks)
{
  Context &context = *Context::get();
  Shader *prev_shader = context.shader;

  Texture *texture_ptr = &texture;
  const int layer_count = texture.layer_count();
  const int mip_count = texture.mip_count();
  const bool is_layered = texture.type_get() & GPU_TEXTURE_ARRAY;

  /* When the texture format can not be written to directly and we can't use a view
   * to write it as another format, use a temporary texture. */
  const bool use_temp_texture = write_format != texture.format_get() &&
                                (texture.usage_get() & GPU_TEXTURE_USAGE_FORMAT_VIEW) == 0;
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

  if (modified_chunks.is_empty() || use_temp_texture) {
    /* Full update of every layer. The temporary texture path does not currently
     * support partial updates, but image textures request #GPU_TEXTURE_USAGE_FORMAT_VIEW for
     * the formats that would need it, so painting does not reach this. */
    update_mipmaps_layers(*texture_ptr, shader, IndexRange(layer_count), write_format, {});
  }
  else {
    /* Partial update of a single layer's changed chunks. */
    update_mipmaps_layers(*texture_ptr,
                          shader,
                          IndexRange::from_single(partial_layer),
                          write_format,
                          modified_chunks);
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

eGPUTextureUsage GPU_texture_mipmap_usage(TextureFormat format)
{
  /* Mipmaps generated with the compute shader need write support. */
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE;

  /* If it's sRGB and we can't write directly to it, use a texture view to write through. */
  if (format == TextureFormat::SRGBA_8_8_8_8) {
    if (!GCaps.srgb_write_direct_support && GCaps.srgb_write_view_support) {
      usage |= GPU_TEXTURE_USAGE_FORMAT_VIEW;
    }
  }

  return usage;
}

static void texture_update_mipmap_chain(Texture *tex,
                                        const int partial_layer,
                                        const BitSpan modified_chunks)
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
    /* For sRGB without direct write support, write with a UNORM_8_8_8_8 format,
     * either in a temporary texture or through a texture view. */
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
      update_mipmaps(*tex, *shader, write_format, partial_layer, modified_chunks);
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

void GPU_texture_update_mipmap_chain(Texture *tex)
{
  texture_update_mipmap_chain(tex, 0, {});
}

void GPU_texture_update_mipmap_chain_partial(Texture *tex,
                                             const int layer,
                                             const BitSpan modified_chunks)
{
  if (bits::any_bit_set(modified_chunks)) {
    texture_update_mipmap_chain(tex, layer, modified_chunks);
  }
}

}  // namespace blender

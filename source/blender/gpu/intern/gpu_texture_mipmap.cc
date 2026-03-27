/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_index_range.hh"

#include "GPU_compute.hh"
#include "GPU_debug.hh"
#include "GPU_platform.hh"
#include "GPU_platform_backend_enum.h"
#include "GPU_shader.hh"
#include "GPU_shader_builtin.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_texture_private.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.mipmap"};

namespace blender {

namespace gpu {

static Shader *get_update_mipmap_shader(TextureFormat texture_format)
{
  switch (texture_format) {
    case TextureFormat::UNORM_8_8_8_8:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_UNORM_8_8_8_8);
    case TextureFormat::SFLOAT_16:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_16);
    case TextureFormat::SFLOAT_16_16_16_16:
      return GPU_shader_get_builtin_shader(GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_16_16_16_16);

    default:
      break;
  }
  return nullptr;
}

static TextureFormat get_view_format(TextureFormat texture_format)
{
  switch (texture_format) {
    case TextureFormat::SRGBA_8_8_8_8:
      return TextureFormat::UNORM_8_8_8_8;

    default:
      return texture_format;
  }

  return texture_format;
}

static void update_mipmaps(Texture &texture, Shader &shader, int layer)
{
  const int num_mipmaps = texture.mip_count();
  const TextureFormat view_format = get_view_format(texture.format_get());
  Vector<Texture *, 16> views;
  for (int mipmap : IndexRange(num_mipmaps)) {
    views.append(GPU_texture_create_view(
        __func__, &texture, view_format, mipmap, 1, layer, 1, false, false));
  }

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

    if (num_levels == 1U) {
      /* Each thread writes one sample. */
      constexpr uint32_t warps = 4;
      const uint32_t samples = mip_size.x * mip_size.y;
      const uint32_t threads = warps * 32U;
      int group_len = divide_ceil_u(samples, threads);
      GPU_compute_dispatch(&shader, group_len, 1, 1);
    }
    else {
      /* Each workgroup handles a tile. */
      constexpr uint32_t TileWidth = 8;
      constexpr uint32_t TileHeight = 8;
      const uint32_t horizontalTiles = divide_ceil_u(mip_size.x, TileWidth);
      const uint32_t verticalTiles = divide_ceil_u(mip_size.y, TileHeight);
      int group_len = horizontalTiles * verticalTiles;
      GPU_compute_dispatch(&shader, group_len, 1, 1);
    }
  }

  for (Texture *view : views) {
    GPU_texture_free(view);
  }
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

static void update_mipmaps(Texture &texture, Shader &shader)
{

  Context &context = *Context::get();
  Shader *prev_shader = context.shader;

  GPU_shader_bind(&shader);
  for (int layer : IndexRange(texture.layer_count())) {
    update_mipmaps(texture, shader, layer);
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

void GPU_texture_update_mipmap_chain(Texture *tex)
{
  BLI_assert(tex);

  const int num_mipmaps = tex->mip_count();
  /* Early exit - nothing to generate as texture only contains 1 mipmap level. */
  if (num_mipmaps == 1) {
    return;
  }

  /* Currently enabled for Vulkan and OpenGL. Metal has render issues that needs to be inspected.
   */
  if (GPU_type_matches_ex(GPU_DEVICE_ANY,
                          GPU_OS_ANY,
                          GPU_DRIVER_ANY,
                          GPUBackendType(GPU_BACKEND_VULKAN | GPU_BACKEND_OPENGL)))
  {
    const TextureFormat texture_format = tex->format_get();
    Shader *shader = get_update_mipmap_shader(texture_format);
    if (shader) {
      GPU_debug_group_begin("Update Mipmaps");
      update_mipmaps(*tex, *shader);
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

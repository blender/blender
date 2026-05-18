/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "eevee_light_data.bsl.hh"

namespace eevee::light {

uint bitfield_mask(uint bit_width, uint bit_min)
{
  /* Cannot bit shift more than 31 positions. */
  uint mask = (bit_width > 31u) ? 0x0u : (0xFFFFFFFFu << bit_width);
  return ~mask << bit_min;
}

uint zbin_mask(uint word_index, uint zbin_min, uint zbin_max)
{
  uint word_start = word_index * 32u;
  uint word_end = word_start + 31u;
  uint local_min = max(zbin_min, word_start);
  uint local_max = min(zbin_max, word_end);
  uint mask_width = local_max - local_min + 1;
  return bitfield_mask(mask_width, local_min);
}

int culling_z_to_zbin(float scale, float bias, float z)
{
  return int(z * scale + bias);
}

/* To be instantiated with a callback class that implement eval_directional and eval_local.
 * If ResourceT is not needed, just pass LightRenderData again. */
template<typename CallbackT, typename ResourceT>
void foreach([[resource_table]] const LightRenderData &srt,
             CallbackT &cb,
             [[resource_table]] ResourceT &res)
{
  const LightCullingData &culling = srt.light_cull_buf;

  for (uint index = culling.local_lights_len; index < culling.items_count; index++) {
    LightData light = srt.light_buf[index];
    cb.eval_directional(res, index, light);
  }

  for (uint index = 0; index < culling.visible_count; index++) {
    LightData light = srt.light_buf[index];
    cb.eval_local(res, index, light);
  }
}

/* To be instantiated with a callback class that implement eval_directional and eval_local.
 * If ResourceT is not needed, just pass LightRenderData again. */
template<typename CallbackT, typename ResourceT>
void foreach_visible([[resource_table]] const LightRenderData &srt,
                     float2 pixel,
                     float linear_view_z,
                     CallbackT &cb,
                     [[resource_table]] ResourceT &res)
{
  const LightCullingData &culling = srt.light_cull_buf;
  const auto &zbins = srt.light_zbin_buf;
  const auto &words = srt.light_tile_buf;

  for (uint index = culling.local_lights_len; index < culling.items_count; index++) {
    LightData light = srt.light_buf[index];
    cb.eval_directional(res, index, light);
  }

/* WORKAROUND: For Surfels to use the same lighting path. Could be improved. */
#ifdef SRT_CONSTANT_light_iter_force_no_culling
  /* Same as light::foreach. */
  for (uint index = 0; index < culling.visible_count; index++) {
    LightData light = srt.light_buf[index];
    cb.eval_local(res, index, light);
  }
  return;
#endif

  {
    uint2 tile_co = uint2(pixel / culling.tile_size);
    uint tile_word_offset = (tile_co.x + tile_co.y * culling.tile_x_len) * culling.tile_word_len;
    int zbin_index = culling_z_to_zbin(culling.zbin_scale, culling.zbin_bias, linear_view_z);
    zbin_index = clamp(zbin_index, 0, CULLING_ZBIN_COUNT - 1);
    uint zbin_data = zbins[zbin_index];
    uint min_index = zbin_data & 0xFFFFu;
    uint max_index = zbin_data >> 16u;
    /* Ensure all threads inside a subgroup get the same value to reduce VGPR usage. */
#ifdef GPU_METAL
    /* Waiting to implement extensions support. We need:
     * - GL_KHR_shader_subgroup_ballot
     * - GL_KHR_shader_subgroup_arithmetic
     * or
     * - Vulkan 1.1
     */
    min_index = simd_broadcast_first(simd_min(min_index));
    max_index = simd_broadcast_first(simd_max(max_index));
#endif
    /* Same as divide by 32 but avoid integer division. */
    uint word_min = min_index >> 5u;
    uint word_max = max_index >> 5u;
    for (uint word_idx = word_min; word_idx <= word_max; word_idx++) {
      uint word = words[tile_word_offset + word_idx];
      word &= zbin_mask(word_idx, min_index, max_index);
      /* Ensure all threads inside a subgroup get the same value to reduce VGPR usage. */
#ifdef GPU_METAL
      /* Waiting to implement extensions support. We need:
       * - GL_KHR_shader_subgroup_ballot
       * - GL_KHR_shader_subgroup_arithmetic
       * or
       * - Vulkan 1.1
       */
      word = simd_broadcast_first(simd_or(word));
#endif
      int bit_index;
      while ((bit_index = findLSB(word)) != -1) {
        word &= ~1u << uint(bit_index);
        uint index = word_idx * 32u + uint(bit_index);
        LightData light = srt.light_buf[index];
        cb.eval_local(res, index, light);
      }
    }
  }
}

}  // namespace eevee::light

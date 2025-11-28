/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_render_pass_out)

void output_renderpass_color(int id, float4 color)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    int2 texel = int2(gl_FragCoord.xy);
    imageStoreFast(rp_color_img, int3(texel, id), color);
  }
#endif
}

void output_renderpass_value(int id, float value)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    int2 texel = int2(gl_FragCoord.xy);
    imageStoreFast(rp_value_img, int3(texel, id), float4(value));
  }
#endif
}

void clear_aovs()
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.color_len; i++) {
    output_renderpass_color(uniform_buf.render_pass.color_len + i, float4(0));
  }
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.value_len; i++) {
    output_renderpass_value(uniform_buf.render_pass.value_len + i, 0.0f);
  }
#endif
}

void output_aov(float4 color, float value, uint hash)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  uint total_len = uniform_buf.render_pass.aovs.color_len + uniform_buf.render_pass.aovs.value_len;

  /* Search hashes in uint4 packs with 4 comparisons to find the index of a matching AOV hash. */
  uint hash_index;
  for (hash_index = 0u; hash_index < AOV_MAX && hash_index < total_len; hash_index += 4u) {
    bool4 cmp_mask = equal(uniform_buf.render_pass.aovs.hash[hash_index >> 2u], uint4(hash));
    if (any(cmp_mask)) {
      /* Left-reduce `cmp_mask` to find the index of the matching AOV hash. */
      hash_index += (cmp_mask[0] ? 0u : (cmp_mask[1] ? 1u : (cmp_mask[2] ? 2u : 3u)));
      break;
    }
  }

  /* If a candidate was found by hash, output to texture array layer. */
  if (hash_index < total_len) {
    /* Value hashes are stored after color hashes, so the index tells us the AOV type. */
    bool is_value = hash_index >= uniform_buf.render_pass.aovs.color_len;
    uint aov_index = hash_index - (is_value ? uniform_buf.render_pass.aovs.color_len : 0u);
    if (is_value) {
      uint render_pass_index = uniform_buf.render_pass.value_len + aov_index;
      imageStoreFast(rp_value_img, int3(int2(gl_FragCoord.xy), render_pass_index), float4(value));
    }
    else {
      uint render_pass_index = uniform_buf.render_pass.color_len + aov_index;
      imageStoreFast(rp_color_img, int3(int2(gl_FragCoord.xy), render_pass_index), color);
    }
  }
#endif
}

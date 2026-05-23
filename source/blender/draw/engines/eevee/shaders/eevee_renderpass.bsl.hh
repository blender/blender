/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

#include "gpu_shader_utildefines_lib.glsl"

namespace eevee {

struct RenderPassOutput {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[image(RBUFS_COLOR_SLOT, write, SFLOAT_16_16_16_16)]] image2DArray rp_color_img;
  [[image(RBUFS_VALUE_SLOT, write, SFLOAT_16)]] image2DArray rp_value_img;

  void store_color(int2 texel, int id, float4 color)
  {
    if (id >= 0) {
      imageStoreFast(rp_color_img, int3(texel, id), color);
    }
  }

  void store_value(int2 texel, int id, float value)
  {
    if (id >= 0) {
      imageStoreFast(rp_value_img, int3(texel, id), float4(value));
    }
  }

  void clear_aovs(int2 texel)
  {
    const auto &uniforms = buffer_get(eevee_global_ubo, uniform_buf);

    for (int i = 0; i < AOV_MAX && i < uniforms.render_pass.aovs.color_len; i++) {
      store_color(texel, uniforms.render_pass.color_len + i, float4(0));
    }
    for (int i = 0; i < AOV_MAX && i < uniforms.render_pass.aovs.value_len; i++) {
      store_value(texel, uniforms.render_pass.value_len + i, 0.0f);
    }
  }
};

}  // namespace eevee

void output_aov(
    int2 texel, float4 color, float value, uint hash, float holdout, eObjectInfoFlag ob_flag)
{
  [[resource_table]] eevee::RenderPassOutput &rp = resource_table_get(eevee::RenderPassOutput);
  const auto &uniforms = buffer_get(eevee_global_ubo, uniform_buf);

  uint total_len = uniforms.render_pass.aovs.color_len + uniforms.render_pass.aovs.value_len;

  /* Search hashes in uint4 packs with 4 comparisons to find the index of a matching AOV hash. */
  uint hash_index;
  for (hash_index = 0u; hash_index < AOV_MAX && hash_index < total_len; hash_index += 4u) {
    bool4 cmp_mask = equal(uniforms.render_pass.aovs.hash[hash_index >> 2u], uint4(hash));
    if (any(cmp_mask)) {
      /* Left-reduce `cmp_mask` to find the index of the matching AOV hash. */
      hash_index += (cmp_mask[0] ? 0u : (cmp_mask[1] ? 1u : (cmp_mask[2] ? 2u : 3u)));
      break;
    }
  }

  /* If a candidate was found by hash, output to texture array layer. */
  if (hash_index < total_len) {
    /* Object holdout. */
    if (flag_test(ob_flag, OBJECT_HOLDOUT)) {
      holdout = 1.0f;
    }
    holdout = saturate(holdout);

    /* Value hashes are stored after color hashes, so the index tells us the AOV type. */
    bool is_value = hash_index >= uint(uniforms.render_pass.aovs.color_len);
    uint aov_index = hash_index - (is_value ? uniforms.render_pass.aovs.color_len : 0u);

    /* Apply holdout to relevant AOV type. */
    float4 out_aov = is_value ? float4(value) : color;
    out_aov *= 1.0f - holdout;

    if (is_value) {
      rp.store_value(texel, int(uniforms.render_pass.value_len + aov_index), out_aov.r);
    }
    else {
      rp.store_color(texel, int(uniforms.render_pass.color_len + aov_index), out_aov);
    }
  }
}

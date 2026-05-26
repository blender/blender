/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Clear render passes and background.
 */

#pragma once

#include "infos/eevee_common_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_global_ubo)

#include "eevee_cryptomatte.bsl.hh"
#include "eevee_renderpass.bsl.hh"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee {

struct RenderPassClearFragOut {
  [[frag_color(0)]] float4 background;
};

[[vertex]]
void renderpass_clear_vert([[vertex_id]] const int vert_id, [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position);
}

[[fragment]]
void renderpass_clear_frag([[resource_table]] CryptomatteOutput &cryptomatte,
                           [[resource_table]] RenderPassOutput &render_passes,
                           [[frag_coord]] const float4 frag_co,
                           [[out]] RenderPassClearFragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  frag_out.background = float4(0.0f);

  /* Clear Render Buffers. */
  render_passes.clear_aovs(texel);
  float4 clear_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  render_passes.store_color(texel, uniform_buf.render_pass.environment_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.normal_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.position_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.diffuse_light_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.specular_light_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.diffuse_color_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.specular_color_id, clear_color);
  render_passes.store_color(texel, uniform_buf.render_pass.emission_id, clear_color);
  render_passes.store_value(texel, uniform_buf.render_pass.shadow_id, 1.0f);
  /** NOTE: AO is done on its own pass. */

  imageStoreFast(cryptomatte.rp_cryptomatte_img, texel, float4(0.0f));
}

PipelineGraphic renderpass_clear(renderpass_clear_vert, renderpass_clear_frag);
}  // namespace eevee

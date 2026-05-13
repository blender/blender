/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Clear render passes and background.
 */

#pragma once

#include "infos/eevee_common_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_global_ubo)
FRAGMENT_SHADER_CREATE_INFO(eevee_render_pass_out)
FRAGMENT_SHADER_CREATE_INFO(eevee_cryptomatte_out)

#include "eevee_renderpass_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee {

struct RenderPassClear {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_render_pass_out;
  [[legacy_info]] ShaderCreateInfo eevee_cryptomatte_out;
};

struct RenderPassClearFragOut {
  [[frag_color(0)]] float4 background;
};

[[vertex]]
void renderpass_clear_vert([[vertex_id]] const int vert_id, [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position);
}

[[fragment]]
void renderpass_clear_frag([[resource_table]] RenderPassClear & /*srt*/,
                           [[frag_coord]] const float4 frag_co,
                           [[out]] RenderPassClearFragOut &frag_out)
{
  frag_out.background = float4(0.0f);

  /* Clear Render Buffers. */
  clear_aovs();
  float4 clear_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  output_renderpass_color(uniform_buf.render_pass.environment_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.normal_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.position_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.specular_light_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.diffuse_color_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.specular_color_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.emission_id, clear_color);
  output_renderpass_value(uniform_buf.render_pass.shadow_id, 1.0f);
  /** NOTE: AO is done on its own pass. */

  imageStoreFast(rp_cryptomatte_img, int2(frag_co.xy), float4(0.0f));
}

PipelineGraphic renderpass_clear(renderpass_clear_vert, renderpass_clear_frag);
}  // namespace eevee

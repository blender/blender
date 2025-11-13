/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Clear render passes and background.
 */

#include "infos/eevee_surf_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_renderpass_clear)

#include "eevee_renderpass_lib.glsl"

void main()
{
  out_background = float4(0.0f);

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

  int2 texel = int2(gl_FragCoord.xy);
  imageStoreFast(rp_cryptomatte_img, texel, float4(0.0f));
}

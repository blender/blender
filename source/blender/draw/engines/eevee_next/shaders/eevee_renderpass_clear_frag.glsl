/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Clear render passes and background.
 */

#include "eevee_renderpass_lib.glsl"

void main()
{
  out_background = vec4(0.0);

  /* Clear Render Buffers. */
  clear_aovs();
  vec4 clear_color = vec4(0.0, 0.0, 0.0, 1.0);
  output_renderpass_color(uniform_buf.render_pass.environment_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.normal_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.position_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.specular_light_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.diffuse_color_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.specular_color_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.emission_id, clear_color);
  output_renderpass_value(uniform_buf.render_pass.shadow_id, 1.0);
  /** NOTE: AO is done on its own pass. */

  ivec2 texel = ivec2(gl_FragCoord.xy);
  imageStoreFast(rp_cryptomatte_img, texel, vec4(0.0));
}

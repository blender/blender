/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Background used to shade the world.
 *
 * Outputs shading parameter per pixel using a set of randomized BSDFs.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  return vec4(0.0);
}

void main()
{
  /* Clear AOVs first. In case the material renders to them. */
  clear_aovs();

  init_globals();
  /* View position is passed to keep accuracy. */
  g_data.N = drw_normal_view_to_world(drw_view_incident_vector(interp.P));
  g_data.Ng = g_data.N;
  g_data.P = -g_data.N;
  attrib_load();

  nodetree_surface(0.0);

  g_holdout = saturate(g_holdout);

  out_background.rgb = colorspace_safe_color(g_emission) * (1.0 - g_holdout);
  out_background.a = saturate(average(g_transmittance)) * g_holdout;

  /* World opacity. */
  out_background = mix(vec4(0.0, 0.0, 0.0, 1.0), out_background, world_opacity_fade);

#ifdef MAT_RENDER_PASS_SUPPORT
  /* Clear Render Buffers. */
  ivec2 texel = ivec2(gl_FragCoord.xy);

  vec4 environment = out_background;
  environment.a = 1.0 - environment.a;
  environment.rgb *= environment.a;
  output_renderpass_color(uniform_buf.render_pass.environment_id, environment);

  vec4 clear_color = vec4(0.0, 0.0, 0.0, 1.0);
  output_renderpass_color(uniform_buf.render_pass.normal_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.position_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.specular_light_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.diffuse_color_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.specular_color_id, clear_color);
  output_renderpass_color(uniform_buf.render_pass.emission_id, clear_color);
  output_renderpass_value(uniform_buf.render_pass.shadow_id, 1.0);
  /** NOTE: AO is done on its own pass. */
  imageStore(rp_cryptomatte_img, texel, vec4(0.0));
#endif
}

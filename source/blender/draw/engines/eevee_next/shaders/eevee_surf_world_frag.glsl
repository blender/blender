
/**
 * Background used to shade the world.
 *
 * Outputs shading parameter per pixel using a set of randomized BSDFs.
 **/

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)

void main()
{
  /* Clear AOVs first. In case the material renders to them. */
  clear_aovs();

  init_globals();
  /* View position is passed to keep accuracy. */
  g_data.N = normal_view_to_world(viewCameraVec(interp.P));
  g_data.Ng = g_data.N;
  g_data.P = -g_data.N + cameraPos;
  attrib_load();

  nodetree_surface();

  g_holdout = saturate(g_holdout);

  out_background.rgb = safe_color(g_emission) * (1.0 - g_holdout);
  out_background.a = saturate(avg(g_transmittance)) * g_holdout;

  /* World opacity. */
  out_background = mix(vec4(0.0, 0.0, 0.0, 1.0), out_background, world_opacity_fade);

  /* Clear Render Buffers. */
  ivec2 texel = ivec2(gl_FragCoord.xy);

  vec4 environment = out_background;
  environment.a = 1.0 - environment.a;
  environment.rgb *= environment.a;
  output_renderpass_color(rp_buf.environment_id, environment);

  vec4 clear_color = vec4(0.0, 0.0, 0.0, 1.0);
  output_renderpass_color(rp_buf.normal_id, clear_color);
  output_renderpass_color(rp_buf.diffuse_light_id, clear_color);
  output_renderpass_color(rp_buf.specular_light_id, clear_color);
  output_renderpass_color(rp_buf.diffuse_color_id, clear_color);
  output_renderpass_color(rp_buf.specular_color_id, clear_color);
  output_renderpass_color(rp_buf.emission_id, clear_color);
  output_renderpass_value(rp_buf.shadow_id, 1.0);
  output_renderpass_value(rp_buf.ambient_occlusion_id, 0.0);
  imageStore(rp_cryptomatte_img, texel, vec4(0.0));
}

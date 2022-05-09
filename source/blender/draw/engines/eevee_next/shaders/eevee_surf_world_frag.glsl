
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
}

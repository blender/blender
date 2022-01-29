
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_matcap_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_world_light_lib.glsl)

void main()
{
  /* Normal and Incident vector are in viewspace. Lighting is evaluated in viewspace. */
  vec3 I = get_view_vector_from_screen_uv(uvcoordsvar.st);
  vec3 N = workbench_normal_decode(texture(normalBuffer, uvcoordsvar.st));
  vec4 mat_data = texture(materialBuffer, uvcoordsvar.st);

  vec3 base_color = mat_data.rgb;

  float roughness, metallic;
  workbench_float_pair_decode(mat_data.a, roughness, metallic);

#ifdef V3D_LIGHTING_MATCAP
  /* When using matcaps, mat_data.a is the back-face sign. */
  N = (mat_data.a > 0.0) ? N : -N;

  fragColor.rgb = get_matcap_lighting(matcap_diffuse_tx, matcap_specular_tx, base_color, N, I);
#endif

#ifdef V3D_LIGHTING_STUDIO
  fragColor.rgb = get_world_lighting(base_color, roughness, metallic, N, I);
#endif

#ifdef V3D_LIGHTING_FLAT
  fragColor.rgb = base_color;
#endif

  fragColor.rgb *= get_shadow(N, forceShadowing);

  fragColor.a = 1.0;
}

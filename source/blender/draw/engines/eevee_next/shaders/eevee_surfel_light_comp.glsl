
/**
 * Apply lights contribution to scene surfel representation.
 */

#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)

void light_eval_surfel(
    ClosureDiffuse diffuse, vec3 P, vec3 Ng, float thickness, inout vec3 out_diffuse)
{
  /* Dummy closure. Not used. */
  ClosureReflection reflection;
  reflection.N = vec3(1.0, 0.0, 0.0);
  reflection.roughness = 0.0;
  vec3 out_specular = vec3(0.0);
  /* Dummy ltc mat parameters. Not used since we have no reflections. */
  vec4 ltc_mat_dummy = utility_tx_sample(utility_tx, vec2(0.0), UTIL_LTC_MAT_LAYER);

  vec3 V = Ng;
  float vP_z = 0.0;
  float out_shadow_unused;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_ex(diffuse,
                  reflection,
                  true,
                  P,
                  Ng,
                  V,
                  vP_z,
                  thickness,
                  ltc_mat_dummy,
                  l_idx,
                  out_diffuse,
                  out_specular,
                  out_shadow_unused);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    light_eval_ex(diffuse,
                  reflection,
                  false,
                  P,
                  Ng,
                  V,
                  vP_z,
                  thickness,
                  ltc_mat_dummy,
                  l_idx,
                  out_diffuse,
                  out_specular,
                  out_shadow_unused);
  }
  LIGHT_FOREACH_END
}

void main()
{
  int index = int(gl_GlobalInvocationID.x);
  if (index >= capture_info_buf.surfel_len) {
    return;
  }

  Surfel surfel = surfel_buf[index];

  ClosureDiffuse diffuse_data;
  diffuse_data.N = surfel.normal;
  /* TODO: These could be saved inside the surfel. */
  diffuse_data.sss_radius = vec3(0.0);
  diffuse_data.sss_id = 0u;
  float thickness = 0.0;

  vec3 diffuse_light = vec3(0.0);
  vec3 reflection_light = vec3(0.0);

  light_eval_surfel(diffuse_data, surfel.position, surfel.normal, thickness, diffuse_light);

  surfel_buf[index].radiance_direct.front.rgb += diffuse_light * surfel.albedo_front;

  diffuse_data.N = -surfel.normal;
  diffuse_light = vec3(0.0);
  reflection_light = vec3(0.0);

  light_eval_surfel(diffuse_data, surfel.position, -surfel.normal, thickness, diffuse_light);

  surfel_buf[index].radiance_direct.back.rgb += diffuse_light * surfel.albedo_back;
}

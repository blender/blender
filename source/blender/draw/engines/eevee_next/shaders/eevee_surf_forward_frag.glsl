
/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 **/

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)

float spec_light(ClosureReflection ref)
{
  float gloss = saturate(1.0 - ref.roughness);
  float shininess = exp2(10.0 * gloss + 1.0);
  vec3 N = ref.N;
  vec3 L = vec3(0.0, 0.0, 1.0);
  vec3 H = normalize(L + cameraVec(g_data.P));
  float spec_angle = saturate(dot(N, H));
  float normalization_factor = shininess * 0.125 + 1.0;
  float spec_light = pow(spec_angle, shininess) * saturate(dot(N, L)) * normalization_factor;
  return spec_light;
}

vec4 closure_to_rgba(Closure cl)
{
  vec4 out_color;
  out_color.rgb = g_emission;
  out_color.rgb += g_diffuse_data.color * g_diffuse_data.weight *
                   saturate(g_diffuse_data.N.z * 0.5 + 0.5);
  out_color.rgb += g_reflection_data.color * g_reflection_data.weight *
                   spec_light(g_reflection_data);
  out_color.rgb += g_refraction_data.color * g_refraction_data.weight *
                   saturate(g_refraction_data.N.z * 0.5 + 0.5);

  out_color.a = saturate(1.0 - avg(g_transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset();

  return out_color;
}

void main()
{
  init_globals();

  fragment_displacement();

  nodetree_surface();

  g_holdout = saturate(g_holdout);

  out_radiance.rgb = g_emission;
  out_radiance.rgb += g_diffuse_data.color * g_diffuse_data.weight *
                      saturate(g_diffuse_data.N.z * 0.5 + 0.5);
  out_radiance.rgb += g_reflection_data.color * g_reflection_data.weight *
                      spec_light(g_reflection_data);
  out_radiance.rgb += g_refraction_data.color * g_refraction_data.weight *
                      saturate(g_refraction_data.N.z * 0.5 + 0.5);
  out_radiance.a = 0.0;

  out_radiance.rgb *= 1.0 - g_holdout;

  out_transmittance.rgb = g_transmittance;
  out_transmittance.a = saturate(avg(g_transmittance));

  /* Test */
  out_transmittance.a = 1.0 - out_transmittance.a;
  out_radiance.a = 1.0 - out_radiance.a;
}

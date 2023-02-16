
/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 **/

#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  vec3 diffuse_light = vec3(0.0);
  vec3 reflection_light = vec3(0.0);
  vec3 refraction_light = vec3(0.0);

  float vP_z = dot(cameraForward, g_data.P) - dot(cameraForward, cameraPos);

  light_eval(g_diffuse_data,
             g_reflection_data,
             g_data.P,
             g_data.Ng,
             cameraVec(g_data.P),
             vP_z,
             0.01 /* TODO(fclem) thickness. */,
             diffuse_light,
             reflection_light);

  vec4 out_color;
  out_color.rgb = g_emission;
  out_color.rgb += g_diffuse_data.color * g_diffuse_data.weight * diffuse_light;
  out_color.rgb += g_reflection_data.color * g_reflection_data.weight * reflection_light;
  out_color.rgb += g_refraction_data.color * g_refraction_data.weight * refraction_light;

  out_color.a = saturate(1.0 - avg(g_transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset();

  return out_color;
}

void main()
{
  init_globals();

  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  g_closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement();

  nodetree_surface();

  g_holdout = saturate(g_holdout);

  vec3 diffuse_light = vec3(0.0);
  vec3 reflection_light = vec3(0.0);
  vec3 refraction_light = vec3(0.0);

  float vP_z = dot(cameraForward, g_data.P) - dot(cameraForward, cameraPos);

  light_eval(g_diffuse_data,
             g_reflection_data,
             g_data.P,
             g_data.Ng,
             cameraVec(g_data.P),
             vP_z,
             0.01 /* TODO(fclem) thickness. */,
             diffuse_light,
             reflection_light);

  g_diffuse_data.color *= g_diffuse_data.weight;
  g_reflection_data.color *= g_reflection_data.weight;
  g_refraction_data.color *= g_refraction_data.weight;
  diffuse_light *= step(1e-5, g_diffuse_data.weight);
  reflection_light *= step(1e-5, g_reflection_data.weight);
  refraction_light *= step(1e-5, g_refraction_data.weight);

  out_radiance.rgb = g_emission;
  out_radiance.rgb += g_diffuse_data.color * diffuse_light;
  out_radiance.rgb += g_reflection_data.color * reflection_light;
  out_radiance.rgb += g_refraction_data.color * refraction_light;
  out_radiance.a = 0.0;

  vec3 specular_light = reflection_light + refraction_light;
  vec3 specular_color = g_reflection_data.color + g_refraction_data.color;

  /* TODO(fclem): This feels way too complex for what is it. */
  bool has_any_bsdf_weight = g_diffuse_data.weight != 0.0 || g_reflection_data.weight != 0.0 ||
                             g_refraction_data.weight != 0.0;
  vec3 out_normal = has_any_bsdf_weight ? vec3(0.0) : g_data.N;
  out_normal += g_diffuse_data.N * g_diffuse_data.weight;
  out_normal += g_reflection_data.N * g_reflection_data.weight;
  out_normal += g_refraction_data.N * g_refraction_data.weight;
  out_normal = safe_normalize(out_normal);

#ifdef MAT_RENDER_PASS_SUPPORT
  ivec2 out_texel = ivec2(gl_FragCoord.xy);
  imageStore(rp_normal_img, out_texel, vec4(out_normal, 1.0));
  imageStore(
      rp_light_img, ivec3(out_texel, RENDER_PASS_LAYER_DIFFUSE_LIGHT), vec4(diffuse_light, 1.0));
  imageStore(
      rp_light_img, ivec3(out_texel, RENDER_PASS_LAYER_SPECULAR_LIGHT), vec4(specular_light, 1.0));
  imageStore(rp_diffuse_color_img, out_texel, vec4(g_diffuse_data.color, 1.0));
  imageStore(rp_specular_color_img, out_texel, vec4(specular_color, 1.0));
  imageStore(rp_emission_img, out_texel, vec4(g_emission, 1.0));
  imageStore(rp_cryptomatte_img,
             out_texel,
             vec4(cryptomatte_object_buf[resource_id], node_tree.crypto_hash, 0.0));
#endif

  out_radiance.rgb *= 1.0 - g_holdout;

  out_transmittance.rgb = g_transmittance;
  out_transmittance.a = saturate(avg(g_transmittance));
}

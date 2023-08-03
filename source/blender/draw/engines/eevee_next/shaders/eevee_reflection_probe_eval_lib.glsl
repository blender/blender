
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)

int reflection_probes_find_closest(vec3 P)
{
  int closest_index = -1;
  float closest_distance = FLT_MAX;

  /* ReflectionProbeData doesn't contain any gab, exit at first item that is invalid. */
  for (int index = 1; reflection_probe_buf[index].layer != -1 && index < REFLECTION_PROBES_MAX;
       index++)
  {
    float dist = distance(P, reflection_probe_buf[index].pos.xyz);
    if (dist < closest_distance) {
      closest_distance = dist;
      closest_index = index;
    }
  }
  return closest_index;
}

#ifdef EEVEE_UTILITY_TX
vec4 reflection_probe_eval(ClosureReflection reflection,
                           vec3 P,
                           vec3 V,
                           ReflectionProbeData probe_data)
{
  ivec3 texture_size = textureSize(reflectionProbes, 0);
  float lod_cube_max = min(log2(float(texture_size.x)) - float(probe_data.layer_subdivision) + 1.0,
                           float(REFLECTION_PROBE_MIPMAP_LEVELS));

  /* Pow2f to distributed across lod more evenly */
  float roughness = clamp(pow2f(reflection.roughness), 1e-4f, 0.9999f);

#  if defined(GPU_COMPUTE_SHADER)
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy) + 0.5;
#  else
  vec2 frag_coord = gl_FragCoord.xy;
#  endif
  vec2 noise = utility_tx_fetch(utility_tx, frag_coord, UTIL_BLUE_NOISE_LAYER).gb;
  vec2 rand = fract(noise + sampling_rng_2D_get(SAMPLING_RAYTRACE_U));

  vec3 Xi = sample_cylinder(rand);

  /* Microfacet normal */
  vec3 T, B;
  make_orthonormal_basis(reflection.N, T, B);
  float pdf;
  vec3 H = sample_ggx_reflect(Xi, roughness, V, reflection.N, T, B, pdf);

  vec3 L = -reflect(V, H);
  float NL = dot(reflection.N, L);

  if (NL > 0.0) {
    /* Coarse Approximation of the mapping distortion
     * Unit Sphere -> Cubemap Face */
    const float dist = 4.0 * M_PI / 6.0;

    /* http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html : Equation 13 */
    float lod = clamp(probe_data.lod_factor - 0.5 * log2(pdf * dist), 0.0, lod_cube_max);
    vec4 l_col = reflection_probes_sample(L, lod, probe_data);

    /* Clamped brightness. */
    /* For artistic freedom this should be read from the scene/reflection probe.
     * Note: Eevee-legacy read the firefly_factor from gi_glossy_clamp.
     * Note: Firefly removal should be moved to a different shader and also take SSR into
     * account.*/
    float luma = max(1e-8, max_v3(l_col));
    const float firefly_factor = 1e16;
    l_col.rgb *= 1.0 - max(0.0, luma - firefly_factor) / luma;

    return l_col;
  }
  return vec4(0.0);
}

void reflection_probes_eval(ClosureReflection reflection, vec3 P, vec3 V, inout vec3 out_specular)
{
  int closest_reflection_probe = reflection_probes_find_closest(P);
  vec4 light_color = vec4(0.0);
  if (closest_reflection_probe != -1) {
    ReflectionProbeData probe_data = reflection_probe_buf[closest_reflection_probe];
    light_color = reflection_probe_eval(reflection, P, V, probe_data);
  }

  /* Mix world lighting. */
  if (light_color.a != 1.0) {
    ReflectionProbeData probe_data = reflection_probe_buf[0];
    light_color.rgb = mix(
        reflection_probe_eval(reflection, P, V, probe_data).rgb, light_color.rgb, light_color.a);
  }

  out_specular += light_color.rgb;
}
#endif

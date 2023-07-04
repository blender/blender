#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)

void light_world_eval(ClosureReflection reflection, vec3 P, vec3 V, inout vec3 out_specular)
{
  ivec3 texture_size = textureSize(reflectionProbes, 0);
  /* TODO: This should be based by actual resolution. Currently the resolution is fixed but
   * eventually this should based on a user setting and part of the reflection probe data that will
   * be introduced by the reflection probe patch. */
  float lod_cube_max = 12.0;

  /* Pow2f to distributed across lod more evenly */
  float roughness = clamp(pow2f(reflection.roughness), 1e-4f, 0.9999f);

#if defined(GPU_COMPUTE_SHADER)
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy) + 0.5;
#else
  vec2 frag_coord = gl_FragCoord.xy;
#endif
  vec2 noise = utility_tx_fetch(utility_tx, frag_coord, UTIL_BLUE_NOISE_LAYER).gb;
  vec2 rand = fract(noise + sampling_rng_2D_get(SAMPLING_RAYTRACE_U));

  vec3 Xi = sample_cylinder(rand);

  /* Microfacet normal */
  vec3 T, B;
  make_orthonormal_basis(reflection.N, T, B);
  float pdf;
  vec3 H = sample_ggx(Xi, roughness, V, reflection.N, T, B, pdf);

  vec3 L = -reflect(V, H);
  float NL = dot(reflection.N, L);

  if (NL > 0.0) {
    /* Coarse Approximation of the mapping distortion
     * Unit Sphere -> Cubemap Face */
    const float dist = 4.0 * M_PI / 6.0;

    /* http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html : Equation 13 */
    /* TODO: lod_factor should be precalculated and stored inside the reflection probe data. */
    const float bias = 0.0;
    const float lod_factor = bias + 0.5 * log(float(square_i(texture_size.x))) / log(2.0);
    /* -2: Don't use LOD levels that are smaller than 4x4 pixels. */
    float lod = clamp(lod_factor - 0.5 * log2(pdf * dist), 0.0, lod_cube_max - 2.0);

    vec3 l_col = light_world_sample(L, lod);

    /* Clamped brightness. */
    /* For artistic freedom this should be read from the scene/reflection probe.
     * Note: Eevee-legacy read the firefly_factor from gi_glossy_clamp.
     * Note: Firefly removal should be moved to a different shader and also take SSR into
     * account.*/
    float luma = max(1e-8, max_v3(l_col));
    const float firefly_factor = 1e16;
    l_col *= 1.0 - max(0.0, luma - firefly_factor) / luma;

    /* TODO: for artistic freedom want to read this from the reflection probe. That will be part of
     * the reflection probe patch. */
    const float intensity_factor = 1.0;
    out_specular += vec3(intensity_factor * l_col);
  }
}

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_fast_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_generate_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_volume_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

#ifdef SPHERE_PROBE

struct LightProbeSample {
  SphericalHarmonicL1 volume_irradiance;
  int spherical_id;
};

/**
 * Return cached light-probe data at P.
 * Ng and V are use for biases.
 */
LightProbeSample lightprobe_load(vec3 P, vec3 Ng, vec3 V)
{
  float noise = interlieved_gradient_noise(UTIL_TEXEL, 0.0, 0.0);
  noise = fract(noise + sampling_rng_1D_get(SAMPLING_LIGHTPROBE));

  LightProbeSample result;
  result.volume_irradiance = lightprobe_irradiance_sample(P, V, Ng);
  result.spherical_id = reflection_probes_select(P, noise);
  return result;
}

/* Return the best parallax corrected ray direction from the probe center. */
vec3 lightprobe_sphere_parallax(SphereProbeData probe, vec3 P, vec3 L)
{
  bool is_world = (probe.influence_scale == 0.0);
  if (is_world) {
    return L;
  }
  /* Correct reflection ray using parallax volume intersection. */
  vec3 lP = vec4(P, 1.0) * probe.world_to_probe_transposed;
  vec3 lL = (mat3x3(probe.world_to_probe_transposed) * L) / probe.parallax_distance;

  float dist = (probe.parallax_shape == SHAPE_ELIPSOID) ? line_unit_sphere_intersect_dist(lP, lL) :
                                                          line_unit_box_intersect_dist(lP, lL);

  /* Use distance in world space directly to recover intersection.
   * This works because we assume no shear in the probe matrix. */
  vec3 L_new = P + L * dist - probe.location;

  /* TODO(fclem): Roughness adjustment. */

  return L_new;
}

/**
 * Return spherical sample normalized by irradiance at sample position.
 * This avoid most of light leaking and reduce the need for many local probes.
 */
vec3 lightprobe_spherical_sample_normalized_with_parallax(
    int probe_index, vec3 P, vec3 L, float lod, SphericalHarmonicL1 P_sh)
{
  SphereProbeData probe = reflection_probe_buf[probe_index];
  ReflectionProbeLowFreqLight shading_sh = reflection_probes_extract_low_freq(P_sh);
  vec3 normalization_factor = reflection_probes_normalization_eval(
      L, shading_sh, probe.low_freq_light);
  L = lightprobe_sphere_parallax(probe, P, L);
  return normalization_factor * reflection_probes_sample(L, lod, probe.atlas_coord).rgb;
}

float pdf_to_lod(float pdf)
{
  return 0.0; /* TODO */
}

vec3 lightprobe_eval_direction(LightProbeSample samp, vec3 P, vec3 L, float pdf)
{
  vec3 radiance_sh = lightprobe_spherical_sample_normalized_with_parallax(
      samp.spherical_id, P, L, pdf_to_lod(pdf), samp.volume_irradiance);

  return radiance_sh;
}

vec3 lightprobe_eval(LightProbeSample samp, ClosureDiffuse cl, vec3 P, vec3 V)
{
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(cl.N, samp.volume_irradiance);
  return radiance_sh;
}

vec3 lightprobe_eval(LightProbeSample samp, ClosureTranslucent cl, vec3 P, vec3 V)
{
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(-cl.N, samp.volume_irradiance);
  return radiance_sh;
}

vec3 lightprobe_reflection_dominant_dir(vec3 N, vec3 V, float roughness)
{
  /* From Frostbite PBR Course
   * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
   * Listing 22.
   * Note that the reference labels squared roughness (GGX input) as roughness. */
  float m = square(roughness);
  vec3 R = -reflect(V, N);
  float smoothness = 1.0 - m;
  float fac = smoothness * (sqrt(smoothness) + m);
  return normalize(mix(N, R, fac));
}

vec3 lightprobe_eval(LightProbeSample samp, ClosureReflection reflection, vec3 P, vec3 V)
{
  vec3 L = lightprobe_reflection_dominant_dir(reflection.N, V, reflection.roughness);

  float lod = sphere_probe_roughness_to_lod(reflection.roughness);
  vec3 radiance_cube = lightprobe_spherical_sample_normalized_with_parallax(
      samp.spherical_id, P, L, lod, samp.volume_irradiance);

  float fac = sphere_probe_roughness_to_mix_fac(reflection.roughness);
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(L, samp.volume_irradiance);
  return mix(radiance_cube, radiance_sh, fac);
}

vec3 lightprobe_refraction_dominant_dir(vec3 N, vec3 V, float ior, float roughness)
{
  /* Reusing same thing as lightprobe_reflection_dominant_dir for now with the roughness mapped to
   * reflection roughness. */
  float m = square(roughness);
  vec3 R = refract(-V, N, 1.0 / ior);
  float smoothness = 1.0 - m;
  float fac = smoothness * (sqrt(smoothness) + m);
  return normalize(mix(-N, R, fac));
}

vec3 lightprobe_eval(LightProbeSample samp, ClosureRefraction cl, vec3 P, vec3 V)
{
  float effective_roughness = refraction_roughness_remapping(cl.roughness, cl.ior);

  vec3 L = lightprobe_refraction_dominant_dir(cl.N, V, cl.ior, effective_roughness);

  float lod = sphere_probe_roughness_to_lod(effective_roughness);
  vec3 radiance_cube = lightprobe_spherical_sample_normalized_with_parallax(
      samp.spherical_id, P, L, lod, samp.volume_irradiance);

  float fac = sphere_probe_roughness_to_mix_fac(effective_roughness);
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(L, samp.volume_irradiance);
  return mix(radiance_cube, radiance_sh, fac);
}

void lightprobe_eval(
    LightProbeSample samp, ClosureUndetermined cl, vec3 P, vec3 V, inout vec3 radiance)
{
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      /* TODO: Support in ray tracing first. Otherwise we have a discrepancy. */
      radiance += lightprobe_eval(samp, to_closure_translucent(cl), P, V);
      break;
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* TODO: Support translucency in ray tracing first. Otherwise we have a discrepancy. */
    case CLOSURE_BSDF_DIFFUSE_ID:
      radiance += lightprobe_eval(samp, to_closure_diffuse(cl), P, V);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      radiance += lightprobe_eval(samp, to_closure_reflection(cl), P, V);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      radiance += lightprobe_eval(samp, to_closure_refraction(cl), P, V);
      break;
    case CLOSURE_NONE_ID:
      /* TODO(fclem): Assert. */
      break;
  }
}

#endif /* SPHERE_PROBE */

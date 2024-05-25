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
#pragma BLENDER_REQUIRE(eevee_subsurface_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_closure_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)

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
vec3 lightprobe_spherical_sample_normalized_with_parallax(LightProbeSample samp,
                                                          vec3 P,
                                                          vec3 L,
                                                          float lod)
{
  SphereProbeData probe = reflection_probe_buf[samp.spherical_id];
  ReflectionProbeLowFreqLight shading_sh = reflection_probes_extract_low_freq(
      samp.volume_irradiance);
  float normalization_factor = reflection_probes_normalization_eval(
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
      samp, P, L, pdf_to_lod(pdf));
  return radiance_sh;
}

#  ifdef EEVEE_UTILITY_TX

/* TODO: Port that inside a BSSDF file. */
vec3 lightprobe_eval(LightProbeSample samp, ClosureSubsurface cl, vec3 P, vec3 V, float thickness)
{
  vec3 sss_profile = subsurface_transmission(cl.sss_radius, abs(thickness));
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(cl.N, samp.volume_irradiance);
  radiance_sh += spherical_harmonics_evaluate_lambert(-cl.N, samp.volume_irradiance) * sss_profile;
  return radiance_sh;
}

vec3 lightprobe_eval(
    LightProbeSample samp, ClosureUndetermined cl, vec3 P, vec3 V, float thickness)
{
  LightProbeRay ray = bxdf_lightprobe_ray(cl, P, V, thickness);

  float lod = sphere_probe_roughness_to_lod(ray.perceptual_roughness);
  float fac = sphere_probe_roughness_to_mix_fac(ray.perceptual_roughness);

  vec3 radiance_cube = lightprobe_spherical_sample_normalized_with_parallax(
      samp, P, ray.dominant_direction, lod);
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(ray.dominant_direction,
                                                          samp.volume_irradiance);
  return mix(radiance_cube, radiance_sh, fac);
}
#  endif

#endif /* SPHERE_PROBE */

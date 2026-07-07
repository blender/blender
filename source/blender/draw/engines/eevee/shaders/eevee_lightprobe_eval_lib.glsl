/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_math_geom_lib.glsl"
#include "eevee_bxdf_lib.glsl"
#include "eevee_closure_lib.glsl"
#include "eevee_lightprobe_sphere_eval_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"
#include "eevee_subsurface_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"

#ifdef SPHERE_PROBE

struct LightProbeSample {
  SphericalHarmonicL1 volume_irradiance;
  int spherical_id;
};

/**
 * Return cached light-probe data at P.
 * Ng and V are use for biases.
 */
LightProbeSample lightprobe_load(float3 P, float3 Ng, float3 V)
{
  float noise = interleaved_gradient_noise(UTIL_TEXEL, 0.0f, 0.0f);
  noise = fract(noise + sampling_rng_1D_get(SAMPLING_LIGHTPROBE));

  LightProbeSample result;
  result.volume_irradiance = lightprobe_volume_sample(P, V, Ng);
  result.spherical_id = lightprobe_spheres_select(P, noise);
  return result;
}

/* Return the best parallax corrected ray direction from the probe center. */
float3 lightprobe_sphere_parallax(SphereProbeData probe, float3 P, float3 L)
{
  bool is_world = (probe.influence_scale == 0.0f);
  if (is_world) {
    return L;
  }
  /* Correct reflection ray using parallax volume intersection. */
  float3 lP = float4(P, 1.0f) * probe.world_to_probe_transposed;
  float3 lL = (to_float3x3(probe.world_to_probe_transposed) * L) / probe.parallax_distance;

  float dist = (probe.parallax_shape == SHAPE_ELIPSOID) ? line_unit_sphere_intersect_dist(lP, lL) :
                                                          line_unit_box_intersect_dist(lP, lL);

  /* Use distance in world space directly to recover intersection.
   * This works because we assume no shear in the probe matrix. */
  float3 L_new = P + L * dist - probe.location;

  /* TODO(fclem): Roughness adjustment. */

  return L_new;
}

/**
 * Return spherical sample normalized by irradiance at sample position.
 * This avoid most of light leaking and reduce the need for many local probes.
 */
float3 lightprobe_spherical_sample_normalized_with_parallax(LightProbeSample samp,
                                                            float3 P,
                                                            float3 L,
                                                            float lod)
{
  SphereProbeData probe = lightprobe_sphere_buf[samp.spherical_id];
  ReflectionProbeLowFreqLight shading_sh = lightprobe_spheres_extract_low_freq(
      samp.volume_irradiance);
  float normalization_factor = lightprobe_spheres_normalization_eval(
      L, shading_sh, probe.low_freq_light);
  L = lightprobe_sphere_parallax(probe, P, L);
  return normalization_factor * lightprobe_spheres_sample(L, lod, probe.atlas_coord).rgb;
}

float pdf_to_lod(float inv_pdf)
{
  float blur_pdf = saturate((2.0f * M_PI) * inv_pdf);
  return blur_pdf * 2.0f;
}

float3 lightprobe_eval_direction(LightProbeSample samp, float3 P, float3 L, float inv_pdf)
{
  float3 radiance_sh = lightprobe_spherical_sample_normalized_with_parallax(
      samp, P, L, pdf_to_lod(inv_pdf));
  return radiance_sh;
}

/* TODO: Port that inside a BSSDF file. */
float3 lightprobe_eval(
    LightProbeSample samp, ClosureSubsurface cl, float3 P, float3 V, float thickness)
{
  float3 sss_profile = subsurface_transmission(cl.sss_radius, abs(thickness));
  float3 radiance_sh = spherical_harmonics_evaluate_lambert(cl.N, samp.volume_irradiance);
  radiance_sh += spherical_harmonics_evaluate_lambert(-cl.N, samp.volume_irradiance) * sss_profile;
  return radiance_sh;
}

float3 lightprobe_eval(
    LightProbeSample samp, ClosureUndetermined cl, float3 P, float3 V, float thickness)
{
  LightProbeRay ray = bxdf_lightprobe_ray(cl, P, V, thickness);

  float lod = sphere_probe_roughness_to_lod(ray.perceptual_roughness);
  float fac = sphere_probe_roughness_to_mix_fac(ray.perceptual_roughness);

  float3 radiance_cube = lightprobe_spherical_sample_normalized_with_parallax(
      samp, P, ray.dominant_direction, lod);
  float3 radiance_sh = spherical_harmonics_evaluate_lambert(ray.dominant_direction,
                                                            samp.volume_irradiance);
  return mix(radiance_cube, radiance_sh, fac);
}

#endif /* SPHERE_PROBE */

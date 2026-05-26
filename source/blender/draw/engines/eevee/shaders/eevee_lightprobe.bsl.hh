/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_closure.bsl.hh"
#include "eevee_lightprobe_sphere.bsl.hh"
#include "eevee_lightprobe_volume.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "eevee_subsurface_lib.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

namespace eevee {

struct LightprobeRenderData {
  [[resource_table]] srt_t<LightprobeSphereRenderData> spheres;
  [[resource_table]] srt_t<LightprobeVolumeRenderData> volumes;
  [[resource_table]] srt_t<Sampling> sampling;

  /**
   * Return cached light-probe data at P.
   * Ng and V are use for biases.
   */
  LightProbeSample load(float2 screen_texel, float3 P, float3 Ng, float3 V) const
  {
    [[resource_table]] const Sampling samp = sampling;
    [[resource_table]] const LightprobeVolumeRenderData &lp_volumes = volumes;
    [[resource_table]] const LightprobeSphereRenderData &lp_spheres = spheres;

    float noise = interleaved_gradient_noise(screen_texel, 0.0f, 0.0f);
    noise = fract(noise + samp.rng_1D_get(SAMPLING_LIGHTPROBE));

    LightProbeSample result;
    result.volume_irradiance = lp_volumes.sample_probe(samp, P, V, Ng);
    result.spherical_id = lp_spheres.select_probe(P, noise);
    return result;
  }

  float3 eval_direction(LightProbeSample samp,
                        float3 P,
                        float3 L,
                        float perceptual_roughness) const
  {
    [[resource_table]] const LightprobeSphereRenderData &lp_spheres = spheres;

    /* Avoid over-blurring diffuse. */
    perceptual_roughness = min(0.6f, perceptual_roughness);
    float lod = lightprobe::sphere::roughness_to_lod(perceptual_roughness);
    float3 radiance_sh = lp_spheres.spherical_sample_normalized_with_parallax(samp, P, L, lod);
    return radiance_sh;
  }

  /* TODO: Port that inside a BSSDF file. */
  float3 eval(LightProbeSample samp,
              ClosureSubsurface cl,
              float3 /*P*/,
              float3 /*V*/,
              Thickness thickness) const
  {
    float3 sss_profile = subsurface_transmission(cl.sss_radius, thickness.value());
    float3 radiance_sh = samp.volume_irradiance.evaluate_lambert(cl.N).rgb;
    radiance_sh += samp.volume_irradiance.evaluate_lambert(-cl.N).rgb * sss_profile;
    return radiance_sh;
  }

  float3 eval(
      LightProbeSample samp, ClosureUndetermined cl, float3 P, float3 V, Thickness thickness) const
  {
    [[resource_table]] const LightprobeSphereRenderData &lp_spheres = spheres;

    LightProbeRay ray = bxdf_lightprobe_ray(cl, P, V, thickness);

    float lod = lightprobe::sphere::roughness_to_lod(ray.perceptual_roughness);
    float fac = lightprobe::sphere::roughness_to_mix_fac(ray.perceptual_roughness);

    float3 radiance_cube = lp_spheres.spherical_sample_normalized_with_parallax(
        samp, P, ray.dominant_direction, lod);
    float3 radiance_sh = samp.volume_irradiance.evaluate_lambert(ray.dominant_direction).rgb;
    return mix(radiance_cube, radiance_sh, fac);
  }
};

}  // namespace eevee

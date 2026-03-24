/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/bsdf.h"

#include "kernel/film/write.h"

CCL_NAMESPACE_BEGIN

#ifdef __DENOISING_FEATURES__
ccl_device_forceinline void film_write_denoising_features_surface(KernelGlobals kg,
                                                                  IntegratorState state,
                                                                  const ccl_private ShaderData *sd,
                                                                  ccl_global float *ccl_restrict
                                                                      render_buffer)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  if (!(path_flag & PATH_RAY_DENOISING_FEATURES)) {
    return;
  }

  /* Don't write denoising passes for paths that were split off for shadow catchers
   * to avoid double-counting. */
  if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
    return;
  }

  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

  float3 normal = zero_float3();
  Spectrum diffuse_albedo = zero_spectrum();
  Spectrum specular_albedo = zero_spectrum();
  Spectrum transparent_albedo = zero_spectrum();
  float specular_roughness = 0.0f;
  float sum_weight = 0.0f;
  float sum_nonspecular_weight = 0.0f;

  for (int i = 0; i < sd->num_closure; i++) {
    const ccl_private ShaderClosure *sc = &sd->closure[i];

    if (!CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
      continue;
    }

    /* Transparency always passes through. */
    if (CLOSURE_IS_BSDF_TRANSPARENT(sc->type)) {
      transparent_albedo += sc->weight;
      continue;
    }

    const Spectrum closure_albedo = bsdf_albedo(kg, sd, sc, true, true);
    const float closure_weight = average(closure_albedo);

    /* All closures contribute to the normal feature, but only diffuse-like ones to the albedo. */
    /* If far-field hair, use fiber tangent as feature instead of normal. */
    normal += (sc->type == CLOSURE_BSDF_HAIR_HUANG_ID ? safe_normalize(sd->dPdu) : sc->N) *
              closure_weight;

    const float roughness = sqrtf(bsdf_get_specular_roughness_squared(sc));
    /* Transition smoothly from specular to diffuse between 0.0 and 0.15 roughness. */
    const float diffuse_weight = (sc->type == CLOSURE_BSDF_HAIR_HUANG_ID) ?
                                     1.0f :
                                     smoothstep(0.0f, 0.15f, roughness);

    diffuse_albedo += closure_albedo * diffuse_weight;
    specular_albedo += closure_albedo * (1.0f - diffuse_weight);
    specular_roughness += roughness * closure_weight;

    sum_weight += closure_weight;
    sum_nonspecular_weight += closure_weight * diffuse_weight;
  }

  /* Fraction of non-transparent closures, for smooth blending at transparent surfaces. */
  const float transparent_weight = average(transparent_albedo);
  const float total_weight = sum_weight + transparent_weight;

  /* Blend between writing features at this bounce vs. deferring to the next bounce based
   * on the proportion of diffuse closures. Smoothly transition between 0.0 and 0.5 diffuse
   * fraction. */
  float feature_weight = 0.0f;
  if (sum_weight > 0.0f) {
    normal /= sum_weight;
    specular_roughness /= sum_weight;

    feature_weight = smoothstep(0.0f, 0.5f, sum_nonspecular_weight / sum_weight);
  }

  const Spectrum denoising_feature_throughput = INTEGRATOR_STATE(
      state, path, denoising_feature_throughput);

  if (kernel_data.film.pass_denoising_depth != PASS_UNUSED) {
    const float depth = sd->ray_length - INTEGRATOR_STATE(state, ray, tmin);
    const float denoising_depth = ensure_finite(depth * average(denoising_feature_throughput));
    film_write_pass_float(buffer + kernel_data.film.pass_denoising_depth, denoising_depth);
  }

  if (feature_weight > 0.0f) {
    if (kernel_data.film.pass_denoising_normal != PASS_UNUSED) {
      /* Transform normal into camera space. */
      const Transform worldtocamera = kernel_data.cam.worldtocamera;
      float3 denoising_normal = transform_direction(&worldtocamera, normal);
      const float opaque_fraction = (total_weight > 0.0f) ? (sum_weight / total_weight) : 1.0f;

      denoising_normal = ensure_finite(denoising_normal * opaque_fraction * feature_weight *
                                       average(denoising_feature_throughput));
      film_write_pass_float3(buffer + kernel_data.film.pass_denoising_normal, denoising_normal);
    }

    if (kernel_data.film.pass_denoising_albedo != PASS_UNUSED) {
      const Spectrum denoising_albedo = ensure_finite(diffuse_albedo * feature_weight *
                                                      denoising_feature_throughput);
      film_write_pass_spectrum(buffer + kernel_data.film.pass_denoising_albedo, denoising_albedo);
    }
  }

  if (INTEGRATOR_STATE(state, path, bounce) == 0) {
    if (kernel_data.film.pass_denoising_roughness != PASS_UNUSED) {
      const float denoising_roughness = ensure_finite(sqrtf(specular_roughness) *
                                                      average(denoising_feature_throughput));
      film_write_pass_float(buffer + kernel_data.film.pass_denoising_roughness,
                            denoising_roughness);
    }

    if (kernel_data.film.pass_denoising_specular_albedo != PASS_UNUSED) {
      const Spectrum denoising_specular_albedo = ensure_finite(specular_albedo *
                                                               denoising_feature_throughput);
      film_write_pass_spectrum(buffer + kernel_data.film.pass_denoising_specular_albedo,
                               denoising_specular_albedo);
    }
  }

  /* Portion deferred to the next bounce. Specularity uses the feature weight, transparent
   * always passes through. */
  const Spectrum deferred_albedo = specular_albedo * (1.0f - feature_weight) + transparent_albedo;

  if (reduce_max(fabs(deferred_albedo)) > 1e-4f) {
    INTEGRATOR_STATE_WRITE(state, path, denoising_feature_throughput) *= deferred_albedo;
  }
  else {
    INTEGRATOR_STATE_WRITE(state, path, flag) &= ~PATH_RAY_DENOISING_FEATURES;
  }
}

ccl_device_forceinline void film_write_denoising_features_surface_volume(
    KernelGlobals kg,
    IntegratorState state,
    const ccl_private ShaderData *sd,
    ccl_global float *ccl_restrict render_buffer)
{
  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

  if (kernel_data.film.pass_denoising_depth != PASS_UNUSED) {
    const Spectrum denoising_feature_throughput = INTEGRATOR_STATE(
        state, path, denoising_feature_throughput);

    const float depth = sd->ray_length - INTEGRATOR_STATE(state, ray, tmin);
    const float denoising_depth = ensure_finite(depth * average(denoising_feature_throughput));
    film_write_pass_float(buffer + kernel_data.film.pass_denoising_depth, denoising_depth);
  }
}

ccl_device_forceinline void film_write_denoising_features_volume(KernelGlobals kg,
                                                                 IntegratorState state,
                                                                 const Spectrum albedo,
                                                                 const bool scatter,
                                                                 ccl_global float *ccl_restrict
                                                                     render_buffer)
{
  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);
  const Spectrum denoising_feature_throughput = INTEGRATOR_STATE(
      state, path, denoising_feature_throughput);

  if (scatter && kernel_data.film.pass_denoising_normal != PASS_UNUSED) {
    /* Assume scatter is sufficiently diffuse to stop writing denoising features. */
    INTEGRATOR_STATE_WRITE(state, path, flag) &= ~PATH_RAY_DENOISING_FEATURES;

    /* Write view direction as normal. */
    const float3 denoising_normal = make_float3(0.0f, 0.0f, -1.0f);
    film_write_pass_float3(buffer + kernel_data.film.pass_denoising_normal, denoising_normal);
  }

  if (kernel_data.film.pass_denoising_albedo != PASS_UNUSED) {
    /* Write albedo. */
    const Spectrum denoising_albedo = ensure_finite(denoising_feature_throughput * albedo);
    film_write_pass_spectrum(buffer + kernel_data.film.pass_denoising_albedo, denoising_albedo);
  }
}

ccl_device_forceinline void film_write_denoising_features_background(
    KernelGlobals kg, IntegratorState state, ccl_global float *ccl_restrict render_buffer)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  if (!(path_flag & PATH_RAY_DENOISING_FEATURES)) {
    return;
  }

  /* Do not write default background denoising data for secondary paths. */
  if (INTEGRATOR_STATE(state, path, bounce) != 0) {
    return;
  }

  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

  if (kernel_data.film.pass_denoising_depth != PASS_UNUSED) {
    film_overwrite_pass_float(buffer + kernel_data.film.pass_denoising_depth, FLT_MAX);
  }

  /* 'pass_denoising_albedo' is written by 'film_write_emission_or_background_pass' */
}
#endif /* __DENOISING_FEATURES__ */

CCL_NAMESPACE_END

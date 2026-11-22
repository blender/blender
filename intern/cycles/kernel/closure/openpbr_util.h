/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/bsdf_microfacet.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "util/defines.h"
#include "util/math_base.h"
#include "util/types_spectrum.h"

CCL_NAMESPACE_BEGIN

/* Analytical approximation of the average directional albedo of the Fresnel factor.
 * "A Hitchhiker's Guide to Multiple Scattering" by Eugene d'Eon, v0.3.2 Eq (11.15),
 * https://eugenedeon.com/hitchhikers */
ccl_device_inline float hemispherical_albedo(const float eta)
{
  const float eta_ = (eta < 1.0f) ? 1.0f / eta : eta;

  const float nominator = (10893.0f * eta_) - 1438.2f;
  const float denominator = (-774.4f * sqr(eta_)) + (10212.0f * eta_) + 1.0f;
  float albedo = logf(nominator / denominator);

  if (eta < 1.0f) {
    /* Eq (11.12). */
    albedo = 1.0f - sqr(eta) * (1.0f - albedo);
  }

  return albedo;
}

struct Transmission {
  PackedSpectrum color;
  float weight;
};

/* Remove closures with low weight. */
ccl_device_forceinline float closure_weight_cutoff(const float weight)
{
  if (weight > CLOSURE_WEIGHT_CUTOFF) {
    return weight;
  }
  return 0.0f;
}

ccl_device_forceinline Transmission openpbr_transmission(
    const ccl_global SVMNodeOpenPBRBsdfData &data, const ccl_private float *ccl_restrict stack)
{
  Transmission transmission;
  transmission.weight = closure_weight_cutoff(
      saturatef(stack_load(stack, data.transmission_weight)));
  transmission.color = saturate(stack_load(stack, data.transmission_color));

  return transmission;
}

struct Subsurface {
  PackedSpectrum color;
  float weight;
  packed_float3 radius;
  float anisotropy;
};

ccl_device_forceinline Subsurface openpbr_subsurface(const ccl_global SVMNodeOpenPBRBsdfData &data,
                                                     const ccl_private float *ccl_restrict stack)
{

  Subsurface subsurface = {zero_spectrum(), 0.0f, zero_float3(), 0.0f};
#ifdef __SUBSURFACE__
  subsurface.weight = closure_weight_cutoff(saturatef(stack_load(stack, data.subsurface_weight)));
  if (subsurface.weight == 0.0f) {
    return subsurface;
  }

  subsurface.color = saturate(stack_load(stack, data.subsurface_color));
  subsurface.anisotropy = clamp(
      stack_load(stack, data.subsurface_scatter_anisotropy), -1.0f, 1.0f);
  if (!stack_load(stack, data.geometry_thin_walled)) {
    const float subsurface_radius = max(stack_load(stack, data.subsurface_radius), 0.0f);
    const float3 subsurface_radius_scale = saturate(
        stack_load(stack, data.subsurface_radius_scale));
    subsurface.radius = subsurface_radius_scale * subsurface_radius;
  }
#endif
  return subsurface;
}

struct Base {
  PackedSpectrum color;
  float weight;
  float metalness;
  float roughness;
};

ccl_device_forceinline Base openpbr_base(const ccl_global SVMNodeOpenPBRBsdfData &data,
                                         const ccl_private float *ccl_restrict stack)
{
  Base base;
  base.weight = closure_weight_cutoff(saturatef(stack_load(stack, data.base_weight)));
  base.color = saturate(stack_load(stack, data.base_color)) * base.weight;
  base.metalness = closure_weight_cutoff(saturatef(stack_load(stack, data.base_metalness)));
  base.roughness = saturatef(stack_load(stack, data.base_diffuse_roughness));

  return base;
}

ccl_device_forceinline float3
openpbr_geometry_normal(const ccl_private ShaderData *sd,
                        const ccl_global SVMNodeOpenPBRBsdfData &data,
                        const ccl_private float *ccl_restrict stack)
{
  const float3 N = stack_load_float3_default(stack, data.geometry_normal_offset, sd->N);
  return safe_normalize_fallback(N, sd->N);
}

ccl_device_forceinline Coat openpbr_coat(const ccl_global SVMNodeOpenPBRBsdfData &data,
                                         const ccl_private float *ccl_restrict stack,
                                         const float3 N)
{
  Coat coat;
  coat.weight = closure_weight_cutoff(saturatef(stack_load(stack, data.coat_weight)));
  if (coat.weight == 0.0f) {
    coat.N = N;
    return coat;
  }

  coat.tint = saturate(stack_load(stack, data.coat_color));
  coat.ior = max(stack_load(stack, data.coat_ior), CLOSURE_EPS);
  coat.N = safe_normalize_fallback(
      stack_load_float3_default(stack, data.geometry_coat_normal_offset, N), N);
  coat.roughness = saturatef(stack_load(stack, data.coat_roughness));
  coat.darkening = saturatef(stack_load(stack, data.coat_darkening));
  coat.anisotropy = saturatef(stack_load(stack, data.coat_roughness_anisotropy));

  const SVMStackOffset coat_tangent_offset = data.geometry_coat_tangent_offset;
  if (coat.anisotropy > 0.0f && stack_valid(coat_tangent_offset)) {
    coat.T = stack_load_float3(stack, coat_tangent_offset);
  }

  /* FIXME(OpenPBR): simply invert ior is incorrect, probably should do the same as thin film?
   * Didn't find in the spec how to address backface. Principled BSDF uses original coat IOR
   * without modification. */
  /* coat.ior = backfacing ? 1.0f / coat.ior : coat.ior; */

  return coat;
}

struct Specular {
  PackedSpectrum color;
  float weight;
  float ior;
  float roughness;
};

/* Modulate the IOR so that the reflectivity at normal incidence is scaled by `weight`.
 * OpenPBR Surface specification v1.1, Eq. (25,26).  */
ccl_device_forceinline float openpbr_modulate_ior(const float ior, const float weight)
{
  if (weight == 1.0f) {
    return ior;
  }

  const float modulated_ior = ior_from_F0(weight * F0_from_ior(ior));
  return (ior < 1.0f) ? 1.0f / modulated_ior : modulated_ior;
}

ccl_device_inline Specular openpbr_specular(const ccl_global SVMNodeOpenPBRBsdfData &data,
                                            ccl_private float *ccl_restrict stack,
                                            const ccl_private Coat &coat,
                                            const bool backfacing)
{
  Specular specular;
  specular.color = rgb_to_spectrum(saturate(stack_load(stack, data.specular_color)));
  specular.weight = closure_weight_cutoff(stack_load(stack, data.specular_weight));
  specular.ior = max(stack_load(stack, data.specular_ior), CLOSURE_EPS);
  specular.roughness = saturatef(stack_load(stack, data.specular_roughness));

  /* We need to figure out what we should do with the coat_ior and specular_ior when coming from
   * the inside of the object. */
  if (coat.weight > 0.0f) {
    /* Fixing the ratio to compensate for total internal reflections (TIR).
     * OpenPBR Surface specification v1.1, Eq. (74) */
    const float tir_fixed_ratio = (coat.ior > specular.ior) ? (coat.ior / specular.ior) :
                                                              (specular.ior / coat.ior);

    const float ambient_ior = 1.0f;
    /* OpenPBR Surface specification v1.1, Eq. (60) */
    specular.ior = mix(specular.ior / ambient_ior, tir_fixed_ratio, coat.weight);
  }

  specular.ior = openpbr_modulate_ior(specular.ior, specular.weight);
  specular.ior = backfacing ? 1.0f / specular.ior : specular.ior;

  return specular;
}

ccl_device_inline float2 openpbr_specular_roughness(const ccl_global SVMNodeOpenPBRBsdfData &data,
                                                    const ccl_private float *ccl_restrict stack,
                                                    ccl_private float3 *tangent,
                                                    float specular_roughness,
                                                    const ccl_private Coat &coat)
{
  if ((coat.weight > 0.0f) && (coat.roughness > 0.0f)) {
    /* Coat roughening, OpenPBR Surface specification v1.1, Eq. (73). */
    const float min_coated_roughness = min(
        1.0f, powf(sqr(sqr(specular_roughness)) + 2.0f * sqr(sqr(coat.roughness)), 0.25f));
    specular_roughness = mix(specular_roughness, min_coated_roughness, coat.weight);
  }

  const float anisotropy = saturatef(stack_load(stack, data.specular_roughness_anisotropy));
  *tangent = zero_float3();
  if (anisotropy > 0.0f) {
    const SVMStackOffset offset = data.geometry_tangent_offset;
    if (stack_valid(offset)) {
      *tangent = stack_load_float3(stack, offset);
    }
  }
  return openpbr_anisotropic_roughness(specular_roughness, anisotropy, *tangent);
}

/* Due to internal reflections in the coat, the base is darkened; this is approximated by applying
 * the following coat darkening factor.
 * OpenPBR Surface specification v1.1, Eq. (64-70).  */
ccl_device_inline float3 openpbr_coat_darkening(const ccl_global SVMNodeOpenPBRBsdfData &data,
                                                const ccl_private float *ccl_restrict stack,
                                                const float3 N,
                                                const float3 wi,
                                                const ccl_private Base &base,
                                                const ccl_private Coat &coat,
                                                const ccl_private Transmission &transmission,
                                                const ccl_private Specular &specular,
                                                const ccl_private Subsurface &subsurface)
{
  if (coat.weight == 0.0f) {
    return one_float3();
  }

  const float coat_darkening = saturatef(stack_load(stack, data.coat_darkening));
  if (coat_darkening == 0.0f) {
    /* Coat darkening may not always be desirable artistically. When set to 0, the base albedo is
     * "boosted" so that the observed coated color matches the input base color. */
    return one_float3();
  }

  /* Normal-incidence albedo of the entire base beneath the coat. */
  float3 E_b;
  {
    const float F0 = F0_from_ior(specular.ior);
    const float3 E_glossy = F0 * specular.color;
    const float3 E_glossy_diffuse = E_glossy + (1.0f - E_glossy) * base.color;
    const float3 E_opaque = mix(E_glossy_diffuse, Spectrum(subsurface.color), subsurface.weight);

    const float3 E_translucent = E_glossy + (1.0f - F0) * transmission.color;
    const float3 E_dielectric = mix(E_opaque, E_translucent, transmission.weight);

    const float3 E_metal = base.color;
    E_b = mix(E_dielectric, E_metal, base.metalness);
  }

  if (isequal(E_b, one_float3())) {
    /* As E_b approaches 1, the darkening factor also approaches 1, as required since physically no
     * energy is lost if the base reflects all incident energy. */
    return one_float3();
  }

  /* Internal diffuse reflection coefficient. */
  float K;
  {
    /* Ambient IOR is 1. */
    const float eta_c = coat.ior;

    /* Internal diffuse reflection coefficient. */
    const float E_F = hemispherical_albedo(eta_c);
    const float K_r = 1.0f - (1.0f - E_F) / sqr(eta_c);
    const float3 coat_normal = safe_normalize_fallback(
        stack_load_float3_default(stack, data.geometry_coat_normal_offset, N), N);
    const float cos_i = dot(coat_normal, wi);
    const float K_s = fresnel_dielectric(cos_i, eta_c, nullptr);

    /* Specular roughness. */
    const float r = specular.roughness;
    /* Metal roughness. */
    const float r_m = r;
    /* Base dielectric roughness. */
    const float r_d = mix(1.0f, r, F0_from_ior(specular.ior));
    /* Effective base roughness. */
    const float r_b = mix(r_d, r_m, base.metalness);

    K = mix(K_s, K_r, r_b);
  }

  /* Darkening factor. */
  const float3 Delta = (1.0f - K) / (1.0f - E_b * K);

  return mix(one_float3(), Delta, coat.weight * coat_darkening);
}

ccl_device_forceinline FresnelThinFilm openpbr_thinfilm(
    const ccl_global SVMNodeOpenPBRBsdfData &data, const ccl_private float *ccl_restrict stack)
{
  FresnelThinFilm thin_film;
  thin_film.weight = closure_weight_cutoff(saturatef(stack_load(stack, data.thin_film_weight)));
  /* OpenPBR uses micrometers as unit for the thin film thickness and we need convert it to
   * nanometers. */
  thin_film.thickness = stack_load(stack, data.thin_film_thickness) * 1000.0f;
  thin_film.ior = max(stack_load(stack, data.thin_film_ior), CLOSURE_EPS);

  return thin_film;
}

CCL_NAMESPACE_END

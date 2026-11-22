/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_fresnel.bsl.hh"
#include "gpu_shader_material_open_pbr_util.bsl.hh"

struct Base {
  packed_float3 tint;
  float weight;
  float metalness;
  float diffuse_roughness;
};

float openpbr_specular_ior(const Specular specular, const Coat coat)
{
  float specular_ior = specular.ior;
  if (coat.weight > 0.0f) {
    /* Fixing the ratio to compensate for total internal reflections (TIR).
     * OpenPBR Surface specification v1.1, Eq. (74) */
    const float tir_fixed_ratio = (coat.ior > specular_ior) ? (coat.ior / specular_ior) :
                                                              (specular_ior / coat.ior);

    const float ambient_ior = 1.0f;
    /* OpenPBR Surface specification v1.1, Eq. (60) */
    specular_ior = mix(specular_ior / ambient_ior, tir_fixed_ratio, coat.weight);
  }

  return openpbr_modulate_ior(specular.weight, specular_ior);
}

float3 openpbr_eval_translucent(KernelGlobals &kg,
                                ShadingData &sd,
                                float3 weight,
                                const Specular specular,
                                const Transmission transmission,
                                const float3 N,
                                const float NV,
                                const bool thin_walled,
                                ClosureReflection &reflection_data)
{
  if (transmission.weight == 0.0f) {
    return weight;
  }

  const bool multiggx = true;
  const float2 bsdf = bsdf_lut(kg, NV, specular.roughness, specular.ior, multiggx);
  float3 R, T;
  if (thin_walled) {
    /* Adjust transmission tint based on relative path length. */
    const float3 C = slab_transmittance_at_angle(transmission.tint, NV, specular.ior);

    /* Account for internal reflections, t' = ct1t2 + ct1(r2c)^2t2 + ct1(r2c)^4t2 + ... */
    T = safe_divide(C * square(bsdf.y), (1.0f - square(bsdf.x * C)));
    /* r' = r1 + ct1r2ct2 + ct1(r2c)^3t2 + ... */
    R = bsdf.x * (specular.tint + T * C);
  }
  else {
    /* An approximation of double reflection events, assuming the object is a slab. */
    R = bsdf.x * (specular.tint + square(bsdf.y * transmission.tint));
    T = transmission.tint * bsdf.y;
  }

  weight = openpbr_eval_translucent(
      kg, sd, weight, transmission.weight, R, T, N, specular, thin_walled, reflection_data);

  return weight;
}

float3 openpbr_eval_gloss(const KernelGlobals &kg,
                          const float3 weight,
                          const Specular specular,
                          const float3 N,
                          const float NV,
                          ClosureReflection &reflection_data)
{
  if (specular.ior == 1.0f) {
    return weight;
  }

  const bool multiggx = true;
  const float reflectance = bsdf_lut(kg, NV, specular.roughness, specular.ior, multiggx).x;

  reflection_data.color += weight * reflectance * specular.tint;

  /* Attenuate lower layers */
  return weight * max(1.0f - reflectance, 0.0f);
}

/* Analytical approximation of the average directional albedo of the Fresnel factor. "A
 * Hitchhiker's Guide to Multiple Scattering" by Eugene d'Eon, v0.3.2 Eq (11.15),
 * https://eugenedeon.com/hitchhikers */
float hemispherical_albedo(float eta)
{
  float eta_ = (eta < 1.0f) ? 1.0f / eta : eta;

  float nominator = (10893.0f * eta_) - 1438.2f;
  float denominator = (-774.4f * square(eta_)) + (10212.0f * eta_) + 1.0f;
  float albedo = log(nominator / denominator);

  if (eta < 1.0f) {
    /* Eq (11.12). */
    albedo = 1.0f - square(eta) * (1.0f - albedo);
  }

  return albedo;
}

float3 openpbr_coat_darkening(const float3 wi,
                              const Base &base,
                              const Coat &coat,
                              const Transmission &transmission,
                              const Specular &specular,
                              const Subsurface &subsurface)
{
  constexpr float3 white = float3(1.0f);
  if (!(coat.weight > 0.0f)) {
    return white;
  }

  if (coat.darkening == 0.0f) {
    return white;
  }

  /* Normal-incidence albedo of the entire base beneath the coat. */
  float3 E_b;
  {
    const float F0 = F0_from_ior(specular.ior);
    const float3 E_glossy = F0 * specular.tint;
    const float3 E_glossy_diffuse = E_glossy + (1.0f - E_glossy) * base.tint;
    const float3 E_opaque = mix(E_glossy_diffuse, subsurface.tint, subsurface.weight);

    const float3 E_translucent = E_glossy + (1.0f - F0) * transmission.tint;
    const float3 E_dielectric = mix(E_opaque, E_translucent, transmission.weight);

    const float3 E_metal = base.tint;
    E_b = mix(E_dielectric, E_metal, base.metalness);
  }

  if (all(equal(E_b, white))) {
    /* As E_b approaches 1, the darkening factor also approaches 1, as required since physically no
     * energy is lost if the base reflects all incident energy. */
    return white;
  }

  /* Internal diffuse reflection coefficient. */
  float K;
  {
    /* Ambient IOR is 1. */
    const float eta_c = coat.ior;

    /* Internal diffuse reflection coefficient. */
    const float E_F = hemispherical_albedo(eta_c);
    const float K_r = 1.0f - (1.0f - E_F) / square(eta_c);
    const float cos_i = dot(coat.N, wi);
    const float K_s = fresnel_dielectric_cos(cos_i, eta_c);

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

  return mix(white, Delta, coat.weight * coat.darkening);
}

[[node]]
void node_bsdf_open_pbr(const float float_weight,                             // #0
                        const float base_weight,                              // #1
                        const float4 base_color,                              // #2
                        const float base_metalness,                           // #3
                        const float base_diffuse_roughness,                   // #4
                        const float specular_weight,                          // #5
                        const float4 specular_color,                          // #6
                        const float specular_roughness,                       // #7
                        const float /*specular_roughness_anisotropy*/,        // #8
                        const float specular_ior,                             // #9
                        const float transmission_weight,                      // #10
                        const float4 transmission_color,                      // #11
                        const float /*transmission_depth*/,                   // #12
                        const float4 /*transmission_scatter*/,                // #13
                        const float /*transmission_scatter_anisotropy*/,      // #14
                        const float /*transmission_dispersion_scale*/,        // #15
                        const float /*transmission_dispersion_abbe_number*/,  // #16
                        const float subsurface_weight,                        // #17
                        const float4 subsurface_color,                        // #18
                        const float subsurface_radius,                        // #19
                        const float4 subsurface_radius_scale,                 // #20
                        const float subsurface_scatter_anisotropy,            // #21
                        const float coat_weight,                              // #22
                        const float4 coat_color,                              // #23
                        const float coat_roughness,                           // #24
                        const float /*coat_roughness_anisotropy*/,            // #25
                        const float coat_ior,                                 // #26
                        const float coat_darkening,                           // #27
                        const float fuzz_weight,                              // #28
                        const float4 fuzz_color,                              // #29
                        const float fuzz_roughness,                           // #30
                        float emission_luminance,                             // #31
                        float4 emission_color,                                // #32
                        const float /*thin_film_weight*/,                     // #33
                        const float /*thin_film_thickness*/,                  // #34
                        const float /*thin_film_ior*/,                        // #35
                        float geometry_opacity,                               // #36
                        const bool geometry_thin_walled,                      // #37
                        const float3 geometry_normal,                         // #38
                        const float3 geometry_tangent,                        // #39
                        const float3 geometry_coat_normal,                    // #40
                        const float3 /*geometry_coat_tangent*/,               // #41
                        KernelGlobals &kg,
                        ShadingData &sd,
                        Closure &result)
{
  /* A small cutoff number to prevent numerical issues. */
  constexpr float CLOSURE_EPS = 1e-5f;
  /* Clamp parameters. OpenPBR Surface Specification, Section 5. */
  /* Base. */
  Base base;
  base.tint = saturate(base_color.rgb);
  base.weight = saturate(base_weight);
  base.metalness = saturate(base_metalness);
  base.diffuse_roughness = saturate(base_diffuse_roughness);

  /* Specular. */
  Specular specular;
  specular.tint = saturate(specular_color.rgb);
  specular.roughness = saturate(specular_roughness);
  specular.ior = max(specular_ior, CLOSURE_EPS);
  specular.weight = max(specular_weight, 0.0f);
  /* Not used by EEVEE */
  /* specular.anisotropy = saturate(specular_roughness_anisotropy); */

  /* Transmission. */
  Transmission transmission;
  /* To avoid numerical problems when a color channel is 0 we clamp it to a minimum of 1e-5 */
  transmission.tint = clamp(transmission_color.rgb, float3(CLOSURE_EPS), float3(1.0f));
  transmission.weight = saturate(transmission_weight);
  /* Not used by EEVEE
   * transmission.depth = max(transmission_depth, 0.0f);
   * transmission.scatter = saturate(transmission_scatter);
   * transmission.anisotropy = clamp(transmission_scatter_anisotropy, -1.0f, 1.0f);
   * transmission.dispersion_scale = saturate(transmission_dispersion_scale);
   * transmission.dispersion_abbe_number = max(transmission_dispersion_abbe_number, CLOSURE_EPS);
   */

  /* Subsurface. */
  Subsurface subsurface;
  subsurface.tint = saturate(subsurface_color.rgb);
  subsurface.anisotropy = clamp(subsurface_scatter_anisotropy, -1.0f, 1.0f);
  subsurface.radius = max(subsurface_radius, 0.0f) * saturate(subsurface_radius_scale.rgb);
  /* EEVEE only has Burley for subsurface, scale to approximately match the look of random walk. */
  subsurface.radius *= 4.0f * M_PI;
  subsurface.weight = saturate(subsurface_weight);

  /* Coat. */
  Coat coat;
  coat.tint = saturate(coat_color.rgb);
  coat.roughness = saturate(coat_roughness);
  coat.N = normalize_fallback(geometry_coat_normal, sd.N);
  coat.ior = max(coat_ior, CLOSURE_EPS);
  coat.weight = saturate(coat_weight);
  coat.darkening = saturate(coat_darkening);
  /* Not used by EEVEE.
   * coat.anisotropy = saturate(coat_roughness_anisotropy);
   * coat.T = geometry_coat_tangent; */

  /* Fuzz. */
  Fuzz fuzz;
  fuzz.tint = saturate(fuzz_color.rgb);
  fuzz.roughness = saturate(fuzz_roughness);
  fuzz.weight = saturate(fuzz_weight);

  /* Emission */
  emission_luminance = max(emission_luminance, 0.0f);
  emission_color = max(emission_color, float4(0.0f));

  /* TODO(OpenPBR): implement thin film. */
  /* ThinFilm thin_film;
   * thin_film.weight = saturate(thin_film_weight);
   * thin_film.thickness = max(thin_film_thickness, 0.0f) * 1000.0f;
   * thin_film.ior = max(thin_film_ior, 0.0f); */

  /* Geometry. */
  geometry_opacity = saturate(geometry_opacity);
  const float3 N = normalize_fallback(geometry_normal, sd.N);
  [[maybe_unused]] float3 T = geometry_tangent;

  /* Adjust parameters. */
  base.tint = base.weight * base.tint;
  specular.ior = openpbr_specular_ior(specular, coat);
  const float3 V = coordinate_impl(kg, sd, sd.P, sd.N).incoming;
  const float NV = dot(N, V);
  const bool multiggx = true;

  float3 weight = float3(float_weight);
  ClosureDiffuse diffuse_data = {.color = float3(0.0f), .N = N};
  ClosureReflection reflection_data = {
      .color = float3(0.0f), .N = N, .roughness = specular.roughness};
  ClosureOrenNayar oren_nayar_data = {
      .color = float3(0.0f), .N = N, .roughness = base.diffuse_roughness};

  weight = openpbr_eval_transparency(kg, sd, weight, geometry_opacity);
  weight = openpbr_eval_fuzz(kg, sd, weight, coat, fuzz, N, V);
  weight = openpbr_eval_coat(kg, sd, weight, coat, V);
  weight = openpbr_eval_emission(kg, sd, weight, emission_color.rgb, emission_luminance);
  weight *= openpbr_coat_darkening(V, base, coat, transmission, specular, subsurface);
  weight = openpbr_eval_metal(
      kg, weight, base.tint, specular, base.metalness, NV, multiggx, reflection_data);
  reflection_data.color *= saturate(specular.weight);
  weight = openpbr_eval_translucent(
      kg, sd, weight, specular, transmission, N, NV, geometry_thin_walled, reflection_data);
  weight = openpbr_eval_gloss(kg, weight, specular, N, NV, reflection_data);
  weight = openpbr_eval_subsurface(
      kg, sd, weight, subsurface, geometry_thin_walled, N, NV, diffuse_data, oren_nayar_data);
  openpbr_eval_diffuse(kg, sd, weight, base.tint, NV, diffuse_data, oren_nayar_data);

  closure_eval(kg, sd, reflection_data);

  /* TODO(fclem): EEVEE implementation leaking. */
  result = CLOSURE_DEFAULT;
}

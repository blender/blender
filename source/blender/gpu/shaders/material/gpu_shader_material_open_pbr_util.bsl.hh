/* SPDX-FileCopyrightText: 2019-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Util functions and closure setup for Principled BSDF. */

#pragma once

#include "gpu_shader_common_math.bsl.hh"
#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_fast.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"

float ior_from_F0(const float F0)
{
  const float f = sqrt(clamp(F0, 0.0f, 0.99f));
  return (-f - 1.0f) / (f - 1.0f);
}

/* Given the transmittance through a slab at normal incidence, compute the transmittance at a
 * certain incident angle, based on Beer-Lambert law. */
float3 slab_transmittance_at_angle(const float3 color, const float cos_theta_i, const float ior)
{
  const float inv_cos_theta_t = ior / sqrt_fast(square(ior) - (1.0f - square(cos_theta_i)));
  return pow(color, float3(inv_cos_theta_t));
}

float thin_glass_transmission_roughness(const float roughness, const float ior)
{
  return saturate(roughness *
                  sqrt(3.4f * (ior - 1.0f) * square(ior - 0.5f) / (square(ior) * ior)));
}

struct Coat {
  packed_float3 tint;
  float roughness;
  packed_float3 N;
  float ior;
  float weight;
};

struct Fuzz {
  packed_float3 tint;
  float roughness;
  float weight;
};

struct Specular {
  packed_float3 tint;
  float roughness;
  float ior;
  float weight;
};

struct Transmission {
  packed_float3 tint;
  float weight;
};

struct Subsurface {
  packed_float3 tint;
  float anisotropy;
  packed_float3 radius;
  float weight;
};

float3 openpbr_eval_transparency(KernelGlobals &kg,
                                 ShadingData &sd,
                                 const float3 weight,
                                 const float opacity)
{
  ClosureTransparency transparency_data;
  transparency_data.transmittance = (1.0f - opacity) * weight;
  transparency_data.holdout = 0.0f;
  closure_eval(kg, sd, transparency_data);

  return weight * opacity;
}

float3 openpbr_eval_fuzz(KernelGlobals &kg,
                         ShadingData &sd,
                         const float3 weight,
                         const Coat coat,
                         const Fuzz fuzz,
                         float3 N,
                         const float3 V)
{
  if (fuzz.weight == 0.0f) {
    return weight;
  }

  if (coat.weight > 0.0f) {
    N = safe_normalize(mix(N, coat.N, coat.weight));
  }

  const float albedo = fuzz.weight * sheen_albedo(kg, dot(N, V), fuzz.roughness);
  const ClosureSheen sheen_data = {
      .color = weight * fuzz.tint * albedo, .N = N, .roughness = fuzz.roughness};
  closure_eval(kg, sd, sheen_data);

  /* Attenuate lower layers */
  return weight * saturate(1.0f - albedo);
}

float3 openpbr_eval_coat(
    KernelGlobals &kg, ShadingData &sd, float3 weight, Coat coat, const float3 V)
{
  if (coat.weight == 0.0f) {
    return weight;
  }

  const float coat_NV = dot(coat.N, V);
  const float reflectance = bsdf_lut(kg, coat_NV, coat.roughness, coat.ior, false).x;

  ClosureReflection coat_data;
  coat_data.N = coat.N;
  coat_data.roughness = coat.roughness;
  coat_data.color = weight * coat.weight * reflectance;
  closure_eval(kg, sd, coat_data);

  if (!all(equal(coat.tint, float3(1.0f)))) {
    coat.tint = slab_transmittance_at_angle(coat.tint, coat_NV, coat.ior);
  }

  /* Attenuate lower layers */
  weight *= saturate(1.0f - (1.0f - coat.tint * (1.0f - reflectance)) * coat.weight);

  return weight;
}

float3 openpbr_eval_emission(KernelGlobals &kg,
                             ShadingData &sd,
                             const float3 weight,
                             const float3 color,
                             const float luminance)
{
  ClosureEmission emission_data;
  emission_data.emission = color * luminance * weight;
  closure_eval(kg, sd, emission_data);
  return weight;
}

float3 openpbr_eval_metal(KernelGlobals &kg,
                          const float3 weight,
                          const float3 F0,
                          const Specular specular,
                          const float metalness,
                          const float NV,
                          const bool multiggx,
                          ClosureReflection &reflection_data)
{
  if (metalness == 0.0f) {
    reflection_data.color = float3(0.0f);
    return weight;
  }

  float3 reflectance;
  brdf_f82_tint_lut(
      kg, saturate(F0), saturate(specular.tint), NV, specular.roughness, multiggx, reflectance);
  reflection_data.color = weight * metalness * reflectance;

  /* Attenuate lower layers */
  return weight * max((1.0f - metalness), 0.0f);
}

float openpbr_modulate_ior(const float specular_weight, const float specular_ior)
{
  if (specular_weight == 1.0f) {
    return specular_ior;
  }

  const float modulated_ior = ior_from_F0(specular_weight * F0_from_ior(specular_ior));
  return (specular_ior < 1.0f) ? 1.0f / modulated_ior : modulated_ior;
}

float3 openpbr_eval_translucent(KernelGlobals &kg,
                                ShadingData &sd,
                                const float3 weight,
                                const float transmission,
                                const float3 reflectance,
                                const float3 transmittance,
                                const float3 N,
                                const Specular specular,
                                const bool thin_walled,
                                ClosureReflection &reflection_data)
{
  const float3 transmission_weight = weight * transmission;
  reflection_data.color += transmission_weight * reflectance;

  if (thin_walled) {
    ClosureThinRefraction refraction_data;
    refraction_data.color = transmittance * transmission_weight;
    refraction_data.N = N;
    refraction_data.roughness = thin_glass_transmission_roughness(specular.roughness,
                                                                  specular.ior);
    closure_eval(kg, sd, refraction_data);
  }
  else {
    ClosureRefraction refraction_data;
    refraction_data.N = N;
    refraction_data.roughness = specular.roughness;
    refraction_data.ior = specular.ior;
    refraction_data.color = transmittance * transmission_weight;
    closure_eval(kg, sd, refraction_data);
  }

  /* Attenuate lower layers */
  return weight * max((1.0f - transmission), 0.0f);
}

float3 openpbr_eval_subsurface(KernelGlobals &kg,
                               ShadingData &sd,
                               const float3 weight,
                               const Subsurface subsurface,
                               const bool thin_walled,
                               const float3 N,
                               const float NV,
                               ClosureDiffuse &diffuse_data,
                               ClosureOrenNayar &oren_nayar_data)
{
  if (subsurface.weight == 0.0f) {
    return weight;
  }

  if (thin_walled) {
    /* Backward scattering is approximated by diffuse. */
    const float3 diffuse_weight = weight * subsurface.weight *
                                  saturate(0.5f * (1.0f - subsurface.anisotropy));
    if (oren_nayar_data.roughness == 0.0f) {
      diffuse_data.color += diffuse_weight * subsurface.tint;
    }
    else {
      oren_nayar_data.color += diffuse_weight *
                               oren_nayar_lut(kg, subsurface.tint, NV, oren_nayar_data.roughness);
    }

    const float transmit_weight = subsurface.weight *
                                  saturate(0.5f * (1.0f + subsurface.anisotropy));
    /* Forward scattering is approximated by translucent. */
    /* TODO(weizhen): diffuse roughness. */
    ClosureTranslucent translucent_data;
    translucent_data.color = subsurface.tint * transmit_weight * weight;
    translucent_data.N = N;
    closure_eval(kg, sd, translucent_data);
  }
  else {
    ClosureSubsurface sss_data;
    sss_data.N = N;
    sss_data.sss_radius = subsurface.radius;
    sss_data.color = subsurface.weight * weight * subsurface.tint;
    closure_eval(kg, sd, sss_data);
  }

  /* Attenuate lower layers */
  return weight * max((1.0f - subsurface.weight), 0.0f);
}

void openpbr_eval_diffuse(KernelGlobals &kg,
                          ShadingData &sd,
                          const float3 weight,
                          const float3 color,
                          const float NV,
                          ClosureDiffuse &diffuse_data,
                          ClosureOrenNayar &oren_nayar_data)
{
  if (oren_nayar_data.roughness == 0.0f) {
    diffuse_data.color += weight * color;
  }
  else {
    oren_nayar_data.color += weight * oren_nayar_lut(kg, color, NV, oren_nayar_data.roughness);

    closure_eval(kg, sd, oren_nayar_data);
  }

  closure_eval(kg, sd, diffuse_data);
}

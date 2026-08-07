/* SPDX-FileCopyrightText: 2019-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Util functions and closure setup for Principled BSDF. */

#include "gpu_shader_common_math.glsl"
#include "gpu_shader_math_fast_lib.glsl"

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
                  sqrt(sqrt(3.4f * (ior - 1.0f) * square(ior - 0.5f) / (square(ior) * ior))));
}

float openpbr_fuzz(const float NV, const float rough)
{
  /* Empirical approximation (manual curve fitting) to the fuzz_weight albedo. Can be refined. */
  const float den = 35.6694f * rough * rough - 24.4269f * rough * NV - 0.1405f * NV * NV +
                    6.1211f * rough + 0.28105f * NV - 0.1405f;
  const float num = 58.5299f * rough * rough - 85.0941f * rough * NV + 9.8955f * NV * NV +
                    1.9250f * rough + 74.2268f * NV - 0.2246f;
  return saturate(den / num);
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

float3 openpbr_eval_transparency(const float3 weight, const float opacity)
{
  ClosureTransparency transparency_data;
  transparency_data.transmittance = (1.0f - opacity) * weight;
  transparency_data.holdout = 0.0f;
  closure_eval(transparency_data);

  return weight * opacity;
}

float3 openpbr_eval_fuzz(const float3 weight,
                         const Coat coat,
                         const Fuzz fuzz,
                         const float3 N,
                         const float3 V,
                         ClosureDiffuse &diffuse_data)
{
  if (fuzz.weight == 0.0f) {
    diffuse_data.color = float3(0.0f);
    return weight;
  }

  float fuzz_NV = dot(N, V);
#ifdef MAT_CLEARCOAT
  if (coat.weight > 0.0f) {
    const float3 fuzz_N = safe_normalize(mix(N, coat.N, coat.weight));
    fuzz_NV = dot(fuzz_N, V);
  }
#endif
  fuzz_NV = saturate(fuzz_NV);

  const float3 fuzz_color = fuzz.weight * fuzz.tint * openpbr_fuzz(fuzz_NV, fuzz.roughness);
  /* Consider fuzz as diffuse until we have proper fuzz BSDF in EEVEE. */
  /* TODO: Maybe fuzz should be specular. */
  diffuse_data.color = weight * fuzz_color;
  /* Attenuate lower layers */
  return weight * max((1.0f - math_reduce_max(fuzz_color)), 0.0f);
}

float3 openpbr_eval_coat(float3 weight, Coat coat, const float3 V)
{
#ifdef MAT_CLEARCOAT
  if (coat.weight == 0.0f) {
    return weight;
  }

  const float coat_NV = dot(coat.N, V);
  const float reflectance = bsdf_lut(coat_NV, coat.roughness, coat.ior, false).x;

  ClosureReflection coat_data;
  coat_data.N = coat.N;
  coat_data.roughness = coat.roughness;
  coat_data.color = weight * coat.weight * reflectance;
  closure_eval(coat_data);

  if (!all(equal(coat.tint, float3(1.0f)))) {
    coat.tint = slab_transmittance_at_angle(coat.tint, coat_NV, coat.ior);
  }

  /* Attenuate lower layers */
  weight *= saturate(1.0f - (1.0f - coat.tint * (1.0f - reflectance)) * coat.weight);
#endif

  return weight;
}

float3 openpbr_eval_emission(const float3 weight, const float3 color, const float luminance)
{
  ClosureEmission emission_data;
  emission_data.emission = color * luminance * weight;
  closure_eval(emission_data);
  return weight;
}

float3 openpbr_eval_metal(const float3 weight,
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
      saturate(F0), saturate(specular.tint), NV, specular.roughness, multiggx, reflectance);
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

float3 openpbr_eval_translucent(const float3 weight,
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
    closure_eval(refraction_data);
  }
  else {
    ClosureRefraction refraction_data;
    refraction_data.N = N;
    refraction_data.roughness = specular.roughness;
    refraction_data.ior = specular.ior;
    refraction_data.color = transmittance * transmission_weight;
    closure_eval(refraction_data);
  }

  /* Attenuate lower layers */
  return weight * max((1.0f - transmission), 0.0f);
}

float3 openpbr_eval_subsurface(const float3 weight,
                               const Subsurface subsurface,
                               const bool thin_walled,
                               const float3 N,
                               ClosureDiffuse &diffuse_data)
{
  if (subsurface.weight == 0.0f) {
    return weight;
  }

  if (thin_walled) {
    /* Backward scattering is approximated by diffuse. */
    diffuse_data.color += subsurface.tint * weight * subsurface.weight *
                          saturate(0.5f * (1.0f - subsurface.anisotropy));

    const float transmit_weight = subsurface.weight *
                                  saturate(0.5f * (1.0f + subsurface.anisotropy));
    /* Forward scattering is approximated by translucent. */
    ClosureTranslucent translucent_data;
    translucent_data.color = subsurface.tint * transmit_weight * weight;
    translucent_data.N = N;
    closure_eval(translucent_data);
  }
#ifdef MAT_SUBSURFACE
  else {
    ClosureSubsurface sss_data;
    sss_data.N = N;
    sss_data.sss_radius = subsurface.radius;
    sss_data.color = subsurface.weight * weight * subsurface.tint;
    closure_eval(sss_data);
  }
#endif

  /* Attenuate lower layers */
  return weight * max((1.0f - subsurface.weight), 0.0f);
}

void openpbr_eval_diffuse(const float3 weight,
                          const float3 color,
                          const float3 N,
                          ClosureDiffuse &diffuse_data)
{
#ifdef MAT_DIFFUSE
  diffuse_data.N = N;
  diffuse_data.color += weight * color;

  closure_eval(diffuse_data);
#endif
}

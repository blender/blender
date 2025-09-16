/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_math.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float3 tint_from_color(float3 color)
{
  float lum = dot(color, float3(0.3f, 0.6f, 0.1f)); /* luminance approx. */
  return (lum > 0.0f) ? color / lum : float3(1.0f); /* normalize lum. to isolate hue+sat */
}

float principled_sheen(float NV, float rough)
{
  /* Empirical approximation (manual curve fitting) to the sheen_weight albedo. Can be refined. */
  float den = 35.6694f * rough * rough - 24.4269f * rough * NV - 0.1405f * NV * NV +
              6.1211f * rough + 0.28105f * NV - 0.1405f;
  float num = 58.5299f * rough * rough - 85.0941f * rough * NV + 9.8955f * NV * NV +
              1.9250f * rough + 74.2268f * NV - 0.2246f;
  return saturate(den / num);
}

float ior_from_F0(float F0)
{
  float f = sqrt(clamp(F0, 0.0f, 0.99f));
  return (-f - 1.0f) / (f - 1.0f);
}

void node_bsdf_principled(float4 base_color,
                          float metallic,
                          float roughness,
                          float ior,
                          float alpha,
                          float3 N,
                          float weight,
                          float diffuse_roughness,
                          float subsurface_weight,
                          float3 subsurface_radius,
                          float subsurface_scale,
                          float subsurface_ior,
                          float subsurface_anisotropy,
                          float specular_ior_level,
                          float4 specular_tint,
                          float anisotropic,
                          float anisotropic_rotation,
                          float3 T,
                          float transmission_weight,
                          float coat_weight,
                          float coat_roughness,
                          float coat_ior,
                          float4 coat_tint,
                          float3 CN,
                          float sheen_weight,
                          float sheen_roughness,
                          float4 sheen_tint,
                          float4 emission,
                          float emission_strength,
                          float thin_film_thickness,
                          float thin_film_ior,
                          const float do_multiscatter,
                          out Closure result)
{
  /* Match cycles. */
  metallic = saturate(metallic);
  roughness = saturate(roughness);
  ior = max(ior, 1e-5f);
  alpha = saturate(alpha);
  subsurface_weight = saturate(subsurface_weight);
  /* Not used by EEVEE */
  /* subsurface_anisotropy = clamp(subsurface_anisotropy, 0.0f, 0.9f); */
  /* subsurface_ior = clamp(subsurface_ior, 1.01f, 3.8f); */
  specular_ior_level = max(specular_ior_level, 0.0f);
  specular_tint = max(specular_tint, float4(0.0f));
  /* Not used by EEVEE */
  /* anisotropic = saturate(anisotropic); */
  transmission_weight = saturate(transmission_weight);
  coat_weight = max(coat_weight, 0.0f);
  coat_roughness = saturate(coat_roughness);
  coat_ior = max(coat_ior, 1.0f);
  coat_tint = max(coat_tint, float4(0.0f));
  sheen_weight = max(sheen_weight, 0.0f);
  sheen_roughness = saturate(sheen_roughness);
  sheen_tint = max(sheen_tint, float4(0.0f));

  base_color = max(base_color, float4(0.0f));
  float4 clamped_base_color = min(base_color, float4(1.0f));

  N = safe_normalize(N);
  CN = safe_normalize(CN);
  float3 V = coordinate_incoming(g_data.P);
  float NV = dot(N, V);

  /* Transparency component. */
  if (true) {
    ClosureTransparency transparency_data;
    transparency_data.weight = weight;
    transparency_data.transmittance = float3(1.0f - alpha);
    transparency_data.holdout = 0.0f;
    closure_eval(transparency_data);

    weight *= alpha;
  }

  /* First layer: Sheen */
  float3 sheen_data_color = float3(0.0f);
  if (sheen_weight > 0.0f) {
    /* TODO: Maybe sheen_weight should be specular. */
    float3 sheen_color = sheen_weight * sheen_tint.rgb * principled_sheen(NV, sheen_roughness);
    sheen_data_color = weight * sheen_color;
    /* Attenuate lower layers */
    weight *= max((1.0f - math_reduce_max(sheen_color)), 0.0f);
  }

#ifdef MAT_CLEARCOAT
  /* Second layer: Coat */
  if (coat_weight > 0.0f) {
    float coat_NV = dot(CN, V);
    float reflectance = bsdf_lut(coat_NV, coat_roughness, coat_ior, false).x;

    ClosureReflection coat_data;
    coat_data.N = CN;
    coat_data.roughness = coat_roughness;
    coat_data.color = float3(1.0f);
    coat_data.weight = weight * coat_weight * reflectance;
    closure_eval(coat_data);

    /* Attenuate lower layers */
    weight *= max((1.0f - reflectance * coat_weight), 0.0f);

    if (!all(equal(coat_tint.rgb, float3(1.0f)))) {
      float coat_neta = 1.0f / coat_ior;
      float NT = sqrt_fast(1.0f - coat_neta * coat_neta * (1 - NV * NV));
      /* Tint lower layers. */
      coat_tint.rgb = mix(
          float3(1.0f), pow(coat_tint.rgb, float3(1.0f / NT)), saturate(coat_weight));
    }
  }
  else {
    coat_tint.rgb = float3(1.0f);
  }
#else
  coat_tint.rgb = float3(1.0f);
#endif

  /* Emission component.
   * Attenuated by sheen and coat.
   */
  if (true) {
    ClosureEmission emission_data;
    emission_data.weight = weight;
    emission_data.emission = coat_tint.rgb * emission.rgb * emission_strength;
    closure_eval(emission_data);
  }

  /* Metallic component */
  float3 reflection_tint = specular_tint.rgb;
  float3 reflection_color = float3(0.0f);
  if (metallic > 0.0f) {
    float3 F0 = clamped_base_color.rgb;
    float3 F82 = min(reflection_tint, float3(1.0f));
    float3 metallic_brdf;
    brdf_f82_tint_lut(F0, F82, NV, roughness, do_multiscatter != 0.0f, metallic_brdf);
    reflection_color = weight * metallic * metallic_brdf;
    /* Attenuate lower layers */
    weight *= max((1.0f - metallic), 0.0f);
  }

#ifdef MAT_REFRACTION
  /* Transmission component */
  if (transmission_weight > 0.0f) {
    float3 F0 = float3(F0_from_ior(ior)) * reflection_tint;
    float3 F90 = float3(1.0f);
    float3 reflectance, transmittance;
    bsdf_lut(F0,
             F90,
             sqrt(clamped_base_color.rgb),
             NV,
             roughness,
             ior,
             do_multiscatter != 0.0f,
             reflectance,
             transmittance);

    reflection_color += weight * transmission_weight * reflectance;

    ClosureRefraction refraction_data;
    refraction_data.N = N;
    refraction_data.roughness = roughness;
    refraction_data.ior = ior;
    refraction_data.weight = weight * transmission_weight;
    refraction_data.color = transmittance * coat_tint.rgb;
    closure_eval(refraction_data);

    /* Attenuate lower layers */
    weight *= max((1.0f - transmission_weight), 0.0f);
  }
#endif

  /* Specular component */
  if (true) {
    float eta = ior;
    float f0 = F0_from_ior(eta);
    if (specular_ior_level != 0.5f) {
      f0 *= 2.0f * specular_ior_level;
      eta = ior_from_F0(f0);
      if (ior < 1.0f) {
        eta = 1.0f / eta;
      }
    }

    float3 F0 = float3(f0) * reflection_tint;
    F0 = clamp(F0, float3(0.0f), float3(1.0f));
    float3 F90 = float3(1.0f);
    float3 reflectance, unused;
    bsdf_lut(
        F0, F90, float3(0.0f), NV, roughness, eta, do_multiscatter != 0.0f, reflectance, unused);

    ClosureReflection reflection_data;
    reflection_data.N = N;
    reflection_data.roughness = roughness;
    reflection_data.color = (reflection_color + weight * reflectance) * coat_tint.rgb;
    /* `weight` is already applied in `color`. */
    reflection_data.weight = 1.0f;
    closure_eval(reflection_data);

    /* Attenuate lower layers */
    weight *= max((1.0f - math_reduce_max(reflectance)), 0.0f);
  }

#ifdef MAT_SUBSURFACE
  /* Subsurface component */
  if (subsurface_weight > 0.0f) {
    ClosureSubsurface sss_data;
    sss_data.N = N;
    sss_data.sss_radius = max(subsurface_radius * subsurface_scale, float3(0.0f));
    /* Subsurface Scattering materials behave unpredictably with values greater than 1.0 in
     * Cycles. So it's clamped there and we clamp here for consistency with Cycles. */
    sss_data.color = (subsurface_weight * weight) * clamped_base_color.rgb * coat_tint.rgb;
    /* Add energy of the sheen layer until we have proper sheen BSDF. */
    sss_data.color += sheen_data_color;
    /* `weight` is already applied in `color`. */
    sss_data.weight = 1.0f;
    closure_eval(sss_data);

    /* Attenuate lower layers */
    weight *= max((1.0f - subsurface_weight), 0.0f);
  }
#endif

#ifdef MAT_DIFFUSE
  /* Diffuse component */
  if (true) {
    ClosureDiffuse diffuse_data;
    diffuse_data.N = N;
    diffuse_data.color = weight * base_color.rgb * coat_tint.rgb;
    /* Add energy of the sheen layer until we have proper sheen BSDF. */
    diffuse_data.color += sheen_data_color;
    /* `weight` is already applied in `color`. */
    diffuse_data.weight = 1.0f;
    closure_eval(diffuse_data);
  }
#endif

  result = Closure(0);
}

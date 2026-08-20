/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_open_pbr_util.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float3 principled_eval_translucent(float3 weight,
                                   const Specular specular,
                                   const Transmission transmission,
                                   const float3 N,
                                   const float NV,
                                   const bool thin_walled,
                                   const bool multiggx,
                                   ClosureReflection &reflection_data)
{
#ifdef MAT_REFRACTION
  if (transmission.weight == 0.0f) {
    return weight;
  }

  const float3 F0 = F0_from_ior(specular.ior) * specular.tint;
  const float3 F90 = float3(1.0f);
  const float3 tint = thin_walled ? float3(1.0f) : sqrt(transmission.tint);
  float3 R, T;
  bsdf_lut(F0, F90, tint, NV, specular.roughness, specular.ior, multiggx, R, T);

  if (thin_walled) {
    /* Adjust transmission tint based on relative path length. */
    const float3 C = slab_transmittance_at_angle(transmission.tint, NV, specular.ior);

    /* Account for internal reflections, t' = ctt + ct(rc)^2t + ct(rc)^4t + ... */
    T = safe_divide(C * square(T), (1.0f - square(R * C)));
    /* r' = r + ctrct + ct(rc)^3t + ... */
    R *= (1.0f + T * C);
  }

  weight = openpbr_eval_translucent(
      weight, transmission.weight, R, T, N, specular, thin_walled, reflection_data);
#endif

  return weight;
}

float3 principled_eval_gloss(const float3 weight,
                             const Specular specular,
                             const float3 N,
                             const float NV,
                             const bool multiggx,
                             ClosureReflection &reflection_data)
{
  const float ior = openpbr_modulate_ior(specular.weight, specular.ior);
  const float3 F0 = saturate(float3(F0_from_ior(ior)) * specular.tint);
  const float3 F90 = float3(1.0f);

  float3 reflectance, unused;
  bsdf_lut(F0, F90, float3(0.0f), NV, specular.roughness, ior, multiggx, reflectance, unused);

  reflection_data.N = N;
  reflection_data.roughness = specular.roughness;
  reflection_data.color += weight * reflectance;
  closure_eval(reflection_data);

  /* Attenuate lower layers */
  return weight * max((1.0f - math_reduce_max(reflectance)), 0.0f);
}

[[node]]
void node_bsdf_principled(float4 base_color,
                          float metallic,
                          const float roughness,
                          const float ior,
                          float alpha,
                          const float thin_wall,
                          float3 N,
                          const float float_weight,
                          const float diffuse_roughness,
                          const float subsurface_weight,
                          const float3 subsurface_radius,
                          const float subsurface_scale,
                          const float subsurface_ior,
                          const float subsurface_anisotropy,
                          const float specular_ior_level,
                          const float4 specular_tint,
                          const float anisotropic,
                          const float anisotropic_rotation,
                          const float3 T,
                          const float transmission_weight,
                          const float transmission_dispersion_scale,
                          const float transmission_dispersion_abbe_number,
                          const float coat_weight,
                          const float coat_roughness,
                          const float coat_ior,
                          const float4 coat_tint,
                          const float3 CN,
                          const float sheen_weight,
                          const float sheen_roughness,
                          const float4 sheen_tint,
                          const float4 emission,
                          const float emission_strength,
                          const float thin_film_thickness,
                          const float thin_film_ior,
                          const float do_multiscatter,
                          const float subsurface_random_walk_radius_scale,
                          Closure &result)
{
  /* Match Cycles. */
  metallic = saturate(metallic);
  alpha = saturate(alpha);
  base_color = max(base_color, float4(0.0f));
  const float3 clamped_base_color = min(base_color.rgb, float3(1.0f));

  Subsurface subsurface;
  subsurface.tint = clamped_base_color;
  subsurface.anisotropy = clamp(subsurface_anisotropy, -1.0f, 1.0f);
  subsurface.radius = subsurface_radius * subsurface_scale * subsurface_random_walk_radius_scale;
  subsurface.radius = max(subsurface.radius, float3(0.0f));
  subsurface.weight = saturate(subsurface_weight);
  /* Not used by EEVEE */
  /* subsurface_ior = clamp(subsurface_ior, 1.01f, 3.8f); */

  Specular specular;
  specular.tint = max(specular_tint.rgb, float3(0.0f));
  specular.roughness = saturate(roughness);
  specular.ior = max(ior, 1e-5f);
  specular.weight = max(specular_ior_level * 2.0f, 0.0f);
  /* Not used by EEVEE */
  /* anisotropic = saturate(anisotropic); */

  Transmission transmission;
  transmission.tint = clamped_base_color;
  transmission.weight = saturate(transmission_weight);
  /* Not used by EEVEE */
  /* transmission_dispersion_scale = saturate(transmission_dispersion_scale); */
  /* transmission_dispersion_abbe_number = max(transmission_dispersion_abbe_number, 0.0f); */

  Coat coat;
  coat.tint = max(coat_tint.rgb, float3(0.0f));
  coat.roughness = saturate(coat_roughness);
  coat.N = normalize_fallback(CN, g_data.N);
  coat.ior = max(coat_ior, 1.0f);
  coat.weight = saturate(coat_weight);

  Fuzz fuzz;
  fuzz.tint = max(sheen_tint.rgb, float3(0.0f));
  fuzz.roughness = saturate(sheen_roughness);
  fuzz.weight = max(sheen_weight, 0.0f);

  /* Geometry. */
  N = normalize_fallback(N, g_data.N);
  const float3 V = coordinate_incoming(g_data.P);
  const float NV = dot(N, V);
  const bool thin_walled = (thin_wall != 0.0f);
  const bool multiggx = thin_walled || (do_multiscatter != 0.0f);

  float3 weight = float3(float_weight);
  ClosureDiffuse diffuse_data;
  ClosureReflection reflection_data;

  weight = openpbr_eval_transparency(weight, alpha);
  weight = openpbr_eval_fuzz(weight, coat, fuzz, N, V, diffuse_data);
  weight = openpbr_eval_coat(weight, coat, V);
  weight = openpbr_eval_emission(weight, emission.rgb, emission_strength);
  weight = openpbr_eval_metal(
      weight, clamped_base_color, specular, metallic, NV, multiggx, reflection_data);
  weight = principled_eval_translucent(
      weight, specular, transmission, N, NV, thin_walled, multiggx, reflection_data);
  weight = principled_eval_gloss(weight, specular, N, NV, multiggx, reflection_data);
  weight = openpbr_eval_subsurface(weight, subsurface, thin_walled, N, diffuse_data);
  openpbr_eval_diffuse(weight, base_color.rgb, N, diffuse_data);

  result = Closure(0);
}

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float3 fresnel_conductor(float cosi, float3 eta, float3 k)
{

  float3 cosi_sqr = float3(cosi * cosi);
  float3 one = float3(1.0f);
  float3 tmp_f = (eta * eta) + (k * k);

  float3 tmp_two_eta_cosi = 2.0f * eta * float3(cosi);

  float3 tmp = tmp_f * cosi_sqr;
  float3 Rparl2 = (tmp - tmp_two_eta_cosi + one) / (tmp + tmp_two_eta_cosi + one);
  float3 Rperp2 = (tmp_f - tmp_two_eta_cosi + cosi_sqr) / (tmp_f + tmp_two_eta_cosi + cosi_sqr);
  return (Rparl2 + Rperp2) * 0.5f;
}

void node_bsdf_metallic(float4 base_color,
                        float4 edge_tint,
                        float3 ior,
                        float3 extinction,
                        float roughness,
                        float anisotropy,
                        float rotation,
                        float3 N,
                        float3 T,
                        float weight,
                        float thin_film_thickness,
                        float thin_film_ior,
                        const float do_multiscatter,
                        const float use_complex_ior,
                        out Closure result)
{
  float3 F0 = base_color.rgb;
  float3 F82 = edge_tint.rgb;
  if (use_complex_ior != 0.0f) {
    /* Compute incidence at 0 and 82 degrees from conductor Fresnel. */
    F0 = fresnel_conductor(1.0f, ior, extinction);
    F82 = fresnel_conductor(1.0f / 7.0f, ior, extinction);
  }

  /* Clamp to match Cycles */
  F0 = saturate(F0);
  F82 = saturate(F82);
  roughness = saturate(roughness);
  /* Not used by EEVEE */
  /* anisotropy = saturate(anisotropy); */

  N = safe_normalize(N);
  float3 V = coordinate_incoming(g_data.P);
  float NV = dot(N, V);

  ClosureReflection reflection_data;
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  float3 metallic_brdf;
  brdf_f82_tint_lut(F0, F82, NV, roughness, do_multiscatter != 0.0f, metallic_brdf);
  reflection_data.color = metallic_brdf;
  reflection_data.weight = weight;

  result = closure_eval(reflection_data);
}

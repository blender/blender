/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec3 fresnel_conductor(float cosi, vec3 eta, vec3 k)
{

  vec3 cosi_sqr = vec3(cosi * cosi);
  vec3 one = vec3(1.0);
  vec3 tmp_f = (eta * eta) + (k * k);

  vec3 tmp_two_eta_cosi = 2.0 * eta * vec3(cosi);

  vec3 tmp = tmp_f * cosi_sqr;
  vec3 Rparl2 = (tmp - tmp_two_eta_cosi + one) / (tmp + tmp_two_eta_cosi + one);
  vec3 Rperp2 = (tmp_f - tmp_two_eta_cosi + cosi_sqr) / (tmp_f + tmp_two_eta_cosi + cosi_sqr);
  return (Rparl2 + Rperp2) * 0.5;
}

void node_bsdf_metallic(vec4 base_color,
                        vec4 edge_tint,
                        vec3 ior,
                        vec3 extinction,
                        float roughness,
                        float anisotropy,
                        float rotation,
                        vec3 N,
                        vec3 T,
                        float weight,
                        const float do_multiscatter,
                        const float use_complex_ior,
                        out Closure result)
{
  vec3 F0 = base_color.rgb;
  vec3 F82 = edge_tint.rgb;
  if (use_complex_ior != 0.0) {
    /* Compute incidence at 0 and 82 degrees from conductor Fresnel. */
    F0 = fresnel_conductor(1.0, ior, extinction);
    F82 = fresnel_conductor(1.0 / 7.0, ior, extinction);
  }

  /* Clamp to match Cycles */
  F0 = saturate(F0);
  F82 = saturate(F82);
  roughness = saturate(roughness);
  /* Not used by EEVEE */
  /* anisotropy = saturate(anisotropy); */

  N = safe_normalize(N);
  vec3 V = coordinate_incoming(g_data.P);
  float NV = dot(N, V);

  ClosureReflection reflection_data;
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  vec3 metallic_brdf;
  brdf_f82_tint_lut(F0, F82, NV, roughness, do_multiscatter != 0.0, metallic_brdf);
  reflection_data.color = metallic_brdf;
  reflection_data.weight = weight;

  result = closure_eval(reflection_data);
}

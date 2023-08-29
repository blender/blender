/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * BxDF evaluation functions.
 */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name GGX
 *
 * \{ */

float bxdf_ggx_D(float NH, float a2)
{
  return a2 / (M_PI * square((a2 - 1.0) * (NH * NH) + 1.0));
}

float bxdf_ggx_D_opti(float NH, float a2)
{
  float tmp = (NH * a2 - NH) * NH + 1.0;
  /* Doing RCP and multiply a2 at the end. */
  return M_PI * tmp * tmp;
}

float bxdf_ggx_smith_G1(float NX, float a2)
{
  return 2.0 / (1.0 + sqrt(1.0 + a2 * (1.0 / (NX * NX) - 1.0)));
}

float bxdf_ggx_smith_G1_opti(float NX, float a2)
{
  /* Using Brian Karis approach and refactoring by NX/NX
   * this way the `(2*NL)*(2*NV)` in `G = G1(V) * G1(L)` gets canceled by the BRDF denominator
   * `4*NL*NV` RCP is done on the whole G later. Note that this is not convenient for the
   * transmission formula. */
  return NX + sqrt(NX * (NX - NX * a2) + a2);
}

/* Compute the GGX BRDF without the Fresnel term, multiplied by the cosine foreshortening term. */
float bsdf_ggx(vec3 N, vec3 L, vec3 V, float roughness)
{
  float a2 = square(roughness);

  vec3 H = normalize(L + V);
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, L), 1e-8);
  float NV = max(dot(N, V), 1e-8);

  /* TODO: maybe implement non-separable shadowing-masking term following Cycles. */
  float G = bxdf_ggx_smith_G1(NV, a2) * bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);

  /* BRDF * NL =  `((D * G) / (4 * NV * NL)) * NL`. */
  return (0.25 * D * G) / NV;
}

/* Compute the GGX BTDF without the Fresnel term, multiplied by the cosine foreshortening term. */
float btdf_ggx(vec3 N, vec3 L, vec3 V, float roughness, float eta)
{
  float LV = dot(L, V);
  if (is_equal(eta, 1.0, 1e-4)) {
    /* Only valid when `L` and `V` point in the opposite directions. */
    return float(is_equal(LV, -1.0, 1e-3));
  }

  bool valid = (eta < 1.0) ? (LV < -eta) : (LV * eta < -1.0);
  if (!valid) {
    /* Impossible configuration for transmission due to total internal reflection. */
    return 0.0;
  }

  vec3 H = eta * L + V;
  H = (eta < 1.0) ? H : -H;
  float inv_len_H = safe_rcp(length(H));
  H *= inv_len_H;

  /* For transmission, `L` lies in the opposite hemisphere as `H`, therefore negate `L`. */
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, -L), 1e-8);
  float NV = max(dot(N, V), 1e-8);
  float VH = saturate(dot(V, H));
  float LH = saturate(dot(-L, H));

  float a2 = square(roughness);
  float G = bxdf_ggx_smith_G1(NV, a2) * bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);

  /* `btdf * NL = abs(VH * LH) * ior^2 * D * G(V) * G(L) / (Ht2 * NV * NL) * NL`. */
  return (D * G * VH * LH * square(eta * inv_len_H)) / NV;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lambert
 *
 * Not really a microfacet model but fits this file.
 * \{ */

float bsdf_lambert(vec3 N, vec3 L)
{
  return saturate(dot(N, L));
}

/** \} */

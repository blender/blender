/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * BxDF evaluation functions.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name GGX
 *
 * \{ */

float bxdf_ggx_D(float NH, float a2)
{
  return a2 / (M_PI * sqr((NH * a2 - NH) * NH + 1.0));
}

float bxdf_ggx_smith_G1(float NX, float a2)
{
  return 2.0 / (1.0 + sqrt(1.0 + a2 * (1 - NX * NX) / (NX * NX)));
}

float bxdf_ggx_D_opti(float NH, float a2)
{
  float tmp = (NH * a2 - NH) * NH + 1.0;
  /* Doing RCP and mul a2 at the end. */
  return M_PI * tmp * tmp;
}

float bxdf_ggx_smith_G1_opti(float NX, float a2)
{
  /* Using Brian Karis approach and refactoring by NX/NX
   * this way the (2*NL)*(2*NV) in G = G1(V) * G1(L) gets canceled by the brdf denominator 4*NL*NV
   * Rcp is done on the whole G later.
   * Note that this is not convenient for the transmission formula. */
  /* return 2 / (1 + sqrt(1 + a2 * (1 - NX*NX) / (NX*NX) ) ); /* Reference function. */
  return NX + sqrt(NX * (NX - NX * a2) + a2);
}

float bsdf_ggx(vec3 N, vec3 L, vec3 V, float roughness)
{
  float a2 = sqr(roughness);

  vec3 H = normalize(L + V);
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, L), 1e-8);
  float NV = max(dot(N, V), 1e-8);

  /* Doing RCP at the end */
  float G = bxdf_ggx_smith_G1_opti(NV, a2) * bxdf_ggx_smith_G1_opti(NL, a2);
  float D = bxdf_ggx_D_opti(NH, a2);

  /* Denominator is canceled by G1_Smith */
  /* bsdf = D * G / (4.0 * NL * NV); /* Reference function. */
  /* NL term to fit Cycles. NOTE(fclem): Not sure what it  */
  return NL * a2 / (D * G);
}

float btdf_ggx(vec3 N, vec3 L, vec3 V, float roughness, float eta)
{
  float a2 = sqr(roughness);

  vec3 H = normalize(L + V);
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, L), 1e-8);
  float NV = max(dot(N, V), 1e-8);
  float VH = max(dot(V, H), 1e-8);
  float LH = max(dot(L, H), 1e-8);
  float Ht2 = sqr(eta * LH + VH);

  /* Doing RCP at the end */
  float G = bxdf_ggx_smith_G1_opti(NV, a2) * bxdf_ggx_smith_G1_opti(NL, a2);
  float D = bxdf_ggx_D_opti(NH, a2);

  /* btdf = abs(VH*LH) * ior^2 * D * G(V) * G(L) / (Ht2 * NV) */
  return abs(VH * LH) * sqr(eta) * 4.0 * a2 / (D * G * (Ht2 * NV));
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

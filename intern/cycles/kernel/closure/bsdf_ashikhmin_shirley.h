/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/*
 * ASHIKHMIN SHIRLEY BSDF
 *
 * Implementation of
 * Michael Ashikhmin and Peter Shirley: "An Anisotropic Phong BRDF Model" (2000)
 *
 * The Fresnel factor is missing to get a separable bsdf (intensity*color), as is
 * the case with all other microfacet-based BSDF implementations in Cycles.
 *
 * Other than that, the implementation directly follows the paper.
 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/bsdf_microfacet.h"

CCL_NAMESPACE_BEGIN

ccl_device int bsdf_ashikhmin_shirley_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = clamp(bsdf->alpha_x, 1e-4f, 1.0f);
  bsdf->alpha_y = clamp(bsdf->alpha_y, 1e-4f, 1.0f);

  bsdf->fresnel_type = MicrofacetFresnel::NONE;
  bsdf->type = CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device void bsdf_ashikhmin_shirley_blur(ccl_private ShaderClosure *sc, const float roughness)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;

  bsdf->alpha_x = fmaxf(roughness, bsdf->alpha_x);
  bsdf->alpha_y = fmaxf(roughness, bsdf->alpha_y);
}

ccl_device_inline float bsdf_ashikhmin_shirley_roughness_to_exponent(const float roughness)
{
  return 2.0f / (roughness * roughness) - 2.0f;
}

ccl_device_forceinline Spectrum bsdf_ashikhmin_shirley_eval(const ccl_private ShaderClosure *sc,
                                                            const float3 Ng,
                                                            const float3 wi,
                                                            const float3 wo,
                                                            ccl_private float *pdf)
{
  const ccl_private MicrofacetBsdf *bsdf = (const ccl_private MicrofacetBsdf *)sc;
  const float cosNgO = dot(Ng, wo);
  const float3 N = bsdf->N;

  float NdotI = dot(N, wi);
  float NdotO = dot(N, wo);

  float out = 0.0f;

  if ((cosNgO < 0.0f) || fmaxf(bsdf->alpha_x, bsdf->alpha_y) <= 1e-4f ||
      !(NdotI > 0.0f && NdotO > 0.0f))
  {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  NdotI = fmaxf(NdotI, 1e-6f);
  NdotO = fmaxf(NdotO, 1e-6f);
  const float3 H = normalize(wi + wo);
  const float HdotI = fmaxf(fabsf(dot(H, wi)), 1e-6f);
  const float HdotN = fmaxf(dot(H, N), 1e-6f);

  /* pump from original paper
   * (first derivative disc., but cancels the HdotI in the pdf nicely) */
  const float pump = 1.0f / fmaxf(1e-6f, (HdotI * fmaxf(NdotI, NdotO)));
  /* `pump` from D-BRDF paper. */
  // float pump = 1.0f / fmaxf(1e-4f, ((NdotI + NdotO) * (NdotI * NdotO)));

  const float n_x = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_x);
  const float n_y = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_y);

  if (n_x == n_y) {
    /* isotropic */
    const float e = n_x;
    const float lobe = powf(HdotN, e);
    const float norm = (n_x + 1.0f) / (8.0f * M_PI_F);

    out = NdotO * norm * lobe * pump;
    /* this is p_h / 4(H.I)  (conversion from 'wh measure' to 'wi measure', eq. 8 in paper). */
    *pdf = norm * lobe / HdotI;
  }
  else {
    /* anisotropic */
    float3 X;
    float3 Y;
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);

    const float HdotX = dot(H, X);
    const float HdotY = dot(H, Y);
    float lobe;
    if (HdotN < 1.0f) {
      const float e = (n_x * HdotX * HdotX + n_y * HdotY * HdotY) / (1.0f - HdotN * HdotN);
      lobe = powf(HdotN, e);
    }
    else {
      lobe = 1.0f;
    }
    const float norm = sqrtf((n_x + 1.0f) * (n_y + 1.0f)) / (8.0f * M_PI_F);

    out = NdotO * norm * lobe * pump;
    *pdf = norm * lobe / HdotI;
  }

  return make_spectrum(out);
}

ccl_device_inline void bsdf_ashikhmin_shirley_sample_first_quadrant(float n_x,
                                                                    const float n_y,
                                                                    const float2 rand,
                                                                    ccl_private float *phi,
                                                                    ccl_private float *cos_theta)
{
  *phi = atanf(sqrtf((n_x + 1.0f) / (n_y + 1.0f)) * tanf(M_PI_2_F * rand.x));
  const float cos_phi = cosf(*phi);
  const float sin_phi = sinf(*phi);
  *cos_theta = powf(rand.y, 1.0f / (n_x * cos_phi * cos_phi + n_y * sin_phi * sin_phi + 1.0f));
}

ccl_device int bsdf_ashikhmin_shirley_sample(const ccl_private ShaderClosure *sc,
                                             const float3 Ng,
                                             const float3 wi,
                                             float2 rand,
                                             ccl_private Spectrum *eval,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf,
                                             ccl_private float2 *sampled_roughness)
{
  const ccl_private MicrofacetBsdf *bsdf = (const ccl_private MicrofacetBsdf *)sc;
  *sampled_roughness = make_float2(bsdf->alpha_x, bsdf->alpha_y);
  const float3 N = bsdf->N;
  int label = LABEL_REFLECT | LABEL_GLOSSY;

  const float NdotI = dot(N, wi);
  if (!(NdotI > 0.0f)) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  const float n_x = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_x);
  const float n_y = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_y);

  /* get x,y basis on the surface for anisotropy */
  float3 X;
  float3 Y;

  if (n_x == n_y) {
    make_orthonormals(N, &X, &Y);
  }
  else {
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);
  }

  /* sample spherical coords for h in tangent space */
  float phi;
  float cos_theta;
  if (n_x == n_y) {
    /* isotropic sampling */
    phi = M_2PI_F * rand.x;
    cos_theta = powf(rand.y, 1.0f / (n_x + 1.0f));
  }
  else {
    /* anisotropic sampling */
    if (rand.x < 0.25f) { /* first quadrant */
      rand.x *= 4.0f;
      bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, rand, &phi, &cos_theta);
    }
    else if (rand.x < 0.5f) { /* second quadrant */
      rand.x = 4.0f * (0.5f - rand.x);
      bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, rand, &phi, &cos_theta);
      phi = M_PI_F - phi;
    }
    else if (rand.x < 0.75f) { /* third quadrant */
      rand.x = 4.0f * (rand.x - 0.5f);
      bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, rand, &phi, &cos_theta);
      phi = M_PI_F + phi;
    }
    else { /* fourth quadrant */
      rand.x = 4.0f * (1.0f - rand.x);
      bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, rand, &phi, &cos_theta);
      phi = 2.0f * M_PI_F - phi;
    }
  }

  /* get half vector in tangent space */
  const float3 h = spherical_cos_to_direction(cos_theta, phi);

  /* half vector to world space */
  float3 H = to_global(h, X, Y, N);
  const float HdotI = dot(H, wi);
  if (HdotI < 0.0f) {
    H = -H;
  }

  /* reflect wi on H to get wo */
  *wo = -wi + (2.0f * HdotI) * H;

  if (fmaxf(bsdf->alpha_x, bsdf->alpha_y) <= 1e-4f) {
    /* Some high number for MIS. */
    *pdf = 1e6f;
    *eval = make_spectrum(1e6f);
    label = LABEL_REFLECT | LABEL_SINGULAR;
  }
  else {
    /* leave the rest to eval */
    *eval = bsdf_ashikhmin_shirley_eval(sc, Ng, wi, *wo, pdf);
  }

  return label;
}

CCL_NAMESPACE_END

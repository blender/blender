/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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

CCL_NAMESPACE_BEGIN

ccl_device int bsdf_ashikhmin_shirley_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = clamp(bsdf->alpha_x, 1e-4f, 1.0f);
  bsdf->alpha_y = clamp(bsdf->alpha_y, 1e-4f, 1.0f);

  bsdf->type = CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device void bsdf_ashikhmin_shirley_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;

  bsdf->alpha_x = fmaxf(roughness, bsdf->alpha_x);
  bsdf->alpha_y = fmaxf(roughness, bsdf->alpha_y);
}

ccl_device_inline float bsdf_ashikhmin_shirley_roughness_to_exponent(float roughness)
{
  return 2.0f / (roughness * roughness) - 2.0f;
}

ccl_device_forceinline Spectrum bsdf_ashikhmin_shirley_eval(ccl_private const ShaderClosure *sc,
                                                            const float3 Ng,
                                                            const float3 wi,
                                                            const float3 wo,
                                                            ccl_private float *pdf)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  const float cosNgO = dot(Ng, wo);
  float3 N = bsdf->N;

  float NdotI = dot(N, wi);
  float NdotO = dot(N, wo);

  float out = 0.0f;

  if ((cosNgO < 0.0f) || fmaxf(bsdf->alpha_x, bsdf->alpha_y) <= 1e-4f ||
      !(NdotI > 0.0f && NdotO > 0.0f)) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  NdotI = fmaxf(NdotI, 1e-6f);
  NdotO = fmaxf(NdotO, 1e-6f);
  float3 H = normalize(wi + wo);
  float HdotI = fmaxf(fabsf(dot(H, wi)), 1e-6f);
  float HdotN = fmaxf(dot(H, N), 1e-6f);

  /* pump from original paper
   * (first derivative disc., but cancels the HdotI in the pdf nicely) */
  float pump = 1.0f / fmaxf(1e-6f, (HdotI * fmaxf(NdotI, NdotO)));
  /* pump from d-brdf paper */
  /*float pump = 1.0f / fmaxf(1e-4f, ((NdotI + NdotO) * (NdotI * NdotO))); */

  float n_x = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_x);
  float n_y = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_y);

  if (n_x == n_y) {
    /* isotropic */
    float e = n_x;
    float lobe = powf(HdotN, e);
    float norm = (n_x + 1.0f) / (8.0f * M_PI_F);

    out = NdotO * norm * lobe * pump;
    /* this is p_h / 4(H.I)  (conversion from 'wh measure' to 'wi measure', eq. 8 in paper). */
    *pdf = norm * lobe / HdotI;
  }
  else {
    /* anisotropic */
    float3 X, Y;
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);

    float HdotX = dot(H, X);
    float HdotY = dot(H, Y);
    float lobe;
    if (HdotN < 1.0f) {
      float e = (n_x * HdotX * HdotX + n_y * HdotY * HdotY) / (1.0f - HdotN * HdotN);
      lobe = powf(HdotN, e);
    }
    else {
      lobe = 1.0f;
    }
    float norm = sqrtf((n_x + 1.0f) * (n_y + 1.0f)) / (8.0f * M_PI_F);

    out = NdotO * norm * lobe * pump;
    *pdf = norm * lobe / HdotI;
  }

  return make_spectrum(out);
}

ccl_device_inline void bsdf_ashikhmin_shirley_sample_first_quadrant(float n_x,
                                                                    float n_y,
                                                                    float randu,
                                                                    float randv,
                                                                    ccl_private float *phi,
                                                                    ccl_private float *cos_theta)
{
  *phi = atanf(sqrtf((n_x + 1.0f) / (n_y + 1.0f)) * tanf(M_PI_2_F * randu));
  float cos_phi = cosf(*phi);
  float sin_phi = sinf(*phi);
  *cos_theta = powf(randv, 1.0f / (n_x * cos_phi * cos_phi + n_y * sin_phi * sin_phi + 1.0f));
}

ccl_device int bsdf_ashikhmin_shirley_sample(ccl_private const ShaderClosure *sc,
                                             float3 Ng,
                                             float3 wi,
                                             float randu,
                                             float randv,
                                             ccl_private Spectrum *eval,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf,
                                             ccl_private float2 *sampled_roughness)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  *sampled_roughness = make_float2(bsdf->alpha_x, bsdf->alpha_y);
  float3 N = bsdf->N;
  int label = LABEL_REFLECT | LABEL_GLOSSY;

  float NdotI = dot(N, wi);
  if (!(NdotI > 0.0f)) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  float n_x = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_x);
  float n_y = bsdf_ashikhmin_shirley_roughness_to_exponent(bsdf->alpha_y);

  /* get x,y basis on the surface for anisotropy */
  float3 X, Y;

  if (n_x == n_y)
    make_orthonormals(N, &X, &Y);
  else
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);

  /* sample spherical coords for h in tangent space */
  float phi;
  float cos_theta;
  if (n_x == n_y) {
    /* isotropic sampling */
    phi = M_2PI_F * randu;
    cos_theta = powf(randv, 1.0f / (n_x + 1.0f));
  }
  else {
    /* anisotropic sampling */
    if (randu < 0.25f) { /* first quadrant */
      float remapped_randu = 4.0f * randu;
      bsdf_ashikhmin_shirley_sample_first_quadrant(
          n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
    }
    else if (randu < 0.5f) { /* second quadrant */
      float remapped_randu = 4.0f * (.5f - randu);
      bsdf_ashikhmin_shirley_sample_first_quadrant(
          n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
      phi = M_PI_F - phi;
    }
    else if (randu < 0.75f) { /* third quadrant */
      float remapped_randu = 4.0f * (randu - 0.5f);
      bsdf_ashikhmin_shirley_sample_first_quadrant(
          n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
      phi = M_PI_F + phi;
    }
    else { /* fourth quadrant */
      float remapped_randu = 4.0f * (1.0f - randu);
      bsdf_ashikhmin_shirley_sample_first_quadrant(
          n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
      phi = 2.0f * M_PI_F - phi;
    }
  }

  /* get half vector in tangent space */
  float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
  float cos_phi = cosf(phi);
  float sin_phi = sinf(phi); /* no sqrt(1-cos^2) here b/c it causes artifacts */
  float3 h = make_float3(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);

  /* half vector to world space */
  float3 H = h.x * X + h.y * Y + h.z * N;
  float HdotI = dot(H, wi);
  if (HdotI < 0.0f)
    H = -H;

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
    *eval = bsdf_ashikhmin_shirley_eval(sc, N, wi, *wo, pdf);
  }

  return label;
}

CCL_NAMESPACE_END

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

typedef struct VelvetBsdf {
  SHADER_CLOSURE_BASE;

  float sigma;
  float invsigma2;
} VelvetBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(VelvetBsdf), "VelvetBsdf is too large!");

ccl_device int bsdf_ashikhmin_velvet_setup(ccl_private VelvetBsdf *bsdf)
{
  float sigma = fmaxf(bsdf->sigma, 0.01f);
  bsdf->invsigma2 = 1.0f / (sigma * sigma);

  bsdf->type = CLOSURE_BSDF_ASHIKHMIN_VELVET_ID;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_ashikhmin_velvet_eval(ccl_private const ShaderClosure *sc,
                                               const float3 wi,
                                               const float3 wo,
                                               ccl_private float *pdf)
{
  ccl_private const VelvetBsdf *bsdf = (ccl_private const VelvetBsdf *)sc;
  float m_invsigma2 = bsdf->invsigma2;
  float3 N = bsdf->N;

  float cosNI = dot(N, wi);
  float cosNO = dot(N, wo);
  if (!(cosNI > 0 && cosNO > 0)) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float3 H = normalize(wi + wo);

  float cosNH = dot(N, H);
  float cosHI = fabsf(dot(wi, H));

  if (!(fabsf(cosNH) < 1.0f - 1e-5f && cosHI > 1e-5f)) {
    *pdf = 0.0f;
    return zero_spectrum();
  }
  float cosNHdivHI = cosNH / cosHI;
  cosNHdivHI = fmaxf(cosNHdivHI, 1e-5f);

  float fac1 = 2 * fabsf(cosNHdivHI * cosNI);
  float fac2 = 2 * fabsf(cosNHdivHI * cosNO);

  float sinNH2 = 1 - cosNH * cosNH;
  float sinNH4 = sinNH2 * sinNH2;
  float cotangent2 = (cosNH * cosNH) / sinNH2;

  float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * M_1_PI_F / sinNH4;
  float G = fminf(1.0f, fminf(fac1, fac2));  // TODO: derive G from D analytically

  float out = 0.25f * (D * G) / cosNI;

  *pdf = 0.5f * M_1_PI_F;
  return make_spectrum(out);
}

ccl_device int bsdf_ashikhmin_velvet_sample(ccl_private const ShaderClosure *sc,
                                            float3 Ng,
                                            float3 wi,
                                            const float2 rand,
                                            ccl_private Spectrum *eval,
                                            ccl_private float3 *wo,
                                            ccl_private float *pdf)
{
  ccl_private const VelvetBsdf *bsdf = (ccl_private const VelvetBsdf *)sc;
  float m_invsigma2 = bsdf->invsigma2;
  float3 N = bsdf->N;

  // we are viewing the surface from above - send a ray out with uniform
  // distribution over the hemisphere
  sample_uniform_hemisphere(N, rand, wo, pdf);

  if (!(dot(Ng, *wo) > 0)) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  float3 H = normalize(wi + *wo);

  float cosNI = dot(N, wi);
  float cosNO = dot(N, *wo);
  float cosHI = fabsf(dot(wi, H));
  float cosNH = dot(N, H);

  if (!(cosNI > 1e-5f && fabsf(cosNH) < 1.0f - 1e-5f && cosHI > 1e-5f)) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  float cosNHdivHI = cosNH / cosHI;
  cosNHdivHI = fmaxf(cosNHdivHI, 1e-5f);

  float fac1 = 2 * fabsf(cosNHdivHI * cosNI);
  float fac2 = 2 * fabsf(cosNHdivHI * cosNO);

  float sinNH2 = 1 - cosNH * cosNH;
  float sinNH4 = sinNH2 * sinNH2;
  float cotangent2 = (cosNH * cosNH) / sinNH2;

  float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * M_1_PI_F / sinNH4;
  float G = fminf(1.0f, fminf(fac1, fac2));  // TODO: derive G from D analytically

  float power = 0.25f * (D * G) / cosNI;

  *eval = make_spectrum(power);

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

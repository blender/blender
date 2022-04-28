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

ccl_device float3 bsdf_ashikhmin_velvet_eval_reflect(ccl_private const ShaderClosure *sc,
                                                     const float3 I,
                                                     const float3 omega_in,
                                                     ccl_private float *pdf)
{
  ccl_private const VelvetBsdf *bsdf = (ccl_private const VelvetBsdf *)sc;
  float m_invsigma2 = bsdf->invsigma2;
  float3 N = bsdf->N;

  float cosNO = dot(N, I);
  float cosNI = dot(N, omega_in);
  if (cosNO > 0 && cosNI > 0) {
    float3 H = normalize(omega_in + I);

    float cosNH = dot(N, H);
    float cosHO = fabsf(dot(I, H));

    if (!(fabsf(cosNH) < 1.0f - 1e-5f && cosHO > 1e-5f)) {
      *pdf = 0.0f;
      return make_float3(0.0f, 0.0f, 0.0f);
    }
    float cosNHdivHO = cosNH / cosHO;
    cosNHdivHO = fmaxf(cosNHdivHO, 1e-5f);

    float fac1 = 2 * fabsf(cosNHdivHO * cosNO);
    float fac2 = 2 * fabsf(cosNHdivHO * cosNI);

    float sinNH2 = 1 - cosNH * cosNH;
    float sinNH4 = sinNH2 * sinNH2;
    float cotangent2 = (cosNH * cosNH) / sinNH2;

    float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * M_1_PI_F / sinNH4;
    float G = fminf(1.0f, fminf(fac1, fac2));  // TODO: derive G from D analytically

    float out = 0.25f * (D * G) / cosNO;

    *pdf = 0.5f * M_1_PI_F;
    return make_float3(out, out, out);
  }

  *pdf = 0.0f;
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_ashikhmin_velvet_eval_transmit(ccl_private const ShaderClosure *sc,
                                                      const float3 I,
                                                      const float3 omega_in,
                                                      ccl_private float *pdf)
{
  *pdf = 0.0f;
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_ashikhmin_velvet_sample(ccl_private const ShaderClosure *sc,
                                            float3 Ng,
                                            float3 I,
                                            float3 dIdx,
                                            float3 dIdy,
                                            float randu,
                                            float randv,
                                            ccl_private float3 *eval,
                                            ccl_private float3 *omega_in,
                                            ccl_private float3 *domega_in_dx,
                                            ccl_private float3 *domega_in_dy,
                                            ccl_private float *pdf)
{
  ccl_private const VelvetBsdf *bsdf = (ccl_private const VelvetBsdf *)sc;
  float m_invsigma2 = bsdf->invsigma2;
  float3 N = bsdf->N;

  // we are viewing the surface from above - send a ray out with uniform
  // distribution over the hemisphere
  sample_uniform_hemisphere(N, randu, randv, omega_in, pdf);

  if (dot(Ng, *omega_in) > 0) {
    float3 H = normalize(*omega_in + I);

    float cosNI = dot(N, *omega_in);
    float cosNO = dot(N, I);
    float cosNH = dot(N, H);
    float cosHO = fabsf(dot(I, H));

    if (fabsf(cosNO) > 1e-5f && fabsf(cosNH) < 1.0f - 1e-5f && cosHO > 1e-5f) {
      float cosNHdivHO = cosNH / cosHO;
      cosNHdivHO = fmaxf(cosNHdivHO, 1e-5f);

      float fac1 = 2 * fabsf(cosNHdivHO * cosNO);
      float fac2 = 2 * fabsf(cosNHdivHO * cosNI);

      float sinNH2 = 1 - cosNH * cosNH;
      float sinNH4 = sinNH2 * sinNH2;
      float cotangent2 = (cosNH * cosNH) / sinNH2;

      float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * M_1_PI_F / sinNH4;
      float G = fminf(1.0f, fminf(fac1, fac2));  // TODO: derive G from D analytically

      float power = 0.25f * (D * G) / cosNO;

      *eval = make_float3(power, power, power);

#ifdef __RAY_DIFFERENTIALS__
      // TODO: find a better approximation for the retroreflective bounce
      *domega_in_dx = (2 * dot(N, dIdx)) * N - dIdx;
      *domega_in_dy = (2 * dot(N, dIdy)) * N - dIdy;
#endif
    }
    else {
      *pdf = 0.0f;
      *eval = make_float3(0.0f, 0.0f, 0.0f);
    }
  }
  else {
    *pdf = 0.0f;
    *eval = make_float3(0.0f, 0.0f, 0.0f);
  }
  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

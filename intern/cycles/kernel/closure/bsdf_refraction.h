/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

CCL_NAMESPACE_BEGIN

/* REFRACTION */

ccl_device int bsdf_refraction_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_REFRACTION_ID;
  return SD_BSDF;
}

ccl_device float3 bsdf_refraction_eval_reflect(ccl_private const ShaderClosure *sc,
                                               const float3 I,
                                               const float3 omega_in,
                                               ccl_private float *pdf)
{
  *pdf = 0.0f;
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_refraction_eval_transmit(ccl_private const ShaderClosure *sc,
                                                const float3 I,
                                                const float3 omega_in,
                                                ccl_private float *pdf)
{
  *pdf = 0.0f;
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_refraction_sample(ccl_private const ShaderClosure *sc,
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
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  float m_eta = bsdf->ior;
  float3 N = bsdf->N;

  float3 R, T;
#ifdef __RAY_DIFFERENTIALS__
  float3 dRdx, dRdy, dTdx, dTdy;
#endif
  bool inside;
  float fresnel;
  fresnel = fresnel_dielectric(m_eta,
                               N,
                               I,
                               &R,
                               &T,
#ifdef __RAY_DIFFERENTIALS__
                               dIdx,
                               dIdy,
                               &dRdx,
                               &dRdy,
                               &dTdx,
                               &dTdy,
#endif
                               &inside);

  if (!inside && fresnel != 1.0f) {
    /* Some high number for MIS. */
    *pdf = 1e6f;
    *eval = make_float3(1e6f, 1e6f, 1e6f);
    *omega_in = T;
#ifdef __RAY_DIFFERENTIALS__
    *domega_in_dx = dTdx;
    *domega_in_dy = dTdy;
#endif
  }
  else {
    *pdf = 0.0f;
    *eval = make_float3(0.0f, 0.0f, 0.0f);
  }
  return LABEL_TRANSMIT | LABEL_SINGULAR;
}

CCL_NAMESPACE_END

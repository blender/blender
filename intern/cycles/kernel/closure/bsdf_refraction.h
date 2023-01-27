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

ccl_device Spectrum bsdf_refraction_eval(ccl_private const ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_refraction_sample(ccl_private const ShaderClosure *sc,
                                      float3 Ng,
                                      float3 wi,
                                      float randu,
                                      float randv,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf,
                                      ccl_private float *eta)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  float m_eta = bsdf->ior;

  *eta = 1.0f / m_eta;
  float3 N = bsdf->N;

  float3 R, T;
  bool inside;
  float fresnel;
  fresnel = fresnel_dielectric(m_eta, N, wi, &R, &T, &inside);

  if (!inside && fresnel != 1.0f) {
    /* Some high number for MIS. */
    *pdf = 1e6f;
    *eval = make_spectrum(1e6f);
    *wo = T;
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }
  return LABEL_TRANSMIT | LABEL_SINGULAR;
}

CCL_NAMESPACE_END

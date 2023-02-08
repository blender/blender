/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

CCL_NAMESPACE_BEGIN

/* REFLECTION */

ccl_device int bsdf_reflection_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_REFLECTION_ID;
  return SD_BSDF;
}

ccl_device Spectrum bsdf_reflection_eval(ccl_private const ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_reflection_sample(ccl_private const ShaderClosure *sc,
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
  float3 N = bsdf->N;
  *eta = bsdf->ior;

  // only one direction is possible
  float cosNI = dot(N, wi);
  if (cosNI > 0) {
    *wo = (2 * cosNI) * N - wi;
    if (dot(Ng, *wo) > 0) {
      /* Some high number for MIS. */
      *pdf = 1e6f;
      *eval = make_spectrum(1e6f);
    }
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }
  return LABEL_REFLECT | LABEL_SINGULAR;
}

CCL_NAMESPACE_END

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

ccl_device Spectrum bsdf_reflection_eval_reflect(ccl_private const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device Spectrum bsdf_reflection_eval_transmit(ccl_private const ShaderClosure *sc,
                                                  const float3 I,
                                                  const float3 omega_in,
                                                  ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_reflection_sample(ccl_private const ShaderClosure *sc,
                                      float3 Ng,
                                      float3 I,
                                      float randu,
                                      float randv,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *omega_in,
                                      ccl_private float *pdf)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  float3 N = bsdf->N;

  // only one direction is possible
  float cosNO = dot(N, I);
  if (cosNO > 0) {
    *omega_in = (2 * cosNO) * N - I;
    if (dot(Ng, *omega_in) > 0) {
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

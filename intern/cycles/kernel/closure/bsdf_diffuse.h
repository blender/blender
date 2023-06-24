/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

typedef struct DiffuseBsdf {
  SHADER_CLOSURE_BASE;
} DiffuseBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(DiffuseBsdf), "DiffuseBsdf is too large!");

/* DIFFUSE */

ccl_device int bsdf_diffuse_setup(ccl_private DiffuseBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_diffuse_eval(ccl_private const ShaderClosure *sc,
                                      const float3 wi,
                                      const float3 wo,
                                      ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  float cosNO = fmaxf(dot(N, wo), 0.0f) * M_1_PI_F;
  *pdf = cosNO;
  return make_spectrum(cosNO);
}

ccl_device int bsdf_diffuse_sample(ccl_private const ShaderClosure *sc,
                                   float3 Ng,
                                   float3 wi,
                                   float2 rand,
                                   ccl_private Spectrum *eval,
                                   ccl_private float3 *wo,
                                   ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  // distribution over the hemisphere
  sample_cos_hemisphere(N, rand, wo, pdf);

  if (dot(Ng, *wo) > 0.0f) {
    *eval = make_spectrum(*pdf);
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }
  return LABEL_REFLECT | LABEL_DIFFUSE;
}

/* TRANSLUCENT */

ccl_device int bsdf_translucent_setup(ccl_private DiffuseBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_TRANSLUCENT_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_HAS_TRANSMISSION;
}

ccl_device Spectrum bsdf_translucent_eval(ccl_private const ShaderClosure *sc,
                                          const float3 wi,
                                          const float3 wo,
                                          ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  float cosNO = fmaxf(-dot(N, wo), 0.0f) * M_1_PI_F;
  *pdf = cosNO;
  return make_spectrum(cosNO);
}

ccl_device int bsdf_translucent_sample(ccl_private const ShaderClosure *sc,
                                       float3 Ng,
                                       float3 wi,
                                       float2 rand,
                                       ccl_private Spectrum *eval,
                                       ccl_private float3 *wo,
                                       ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  // we are viewing the surface from the right side - send a ray out with cosine
  // distribution over the hemisphere
  sample_cos_hemisphere(-N, rand, wo, pdf);
  if (dot(Ng, *wo) < 0) {
    *eval = make_spectrum(*pdf);
  }
  else {
    *pdf = 0;
    *eval = zero_spectrum();
  }
  return LABEL_TRANSMIT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

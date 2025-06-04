/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/types.h"

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

struct DiffuseBsdf {
  SHADER_CLOSURE_BASE;
};

static_assert(sizeof(ShaderClosure) >= sizeof(DiffuseBsdf), "DiffuseBsdf is too large!");

/* DIFFUSE */

ccl_device int bsdf_diffuse_setup(ccl_private DiffuseBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_diffuse_eval(const ccl_private ShaderClosure *sc,
                                      const float3 wi,
                                      const float3 wo,
                                      ccl_private float *pdf)
{
  const ccl_private DiffuseBsdf *bsdf = (const ccl_private DiffuseBsdf *)sc;
  const float3 N = bsdf->N;

  const float cosNO = fmaxf(dot(N, wo), 0.0f) * M_1_PI_F;
  *pdf = cosNO;
  return make_spectrum(cosNO);
}

ccl_device int bsdf_diffuse_sample(const ccl_private ShaderClosure *sc,
                                   const float3 Ng,
                                   const float3 wi,
                                   const float2 rand,
                                   ccl_private Spectrum *eval,
                                   ccl_private float3 *wo,
                                   ccl_private float *pdf)
{
  const ccl_private DiffuseBsdf *bsdf = (const ccl_private DiffuseBsdf *)sc;
  const float3 N = bsdf->N;

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

ccl_device Spectrum bsdf_translucent_eval(const ccl_private ShaderClosure *sc,
                                          const float3 wi,
                                          const float3 wo,
                                          ccl_private float *pdf)
{
  const ccl_private DiffuseBsdf *bsdf = (const ccl_private DiffuseBsdf *)sc;
  const float3 N = bsdf->N;

  const float cosNO = fmaxf(-dot(N, wo), 0.0f) * M_1_PI_F;
  *pdf = cosNO;
  return make_spectrum(cosNO);
}

ccl_device int bsdf_translucent_sample(const ccl_private ShaderClosure *sc,
                                       const float3 Ng,
                                       const float3 wi,
                                       const float2 rand,
                                       ccl_private Spectrum *eval,
                                       ccl_private float3 *wo,
                                       ccl_private float *pdf)
{
  const ccl_private DiffuseBsdf *bsdf = (const ccl_private DiffuseBsdf *)sc;
  const float3 N = bsdf->N;

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

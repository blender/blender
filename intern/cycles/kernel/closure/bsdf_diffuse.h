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

ccl_device Spectrum bsdf_diffuse_eval_reflect(ccl_private const ShaderClosure *sc,
                                              const float3 I,
                                              const float3 omega_in,
                                              ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  float cos_pi = fmaxf(dot(N, omega_in), 0.0f) * M_1_PI_F;
  *pdf = cos_pi;
  return make_spectrum(cos_pi);
}

ccl_device Spectrum bsdf_diffuse_eval_transmit(ccl_private const ShaderClosure *sc,
                                               const float3 I,
                                               const float3 omega_in,
                                               ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_diffuse_sample(ccl_private const ShaderClosure *sc,
                                   float3 Ng,
                                   float3 I,
                                   float randu,
                                   float randv,
                                   ccl_private Spectrum *eval,
                                   ccl_private float3 *omega_in,
                                   ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  // distribution over the hemisphere
  sample_cos_hemisphere(N, randu, randv, omega_in, pdf);

  if (dot(Ng, *omega_in) > 0.0f) {
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
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_translucent_eval_reflect(ccl_private const ShaderClosure *sc,
                                                  const float3 I,
                                                  const float3 omega_in,
                                                  ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device Spectrum bsdf_translucent_eval_transmit(ccl_private const ShaderClosure *sc,
                                                   const float3 I,
                                                   const float3 omega_in,
                                                   ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  float cos_pi = fmaxf(-dot(N, omega_in), 0.0f) * M_1_PI_F;
  *pdf = cos_pi;
  return make_spectrum(cos_pi);
}

ccl_device int bsdf_translucent_sample(ccl_private const ShaderClosure *sc,
                                       float3 Ng,
                                       float3 I,
                                       float randu,
                                       float randv,
                                       ccl_private Spectrum *eval,
                                       ccl_private float3 *omega_in,
                                       ccl_private float *pdf)
{
  ccl_private const DiffuseBsdf *bsdf = (ccl_private const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  // we are viewing the surface from the right side - send a ray out with cosine
  // distribution over the hemisphere
  sample_cos_hemisphere(-N, randu, randv, omega_in, pdf);
  if (dot(Ng, *omega_in) < 0) {
    *eval = make_spectrum(*pdf);
  }
  else {
    *pdf = 0;
    *eval = zero_spectrum();
  }
  return LABEL_TRANSMIT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

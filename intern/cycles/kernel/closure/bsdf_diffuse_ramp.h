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

#ifdef __OSL__

typedef struct DiffuseRampBsdf {
  SHADER_CLOSURE_BASE;

  ccl_private float3 *colors;
} DiffuseRampBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(DiffuseRampBsdf), "DiffuseRampBsdf is too large!");

ccl_device float3 bsdf_diffuse_ramp_get_color(const float3 colors[8], float pos)
{
  int MAXCOLORS = 8;

  float npos = pos * (float)(MAXCOLORS - 1);
  int ipos = float_to_int(npos);
  if (ipos < 0)
    return colors[0];
  if (ipos >= (MAXCOLORS - 1))
    return colors[MAXCOLORS - 1];
  float offset = npos - (float)ipos;
  return colors[ipos] * (1.0f - offset) + colors[ipos + 1] * offset;
}

ccl_device int bsdf_diffuse_ramp_setup(DiffuseRampBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_RAMP_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device void bsdf_diffuse_ramp_blur(ccl_private ShaderClosure *sc, float roughness)
{
}

ccl_device float3 bsdf_diffuse_ramp_eval_reflect(ccl_private const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 ccl_private float *pdf)
{
  const DiffuseRampBsdf *bsdf = (const DiffuseRampBsdf *)sc;
  float3 N = bsdf->N;

  float cos_pi = fmaxf(dot(N, omega_in), 0.0f);
  *pdf = cos_pi * M_1_PI_F;
  return bsdf_diffuse_ramp_get_color(bsdf->colors, cos_pi) * M_1_PI_F;
}

ccl_device float3 bsdf_diffuse_ramp_eval_transmit(ccl_private const ShaderClosure *sc,
                                                  const float3 I,
                                                  const float3 omega_in,
                                                  ccl_private float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_diffuse_ramp_sample(ccl_private const ShaderClosure *sc,
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
  const DiffuseRampBsdf *bsdf = (const DiffuseRampBsdf *)sc;
  float3 N = bsdf->N;

  // distribution over the hemisphere
  sample_cos_hemisphere(N, randu, randv, omega_in, pdf);

  if (dot(Ng, *omega_in) > 0.0f) {
    *eval = bsdf_diffuse_ramp_get_color(bsdf->colors, *pdf * M_PI_F) * M_1_PI_F;
#  ifdef __RAY_DIFFERENTIALS__
    *domega_in_dx = (2 * dot(N, dIdx)) * N - dIdx;
    *domega_in_dy = (2 * dot(N, dIdy)) * N - dIdy;
#  endif
  }
  else {
    *pdf = 0.0f;
    *eval = make_float3(0.0f, 0.0f, 0.0f);
  }
  return LABEL_REFLECT | LABEL_DIFFUSE;
}

#endif /* __OSL__ */

CCL_NAMESPACE_END

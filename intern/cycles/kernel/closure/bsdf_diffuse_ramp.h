/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/types.h"

#include "kernel/sample/mapping.h"
#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

#ifdef __OSL__

struct DiffuseRampBsdf {
  SHADER_CLOSURE_BASE;

  ccl_private float3 *colors;
};

static_assert(sizeof(ShaderClosure) >= sizeof(DiffuseRampBsdf), "DiffuseRampBsdf is too large!");

ccl_device float3 bsdf_diffuse_ramp_get_color(const float3 colors[8], float pos)
{
  const int MAXCOLORS = 8;

  const float npos = pos * (float)(MAXCOLORS - 1);
  const int ipos = float_to_int(npos);
  if (ipos < 0) {
    return colors[0];
  }
  if (ipos >= (MAXCOLORS - 1)) {
    return colors[MAXCOLORS - 1];
  }
  const float offset = npos - (float)ipos;
  return colors[ipos] * (1.0f - offset) + colors[ipos + 1] * offset;
}

ccl_device int bsdf_diffuse_ramp_setup(DiffuseRampBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_RAMP_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device void bsdf_diffuse_ramp_blur(ccl_private ShaderClosure *sc, const float roughness) {}

ccl_device Spectrum bsdf_diffuse_ramp_eval(const ccl_private ShaderClosure *sc,
                                           const float3 wi,
                                           const float3 wo,
                                           ccl_private float *pdf)
{
  const DiffuseRampBsdf *bsdf = (const DiffuseRampBsdf *)sc;
  const float3 N = bsdf->N;

  const float cosNO = fmaxf(dot(N, wo), 0.0f);
  if (cosNO >= 0.0f) {
    *pdf = cosNO * M_1_PI_F;
    return rgb_to_spectrum(bsdf_diffuse_ramp_get_color(bsdf->colors, cosNO) * M_1_PI_F);
  }
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_diffuse_ramp_sample(const ccl_private ShaderClosure *sc,
                                        const float3 Ng,
                                        const float3 wi,
                                        const float2 rand,
                                        ccl_private Spectrum *eval,
                                        ccl_private float3 *wo,
                                        ccl_private float *pdf)
{
  const DiffuseRampBsdf *bsdf = (const DiffuseRampBsdf *)sc;
  const float3 N = bsdf->N;

  // distribution over the hemisphere
  sample_cos_hemisphere(N, rand, wo, pdf);

  if (dot(Ng, *wo) > 0.0f) {
    *eval = rgb_to_spectrum(bsdf_diffuse_ramp_get_color(bsdf->colors, *pdf * M_PI_F) * M_1_PI_F);
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }
  return LABEL_REFLECT | LABEL_DIFFUSE;
}

#endif /* __OSL__ */

CCL_NAMESPACE_END

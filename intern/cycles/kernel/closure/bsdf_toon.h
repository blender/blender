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

struct ToonBsdf {
  SHADER_CLOSURE_BASE;

  float size;
  float smooth;
};

static_assert(sizeof(ShaderClosure) >= sizeof(ToonBsdf), "ToonBsdf is too large!");

ccl_device_inline int bsdf_toon_setup_common(ccl_private ToonBsdf *bsdf)
{
  bsdf->size = clamp(bsdf->size, 1e-5f, 1.0f) * M_PI_2_F;
  bsdf->smooth = saturatef(bsdf->smooth) * M_PI_2_F;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

/* DIFFUSE TOON */

ccl_device int bsdf_diffuse_toon_setup(ccl_private ToonBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_TOON_ID;
  return bsdf_toon_setup_common(bsdf);
}

ccl_device float bsdf_toon_get_intensity(const float max_angle,
                                         const float smooth,
                                         const float angle)
{
  float is;

  if (angle < max_angle) {
    is = 1.0f;
  }
  else if (angle < (max_angle + smooth) && smooth != 0.0f) {
    is = (1.0f - (angle - max_angle) / smooth);
  }
  else {
    is = 0.0f;
  }

  return is;
}

ccl_device float bsdf_toon_get_sample_angle(const float max_angle, const float smooth)
{
  return fminf(max_angle + smooth, M_PI_2_F);
}

ccl_device Spectrum bsdf_diffuse_toon_eval(const ccl_private ShaderClosure *sc,
                                           const float3 wi,
                                           const float3 wo,
                                           ccl_private float *pdf)
{
  const ccl_private ToonBsdf *bsdf = (const ccl_private ToonBsdf *)sc;
  const float max_angle = bsdf->size;
  const float smooth = bsdf->smooth;
  const float cosNO = dot(bsdf->N, wo);

  if (cosNO >= 0.0f) {
    const float angle = safe_acosf(fmaxf(cosNO, 0.0f));
    const float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);

    if (angle < sample_angle) {
      const float eval = bsdf_toon_get_intensity(max_angle, smooth, angle);
      *pdf = M_1_2PI_F / one_minus_cos(sample_angle);
      return make_spectrum(*pdf * eval);
    }
  }

  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_diffuse_toon_sample(const ccl_private ShaderClosure *sc,
                                        const float3 Ng,
                                        const float3 wi,
                                        const float2 rand,
                                        ccl_private Spectrum *eval,
                                        ccl_private float3 *wo,
                                        ccl_private float *pdf)
{
  const ccl_private ToonBsdf *bsdf = (const ccl_private ToonBsdf *)sc;
  const float max_angle = bsdf->size;
  const float smooth = bsdf->smooth;
  const float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);

  float cosNO;
  *wo = sample_uniform_cone(bsdf->N, one_minus_cos(sample_angle), rand, &cosNO, pdf);

  if (dot(Ng, *wo) > 0.0f) {
    const float angle = acosf(cosNO);
    *eval = make_spectrum(*pdf * bsdf_toon_get_intensity(max_angle, smooth, angle));
    return LABEL_REFLECT | LABEL_DIFFUSE;
  }

  *pdf = 0.0f;
  *eval = zero_spectrum();
  return LABEL_NONE;
}

/* GLOSSY TOON */

ccl_device int bsdf_glossy_toon_setup(ccl_private ToonBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_GLOSSY_TOON_ID;
  return bsdf_toon_setup_common(bsdf);
}

ccl_device Spectrum bsdf_glossy_toon_eval(const ccl_private ShaderClosure *sc,
                                          const float3 wi,
                                          const float3 wo,
                                          ccl_private float *pdf)
{
  const ccl_private ToonBsdf *bsdf = (const ccl_private ToonBsdf *)sc;
  const float max_angle = bsdf->size;
  const float smooth = bsdf->smooth;
  const float cosNI = dot(bsdf->N, wi);
  const float cosNO = dot(bsdf->N, wo);

  if (cosNI > 0 && cosNO > 0) {
    /* reflect the view vector */
    const float3 R = (2 * cosNI) * bsdf->N - wi;
    const float cosRO = dot(R, wo);

    const float angle = safe_acosf(fmaxf(cosRO, 0.0f));
    const float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);

    if (angle < sample_angle) {
      const float eval = bsdf_toon_get_intensity(max_angle, smooth, angle);
      *pdf = M_1_2PI_F / one_minus_cos(sample_angle);
      return make_spectrum(*pdf * eval);
    }
  }
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_glossy_toon_sample(const ccl_private ShaderClosure *sc,
                                       const float3 Ng,
                                       const float3 wi,
                                       const float2 rand,
                                       ccl_private Spectrum *eval,
                                       ccl_private float3 *wo,
                                       ccl_private float *pdf)
{
  const ccl_private ToonBsdf *bsdf = (const ccl_private ToonBsdf *)sc;
  const float max_angle = bsdf->size;
  const float smooth = bsdf->smooth;
  const float cosNI = dot(bsdf->N, wi);

  if (cosNI > 0) {
    /* reflect the view vector */
    const float3 R = (2 * cosNI) * bsdf->N - wi;

    const float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);

    float cosRO;
    *wo = sample_uniform_cone(R, one_minus_cos(sample_angle), rand, &cosRO, pdf);

    /* make sure the direction we chose is still in the right hemisphere */
    if (dot(Ng, *wo) > 0.0f && dot(bsdf->N, *wo) > 0.0f) {
      const float angle = acosf(cosRO);
      *eval = make_spectrum(*pdf * bsdf_toon_get_intensity(max_angle, smooth, angle));
      return LABEL_GLOSSY | LABEL_REFLECT;
    }
  }

  *pdf = 0.0f;
  *eval = zero_spectrum();
  return LABEL_NONE;
}

CCL_NAMESPACE_END

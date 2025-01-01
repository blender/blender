/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/types.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

#ifdef __OSL__

struct PhongRampBsdf {
  SHADER_CLOSURE_BASE;

  float exponent;
  ccl_private float3 *colors;
};

static_assert(sizeof(ShaderClosure) >= sizeof(PhongRampBsdf), "PhongRampBsdf is too large!");

ccl_device float3 bsdf_phong_ramp_get_color(const float3 colors[8], float pos)
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

ccl_device int bsdf_phong_ramp_setup(ccl_private PhongRampBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_PHONG_RAMP_ID;
  bsdf->exponent = max(bsdf->exponent, 0.0f);
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_phong_ramp_eval(const ccl_private ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  const ccl_private PhongRampBsdf *bsdf = (const ccl_private PhongRampBsdf *)sc;
  const float m_exponent = bsdf->exponent;
  const float cosNI = dot(bsdf->N, wi);
  const float cosNO = dot(bsdf->N, wo);

  if (cosNI > 0 && cosNO > 0) {
    // reflect the view vector
    const float3 R = (2 * cosNI) * bsdf->N - wi;
    const float cosRO = dot(R, wo);
    if (cosRO > 0) {
      const float cosp = powf(cosRO, m_exponent);
      const float common = 0.5f * M_1_PI_F * cosp;
      const float out = cosNO * (m_exponent + 2) * common;
      *pdf = (m_exponent + 1) * common;
      return rgb_to_spectrum(bsdf_phong_ramp_get_color(bsdf->colors, cosp) * out);
    }
  }
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device_inline float phong_ramp_exponent_to_roughness(const float exponent)
{
  return sqrt(1.0f / ((exponent + 2.0f) / 2.0f));
}

ccl_device int bsdf_phong_ramp_sample(const ccl_private ShaderClosure *sc,
                                      const float3 Ng,
                                      const float3 wi,
                                      const float2 rand,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf,
                                      ccl_private float2 *sampled_roughness)
{
  const ccl_private PhongRampBsdf *bsdf = (const ccl_private PhongRampBsdf *)sc;
  const float cosNI = dot(bsdf->N, wi);
  const float m_exponent = bsdf->exponent;
  const float m_roughness = phong_ramp_exponent_to_roughness(m_exponent);
  *sampled_roughness = make_float2(m_roughness, m_roughness);

  if (cosNI > 0) {
    // reflect the view vector
    const float3 R = (2 * cosNI) * bsdf->N - wi;
    float3 T;
    float3 B;
    make_orthonormals(R, &T, &B);
    const float phi = M_2PI_F * rand.x;
    const float cosTheta = powf(rand.y, 1 / (m_exponent + 1));
    *wo = to_global(spherical_cos_to_direction(cosTheta, phi), T, B, R);
    if (dot(Ng, *wo) > 0.0f) {
      // common terms for pdf and eval
      const float cosNO = dot(bsdf->N, *wo);
      // make sure the direction we chose is still in the right hemisphere
      if (cosNO > 0) {
        const float cosp = powf(cosTheta, m_exponent);
        const float common = 0.5f * M_1_PI_F * cosp;
        *pdf = (m_exponent + 1) * common;
        const float out = cosNO * (m_exponent + 2) * common;
        *eval = rgb_to_spectrum(bsdf_phong_ramp_get_color(bsdf->colors, cosp) * out);
      }
    }
  }
  else {
    *eval = zero_spectrum();
    *pdf = 0.0f;
  }
  return LABEL_REFLECT | LABEL_GLOSSY;
}

#endif /* __OSL__ */

CCL_NAMESPACE_END

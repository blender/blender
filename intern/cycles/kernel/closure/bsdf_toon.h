/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BSDF_TOON_H__
#define __BSDF_TOON_H__

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct ToonBsdf {
  SHADER_CLOSURE_BASE;

  float size;
  float smooth;
} ToonBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(ToonBsdf), "ToonBsdf is too large!");

/* DIFFUSE TOON */

ccl_device int bsdf_diffuse_toon_setup(ToonBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_TOON_ID;
  bsdf->size = saturate(bsdf->size);
  bsdf->smooth = saturate(bsdf->smooth);

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device bool bsdf_toon_merge(const ShaderClosure *a, const ShaderClosure *b)
{
  const ToonBsdf *bsdf_a = (const ToonBsdf *)a;
  const ToonBsdf *bsdf_b = (const ToonBsdf *)b;

  return (isequal_float3(bsdf_a->N, bsdf_b->N)) && (bsdf_a->size == bsdf_b->size) &&
         (bsdf_a->smooth == bsdf_b->smooth);
}

ccl_device float3 bsdf_toon_get_intensity(float max_angle, float smooth, float angle)
{
  float is;

  if (angle < max_angle)
    is = 1.0f;
  else if (angle < (max_angle + smooth) && smooth != 0.0f)
    is = (1.0f - (angle - max_angle) / smooth);
  else
    is = 0.0f;

  return make_float3(is, is, is);
}

ccl_device float bsdf_toon_get_sample_angle(float max_angle, float smooth)
{
  return fminf(max_angle + smooth, M_PI_2_F);
}

ccl_device float3 bsdf_diffuse_toon_eval_reflect(const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 float *pdf)
{
  const ToonBsdf *bsdf = (const ToonBsdf *)sc;
  float max_angle = bsdf->size * M_PI_2_F;
  float smooth = bsdf->smooth * M_PI_2_F;
  float angle = safe_acosf(fmaxf(dot(bsdf->N, omega_in), 0.0f));

  float3 eval = bsdf_toon_get_intensity(max_angle, smooth, angle);

  if (eval.x > 0.0f) {
    float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);

    *pdf = 0.5f * M_1_PI_F / (1.0f - cosf(sample_angle));
    return *pdf * eval;
  }

  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_diffuse_toon_eval_transmit(const ShaderClosure *sc,
                                                  const float3 I,
                                                  const float3 omega_in,
                                                  float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_diffuse_toon_sample(const ShaderClosure *sc,
                                        float3 Ng,
                                        float3 I,
                                        float3 dIdx,
                                        float3 dIdy,
                                        float randu,
                                        float randv,
                                        float3 *eval,
                                        float3 *omega_in,
                                        float3 *domega_in_dx,
                                        float3 *domega_in_dy,
                                        float *pdf)
{
  const ToonBsdf *bsdf = (const ToonBsdf *)sc;
  float max_angle = bsdf->size * M_PI_2_F;
  float smooth = bsdf->smooth * M_PI_2_F;
  float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);
  float angle = sample_angle * randu;

  if (sample_angle > 0.0f) {
    sample_uniform_cone(bsdf->N, sample_angle, randu, randv, omega_in, pdf);

    if (dot(Ng, *omega_in) > 0.0f) {
      *eval = *pdf * bsdf_toon_get_intensity(max_angle, smooth, angle);

#ifdef __RAY_DIFFERENTIALS__
      // TODO: find a better approximation for the bounce
      *domega_in_dx = (2.0f * dot(bsdf->N, dIdx)) * bsdf->N - dIdx;
      *domega_in_dy = (2.0f * dot(bsdf->N, dIdy)) * bsdf->N - dIdy;
#endif
    }
    else
      *pdf = 0.0f;
  }

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

/* GLOSSY TOON */

ccl_device int bsdf_glossy_toon_setup(ToonBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_GLOSSY_TOON_ID;
  bsdf->size = saturate(bsdf->size);
  bsdf->smooth = saturate(bsdf->smooth);

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device float3 bsdf_glossy_toon_eval_reflect(const ShaderClosure *sc,
                                                const float3 I,
                                                const float3 omega_in,
                                                float *pdf)
{
  const ToonBsdf *bsdf = (const ToonBsdf *)sc;
  float max_angle = bsdf->size * M_PI_2_F;
  float smooth = bsdf->smooth * M_PI_2_F;
  float cosNI = dot(bsdf->N, omega_in);
  float cosNO = dot(bsdf->N, I);

  if (cosNI > 0 && cosNO > 0) {
    /* reflect the view vector */
    float3 R = (2 * cosNO) * bsdf->N - I;
    float cosRI = dot(R, omega_in);

    float angle = safe_acosf(fmaxf(cosRI, 0.0f));

    float3 eval = bsdf_toon_get_intensity(max_angle, smooth, angle);
    float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);

    *pdf = 0.5f * M_1_PI_F / (1.0f - cosf(sample_angle));
    return *pdf * eval;
  }

  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_glossy_toon_eval_transmit(const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_glossy_toon_sample(const ShaderClosure *sc,
                                       float3 Ng,
                                       float3 I,
                                       float3 dIdx,
                                       float3 dIdy,
                                       float randu,
                                       float randv,
                                       float3 *eval,
                                       float3 *omega_in,
                                       float3 *domega_in_dx,
                                       float3 *domega_in_dy,
                                       float *pdf)
{
  const ToonBsdf *bsdf = (const ToonBsdf *)sc;
  float max_angle = bsdf->size * M_PI_2_F;
  float smooth = bsdf->smooth * M_PI_2_F;
  float cosNO = dot(bsdf->N, I);

  if (cosNO > 0) {
    /* reflect the view vector */
    float3 R = (2 * cosNO) * bsdf->N - I;

    float sample_angle = bsdf_toon_get_sample_angle(max_angle, smooth);
    float angle = sample_angle * randu;

    sample_uniform_cone(R, sample_angle, randu, randv, omega_in, pdf);

    if (dot(Ng, *omega_in) > 0.0f) {
      float cosNI = dot(bsdf->N, *omega_in);

      /* make sure the direction we chose is still in the right hemisphere */
      if (cosNI > 0) {
        *eval = *pdf * bsdf_toon_get_intensity(max_angle, smooth, angle);

#ifdef __RAY_DIFFERENTIALS__
        *domega_in_dx = (2 * dot(bsdf->N, dIdx)) * bsdf->N - dIdx;
        *domega_in_dy = (2 * dot(bsdf->N, dIdy)) * bsdf->N - dIdy;
#endif
      }
      else
        *pdf = 0.0f;
    }
    else
      *pdf = 0.0f;
  }

  return LABEL_GLOSSY | LABEL_REFLECT;
}

CCL_NAMESPACE_END

#endif /* __BSDF_TOON_H__ */

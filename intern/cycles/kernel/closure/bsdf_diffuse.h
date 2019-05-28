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

#ifndef __BSDF_DIFFUSE_H__
#define __BSDF_DIFFUSE_H__

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct DiffuseBsdf {
  SHADER_CLOSURE_BASE;
} DiffuseBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(DiffuseBsdf), "DiffuseBsdf is too large!");

/* DIFFUSE */

ccl_device int bsdf_diffuse_setup(DiffuseBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_DIFFUSE_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device bool bsdf_diffuse_merge(const ShaderClosure *a, const ShaderClosure *b)
{
  const DiffuseBsdf *bsdf_a = (const DiffuseBsdf *)a;
  const DiffuseBsdf *bsdf_b = (const DiffuseBsdf *)b;

  return (isequal_float3(bsdf_a->N, bsdf_b->N));
}

ccl_device float3 bsdf_diffuse_eval_reflect(const ShaderClosure *sc,
                                            const float3 I,
                                            const float3 omega_in,
                                            float *pdf)
{
  const DiffuseBsdf *bsdf = (const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  float cos_pi = fmaxf(dot(N, omega_in), 0.0f) * M_1_PI_F;
  *pdf = cos_pi;
  return make_float3(cos_pi, cos_pi, cos_pi);
}

ccl_device float3 bsdf_diffuse_eval_transmit(const ShaderClosure *sc,
                                             const float3 I,
                                             const float3 omega_in,
                                             float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_diffuse_sample(const ShaderClosure *sc,
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
  const DiffuseBsdf *bsdf = (const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  // distribution over the hemisphere
  sample_cos_hemisphere(N, randu, randv, omega_in, pdf);

  if (dot(Ng, *omega_in) > 0.0f) {
    *eval = make_float3(*pdf, *pdf, *pdf);
#ifdef __RAY_DIFFERENTIALS__
    // TODO: find a better approximation for the diffuse bounce
    *domega_in_dx = (2 * dot(N, dIdx)) * N - dIdx;
    *domega_in_dy = (2 * dot(N, dIdy)) * N - dIdy;
#endif
  }
  else
    *pdf = 0.0f;

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

/* TRANSLUCENT */

ccl_device int bsdf_translucent_setup(DiffuseBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_TRANSLUCENT_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device float3 bsdf_translucent_eval_reflect(const ShaderClosure *sc,
                                                const float3 I,
                                                const float3 omega_in,
                                                float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_translucent_eval_transmit(const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 float *pdf)
{
  const DiffuseBsdf *bsdf = (const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  float cos_pi = fmaxf(-dot(N, omega_in), 0.0f) * M_1_PI_F;
  *pdf = cos_pi;
  return make_float3(cos_pi, cos_pi, cos_pi);
}

ccl_device int bsdf_translucent_sample(const ShaderClosure *sc,
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
  const DiffuseBsdf *bsdf = (const DiffuseBsdf *)sc;
  float3 N = bsdf->N;

  // we are viewing the surface from the right side - send a ray out with cosine
  // distribution over the hemisphere
  sample_cos_hemisphere(-N, randu, randv, omega_in, pdf);
  if (dot(Ng, *omega_in) < 0) {
    *eval = make_float3(*pdf, *pdf, *pdf);
#ifdef __RAY_DIFFERENTIALS__
    // TODO: find a better approximation for the diffuse bounce
    *domega_in_dx = -((2 * dot(N, dIdx)) * N - dIdx);
    *domega_in_dy = -((2 * dot(N, dIdy)) * N - dIdy);
#endif
  }
  else {
    *pdf = 0;
  }
  return LABEL_TRANSMIT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

#endif /* __BSDF_DIFFUSE_H__ */

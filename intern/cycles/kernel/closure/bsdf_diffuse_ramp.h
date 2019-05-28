/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2012, Blender Foundation.
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

#ifndef __BSDF_DIFFUSE_RAMP_H__
#define __BSDF_DIFFUSE_RAMP_H__

CCL_NAMESPACE_BEGIN

#ifdef __OSL__

typedef ccl_addr_space struct DiffuseRampBsdf {
  SHADER_CLOSURE_BASE;

  float3 *colors;
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

ccl_device void bsdf_diffuse_ramp_blur(ShaderClosure *sc, float roughness)
{
}

ccl_device float3 bsdf_diffuse_ramp_eval_reflect(const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 float *pdf)
{
  const DiffuseRampBsdf *bsdf = (const DiffuseRampBsdf *)sc;
  float3 N = bsdf->N;

  float cos_pi = fmaxf(dot(N, omega_in), 0.0f);
  *pdf = cos_pi * M_1_PI_F;
  return bsdf_diffuse_ramp_get_color(bsdf->colors, cos_pi) * M_1_PI_F;
}

ccl_device float3 bsdf_diffuse_ramp_eval_transmit(const ShaderClosure *sc,
                                                  const float3 I,
                                                  const float3 omega_in,
                                                  float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_diffuse_ramp_sample(const ShaderClosure *sc,
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
  else
    *pdf = 0.0f;

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

#endif /* __OSL__ */

CCL_NAMESPACE_END

#endif /* __BSDF_DIFFUSE_RAMP_H__ */

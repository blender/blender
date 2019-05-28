/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BSDF_OREN_NAYAR_H__
#define __BSDF_OREN_NAYAR_H__

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct OrenNayarBsdf {
  SHADER_CLOSURE_BASE;

  float roughness;
  float a;
  float b;
} OrenNayarBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(OrenNayarBsdf), "OrenNayarBsdf is too large!");

ccl_device float3 bsdf_oren_nayar_get_intensity(const ShaderClosure *sc,
                                                float3 n,
                                                float3 v,
                                                float3 l)
{
  const OrenNayarBsdf *bsdf = (const OrenNayarBsdf *)sc;
  float nl = max(dot(n, l), 0.0f);
  float nv = max(dot(n, v), 0.0f);
  float t = dot(l, v) - nl * nv;

  if (t > 0.0f)
    t /= max(nl, nv) + FLT_MIN;
  float is = nl * (bsdf->a + bsdf->b * t);
  return make_float3(is, is, is);
}

ccl_device int bsdf_oren_nayar_setup(OrenNayarBsdf *bsdf)
{
  float sigma = bsdf->roughness;

  bsdf->type = CLOSURE_BSDF_OREN_NAYAR_ID;

  sigma = saturate(sigma);

  float div = 1.0f / (M_PI_F + ((3.0f * M_PI_F - 4.0f) / 6.0f) * sigma);

  bsdf->a = 1.0f * div;
  bsdf->b = sigma * div;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device bool bsdf_oren_nayar_merge(const ShaderClosure *a, const ShaderClosure *b)
{
  const OrenNayarBsdf *bsdf_a = (const OrenNayarBsdf *)a;
  const OrenNayarBsdf *bsdf_b = (const OrenNayarBsdf *)b;

  return (isequal_float3(bsdf_a->N, bsdf_b->N)) && (bsdf_a->roughness == bsdf_b->roughness);
}

ccl_device float3 bsdf_oren_nayar_eval_reflect(const ShaderClosure *sc,
                                               const float3 I,
                                               const float3 omega_in,
                                               float *pdf)
{
  const OrenNayarBsdf *bsdf = (const OrenNayarBsdf *)sc;
  if (dot(bsdf->N, omega_in) > 0.0f) {
    *pdf = 0.5f * M_1_PI_F;
    return bsdf_oren_nayar_get_intensity(sc, bsdf->N, I, omega_in);
  }
  else {
    *pdf = 0.0f;
    return make_float3(0.0f, 0.0f, 0.0f);
  }
}

ccl_device float3 bsdf_oren_nayar_eval_transmit(const ShaderClosure *sc,
                                                const float3 I,
                                                const float3 omega_in,
                                                float *pdf)
{
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_oren_nayar_sample(const ShaderClosure *sc,
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
  const OrenNayarBsdf *bsdf = (const OrenNayarBsdf *)sc;
  sample_uniform_hemisphere(bsdf->N, randu, randv, omega_in, pdf);

  if (dot(Ng, *omega_in) > 0.0f) {
    *eval = bsdf_oren_nayar_get_intensity(sc, bsdf->N, I, *omega_in);

#ifdef __RAY_DIFFERENTIALS__
    // TODO: find a better approximation for the bounce
    *domega_in_dx = (2.0f * dot(bsdf->N, dIdx)) * bsdf->N - dIdx;
    *domega_in_dy = (2.0f * dot(bsdf->N, dIdy)) * bsdf->N - dIdy;
#endif
  }
  else {
    *pdf = 0.0f;
    *eval = make_float3(0.0f, 0.0f, 0.0f);
  }

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

#endif /* __BSDF_OREN_NAYAR_H__ */

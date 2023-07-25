/* SPDX-FileCopyrightText: Copyright 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Shading model by Tizian Zeltner, Brent Burley, Matt Jen-Yuan Chiang:
 * "Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines" (2022)
 * https://tizianzeltner.com/projects/Zeltner2022Practical/
 */

#include "kernel/closure/bsdf_util.h"

CCL_NAMESPACE_BEGIN

typedef struct SheenBsdf {
  SHADER_CLOSURE_BASE;
  float roughness;
  float transformA, transformB;
  float3 T, B;
} SheenBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(SheenBsdf), "SheenBsdf is too large!");

ccl_device int bsdf_sheen_setup(KernelGlobals kg,
                                ccl_private const ShaderData *sd,
                                ccl_private SheenBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_SHEEN_ID;

  bsdf->roughness = clamp(bsdf->roughness, 1e-3f, 1.0f);
  make_orthonormals_tangent(bsdf->N, sd->wi, &bsdf->T, &bsdf->B);
  float cosNI = dot(bsdf->N, sd->wi);

  int offset = kernel_data.tables.sheen_ltc;
  bsdf->transformA = lookup_table_read_2D(kg, cosNI, bsdf->roughness, offset, 32, 32);
  bsdf->transformB = lookup_table_read_2D(kg, cosNI, bsdf->roughness, offset + 32 * 32, 32, 32);
  float albedo = lookup_table_read_2D(kg, cosNI, bsdf->roughness, offset + 2 * 32 * 32, 32, 32);

  bsdf->weight *= albedo;
  bsdf->sample_weight *= albedo;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_sheen_eval(ccl_private const ShaderClosure *sc,
                                    const float3 wi,
                                    const float3 wo,
                                    ccl_private float *pdf)
{
  ccl_private const SheenBsdf *bsdf = (ccl_private const SheenBsdf *)sc;
  const float3 N = bsdf->N, T = bsdf->T, B = bsdf->B;
  float a = bsdf->transformA, b = bsdf->transformB;

  if (dot(N, wo) <= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float3 localO = make_float3(dot(T, wo), dot(B, wo), dot(N, wo));
  if (localO.z <= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float lenSqr = sqr(a * localO.x + b * localO.z) + sqr(a * localO.y) + sqr(localO.z);
  float val = M_1_PI_F * localO.z * sqr(a / lenSqr);

  *pdf = val;
  return make_spectrum(val);
}

ccl_device int bsdf_sheen_sample(ccl_private const ShaderClosure *sc,
                                 float3 Ng,
                                 float3 wi,
                                 float2 rand,
                                 ccl_private Spectrum *eval,
                                 ccl_private float3 *wo,
                                 ccl_private float *pdf)
{
  ccl_private const SheenBsdf *bsdf = (ccl_private const SheenBsdf *)sc;
  const float3 N = bsdf->N, T = bsdf->T, B = bsdf->B;
  float a = bsdf->transformA, b = bsdf->transformB;

  float2 disk = concentric_sample_disk(rand);
  float diskZ = safe_sqrtf(1.0f - dot(disk, disk));
  float3 localO = normalize(make_float3((disk.x - diskZ * b) / a, disk.y / a, diskZ));

  *wo = localO.x * T + localO.y * B + localO.z * N;

  if (dot(Ng, *wo) <= 0) {
    *eval = zero_spectrum();
    *pdf = 0.0f;
    return LABEL_REFLECT | LABEL_DIFFUSE;
  }

  float lenSqr = sqr(a * localO.x + b * localO.z) + sqr(a * localO.y) + sqr(localO.z);
  float val = M_1_PI_F * localO.z * sqr(a / lenSqr);
  *pdf = val;
  *eval = make_spectrum(val);

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

/* SPDX-FileCopyrightText: Copyright 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Shading model by Tizian Zeltner, Brent Burley, Matt Jen-Yuan Chiang:
 * "Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines" (2022)
 * https://tizianzeltner.com/projects/Zeltner2022Practical/
 */

#include "kernel/sample/mapping.h"

#include "kernel/util/lookup_table.h"

CCL_NAMESPACE_BEGIN

struct SheenBsdf {
  SHADER_CLOSURE_BASE;
  float roughness;
  float transformA, transformB;
  float3 T, B;
};

static_assert(sizeof(ShaderClosure) >= sizeof(SheenBsdf), "SheenBsdf is too large!");

ccl_device int bsdf_sheen_setup(KernelGlobals kg,
                                const ccl_private ShaderData *sd,
                                ccl_private SheenBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_SHEEN_ID;

  bsdf->roughness = clamp(bsdf->roughness, 1e-3f, 1.0f);
  make_orthonormals_safe_tangent(bsdf->N, sd->wi, &bsdf->T, &bsdf->B);
  const float cosNI = dot(bsdf->N, sd->wi);

  const int offset = kernel_data.tables.sheen_ltc;
  bsdf->transformA = lookup_table_read_2D(kg, cosNI, bsdf->roughness, offset, 32, 32);
  bsdf->transformB = lookup_table_read_2D(kg, cosNI, bsdf->roughness, offset + 32 * 32, 32, 32);
  const float albedo = lookup_table_read_2D(
      kg, cosNI, bsdf->roughness, offset + 2 * 32 * 32, 32, 32);

  /* If the given roughness and angle result in an invalid LTC, skip the closure. */
  if (fabsf(bsdf->transformA) < 1e-5f || albedo < 1e-5f) {
    bsdf->type = CLOSURE_NONE_ID;
    bsdf->sample_weight = 0.0f;
    return 0;
  }

  bsdf->weight *= albedo;
  bsdf->sample_weight *= albedo;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_sheen_eval(const ccl_private ShaderClosure *sc,
                                    const float3 wi,
                                    const float3 wo,
                                    ccl_private float *pdf)
{
  const ccl_private SheenBsdf *bsdf = (const ccl_private SheenBsdf *)sc;
  const float3 N = bsdf->N;
  const float3 T = bsdf->T;
  const float3 B = bsdf->B;
  const float a = bsdf->transformA;
  const float b = bsdf->transformB;

  if (dot(N, wo) <= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float3 localO = to_local(wo, T, B, N);
  if (localO.z <= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float lenSqr = sqr(a * localO.x + b * localO.z) + sqr(a * localO.y) + sqr(localO.z);
  const float val = M_1_PI_F * localO.z * sqr(a / lenSqr);

  *pdf = val;
  return make_spectrum(val);
}

ccl_device int bsdf_sheen_sample(const ccl_private ShaderClosure *sc,
                                 const float3 Ng,
                                 const float3 wi,
                                 const float2 rand,
                                 ccl_private Spectrum *eval,
                                 ccl_private float3 *wo,
                                 ccl_private float *pdf)
{
  const ccl_private SheenBsdf *bsdf = (const ccl_private SheenBsdf *)sc;
  const float3 N = bsdf->N;
  const float3 T = bsdf->T;
  const float3 B = bsdf->B;
  const float a = bsdf->transformA;
  const float b = bsdf->transformB;

  const float2 disk = sample_uniform_disk(rand);
  const float diskZ = safe_sqrtf(1.0f - dot(disk, disk));
  const float3 localO = normalize(make_float3((disk.x - diskZ * b), disk.y, diskZ * a));

  *wo = to_global(localO, T, B, N);

  if (dot(Ng, *wo) <= 0) {
    *eval = zero_spectrum();
    *pdf = 0.0f;
    return LABEL_REFLECT | LABEL_DIFFUSE;
  }

  const float lenSqr = sqr(a * localO.x + b * localO.z) + sqr(a * localO.y) + sqr(localO.z);
  const float val = M_1_PI_F * localO.z * sqr(a / lenSqr);
  *pdf = val;
  *eval = make_spectrum(val);

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

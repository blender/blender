/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/alloc.h"

CCL_NAMESPACE_BEGIN

struct RayPortalClosure {
  SHADER_CLOSURE_BASE;

  float3 P;
  float3 D;
};

static_assert(sizeof(ShaderClosure) >= sizeof(RayPortalClosure), "RayPortalClosure is too large!");

ccl_device void bsdf_ray_portal_setup(ccl_private ShaderData *sd,
                                      const Spectrum weight,
                                      const uint32_t path_flag,
                                      const float3 position,
                                      float3 direction)
{
  /* Check cutoff weight. */
  const float sample_weight = fabsf(average(weight));
  if (!(sample_weight >= CLOSURE_WEIGHT_CUTOFF)) {
    return;
  }

  sd->closure_transparent_extinction += weight;

  ccl_private RayPortalClosure *pc = (ccl_private RayPortalClosure *)closure_alloc(
      sd, sizeof(RayPortalClosure), CLOSURE_BSDF_RAY_PORTAL_ID, weight);

  if (pc) {
    sd->flag |= SD_BSDF | SD_RAY_PORTAL;
    if (is_zero(direction)) {
      direction = -sd->wi;
    }
    pc->sample_weight = sample_weight;
    pc->N = sd->N;
    pc->P = position;
    pc->D = direction;
  }
}

ccl_device Spectrum bsdf_ray_portal_eval(const ccl_private ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  *pdf = 0.0f;
  return zero_spectrum();
}

CCL_NAMESPACE_END

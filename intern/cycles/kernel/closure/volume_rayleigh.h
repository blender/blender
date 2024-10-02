/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/volume_util.h"

CCL_NAMESPACE_BEGIN

/* RAYLEIGH CLOSURE */

typedef struct RayleighVolume {
  SHADER_CLOSURE_VOLUME_BASE;
} RayleighVolume;
static_assert(sizeof(ShaderVolumeClosure) >= sizeof(RayleighVolume),
              "RayleighVolume is too large!");

ccl_device int volume_rayleigh_setup(ccl_private RayleighVolume *volume)
{
  volume->type = CLOSURE_VOLUME_RAYLEIGH_ID;
  return SD_SCATTER;
}

ccl_device Spectrum volume_rayleigh_eval(ccl_private const ShaderData *sd,
                                         float3 wo,
                                         ccl_private float *pdf)
{
  /* note that wi points towards the viewer */
  float cos_theta = dot(-sd->wi, wo);
  *pdf = phase_rayleigh(cos_theta);

  return make_spectrum(*pdf);
}

ccl_device int volume_rayleigh_sample(ccl_private const ShaderData *sd,
                                      float2 rand,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf)
{
  /* note that wi points towards the viewer and so is used negated */
  *wo = phase_rayleigh_sample(-sd->wi, rand, pdf);
  *eval = make_spectrum(*pdf); /* perfect importance sampling */

  return LABEL_VOLUME_SCATTER;
}

CCL_NAMESPACE_END

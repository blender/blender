/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/volume_util.h"

CCL_NAMESPACE_BEGIN

/* DRAINE CLOSURE */

struct DraineVolume {
  SHADER_CLOSURE_VOLUME_BASE;

  float g;
  float alpha;
};
static_assert(sizeof(ShaderVolumeClosure) >= sizeof(DraineVolume), "DraineVolume is too large!");

ccl_device int volume_draine_setup(ccl_private DraineVolume *volume)
{
  volume->type = CLOSURE_VOLUME_DRAINE_ID;
  /* clamp anisotropy */
  volume->g = signf(volume->g) * min(fabsf(volume->g), 1.0f - 1e-3f);

  return SD_SCATTER;
}

ccl_device Spectrum volume_draine_eval(const ccl_private ShaderData *sd,
                                       const ccl_private ShaderVolumeClosure *svc,
                                       const float3 wo,
                                       ccl_private float *pdf)
{
  const ccl_private DraineVolume *volume = (const ccl_private DraineVolume *)svc;

  /* note that wi points towards the viewer */
  const float cos_theta = dot(-sd->wi, wo);
  *pdf = phase_draine(cos_theta, volume->g, volume->alpha);

  return make_spectrum(*pdf);
}

ccl_device int volume_draine_sample(const ccl_private ShaderData *sd,
                                    const ccl_private ShaderVolumeClosure *svc,
                                    const float2 rand,
                                    ccl_private Spectrum *eval,
                                    ccl_private float3 *wo,
                                    ccl_private float *pdf)
{
  const ccl_private DraineVolume *volume = (const ccl_private DraineVolume *)svc;

  /* note that wi points towards the viewer and so is used negated */
  *wo = phase_draine_sample(-sd->wi, volume->g, volume->alpha, rand, pdf);
  *eval = make_spectrum(*pdf); /* perfect importance sampling */

  return LABEL_VOLUME_SCATTER;
}

CCL_NAMESPACE_END

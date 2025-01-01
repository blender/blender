/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/volume_util.h"

CCL_NAMESPACE_BEGIN

/* HENYEY-GREENSTEIN CLOSURE */

struct HenyeyGreensteinVolume {
  SHADER_CLOSURE_VOLUME_BASE;

  float g;
};
static_assert(sizeof(ShaderVolumeClosure) >= sizeof(HenyeyGreensteinVolume),
              "HenyeyGreensteinVolume is too large!");

ccl_device int volume_henyey_greenstein_setup(ccl_private HenyeyGreensteinVolume *volume)
{
  volume->type = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
  /* clamp anisotropy to avoid delta function */
  volume->g = signf(volume->g) * min(fabsf(volume->g), 1.0f - 1e-3f);

  return SD_SCATTER;
}

ccl_device Spectrum volume_henyey_greenstein_eval(const ccl_private ShaderData *sd,
                                                  const ccl_private ShaderVolumeClosure *svc,
                                                  const float3 wo,
                                                  ccl_private float *pdf)
{
  const ccl_private HenyeyGreensteinVolume *volume = (const ccl_private HenyeyGreensteinVolume *)
      svc;

  /* note that wi points towards the viewer */
  const float cos_theta = dot(-sd->wi, wo);
  *pdf = phase_henyey_greenstein(cos_theta, volume->g);

  return make_spectrum(*pdf);
}

ccl_device int volume_henyey_greenstein_sample(const ccl_private ShaderData *sd,
                                               const ccl_private ShaderVolumeClosure *svc,
                                               const float2 rand,
                                               ccl_private Spectrum *eval,
                                               ccl_private float3 *wo,
                                               ccl_private float *pdf)
{
  const ccl_private HenyeyGreensteinVolume *volume = (const ccl_private HenyeyGreensteinVolume *)
      svc;

  /* note that wi points towards the viewer and so is used negated */
  *wo = phase_henyey_greenstein_sample(-sd->wi, volume->g, rand, pdf);
  *eval = make_spectrum(*pdf); /* perfect importance sampling */

  return LABEL_VOLUME_SCATTER;
}

CCL_NAMESPACE_END

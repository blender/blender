/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/volume_util.h"

CCL_NAMESPACE_BEGIN

/* FOURNIER-FORAND CLOSURE */

struct FournierForandVolume {
  SHADER_CLOSURE_VOLUME_BASE;

  /* Precomputed coefficients, based on B and IOR */
  float c1, c2, c3;
};
static_assert(sizeof(ShaderVolumeClosure) >= sizeof(FournierForandVolume),
              "FournierForandVolume is too large!");

ccl_device int volume_fournier_forand_setup(ccl_private FournierForandVolume *volume,
                                            float B,
                                            float IOR)
{
  volume->type = CLOSURE_VOLUME_FOURNIER_FORAND_ID;

  /* clamp backscatter fraction to avoid delta function */
  B = min(fabsf(B), 0.5f - 1e-3f);
  IOR = max(IOR, 1.0f + 1e-3f);
  const float3 coeffs = phase_fournier_forand_coeffs(B, IOR);
  volume->c1 = coeffs.x;
  volume->c2 = coeffs.y;
  volume->c3 = coeffs.z;

  return SD_SCATTER;
}

ccl_device Spectrum volume_fournier_forand_eval(const ccl_private ShaderData *sd,
                                                const ccl_private ShaderVolumeClosure *svc,
                                                const float3 wo,
                                                ccl_private float *pdf)
{
  const ccl_private FournierForandVolume *volume = (const ccl_private FournierForandVolume *)svc;
  const float3 coeffs = make_float3(volume->c1, volume->c2, volume->c3);

  /* note that wi points towards the viewer */
  const float cos_theta = dot(-sd->wi, wo);
  *pdf = phase_fournier_forand(cos_theta, coeffs);

  return make_spectrum(*pdf);
}

ccl_device int volume_fournier_forand_sample(const ccl_private ShaderData *sd,
                                             const ccl_private ShaderVolumeClosure *svc,
                                             const float2 rand,
                                             ccl_private Spectrum *eval,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf)
{
  const ccl_private FournierForandVolume *volume = (const ccl_private FournierForandVolume *)svc;
  const float3 coeffs = make_float3(volume->c1, volume->c2, volume->c3);

  /* note that wi points towards the viewer and so is used negated */
  *wo = phase_fournier_forand_sample(-sd->wi, coeffs, rand, pdf);
  *eval = make_spectrum(*pdf); /* perfect importance sampling */

  return LABEL_VOLUME_SCATTER;
}

CCL_NAMESPACE_END

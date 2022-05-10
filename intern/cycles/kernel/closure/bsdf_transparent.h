/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device void bsdf_transparent_setup(ccl_private ShaderData *sd,
                                       const float3 weight,
                                       uint32_t path_flag)
{
  /* Check cutoff weight. */
  float sample_weight = fabsf(average(weight));
  if (!(sample_weight >= CLOSURE_WEIGHT_CUTOFF)) {
    return;
  }

  if (sd->flag & SD_TRANSPARENT) {
    sd->closure_transparent_extinction += weight;

    /* Add weight to existing transparent BSDF. */
    for (int i = 0; i < sd->num_closure; i++) {
      ccl_private ShaderClosure *sc = &sd->closure[i];

      if (sc->type == CLOSURE_BSDF_TRANSPARENT_ID) {
        sc->weight += weight;
        sc->sample_weight += sample_weight;
        break;
      }
    }
  }
  else {
    sd->flag |= SD_BSDF | SD_TRANSPARENT;
    sd->closure_transparent_extinction = weight;

    if (path_flag & PATH_RAY_TERMINATE) {
      /* In this case the number of closures is set to zero to disable
       * all others, but we still want to get transparency so increase
       * the number just for this. */
      sd->num_closure_left = 1;
    }

    /* Create new transparent BSDF. */
    ccl_private ShaderClosure *bsdf = closure_alloc(
        sd, sizeof(ShaderClosure), CLOSURE_BSDF_TRANSPARENT_ID, weight);

    if (bsdf) {
      bsdf->sample_weight = sample_weight;
      bsdf->N = sd->N;
    }
    else if (path_flag & PATH_RAY_TERMINATE) {
      sd->num_closure_left = 0;
    }
  }
}

ccl_device float3 bsdf_transparent_eval_reflect(ccl_private const ShaderClosure *sc,
                                                const float3 I,
                                                const float3 omega_in,
                                                ccl_private float *pdf)
{
  *pdf = 0.0f;
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_transparent_eval_transmit(ccl_private const ShaderClosure *sc,
                                                 const float3 I,
                                                 const float3 omega_in,
                                                 ccl_private float *pdf)
{
  *pdf = 0.0f;
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_transparent_sample(ccl_private const ShaderClosure *sc,
                                       float3 Ng,
                                       float3 I,
                                       float3 dIdx,
                                       float3 dIdy,
                                       float randu,
                                       float randv,
                                       ccl_private float3 *eval,
                                       ccl_private float3 *omega_in,
                                       ccl_private float3 *domega_in_dx,
                                       ccl_private float3 *domega_in_dy,
                                       ccl_private float *pdf)
{
  // only one direction is possible
  *omega_in = -I;
#ifdef __RAY_DIFFERENTIALS__
  *domega_in_dx = -dIdx;
  *domega_in_dy = -dIdy;
#endif
  *pdf = 1;
  *eval = make_float3(1, 1, 1);
  return LABEL_TRANSMIT | LABEL_TRANSPARENT;
}

CCL_NAMESPACE_END

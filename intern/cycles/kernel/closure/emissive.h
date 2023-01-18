/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

CCL_NAMESPACE_BEGIN

/* BACKGROUND CLOSURE */

ccl_device void background_setup(ccl_private ShaderData *sd, const Spectrum weight)
{
  if (sd->flag & SD_EMISSION) {
    sd->closure_emission_background += weight;
  }
  else {
    sd->flag |= SD_EMISSION;
    sd->closure_emission_background = weight;
  }
}

/* EMISSION CLOSURE */

ccl_device void emission_setup(ccl_private ShaderData *sd, const Spectrum weight)
{
  if (sd->flag & SD_EMISSION) {
    sd->closure_emission_background += weight;
  }
  else {
    sd->flag |= SD_EMISSION;
    sd->closure_emission_background = weight;
  }
}

/* return the probability distribution function in the direction wi,
 * given the parameters and the light's surface normal.  This MUST match
 * the PDF computed by sample(). */
ccl_device float emissive_pdf(const float3 Ng, const float3 wi)
{
  float cosNI = fabsf(dot(Ng, wi));
  return (cosNI > 0.0f) ? 1.0f : 0.0f;
}

ccl_device void emissive_sample(
    const float3 Ng, float randu, float randv, ccl_private float3 *wi, ccl_private float *pdf)
{
  /* todo: not implemented and used yet */
}

ccl_device Spectrum emissive_simple_eval(const float3 Ng, const float3 wi)
{
  float res = emissive_pdf(Ng, wi);

  return make_spectrum(res);
}

CCL_NAMESPACE_END

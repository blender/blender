/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/volume_draine.h"
#include "kernel/closure/volume_fournier_forand.h"
#include "kernel/closure/volume_henyey_greenstein.h"
#include "kernel/closure/volume_rayleigh.h"

CCL_NAMESPACE_BEGIN

/* VOLUME EXTINCTION */

ccl_device void volume_extinction_setup(ccl_private ShaderData *sd, Spectrum weight)
{
  if (sd->flag & SD_EXTINCTION) {
    sd->closure_transparent_extinction += weight;
  }
  else {
    sd->flag |= SD_EXTINCTION;
    sd->closure_transparent_extinction = weight;
  }
}

/* VOLUME SCATTERING */

ccl_device Spectrum volume_phase_eval(const ccl_private ShaderData *sd,
                                      const ccl_private ShaderVolumeClosure *svc,
                                      const float3 wo,
                                      ccl_private float *pdf)
{
  switch (svc->type) {
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID:
      return volume_fournier_forand_eval(sd, svc, wo, pdf);
    case CLOSURE_VOLUME_RAYLEIGH_ID:
      return volume_rayleigh_eval(sd, wo, pdf);
    case CLOSURE_VOLUME_DRAINE_ID:
      return volume_draine_eval(sd, svc, wo, pdf);
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
      return volume_henyey_greenstein_eval(sd, svc, wo, pdf);
    default:
      kernel_assert(false);
      *pdf = 0.0f;
      return zero_spectrum();
  }
}

ccl_device int volume_phase_sample(const ccl_private ShaderData *sd,
                                   const ccl_private ShaderVolumeClosure *svc,
                                   const float2 rand,
                                   ccl_private Spectrum *eval,
                                   ccl_private float3 *wo,
                                   ccl_private float *pdf)
{
  switch (svc->type) {
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID:
      return volume_fournier_forand_sample(sd, svc, rand, eval, wo, pdf);
    case CLOSURE_VOLUME_RAYLEIGH_ID:
      return volume_rayleigh_sample(sd, rand, eval, wo, pdf);
    case CLOSURE_VOLUME_DRAINE_ID:
      return volume_draine_sample(sd, svc, rand, eval, wo, pdf);
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
      return volume_henyey_greenstein_sample(sd, svc, rand, eval, wo, pdf);
    default:
      kernel_assert(false);
      *pdf = 0.0f;
      return 0;
  }
}

ccl_device bool volume_phase_equal(const ccl_private ShaderClosure *c1,
                                   const ccl_private ShaderClosure *c2)
{
  if (c1->type != c2->type) {
    return false;
  }
  switch (c1->type) {
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID: {
      ccl_private FournierForandVolume *v1 = (ccl_private FournierForandVolume *)c1;
      ccl_private FournierForandVolume *v2 = (ccl_private FournierForandVolume *)c2;
      return v1->c1 == v2->c1 && v1->c2 == v2->c2 && v1->c3 == v2->c3;
    }
    case CLOSURE_VOLUME_RAYLEIGH_ID:
      return true;
    case CLOSURE_VOLUME_DRAINE_ID: {
      ccl_private DraineVolume *v1 = (ccl_private DraineVolume *)c1;
      ccl_private DraineVolume *v2 = (ccl_private DraineVolume *)c2;
      return v1->g == v2->g && v1->alpha == v2->alpha;
    }
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID: {
      ccl_private HenyeyGreensteinVolume *v1 = (ccl_private HenyeyGreensteinVolume *)c1;
      ccl_private HenyeyGreensteinVolume *v2 = (ccl_private HenyeyGreensteinVolume *)c2;
      return v1->g == v2->g;
    }
    default:
      return false;
  }
  return false;
}

/* Approximate phase functions as Henyey-Greenstein for volume guiding.
 * TODO: This is not ideal, we should use RIS guiding for non-HG phase functions. */
ccl_device float volume_phase_get_g(const ccl_private ShaderVolumeClosure *svc)
{
  switch (svc->type) {
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID:
      /* TODO */
      return 1.0f;
    case CLOSURE_VOLUME_RAYLEIGH_ID:
      /* Approximate as isotropic */
      return 0.0f;
    case CLOSURE_VOLUME_DRAINE_ID:
      /* Approximate as HG, TODO */
      return ((ccl_private DraineVolume *)svc)->g;
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
      return ((ccl_private HenyeyGreensteinVolume *)svc)->g;
    default:
      return 0.0f;
  }
}

/* Volume sampling utilities. */

/* todo: this value could be tweaked or turned into a probability to avoid
 * unnecessary work in volumes and subsurface scattering. */
#define VOLUME_THROUGHPUT_EPSILON 1e-6f

ccl_device Spectrum volume_color_transmittance(Spectrum sigma, const float t)
{
  return exp(-sigma * t);
}

ccl_device float volume_channel_get(Spectrum value, const int channel)
{
  return GET_SPECTRUM_CHANNEL(value, channel);
}

ccl_device int volume_sample_channel(Spectrum albedo,
                                     Spectrum throughput,
                                     ccl_private float *rand,
                                     ccl_private Spectrum *pdf)
{
  /* Sample color channel proportional to throughput and single scattering
   * albedo, to significantly reduce noise with many bounce, following:
   *
   * "Practical and Controllable Subsurface Scattering for Production Path
   *  Tracing". Matt Jen-Yuan Chiang, Peter Kutz, Brent Burley. SIGGRAPH 2016. */
  const Spectrum weights = fabs(throughput * albedo);
  const float sum_weights = reduce_add(weights);

  if (sum_weights > 0.0f) {
    *pdf = weights / sum_weights;
  }
  else {
    *pdf = make_spectrum(1.0f / SPECTRUM_CHANNELS);
  }

  float pdf_sum = 0.0f;
  FOREACH_SPECTRUM_CHANNEL (i) {
    const float channel_pdf = GET_SPECTRUM_CHANNEL(*pdf, i);
    if (*rand < pdf_sum + channel_pdf) {
      /* Rescale to reuse. */
      *rand = (*rand - pdf_sum) / channel_pdf;
      return i;
    }
    pdf_sum += channel_pdf;
  }
  return SPECTRUM_CHANNELS - 1;
}

CCL_NAMESPACE_END

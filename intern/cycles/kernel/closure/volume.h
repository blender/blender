/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

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

/* HENYEY-GREENSTEIN CLOSURE */

typedef struct HenyeyGreensteinVolume {
  SHADER_CLOSURE_BASE;

  float g;
} HenyeyGreensteinVolume;

static_assert(sizeof(ShaderClosure) >= sizeof(HenyeyGreensteinVolume),
              "HenyeyGreensteinVolume is too large!");

/* Given cosine between rays, return probability density that a photon bounces
 * to that direction. The g parameter controls how different it is from the
 * uniform sphere. g=0 uniform diffuse-like, g=1 close to sharp single ray. */
ccl_device float single_peaked_henyey_greenstein(float cos_theta, float g)
{
  return ((1.0f - g * g) / safe_powf(1.0f + g * g - 2.0f * g * cos_theta, 1.5f)) *
         (M_1_PI_F * 0.25f);
};

ccl_device int volume_henyey_greenstein_setup(ccl_private HenyeyGreensteinVolume *volume)
{
  volume->type = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;

  /* clamp anisotropy to avoid delta function */
  volume->g = signf(volume->g) * min(fabsf(volume->g), 1.0f - 1e-3f);

  return SD_SCATTER;
}

ccl_device Spectrum volume_henyey_greenstein_eval_phase(ccl_private const ShaderVolumeClosure *svc,
                                                        const float3 wi,
                                                        float3 wo,
                                                        ccl_private float *pdf)
{
  float g = svc->g;

  /* note that wi points towards the viewer */
  if (fabsf(g) < 1e-3f) {
    *pdf = M_1_PI_F * 0.25f;
  }
  else {
    float cos_theta = dot(-wi, wo);
    *pdf = single_peaked_henyey_greenstein(cos_theta, g);
  }

  return make_spectrum(*pdf);
}

ccl_device float3 henyey_greenstrein_sample(float3 D, float g, float2 rand, ccl_private float *pdf)
{
  /* match pdf for small g */
  float cos_theta;
  bool isotropic = fabsf(g) < 1e-3f;

  if (isotropic) {
    cos_theta = (1.0f - 2.0f * rand.x);
    if (pdf) {
      *pdf = M_1_PI_F * 0.25f;
    }
  }
  else {
    float k = (1.0f - g * g) / (1.0f - g + 2.0f * g * rand.x);
    cos_theta = (1.0f + g * g - k * k) / (2.0f * g);
    if (pdf) {
      *pdf = single_peaked_henyey_greenstein(cos_theta, g);
    }
  }

  float sin_theta = sin_from_cos(cos_theta);
  float phi = M_2PI_F * rand.y;
  float3 dir = make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);

  float3 T, B;
  make_orthonormals(D, &T, &B);
  dir = dir.x * T + dir.y * B + dir.z * D;

  return dir;
}

ccl_device int volume_henyey_greenstein_sample(ccl_private const ShaderVolumeClosure *svc,
                                               float3 wi,
                                               float2 rand,
                                               ccl_private Spectrum *eval,
                                               ccl_private float3 *wo,
                                               ccl_private float *pdf)
{
  float g = svc->g;

  /* note that wi points towards the viewer and so is used negated */
  *wo = henyey_greenstrein_sample(-wi, g, rand, pdf);
  *eval = make_spectrum(*pdf); /* perfect importance sampling */

  return LABEL_VOLUME_SCATTER;
}

/* VOLUME CLOSURE */

ccl_device Spectrum volume_phase_eval(ccl_private const ShaderData *sd,
                                      ccl_private const ShaderVolumeClosure *svc,
                                      float3 wo,
                                      ccl_private float *pdf)
{
  return volume_henyey_greenstein_eval_phase(svc, sd->wi, wo, pdf);
}

ccl_device int volume_phase_sample(ccl_private const ShaderData *sd,
                                   ccl_private const ShaderVolumeClosure *svc,
                                   float2 rand,
                                   ccl_private Spectrum *eval,
                                   ccl_private float3 *wo,
                                   ccl_private float *pdf)
{
  return volume_henyey_greenstein_sample(svc, sd->wi, rand, eval, wo, pdf);
}

/* Volume sampling utilities. */

/* todo: this value could be tweaked or turned into a probability to avoid
 * unnecessary work in volumes and subsurface scattering. */
#define VOLUME_THROUGHPUT_EPSILON 1e-6f

ccl_device Spectrum volume_color_transmittance(Spectrum sigma, float t)
{
  return exp(-sigma * t);
}

ccl_device float volume_channel_get(Spectrum value, int channel)
{
  return GET_SPECTRUM_CHANNEL(value, channel);
}

ccl_device int volume_sample_channel(Spectrum albedo,
                                     Spectrum throughput,
                                     float rand,
                                     ccl_private Spectrum *pdf)
{
  /* Sample color channel proportional to throughput and single scattering
   * albedo, to significantly reduce noise with many bounce, following:
   *
   * "Practical and Controllable Subsurface Scattering for Production Path
   *  Tracing". Matt Jen-Yuan Chiang, Peter Kutz, Brent Burley. SIGGRAPH 2016. */
  Spectrum weights = fabs(throughput * albedo);
  float sum_weights = reduce_add(weights);

  if (sum_weights > 0.0f) {
    *pdf = weights / sum_weights;
  }
  else {
    *pdf = make_spectrum(1.0f / SPECTRUM_CHANNELS);
  }

  float pdf_sum = 0.0f;
  FOREACH_SPECTRUM_CHANNEL (i) {
    pdf_sum += GET_SPECTRUM_CHANNEL(*pdf, i);
    if (rand < pdf_sum) {
      return i;
    }
  }
  return SPECTRUM_CHANNELS - 1;
}

CCL_NAMESPACE_END

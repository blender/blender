/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse.h"

CCL_NAMESPACE_BEGIN

struct Bssrdf {
  SHADER_CLOSURE_BASE;

  Spectrum radius;
  Spectrum albedo;
  float anisotropy;

  /* Parameters for refractive entry bounce. */
  float ior;
  float alpha;
};

static_assert(sizeof(ShaderClosure) >= sizeof(Bssrdf), "Bssrdf is too large!");

/* Random Walk BSSRDF */

ccl_device float bssrdf_dipole_compute_Rd(const float alpha_prime, const float fourthirdA)
{
  const float s = sqrtf(3.0f * (1.0f - alpha_prime));
  return 0.5f * alpha_prime * (1.0f + expf(-fourthirdA * s)) * expf(-s);
}

ccl_device float bssrdf_dipole_compute_alpha_prime(const float rd, const float fourthirdA)
{
  /* Little Newton solver. */
  if (rd < 1e-4f) {
    return 0.0f;
  }
  if (rd >= 0.995f) {
    return 0.999999f;
  }

  float x0 = 0.0f;
  float x1 = 1.0f;
  float xmid;
  float fmid;

  constexpr const int max_num_iterations = 12;
  for (int i = 0; i < max_num_iterations; ++i) {
    xmid = 0.5f * (x0 + x1);
    fmid = bssrdf_dipole_compute_Rd(xmid, fourthirdA);
    if (fmid < rd) {
      x0 = xmid;
    }
    else {
      x1 = xmid;
    }
  }

  return xmid;
}

ccl_device void bssrdf_setup_radius(ccl_private Bssrdf *bssrdf, const ClosureType type)
{
  if (type == CLOSURE_BSSRDF_BURLEY_ID || type == CLOSURE_BSSRDF_RANDOM_WALK_ID) {
    /* Scale mean free path length so it gives similar looking result to older
     * Cubic, Gaussian and Burley models. */
    bssrdf->radius *= 0.25f * M_1_PI_F;
  }
  else {
    /* Adjust radius based on IOR and albedo. */
    const float inv_eta = 1.0f / bssrdf->ior;
    const float F_dr = inv_eta * (-1.440f * inv_eta + 0.710f) + 0.668f + 0.0636f * bssrdf->ior;
    const float fourthirdA = (4.0f / 3.0f) * (1.0f + F_dr) /
                             (1.0f - F_dr); /* From Jensen's `Fdr` ratio formula. */

    Spectrum alpha_prime;
    FOREACH_SPECTRUM_CHANNEL (i) {
      GET_SPECTRUM_CHANNEL(alpha_prime, i) = bssrdf_dipole_compute_alpha_prime(
          GET_SPECTRUM_CHANNEL(bssrdf->albedo, i), fourthirdA);
    }

    bssrdf->radius *= sqrt(3.0f * (one_spectrum() - alpha_prime));
  }
}

/* Christensen-Burley BSSRDF.
 *
 * Approximate Reflectance Profiles from
 * http://graphics.pixar.com/library/ApproxBSSRDF/paper.pdf
 */

/* This is a bit arbitrary, just need big enough radius so it matches
 * the mean free length, but still not too big so sampling is still
 * effective. */
#define BURLEY_TRUNCATE 16.0f
#define BURLEY_TRUNCATE_CDF 0.9963790093708328f  // cdf(BURLEY_TRUNCATE)

ccl_device_inline float bssrdf_burley_fitting(const float A)
{
  /* Diffuse surface transmission, equation (6). */
  return 1.9f - A + 3.5f * (A - 0.8f) * (A - 0.8f);
}

/* Scale mean free path length so it gives similar looking result
 * to Cubic and Gaussian models. */
ccl_device_inline Spectrum bssrdf_burley_compatible_mfp(Spectrum r)
{
  return 0.25f * M_1_PI_F * r;
}

ccl_device void bssrdf_burley_setup(ccl_private Bssrdf *bssrdf)
{
  /* Mean free path length. */
  const Spectrum l = bssrdf_burley_compatible_mfp(bssrdf->radius);
  /* Surface albedo. */
  const Spectrum A = bssrdf->albedo;
  Spectrum s;
  FOREACH_SPECTRUM_CHANNEL (i) {
    GET_SPECTRUM_CHANNEL(s, i) = bssrdf_burley_fitting(GET_SPECTRUM_CHANNEL(A, i));
  }

  bssrdf->radius = l / s;
}

ccl_device float bssrdf_burley_eval(const float d, const float r)
{
  const float Rm = BURLEY_TRUNCATE * d;

  if (r >= Rm) {
    return 0.0f;
  }

  /* Burley reflectance profile, equation (3).
   *
   * NOTES:
   * - Surface albedo is already included into `sc->weight`, no need to
   *   multiply by this term here.
   * - This is normalized diffuse model, so the equation is multiplied
   *   by `2*pi`, which also matches `cdf()`.
   */
  const float exp_r_3_d = expf(-r / (3.0f * d));
  const float exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
  return (exp_r_d + exp_r_3_d) / (4.0f * d);
}

ccl_device float bssrdf_burley_pdf(const float d, const float r)
{
  if (r == 0.0f) {
    return 0.0f;
  }

  return bssrdf_burley_eval(d, r) * (1.0f / BURLEY_TRUNCATE_CDF);
}

/* Find the radius for desired CDF value.
 * Returns scaled radius, meaning the result is to be scaled up by d.
 * Since there's no closed form solution we do Newton-Raphson method to find it.
 */
ccl_device_forceinline float bssrdf_burley_root_find(const float xi)
{
  const float tolerance = 1e-6f;
  const int max_iteration_count = 10;
  /* Do initial guess based on manual curve fitting, this allows us to reduce
   * number of iterations to maximum 4 across the [0..1] range. We keep maximum
   * number of iteration higher just to be sure we didn't miss root in some
   * corner case.
   */
  float r;
  if (xi <= 0.9f) {
    r = expf(xi * xi * 2.4f) - 1.0f;
  }
  else {
    /* TODO(sergey): Some nicer curve fit is possible here. */
    r = 15.0f;
  }
  /* Solve against scaled radius. */
  for (int i = 0; i < max_iteration_count; i++) {
    const float exp_r_3 = expf(-r / 3.0f);
    const float exp_r = exp_r_3 * exp_r_3 * exp_r_3;
    const float f = 1.0f - 0.25f * exp_r - 0.75f * exp_r_3 - xi;
    const float f_ = 0.25f * exp_r + 0.25f * exp_r_3;

    if (fabsf(f) < tolerance || f_ == 0.0f) {
      break;
    }

    r = r - f / f_;
    r = fmaxf(r, 0.0f);
  }
  return r;
}

ccl_device void bssrdf_burley_sample(const float d,
                                     const float xi,
                                     ccl_private float *r,
                                     ccl_private float *h)
{
  const float Rm = BURLEY_TRUNCATE * d;
  const float r_ = bssrdf_burley_root_find(xi * BURLEY_TRUNCATE_CDF) * d;

  *r = r_;

  /* h^2 + r^2 = Rm^2 */
  *h = safe_sqrtf(Rm * Rm - r_ * r_);
}

ccl_device float bssrdf_num_channels(const Spectrum radius)
{
  float channels = 0;
  FOREACH_SPECTRUM_CHANNEL (i) {
    if (GET_SPECTRUM_CHANNEL(radius, i) > 0.0f) {
      channels += 1.0f;
    }
  }
  return channels;
}

ccl_device void bssrdf_sample(const Spectrum radius,
                              float xi,
                              ccl_private float *r,
                              ccl_private float *h)
{
  const float num_channels = bssrdf_num_channels(radius);
  float sampled_radius;

  /* Sample color channel and reuse random number. Only a subset of channels
   * may be used if their radius was too small to handle as BSSRDF. */
  xi *= num_channels;
  sampled_radius = 0.0f;

  float sum = 0.0f;
  FOREACH_SPECTRUM_CHANNEL (i) {
    const float channel_radius = GET_SPECTRUM_CHANNEL(radius, i);
    if (channel_radius > 0.0f) {
      const float next_sum = sum + 1.0f;
      if (xi < next_sum) {
        xi -= sum;
        sampled_radius = channel_radius;
        break;
      }
      sum = next_sum;
    }
  }

  /* Sample BSSRDF. */
  bssrdf_burley_sample(sampled_radius, xi, r, h);
}

ccl_device_forceinline Spectrum bssrdf_eval(const Spectrum radius, const float r)
{
  Spectrum result;
  FOREACH_SPECTRUM_CHANNEL (i) {
    GET_SPECTRUM_CHANNEL(result, i) = bssrdf_burley_pdf(GET_SPECTRUM_CHANNEL(radius, i), r);
  }
  return result;
}

ccl_device_forceinline float bssrdf_pdf(const Spectrum radius, const float r)
{
  const Spectrum pdf = bssrdf_eval(radius, r);
  return reduce_add(pdf) / bssrdf_num_channels(radius);
}

/* Setup */

ccl_device_inline ccl_private Bssrdf *bssrdf_alloc(ccl_private ShaderData *sd, Spectrum weight)
{
  const float sample_weight = fabsf(average(weight));
  if (sample_weight < CLOSURE_WEIGHT_CUTOFF) {
    return nullptr;
  }

  ccl_private Bssrdf *bssrdf = (ccl_private Bssrdf *)closure_alloc(
      sd, sizeof(Bssrdf), CLOSURE_NONE_ID, weight);

  if (bssrdf == nullptr) {
    return nullptr;
  }

  bssrdf->sample_weight = sample_weight;
  return bssrdf;
}

ccl_device int bssrdf_setup(ccl_private ShaderData *sd,
                            ccl_private Bssrdf *bssrdf,
                            const int path_flag,
                            ClosureType type)
{
  /* Clamps protecting against bad/extreme and non physical values. */
  bssrdf->anisotropy = clamp(bssrdf->anisotropy, 0.0f, 0.9f);
  bssrdf->ior = clamp(bssrdf->ior, 1.01f, 3.8f);

  int flag = 0;

  if (type == CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID) {
    /* CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID uses a fixed roughness. */
    bssrdf->alpha = 1.0f;
  }

  /* Verify if the radii are large enough to sample without precision issues. */
  int bssrdf_channels = SPECTRUM_CHANNELS;
  Spectrum diffuse_weight = zero_spectrum();

  if (path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
    /* Always fall back to diffuse after a diffuse ancestor. Can't see it that well and it adds
     * considerable noise due to probabilities of continuing the path getting lower and lower. */
    bssrdf_channels = 0;
    diffuse_weight = bssrdf->weight;
  }
  else {
    FOREACH_SPECTRUM_CHANNEL (i) {
      if (GET_SPECTRUM_CHANNEL(bssrdf->radius, i) < BSSRDF_MIN_RADIUS) {
        GET_SPECTRUM_CHANNEL(diffuse_weight, i) = GET_SPECTRUM_CHANNEL(bssrdf->weight, i);
        GET_SPECTRUM_CHANNEL(bssrdf->weight, i) = 0.0f;
        GET_SPECTRUM_CHANNEL(bssrdf->radius, i) = 0.0f;
        bssrdf_channels--;
      }
    }
  }

  if (bssrdf_channels < SPECTRUM_CHANNELS) {
    /* Add diffuse BSDF if any radius too small. */
    ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
        sd, sizeof(DiffuseBsdf), diffuse_weight);

    if (bsdf) {
      bsdf->N = bssrdf->N;
      flag |= bsdf_diffuse_setup(bsdf);
    }
  }

  /* Setup BSSRDF if radius is large enough. */
  if (bssrdf_channels > 0) {
    bssrdf->type = type;
    bssrdf->sample_weight = fabsf(average(bssrdf->weight)) * bssrdf_channels;

    bssrdf_setup_radius(bssrdf, type);

    flag |= SD_BSSRDF;
  }
  else {
    bssrdf->type = CLOSURE_NONE_ID;
    bssrdf->sample_weight = 0.0f;
  }

  return flag;
}

CCL_NAMESPACE_END

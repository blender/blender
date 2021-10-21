/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

CCL_NAMESPACE_BEGIN

typedef struct Bssrdf {
  SHADER_CLOSURE_BASE;

  float3 radius;
  float3 albedo;
  float roughness;
  float anisotropy;
} Bssrdf;

static_assert(sizeof(ShaderClosure) >= sizeof(Bssrdf), "Bssrdf is too large!");

/* Random Walk BSSRDF */

ccl_device float bssrdf_dipole_compute_Rd(float alpha_prime, float fourthirdA)
{
  float s = sqrtf(3.0f * (1.0f - alpha_prime));
  return 0.5f * alpha_prime * (1.0f + expf(-fourthirdA * s)) * expf(-s);
}

ccl_device float bssrdf_dipole_compute_alpha_prime(float rd, float fourthirdA)
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
  float xmid, fmid;

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

ccl_device void bssrdf_setup_radius(ccl_private Bssrdf *bssrdf,
                                    const ClosureType type,
                                    const float eta)
{
  if (type == CLOSURE_BSSRDF_BURLEY_ID || type == CLOSURE_BSSRDF_RANDOM_WALK_FIXED_RADIUS_ID) {
    /* Scale mean free path length so it gives similar looking result to older
     * Cubic, Gaussian and Burley models. */
    bssrdf->radius *= 0.25f * M_1_PI_F;
  }
  else {
    /* Adjust radius based on IOR and albedo. */
    const float inv_eta = 1.0f / eta;
    const float F_dr = inv_eta * (-1.440f * inv_eta + 0.710f) + 0.668f + 0.0636f * eta;
    const float fourthirdA = (4.0f / 3.0f) * (1.0f + F_dr) /
                             (1.0f - F_dr); /* From Jensen's `Fdr` ratio formula. */

    const float3 alpha_prime = make_float3(
        bssrdf_dipole_compute_alpha_prime(bssrdf->albedo.x, fourthirdA),
        bssrdf_dipole_compute_alpha_prime(bssrdf->albedo.y, fourthirdA),
        bssrdf_dipole_compute_alpha_prime(bssrdf->albedo.z, fourthirdA));

    bssrdf->radius *= sqrt(3.0f * (one_float3() - alpha_prime));
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

ccl_device_inline float bssrdf_burley_fitting(float A)
{
  /* Diffuse surface transmission, equation (6). */
  return 1.9f - A + 3.5f * (A - 0.8f) * (A - 0.8f);
}

/* Scale mean free path length so it gives similar looking result
 * to Cubic and Gaussian models. */
ccl_device_inline float3 bssrdf_burley_compatible_mfp(float3 r)
{
  return 0.25f * M_1_PI_F * r;
}

ccl_device void bssrdf_burley_setup(ccl_private Bssrdf *bssrdf)
{
  /* Mean free path length. */
  const float3 l = bssrdf_burley_compatible_mfp(bssrdf->radius);
  /* Surface albedo. */
  const float3 A = bssrdf->albedo;
  const float3 s = make_float3(
      bssrdf_burley_fitting(A.x), bssrdf_burley_fitting(A.y), bssrdf_burley_fitting(A.z));

  bssrdf->radius = l / s;
}

ccl_device float bssrdf_burley_eval(const float d, float r)
{
  const float Rm = BURLEY_TRUNCATE * d;

  if (r >= Rm)
    return 0.0f;

  /* Burley reflectance profile, equation (3).
   *
   * NOTES:
   * - Surface albedo is already included into `sc->weight`, no need to
   *   multiply by this term here.
   * - This is normalized diffuse model, so the equation is multiplied
   *   by `2*pi`, which also matches `cdf()`.
   */
  float exp_r_3_d = expf(-r / (3.0f * d));
  float exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
  return (exp_r_d + exp_r_3_d) / (4.0f * d);
}

ccl_device float bssrdf_burley_pdf(const float d, float r)
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
ccl_device_forceinline float bssrdf_burley_root_find(float xi)
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
    float exp_r_3 = expf(-r / 3.0f);
    float exp_r = exp_r_3 * exp_r_3 * exp_r_3;
    float f = 1.0f - 0.25f * exp_r - 0.75f * exp_r_3 - xi;
    float f_ = 0.25f * exp_r + 0.25f * exp_r_3;

    if (fabsf(f) < tolerance || f_ == 0.0f) {
      break;
    }

    r = r - f / f_;
    if (r < 0.0f) {
      r = 0.0f;
    }
  }
  return r;
}

ccl_device void bssrdf_burley_sample(const float d,
                                     float xi,
                                     ccl_private float *r,
                                     ccl_private float *h)
{
  const float Rm = BURLEY_TRUNCATE * d;
  const float r_ = bssrdf_burley_root_find(xi * BURLEY_TRUNCATE_CDF) * d;

  *r = r_;

  /* h^2 + r^2 = Rm^2 */
  *h = safe_sqrtf(Rm * Rm - r_ * r_);
}

ccl_device float bssrdf_num_channels(const float3 radius)
{
  float channels = 0;
  if (radius.x > 0.0f) {
    channels += 1.0f;
  }
  if (radius.y > 0.0f) {
    channels += 1.0f;
  }
  if (radius.z > 0.0f) {
    channels += 1.0f;
  }
  return channels;
}

ccl_device void bssrdf_sample(const float3 radius,
                              float xi,
                              ccl_private float *r,
                              ccl_private float *h)
{
  const float num_channels = bssrdf_num_channels(radius);
  float sampled_radius;

  /* Sample color channel and reuse random number. Only a subset of channels
   * may be used if their radius was too small to handle as BSSRDF. */
  xi *= num_channels;

  if (xi < 1.0f) {
    sampled_radius = (radius.x > 0.0f) ? radius.x : (radius.y > 0.0f) ? radius.y : radius.z;
  }
  else if (xi < 2.0f) {
    xi -= 1.0f;
    sampled_radius = (radius.x > 0.0f && radius.y > 0.0f) ? radius.y : radius.z;
  }
  else {
    xi -= 2.0f;
    sampled_radius = radius.z;
  }

  /* Sample BSSRDF. */
  bssrdf_burley_sample(sampled_radius, xi, r, h);
}

ccl_device_forceinline float3 bssrdf_eval(const float3 radius, float r)
{
  return make_float3(bssrdf_burley_pdf(radius.x, r),
                     bssrdf_burley_pdf(radius.y, r),
                     bssrdf_burley_pdf(radius.z, r));
}

ccl_device_forceinline float bssrdf_pdf(const float3 radius, float r)
{
  float3 pdf = bssrdf_eval(radius, r);
  return (pdf.x + pdf.y + pdf.z) / bssrdf_num_channels(radius);
}

/* Setup */

ccl_device_inline ccl_private Bssrdf *bssrdf_alloc(ccl_private ShaderData *sd, float3 weight)
{
  ccl_private Bssrdf *bssrdf = (ccl_private Bssrdf *)closure_alloc(
      sd, sizeof(Bssrdf), CLOSURE_NONE_ID, weight);

  if (bssrdf == NULL) {
    return NULL;
  }

  float sample_weight = fabsf(average(weight));
  bssrdf->sample_weight = sample_weight;
  return (sample_weight >= CLOSURE_WEIGHT_CUTOFF) ? bssrdf : NULL;
}

ccl_device int bssrdf_setup(ccl_private ShaderData *sd,
                            ccl_private Bssrdf *bssrdf,
                            ClosureType type,
                            const float ior)
{
  int flag = 0;

  /* Add retro-reflection component as separate diffuse BSDF. */
  if (bssrdf->roughness != FLT_MAX) {
    ccl_private PrincipledDiffuseBsdf *bsdf = (ccl_private PrincipledDiffuseBsdf *)bsdf_alloc(
        sd, sizeof(PrincipledDiffuseBsdf), bssrdf->weight);

    if (bsdf) {
      bsdf->N = bssrdf->N;
      bsdf->roughness = bssrdf->roughness;
      flag |= bsdf_principled_diffuse_setup(bsdf, PRINCIPLED_DIFFUSE_RETRO_REFLECTION);

      /* Ad-hoc weight adjustment to avoid retro-reflection taking away half the
       * samples from BSSRDF. */
      bsdf->sample_weight *= bsdf_principled_diffuse_retro_reflection_sample_weight(bsdf, sd->I);
    }
  }

  /* Verify if the radii are large enough to sample without precision issues. */
  int bssrdf_channels = 3;
  float3 diffuse_weight = make_float3(0.0f, 0.0f, 0.0f);

  if (bssrdf->radius.x < BSSRDF_MIN_RADIUS) {
    diffuse_weight.x = bssrdf->weight.x;
    bssrdf->weight.x = 0.0f;
    bssrdf->radius.x = 0.0f;
    bssrdf_channels--;
  }
  if (bssrdf->radius.y < BSSRDF_MIN_RADIUS) {
    diffuse_weight.y = bssrdf->weight.y;
    bssrdf->weight.y = 0.0f;
    bssrdf->radius.y = 0.0f;
    bssrdf_channels--;
  }
  if (bssrdf->radius.z < BSSRDF_MIN_RADIUS) {
    diffuse_weight.z = bssrdf->weight.z;
    bssrdf->weight.z = 0.0f;
    bssrdf->radius.z = 0.0f;
    bssrdf_channels--;
  }

  if (bssrdf_channels < 3) {
    /* Add diffuse BSDF if any radius too small. */
#ifdef __PRINCIPLED__
    if (bssrdf->roughness != FLT_MAX) {
      ccl_private PrincipledDiffuseBsdf *bsdf = (ccl_private PrincipledDiffuseBsdf *)bsdf_alloc(
          sd, sizeof(PrincipledDiffuseBsdf), diffuse_weight);

      if (bsdf) {
        bsdf->N = bssrdf->N;
        bsdf->roughness = bssrdf->roughness;
        flag |= bsdf_principled_diffuse_setup(bsdf, PRINCIPLED_DIFFUSE_LAMBERT);
      }
    }
    else
#endif /* __PRINCIPLED__ */
    {
      ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
          sd, sizeof(DiffuseBsdf), diffuse_weight);

      if (bsdf) {
        bsdf->N = bssrdf->N;
        flag |= bsdf_diffuse_setup(bsdf);
      }
    }
  }

  /* Setup BSSRDF if radius is large enough. */
  if (bssrdf_channels > 0) {
    bssrdf->type = type;
    bssrdf->sample_weight = fabsf(average(bssrdf->weight)) * bssrdf_channels;

    bssrdf_setup_radius(bssrdf, type, ior);

    flag |= SD_BSSRDF;
  }
  else {
    bssrdf->type = type;
    bssrdf->sample_weight = 0.0f;
  }

  return flag;
}

CCL_NAMESPACE_END

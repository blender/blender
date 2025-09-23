/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "util/math.h"
#include "util/projection.h"

CCL_NAMESPACE_BEGIN

/* Distribute 2D uniform random samples on [0, 1] over unit disk [-1, 1], with concentric mapping
 * to better preserve stratification for some RNG sequences. */
ccl_device float2 sample_uniform_disk(const float2 rand)
{
  float phi;
  float r;
  const float a = 2.0f * rand.x - 1.0f;
  const float b = 2.0f * rand.y - 1.0f;

  if (a == 0.0f && b == 0.0f) {
    return zero_float2();
  }

  if (a * a > b * b) {
    r = a;
    phi = M_PI_4_F * (b / a);
  }
  else {
    r = b;
    phi = M_PI_2_F - M_PI_4_F * (a / b);
  }

  return polar_to_cartesian(r, phi);
}

/* return an orthogonal tangent and bitangent given a normal and tangent that
 * may not be exactly orthogonal */
ccl_device void make_orthonormals_tangent(const float3 N,
                                          const float3 T,
                                          ccl_private float3 *a,
                                          ccl_private float3 *b)
{
  *b = normalize(cross(N, T));
  *a = cross(*b, N);
}

ccl_device void make_orthonormals_safe_tangent(const float3 N,
                                               const float3 T,
                                               ccl_private float3 *a,
                                               ccl_private float3 *b)
{
  *b = safe_normalize(cross(N, T));
  if (len_squared(*b) < 0.99f) {
    /* Normalization failed, so fall back to basic orthonormals. */
    make_orthonormals(N, a, b);
  }
  else {
    *a = cross(*b, N);
  }
}

/* sample direction with cosine weighted distributed in hemisphere */
ccl_device_inline void sample_cos_hemisphere(const float3 N,
                                             const float2 rand_in,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf)
{
  const float2 rand = sample_uniform_disk(rand_in);
  const float costheta = safe_sqrtf(1.0f - len_squared(rand));

  float3 T;
  float3 B;
  make_orthonormals(N, &T, &B);
  *wo = rand.x * T + rand.y * B + costheta * N;
  *pdf = costheta * M_1_PI_F;
}

ccl_device_inline float pdf_cos_hemisphere(const float3 N, const float3 D)
{
  const float cos_theta = dot(N, D);
  return cos_theta > 0 ? cos_theta * M_1_PI_F : 0.0f;
}

/* sample direction uniformly distributed in hemisphere */
ccl_device_inline void sample_uniform_hemisphere(const float3 N,
                                                 const float2 rand,
                                                 ccl_private float3 *wo,
                                                 ccl_private float *pdf)
{
  float2 xy = sample_uniform_disk(rand);
  const float z = 1.0f - len_squared(xy);

  xy *= safe_sqrtf(z + 1.0f);

  float3 T;
  float3 B;
  make_orthonormals(N, &T, &B);

  *wo = xy.x * T + xy.y * B + z * N;
  *pdf = M_1_2PI_F;
}

ccl_device_inline float pdf_uniform_cone(const float3 N, const float3 D, const float angle)
{
  const float z = precise_angle(N, D);
  if (z < angle) {
    return M_1_2PI_F / one_minus_cos(angle);
  }
  return 0.0f;
}

/* Uniformly sample a direction in a cone of given angle around `N`. Use concentric mapping to
 * better preserve stratification. Return the angle between `N` and the sampled direction as
 * `cos_theta`.
 * Pass `1 - cos(angle)` as argument instead of `angle` to alleviate precision issues at small
 * angles (see sphere light for reference). */
ccl_device_inline float3 sample_uniform_cone(const float3 N,
                                             const float one_minus_cos_angle,
                                             const float2 rand,
                                             ccl_private float *cos_theta,
                                             ccl_private float *pdf)
{
  if (one_minus_cos_angle > 0) {
    /* Remap radius to get a uniform distribution w.r.t. solid angle on the cone.
     * The logic to derive this mapping is as follows:
     *
     * Sampling a cone is comparable to sampling the hemisphere, we just restrict theta. Therefore,
     * the same trick of first sampling the unit disk and the projecting the result up towards the
     * hemisphere by calculating the appropriate z coordinate still works.
     *
     * However, by itself this results in cosine-weighted hemisphere sampling, so we need some kind
     * of remapping. Cosine-weighted hemisphere and uniform cone sampling have the same conditional
     * PDF for phi (both are constant), so we only need to think about theta, which corresponds
     * directly to the radius.
     *
     * To find this mapping, we consider the simplest sampling strategies for cosine-weighted
     * hemispheres and uniform cones. In both, phi is chosen as `2pi * random()`. For the former,
     * `r_disk(rand) = sqrt(rand)`. This is just naive disk sampling, since the projection to the
     * hemisphere doesn't change the radius.
     * For the latter, `r_cone(rand) = sin_from_cos(mix(cos_angle, 1, rand))`.
     *
     * So, to remap, we just invert r_disk `(-> rand(r_disk) = r_disk^2)` and insert it into
     * r_cone: `r_cone(r_disk) = r_cone(rand(r_disk)) = sin_from_cos(mix(cos_angle, 1, r_disk^2))`.
     * In practice, we need to replace `rand` with `1 - rand` to preserve the stratification,
     * but since it's uniform, that's fine. */
    float2 xy = sample_uniform_disk(rand);
    const float r2 = len_squared(xy);

    /* Equivalent to `mix(cos_angle, 1.0f, 1.0f - r2)`. */
    *cos_theta = 1.0f - r2 * one_minus_cos_angle;

    /* Remap disk radius to cone radius, equivalent to `xy *= sin_theta / sqrt(r2)`. */
    xy *= safe_sqrtf(one_minus_cos_angle * (2.0f - one_minus_cos_angle * r2));

    *pdf = M_1_2PI_F / one_minus_cos_angle;

    float3 T;
    float3 B;
    make_orthonormals(N, &T, &B);
    return xy.x * T + xy.y * B + *cos_theta * N;
  }

  *cos_theta = 1.0f;
  *pdf = 1.0f;

  return N;
}

/* sample uniform point on the surface of a sphere */
ccl_device float3 sample_uniform_sphere(const float2 rand)
{
  const float z = 1.0f - 2.0f * rand.x;
  const float r = sin_from_cos(z);
  const float phi = M_2PI_F * rand.y;

  return make_float3(polar_to_cartesian(r, phi), z);
}

/* sample point in unit polygon with given number of corners and rotation */
ccl_device float2 regular_polygon_sample(const float corners, float rotation, const float2 rand)
{
  float u = rand.x;
  float v = rand.y;

  /* sample corner number and reuse u */
  const float corner = floorf(u * corners);
  u = u * corners - corner;

  /* uniform sampled triangle weights */
  u = sqrtf(u);
  v = v * u;
  u = 1.0f - u;

  /* point in triangle */
  const float angle = M_PI_F / corners;
  const float2 p = make_float2((u + v) * cosf(angle), (u - v) * sinf(angle));

  /* rotate */
  rotation += corner * 2.0f * angle;

  const float cr = cosf(rotation);
  const float sr = sinf(rotation);

  return make_float2(cr * p.x - sr * p.y, sr * p.x + cr * p.y);
}

/* Generate random variable x following geometric distribution p(x) = r * (1 - r)^x, 0 <= p <= 1.
 * Also compute the probability mass function pmf.
 * The sampled order is truncated at `cut_off`. */
ccl_device_inline int sample_geometric_distribution(const float rand,
                                                    const float r,
                                                    ccl_private float &pmf,
                                                    const int cut_off = INT_MAX)
{
  const int n = min(int(floorf(logf(rand) / logf(1.0f - r))), cut_off);
  pmf = (n == cut_off) ? powf(1.0f - r, n) : r * powf(1.0f - r, n);
  return n;
}

/* Generate random variable x following exponential distribution p(x) = lambda * exp(-lambda * x),
 * where lambda > 0 is the rate parameter. */
ccl_device_inline float sample_exponential_distribution(const float rand, const float lambda)
{
  return -logf(1.0f - rand) / lambda;
}

/* Generate random variable x following bounded exponential distribution
 * p(x) = lambda * exp(-lambda * x) / (exp(-lambda * t.min) - exp(-lambda * t.max)),
 * where lambda > 0 is the rate parameter.
 * The generated sample lies in (t.min, t.max). */
ccl_device_inline float sample_exponential_distribution(const float rand,
                                                        const float lambda,
                                                        const Interval<float> t)
{
  const float attenuation = 1.0f - expf(lambda * (t.min - t.max));
  return clamp(t.min - logf(1.0f - rand * attenuation) / lambda, t.min, t.max);
}

ccl_device_inline Spectrum pdf_exponential_distribution(const float x,
                                                        const Spectrum lambda,
                                                        const Interval<float> t)
{
  const Spectrum attenuation = exp(-lambda * t.min) - exp(-lambda * t.max);
  return safe_divide(lambda * exp(-lambda * clamp(x, t.min, t.max)), attenuation);
}

CCL_NAMESPACE_END

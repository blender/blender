/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

CCL_NAMESPACE_BEGIN

/* distribute uniform xy on [0,1] over unit disk [-1,1] */
ccl_device void to_unit_disk(ccl_private float2 *rand)
{
  float phi = M_2PI_F * rand->x;
  float r = sqrtf(rand->y);

  rand->x = r * cosf(phi);
  rand->y = r * sinf(phi);
}

/* Distribute 2D uniform random samples on [0, 1] over unit disk [-1, 1], with concentric mapping
 * to better preserve stratification for some RNG sequences. */
ccl_device float2 concentric_sample_disk(const float2 rand)
{
  float phi, r;
  float a = 2.0f * rand.x - 1.0f;
  float b = 2.0f * rand.y - 1.0f;

  if (a == 0.0f && b == 0.0f) {
    return zero_float2();
  }
  else if (a * a > b * b) {
    r = a;
    phi = M_PI_4_F * (b / a);
  }
  else {
    r = b;
    phi = M_PI_2_F - M_PI_4_F * (a / b);
  }

  return make_float2(r * cosf(phi), r * sinf(phi));
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

/* sample direction with cosine weighted distributed in hemisphere */
ccl_device_inline void sample_cos_hemisphere(const float3 N,
                                             float2 rand,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf)
{
  to_unit_disk(&rand);
  float costheta = safe_sqrtf(1.0f - len_squared(rand));

  float3 T, B;
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
  float z = rand.x;
  float r = sin_from_cos(z);
  float phi = M_2PI_F * rand.y;
  float x = r * cosf(phi);
  float y = r * sinf(phi);

  float3 T, B;
  make_orthonormals(N, &T, &B);
  *wo = x * T + y * B + z * N;
  *pdf = 0.5f * M_1_PI_F;
}

/* sample direction uniformly distributed in cone */
ccl_device_inline void sample_uniform_cone(
    const float3 N, float angle, const float2 rand, ccl_private float3 *wo, ccl_private float *pdf)
{
  const float cosThetaMin = cosf(angle);
  const float cosTheta = mix(cosThetaMin, 1.0f, rand.x);
  const float sinTheta = sin_from_cos(cosTheta);
  const float phi = M_2PI_F * rand.y;
  const float x = sinTheta * cosf(phi);
  const float y = sinTheta * sinf(phi);
  const float z = cosTheta;

  float3 T, B;
  make_orthonormals(N, &T, &B);
  *wo = x * T + y * B + z * N;
  *pdf = M_1_2PI_F / (1.0f - cosThetaMin);
}

ccl_device_inline float pdf_uniform_cone(const float3 N, float3 D, float angle)
{
  float zMin = cosf(angle);
  float z = precise_angle(N, D);
  if (z < angle) {
    return M_1_2PI_F / (1.0f - zMin);
  }
  return 0.0f;
}

/* Uniformly sample a direction in a cone of given angle around `N`. Use concentric mapping to
 * better preserve stratification. Return the angle between `N` and the sampled direction as
 * `cos_theta`.
 * Pass `1 - cos(angle)` as argument instead of `angle` to alleviate precision issues at small
 * angles (see sphere light for reference). */
ccl_device_inline void sample_uniform_cone_concentric(const float3 N,
                                                      const float one_minus_cos_angle,
                                                      const float2 rand,
                                                      ccl_private float *cos_theta,
                                                      ccl_private float3 *wo,
                                                      ccl_private float *pdf)
{
  if (one_minus_cos_angle > 0) {
    /* Map random number from 2D to 1D. */
    float2 xy = concentric_sample_disk(rand);
    const float r2 = len_squared(xy);

    /* Equivalent to `mix(cos_angle, 1.0f, 1.0f - r2)` */
    *cos_theta = 1.0f - r2 * one_minus_cos_angle;

    /* Equivalent to `xy *= sin_theta / sqrt(r2); */
    xy *= safe_sqrtf(one_minus_cos_angle * (2.0f - one_minus_cos_angle * r2));

    float3 T, B;
    make_orthonormals(N, &T, &B);

    *wo = xy.x * T + xy.y * B + *cos_theta * N;
    *pdf = M_1_2PI_F / one_minus_cos_angle;
  }
  else {
    *cos_theta = 1.0f;
    *wo = N;
    *pdf = 1.0f;
  }
}

/* sample uniform point on the surface of a sphere */
ccl_device float3 sample_uniform_sphere(const float2 rand)
{
  float z = 1.0f - 2.0f * rand.x;
  float r = sin_from_cos(z);
  float phi = M_2PI_F * rand.y;
  float x = r * cosf(phi);
  float y = r * sinf(phi);

  return make_float3(x, y, z);
}

/* sample point in unit polygon with given number of corners and rotation */
ccl_device float2 regular_polygon_sample(float corners, float rotation, const float2 rand)
{
  float u = rand.x, v = rand.y;

  /* sample corner number and reuse u */
  float corner = floorf(u * corners);
  u = u * corners - corner;

  /* uniform sampled triangle weights */
  u = sqrtf(u);
  v = v * u;
  u = 1.0f - u;

  /* point in triangle */
  float angle = M_PI_F / corners;
  float2 p = make_float2((u + v) * cosf(angle), (u - v) * sinf(angle));

  /* rotate */
  rotation += corner * 2.0f * angle;

  float cr = cosf(rotation);
  float sr = sinf(rotation);

  return make_float2(cr * p.x - sr * p.y, sr * p.x + cr * p.y);
}

CCL_NAMESPACE_END

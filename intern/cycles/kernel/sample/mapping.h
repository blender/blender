/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

CCL_NAMESPACE_BEGIN

/* distribute uniform xy on [0,1] over unit disk [-1,1] */
ccl_device void to_unit_disk(ccl_private float *x, ccl_private float *y)
{
  float phi = M_2PI_F * (*x);
  float r = sqrtf(*y);

  *x = r * cosf(phi);
  *y = r * sinf(phi);
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
ccl_device_inline void sample_cos_hemisphere(
    const float3 N, float randu, float randv, ccl_private float3 *wo, ccl_private float *pdf)
{
  to_unit_disk(&randu, &randv);
  float costheta = sqrtf(max(1.0f - randu * randu - randv * randv, 0.0f));
  float3 T, B;
  make_orthonormals(N, &T, &B);
  *wo = randu * T + randv * B + costheta * N;
  *pdf = costheta * M_1_PI_F;
}

/* sample direction uniformly distributed in hemisphere */
ccl_device_inline void sample_uniform_hemisphere(
    const float3 N, float randu, float randv, ccl_private float3 *wo, ccl_private float *pdf)
{
  float z = randu;
  float r = sqrtf(max(0.0f, 1.0f - z * z));
  float phi = M_2PI_F * randv;
  float x = r * cosf(phi);
  float y = r * sinf(phi);

  float3 T, B;
  make_orthonormals(N, &T, &B);
  *wo = x * T + y * B + z * N;
  *pdf = 0.5f * M_1_PI_F;
}

/* sample direction uniformly distributed in cone */
ccl_device_inline void sample_uniform_cone(const float3 N,
                                           float angle,
                                           float randu,
                                           float randv,
                                           ccl_private float3 *wo,
                                           ccl_private float *pdf)
{
  const float cosThetaMin = cosf(angle);
  const float cosTheta = mix(cosThetaMin, 1.0f, randu);
  const float sinTheta = sin_from_cos(cosTheta);
  const float phi = M_2PI_F * randv;
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
  float z = dot(N, D);
  if (z > zMin) {
    return M_1_2PI_F / (1.0f - zMin);
  }
  return 0.0f;
}

/* sample uniform point on the surface of a sphere */
ccl_device float3 sample_uniform_sphere(float u1, float u2)
{
  float z = 1.0f - 2.0f * u1;
  float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
  float phi = M_2PI_F * u2;
  float x = r * cosf(phi);
  float y = r * sinf(phi);

  return make_float3(x, y, z);
}

/* distribute uniform xy on [0,1] over unit disk [-1,1], with concentric mapping
 * to better preserve stratification for some RNG sequences */
ccl_device float2 concentric_sample_disk(float u1, float u2)
{
  float phi, r;
  float a = 2.0f * u1 - 1.0f;
  float b = 2.0f * u2 - 1.0f;

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

/* sample point in unit polygon with given number of corners and rotation */
ccl_device float2 regular_polygon_sample(float corners, float rotation, float u, float v)
{
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

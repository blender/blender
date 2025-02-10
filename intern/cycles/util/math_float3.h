/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"
#include "util/math_float4.h"
#include "util/types_float3.h"
#include "util/types_float4.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline float3 zero_float3()
{
#ifdef __KERNEL_SSE__
  return float3(_mm_setzero_ps());
#else
  return make_float3(0.0f, 0.0f, 0.0f);
#endif
}

ccl_device_inline float3 one_float3()
{
  return make_float3(1.0f, 1.0f, 1.0f);
}

ccl_device_inline float3 reciprocal(const float3 a)
{
#ifdef __KERNEL_SSE__
  /* Don't use _mm_rcp_ps due to poor precision. */
  return float3(_mm_div_ps(_mm_set_ps1(1.0f), a.m128));
#else
  return make_float3(1.0f / a.x, 1.0f / a.y, 1.0f / a.z);
#endif
}

#ifndef __KERNEL_METAL__

ccl_device_inline float3 operator-(const float3 &a)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_xor_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))));
#  else
  return make_float3(-a.x, -a.y, -a.z);
#  endif
}

ccl_device_inline float3 operator*(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_mul_ps(a.m128, b.m128));
#  else
  return make_float3(a.x * b.x, a.y * b.y, a.z * b.z);
#  endif
}

ccl_device_inline float3 operator*(const float3 a, const float f)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_mul_ps(a.m128, _mm_set1_ps(f)));
#  else
  return make_float3(a.x * f, a.y * f, a.z * f);
#  endif
}

ccl_device_inline float3 operator*(const float f, const float3 a)
{
#  if defined(__KERNEL_SSE__)
  return float3(_mm_mul_ps(_mm_set1_ps(f), a.m128));
#  else
  return make_float3(a.x * f, a.y * f, a.z * f);
#  endif
}

ccl_device_inline float3 operator/(const float f, const float3 a)
{
#  if defined(__KERNEL_SSE__)
  return float3(_mm_div_ps(_mm_set1_ps(f), a.m128));
#  else
  return make_float3(f / a.x, f / a.y, f / a.z);
#  endif
}

ccl_device_inline float3 operator/(const float3 a, const float f)
{
#  if defined(__KERNEL_SSE__)
  return float3(_mm_div_ps(a.m128, _mm_set1_ps(f)));
#  else
  float invf = 1.0f / f;
  return make_float3(a.x * invf, a.y * invf, a.z * invf);
#  endif
}

ccl_device_inline float3 operator/(const float3 a, const float3 b)
{
#  if defined(__KERNEL_SSE__)
  return float3(_mm_div_ps(a.m128, b.m128));
#  else
  return make_float3(a.x / b.x, a.y / b.y, a.z / b.z);
#  endif
}

ccl_device_inline float3 operator+(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_add_ps(a.m128, b.m128));
#  else
  return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
#  endif
}

ccl_device_inline float3 operator+(const float3 a, const float f)
{
  return a + make_float3(f);
}

ccl_device_inline float3 operator-(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_sub_ps(a.m128, b.m128));
#  else
  return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
#  endif
}

ccl_device_inline float3 operator-(const float3 a, const float f)
{
  return a - make_float3(f);
}

ccl_device_inline float3 operator+=(float3 &a, const float3 b)
{
  return a = a + b;
}

ccl_device_inline float3 operator-=(float3 &a, const float3 b)
{
  return a = a - b;
}

ccl_device_inline float3 operator*=(float3 &a, const float3 b)
{
  return a = a * b;
}

ccl_device_inline float3 operator*=(float3 &a, const float f)
{
  return a = a * f;
}

ccl_device_inline float3 operator/=(float3 &a, const float3 b)
{
  return a = a / b;
}

ccl_device_inline float3 operator/=(float3 &a, const float f)
{
  const float invf = 1.0f / f;
  return a = a * invf;
}

#  if !(defined(__KERNEL_METAL__) || defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__) || \
        defined(__KERNEL_ONEAPI__))
ccl_device_inline packed_float3 operator*=(packed_float3 &a, const float3 b)
{
  a = float3(a) * b;
  return a;
}

ccl_device_inline packed_float3 operator*=(packed_float3 &a, const float f)
{
  a = float3(a) * f;
  return a;
}

ccl_device_inline packed_float3 operator/=(packed_float3 &a, const float3 b)
{
  a = float3(a) / b;
  return a;
}

ccl_device_inline packed_float3 operator/=(packed_float3 &a, const float f)
{
  a = float3(a) / f;
  return a;
}
#  endif

ccl_device_inline bool operator==(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  return (_mm_movemask_ps(_mm_cmpeq_ps(a.m128, b.m128)) & 7) == 7;
#  else
  return (a.x == b.x && a.y == b.y && a.z == b.z);
#  endif
}

ccl_device_inline bool operator!=(const float3 a, const float3 b)
{
  return !(a == b);
}

ccl_device_inline float dot(const float3 a, const float3 b)
{
#  if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  return _mm_cvtss_f32(_mm_dp_ps(a, b, 0x7F));
#  else
  return a.x * b.x + a.y * b.y + a.z * b.z;
#  endif
}

#endif

ccl_device_inline float dot_xy(const float3 a, const float3 b)
{
#if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  return _mm_cvtss_f32(_mm_hadd_ps(_mm_mul_ps(a, b), b));
#else
  return a.x * b.x + a.y * b.y;
#endif
}

ccl_device_inline float len(const float3 a)
{
#if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(a.m128, a.m128, 0x7F)));
#else
  return sqrtf(dot(a, a));
#endif
}

ccl_device_inline float reduce_min(const float3 a)
{
  return min(min(a.x, a.y), a.z);
}

ccl_device_inline float reduce_max(const float3 a)
{
  return max(max(a.x, a.y), a.z);
}

ccl_device_inline float len_squared(const float3 a)
{
  return dot(a, a);
}

#ifndef __KERNEL_METAL__

ccl_device_inline float distance(const float3 a, const float3 b)
{
  return len(a - b);
}

ccl_device_inline float3 cross(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  const float4 x = float4(a.m128);
  const float4 y = shuffle<1, 2, 0, 3>(float4(b.m128));
  const float4 z = float4(_mm_mul_ps(shuffle<1, 2, 0, 3>(float4(a.m128)), float4(b.m128)));

  return float3(shuffle<1, 2, 0, 3>(msub(x, y, z)).m128);
#  else
  return make_float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
#  endif
}

ccl_device_inline float3 normalize(const float3 a)
{
#  if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  const __m128 norm = _mm_sqrt_ps(_mm_dp_ps(a.m128, a.m128, 0x7F));
  return float3(_mm_div_ps(a.m128, norm));
#  else
  return a / len(a);
#  endif
}

ccl_device_inline float3 min(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_min_ps(a.m128, b.m128));
#  else
  return make_float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
#  endif
}

ccl_device_inline float3 max(const float3 a, const float3 b)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_max_ps(a.m128, b.m128));
#  else
  return make_float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
#  endif
}

ccl_device_inline float3 clamp(const float3 a, const float3 mn, const float3 mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline float3 fabs(const float3 a)
{
#  ifdef __KERNEL_SSE__
#    ifdef __KERNEL_NEON__
  return float3(vabsq_f32(a.m128));
#    else
  __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
  return float3(_mm_and_ps(a.m128, mask));
#    endif
#  else
  return make_float3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
#  endif
}

ccl_device_inline float3 fmod(const float3 a, const float b)
{
  return make_float3(fmodf(a.x, b), fmodf(a.y, b), fmodf(a.z, b));
}

ccl_device_inline float3 sqrt(const float3 a)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_sqrt_ps(a));
#  else
  return make_float3(sqrtf(a.x), sqrtf(a.y), sqrtf(a.z));
#  endif
}

ccl_device_inline float3 floor(const float3 a)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_floor_ps(a));
#  else
  return make_float3(floorf(a.x), floorf(a.y), floorf(a.z));
#  endif
}

ccl_device_inline float3 ceil(const float3 a)
{
#  ifdef __KERNEL_SSE__
  return float3(_mm_ceil_ps(a));
#  else
  return make_float3(ceilf(a.x), ceilf(a.y), ceilf(a.z));
#  endif
}

ccl_device_inline float3 mix(const float3 a, const float3 b, const float t)
{
  return a + t * (b - a);
}

ccl_device_inline float3 saturate(const float3 a)
{
  return make_float3(saturatef(a.x), saturatef(a.y), saturatef(a.z));
}

ccl_device_inline float3 exp(const float3 v)
{
  return make_float3(expf(v.x), expf(v.y), expf(v.z));
}

ccl_device_inline float3 log(const float3 v)
{
  return make_float3(logf(v.x), logf(v.y), logf(v.z));
}

ccl_device_inline float3 cos(const float3 v)
{
  return make_float3(cosf(v.x), cosf(v.y), cosf(v.z));
}

ccl_device_inline float3 reflect(const float3 incident, const float3 unit_normal)
{
  return incident - 2.0f * unit_normal * dot(incident, unit_normal);
}

ccl_device_inline float3 refract(const float3 incident, const float3 normal, const float eta)
{
  const float k = 1.0f - eta * eta * (1.0f - dot(normal, incident) * dot(normal, incident));
  if (k < 0.0f) {
    return zero_float3();
  }
  return eta * incident - (eta * dot(normal, incident) + sqrt(k)) * normal;
}

ccl_device_inline float3 faceforward(const float3 vector,
                                     const float3 incident,
                                     const float3 reference)
{
  return (dot(reference, incident) < 0.0f) ? vector : -vector;
}
#endif

ccl_device_inline float3 project(const float3 v, const float3 v_proj)
{
  const float len_squared = dot(v_proj, v_proj);
  return (len_squared != 0.0f) ? (dot(v, v_proj) / len_squared) * v_proj : zero_float3();
}

ccl_device_inline float3 normalize_len(const float3 a, ccl_private float *t)
{
  *t = len(a);
  const float x = 1.0f / *t;
  return a * x;
}

ccl_device_inline float3 safe_normalize(const float3 a)
{
  const float t = len(a);
  return (t != 0.0f) ? a * (1.0f / t) : a;
}

ccl_device_inline float3 safe_normalize_fallback(const float3 a, const float3 fallback)
{
  const float t = len(a);
  return (t != 0.0f) ? a * (1.0f / t) : fallback;
}

ccl_device_inline float3 safe_normalize_len(const float3 a, ccl_private float *t)
{
  *t = len(a);
  return (*t != 0.0f) ? a / (*t) : a;
}

ccl_device_inline float3 safe_divide(const float3 a, const float3 b)
{
  return make_float3((b.x != 0.0f) ? a.x / b.x : 0.0f,
                     (b.y != 0.0f) ? a.y / b.y : 0.0f,
                     (b.z != 0.0f) ? a.z / b.z : 0.0f);
}

ccl_device_inline float3 safe_divide(const float3 a, const float b)
{
  return (b != 0.0f) ? a / b : zero_float3();
}

ccl_device_inline float3 interp(const float3 a, const float3 b, const float t)
{
  return a + t * (b - a);
}

ccl_device_inline float3 sqr(const float3 a)
{
  return a * a;
}

ccl_device_inline bool is_zero(const float3 a)
{
#ifdef __KERNEL_SSE__
  return a == make_float3(0.0f);
#else
  return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f);
#endif
}

ccl_device_inline float reduce_add(const float3 a)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_NEON__)
  __m128 t = a.m128;
  t = vsetq_lane_f32(0.0f, t, 3);
  return vaddvq_f32(t);
#else
  return (a.x + a.y + a.z);
#endif
}

ccl_device_inline float average(const float3 a)
{
  return reduce_add(a) * (1.0f / 3.0f);
}

ccl_device_inline bool isequal(const float3 a, const float3 b)
{
#if defined(__KERNEL_METAL__)
  return all(a == b);
#else
  return a == b;
#endif
}

/* Consistent name for this would be pow, but HIP compiler crashes in name mangling. */
ccl_device_inline float3 power(const float3 v, const float e)
{
  return make_float3(powf(v.x, e), powf(v.y, e), powf(v.z, e));
}

ccl_device_inline bool isfinite_safe(const float3 v)
{
  return isfinite_safe(v.x) && isfinite_safe(v.y) && isfinite_safe(v.z);
}

ccl_device_inline float3 ensure_finite(const float3 v)
{
  float3 r = v;
  if (!isfinite_safe(r.x)) {
    r.x = 0.0f;
  }
  if (!isfinite_safe(r.y)) {
    r.y = 0.0f;
  }
  if (!isfinite_safe(r.z)) {
    r.z = 0.0f;
  }
  return r;
}

/* Triangle */

ccl_device_inline float triangle_area(const ccl_private float3 &v1,
                                      const ccl_private float3 &v2,
                                      const ccl_private float3 &v3)
{
  return len(cross(v3 - v2, v1 - v2)) * 0.5f;
}

/* Orthonormal vectors */

ccl_device_inline void make_orthonormals(const float3 N,
                                         ccl_private float3 *a,
                                         ccl_private float3 *b)
{
#if 0
  if (fabsf(N.y) >= 0.999f) {
    *a = make_float3(1, 0, 0);
    *b = make_float3(0, 0, 1);
    return;
  }
  if (fabsf(N.z) >= 0.999f) {
    *a = make_float3(1, 0, 0);
    *b = make_float3(0, 1, 0);
    return;
  }
#endif

  if (N.x != N.y || N.x != N.z) {
    *a = make_float3(N.z - N.y, N.x - N.z, N.y - N.x);  //(1,1,1)x N
  }
  else {
    *a = make_float3(N.z - N.y, N.x + N.z, -N.y - N.x);  //(-1,1,1)x N
  }

  *a = normalize(*a);
  *b = cross(N, *a);
}

/* Rotation of point around axis and angle */

ccl_device_inline float3 rotate_around_axis(const float3 p, const float3 axis, const float angle)
{
  const float costheta = cosf(angle);
  const float sintheta = sinf(angle);
  float3 r;

  r.x = ((costheta + (1 - costheta) * axis.x * axis.x) * p.x) +
        (((1 - costheta) * axis.x * axis.y - axis.z * sintheta) * p.y) +
        (((1 - costheta) * axis.x * axis.z + axis.y * sintheta) * p.z);

  r.y = (((1 - costheta) * axis.x * axis.y + axis.z * sintheta) * p.x) +
        ((costheta + (1 - costheta) * axis.y * axis.y) * p.y) +
        (((1 - costheta) * axis.y * axis.z - axis.x * sintheta) * p.z);

  r.z = (((1 - costheta) * axis.x * axis.z - axis.y * sintheta) * p.x) +
        (((1 - costheta) * axis.y * axis.z + axis.x * sintheta) * p.y) +
        ((costheta + (1 - costheta) * axis.z * axis.z) * p.z);

  return r;
}

/* Calculate the angle between the two vectors a and b.
 * The usual approach `acos(dot(a, b))` has severe precision issues for small angles,
 * which are avoided by this method.
 * Based on "Mangled Angles" from https://people.eecs.berkeley.edu/~wkahan/Mindless.pdf
 */
ccl_device_inline float precise_angle(const float3 a, const float3 b)
{
  return 2.0f * atan2f(len(a - b), len(a + b));
}

/* Tangent of the angle between vectors a and b. */
ccl_device_inline float tan_angle(const float3 a, const float3 b)
{
  return len(cross(a, b)) / dot(a, b);
}

/* projections */
ccl_device_inline float2 map_to_tube(const float3 co)
{
  float len;
  float u;
  float v;
  len = sqrtf(co.x * co.x + co.y * co.y);
  if (len > 0.0f) {
    u = (1.0f - (atan2f(co.x / len, co.y / len) / M_PI_F)) * 0.5f;
    v = (co.z + 1.0f) * 0.5f;
  }
  else {
    u = v = 0.0f;
  }
  return make_float2(u, v);
}

ccl_device_inline float2 map_to_sphere(const float3 co)
{
  const float l = dot(co, co);
  float u;
  float v;
  if (l > 0.0f) {
    if (UNLIKELY(co.x == 0.0f && co.y == 0.0f)) {
      u = 0.0f; /* Otherwise domain error. */
    }
    else {
      u = (0.5f - atan2f(co.x, co.y) * M_1_2PI_F);
    }
    v = 1.0f - safe_acosf(co.z / sqrtf(l)) * M_1_PI_F;
  }
  else {
    u = v = 0.0f;
  }
  return make_float2(u, v);
}

CCL_NAMESPACE_END

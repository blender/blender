/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_MATH_FLOAT3_H__
#define __UTIL_MATH_FLOAT3_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

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

#if defined(__KERNEL_METAL__)

ccl_device_inline float3 rcp(float3 a)
{
  return make_float3(1.0f / a.x, 1.0f / a.y, 1.0f / a.z);
}

#else

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
  return a + make_float3(f, f, f);
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
  return a - make_float3(f, f, f);
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

ccl_device_inline float3 operator*=(float3 &a, float f)
{
  return a = a * f;
}

ccl_device_inline float3 operator/=(float3 &a, const float3 b)
{
  return a = a / b;
}

ccl_device_inline float3 operator/=(float3 &a, float f)
{
  float invf = 1.0f / f;
  return a = a * invf;
}

#  if !(defined(__KERNEL_METAL__) || defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__))
ccl_device_inline packed_float3 operator*=(packed_float3 &a, const float3 b)
{
  a = float3(a) * b;
  return a;
}

ccl_device_inline packed_float3 operator*=(packed_float3 &a, float f)
{
  a = float3(a) * f;
  return a;
}

ccl_device_inline packed_float3 operator/=(packed_float3 &a, const float3 b)
{
  a = float3(a) / b;
  return a;
}

ccl_device_inline packed_float3 operator/=(packed_float3 &a, float f)
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
#  if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
  return _mm_cvtss_f32(_mm_dp_ps(a, b, 0x7F));
#  else
  return a.x * b.x + a.y * b.y + a.z * b.z;
#  endif
}

#endif

ccl_device_inline float dot_xy(const float3 a, const float3 b)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
  return _mm_cvtss_f32(_mm_hadd_ps(_mm_mul_ps(a, b), b));
#else
  return a.x * b.x + a.y * b.y;
#endif
}

ccl_device_inline float len(const float3 a)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
  return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(a.m128, a.m128, 0x7F)));
#else
  return sqrtf(dot(a, a));
#endif
}

ccl_device_inline float reduce_min(float3 a)
{
  return min(min(a.x, a.y), a.z);
}

ccl_device_inline float reduce_max(float3 a)
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
#  if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
  __m128 norm = _mm_sqrt_ps(_mm_dp_ps(a.m128, a.m128, 0x7F));
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

ccl_device_inline float3 mix(const float3 a, const float3 b, float t)
{
  return a + t * (b - a);
}

ccl_device_inline float3 rcp(const float3 a)
{
#  ifdef __KERNEL_SSE__
  /* Don't use _mm_rcp_ps due to poor precision. */
  return float3(_mm_div_ps(_mm_set_ps1(1.0f), a.m128));
#  else
  return make_float3(1.0f / a.x, 1.0f / a.y, 1.0f / a.z);
#  endif
}

ccl_device_inline float3 saturate(float3 a)
{
  return make_float3(saturatef(a.x), saturatef(a.y), saturatef(a.z));
}

ccl_device_inline float3 exp(float3 v)
{
  return make_float3(expf(v.x), expf(v.y), expf(v.z));
}

ccl_device_inline float3 log(float3 v)
{
  return make_float3(logf(v.x), logf(v.y), logf(v.z));
}

ccl_device_inline float3 reflect(const float3 incident, const float3 normal)
{
  float3 unit_normal = normalize(normal);
  return incident - 2.0f * unit_normal * dot(incident, unit_normal);
}

ccl_device_inline float3 refract(const float3 incident, const float3 normal, const float eta)
{
  float k = 1.0f - eta * eta * (1.0f - dot(normal, incident) * dot(normal, incident));
  if (k < 0.0f)
    return zero_float3();
  else
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
  float len_squared = dot(v_proj, v_proj);
  return (len_squared != 0.0f) ? (dot(v, v_proj) / len_squared) * v_proj : zero_float3();
}

ccl_device_inline float3 normalize_len(const float3 a, ccl_private float *t)
{
  *t = len(a);
  float x = 1.0f / *t;
  return a * x;
}

ccl_device_inline float3 safe_normalize(const float3 a)
{
  float t = len(a);
  return (t != 0.0f) ? a * (1.0f / t) : a;
}

ccl_device_inline float3 safe_normalize_fallback(const float3 a, const float3 fallback)
{
  float t = len(a);
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

ccl_device_inline float3 interp(float3 a, float3 b, float t)
{
  return a + t * (b - a);
}

ccl_device_inline float3 sqr(float3 a)
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
  t[3] = 0.0f;
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
ccl_device_inline float3 power(float3 v, float e)
{
  return make_float3(powf(v.x, e), powf(v.y, e), powf(v.z, e));
}

ccl_device_inline bool isfinite_safe(float3 v)
{
  return isfinite_safe(v.x) && isfinite_safe(v.y) && isfinite_safe(v.z);
}

ccl_device_inline float3 ensure_finite(float3 v)
{
  if (!isfinite_safe(v.x))
    v.x = 0.0f;
  if (!isfinite_safe(v.y))
    v.y = 0.0f;
  if (!isfinite_safe(v.z))
    v.z = 0.0f;
  return v;
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_FLOAT3_H__ */

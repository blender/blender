/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 Blender Foundation */

#ifndef __UTIL_MATH_FLOAT8_H__
#define __UTIL_MATH_FLOAT8_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

ccl_device_inline float8 operator+(const float8 &a, const float8 &b);
ccl_device_inline float8 operator+(const float8 &a, const float f);
ccl_device_inline float8 operator+(const float f, const float8 &a);

ccl_device_inline float8 operator-(const float8 &a);
ccl_device_inline float8 operator-(const float8 &a, const float8 &b);
ccl_device_inline float8 operator-(const float8 &a, const float f);
ccl_device_inline float8 operator-(const float f, const float8 &a);

ccl_device_inline float8 operator*(const float8 &a, const float8 &b);
ccl_device_inline float8 operator*(const float8 &a, const float f);
ccl_device_inline float8 operator*(const float f, const float8 &a);

ccl_device_inline float8 operator/(const float8 &a, const float8 &b);
ccl_device_inline float8 operator/(const float8 &a, float f);
ccl_device_inline float8 operator/(const float f, const float8 &a);

ccl_device_inline float8 operator+=(float8 &a, const float8 &b);

ccl_device_inline float8 operator*=(float8 &a, const float8 &b);
ccl_device_inline float8 operator*=(float8 &a, float f);

ccl_device_inline float8 operator/=(float8 &a, float f);

ccl_device_inline bool operator==(const float8 &a, const float8 &b);

ccl_device_inline float8 rcp(const float8 &a);
ccl_device_inline float8 sqrt(const float8 &a);
ccl_device_inline float8 sqr(const float8 &a);
ccl_device_inline bool is_zero(const float8 &a);
ccl_device_inline float average(const float8 &a);
ccl_device_inline float8 min(const float8 &a, const float8 &b);
ccl_device_inline float8 max(const float8 &a, const float8 &b);
ccl_device_inline float8 clamp(const float8 &a, const float8 &mn, const float8 &mx);
ccl_device_inline float8 fabs(const float8 &a);
ccl_device_inline float8 mix(const float8 &a, const float8 &b, float t);

ccl_device_inline float8 safe_divide(const float8 a, const float b);
ccl_device_inline float8 safe_divide(const float8 a, const float8 b);

ccl_device_inline float reduce_min(const float8 &a);
ccl_device_inline float reduce_max(const float8 &a);
ccl_device_inline float reduce_add(const float8 &a);

ccl_device_inline float8 saturate(const float8 &a);
ccl_device_inline bool isequal(const float8 a, const float8 b);

/*******************************************************************************
 * Definition.
 */

ccl_device_inline float8 zero_float8()
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_setzero_ps());
#else
  return make_float8(0.0f);
#endif
}

ccl_device_inline float8 one_float8()
{
  return make_float8(1.0f);
}

ccl_device_inline float8 operator+(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_add_ps(a.m256, b.m256));
#else
  return make_float8(
      a.a + b.a, a.b + b.b, a.c + b.c, a.d + b.d, a.e + b.e, a.f + b.f, a.g + b.g, a.h + b.h);
#endif
}

ccl_device_inline float8 operator+(const float8 &a, const float f)
{
  return a + make_float8(f);
}

ccl_device_inline float8 operator+(const float f, const float8 &a)
{
  return make_float8(f) + a;
}

ccl_device_inline float8 operator-(const float8 &a)
{
#ifdef __KERNEL_AVX2__
  __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x80000000));
  return float8(_mm256_xor_ps(a.m256, mask));
#else
  return make_float8(-a.a, -a.b, -a.c, -a.d, -a.e, -a.f, -a.g, -a.h);
#endif
}

ccl_device_inline float8 operator-(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_sub_ps(a.m256, b.m256));
#else
  return make_float8(
      a.a - b.a, a.b - b.b, a.c - b.c, a.d - b.d, a.e - b.e, a.f - b.f, a.g - b.g, a.h - b.h);
#endif
}

ccl_device_inline float8 operator-(const float8 &a, const float f)
{
  return a - make_float8(f);
}

ccl_device_inline float8 operator-(const float f, const float8 &a)
{
  return make_float8(f) - a;
}

ccl_device_inline float8 operator*(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_mul_ps(a.m256, b.m256));
#else
  return make_float8(
      a.a * b.a, a.b * b.b, a.c * b.c, a.d * b.d, a.e * b.e, a.f * b.f, a.g * b.g, a.h * b.h);
#endif
}

ccl_device_inline float8 operator*(const float8 &a, const float f)
{
  return a * make_float8(f);
}

ccl_device_inline float8 operator*(const float f, const float8 &a)
{
  return make_float8(f) * a;
}

ccl_device_inline float8 operator/(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_div_ps(a.m256, b.m256));
#else
  return make_float8(
      a.a / b.a, a.b / b.b, a.c / b.c, a.d / b.d, a.e / b.e, a.f / b.f, a.g / b.g, a.h / b.h);
#endif
}

ccl_device_inline float8 operator/(const float8 &a, const float f)
{
  return a / make_float8(f);
}

ccl_device_inline float8 operator/(const float f, const float8 &a)
{
  return make_float8(f) / a;
}

ccl_device_inline float8 operator+=(float8 &a, const float8 &b)
{
  return a = a + b;
}

ccl_device_inline float8 operator-=(float8 &a, const float8 &b)
{
  return a = a - b;
}

ccl_device_inline float8 operator*=(float8 &a, const float8 &b)
{
  return a = a * b;
}

ccl_device_inline float8 operator*=(float8 &a, float f)
{
  return a = a * f;
}

ccl_device_inline float8 operator/=(float8 &a, float f)
{
  return a = a / f;
}

ccl_device_inline bool operator==(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return (_mm256_movemask_ps(_mm256_castsi256_ps(
              _mm256_cmpeq_epi32(_mm256_castps_si256(a.m256), _mm256_castps_si256(b.m256)))) &
          0b11111111) == 0b11111111;
#else
  return (a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d && a.e == b.e && a.f == b.f &&
          a.g == b.g && a.h == b.h);
#endif
}

ccl_device_inline float8 rcp(const float8 &a)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_rcp_ps(a.m256));
#else
  return make_float8(1.0f / a.a,
                     1.0f / a.b,
                     1.0f / a.c,
                     1.0f / a.d,
                     1.0f / a.e,
                     1.0f / a.f,
                     1.0f / a.g,
                     1.0f / a.h);
#endif
}

ccl_device_inline float8 sqrt(const float8 &a)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_sqrt_ps(a.m256));
#else
  return make_float8(sqrtf(a.a),
                     sqrtf(a.b),
                     sqrtf(a.c),
                     sqrtf(a.d),
                     sqrtf(a.e),
                     sqrtf(a.f),
                     sqrtf(a.g),
                     sqrtf(a.h));
#endif
}

ccl_device_inline float8 sqr(const float8 &a)
{
  return a * a;
}

ccl_device_inline bool is_zero(const float8 &a)
{
  return a == make_float8(0.0f);
}

ccl_device_inline float average(const float8 &a)
{
  return reduce_add(a) / 8.0f;
}

ccl_device_inline float8 min(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_min_ps(a.m256, b.m256));
#else
  return make_float8(min(a.a, b.a),
                     min(a.b, b.b),
                     min(a.c, b.c),
                     min(a.d, b.d),
                     min(a.e, b.e),
                     min(a.f, b.f),
                     min(a.g, b.g),
                     min(a.h, b.h));
#endif
}

ccl_device_inline float8 max(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_max_ps(a.m256, b.m256));
#else
  return make_float8(max(a.a, b.a),
                     max(a.b, b.b),
                     max(a.c, b.c),
                     max(a.d, b.d),
                     max(a.e, b.e),
                     max(a.f, b.f),
                     max(a.g, b.g),
                     max(a.h, b.h));
#endif
}

ccl_device_inline float8 clamp(const float8 &a, const float8 &mn, const float8 &mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline float8 fabs(const float8 &a)
{
#ifdef __KERNEL_AVX2__
  return float8(_mm256_and_ps(a.m256, _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff))));
#else
  return make_float8(fabsf(a.a),
                     fabsf(a.b),
                     fabsf(a.c),
                     fabsf(a.d),
                     fabsf(a.e),
                     fabsf(a.f),
                     fabsf(a.g),
                     fabsf(a.h));
#endif
}

ccl_device_inline float8 mix(const float8 &a, const float8 &b, float t)
{
  return a + t * (b - a);
}

ccl_device_inline float reduce_min(const float8 &a)
{
  return min(min(min(a.a, a.b), min(a.c, a.d)), min(min(a.e, a.f), min(a.g, a.h)));
}

ccl_device_inline float reduce_max(const float8 &a)
{
  return max(max(max(a.a, a.b), max(a.c, a.d)), max(max(a.e, a.f), max(a.g, a.h)));
}

ccl_device_inline float reduce_add(const float8 &a)
{
#ifdef __KERNEL_AVX2__
  float8 b(_mm256_hadd_ps(a.m256, a.m256));
  float8 h(_mm256_hadd_ps(b.m256, b.m256));
  return h[0] + h[4];
#else
  return a.a + a.b + a.c + a.d + a.e + a.f + a.g + a.h;
#endif
}

ccl_device_inline float8 saturate(const float8 &a)
{
  return clamp(a, make_float8(0.0f), make_float8(1.0f));
}

ccl_device_inline bool isequal(const float8 a, const float8 b)
{
  return a == b;
}

ccl_device_inline float8 safe_divide(const float8 a, const float b)
{
  return (b != 0.0f) ? a / b : make_float8(0.0f);
}

ccl_device_inline float8 safe_divide(const float8 a, const float8 b)
{
  return make_float8((b.a != 0.0f) ? a.a / b.a : 0.0f,
                     (b.b != 0.0f) ? a.b / b.b : 0.0f,
                     (b.c != 0.0f) ? a.c / b.c : 0.0f,
                     (b.d != 0.0f) ? a.d / b.d : 0.0f,
                     (b.e != 0.0f) ? a.e / b.e : 0.0f,
                     (b.f != 0.0f) ? a.f / b.f : 0.0f,
                     (b.g != 0.0f) ? a.g / b.g : 0.0f,
                     (b.h != 0.0f) ? a.h / b.h : 0.0f);
}

ccl_device_inline float8 ensure_finite(float8 v)
{
  v.a = ensure_finite(v.a);
  v.b = ensure_finite(v.b);
  v.c = ensure_finite(v.c);
  v.d = ensure_finite(v.d);
  v.e = ensure_finite(v.e);
  v.f = ensure_finite(v.f);
  v.g = ensure_finite(v.g);
  v.h = ensure_finite(v.h);

  return v;
}

ccl_device_inline bool isfinite_safe(float8 v)
{
  return isfinite_safe(v.a) && isfinite_safe(v.b) && isfinite_safe(v.c) && isfinite_safe(v.d) &&
         isfinite_safe(v.e) && isfinite_safe(v.f) && isfinite_safe(v.g) && isfinite_safe(v.h);
}

ccl_device_inline float8 pow(float8 v, float e)
{
  return make_float8(powf(v.a, e),
                     powf(v.b, e),
                     powf(v.c, e),
                     powf(v.d, e),
                     powf(v.e, e),
                     powf(v.f, e),
                     powf(v.g, e),
                     powf(v.h, e));
}

ccl_device_inline float8 exp(float8 v)
{
  return make_float8(
      expf(v.a), expf(v.b), expf(v.c), expf(v.d), expf(v.e), expf(v.f), expf(v.g), expf(v.h));
}

ccl_device_inline float8 log(float8 v)
{
  return make_float8(
      logf(v.a), logf(v.b), logf(v.c), logf(v.d), logf(v.e), logf(v.f), logf(v.g), logf(v.h));
}

ccl_device_inline float dot(const float8 &a, const float8 &b)
{
#ifdef __KERNEL_AVX2__
  float8 t(_mm256_dp_ps(a.m256, b.m256, 0xFF));
  return t[0] + t[4];
#else
  return (a.a * b.a) + (a.b * b.b) + (a.c * b.c) + (a.d * b.d) + (a.e * b.e) + (a.f * b.f) +
         (a.g * b.g) + (a.h * b.h);
#endif
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_FLOAT8_H__ */

/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"
#include "util/types_float8.h"
#include "util/types_int8.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline vfloat8 zero_vfloat8()
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_setzero_ps());
#else
  return make_vfloat8(0.0f);
#endif
}

ccl_device_inline vfloat8 one_vfloat8()
{
  return make_vfloat8(1.0f);
}

ccl_device_inline vfloat8 operator+(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_add_ps(a.m256, b.m256));
#else
  return make_vfloat8(
      a.a + b.a, a.b + b.b, a.c + b.c, a.d + b.d, a.e + b.e, a.f + b.f, a.g + b.g, a.h + b.h);
#endif
}

ccl_device_inline vfloat8 operator+(const vfloat8 a, const float f)
{
  return a + make_vfloat8(f);
}

ccl_device_inline vfloat8 operator+(const float f, const vfloat8 a)
{
  return make_vfloat8(f) + a;
}

ccl_device_inline vfloat8 operator-(const vfloat8 a)
{
#ifdef __KERNEL_AVX__
  __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x80000000));
  return vfloat8(_mm256_xor_ps(a.m256, mask));
#else
  return make_vfloat8(-a.a, -a.b, -a.c, -a.d, -a.e, -a.f, -a.g, -a.h);
#endif
}

ccl_device_inline vfloat8 operator-(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_sub_ps(a.m256, b.m256));
#else
  return make_vfloat8(
      a.a - b.a, a.b - b.b, a.c - b.c, a.d - b.d, a.e - b.e, a.f - b.f, a.g - b.g, a.h - b.h);
#endif
}

ccl_device_inline vfloat8 operator-(const vfloat8 a, const float f)
{
  return a - make_vfloat8(f);
}

ccl_device_inline vfloat8 operator-(const float f, const vfloat8 a)
{
  return make_vfloat8(f) - a;
}

ccl_device_inline vfloat8 operator*(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_mul_ps(a.m256, b.m256));
#else
  return make_vfloat8(
      a.a * b.a, a.b * b.b, a.c * b.c, a.d * b.d, a.e * b.e, a.f * b.f, a.g * b.g, a.h * b.h);
#endif
}

ccl_device_inline vfloat8 operator*(const vfloat8 a, const float f)
{
  return a * make_vfloat8(f);
}

ccl_device_inline vfloat8 operator*(const float f, const vfloat8 a)
{
  return make_vfloat8(f) * a;
}

ccl_device_inline vfloat8 operator/(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_div_ps(a.m256, b.m256));
#else
  return make_vfloat8(
      a.a / b.a, a.b / b.b, a.c / b.c, a.d / b.d, a.e / b.e, a.f / b.f, a.g / b.g, a.h / b.h);
#endif
}

ccl_device_inline vfloat8 operator/(const vfloat8 a, const float f)
{
  return a / make_vfloat8(f);
}

ccl_device_inline vfloat8 operator/(const float f, const vfloat8 a)
{
  return make_vfloat8(f) / a;
}

ccl_device_inline vfloat8 operator+=(vfloat8 a, const vfloat8 b)
{
  return a = a + b;
}

ccl_device_inline vfloat8 operator-=(vfloat8 a, const vfloat8 b)
{
  return a = a - b;
}

ccl_device_inline vfloat8 operator*=(vfloat8 a, const vfloat8 b)
{
  return a = a * b;
}

ccl_device_inline vfloat8 operator*=(vfloat8 a, const float f)
{
  return a = a * f;
}

ccl_device_inline vfloat8 operator/=(vfloat8 a, const float f)
{
  return a = a / f;
}

ccl_device_inline bool operator==(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return (_mm256_movemask_ps(_mm256_castsi256_ps(
              _mm256_cmpeq_epi32(_mm256_castps_si256(a.m256), _mm256_castps_si256(b.m256)))) &
          0b11111111) == 0b11111111;
#else
  return (a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d && a.e == b.e && a.f == b.f &&
          a.g == b.g && a.h == b.h);
#endif
}

ccl_device_inline vfloat8 operator^(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_xor_ps(a.m256, b.m256));
#else
  return make_vfloat8(__uint_as_float(__float_as_uint(a.a) ^ __float_as_uint(b.a)),
                      __uint_as_float(__float_as_uint(a.b) ^ __float_as_uint(b.b)),
                      __uint_as_float(__float_as_uint(a.c) ^ __float_as_uint(b.c)),
                      __uint_as_float(__float_as_uint(a.d) ^ __float_as_uint(b.d)),
                      __uint_as_float(__float_as_uint(a.e) ^ __float_as_uint(b.e)),
                      __uint_as_float(__float_as_uint(a.f) ^ __float_as_uint(b.f)),
                      __uint_as_float(__float_as_uint(a.g) ^ __float_as_uint(b.g)),
                      __uint_as_float(__float_as_uint(a.h) ^ __float_as_uint(b.h)));
#endif
}

ccl_device_inline vfloat8 sqrt(const vfloat8 a)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_sqrt_ps(a.m256));
#else
  return make_vfloat8(sqrtf(a.a),
                      sqrtf(a.b),
                      sqrtf(a.c),
                      sqrtf(a.d),
                      sqrtf(a.e),
                      sqrtf(a.f),
                      sqrtf(a.g),
                      sqrtf(a.h));
#endif
}

ccl_device_inline vfloat8 sqr(const vfloat8 a)
{
  return a * a;
}

ccl_device_inline bool is_zero(const vfloat8 a)
{
  return a == make_vfloat8(0.0f);
}

ccl_device_inline float reduce_add(const vfloat8 a)
{
#ifdef __KERNEL_AVX__
  vfloat8 b(_mm256_hadd_ps(a.m256, a.m256));
  vfloat8 h(_mm256_hadd_ps(b.m256, b.m256));
  return h[0] + h[4];
#else
  return a.a + a.b + a.c + a.d + a.e + a.f + a.g + a.h;
#endif
}

ccl_device_inline float average(const vfloat8 a)
{
  return reduce_add(a) / 8.0f;
}

ccl_device_inline vfloat8 min(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_min_ps(a.m256, b.m256));
#else
  return make_vfloat8(min(a.a, b.a),
                      min(a.b, b.b),
                      min(a.c, b.c),
                      min(a.d, b.d),
                      min(a.e, b.e),
                      min(a.f, b.f),
                      min(a.g, b.g),
                      min(a.h, b.h));
#endif
}

ccl_device_inline vfloat8 max(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_max_ps(a.m256, b.m256));
#else
  return make_vfloat8(max(a.a, b.a),
                      max(a.b, b.b),
                      max(a.c, b.c),
                      max(a.d, b.d),
                      max(a.e, b.e),
                      max(a.f, b.f),
                      max(a.g, b.g),
                      max(a.h, b.h));
#endif
}

ccl_device_inline vfloat8 clamp(const vfloat8 a, const vfloat8 mn, const vfloat8 mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline vfloat8 select(const vint8 mask, const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_blendv_ps(b, a, _mm256_castsi256_ps(mask)));
#else
  return make_vfloat8((mask.a) ? a.a : b.a,
                      (mask.b) ? a.b : b.b,
                      (mask.c) ? a.c : b.c,
                      (mask.d) ? a.d : b.d,
                      (mask.e) ? a.e : b.e,
                      (mask.f) ? a.f : b.f,
                      (mask.g) ? a.g : b.g,
                      (mask.h) ? a.h : b.h);
#endif
}

ccl_device_inline vfloat8 fabs(const vfloat8 a)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_and_ps(a.m256, _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff))));
#else
  return make_vfloat8(fabsf(a.a),
                      fabsf(a.b),
                      fabsf(a.c),
                      fabsf(a.d),
                      fabsf(a.e),
                      fabsf(a.f),
                      fabsf(a.g),
                      fabsf(a.h));
#endif
}

ccl_device_inline vfloat8 mix(const vfloat8 a, const vfloat8 b, const float t)
{
  return a + t * (b - a);
}

ccl_device_inline vfloat8 mix(const vfloat8 a, const vfloat8 b, vfloat8 t)
{
  return a + t * (b - a);
}

ccl_device_inline vfloat8 saturate(const vfloat8 a)
{
  return clamp(a, make_vfloat8(0.0f), make_vfloat8(1.0f));
}

ccl_device_inline vfloat8 exp(vfloat8 v)
{
  return make_vfloat8(
      expf(v.a), expf(v.b), expf(v.c), expf(v.d), expf(v.e), expf(v.f), expf(v.g), expf(v.h));
}

ccl_device_inline vfloat8 log(vfloat8 v)
{
  return make_vfloat8(
      logf(v.a), logf(v.b), logf(v.c), logf(v.d), logf(v.e), logf(v.f), logf(v.g), logf(v.h));
}

ccl_device_inline float dot(const vfloat8 a, const vfloat8 b)
{
#ifdef __KERNEL_AVX__
  vfloat8 t(_mm256_dp_ps(a.m256, b.m256, 0xFF));
  return t[0] + t[4];
#else
  return (a.a * b.a) + (a.b * b.b) + (a.c * b.c) + (a.d * b.d) + (a.e * b.e) + (a.f * b.f) +
         (a.g * b.g) + (a.h * b.h);
#endif
}

ccl_device_inline vfloat8 pow(vfloat8 v, const float e)
{
  return make_vfloat8(powf(v.a, e),
                      powf(v.b, e),
                      powf(v.c, e),
                      powf(v.d, e),
                      powf(v.e, e),
                      powf(v.f, e),
                      powf(v.g, e),
                      powf(v.h, e));
}

ccl_device_inline float reduce_min(const vfloat8 a)
{
  return min(min(min(a.a, a.b), min(a.c, a.d)), min(min(a.e, a.f), min(a.g, a.h)));
}

ccl_device_inline float reduce_max(const vfloat8 a)
{
  return max(max(max(a.a, a.b), max(a.c, a.d)), max(max(a.e, a.f), max(a.g, a.h)));
}

ccl_device_inline bool isequal(const vfloat8 a, const vfloat8 b)
{
  return a == b;
}

ccl_device_inline vfloat8 safe_divide(const vfloat8 a, const float b)
{
  return (b != 0.0f) ? a / b : make_vfloat8(0.0f);
}

ccl_device_inline vfloat8 safe_divide(const vfloat8 a, const vfloat8 b)
{
  return make_vfloat8((b.a != 0.0f) ? a.a / b.a : 0.0f,
                      (b.b != 0.0f) ? a.b / b.b : 0.0f,
                      (b.c != 0.0f) ? a.c / b.c : 0.0f,
                      (b.d != 0.0f) ? a.d / b.d : 0.0f,
                      (b.e != 0.0f) ? a.e / b.e : 0.0f,
                      (b.f != 0.0f) ? a.f / b.f : 0.0f,
                      (b.g != 0.0f) ? a.g / b.g : 0.0f,
                      (b.h != 0.0f) ? a.h / b.h : 0.0f);
}

ccl_device_inline vfloat8 ensure_finite(vfloat8 v)
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

ccl_device_inline bool isfinite_safe(vfloat8 v)
{
  return isfinite_safe(v.a) && isfinite_safe(v.b) && isfinite_safe(v.c) && isfinite_safe(v.d) &&
         isfinite_safe(v.e) && isfinite_safe(v.f) && isfinite_safe(v.g) && isfinite_safe(v.h);
}

ccl_device_inline vint8 cast(const vfloat8 a)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_castps_si256(a));
#else
  return make_vint8(__float_as_int(a.a),
                    __float_as_int(a.b),
                    __float_as_int(a.c),
                    __float_as_int(a.d),
                    __float_as_int(a.e),
                    __float_as_int(a.f),
                    __float_as_int(a.g),
                    __float_as_int(a.h));
#endif
}

#ifdef __KERNEL_SSE__
ccl_device_forceinline float4 low(const vfloat8 a)
{
#  ifdef __KERNEL_AVX__
  return float4(_mm256_extractf128_ps(a.m256, 0));
#  else
  return make_float4(a.e, a.f, a.g, a.h);
#  endif
}
ccl_device_forceinline float4 high(const vfloat8 a)
{
#  ifdef __KERNEL_AVX__
  return float4(_mm256_extractf128_ps(a.m256, 1));
#  else
  return make_float4(a.a, a.b, a.c, a.d);
#  endif
}

template<int i0,
         const int i1,
         const int i2,
         const int i3,
         const int i4,
         const int i5,
         const int i6,
         const int i7>
ccl_device_forceinline vfloat8 shuffle(const vfloat8 a)
{
#  ifdef __KERNEL_AVX__
  return vfloat8(_mm256_permutevar_ps(a, _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0)));
#  else
  return make_vfloat8(a[i0], a[i1], a[i2], a[i3], a[i4 + 4], a[i5 + 4], a[i6 + 4], a[i7 + 4]);
#  endif
}

template<size_t i0, const size_t i1, const size_t i2, const size_t i3>
ccl_device_forceinline vfloat8 shuffle(const vfloat8 a, const vfloat8 b)
{
#  ifdef __KERNEL_AVX__
  return vfloat8(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0)));
#  else
  return make_vfloat8(shuffle<i0, i1, i2, i3>(high(a), high(b)),
                      shuffle<i0, i1, i2, i3>(low(a), low(b)));
#  endif
}

template<size_t i0, const size_t i1, const size_t i2, const size_t i3>
ccl_device_forceinline vfloat8 shuffle(const vfloat8 a)
{
  return shuffle<i0, i1, i2, i3>(a, a);
}
template<size_t i0> ccl_device_forceinline vfloat8 shuffle(const vfloat8 a, const vfloat8 b)
{
  return shuffle<i0, i0, i0, i0>(a, b);
}
template<size_t i0> ccl_device_forceinline vfloat8 shuffle(const vfloat8 a)
{
  return shuffle<i0>(a, a);
}

template<size_t i> ccl_device_forceinline float extract(const vfloat8 a)
{
#  ifdef __KERNEL_AVX__
  __m256 b = shuffle<i, i, i, i>(a).m256;
  return _mm256_cvtss_f32(b);
#  else
  return a[i];
#  endif
}
#endif

CCL_NAMESPACE_END

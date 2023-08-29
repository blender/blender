/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_MATH_INT8_H__
#define __UTIL_MATH_INT8_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
ccl_device_inline vint8 operator+(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_add_epi32(a.m256, b.m256));
#  else
  return make_vint8(
      a.a + b.a, a.b + b.b, a.c + b.c, a.d + b.d, a.e + b.e, a.f + b.f, a.g + b.g, a.h + b.h);
#  endif
}

ccl_device_inline vint8 operator+=(vint8 &a, const vint8 b)
{
  return a = a + b;
}

ccl_device_inline vint8 operator-(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_sub_epi32(a.m256, b.m256));
#  else
  return make_vint8(
      a.a - b.a, a.b - b.b, a.c - b.c, a.d - b.d, a.e - b.e, a.f - b.f, a.g - b.g, a.h - b.h);
#  endif
}

ccl_device_inline vint8 operator-=(vint8 &a, const vint8 b)
{
  return a = a - b;
}

ccl_device_inline vint8 operator>>(const vint8 a, int i)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_srai_epi32(a.m256, i));
#  else
  return make_vint8(
      a.a >> i, a.b >> i, a.c >> i, a.d >> i, a.e >> i, a.f >> i, a.g >> i, a.h >> i);
#  endif
}

ccl_device_inline vint8 operator<<(const vint8 a, int i)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_slli_epi32(a.m256, i));
#  else
  return make_vint8(
      a.a << i, a.b << i, a.c << i, a.d << i, a.e << i, a.f << i, a.g << i, a.h << i);
#  endif
}

ccl_device_inline vint8 operator<(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_cmpgt_epi32(b.m256, a.m256));
#  else
  return make_vint8(
      a.a < b.a, a.b < b.b, a.c < b.c, a.d < b.d, a.e < b.e, a.f < b.f, a.g < b.g, a.h < b.h);
#  endif
}

ccl_device_inline vint8 operator<(const vint8 a, const int b)
{
  return a < make_vint8(b);
}

ccl_device_inline vint8 operator==(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_cmpeq_epi32(a.m256, b.m256));
#  else
  return make_vint8(a.a == b.a,
                    a.b == b.b,
                    a.c == b.c,
                    a.d == b.d,
                    a.e == b.e,
                    a.f == b.f,
                    a.g == b.g,
                    a.h == b.h);
#  endif
}

ccl_device_inline vint8 operator==(const vint8 a, const int b)
{
  return a == make_vint8(b);
}

ccl_device_inline vint8 operator>=(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(
      _mm256_xor_si256(_mm256_set1_epi32(0xffffffff), _mm256_cmpgt_epi32(b.m256, a.m256)));
#  else
  return make_vint8(a.a >= b.a,
                    a.b >= b.b,
                    a.c >= b.c,
                    a.d >= b.d,
                    a.e >= b.e,
                    a.f >= b.f,
                    a.g >= b.g,
                    a.h >= b.h);
#  endif
}

ccl_device_inline vint8 operator>=(const vint8 a, const int b)
{
  return a >= make_vint8(b);
}

ccl_device_inline vint8 operator&(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_and_si256(a.m256, b.m256));
#  else
  return make_vint8(
      a.a & b.a, a.b & b.b, a.c & b.c, a.d & b.d, a.e & b.e, a.f & b.f, a.g & b.g, a.h & b.h);
#  endif
}

ccl_device_inline vint8 operator|(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_or_si256(a.m256, b.m256));
#  else
  return make_vint8(
      a.a | b.a, a.b | b.b, a.c | b.c, a.d | b.d, a.e | b.e, a.f | b.f, a.g | b.g, a.h | b.h);
#  endif
}

ccl_device_inline vint8 operator^(const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_xor_si256(a.m256, b.m256));
#  else
  return make_vint8(
      a.a ^ b.a, a.b ^ b.b, a.c ^ b.c, a.d ^ b.d, a.e ^ b.e, a.f ^ b.f, a.g ^ b.g, a.h ^ b.h);
#  endif
}

ccl_device_inline vint8 operator&(const int32_t a, const vint8 b)
{
  return make_vint8(a) & b;
}

ccl_device_inline vint8 operator&(const vint8 a, const int32_t b)
{
  return a & make_vint8(b);
}

ccl_device_inline vint8 operator|(const int32_t a, const vint8 b)
{
  return make_vint8(a) | b;
}

ccl_device_inline vint8 operator|(const vint8 a, const int32_t b)
{
  return a | make_vint8(b);
}

ccl_device_inline vint8 operator^(const int32_t a, const vint8 b)
{
  return make_vint8(a) ^ b;
}

ccl_device_inline vint8 operator^(const vint8 a, const int32_t b)
{
  return a ^ make_vint8(b);
}

ccl_device_inline vint8 &operator&=(vint8 &a, const vint8 b)
{
  return a = a & b;
}
ccl_device_inline vint8 &operator&=(vint8 &a, const int32_t b)
{
  return a = a & b;
}

ccl_device_inline vint8 &operator|=(vint8 &a, const vint8 b)
{
  return a = a | b;
}
ccl_device_inline vint8 &operator|=(vint8 &a, const int32_t b)
{
  return a = a | b;
}

ccl_device_inline vint8 &operator^=(vint8 &a, const vint8 b)
{
  return a = a ^ b;
}
ccl_device_inline vint8 &operator^=(vint8 &a, const int32_t b)
{
  return a = a ^ b;
}

ccl_device_inline vint8 &operator<<=(vint8 &a, const int32_t b)
{
  return a = a << b;
}
ccl_device_inline vint8 &operator>>=(vint8 &a, const int32_t b)
{
  return a = a >> b;
}

#  ifdef __KERNEL_AVX__
ccl_device_forceinline const vint8 srl(const vint8 a, const int32_t b)
{
  return vint8(_mm256_srli_epi32(a.m256, b));
}
#  endif

ccl_device_inline vint8 min(vint8 a, vint8 b)
{
#  if defined(__KERNEL_AVX__) && defined(__KERNEL_AVX41__)
  return vint8(_mm256_min_epi32(a.m256, b.m256));
#  else
  return make_vint8(min(a.a, b.a),
                    min(a.b, b.b),
                    min(a.c, b.c),
                    min(a.d, b.d),
                    min(a.e, b.e),
                    min(a.f, b.f),
                    min(a.g, b.g),
                    min(a.h, b.h));
#  endif
}

ccl_device_inline vint8 max(vint8 a, vint8 b)
{
#  if defined(__KERNEL_AVX__) && defined(__KERNEL_AVX41__)
  return vint8(_mm256_max_epi32(a.m256, b.m256));
#  else
  return make_vint8(max(a.a, b.a),
                    max(a.b, b.b),
                    max(a.c, b.c),
                    max(a.d, b.d),
                    max(a.e, b.e),
                    max(a.f, b.f),
                    max(a.g, b.g),
                    max(a.h, b.h));
#  endif
}

ccl_device_inline vint8 clamp(const vint8 a, const vint8 mn, const vint8 mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline vint8 select(const vint8 mask, const vint8 a, const vint8 b)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_castps_si256(_mm256_blendv_ps(
      _mm256_castsi256_ps(b), _mm256_castsi256_ps(a), _mm256_castsi256_ps(mask))));
#  else
  return make_vint8((mask.a) ? a.a : b.a,
                    (mask.b) ? a.b : b.b,
                    (mask.c) ? a.c : b.c,
                    (mask.d) ? a.d : b.d,
                    (mask.e) ? a.e : b.e,
                    (mask.f) ? a.f : b.f,
                    (mask.g) ? a.g : b.g,
                    (mask.h) ? a.h : b.h);
#  endif
}

ccl_device_inline vint8 load_vint8(const int *v)
{
#  ifdef __KERNEL_AVX__
  return vint8(_mm256_loadu_si256((__m256i *)v));
#  else
  return make_vint8(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
#  endif
}
#endif /* __KERNEL_GPU__ */

ccl_device_inline vfloat8 cast(const vint8 a)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_castsi256_ps(a));
#else
  return make_vfloat8(__int_as_float(a.a),
                      __int_as_float(a.b),
                      __int_as_float(a.c),
                      __int_as_float(a.d),
                      __int_as_float(a.e),
                      __int_as_float(a.f),
                      __int_as_float(a.g),
                      __int_as_float(a.h));
#endif
}

#ifdef __KERNEL_AVX__
template<size_t i> ccl_device_forceinline const vint8 shuffle(const vint8 a)
{
  return vint8(
      _mm256_castps_si256(_mm256_permute_ps(_mm256_castsi256_ps(a), _MM_SHUFFLE(i, i, i, i))));
}

template<size_t i0, size_t i1> ccl_device_forceinline const vint8 shuffle(const vint8 a)
{
  return vint8(_mm256_permute2f128_si256(a, a, (i1 << 4) | (i0 << 0)));
}

template<size_t i0, size_t i1>
ccl_device_forceinline const vint8 shuffle(const vint8 a, const vint8 b)
{
  return vint8(_mm256_permute2f128_si256(a, b, (i1 << 4) | (i0 << 0)));
}

template<size_t i0, size_t i1, size_t i2, size_t i3>
ccl_device_forceinline const vint8 shuffle(const vint8 a)
{
  return vint8(
      _mm256_castps_si256(_mm256_permute_ps(_mm256_castsi256_ps(a), _MM_SHUFFLE(i3, i2, i1, i0))));
}

template<size_t i0, size_t i1, size_t i2, size_t i3>
ccl_device_forceinline const vint8 shuffle(const vint8 a, const vint8 b)
{
  return vint8(_mm256_castps_si256(_mm256_shuffle_ps(
      _mm256_castsi256_ps(a), _mm256_castsi256_ps(b), _MM_SHUFFLE(i3, i2, i1, i0))));
}

template<> __forceinline const vint8 shuffle<0, 0, 2, 2>(const vint8 b)
{
  return vint8(_mm256_castps_si256(_mm256_moveldup_ps(_mm256_castsi256_ps(b))));
}
template<> __forceinline const vint8 shuffle<1, 1, 3, 3>(const vint8 b)
{
  return vint8(_mm256_castps_si256(_mm256_movehdup_ps(_mm256_castsi256_ps(b))));
}
template<> __forceinline const vint8 shuffle<0, 1, 0, 1>(const vint8 b)
{
  return vint8(_mm256_castps_si256(
      _mm256_castpd_ps(_mm256_movedup_pd(_mm256_castps_pd(_mm256_castsi256_ps(b))))));
}
#endif

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INT8_H__ */

/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2014-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_SIMD_TYPES_H__
#define __UTIL_SIMD_TYPES_H__

#include <limits>
#include <stdint.h>

#include "util/defines.h"

/* SSE Intrinsics includes
 *
 * We assume __KERNEL_SSEX__ flags to have been defined at this point.
 *
 * MinGW64 has conflicting declarations for these SSE headers in <windows.h>.
 * Since we can't avoid including <windows.h>, better only include that */
#if defined(FREE_WINDOWS64)
#  include "util/windows.h"
#elif defined(_MSC_VER)
#  include <intrin.h>
#elif (defined(__x86_64__) || defined(__i386__))
#  include <x86intrin.h>
#elif defined(__KERNEL_NEON__)
#  define SSE2NEON_PRECISE_MINMAX 1
#  include <sse2neon.h>
#endif

/* Floating Point Control, for Embree. */
#if defined(__x86_64__) || defined(_M_X64)
#  define SIMD_SET_FLUSH_TO_ZERO \
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); \
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#elif defined(__aarch64__) || defined(_M_ARM64)
/* The get/set denormals to zero was implemented in sse2neon v1.5.0.
 * Keep the compatibility code until the minimum library version is increased. */
#  if defined(_MM_SET_FLUSH_ZERO_MODE)
#    define SIMD_SET_FLUSH_TO_ZERO \
      _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); \
      _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#  else
#    define _MM_FLUSH_ZERO_ON 24
#    define __get_fpcr(__fpcr) __asm__ __volatile__("mrs %0,fpcr" : "=r"(__fpcr))
#    define __set_fpcr(__fpcr) __asm__ __volatile__("msr fpcr,%0" : : "ri"(__fpcr))
#    define SIMD_SET_FLUSH_TO_ZERO set_fz(_MM_FLUSH_ZERO_ON);
#    define SIMD_GET_FLUSH_TO_ZERO get_fz(_MM_FLUSH_ZERO_ON)
#  endif
#else
#  define SIMD_SET_FLUSH_TO_ZERO
#endif

CCL_NAMESPACE_BEGIN

/* Data structures used by SSE classes. */
#ifdef __KERNEL_SSE2__

extern const __m128 _mm_lookupmask_ps[16];

static struct TrueTy {
  __forceinline operator bool() const
  {
    return true;
  }
} True ccl_attr_maybe_unused;

static struct FalseTy {
  __forceinline operator bool() const
  {
    return false;
  }
} False ccl_attr_maybe_unused;

static struct ZeroTy {
  __forceinline operator float() const
  {
    return 0;
  }
  __forceinline operator int() const
  {
    return 0;
  }
} zero ccl_attr_maybe_unused;

static struct OneTy {
  __forceinline operator float() const
  {
    return 1;
  }
  __forceinline operator int() const
  {
    return 1;
  }
} one ccl_attr_maybe_unused;

static struct NegInfTy {
  __forceinline operator float() const
  {
    return -std::numeric_limits<float>::infinity();
  }
  __forceinline operator int() const
  {
    return std::numeric_limits<int>::min();
  }
} neg_inf ccl_attr_maybe_unused;

static struct PosInfTy {
  __forceinline operator float() const
  {
    return std::numeric_limits<float>::infinity();
  }
  __forceinline operator int() const
  {
    return std::numeric_limits<int>::max();
  }
} inf ccl_attr_maybe_unused, pos_inf ccl_attr_maybe_unused;

static struct StepTy {
} step ccl_attr_maybe_unused;

#endif
#if (defined(__aarch64__) || defined(_M_ARM64)) && !defined(_MM_SET_FLUSH_ZERO_MODE)
__forceinline int set_fz(uint32_t flag)
{
  uint64_t old_fpcr, new_fpcr;
  __get_fpcr(old_fpcr);
  new_fpcr = old_fpcr | (1ULL << flag);
  __set_fpcr(new_fpcr);
  __get_fpcr(old_fpcr);
  return old_fpcr == new_fpcr;
}
__forceinline int get_fz(uint32_t flag)
{
  uint64_t cur_fpcr;
  __get_fpcr(cur_fpcr);
  return (cur_fpcr & (1ULL << flag)) > 0 ? 1 : 0;
}
#endif

/* Utilities used by Neon */
#if defined(__KERNEL_NEON__)
template<class type, int i0, int i1, int i2, int i3> type shuffle_neon(const type &a)
{
  if (i0 == i1 && i0 == i2 && i0 == i3) {
    return type(vdupq_laneq_s32(int32x4_t(a), i0));
  }
  static const uint8_t tbl[16] = {(i0 * 4) + 0,
                                  (i0 * 4) + 1,
                                  (i0 * 4) + 2,
                                  (i0 * 4) + 3,
                                  (i1 * 4) + 0,
                                  (i1 * 4) + 1,
                                  (i1 * 4) + 2,
                                  (i1 * 4) + 3,
                                  (i2 * 4) + 0,
                                  (i2 * 4) + 1,
                                  (i2 * 4) + 2,
                                  (i2 * 4) + 3,
                                  (i3 * 4) + 0,
                                  (i3 * 4) + 1,
                                  (i3 * 4) + 2,
                                  (i3 * 4) + 3};

  return type(vqtbl1q_s8(int8x16_t(a), *(uint8x16_t *)tbl));
}

template<class type, int i0, int i1, int i2, int i3>
type shuffle_neon(const type &a, const type &b)
{
  if (&a == &b) {
    static const uint8_t tbl[16] = {(i0 * 4) + 0,
                                    (i0 * 4) + 1,
                                    (i0 * 4) + 2,
                                    (i0 * 4) + 3,
                                    (i1 * 4) + 0,
                                    (i1 * 4) + 1,
                                    (i1 * 4) + 2,
                                    (i1 * 4) + 3,
                                    (i2 * 4) + 0,
                                    (i2 * 4) + 1,
                                    (i2 * 4) + 2,
                                    (i2 * 4) + 3,
                                    (i3 * 4) + 0,
                                    (i3 * 4) + 1,
                                    (i3 * 4) + 2,
                                    (i3 * 4) + 3};

    return type(vqtbl1q_s8(int8x16_t(b), *(uint8x16_t *)tbl));
  }
  else {

    static const uint8_t tbl[16] = {(i0 * 4) + 0,
                                    (i0 * 4) + 1,
                                    (i0 * 4) + 2,
                                    (i0 * 4) + 3,
                                    (i1 * 4) + 0,
                                    (i1 * 4) + 1,
                                    (i1 * 4) + 2,
                                    (i1 * 4) + 3,
                                    (i2 * 4) + 0 + 16,
                                    (i2 * 4) + 1 + 16,
                                    (i2 * 4) + 2 + 16,
                                    (i2 * 4) + 3 + 16,
                                    (i3 * 4) + 0 + 16,
                                    (i3 * 4) + 1 + 16,
                                    (i3 * 4) + 2 + 16,
                                    (i3 * 4) + 3 + 16};

    return type(vqtbl2q_s8((int8x16x2_t){int8x16_t(a), int8x16_t(b)}, *(uint8x16_t *)tbl));
  }
}
#endif /* __KERNEL_NEON */

/* Intrinsics Functions
 *
 * For fast bit operations. */

#if defined(__BMI__) && defined(__GNUC__)
#  ifndef _tzcnt_u32
#    define _tzcnt_u32 __tzcnt_u32
#  endif
#  ifndef _tzcnt_u64
#    define _tzcnt_u64 __tzcnt_u64
#  endif
#endif

#if defined(__LZCNT__)
#  define _lzcnt_u32 __lzcnt32
#  define _lzcnt_u64 __lzcnt64
#endif

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__clang__)
/* Intrinsic functions on Windows. */
__forceinline uint32_t __bsf(uint32_t v)
{
#  if defined(__KERNEL_AVX2__)
  return _tzcnt_u32(v);
#  else
  unsigned long r = 0;
  _BitScanForward(&r, v);
  return r;
#  endif
}

__forceinline uint32_t __bsr(uint32_t v)
{
  unsigned long r = 0;
  _BitScanReverse(&r, v);
  return r;
}

__forceinline uint32_t __btc(uint32_t v, uint32_t i)
{
  long r = v;
  _bittestandcomplement(&r, i);
  return r;
}

__forceinline uint32_t bitscan(uint32_t v)
{
#  if defined(__KERNEL_AVX2__)
  return _tzcnt_u32(v);
#  else
  return __bsf(v);
#  endif
}

#  if defined(__KERNEL_64_BIT__)

__forceinline uint64_t __bsf(uint64_t v)
{
#    if defined(__KERNEL_AVX2__)
  return _tzcnt_u64(v);
#    else
  unsigned long r = 0;
  _BitScanForward64(&r, v);
  return r;
#    endif
}

__forceinline uint64_t __bsr(uint64_t v)
{
  unsigned long r = 0;
  _BitScanReverse64(&r, v);
  return r;
}

__forceinline uint64_t __btc(uint64_t v, uint64_t i)
{
  uint64_t r = v;
  _bittestandcomplement64((__int64 *)&r, i);
  return r;
}

__forceinline uint64_t bitscan(uint64_t v)
{
#    if defined(__KERNEL_AVX2__)
#      if defined(__KERNEL_64_BIT__)
  return _tzcnt_u64(v);
#      else
  return _tzcnt_u32(v);
#      endif
#    else
  return __bsf(v);
#    endif
}

#  endif /* __KERNEL_64_BIT__ */

#elif (defined(__x86_64__) || defined(__i386__)) && defined(__KERNEL_SSE2__)
/* Intrinsic functions with x86 SSE. */

__forceinline uint32_t __bsf(const uint32_t v)
{
  uint32_t r = 0;
  asm("bsf %1,%0" : "=r"(r) : "r"(v));
  return r;
}

__forceinline uint32_t __bsr(const uint32_t v)
{
  uint32_t r = 0;
  asm("bsr %1,%0" : "=r"(r) : "r"(v));
  return r;
}

__forceinline uint32_t __btc(const uint32_t v, uint32_t i)
{
  uint32_t r = 0;
  asm("btc %1,%0" : "=r"(r) : "r"(i), "0"(v) : "flags");
  return r;
}

#  if (defined(__KERNEL_64_BIT__) || defined(__APPLE__)) && \
      !(defined(__ILP32__) && defined(__x86_64__))
__forceinline uint64_t __bsf(const uint64_t v)
{
  uint64_t r = 0;
  asm("bsf %1,%0" : "=r"(r) : "r"(v));
  return r;
}
#  endif

__forceinline uint64_t __bsr(const uint64_t v)
{
  uint64_t r = 0;
  asm("bsr %1,%0" : "=r"(r) : "r"(v));
  return r;
}

__forceinline uint64_t __btc(const uint64_t v, const uint64_t i)
{
  uint64_t r = 0;
  asm("btc %1,%0" : "=r"(r) : "r"(i), "0"(v) : "flags");
  return r;
}

__forceinline uint32_t bitscan(uint32_t v)
{
#  if defined(__KERNEL_AVX2__)
  return _tzcnt_u32(v);
#  else
  return __bsf(v);
#  endif
}

#  if (defined(__KERNEL_64_BIT__) || defined(__APPLE__)) && \
      !(defined(__ILP32__) && defined(__x86_64__))
__forceinline uint64_t bitscan(uint64_t v)
{
#    if defined(__KERNEL_AVX2__)
#      if defined(__KERNEL_64_BIT__)
  return _tzcnt_u64(v);
#      else
  return _tzcnt_u32(v);
#      endif
#    else
  return __bsf(v);
#    endif
}
#  endif

#else
/* Intrinsic functions fallback for arbitrary processor. */
__forceinline uint32_t __bsf(const uint32_t x)
{
  for (int i = 0; i < 32; i++) {
    if (x & (1U << i))
      return i;
  }
  return 32;
}

__forceinline uint32_t __bsr(const uint32_t x)
{
  for (int i = 0; i < 32; i++) {
    if (x & (1U << (31 - i)))
      return (31 - i);
  }
  return 32;
}

__forceinline uint32_t __btc(const uint32_t x, const uint32_t bit)
{
  uint32_t mask = 1U << bit;
  return x & (~mask);
}

__forceinline uint32_t __bsf(const uint64_t x)
{
  for (int i = 0; i < 64; i++) {
    if (x & (1UL << i))
      return i;
  }
  return 64;
}

__forceinline uint32_t __bsr(const uint64_t x)
{
  for (int i = 0; i < 64; i++) {
    if (x & (1UL << (63 - i)))
      return (63 - i);
  }
  return 64;
}

__forceinline uint64_t __btc(const uint64_t x, const uint32_t bit)
{
  uint64_t mask = 1UL << bit;
  return x & (~mask);
}

__forceinline uint32_t bitscan(uint32_t value)
{
  assert(value != 0);
  uint32_t bit = 0;
  while ((value & (1 << bit)) == 0) {
    ++bit;
  }
  return bit;
}

__forceinline uint64_t bitscan(uint64_t value)
{
  assert(value != 0);
  uint64_t bit = 0;
  while ((value & (1 << bit)) == 0) {
    ++bit;
  }
  return bit;
}

#endif /* Intrinsics */

/* SSE compatibility.
 *
 * Various utilities to smooth over differences between SSE versions and
 * implementations. */
#ifdef __KERNEL_SSE2__

/* Test __KERNEL_SSE41__ for MSVC which does not define __SSE4_1__, and test
 * __SSE4_1__ to avoid OpenImageIO conflicts with our emulation macros on other
 * platforms when compiling code outside the kernel. */
#  if !(defined(__KERNEL_SSE41__) || defined(__SSE4_1__) || defined(__SSE4_2__))

/* Emulation of SSE4 functions with SSE2 */

#    define _MM_FROUND_TO_NEAREST_INT 0x00
#    define _MM_FROUND_TO_NEG_INF 0x01
#    define _MM_FROUND_TO_POS_INF 0x02
#    define _MM_FROUND_TO_ZERO 0x03
#    define _MM_FROUND_CUR_DIRECTION 0x04

#    undef _mm_blendv_ps
#    define _mm_blendv_ps _mm_blendv_ps_emu
__forceinline __m128 _mm_blendv_ps_emu(__m128 value, __m128 input, __m128 mask)
{
  __m128i isignmask = _mm_set1_epi32(0x80000000);
  __m128 signmask = _mm_castsi128_ps(isignmask);
  __m128i iandsign = _mm_castps_si128(_mm_and_ps(mask, signmask));
  __m128i icmpmask = _mm_cmpeq_epi32(iandsign, isignmask);
  __m128 cmpmask = _mm_castsi128_ps(icmpmask);
  return _mm_or_ps(_mm_and_ps(cmpmask, input), _mm_andnot_ps(cmpmask, value));
}

#    undef _mm_blend_ps
#    define _mm_blend_ps _mm_blend_ps_emu
__forceinline __m128 _mm_blend_ps_emu(__m128 value, __m128 input, const int mask)
{
  assert(mask < 0x10);
  return _mm_blendv_ps(value, input, _mm_lookupmask_ps[mask]);
}

#    undef _mm_blendv_epi8
#    define _mm_blendv_epi8 _mm_blendv_epi8_emu
__forceinline __m128i _mm_blendv_epi8_emu(__m128i value, __m128i input, __m128i mask)
{
  return _mm_or_si128(_mm_and_si128(mask, input), _mm_andnot_si128(mask, value));
}

#    undef _mm_min_epi32
#    define _mm_min_epi32 _mm_min_epi32_emu
__forceinline __m128i _mm_min_epi32_emu(__m128i value, __m128i input)
{
  return _mm_blendv_epi8(input, value, _mm_cmplt_epi32(value, input));
}

#    undef _mm_max_epi32
#    define _mm_max_epi32 _mm_max_epi32_emu
__forceinline __m128i _mm_max_epi32_emu(__m128i value, __m128i input)
{
  return _mm_blendv_epi8(value, input, _mm_cmplt_epi32(value, input));
}

#    ifndef __KERNEL_NEON__
#      undef _mm_extract_epi32
#      define _mm_extract_epi32 _mm_extract_epi32_emu
__forceinline int _mm_extract_epi32_emu(__m128i input, const int index)
{
  switch (index) {
    case 0:
      return _mm_cvtsi128_si32(input);
    case 1:
      return _mm_cvtsi128_si32(_mm_shuffle_epi32(input, _MM_SHUFFLE(1, 1, 1, 1)));
    case 2:
      return _mm_cvtsi128_si32(_mm_shuffle_epi32(input, _MM_SHUFFLE(2, 2, 2, 2)));
    case 3:
      return _mm_cvtsi128_si32(_mm_shuffle_epi32(input, _MM_SHUFFLE(3, 3, 3, 3)));
    default:
      assert(false);
      return 0;
  }
}
#    endif

#    undef _mm_insert_epi32
#    define _mm_insert_epi32 _mm_insert_epi32_emu
__forceinline __m128i _mm_insert_epi32_emu(__m128i value, int input, const int index)
{
  assert(index >= 0 && index < 4);
  ((int *)&value)[index] = input;
  return value;
}

#    undef _mm_insert_ps
#    define _mm_insert_ps _mm_insert_ps_emu
__forceinline __m128 _mm_insert_ps_emu(__m128 value, __m128 input, const int index)
{
  assert(index < 0x100);
  ((float *)&value)[(index >> 4) & 0x3] = ((float *)&input)[index >> 6];
  return _mm_andnot_ps(_mm_lookupmask_ps[index & 0xf], value);
}

#    undef _mm_round_ps
#    define _mm_round_ps _mm_round_ps_emu
__forceinline __m128 _mm_round_ps_emu(__m128 value, const int flags)
{
  switch (flags) {
    case _MM_FROUND_TO_NEAREST_INT:
      return _mm_cvtepi32_ps(_mm_cvtps_epi32(value));
    case _MM_FROUND_TO_NEG_INF:
      return _mm_cvtepi32_ps(_mm_cvtps_epi32(_mm_add_ps(value, _mm_set1_ps(-0.5f))));
    case _MM_FROUND_TO_POS_INF:
      return _mm_cvtepi32_ps(_mm_cvtps_epi32(_mm_add_ps(value, _mm_set1_ps(0.5f))));
    case _MM_FROUND_TO_ZERO:
      return _mm_cvtepi32_ps(_mm_cvttps_epi32(value));
  }
  return value;
}

#  endif /* !(defined(__KERNEL_SSE41__) || defined(__SSE4_1__) || defined(__SSE4_2__)) */

/* Older GCC versions do not have _mm256_cvtss_f32 yet, so define it ourselves.
 * _mm256_castps256_ps128 generates no instructions so this is just as efficient. */
#  if defined(__KERNEL_AVX__) || defined(__KERNEL_AVX2__)
#    undef _mm256_cvtss_f32
#    define _mm256_cvtss_f32(a) (_mm_cvtss_f32(_mm256_castps256_ps128(a)))
#  endif

#endif /* __KERNEL_SSE2__ */

/* quiet unused define warnings */
#if defined(__KERNEL_SSE2__) || defined(__KERNEL_SSE3__) || defined(__KERNEL_SSSE3__) || \
    defined(__KERNEL_SSE41__) || defined(__KERNEL_AVX__) || defined(__KERNEL_AVX2__)
/* do nothing */
#endif

CCL_NAMESPACE_END

#endif /* __UTIL_SIMD_TYPES_H__ */

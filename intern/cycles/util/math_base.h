/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Math
 *
 * Basic math functions on scalar and vector types. This header is used by
 * both the kernel code when compiled as C++, and other C++ non-kernel code. */

#include "util/defines.h"
#include "util/types_base.h"

#ifdef __HIP__
#  include <hip/hip_vector_types.h>
#endif

#if !defined(__KERNEL_METAL__)
#  include <cfloat>  // IWYU pragma: export
#  include <cmath>   // IWYU pragma: export
#endif

CCL_NAMESPACE_BEGIN

/* Float Pi variations */

/* Division */
#ifndef M_PI_F
#  define M_PI_F (3.1415926535897932f) /* `pi` */
#endif
#ifndef M_PI_2_F
#  define M_PI_2_F (1.5707963267948966f) /* `pi/2` */
#endif
#ifndef M_PI_4_F
#  define M_PI_4_F (0.7853981633974830f) /* `pi/4` */
#endif
#ifndef M_1_PI_F
#  define M_1_PI_F (0.3183098861837067f) /* `1/pi` */
#endif
#ifndef M_2_PI_F
#  define M_2_PI_F (0.6366197723675813f) /* `2/pi` */
#endif
#ifndef M_1_2PI_F
#  define M_1_2PI_F (0.1591549430918953f) /* `1/(2*pi)` */
#endif
#ifndef M_1_4PI_F
#  define M_1_4PI_F (0.0795774715459476f) /* `1/(4*pi)` */
#endif
#ifndef M_SQRT_PI_8_F
#  define M_SQRT_PI_8_F (0.6266570686577501f) /* `sqrt(pi/8)` */
#endif
#ifndef M_LN_2PI_F
#  define M_LN_2PI_F (1.8378770664093454f) /* `ln(2*pi)` */
#endif

/* Multiplication */
#ifndef M_2PI_F
#  define M_2PI_F (6.2831853071795864f) /* `2*pi` */
#endif
#ifndef M_4PI_F
#  define M_4PI_F (12.566370614359172f) /* `4*pi` */
#endif
#ifndef M_PI_4F
#  define M_PI_4F 0.78539816339744830962f /* `pi/4` */
#endif

/* Float sqrt variations */
#ifndef M_SQRT2_F
#  define M_SQRT2_F (1.4142135623730950f) /* `sqrt(2)` */
#endif
#ifndef M_CBRT2_F
#  define M_CBRT2_F 1.2599210498948732f /* `cbrt(2)` */
#endif
#ifndef M_SQRT1_2F
#  define M_SQRT1_2F 0.70710678118654752440f /* `sqrt(1/2)` */
#endif
#ifndef M_SQRT3_F
#  define M_SQRT3_F (1.7320508075688772f) /* `sqrt(3)` */
#endif
#ifndef M_LN2_F
#  define M_LN2_F (0.6931471805599453f) /* `ln(2)` */
#endif
#ifndef M_LN10_F
#  define M_LN10_F (2.3025850929940457f) /* `ln(10)` */
#endif

/* Scalar */

#if !defined(__HIP__) && !defined(__KERNEL_ONEAPI__)
#  ifdef _WIN32
ccl_device_inline float fmaxf(const float a, const float b)
{
  return (a > b) ? a : b;
}

ccl_device_inline float fminf(const float a, const float b)
{
  return (a < b) ? a : b;
}

#  endif /* _WIN32 */
#endif   /* __HIP__, __KERNEL_ONEAPI__ */

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
#  ifndef __KERNEL_ONEAPI__
using std::isfinite;
using std::isnan;
using std::sqrt;
#  else
#    define isfinite(x) sycl::isfinite((x))
#    define isnan(x) sycl::isnan((x))
#  endif

ccl_device_inline int abs(const int x)
{
  return (x > 0) ? x : -x;
}

ccl_device_inline int max(const int a, const int b)
{
  return (a > b) ? a : b;
}

ccl_device_inline int min(const int a, const int b)
{
  return (a < b) ? a : b;
}

ccl_device_inline uint32_t max(const uint32_t a, const uint32_t b)
{
  return (a > b) ? a : b;
}

ccl_device_inline uint32_t min(const uint32_t a, const uint32_t b)
{
  return (a < b) ? a : b;
}

ccl_device_inline uint64_t max(const uint64_t a, const uint64_t b)
{
  return (a > b) ? a : b;
}

ccl_device_inline uint64_t min(const uint64_t a, const uint64_t b)
{
  return (a < b) ? a : b;
}

/* NOTE: On 64bit Darwin the `size_t` is defined as `unsigned long int` and `uint64_t` is defined
 * as `unsigned long long`. Both of the definitions are 64 bit unsigned integer, but the automatic
 * substitution does not allow to automatically pick function defined for `uint64_t` as it is not
 * exactly the same type definition.
 * Work this around by adding a templated function enabled for `size_t` type which will be used
 * when there is no explicit specialization of `min()`/`max()` above. */

template<class T>
ccl_device_inline typename std::enable_if_t<std::is_same_v<T, size_t>, T> max(T a, T b)
{
  return (a > b) ? a : b;
}

template<class T>
ccl_device_inline typename std::enable_if_t<std::is_same_v<T, size_t>, T> min(T a, T b)
{
  return (a < b) ? a : b;
}

ccl_device_inline float max(const float a, const float b)
{
  return (a > b) ? a : b;
}

ccl_device_inline float min(const float a, const float b)
{
  return (a < b) ? a : b;
}

ccl_device_inline double max(const double a, const double b)
{
  return (a > b) ? a : b;
}

ccl_device_inline double min(const double a, const double b)
{
  return (a < b) ? a : b;
}

/* These 2 guys are templated for usage with registers data.
 *
 * NOTE: Since this is CPU-only functions it is ok to use references here.
 * But for other devices we'll need to be careful about this.
 */

template<typename T> ccl_device_inline T min4(const T &a, const T &b, const T &c, const T &d)
{
  return min(min(a, b), min(c, d));
}

template<typename T> ccl_device_inline T max4(const T &a, const T &b, const T &c, const T &d)
{
  return max(max(a, b), max(c, d));
}
#endif /* __KERNEL_GPU__ */

ccl_device_inline float min4(const float a, const float b, float c, const float d)
{
  return min(min(a, b), min(c, d));
}

ccl_device_inline float max4(const float a, const float b, float c, const float d)
{
  return max(max(a, b), max(c, d));
}

#if !defined(__KERNEL_METAL__) && !defined(__KERNEL_ONEAPI__)
/* Int/Float conversion */

ccl_device_inline int as_int(const uint i)
{
  union {
    uint ui;
    int i;
  } u;
  u.ui = i;
  return u.i;
}

ccl_device_inline uint as_uint(const int i)
{
  union {
    uint ui;
    int i;
  } u;
  u.i = i;
  return u.ui;
}

ccl_device_inline uint as_uint(const float f)
{
  union {
    uint i;
    float f;
  } u;
  u.f = f;
  return u.i;
}

#  ifndef __HIP__
ccl_device_inline int __float_as_int(const float f)
{
  union {
    int i;
    float f;
  } u;
  u.f = f;
  return u.i;
}

ccl_device_inline float __int_as_float(const int i)
{
  union {
    int i;
    float f;
  } u;
  u.i = i;
  return u.f;
}

ccl_device_inline uint __float_as_uint(const float f)
{
  union {
    uint i;
    float f;
  } u;
  u.f = f;
  return u.i;
}

ccl_device_inline float __uint_as_float(const uint i)
{
  union {
    uint i;
    float f;
  } u;
  u.i = i;
  return u.f;
}
#  endif

#endif /* !defined(__KERNEL_METAL__) */

#if defined(__KERNEL_METAL__)
ccl_device_forceinline bool isnan_safe(const float f)
{
  return isnan(f);
}

ccl_device_forceinline bool isfinite_safe(const float f)
{
  return isfinite(f);
}
#else
template<typename T> ccl_device_inline uint pointer_pack_to_uint_0(T *ptr)
{
  return ((uint64_t)ptr) & 0xFFFFFFFF;
}

template<typename T> ccl_device_inline uint pointer_pack_to_uint_1(T *ptr)
{
  return (((uint64_t)ptr) >> 32) & 0xFFFFFFFF;
}

template<typename T> ccl_device_inline T *pointer_unpack_from_uint(const uint a, const uint b)
{
  return (T *)(((uint64_t)b << 32) | a);
}

ccl_device_inline uint uint16_pack_to_uint(const uint a, const uint b)
{
  return (a << 16) | b;
}

ccl_device_inline uint uint16_unpack_from_uint_0(const uint i)
{
  return i >> 16;
}

ccl_device_inline uint uint16_unpack_from_uint_1(const uint i)
{
  return i & 0xFFFF;
}

/* Versions of functions which are safe for fast math. */
ccl_device_inline bool isnan_safe(const float f)
{
  const unsigned int x = __float_as_uint(f);
  return (x << 1) > 0xff000000u;
}

ccl_device_inline bool isfinite_safe(const float f)
{
  /* By IEEE 754 rule, 2*Inf equals Inf */
  const unsigned int x = __float_as_uint(f);
  return (f == f) && (x == 0 || x == (1u << 31) || (f != 2.0f * f)) && !((x << 1) > 0xff000000u);
}
#endif

ccl_device_inline float ensure_finite(const float v)
{
  return isfinite_safe(v) ? v : 0.0f;
}

#if !defined(__KERNEL_METAL__)
ccl_device_inline int clamp(const int a, const int mn, const int mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline float clamp(const float a, const float mn, const float mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline float mix(const float a, const float b, float t)
{
  return a + t * (b - a);
}

ccl_device_inline float smoothstep(const float edge0, const float edge1, const float x)
{
  float result;
  if (x < edge0) {
    result = 0.0f;
  }
  else if (x >= edge1) {
    result = 1.0f;
  }
  else {
    const float t = (x - edge0) / (edge1 - edge0);
    result = (3.0f - 2.0f * t) * (t * t);
  }
  return result;
}

#endif /* !defined(__KERNEL_METAL__) */

#if defined(__KERNEL_CUDA__)
ccl_device_inline float saturatef(const float a)
{
  return __saturatef(a);
}
#elif !defined(__KERNEL_METAL__)
ccl_device_inline float saturatef(const float a)
{
  return clamp(a, 0.0f, 1.0f);
}
#endif /* __KERNEL_CUDA__ */

ccl_device_inline int float_to_int(const float f)
{
  return (int)f;
}

ccl_device_inline int floor_to_int(const float f)
{
  return float_to_int(floorf(f));
}

ccl_device_inline float floorfrac(const float x, ccl_private int *i)
{
  const float f = floorf(x);
  *i = float_to_int(f);
  return x - f;
}

ccl_device_inline int ceil_to_int(const float f)
{
  return float_to_int(ceilf(f));
}

ccl_device_inline float fractf(const float x)
{
  return x - floorf(x);
}

/* Adapted from `godot-engine` math_funcs.h. */
ccl_device_inline float wrapf(const float value, const float max, const float min)
{
  const float range = max - min;
  return (range != 0.0f) ? value - (range * floorf((value - min) / range)) : min;
}

ccl_device_inline float pingpongf(const float a, const float b)
{
  return (b != 0.0f) ? fabsf(fractf((a - b) / (b * 2.0f)) * b * 2.0f - b) : 0.0f;
}

ccl_device_inline float smoothminf(const float a, const float b, float k)
{
  if (k != 0.0f) {
    const float h = fmaxf(k - fabsf(a - b), 0.0f) / k;
    return fminf(a, b) - h * h * h * k * (1.0f / 6.0f);
  }
  return fminf(a, b);
}

ccl_device_inline float signf(const float f)
{
  return (f < 0.0f) ? -1.0f : 1.0f;
}

ccl_device_inline float nonzerof(const float f, const float eps)
{
  if (fabsf(f) < eps) {
    return signf(f) * eps;
  }
  return f;
}

/* The behavior of `atan2(0, 0)` is undefined on many platforms, to ensure consistent behavior, we
 * return 0 in this case. See !126951.
 * Computes the angle between the positive x axis and the vector pointing from origin to (x, y). */
ccl_device_inline float compatible_atan2(const float y, const float x)
{
  return (x == 0.0f && y == 0.0f) ? 0.0f : atan2f(y, x);
}

/* `signum` function testing for zero. Matches GLSL and OSL functions. */
ccl_device_inline float compatible_signf(const float f)
{
  if (f == 0.0f) {
    return 0.0f;
  }
  return signf(f);
}

ccl_device_inline float smoothstepf(const float f)
{
  if (f <= 0.0f) {
    return 0.0f;
  }
  if (f >= 1.0f) {
    return 1.0f;
  }
  const float ff = f * f;
  return (3.0f * ff - 2.0f * ff * f);
}

ccl_device_inline int mod(const int x, const int m)
{
  return (x % m + m) % m;
}

ccl_device_inline float inverse_lerp(const float a, const float b, float x)
{
  return (x - a) / (b - a);
}

/* Cubic interpolation between b and c, a and d are the previous and next point. */
ccl_device_inline float cubic_interp(const float a, const float b, float c, const float d, float x)
{
  return 0.5f *
             (((d + 3.0f * (b - c) - a) * x + (2.0f * a - 5.0f * b + 4.0f * c - d)) * x +
              (c - a)) *
             x +
         b;
}

/* NaN-safe math ops */

ccl_device_inline float safe_sqrtf(const float f)
{
  return sqrtf(max(f, 0.0f));
}

ccl_device_inline float inversesqrtf(const float f)
{
#if defined(__KERNEL_METAL__)
  return (f > 0.0f) ? rsqrt(f) : 0.0f;
#else
  return (f > 0.0f) ? 1.0f / sqrtf(f) : 0.0f;
#endif
}

ccl_device float safe_asinf(const float a)
{
  return asinf(clamp(a, -1.0f, 1.0f));
}

ccl_device float safe_acosf(const float a)
{
  return acosf(clamp(a, -1.0f, 1.0f));
}

ccl_device float compatible_powf(const float x, const float y)
{
#ifdef __KERNEL_GPU__
  if (y == 0.0f) /* x^0 -> 1, including 0^0 */
    return 1.0f;

  /* GPU pow doesn't accept negative x, do manual checks here */
  if (x < 0.0f) {
    if (fmodf(-y, 2.0f) == 0.0f)
      return powf(-x, y);
    else
      return -powf(-x, y);
  }
  else if (x == 0.0f)
    return 0.0f;
#endif
  return powf(x, y);
}

ccl_device float safe_powf(const float a, const float b)
{
  if (UNLIKELY(a < 0.0f && b != float_to_int(b))) {
    return 0.0f;
  }

  return compatible_powf(a, b);
}

ccl_device float safe_divide(const float a, const float b)
{
  return (b != 0.0f) ? a / b : 0.0f;
}

ccl_device float safe_logf(const float a, const float b)
{
  if (UNLIKELY(a <= 0.0f || b <= 0.0f)) {
    return 0.0f;
  }

  return safe_divide(logf(a), logf(b));
}

ccl_device float safe_modulo(const float a, const float b)
{
  return (b != 0.0f) ? fmodf(a, b) : 0.0f;
}

ccl_device float safe_floored_modulo(const float a, const float b)
{
  return (b != 0.0f) ? a - floorf(a / b) * b : 0.0f;
}

ccl_device_inline float sqr(const float a)
{
  return a * a;
}

ccl_device_inline float sin_from_cos(const float c)
{
  return safe_sqrtf(1.0f - sqr(c));
}

ccl_device_inline float cos_from_sin(const float s)
{
  return safe_sqrtf(1.0f - sqr(s));
}

ccl_device_inline float sin_sqr_to_one_minus_cos(const float s_sq)
{
  /* Using second-order Taylor expansion at small angles for better accuracy. */
  return s_sq > 0.0004f ? 1.0f - safe_sqrtf(1.0f - s_sq) : 0.5f * s_sq;
}

ccl_device_inline float one_minus_cos(const float angle)
{
  /* Using second-order Taylor expansion at small angles for better accuracy. */
  return angle > 0.02f ? 1.0f - cosf(angle) : 0.5f * sqr(angle);
}

ccl_device_inline float pow20(const float a)
{
  return sqr(sqr(sqr(sqr(a)) * a));
}

ccl_device_inline float pow22(const float a)
{
  return sqr(a * sqr(sqr(sqr(a)) * a));
}

#ifdef __KERNEL_METAL__
ccl_device_inline float lgammaf(const float x)
{
  /* Nemes, Gergő (2010), "New asymptotic expansion for the Gamma function", Archiv der Mathematik
   */
  const float _1_180 = 1.0f / 180.0f;
  const float log2pi = 1.83787706641f;
  const float logx = log(x);
  return (log2pi - logx +
          x * (logx * 2.0f + log(x * sinh(1.0f / x) + (_1_180 / pow(x, 6.0f))) - 2.0f)) *
         0.5f;
}
#endif

ccl_device_inline float beta(const float x, const float y)
{
  return expf(lgammaf(x) + lgammaf(y) - lgammaf(x + y));
}

ccl_device_inline float xor_signmask(const float x, const int y)
{
  return __int_as_float(__float_as_int(x) ^ y);
}

ccl_device float bits_to_01(const uint bits)
{
  return bits * (1.0f / (float)0xFFFFFFFF);
}

#if !defined(__KERNEL_GPU__)
#  if defined(__GNUC__)
ccl_device_inline uint popcount(const uint x)
{
  return __builtin_popcount(x);
}
#  else
ccl_device_inline uint popcount(const uint x)
{
  /* TODO(Stefan): pop-count intrinsic for Windows with fallback for older CPUs. */
  uint i = x;
  i = i - ((i >> 1) & 0x55555555);
  i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
  i = (((i + (i >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
  return i;
}
#  endif
#elif defined(__KERNEL_ONEAPI__)
#  define popcount(x) sycl::popcount(x)
#elif defined(__KERNEL_HIP__)
/* Use popcll to support 64-bit wave for pre-RDNA AMD GPUs */
#  define popcount(x) __popcll(x)
#elif !defined(__KERNEL_METAL__)
#  define popcount(x) __popc(x)
#endif

ccl_device_inline uint count_leading_zeros(const uint x)
{
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_OPTIX__) || defined(__KERNEL_HIP__)
  return __clz(x);
#elif defined(__KERNEL_METAL__)
  return clz(x);
#elif defined(__KERNEL_ONEAPI__)
  return sycl::clz(x);
#else
  assert(x != 0);
#  ifdef _MSC_VER
  unsigned long leading_zero = 0;
  _BitScanReverse(&leading_zero, x);
  return (31 - leading_zero);
#  else
  return __builtin_clz(x);
#  endif
#endif
}

ccl_device_inline uint count_trailing_zeros(const uint x)
{
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_OPTIX__) || defined(__KERNEL_HIP__)
  return (__ffs(x) - 1);
#elif defined(__KERNEL_METAL__)
  return ctz(x);
#elif defined(__KERNEL_ONEAPI__)
  return sycl::ctz(x);
#else
  assert(x != 0);
#  ifdef _MSC_VER
  unsigned long ctz = 0;
  _BitScanForward(&ctz, x);
  return ctz;
#  else
  return __builtin_ctz(x);
#  endif
#endif
}

ccl_device_inline uint find_first_set(const uint x)
{
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_OPTIX__) || defined(__KERNEL_HIP__)
  return __ffs(x);
#elif defined(__KERNEL_METAL__)
  return (x != 0) ? ctz(x) + 1 : 0;
#else
#  ifdef _MSC_VER
  return (x != 0) ? (32 - count_leading_zeros(x & (~x + 1))) : 0;
#  else
  return __builtin_ffs(x);
#  endif
#endif
}

/* Compares two floats.
 * Returns true if their absolute difference is smaller than abs_diff (for numbers near zero)
 * or their relative difference is less than ulp_diff ULPs.
 * Based on
 * https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 */

ccl_device_inline bool compare_floats(const float a,
                                      const float b,
                                      float abs_diff,
                                      const int ulp_diff)
{
  if (fabsf(a - b) < abs_diff) {
    return true;
  }

  if ((a < 0.0f) != (b < 0.0f)) {
    return false;
  }

  return (abs(__float_as_int(a) - __float_as_int(b)) < ulp_diff);
}

/* Return value which is greater than the given one and is a power of two. */
ccl_device_inline uint next_power_of_two(const uint x)
{
  return x == 0 ? 1 : 1 << (32 - count_leading_zeros(x));
}

/* Return value which is lower than the given one and is a power of two. */
ccl_device_inline uint prev_power_of_two(const uint x)
{
  return x < 2 ? x : 1 << (31 - count_leading_zeros(x - 1));
}

#ifndef __has_builtin
#  define __has_builtin(v) 0
#endif

/* Reverses the bits of a 32 bit integer. */
ccl_device_inline uint32_t reverse_integer_bits(uint32_t x)
{
  /* Use a native instruction if it exists. */
#if defined(__KERNEL_CUDA__)
  return __brev(x);
#elif defined(__KERNEL_METAL__)
  return reverse_bits(x);
#elif defined(__aarch64__) || (defined(_M_ARM64) && !defined(_MSC_VER))
  /* Assume the rbit is always available on 64bit ARM architecture. */
  __asm__("rbit %w0, %w1" : "=r"(x) : "r"(x));
  return x;
#elif defined(__arm__) && ((__ARM_ARCH > 7) || __ARM_ARCH == 6 && __ARM_ARCH_ISA_THUMB >= 2)
  /* This ARM instruction is available in ARMv6T2 and above.
   * This 32-bit Thumb instruction is available in ARMv6T2 and above. */
  __asm__("rbit %0, %1" : "=r"(x) : "r"(x));
  return x;
#elif __has_builtin(__builtin_bitreverse32)
  return __builtin_bitreverse32(x);
#else
  /* Flip pairwise. */
  x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
  /* Flip pairs. */
  x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
  /* Flip nibbles. */
  x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
  /* Flip bytes. CPUs have an instruction for that, pretty fast one. */
#  ifdef _MSC_VER
  return _byteswap_ulong(x);
#  elif defined(__INTEL_COMPILER)
  return (uint32_t)_bswap((int)x);
#  else
  /* Assuming gcc or clang. */
  return __builtin_bswap32(x);
#  endif
#endif
}

/* Solve quadratic equation a*x^2 + b*x + c = 0, adapted from Mitsuba 3
 * The solution is ordered so that x1 <= x2.
 * Returns true if at least one solution is found.  */
ccl_device_inline bool solve_quadratic(
    const float a, const float b, const float c, ccl_private float &x1, ccl_private float &x2)
{
  /* If the equation is linear, the solution is -c/b, but b has to be non-zero. */
  const bool valid_linear = (a == 0.0f) && (b != 0.0f);
  x1 = x2 = -c / b;

  const float discriminant = sqr(b) - 4.0f * a * c;
  /* Allow slightly negative discriminant in case of numerical precision issues. */
  const bool valid_quadratic = (a != 0.0f) && (discriminant > -1e-5f);

  if (valid_quadratic) {
    /* Numerically stable version of (-b ± sqrt(discriminant)) / (2 * a), avoiding catastrophic
     * cancellation when `b` is very close to `sqrt(discriminant)`, by finding the solution of
     * greater magnitude which does not suffer from loss of precision, then using the identity
     * x1 * x2 = c / a. */
    const float temp = -0.5f * (b + copysignf(safe_sqrtf(discriminant), b));
    const float r1 = temp / a;
    const float r2 = c / temp;

    x1 = fminf(r1, r2);
    x2 = fmaxf(r1, r2);
  }

  return (valid_linear || valid_quadratic);
}

/* Defines a closed interval [min, max]. */
template<typename T> struct Interval {
  T min;
  T max;

  ccl_device_inline_method bool is_empty() const
  {
    return min >= max;
  }

  ccl_device_inline_method bool contains(T value) const
  {
    return value >= min && value <= max;
  }

  ccl_device_inline_method T length() const
  {
    return max - min;
  }
};

/* Computes the intersection of two intervals. */
template<typename T>
ccl_device_inline Interval<T> intervals_intersection(const ccl_private Interval<T> &first,
                                                     const ccl_private Interval<T> &second)
{
  return {max(first.min, second.min), min(first.max, second.max)};
}

CCL_NAMESPACE_END

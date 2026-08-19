/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * Data-type for 3x3 matrix.
 * Stored as (x, y, z) describing the matrix coordinate system, which makes it row-major.
 */

#include "util/math_float3.h"
#include "util/types_base.h"
#include "util/types_float3.h"
#include "util/types_float4.h"

CCL_NAMESPACE_BEGIN

/* NOTE: Elements might be padded to 16 bytes. */
struct float3x3 {
#if !defined(__KERNEL_GPU__)
  __forceinline const float3 &operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }
  __forceinline float3 &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }
#endif

  float3 x;
  float3 y;
  float3 z;
};

/* Construct new 3x3 matrix from the given coordinate system (rows of the matrix). */
ccl_device_inline float3x3 make_float3x3(const float3 x, const float3 y, const float3 z)
{
  return {x, y, z};
}

ccl_device_inline float3x3 zero_float3x3()
{
  return make_float3x3(zero_float3(), zero_float3(), zero_float3());
}

ccl_device_inline float3x3 identity_float3x3()
{
  return make_float3x3(
      make_float3(1.0f, 0.0f, 0.0f), make_float3(0.0f, 1.0f, 0.0f), make_float3(0.0f, 0.0f, 1.0f));
}

/* Packed float3x3.
 * Potentially smaller than float3 for storage, with no requirement for alignment.
 *
 * For math operations this must be converted to float3x3, so that on the CPU SIMD instructions
 * can be used. */
struct packed_float3x3 {
  packed_float3x3() = default;

  ccl_device_inline_method packed_float3x3(const float3x3 m) : x(m.x), y(m.y), z(m.z) {}

  ccl_device_inline_method operator float3x3() const
  {
    return make_float3x3(x, y, z);
  }

  ccl_device_inline_method packed_float3x3 operator=(const float3x3 a)
  {
    x = a.x;
    y = a.y;
    z = a.z;
    return *this;
  }

  packed_float3 x;
  packed_float3 y;
  packed_float3 z;
};
static_assert(sizeof(packed_float3x3) == 36, "packed_float3x3 expected to be exactly 36 bytes");

#if defined __METAL_PRINTF__
#  define print_float3x3(label, a) \
    metal::os_log_default.log_debug(label ":\n%.8f %.8f %.8f\n%.8f %.8f %.8f\n%.8f %.8f %.8f", \
                                    (a).x.x, \
                                    (a).x.y, \
                                    (a).x.z, \
                                    (a).y.x, \
                                    (a).y.y, \
                                    (a).y.z, \
                                    (a).z.x, \
                                    (a).z.y, \
                                    (a).z.z)
#else
ccl_device_inline void print_float3x3(const ccl_private char *label, const float3x3 a)
{
#  ifdef __KERNEL_PRINTF__
  printf("%s:\n%.8f %.8f %.8f\n%.8f %.8f %.8f\n%.8f %.8f %.8f\n",
         label,
         (double)a.x.x,
         (double)a.x.y,
         (double)a.x.z,
         (double)a.y.x,
         (double)a.y.y,
         (double)a.y.z,
         (double)a.z.x,
         (double)a.z.y,
         (double)a.z.z);
#  else
  (void)label;
  (void)a;
#  endif
}
#endif

CCL_NAMESPACE_END

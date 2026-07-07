/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uint3 {
  uint x, y, z;

#  ifndef __KERNEL_GPU__
  __forceinline uint operator[](uint i) const
  {
    util_assert(i < 3);
    return *(&x + i);
  }

  __forceinline uint &operator[](uint i)
  {
    util_assert(i < 3);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline uint3 make_uint3(const uint x, const uint y, uint z)
{
  uint3 a = {x, y, z};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

#if defined(__KERNEL_METAL__)
/* Metal has native packed_float3. */
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_ONEAPI__)
/* CUDA/oneAPI uint3 is already packed. */
using packed_uint3 = uint3;
#else
/* HIP uint3 is not packed (https://github.com/ROCm-Developer-Tools/HIP/issues/706). */
struct packed_uint3 {
  uint x, y, z;

  ccl_device_inline_method packed_uint3() = default;

  ccl_device_inline_method packed_uint3(const uint px, const uint py, const uint pz)
      : x(px), y(py), z(pz) {};

  ccl_device_inline_method packed_uint3(const uint3 &a) : x(a.x), y(a.y), z(a.z) {}

  ccl_device_inline_method operator uint3() const
  {
    return make_uint3(x, y, z);
  }

  ccl_device_inline_method packed_uint3 &operator=(const uint3 &a)
  {
    x = a.x;
    y = a.y;
    z = a.z;
    return *this;
  }

#  ifndef __KERNEL_GPU__
  __forceinline uint operator[](uint i) const
  {
    util_assert(i < 3);
    return *(&x + i);
  }

  __forceinline uint &operator[](uint i)
  {
    util_assert(i < 3);
    return *(&x + i);
  }
#  endif
};
#endif

static_assert(sizeof(packed_uint3) == 12, "packed_uint3 expected to be exactly 12 bytes");

ccl_device_inline packed_uint3 make_packed_uint3(const uint x, const uint y, uint z)
{
  packed_uint3 a = {x, y, z};
  return a;
}

CCL_NAMESPACE_END

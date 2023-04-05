/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uint3 {
  uint x, y, z;

#  ifndef __KERNEL_GPU__
  __forceinline uint operator[](uint i) const;
  __forceinline uint &operator[](uint i);
#  endif
};

ccl_device_inline uint3 make_uint3(uint x, uint y, uint z);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

#if defined(__KERNEL_METAL__)
/* Metal has native packed_float3. */
#elif defined(__KERNEL_CUDA__)
/* CUDA uint3 is already packed. */
typedef uint3 packed_uint3;
#else
/* HIP uint3 is not packed (https://github.com/ROCm-Developer-Tools/HIP/issues/706). */
struct packed_uint3 {
  uint x, y, z;

  ccl_device_inline_method packed_uint3(){};

  ccl_device_inline_method packed_uint3(const uint px, const uint py, const uint pz)
      : x(px), y(py), z(pz){};

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
  __forceinline uint operator[](uint i) const;
  __forceinline uint &operator[](uint i);
#  endif
};

static_assert(sizeof(packed_uint3) == 12, "packed_uint3 expected to be exactly 12 bytes");
#endif
CCL_NAMESPACE_END

/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_SSE__
__forceinline float3::float3() {}

__forceinline float3::float3(const float3 &a) : m128(a.m128) {}

__forceinline float3::float3(const __m128 &a) : m128(a) {}

__forceinline float3::operator const __m128 &() const
{
  return m128;
}

__forceinline float3::operator __m128 &()
{
  return m128;
}

__forceinline float3 &float3::operator=(const float3 &a)
{
  m128 = a.m128;
  return *this;
}
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
__forceinline float float3::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

__forceinline float &float3::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}
#  endif

ccl_device_inline float3 make_float3(float x, float y, float z)
{
#  if defined(__KERNEL_GPU__)
  return {x, y, z};
#  elif defined(__KERNEL_SSE__)
  return float3(_mm_set_ps(0.0f, z, y, x));
#  else
  return {x, y, z, 0.0f};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline float3 make_float3(float f)
{
#if defined(__KERNEL_GPU__)
  return make_float3(f, f, f);
#elif defined(__KERNEL_SSE__)
  return float3(_mm_set1_ps(f));
#else
  return {f, f, f, f};
#endif
}

ccl_device_inline void print_float3(ccl_private const char *label, const float3 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f %.8f\n", label, (double)a.x, (double)a.y, (double)a.z);
#endif
}

CCL_NAMESPACE_END

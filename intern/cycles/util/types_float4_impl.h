/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_SSE__
__forceinline float4::float4() {}

__forceinline float4::float4(const __m128 &a) : m128(a) {}

__forceinline float4::operator const __m128 &() const
{
  return m128;
}

__forceinline float4::operator __m128 &()
{
  return m128;
}

__forceinline float4 &float4::operator=(const float4 &a)
{
  m128 = a.m128;
  return *this;
}
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
__forceinline float float4::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 4);
  return *(&x + i);
}

__forceinline float &float4::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 4);
  return *(&x + i);
}
#  endif

ccl_device_inline float4 make_float4(float x, float y, float z, float w)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_set_ps(w, z, y, x));
#  else
  return {x, y, z, w};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline float4 make_float4(float f)
{
#ifdef __KERNEL_SSE__
  return float4(_mm_set1_ps(f));
#else
  return make_float4(f, f, f, f);
#endif
}

ccl_device_inline float4 make_float4(const int4 i)
{
#ifdef __KERNEL_SSE__
  return float4(_mm_cvtepi32_ps(i.m128));
#else
  return make_float4((float)i.x, (float)i.y, (float)i.z, (float)i.w);
#endif
}

ccl_device_inline void print_float4(ccl_private const char *label, const float4 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f %.8f %.8f\n", label, (double)a.x, (double)a.y, (double)a.z, (double)a.w);
#endif
}

CCL_NAMESPACE_END

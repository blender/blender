/*
 * Copyright 2011-2017 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_TYPES_FLOAT3_IMPL_H__
#define __UTIL_TYPES_FLOAT3_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
#  ifdef __KERNEL_SSE__
__forceinline float3::float3()
{
}

__forceinline float3::float3(const float3 &a) : m128(a.m128)
{
}

__forceinline float3::float3(const __m128 &a) : m128(a)
{
}

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

ccl_device_inline float3 make_float3(float f)
{
#  ifdef __KERNEL_SSE__
  float3 a(_mm_set1_ps(f));
#  else
  float3 a = {f, f, f, f};
#  endif
  return a;
}

ccl_device_inline float3 make_float3(float x, float y, float z)
{
#  ifdef __KERNEL_SSE__
  float3 a(_mm_set_ps(0.0f, z, y, x));
#  else
  float3 a = {x, y, z, 0.0f};
#  endif
  return a;
}

ccl_device_inline void print_float3(const char *label, const float3 &a)
{
  printf("%s: %.8f %.8f %.8f\n", label, (double)a.x, (double)a.y, (double)a.z);
}
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_FLOAT3_IMPL_H__ */

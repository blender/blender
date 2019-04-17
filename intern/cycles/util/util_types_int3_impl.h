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

#ifndef __UTIL_TYPES_INT3_IMPL_H__
#define __UTIL_TYPES_INT3_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
#  ifdef __KERNEL_SSE__
__forceinline int3::int3()
{
}

__forceinline int3::int3(const __m128i &a) : m128(a)
{
}

__forceinline int3::int3(const int3 &a) : m128(a.m128)
{
}

__forceinline int3::operator const __m128i &() const
{
  return m128;
}

__forceinline int3::operator __m128i &()
{
  return m128;
}

__forceinline int3 &int3::operator=(const int3 &a)
{
  m128 = a.m128;
  return *this;
}
#  endif /* __KERNEL_SSE__ */

__forceinline int int3::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

__forceinline int &int3::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

ccl_device_inline int3 make_int3(int i)
{
#  ifdef __KERNEL_SSE__
  int3 a(_mm_set1_epi32(i));
#  else
  int3 a = {i, i, i, i};
#  endif
  return a;
}

ccl_device_inline int3 make_int3(int x, int y, int z)
{
#  ifdef __KERNEL_SSE__
  int3 a(_mm_set_epi32(0, z, y, x));
#  else
  int3 a = {x, y, z, 0};
#  endif

  return a;
}

ccl_device_inline void print_int3(const char *label, const int3 &a)
{
  printf("%s: %d %d %d\n", label, a.x, a.y, a.z);
}
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_INT3_IMPL_H__ */

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

#ifndef __UTIL_TYPES_INT4_IMPL_H__
#define __UTIL_TYPES_INT4_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
#ifdef __KERNEL_SSE__
__forceinline int4::int4()
{
}

__forceinline int4::int4(const int4& a)
        : m128(a.m128)
{
}

__forceinline int4::int4(const __m128i& a)
        : m128(a)
{
}

__forceinline int4::operator const __m128i&(void) const
{
	return m128;
}

__forceinline int4::operator __m128i&(void)
{
	return m128;
}

__forceinline int4& int4::operator=(const int4& a)
{
	m128 = a.m128;
	return *this;
}
#endif  /* __KERNEL_SSE__ */

__forceinline int int4::operator[](int i) const
{
	util_assert(i >= 0);
	util_assert(i < 4);
	return *(&x + i);
}

__forceinline int& int4::operator[](int i)
{
	util_assert(i >= 0);
	util_assert(i < 4);
	return *(&x + i);
}

ccl_device_inline int4 make_int4(int i)
{
#ifdef __KERNEL_SSE__
	int4 a(_mm_set1_epi32(i));
#else
	int4 a = {i, i, i, i};
#endif
	return a;
}

ccl_device_inline int4 make_int4(int x, int y, int z, int w)
{
#ifdef __KERNEL_SSE__
	int4 a(_mm_set_epi32(w, z, y, x));
#else
	int4 a = {x, y, z, w};
#endif
	return a;
}

ccl_device_inline int4 make_int4(const float3& f)
{
#ifdef __KERNEL_SSE__
	int4 a(_mm_cvtps_epi32(f.m128));
#else
	int4 a = {(int)f.x, (int)f.y, (int)f.z, (int)f.w};
#endif
	return a;
}

ccl_device_inline int4 make_int4(const float4& f)
{
#ifdef __KERNEL_SSE__
	int4 a(_mm_cvtps_epi32(f.m128));
#else
	int4 a = {(int)f.x, (int)f.y, (int)f.z, (int)f.w};
#endif
	return a;
}

ccl_device_inline void print_int4(const char *label, const int4& a)
{
	printf("%s: %d %d %d %d\n", label, a.x, a.y, a.z, a.w);
}
#endif  /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif  /* __UTIL_TYPES_INT4_IMPL_H__ */

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

#ifndef __UTIL_MATH_INT3_H__
#define __UTIL_MATH_INT3_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline int3 min(int3 a, int3 b);
ccl_device_inline int3 max(int3 a, int3 b);
ccl_device_inline int3 clamp(const int3& a, int mn, int mx);
ccl_device_inline int3 clamp(const int3& a, int3& mn, int mx);
#endif  /* !__KERNEL_OPENCL__ */

/*******************************************************************************
 * Definition.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline int3 min(int3 a, int3 b)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE41__)
	return int3(_mm_min_epi32(a.m128, b.m128));
#else
	return make_int3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
#endif
}

ccl_device_inline int3 max(int3 a, int3 b)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE41__)
	return int3(_mm_max_epi32(a.m128, b.m128));
#else
	return make_int3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
#endif
}

ccl_device_inline int3 clamp(const int3& a, int mn, int mx)
{
#ifdef __KERNEL_SSE__
	return min(max(a, make_int3(mn)), make_int3(mx));
#else
	return make_int3(clamp(a.x, mn, mx), clamp(a.y, mn, mx), clamp(a.z, mn, mx));
#endif
}

ccl_device_inline int3 clamp(const int3& a, int3& mn, int mx)
{
#ifdef __KERNEL_SSE__
	return min(max(a, mn), make_int3(mx));
#else
	return make_int3(clamp(a.x, mn.x, mx),
	                 clamp(a.y, mn.y, mx),
	                 clamp(a.z, mn.z, mx));
#endif
}

ccl_device_inline bool operator==(const int3 &a, const int3 &b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

ccl_device_inline bool operator!=(const int3 &a, const int3 &b)
{
	return !(a == b);
}

ccl_device_inline bool operator<(const int3 &a, const int3 &b)
{
	return a.x < b.x && a.y < b.y && a.z < b.z;
}

ccl_device_inline int3 operator+(const int3 &a, const int3 &b)
{
#ifdef __KERNEL_SSE__
	return int3(_mm_add_epi32(a.m128, b.m128));
#else
	return make_int3(a.x + b.x, a.y + b.y, a.z + b.z);
#endif
}

ccl_device_inline int3 operator-(const int3 &a, const int3 &b)
{
#ifdef __KERNEL_SSE__
	return int3(_mm_sub_epi32(a.m128, b.m128));
#else
	return make_int3(a.x - b.x, a.y - b.y, a.z - b.z);
#endif
}
#endif  /* !__KERNEL_OPENCL__ */

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INT3_H__ */

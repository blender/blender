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

#ifndef __UTIL_TYPES_INT3_H__
#define __UTIL_TYPES_INT3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
struct ccl_try_align(16) int3 {
#ifdef __KERNEL_SSE__
	union {
		__m128i m128;
		struct { int x, y, z, w; };
	};

	__forceinline int3();
	__forceinline int3(const int3& a);
	__forceinline explicit int3(const __m128i& a);

	__forceinline operator const __m128i&() const;
	__forceinline operator __m128i&();

	__forceinline int3& operator =(const int3& a);
#else  /* __KERNEL_SSE__ */
	int x, y, z, w;
#endif  /* __KERNEL_SSE__ */

	__forceinline int operator[](int i) const;
	__forceinline int& operator[](int i);
};

ccl_device_inline int3 make_int3(int i);
ccl_device_inline int3 make_int3(int x, int y, int z);
ccl_device_inline void print_int3(const char *label, const int3& a);
#endif  /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif  /* __UTIL_TYPES_INT3_H__ */

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

#ifndef __UTIL_TYPES_FLOAT2_IMPL_H__
#define __UTIL_TYPES_FLOAT2_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
__forceinline float float2::operator[](int i) const
{
	util_assert(i >= 0);
	util_assert(i < 2);
	return *(&x + i);
}

__forceinline float& float2::operator[](int i)
{
	util_assert(i >= 0);
	util_assert(i < 2);
	return *(&x + i);
}

ccl_device_inline float2 make_float2(float x, float y)
{
	float2 a = {x, y};
	return a;
}

ccl_device_inline void print_float2(const char *label, const float2& a)
{
	printf("%s: %.8f %.8f\n", label, (double)a.x, (double)a.y);
}
#endif  /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif  /* __UTIL_TYPES_FLOAT2_IMPL_H__ */

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

#ifndef __UTIL_MATH_INT2_H__
#define __UTIL_MATH_INT2_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline bool operator==(const int2 a, const int2 b);
ccl_device_inline int2 operator+(const int2 &a, const int2 &b);
ccl_device_inline int2 operator+=(int2 &a, const int2 &b);
ccl_device_inline int2 operator-(const int2 &a, const int2 &b);
ccl_device_inline int2 operator*(const int2 &a, const int2 &b);
ccl_device_inline int2 operator/(const int2 &a, const int2 &b);
#endif /* !__KERNEL_OPENCL__ */

/*******************************************************************************
 * Definition.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline bool operator==(const int2 a, const int2 b)
{
  return (a.x == b.x && a.y == b.y);
}

ccl_device_inline int2 operator+(const int2 &a, const int2 &b)
{
  return make_int2(a.x + b.x, a.y + b.y);
}

ccl_device_inline int2 operator+=(int2 &a, const int2 &b)
{
  return a = a + b;
}

ccl_device_inline int2 operator-(const int2 &a, const int2 &b)
{
  return make_int2(a.x - b.x, a.y - b.y);
}

ccl_device_inline int2 operator*(const int2 &a, const int2 &b)
{
  return make_int2(a.x * b.x, a.y * b.y);
}

ccl_device_inline int2 operator/(const int2 &a, const int2 &b)
{
  return make_int2(a.x / b.x, a.y / b.y);
}
#endif /* !__KERNEL_OPENCL__ */

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INT2_H__ */

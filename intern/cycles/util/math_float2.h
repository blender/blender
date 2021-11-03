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

#ifndef __UTIL_MATH_FLOAT2_H__
#define __UTIL_MATH_FLOAT2_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

ccl_device_inline float2 operator-(const float2 &a);
ccl_device_inline float2 operator*(const float2 &a, const float2 &b);
ccl_device_inline float2 operator*(const float2 &a, float f);
ccl_device_inline float2 operator*(float f, const float2 &a);
ccl_device_inline float2 operator/(float f, const float2 &a);
ccl_device_inline float2 operator/(const float2 &a, float f);
ccl_device_inline float2 operator/(const float2 &a, const float2 &b);
ccl_device_inline float2 operator+(const float2 &a, const float f);
ccl_device_inline float2 operator+(const float2 &a, const float2 &b);
ccl_device_inline float2 operator-(const float2 &a, const float f);
ccl_device_inline float2 operator-(const float2 &a, const float2 &b);
ccl_device_inline float2 operator+=(float2 &a, const float2 &b);
ccl_device_inline float2 operator*=(float2 &a, const float2 &b);
ccl_device_inline float2 operator*=(float2 &a, float f);
ccl_device_inline float2 operator/=(float2 &a, const float2 &b);
ccl_device_inline float2 operator/=(float2 &a, float f);

ccl_device_inline bool operator==(const float2 &a, const float2 &b);
ccl_device_inline bool operator!=(const float2 &a, const float2 &b);

ccl_device_inline bool is_zero(const float2 &a);
ccl_device_inline float average(const float2 &a);
ccl_device_inline float distance(const float2 &a, const float2 &b);
ccl_device_inline float dot(const float2 &a, const float2 &b);
ccl_device_inline float cross(const float2 &a, const float2 &b);
ccl_device_inline float len(const float2 &a);
ccl_device_inline float2 normalize(const float2 &a);
ccl_device_inline float2 normalize_len(const float2 &a, float *t);
ccl_device_inline float2 safe_normalize(const float2 &a);
ccl_device_inline float2 min(const float2 &a, const float2 &b);
ccl_device_inline float2 max(const float2 &a, const float2 &b);
ccl_device_inline float2 clamp(const float2 &a, const float2 &mn, const float2 &mx);
ccl_device_inline float2 fabs(const float2 &a);
ccl_device_inline float2 as_float2(const float4 &a);
ccl_device_inline float2 interp(const float2 &a, const float2 &b, float t);
ccl_device_inline float2 floor(const float2 &a);

ccl_device_inline float2 safe_divide_float2_float(const float2 a, const float b);

/*******************************************************************************
 * Definition.
 */

ccl_device_inline float2 zero_float2()
{
  return make_float2(0.0f, 0.0f);
}

ccl_device_inline float2 one_float2()
{
  return make_float2(1.0f, 1.0f);
}

ccl_device_inline float2 operator-(const float2 &a)
{
  return make_float2(-a.x, -a.y);
}

ccl_device_inline float2 operator*(const float2 &a, const float2 &b)
{
  return make_float2(a.x * b.x, a.y * b.y);
}

ccl_device_inline float2 operator*(const float2 &a, float f)
{
  return make_float2(a.x * f, a.y * f);
}

ccl_device_inline float2 operator*(float f, const float2 &a)
{
  return make_float2(a.x * f, a.y * f);
}

ccl_device_inline float2 operator/(float f, const float2 &a)
{
  return make_float2(f / a.x, f / a.y);
}

ccl_device_inline float2 operator/(const float2 &a, float f)
{
  float invf = 1.0f / f;
  return make_float2(a.x * invf, a.y * invf);
}

ccl_device_inline float2 operator/(const float2 &a, const float2 &b)
{
  return make_float2(a.x / b.x, a.y / b.y);
}

ccl_device_inline float2 operator+(const float2 &a, const float f)
{
  return a + make_float2(f, f);
}

ccl_device_inline float2 operator+(const float2 &a, const float2 &b)
{
  return make_float2(a.x + b.x, a.y + b.y);
}

ccl_device_inline float2 operator-(const float2 &a, const float f)
{
  return a - make_float2(f, f);
}

ccl_device_inline float2 operator-(const float2 &a, const float2 &b)
{
  return make_float2(a.x - b.x, a.y - b.y);
}

ccl_device_inline float2 operator+=(float2 &a, const float2 &b)
{
  return a = a + b;
}

ccl_device_inline float2 operator*=(float2 &a, const float2 &b)
{
  return a = a * b;
}

ccl_device_inline float2 operator*=(float2 &a, float f)
{
  return a = a * f;
}

ccl_device_inline float2 operator/=(float2 &a, const float2 &b)
{
  return a = a / b;
}

ccl_device_inline float2 operator/=(float2 &a, float f)
{
  float invf = 1.0f / f;
  return a = a * invf;
}

ccl_device_inline bool operator==(const float2 &a, const float2 &b)
{
  return (a.x == b.x && a.y == b.y);
}

ccl_device_inline bool operator!=(const float2 &a, const float2 &b)
{
  return !(a == b);
}

ccl_device_inline bool is_zero(const float2 &a)
{
  return (a.x == 0.0f && a.y == 0.0f);
}

ccl_device_inline float average(const float2 &a)
{
  return (a.x + a.y) * (1.0f / 2.0f);
}

ccl_device_inline float distance(const float2 &a, const float2 &b)
{
  return len(a - b);
}

ccl_device_inline float dot(const float2 &a, const float2 &b)
{
  return a.x * b.x + a.y * b.y;
}

ccl_device_inline float cross(const float2 &a, const float2 &b)
{
  return (a.x * b.y - a.y * b.x);
}

ccl_device_inline float len(const float2 &a)
{
  return sqrtf(dot(a, a));
}

ccl_device_inline float2 normalize(const float2 &a)
{
  return a / len(a);
}

ccl_device_inline float2 normalize_len(const float2 &a, ccl_private float *t)
{
  *t = len(a);
  return a / (*t);
}

ccl_device_inline float2 safe_normalize(const float2 &a)
{
  float t = len(a);
  return (t != 0.0f) ? a / t : a;
}

ccl_device_inline float2 min(const float2 &a, const float2 &b)
{
  return make_float2(min(a.x, b.x), min(a.y, b.y));
}

ccl_device_inline float2 max(const float2 &a, const float2 &b)
{
  return make_float2(max(a.x, b.x), max(a.y, b.y));
}

ccl_device_inline float2 clamp(const float2 &a, const float2 &mn, const float2 &mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline float2 fabs(const float2 &a)
{
  return make_float2(fabsf(a.x), fabsf(a.y));
}

ccl_device_inline float2 as_float2(const float4 &a)
{
  return make_float2(a.x, a.y);
}

ccl_device_inline float2 interp(const float2 &a, const float2 &b, float t)
{
  return a + t * (b - a);
}

ccl_device_inline float2 mix(const float2 &a, const float2 &b, float t)
{
  return a + t * (b - a);
}

ccl_device_inline float2 floor(const float2 &a)
{
  return make_float2(floorf(a.x), floorf(a.y));
}

ccl_device_inline float2 safe_divide_float2_float(const float2 a, const float b)
{
  return (b != 0.0f) ? a / b : zero_float2();
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_FLOAT2_H__ */

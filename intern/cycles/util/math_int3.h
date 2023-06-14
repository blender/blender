/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_MATH_INT3_H__
#define __UTIL_MATH_INT3_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_METAL__)
ccl_device_inline int3 min(int3 a, int3 b)
{
#  if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE41__)
  return int3(_mm_min_epi32(a.m128, b.m128));
#  else
  return make_int3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
#  endif
}

ccl_device_inline int3 max(int3 a, int3 b)
{
#  if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE41__)
  return int3(_mm_max_epi32(a.m128, b.m128));
#  else
  return make_int3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
#  endif
}

ccl_device_inline int3 clamp(const int3 a, int mn, int mx)
{
#  ifdef __KERNEL_SSE__
  return min(max(a, make_int3(mn)), make_int3(mx));
#  else
  return make_int3(clamp(a.x, mn, mx), clamp(a.y, mn, mx), clamp(a.z, mn, mx));
#  endif
}

ccl_device_inline int3 clamp(const int3 a, int3 &mn, int mx)
{
#  ifdef __KERNEL_SSE__
  return min(max(a, mn), make_int3(mx));
#  else
  return make_int3(clamp(a.x, mn.x, mx), clamp(a.y, mn.y, mx), clamp(a.z, mn.z, mx));
#  endif
}

ccl_device_inline bool operator==(const int3 a, const int3 b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

ccl_device_inline bool operator!=(const int3 a, const int3 b)
{
  return !(a == b);
}

ccl_device_inline bool operator<(const int3 a, const int3 b)
{
  return a.x < b.x && a.y < b.y && a.z < b.z;
}

ccl_device_inline int3 operator+(const int3 a, const int3 b)
{
#  ifdef __KERNEL_SSE__
  return int3(_mm_add_epi32(a.m128, b.m128));
#  else
  return make_int3(a.x + b.x, a.y + b.y, a.z + b.z);
#  endif
}

ccl_device_inline int3 operator-(const int3 a, const int3 b)
{
#  ifdef __KERNEL_SSE__
  return int3(_mm_sub_epi32(a.m128, b.m128));
#  else
  return make_int3(a.x - b.x, a.y - b.y, a.z - b.z);
#  endif
}
#endif /* !__KERNEL_METAL__ */

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INT3_H__ */

/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"
#include "util/types_dual.h"

CCL_NAMESPACE_BEGIN

ccl_device_template_spec dual1 make_zero()
{
  return dual1();
}

ccl_device_template_spec dual2 make_zero()
{
  return dual2();
}

ccl_device_template_spec dual3 make_zero()
{
  return dual3();
}

ccl_device_template_spec dual4 make_zero()
{
  return dual4();
}

/* Multiplication of dual by scalar. */
template<class T1, class T2> ccl_device_inline dual<T1> operator*(const dual<T1> a, T2 b)
{
  return {a.val * b, a.dx * b, a.dy * b};
}

/* Negation. */
template<class T> ccl_device_inline dual<T> operator-(const ccl_private dual<T> &a)
{
  return {-a.val, -a.dx, -a.dy};
}

template<class T> ccl_device_inline dual1 average(const dual<T> a)
{
  return {average(a.val), average(a.dx), average(a.dy)};
}

template<class T> ccl_device_inline dual1 reduce_add(const dual<T> a)
{
  return {reduce_add(a.val), reduce_add(a.dx), reduce_add(a.dy)};
}

template<class T1, class T2> ccl_device_inline dual1 dot(const dual<T1> a, const T2 b)
{
  return reduce_add(a * b);
}

CCL_NAMESPACE_END

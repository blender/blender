/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* ---------------------------------------------------------------------- */
/** \name Comparison
 * \{ */

/**
 * Return true if all components is equal to zero.
 */
template<typename VecT> bool is_zero(VecT vec)
{
  return all(equal(vec, VecT(0.0f)));
}
template bool is_zero<float2>(float2);
template bool is_zero<float3>(float3);
template bool is_zero<float4>(float4);

/**
 * Return true if any component is equal to zero.
 */
template<typename VecT> bool is_any_zero(VecT vec)
{
  return any(equal(vec, VecT(0.0f)));
}
template bool is_any_zero<float2>(float2);
template bool is_any_zero<float3>(float3);
template bool is_any_zero<float4>(float4);

/**
 * Return true if the difference between`a` and `b` is below the `epsilon` value.
 */
template<typename VecT> bool is_equal(VecT a, VecT b, float epsilon)
{
  return all(lessThanEqual(abs(a - b), VecT(epsilon)));
}
template bool is_equal<float2>(float2, float2, float);
template bool is_equal<float3>(float3, float3, float);
template bool is_equal<float4>(float4, float4, float);

/**
 * Return true if the deference between`a` and `b` is below the `epsilon` value.
 * Epsilon value is scaled by magnitude of `a` before comparison.
 */
template<typename VecT, int dim>
bool almost_equal_relative(VecT a, VecT b, const float epsilon_factor)
{
  for (int i = 0; i < dim; i++) {
    if (abs(a[i] - b[i]) > epsilon_factor * abs(a[i])) {
      return false;
    }
  }
  return true;
}
template bool almost_equal_relative<float2, 2>(float2 a, float2 b, const float epsilon_factor);
template bool almost_equal_relative<float3, 3>(float3 a, float3 b, const float epsilon_factor);
template bool almost_equal_relative<float4, 4>(float4 a, float4 b, const float epsilon_factor);

/* Checks are flipped so NAN doesn't assert because we're making sure the value was
 * normalized and in the case we don't want NAN to be raising asserts since there
 * is nothing to be done in that case. */
template<typename VecT> bool is_unit_scale(VecT v)
{
  constexpr float assert_unit_epsilon = 0.0002f;
  float test_unit = dot(v, v);
  return (!(abs(test_unit - 1.0f) >= assert_unit_epsilon) ||
          !(abs(test_unit) >= assert_unit_epsilon));
}
template bool is_unit_scale<float2>(float2);
template bool is_unit_scale<float3>(float3);
template bool is_unit_scale<float4>(float4);

/** \} */

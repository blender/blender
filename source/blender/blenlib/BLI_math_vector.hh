/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <cmath>
#include <type_traits>

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender::math {

/**
 * Returns true if all components are exactly equal to 0.
 */
template<typename T, int Size> [[nodiscard]] inline bool is_zero(const VecBase<T, Size> &a)
{
  for (int i = 0; i < Size; i++) {
    if (a[i] != T(0)) {
      return false;
    }
  }
  return true;
}

/**
 * Returns true if at least one component is exactly equal to 0.
 */
template<typename T, int Size> [[nodiscard]] inline bool is_any_zero(const VecBase<T, Size> &a)
{
  for (int i = 0; i < Size; i++) {
    if (a[i] == T(0)) {
      return true;
    }
  }
  return false;
}

/**
 * Returns true if the given vectors are equal within the given epsilon.
 * The epsilon is scaled for each component by magnitude of the matching component of `a`.
 */
template<typename T, int Size>
[[nodiscard]] inline bool almost_equal_relative(const VecBase<T, Size> &a,
                                                const VecBase<T, Size> &b,
                                                const T &epsilon_factor)
{
  for (int i = 0; i < Size; i++) {
    const float epsilon = epsilon_factor * math::abs(a[i]);
    if (math::distance(a[i], b[i]) > epsilon) {
      return false;
    }
  }
  return true;
}

template<typename T, int Size> [[nodiscard]] inline VecBase<T, Size> abs(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = a[i] >= 0 ? a[i] : -a[i];
  }
  return result;
}

/**
 * Returns -1 if \a a is less than 0, 0 if \a a is equal to 0, and +1 if \a a is greater than 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> sign(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::sign(a[i]);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> min(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = a[i] < b[i] ? a[i] : b[i];
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> max(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = a[i] > b[i] ? a[i] : b[i];
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> clamp(const VecBase<T, Size> &a,
                                            const VecBase<T, Size> &min,
                                            const VecBase<T, Size> &max)
{
  VecBase<T, Size> result = a;
  for (int i = 0; i < Size; i++) {
    result[i] = math::clamp(result[i], min[i], max[i]);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> clamp(const VecBase<T, Size> &a, const T &min, const T &max)
{
  VecBase<T, Size> result = a;
  for (int i = 0; i < Size; i++) {
    result[i] = math::clamp(result[i], min, max);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> mod(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    BLI_assert(b[i] != 0);
    result[i] = math::mod(a[i], b[i]);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> mod(const VecBase<T, Size> &a, const T &b)
{
  BLI_assert(b != 0);
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::mod(a[i], b);
  }
  return result;
}

/**
 * Safe version of mod(a, b) that returns 0 if b is equal to 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> safe_mod(const VecBase<T, Size> &a,
                                               const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = (b[i] != 0) ? math::mod(a[i], b[i]) : 0;
  }
  return result;
}

/**
 * Safe version of mod(a, b) that returns 0 if b is equal to 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> safe_mod(const VecBase<T, Size> &a, const T &b)
{
  if (b == 0) {
    return VecBase<T, Size>(0);
  }
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::mod(a[i], b);
  }
  return result;
}

/**
 * Return the value of x raised to the y power.
 * The result is undefined if x < 0 or if x = 0 and y ≤ 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> pow(const VecBase<T, Size> &x, const T &y)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::pow(x[i], y);
  }
  return result;
}

/**
 * Return the value of x raised to the y power.
 * The result is undefined if x < 0 or if x = 0 and y ≤ 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> pow(const VecBase<T, Size> &x, const VecBase<T, Size> &y)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::pow(x[i], y[i]);
  }
  return result;
}

/* Per-element exponent. */
template<typename T, int Size> [[nodiscard]] inline VecBase<T, Size> exp(const VecBase<T, Size> &x)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::exp(x[i]);
  }
  return result;
}

/**
 * Returns \a a if it is a multiple of \a b or the next multiple or \a b after \b a .
 * In other words, it is equivalent to `divide_ceil(a, b) * b`.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> ceil_to_multiple(const VecBase<T, Size> &a,
                                                       const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    BLI_assert(a[i] >= 0);
    BLI_assert(b[i] > 0);
    result[i] = ((a[i] + b[i] - 1) / b[i]) * b[i];
  }
  return result;
}

/**
 * Integer division that returns the ceiling, instead of flooring like normal C division.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> divide_ceil(const VecBase<T, Size> &a,
                                                  const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    BLI_assert(a[i] >= 0);
    BLI_assert(b[i] > 0);
    result[i] = (a[i] + b[i] - 1) / b[i];
  }
  return result;
}

template<typename T, int Size>
void min_max(const VecBase<T, Size> &vector, VecBase<T, Size> &min, VecBase<T, Size> &max)
{
  min = math::min(vector, min);
  max = math::max(vector, max);
}

/**
 * Returns 0 if denominator is exactly equal to 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> safe_divide(const VecBase<T, Size> &a,
                                                  const VecBase<T, Size> &b)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = (b[i] == 0) ? 0 : a[i] / b[i];
  }
  return result;
}

/**
 * Returns 0 if denominator is exactly equal to 0.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> safe_divide(const VecBase<T, Size> &a, const T &b)
{
  return (b != 0) ? a / b : VecBase<T, Size>(0.0f);
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> floor(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::floor(a[i]);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> round(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::round(a[i]);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> ceil(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::ceil(a[i]);
  }
  return result;
}

/**
 * Per-element square root.
 * Negative elements are evaluated to NaN.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> sqrt(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::sqrt(a[i]);
  }
  return result;
}

/**
 * Per-element square root.
 * Negative elements are evaluated to zero.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> safe_sqrt(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = a[i] >= T(0) ? math ::sqrt(a[i]) : T(0);
  }
  return result;
}

/**
 * Per-element inverse.
 * Zero elements are evaluated to NaN.
 */
template<typename T, int Size> [[nodiscard]] inline VecBase<T, Size> rcp(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::rcp(a[i]);
  }
  return result;
}

/**
 * Per-element inverse.
 * Zero elements are evaluated to zero.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> safe_rcp(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::safe_rcp(a[i]);
  }
  return result;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> fract(const VecBase<T, Size> &a)
{
  VecBase<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result[i] = math::fract(a[i]);
  }
  return result;
}

/**
 * Dot product between two vectors.
 * Equivalent to component wise multiplication followed by summation of the result.
 * Equivalent to the cosine of the angle between the two vectors if the vectors are normalized.
 * \note prefer using `length_manhattan(a)` than `dot(a, vec(1))` to get the sum of all components.
 */
template<typename T, int Size>
[[nodiscard]] inline T dot(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  T result = a[0] * b[0];
  for (int i = 1; i < Size; i++) {
    result += a[i] * b[i];
  }
  return result;
}

/**
 * Returns summation of all components.
 */
template<typename T, int Size> [[nodiscard]] inline T length_manhattan(const VecBase<T, Size> &a)
{
  T result = math::abs(a[0]);
  for (int i = 1; i < Size; i++) {
    result += math::abs(a[i]);
  }
  return result;
}

template<typename T, int Size> [[nodiscard]] inline T length_squared(const VecBase<T, Size> &a)
{
  return dot(a, a);
}

template<typename T, int Size> [[nodiscard]] inline T length(const VecBase<T, Size> &a)
{
  return math::sqrt(length_squared(a));
}

/** Return true if each individual column is unit scaled. Mainly for assert usage. */
template<typename T, int Size> [[nodiscard]] inline bool is_unit_scale(const VecBase<T, Size> &v)
{
  /* Checks are flipped so NAN doesn't assert because we're making sure the value was
   * normalized and in the case we don't want NAN to be raising asserts since there
   * is nothing to be done in that case. */
  const T test_unit = math::length_squared(v);
  return (!(math::abs(test_unit - T(1)) >= AssertUnitEpsilon<T>::value) ||
          !(math::abs(test_unit) >= AssertUnitEpsilon<T>::value));
}

template<typename T, int Size>
[[nodiscard]] inline T distance_manhattan(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  return length_manhattan(a - b);
}

template<typename T, int Size>
[[nodiscard]] inline T distance_squared(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  return length_squared(a - b);
}

template<typename T, int Size>
[[nodiscard]] inline T distance(const VecBase<T, Size> &a, const VecBase<T, Size> &b)
{
  return length(a - b);
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> reflect(const VecBase<T, Size> &incident,
                                              const VecBase<T, Size> &normal)
{
  BLI_assert(is_unit_scale(normal));
  return incident - 2.0 * dot(normal, incident) * normal;
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> refract(const VecBase<T, Size> &incident,
                                              const VecBase<T, Size> &normal,
                                              const T &eta)
{
  float dot_ni = dot(normal, incident);
  float k = 1.0f - eta * eta * (1.0f - dot_ni * dot_ni);
  if (k < 0.0f) {
    return VecBase<T, Size>(0.0f);
  }
  return eta * incident - (eta * dot_ni + sqrt(k)) * normal;
}

/**
 * Project \a p onto \a v_proj .
 * Returns zero vector if \a v_proj is degenerate (zero length).
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> project(const VecBase<T, Size> &p,
                                              const VecBase<T, Size> &v_proj)
{
  if (UNLIKELY(is_zero(v_proj))) {
    return VecBase<T, Size>(0.0f);
  }
  return v_proj * (dot(p, v_proj) / dot(v_proj, v_proj));
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> normalize_and_get_length(const VecBase<T, Size> &v,
                                                               T &out_length)
{
  out_length = length_squared(v);
  /* A larger value causes normalize errors in a scaled down models with camera extreme close. */
  constexpr T threshold = std::is_same_v<T, double> ? 1.0e-70 : 1.0e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return v / out_length;
  }
  /* Either the vector is small or one of it's values contained `nan`. */
  out_length = 0.0;
  return VecBase<T, Size>(0.0);
}

template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> normalize(const VecBase<T, Size> &v)
{
  T len;
  return normalize_and_get_length(v, len);
}

/**
 * \return cross perpendicular vector to \a a and \a b.
 * \note Return zero vector if \a a and \a b are collinear.
 * \note The length of the resulting vector is equal to twice the area of the triangle between \a a
 * and \a b ; and it is equal to the sine of the angle between \a a and \a b if they are
 * normalized.
 * \note Blender 3D space uses right handedness: \a a = thumb, \a b = index, return = middle.
 */
template<typename T>
[[nodiscard]] inline VecBase<T, 3> cross(const VecBase<T, 3> &a, const VecBase<T, 3> &b)
{
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/**
 * Same as `cross(a, b)` but use double float precision for the computation.
 */
[[nodiscard]] inline VecBase<float, 3> cross_high_precision(const VecBase<float, 3> &a,
                                                            const VecBase<float, 3> &b)
{
  return {float(double(a.y) * double(b.z) - double(a.z) * double(b.y)),
          float(double(a.z) * double(b.x) - double(a.x) * double(b.z)),
          float(double(a.x) * double(b.y) - double(a.y) * double(b.x))};
}

/**
 * \param poly: Array of points around a polygon. They don't have to be co-planar.
 * \return Best fit plane normal for the given polygon loop or zero vector if point
 * array is too short. Not normalized.
 */
template<typename T> [[nodiscard]] inline VecBase<T, 3> cross_poly(Span<VecBase<T, 3>> poly)
{
  /* Newell's Method. */
  int nv = int(poly.size());
  if (nv < 3) {
    return VecBase<T, 3>(0, 0, 0);
  }
  const VecBase<T, 3> *v_prev = &poly[nv - 1];
  const VecBase<T, 3> *v_curr = &poly[0];
  VecBase<T, 3> n(0, 0, 0);
  for (int i = 0; i < nv;) {
    n[0] = n[0] + ((*v_prev)[1] - (*v_curr)[1]) * ((*v_prev)[2] + (*v_curr)[2]);
    n[1] = n[1] + ((*v_prev)[2] - (*v_curr)[2]) * ((*v_prev)[0] + (*v_curr)[0]);
    n[2] = n[2] + ((*v_prev)[0] - (*v_curr)[0]) * ((*v_prev)[1] + (*v_curr)[1]);
    v_prev = v_curr;
    ++i;
    if (i < nv) {
      v_curr = &poly[i];
    }
  }
  return n;
}

/**
 * Return normal vector to a triangle.
 * The result is not normalized and can be degenerate.
 */
template<typename T>
[[nodiscard]] inline VecBase<T, 3> cross_tri(const VecBase<T, 3> &v1,
                                             const VecBase<T, 3> &v2,
                                             const VecBase<T, 3> &v3)
{
  return cross(v1 - v2, v2 - v3);
}

/**
 * Return normal vector to a triangle.
 * The result is normalized but can still be degenerate.
 */
template<typename T>
[[nodiscard]] inline VecBase<T, 3> normal_tri(const VecBase<T, 3> &v1,
                                              const VecBase<T, 3> &v2,
                                              const VecBase<T, 3> &v3)
{
  return normalize(cross_tri(v1, v2, v3));
}

/**
 * Per component linear interpolation.
 * \param t: interpolation factor. Return \a a if equal 0. Return \a b if equal 1.
 * Outside of [0..1] range, use linear extrapolation.
 */
template<typename T, typename FactorT, int Size>
[[nodiscard]] inline VecBase<T, Size> interpolate(const VecBase<T, Size> &a,
                                                  const VecBase<T, Size> &b,
                                                  const FactorT &t)
{
  return a * (1 - t) + b * t;
}

/**
 * \return Point halfway between \a a and \a b.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> midpoint(const VecBase<T, Size> &a,
                                               const VecBase<T, Size> &b)
{
  return (a + b) * 0.5;
}

/**
 * Return `vector` if `incident` and `reference` are pointing in the same direction.
 */
template<typename T, int Size>
[[nodiscard]] inline VecBase<T, Size> faceforward(const VecBase<T, Size> &vector,
                                                  const VecBase<T, Size> &incident,
                                                  const VecBase<T, Size> &reference)
{
  return (dot(reference, incident) < 0) ? vector : -vector;
}

/**
 * \return Index of the component with the greatest magnitude.
 */
template<typename T> [[nodiscard]] inline int dominant_axis(const VecBase<T, 3> &a)
{
  VecBase<T, 3> b = abs(a);
  return ((b.x > b.y) ? ((b.x > b.z) ? 0 : 2) : ((b.y > b.z) ? 1 : 2));
}

/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector can be in any perpendicular direction.
 * \note Returned vector might not the same length as \a v.
 */
template<typename T> [[nodiscard]] inline VecBase<T, 3> orthogonal(const VecBase<T, 3> &v)
{
  const int axis = dominant_axis(v);
  switch (axis) {
    case 0:
      return {-v.y - v.z, v.x, v.x};
    case 1:
      return {v.y, -v.x - v.z, v.y};
    case 2:
      return {v.z, v.z, -v.x - v.y};
  }
  return v;
}

/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector can be in any perpendicular direction.
 */
template<typename T> [[nodiscard]] inline VecBase<T, 2> orthogonal(const VecBase<T, 2> &v)
{
  return {-v.y, v.x};
}

/**
 * Returns true if vectors are equal within the given epsilon.
 */
template<typename T, int Size>
[[nodiscard]] inline bool is_equal(const VecBase<T, Size> &a,
                                   const VecBase<T, Size> &b,
                                   const T epsilon = T(0))
{
  for (int i = 0; i < Size; i++) {
    if (math::abs(a[i] - b[i]) > epsilon) {
      return false;
    }
  }
  return true;
}

/** Intersections. */

template<typename T> struct isect_result {
  enum {
    LINE_LINE_COLINEAR = -1,
    LINE_LINE_NONE = 0,
    LINE_LINE_EXACT = 1,
    LINE_LINE_CROSS = 2,
  } kind;
  typename T::base_type lambda;
};

template<typename T, int Size>
[[nodiscard]] isect_result<VecBase<T, Size>> isect_seg_seg(const VecBase<T, Size> &v1,
                                                           const VecBase<T, Size> &v2,
                                                           const VecBase<T, Size> &v3,
                                                           const VecBase<T, Size> &v4);

}  // namespace blender::math

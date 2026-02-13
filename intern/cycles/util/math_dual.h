/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"
#include "util/math_float3.h"
#include "util/types_dual.h"
#include "util/types_float3.h"

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

template<class T> ccl_device_inline bool is_zero(const ccl_private dual<T> &a)
{
  return is_zero(a.val);
}

ccl_device_inline bool operator<(const ccl_private dual1 &a, const float b)
{
  return a.val < b;
}

/* Multiplication of dual by scalar. */
template<class T1, class T2> ccl_device_inline dual<T1> operator*(const dual<T1> a, T2 b)
{
  return {a.val * b, a.dx * b, a.dy * b};
}

/* Multiplication of scalar by dual. */
template<class T> ccl_device_inline dual<T> operator*(const T a, const ccl_private dual<T> &b)
{
  return {a * b.val, a * b.dx, a * b.dy};
}

/* Multiplication of duals.
 * (uv)' = uv' + u'v. */
template<class T1, class T2>
ccl_device_inline dual<T1> operator*(const ccl_private dual<T1> &u, const ccl_private dual<T2> &v)
{
  return {u.val * v.val, u.val * v.dx + u.dx * v.val, u.val * v.dy + u.dy * v.val};
}

/* Division of dual by scalar. */
template<class T> ccl_device_inline dual<T> operator/(const dual<T> a, T b)
{
  const T inv_b = 1.0f / b;
  return {a.val * inv_b, a.dx * inv_b, a.dy * inv_b};
}

/* Division of dual by dual.
 * (u/v)' = (u' - v' * u/v) / v. */
template<class T1, class T2>
ccl_device_inline dual<T1> operator/(const ccl_private dual<T1> &u, const ccl_private dual<T2> &v)
{
  const T2 inv_v = 1.0f / v.val;
  /* NOTE: Numerically `u/v != u*inv_v`, for compatibility we compute `u/v`. */
  const T1 u_v = u.val / v.val;
  return {u_v, (u.dx - u_v * v.dx) * inv_v, (u.dy - u_v * v.dy) * inv_v};
}

template<class T1, class T2>
ccl_device_inline dual<T1> operator/=(ccl_private dual<T1> &a, const ccl_private dual<T2> &b)
{
  return a = a / b;
}

/* Addition of duals. */
template<class T> ccl_device_inline dual<T> operator+(const dual<T> a, const dual<T> b)
{
  return {a.val + b.val, a.dx + b.dx, a.dy + b.dy};
}

/* Addition of dual and scalar. */
template<class T1, class T2> ccl_device_inline dual<T1> operator+(const dual<T1> a, T2 b)
{
  return {a.val + b, a.dx, a.dy};
}

/* Addition of scalar and dual. */
template<class T1, class T2> ccl_device_inline dual<T2> operator+(const T1 a, const dual<T2> b)
{
  return {a + b.val, b.dx, b.dy};
}

/* Subtraction of dual by scalar. */
template<class T1, class T2> ccl_device_inline dual<T1> operator-(const dual<T1> a, T2 b)
{
  return {a.val - b, a.dx, a.dy};
}

/* Subtraction of scalar by dual. */
template<class T1, class T2> ccl_device_inline dual<T2> operator-(const T1 a, const dual<T2> b)
{
  return {a - b.val, -b.dx, -b.dy};
}

/* Subtraction of duals. */
template<class T>
ccl_device_inline dual<T> operator-(const ccl_private dual<T> &a, const ccl_private dual<T> &b)
{
  return {a.val - b.val, a.dx - b.dx, a.dy - b.dy};
}

/* Negation. */
template<class T> ccl_device_inline dual<T> operator-(const ccl_private dual<T> &a)
{
  return {-a.val, -a.dx, -a.dy};
}

/* dfdx = dfdu * dudx */
template<class T>
ccl_device_inline dual<T> chain_rule(const ccl_private dual<T> &u,
                                     const ccl_private T &f,
                                     const ccl_private T &dfdu)
{
  return {f, dfdu * u.dx, dfdu * u.dy};
}

/* dfdx = dfdu * dudx + dfdv * dvdx. */
template<class T>
ccl_device_inline dual<T> chain_rule(const ccl_private dual<T> &u,
                                     const ccl_private dual<T> &v,
                                     const ccl_private T &f,
                                     const ccl_private T &dfdu,
                                     const ccl_private T &dfdv)
{
  return {f, dfdu * u.dx + dfdv * v.dx, dfdu * u.dy + dfdv * v.dy};
}

template<class MaskType>
ccl_device_inline dual3 select(const MaskType mask, const dual3 a, const dual3 b)
{
#if defined(__KERNEL_METAL__)
  const bool3 mask_ = bool3(mask);
  return {metal::select(b.val, a.val, mask_),
          metal::select(b.dx, a.dx, mask_),
          metal::select(b.dy, a.dy, mask_)};
#elif defined(__KERNEL_SSE__)
#  ifdef __KERNEL_SSE42__
  const auto mask_ = _mm_castsi128_ps(mask.m128);
  return {float3(_mm_blendv_ps(b.val.m128, a.val.m128, mask_)),
          float3(_mm_blendv_ps(b.dx.m128, a.dx.m128, mask_)),
          float3(_mm_blendv_ps(b.dy.m128, a.dy.m128, mask_))};
#  else
  const auto mask_ = _mm_castsi128_ps(mask);
  return {float3(_mm_or_ps(_mm_and_ps(mask_, a.val), _mm_andnot_ps(mask_, b.val))),
          float3(_mm_or_ps(_mm_and_ps(mask_, a.dx), _mm_andnot_ps(mask_, b.dx))),
          float3(_mm_or_ps(_mm_and_ps(mask_, a.dy), _mm_andnot_ps(mask_, b.dy)))};
#  endif
#else
  return make_float3(mask.x ? a.x() : b.x(), mask.y ? a.y() : b.y(), mask.z ? a.z() : b.z());
#endif
}

/* Functions with zero derivatives. */
template<class T> ccl_device_inline dual<T> floor(const ccl_private dual<T> &a)
{
  return dual<T>(floor(a.val));
}

template<class T> ccl_device_inline dual<T> ceil(const ccl_private dual<T> &a)
{
  return dual<T>(ceil(a.val));
}

template<class T> ccl_device_inline dual<T> compatible_sign(const ccl_private dual<T> &u)
{
  return dual<T>(compatible_sign(u.val));
}

/* f = u - round(u / v) * v, f' = u'. */
ccl_device_inline dual3 safe_fmod(const dual3 u, const dual3 v)
{
  return {safe_fmod(u.val, v.val), u.dx, u.dy};
}

ccl_device_inline dual3 safe_floored_fmod(const dual3 a, const dual3 b)
{
  return select(component_is_zero(b.val), make_zero<dual3>(), a - floor(a.val / b.val) * b);
}

template<class T> ccl_device_inline dual<T> safe_divide(const dual<T> f, const T g)
{
  return select(component_is_zero(g), make_zero<dual<T>>(), f / g);
}

template<class T>
ccl_device_inline dual<T> safe_divide(const ccl_private dual<T> &f, const ccl_private dual<T> &g)
{
  return select(component_is_zero(g.val), make_zero<dual<T>>(), f / g);
}

/* Adapted from GODOT-engine math_funcs.h. */
ccl_device_inline dual3 wrap(const dual3 value, const dual3 max, const dual3 min)
{
  return safe_floored_fmod(value - min, max - min) + min;
}

ccl_device_inline dual3 min(const ccl_private dual3 &a, const ccl_private dual3 &b)
{
  return select(a.val < b.val, a, b);
}

ccl_device_inline dual3 max(const ccl_private dual3 &a, const ccl_private dual3 &b)
{
  return select(a.val > b.val, a, b);
}

ccl_device_inline dual1 max(const ccl_private dual1 &a, const ccl_private dual1 &b)
{
  return a.val > b.val ? a : b;
}

ccl_device_inline dual3 fabs(const ccl_private dual3 &a)
{
  return select(a.val > zero_float3(), a, -a);
}

template<class T> ccl_device_inline dual1 average(const dual<T> a)
{
  return {average(a.val), average(a.dx), average(a.dy)};
}

template<class T> ccl_device_inline dual1 reduce_add(const dual<T> a)
{
  return {reduce_add(a.val), reduce_add(a.dx), reduce_add(a.dy)};
}

/* f(u) = sqrt(u), dfdu = 1 / (2 * sqrt(u)). */
ccl_device_inline dual1 sqrt(const ccl_private dual1 &u)
{
  const float f = sqrtf(u.val);
  return chain_rule(u, f, 0.5f / f);
}

template<class T> ccl_device_inline dual1 len(const ccl_private dual<T> &a)
{
  return sqrt(dot(a, a));
}

template<class T1, class T2> ccl_device_inline dual1 dot(const dual<T1> a, const T2 b)
{
  return reduce_add(a * b);
}

template<class T> ccl_device_inline dual1 len_squared(const ccl_private dual<T> &a)
{
  return dot(a, a);
}

template<class T>
ccl_device_inline dual1 distance(const ccl_private dual<T> &a, const ccl_private dual<T> &b)
{
  return len(a - b);
}

ccl_device_inline dual3 cross(const ccl_private dual3 &a, const ccl_private dual3 &b)
{
  return {cross(a.val, b.val),
          cross(a.val, b.dx) + cross(a.dx, b.val),
          cross(a.val, b.dy) + cross(a.dy, b.val)};
}

ccl_device_inline dual3 cross(const ccl_private dual3 &a, const ccl_private float3 &b)
{
  return {cross(a.val, b), cross(a.dx, b), cross(a.dy, b)};
}

ccl_device_inline dual3 cross(const ccl_private float3 &a, const ccl_private dual3 &b)
{
  return -cross(b, a);
}

/* f(u) = 1 / sqrt(u), dfdu = -1 / (2 * u^(3/2)). */
ccl_device_inline dual1 inversesqrt(const ccl_private dual1 &u)
{
  const float f = inversesqrtf(u.val);
  return chain_rule(u, f, -0.5f * safe_divide(f, u.val));
}

template<class T> ccl_device_inline dual<T> normalize(const ccl_private dual<T> &a)
{
  return a * inversesqrt(len_squared(a));
}

template<class T> ccl_device_inline dual<T> safe_normalize(const ccl_private dual<T> &a)
{
  const dual1 len_sq = len_squared(a);
  return is_zero(len_sq) ? make_zero<dual<T>>() : a * inversesqrt(len_sq);
}

/* f(y, x) = atan2(y, x),
 * dfdx = -y / (x^2 + y^2),
 * dfdy = x / (x^2 + y^2) */
ccl_device_inline dual1 atan2(const ccl_private dual1 &y, const ccl_private dual1 &x)
{
  const float inv_len = safe_divide(1.0f, sqr(x.val) + sqr(y.val));
  const float dfdx = -y.val * inv_len;
  const float dfdy = x.val * inv_len;
  return chain_rule(x, y, atan2f(y.val, x.val), dfdx, dfdy);
}

/* f(u) = acos(u), dfdu = -1 / sqrt(1 - u^2). */
ccl_device_inline dual1 acos(const ccl_private dual1 &u)
{
  return chain_rule(u, acosf(u.val), -inversesqrtf(1.0f - sqr(u.val)));
}

ccl_device_inline dual1 safe_acos(const ccl_private dual1 &u)
{
  return chain_rule(u, safe_acosf(u.val), -inversesqrtf(1.0f - sqr(u.val)));
}

template<class T> ccl_device_inline dual3 reflect(const dual3 incident, const T unit_normal)
{
  return incident - unit_normal * make_float3(dot(incident, unit_normal)) * 2.0f;
}

ccl_device_inline dual3 refract(const dual3 incident, const dual3 normal, const dual1 eta)
{
  const dual1 NI = dot(incident, normal);
  const dual1 k = 1.0f - eta * eta * (1.0f - NI * NI);
  if (k.val < 0.0f) {
    return dual3();
  }
  return incident * eta - normal * (eta * NI + sqrt(k));
}

ccl_device_inline dual3 faceforward(const dual3 vector,
                                    const dual3 incident,
                                    const dual3 reference)
{
  return (dot(reference, incident) < 0.0f) ? vector : -vector;
}

ccl_device_inline dual3 project(const dual3 v, const dual3 v_proj)
{
  const dual1 len_squared = dot(v_proj, v_proj);
  return (len_squared.val != 0.0f) ? v_proj * (dot(v, v_proj) / len_squared) : dual3();
}

template<class T> ccl_device_inline dual<T> sin(const ccl_private dual<T> &x)
{
  T sinx, cosx;
  sincos(x.val, &sinx, &cosx);
  return chain_rule(x, sinx, cosx);
}

template<class T> ccl_device_inline dual<T> cos(const ccl_private dual<T> &x)
{
  T sinx, cosx;
  sincos(x.val, &sinx, &cosx);
  return chain_rule(x, cosx, -sinx);
}

ccl_device_inline dual3 tan(const ccl_private dual3 &x)
{
  const float3 tanx = tan(x.val);
  const float3 secx = safe_divide(one_float3(), cos(x.val));
  return chain_rule(x, tanx, sqr(secx));
}

/* f(u, v) = u^v, dfdu = v u^(v-1), dfdv = u^v ln(u). */
template<class T>
ccl_device_inline dual<T> safe_pow(const ccl_private dual<T> &u, const ccl_private dual<T> &v)
{
  /* u^(v-1). */
  const T u_v_minus_1 = safe_pow(u.val, v.val - 1.0f);
  /* u^v = u * u^(v-1). */
  /* NOTE: numerically `u^v != u*u^(v-1)`, but the current behaviour matches OSL. */
  const T f = u.val * u_v_minus_1;
  return chain_rule(u, v, f, v.val * u_v_minus_1, f * safe_log(u.val));
}

/* Projections. */
ccl_device_inline dual2 map_to_tube(const dual3 co)
{
  dual1 u, v;
  const dual1 length = len(make_float2(co));
  if (length.val > 0.0f) {
    u = (1.0f - (atan2(co.x(), co.y()) / M_PI_F)) * 0.5f;
    v = (co.z() + 1.0f) * 0.5f;
  }
  else {
    u = v = make_zero<dual1>();
  }
  return make_float2(u, v);
}

ccl_device_inline dual2 map_to_sphere(const dual3 co)
{
  const dual1 l = dot(co, co);
  dual1 u, v;
  if (l.val > 0.0f) {
    if (UNLIKELY(co.val.x == 0.0f && co.val.y == 0.0f)) {
      u = make_zero<dual1>(); /* Otherwise domain error. */
    }
    else {
      u = (0.5f - atan2(co.x(), co.y()) * M_1_2PI_F);
    }
    v = 1.0f - safe_acos(co.z() * inversesqrt(l)) * M_1_PI_F;
  }
  else {
    u = v = make_zero<dual1>();
  }
  return make_float2(u, v);
}

CCL_NAMESPACE_END

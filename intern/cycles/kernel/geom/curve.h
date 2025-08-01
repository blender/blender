/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/motion_curve.h"

CCL_NAMESPACE_BEGIN

/* Curve Primitive
 *
 * Curve primitive for rendering hair and fur. These can be render as flat
 * ribbons or curves with actual thickness. The curve can also be rendered as
 * line segments rather than curves for better performance.
 */

#ifdef __HAIR__

/* Partial derivative of f w.r.t. x, namely ∂f/∂x
 * f is a function of u (along the curve)
 *       f(u) = f0 * (1 - u) + f1 * u,
 * The partial derivative in x is
 *    ∂f/∂x = ∂f/∂u * ∂u/∂x
 *          = (f1 - f0) * du.dx. */
template<typename T>
ccl_device_inline T curve_attribute_dfdx(const ccl_private differential &du,
                                         const ccl_private T &f0,
                                         const ccl_private T &f1)
{
  return du.dx * (f1 - f0);
}

/* Partial derivative of f w.r.t. in x, namely ∂f/∂y, similarly computed as ∂f/∂x above. */
template<typename T>
ccl_device_inline T curve_attribute_dfdy(const ccl_private differential &du,
                                         const ccl_private T &f0,
                                         const ccl_private T &f1)
{
  return du.dy * (f1 - f0);
}

/* Read attributes on various curve elements, and compute the partial derivatives if requested. */

template<typename T>
ccl_device dual<T> curve_attribute(KernelGlobals kg,
                                   const ccl_private ShaderData *sd,
                                   const AttributeDescriptor desc,
                                   const bool dx = false,
                                   const bool dy = false)
{
  dual<T> result;
  if (desc.element & (ATTR_ELEMENT_CURVE_KEY | ATTR_ELEMENT_CURVE_KEY_MOTION)) {
    const KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    const int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    const int k1 = k0 + 1;

    const T f0 = attribute_data_fetch<T>(kg, desc.offset + k0);
    const T f1 = attribute_data_fetch<T>(kg, desc.offset + k1);

#  ifdef __RAY_DIFFERENTIALS__
    if (dx) {
      result.dx = curve_attribute_dfdx(sd->du, f0, f1);
    }
    if (dy) {
      result.dy = curve_attribute_dfdy(sd->du, f0, f1);
    }
#  endif

    result.val = mix(f0, f1, sd->u);
    return result;
  }

  /* idea: we can't derive any useful differentials here, but for tiled
   * mipmap image caching it would be useful to avoid reading the highest
   * detail level always. maybe a derivative based on the hair density
   * could be computed somehow? */

  if (desc.element == ATTR_ELEMENT_CURVE) {
    return dual<T>(attribute_data_fetch<T>(kg, desc.offset + sd->prim));
  }
  return make_zero<dual<T>>();
}

/* Curve thickness */

ccl_device float curve_thickness(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  float r = 0.0f;

  if (sd->type & PRIMITIVE_CURVE) {
    const KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    const int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    const int k1 = k0 + 1;

    float4 P_curve[2];

#  ifdef __OBJECT_MOTION__
    if (sd->type & PRIMITIVE_MOTION) {
      motion_curve_keys_linear(kg, sd->object, sd->time, k0, k1, P_curve);
    }
    else
#  endif
    {
      P_curve[0] = kernel_data_fetch(curve_keys, k0);
      P_curve[1] = kernel_data_fetch(curve_keys, k1);
    }

    r = (P_curve[1].w - P_curve[0].w) * sd->u + P_curve[0].w;
  }

  return r * 2.0f;
}

/* Curve random */

ccl_device float curve_random(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  if (sd->type & PRIMITIVE_CURVE) {
    const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_CURVE_RANDOM);
    return (desc.offset != ATTR_STD_NOT_FOUND) ? curve_attribute<float>(kg, sd, desc).val : 0.0f;
  }
  return 0.0f;
}

/* Curve location for motion pass, linear interpolation between keys and
 * ignoring radius because we do the same for the motion keys */

ccl_device float3 curve_motion_center_location(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  const KernelCurve curve = kernel_data_fetch(curves, sd->prim);
  const int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
  const int k1 = k0 + 1;

  float4 P_curve[2];

  P_curve[0] = kernel_data_fetch(curve_keys, k0);
  P_curve[1] = kernel_data_fetch(curve_keys, k1);

  return make_float3(P_curve[1]) * sd->u + make_float3(P_curve[0]) * (1.0f - sd->u);
}

/* Curve tangent normal */

ccl_device float3 curve_tangent_normal(const ccl_private ShaderData *sd)
{
  float3 tgN = make_float3(0.0f, 0.0f, 0.0f);

  if (sd->type & PRIMITIVE_CURVE) {

    tgN = -(-sd->wi - sd->dPdu * (dot(sd->dPdu, -sd->wi) / len_squared(sd->dPdu)));
    tgN = normalize(tgN);

    /* need to find suitable scaled gd for corrected normal */
#  if 0
    tgN = normalize(tgN - gd * sd->dPdu);
#  endif
  }

  return tgN;
}

/* Curve bounds utility function */

ccl_device_inline void curvebounds(ccl_private float *lower,
                                   ccl_private float *upper,
                                   ccl_private float *extremta,
                                   ccl_private float *extrema,
                                   ccl_private float *extremtb,
                                   ccl_private float *extremb,
                                   float p0,
                                   float p1,
                                   float p2,
                                   float p3)
{
  float halfdiscroot = (p2 * p2 - 3 * p3 * p1);
  float ta = -1.0f;
  float tb = -1.0f;

  *extremta = -1.0f;
  *extremtb = -1.0f;
  *upper = p0;
  *lower = (p0 + p1) + (p2 + p3);
  *extrema = *upper;
  *extremb = *lower;

  if (*lower >= *upper) {
    *upper = *lower;
    *lower = p0;
  }

  if (halfdiscroot >= 0) {
    const float inv3p3 = (1.0f / 3.0f) / p3;
    halfdiscroot = sqrtf(halfdiscroot);
    ta = (-p2 - halfdiscroot) * inv3p3;
    tb = (-p2 + halfdiscroot) * inv3p3;
  }

  float t2;
  float t3;

  if (ta > 0.0f && ta < 1.0f) {
    t2 = ta * ta;
    t3 = t2 * ta;
    *extremta = ta;
    *extrema = p3 * t3 + p2 * t2 + p1 * ta + p0;

    *upper = fmaxf(*extrema, *upper);
    *lower = fminf(*extrema, *lower);
  }

  if (tb > 0.0f && tb < 1.0f) {
    t2 = tb * tb;
    t3 = t2 * tb;
    *extremtb = tb;
    *extremb = p3 * t3 + p2 * t2 + p1 * tb + p0;

    *upper = fmaxf(*extremb, *upper);
    *lower = fminf(*extremb, *lower);
  }
}

#endif /* __HAIR__ */

CCL_NAMESPACE_END

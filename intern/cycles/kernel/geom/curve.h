/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Curve Primitive
 *
 * Curve primitive for rendering hair and fur. These can be render as flat
 * ribbons or curves with actual thickness. The curve can also be rendered as
 * line segments rather than curves for better performance.
 */

#ifdef __HAIR__

/* Reading attributes on various curve elements */

ccl_device float curve_attribute_float(KernelGlobals kg,
                                       ccl_private const ShaderData *sd,
                                       const AttributeDescriptor desc,
                                       ccl_private float *dx,
                                       ccl_private float *dy)
{
  if (desc.element & (ATTR_ELEMENT_CURVE_KEY | ATTR_ELEMENT_CURVE_KEY_MOTION)) {
    KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    int k1 = k0 + 1;

    float f0 = kernel_data_fetch(attributes_float, desc.offset + k0);
    float f1 = kernel_data_fetch(attributes_float, desc.offset + k1);

#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = sd->du.dx * (f1 - f0);
    if (dy)
      *dy = 0.0f;
#  endif

    return (1.0f - sd->u) * f0 + sd->u * f1;
  }
  else {
#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = 0.0f;
    if (dy)
      *dy = 0.0f;
#  endif

    if (desc.element & (ATTR_ELEMENT_CURVE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_CURVE) ? desc.offset + sd->prim :
                                                                desc.offset;
      return kernel_data_fetch(attributes_float, offset);
    }
    else {
      return 0.0f;
    }
  }
}

ccl_device float2 curve_attribute_float2(KernelGlobals kg,
                                         ccl_private const ShaderData *sd,
                                         const AttributeDescriptor desc,
                                         ccl_private float2 *dx,
                                         ccl_private float2 *dy)
{
  if (desc.element & (ATTR_ELEMENT_CURVE_KEY | ATTR_ELEMENT_CURVE_KEY_MOTION)) {
    KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    int k1 = k0 + 1;

    float2 f0 = kernel_data_fetch(attributes_float2, desc.offset + k0);
    float2 f1 = kernel_data_fetch(attributes_float2, desc.offset + k1);

#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = sd->du.dx * (f1 - f0);
    if (dy)
      *dy = make_float2(0.0f, 0.0f);
#  endif

    return (1.0f - sd->u) * f0 + sd->u * f1;
  }
  else {
    /* idea: we can't derive any useful differentials here, but for tiled
     * mipmap image caching it would be useful to avoid reading the highest
     * detail level always. maybe a derivative based on the hair density
     * could be computed somehow? */
#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = make_float2(0.0f, 0.0f);
    if (dy)
      *dy = make_float2(0.0f, 0.0f);
#  endif

    if (desc.element & (ATTR_ELEMENT_CURVE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_CURVE) ? desc.offset + sd->prim :
                                                                desc.offset;
      return kernel_data_fetch(attributes_float2, offset);
    }
    else {
      return make_float2(0.0f, 0.0f);
    }
  }
}

ccl_device float3 curve_attribute_float3(KernelGlobals kg,
                                         ccl_private const ShaderData *sd,
                                         const AttributeDescriptor desc,
                                         ccl_private float3 *dx,
                                         ccl_private float3 *dy)
{
  if (desc.element & (ATTR_ELEMENT_CURVE_KEY | ATTR_ELEMENT_CURVE_KEY_MOTION)) {
    KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    int k1 = k0 + 1;

    float3 f0 = kernel_data_fetch(attributes_float3, desc.offset + k0);
    float3 f1 = kernel_data_fetch(attributes_float3, desc.offset + k1);

#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = sd->du.dx * (f1 - f0);
    if (dy)
      *dy = make_float3(0.0f, 0.0f, 0.0f);
#  endif

    return (1.0f - sd->u) * f0 + sd->u * f1;
  }
  else {
#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = make_float3(0.0f, 0.0f, 0.0f);
    if (dy)
      *dy = make_float3(0.0f, 0.0f, 0.0f);
#  endif

    if (desc.element & (ATTR_ELEMENT_CURVE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_CURVE) ? desc.offset + sd->prim :
                                                                desc.offset;
      return kernel_data_fetch(attributes_float3, offset);
    }
    else {
      return make_float3(0.0f, 0.0f, 0.0f);
    }
  }
}

ccl_device float4 curve_attribute_float4(KernelGlobals kg,
                                         ccl_private const ShaderData *sd,
                                         const AttributeDescriptor desc,
                                         ccl_private float4 *dx,
                                         ccl_private float4 *dy)
{
  if (desc.element & (ATTR_ELEMENT_CURVE_KEY | ATTR_ELEMENT_CURVE_KEY_MOTION)) {
    KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    int k1 = k0 + 1;

    float4 f0 = kernel_data_fetch(attributes_float4, desc.offset + k0);
    float4 f1 = kernel_data_fetch(attributes_float4, desc.offset + k1);

#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = sd->du.dx * (f1 - f0);
    if (dy)
      *dy = zero_float4();
#  endif

    return (1.0f - sd->u) * f0 + sd->u * f1;
  }
  else {
#  ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = zero_float4();
    if (dy)
      *dy = zero_float4();
#  endif

    if (desc.element & (ATTR_ELEMENT_CURVE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_CURVE) ? desc.offset + sd->prim :
                                                                desc.offset;
      return kernel_data_fetch(attributes_float4, offset);
    }
    else {
      return zero_float4();
    }
  }
}

/* Curve thickness */

ccl_device float curve_thickness(KernelGlobals kg, ccl_private const ShaderData *sd)
{
  float r = 0.0f;

  if (sd->type & PRIMITIVE_CURVE) {
    KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    int k1 = k0 + 1;

    float4 P_curve[2];

    if (!(sd->type & PRIMITIVE_MOTION)) {
      P_curve[0] = kernel_data_fetch(curve_keys, k0);
      P_curve[1] = kernel_data_fetch(curve_keys, k1);
    }
    else {
      motion_curve_keys_linear(kg, sd->object, sd->time, k0, k1, P_curve);
    }

    r = (P_curve[1].w - P_curve[0].w) * sd->u + P_curve[0].w;
  }

  return r * 2.0f;
}

/* Curve random */

ccl_device float curve_random(KernelGlobals kg, ccl_private const ShaderData *sd)
{
  if (sd->type & PRIMITIVE_CURVE) {
    const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_CURVE_RANDOM);
    return (desc.offset != ATTR_STD_NOT_FOUND) ? curve_attribute_float(kg, sd, desc, NULL, NULL) :
                                                 0.0f;
  }
  return 0.0f;
}

/* Curve location for motion pass, linear interpolation between keys and
 * ignoring radius because we do the same for the motion keys */

ccl_device float3 curve_motion_center_location(KernelGlobals kg, ccl_private const ShaderData *sd)
{
  KernelCurve curve = kernel_data_fetch(curves, sd->prim);
  int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
  int k1 = k0 + 1;

  float4 P_curve[2];

  P_curve[0] = kernel_data_fetch(curve_keys, k0);
  P_curve[1] = kernel_data_fetch(curve_keys, k1);

  return float4_to_float3(P_curve[1]) * sd->u + float4_to_float3(P_curve[0]) * (1.0f - sd->u);
}

/* Curve tangent normal */

ccl_device float3 curve_tangent_normal(KernelGlobals kg, ccl_private const ShaderData *sd)
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
    float inv3p3 = (1.0f / 3.0f) / p3;
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

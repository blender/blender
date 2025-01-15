/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/motion_point.h"
#include "kernel/geom/object.h"

CCL_NAMESPACE_BEGIN

/* Point Primitive
 *
 * Point primitive for rendering point clouds.
 */

#ifdef __POINTCLOUD__

/* Reading attributes on various point elements */

ccl_device float point_attribute_float(KernelGlobals kg,
                                       const ccl_private ShaderData *sd,
                                       const AttributeDescriptor desc,
                                       ccl_private float *dx,
                                       ccl_private float *dy)
{
#  ifdef __RAY_DIFFERENTIALS__
  if (dx) {
    *dx = 0.0f;
  }
  if (dy) {
    *dy = 0.0f;
  }
#  endif

  if (desc.element == ATTR_ELEMENT_VERTEX) {
    return kernel_data_fetch(attributes_float, desc.offset + sd->prim);
  }
  return 0.0f;
}

ccl_device float2 point_attribute_float2(KernelGlobals kg,
                                         const ccl_private ShaderData *sd,
                                         const AttributeDescriptor desc,
                                         ccl_private float2 *dx,
                                         ccl_private float2 *dy)
{
#  ifdef __RAY_DIFFERENTIALS__
  if (dx) {
    *dx = make_float2(0.0f, 0.0f);
  }
  if (dy) {
    *dy = make_float2(0.0f, 0.0f);
  }
#  endif

  if (desc.element == ATTR_ELEMENT_VERTEX) {
    return kernel_data_fetch(attributes_float2, desc.offset + sd->prim);
  }
  return make_float2(0.0f, 0.0f);
}

ccl_device float3 point_attribute_float3(KernelGlobals kg,
                                         const ccl_private ShaderData *sd,
                                         const AttributeDescriptor desc,
                                         ccl_private float3 *dx,
                                         ccl_private float3 *dy)
{
#  ifdef __RAY_DIFFERENTIALS__
  if (dx) {
    *dx = make_float3(0.0f, 0.0f, 0.0f);
  }
  if (dy) {
    *dy = make_float3(0.0f, 0.0f, 0.0f);
  }
#  endif

  if (desc.element == ATTR_ELEMENT_VERTEX) {
    return kernel_data_fetch(attributes_float3, desc.offset + sd->prim);
  }
  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float4 point_attribute_float4(KernelGlobals kg,
                                         const ccl_private ShaderData *sd,
                                         const AttributeDescriptor desc,
                                         ccl_private float4 *dx,
                                         ccl_private float4 *dy)
{
#  ifdef __RAY_DIFFERENTIALS__
  if (dx) {
    *dx = zero_float4();
  }
  if (dy) {
    *dy = zero_float4();
  }
#  endif

  if (desc.element == ATTR_ELEMENT_VERTEX) {
    return kernel_data_fetch(attributes_float4, desc.offset + sd->prim);
  }
  return zero_float4();
}

/* Point position */

ccl_device float3 point_position(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  if (sd->type & PRIMITIVE_POINT) {
    /* World space center. */
    float3 P = (sd->type & PRIMITIVE_MOTION) ?
                   make_float3(motion_point(kg, sd->object, sd->prim, sd->time)) :
                   make_float3(kernel_data_fetch(points, sd->prim));

    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      object_position_transform(kg, sd, &P);
    }

    return P;
  }

  return zero_float3();
}

/* Point radius */

ccl_device float point_radius(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  if (sd->type & PRIMITIVE_POINT) {
    /* World space radius. */
    const float r = kernel_data_fetch(points, sd->prim).w;

    if (sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED) {
      return r;
    }

    const float normalized_r = r * (1.0f / M_SQRT3_F);
    float3 dir = make_float3(normalized_r, normalized_r, normalized_r);
    object_dir_transform(kg, sd, &dir);
    return len(dir);
  }

  return 0.0f;
}

/* Point random */

ccl_device float point_random(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  if (sd->type & PRIMITIVE_POINT) {
    const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POINT_RANDOM);
    return (desc.offset != ATTR_STD_NOT_FOUND) ?
               point_attribute_float(kg, sd, desc, nullptr, nullptr) :
               0.0f;
  }
  return 0.0f;
}

/* Point location for motion pass, linear interpolation between keys and
 * ignoring radius because we do the same for the motion keys */

ccl_device float3 point_motion_center_location(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  return make_float3(kernel_data_fetch(points, sd->prim));
}

#endif /* __POINTCLOUD__ */

CCL_NAMESPACE_END

/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/geom/motion_point.h"
#include "kernel/geom/object.h"

CCL_NAMESPACE_BEGIN

/* Point primitive intersection functions. */

#ifdef __POINTCLOUD__

ccl_device_forceinline bool point_intersect_test(const float4 point,
                                                 const float3 ray_P,
                                                 const float3 ray_D,
                                                 const float ray_tmin,
                                                 const float ray_tmax,
                                                 ccl_private float *t)
{
  const float3 center = make_float3(point);
  const float radius = point.w;

  const float rd2 = 1.0f / dot(ray_D, ray_D);

  const float3 c0 = center - ray_P;
  const float projC0 = dot(c0, ray_D) * rd2;
  const float3 perp = c0 - projC0 * ray_D;
  const float l2 = dot(perp, perp);
  const float r2 = radius * radius;
  if (!(l2 <= r2)) {
    return false;
  }

  const float td = sqrt((r2 - l2) * rd2);
  const float t_front = projC0 - td;
  const bool valid_front = (ray_tmin <= t_front) & (t_front <= ray_tmax);

  /* Always back-face culling for now. */
#  if 0
  const float t_back = projC0 + td;
  const bool valid_back = (ray_tmin <= t_back) & (t_back <= ray_tmax);

  /* check if there is a first hit */
  const bool valid_first = valid_front | valid_back;
  if (!valid_first) {
    return false;
  }

  *t = (valid_front) ? t_front : t_back;
  return true;
#  else
  if (!valid_front) {
    return false;
  }
  *t = t_front;
  return true;
#  endif
}

ccl_device_forceinline bool point_intersect(KernelGlobals kg,
                                            ccl_private Intersection *isect,
                                            const float3 ray_P,
                                            const float3 ray_D,
                                            const float ray_tmin,
                                            const float ray_tmax,
                                            const int object,
                                            const int prim,
                                            const float time,
                                            const int type)
{
  const float4 point = (type & PRIMITIVE_MOTION) ? motion_point(kg, object, prim, time) :
                                                   kernel_data_fetch(points, prim);

  if (!point_intersect_test(point, ray_P, ray_D, ray_tmin, ray_tmax, &isect->t)) {
    return false;
  }

  isect->prim = prim;
  isect->object = object;
  isect->type = type;
  isect->u = 0.0f;
  isect->v = 0.0f;
  return true;
}

ccl_device_inline void point_shader_setup(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          const ccl_private Intersection *isect,
                                          const ccl_private Ray *ray)
{
  sd->shader = kernel_data_fetch(points_shader, isect->prim);
  sd->P = ray->P + ray->D * isect->t;

  /* Texture coordinates, zero for now. */
#  ifdef __UV__
  sd->u = isect->u;
  sd->v = isect->v;
#  endif

  /* Compute point center for normal. */
  float3 center = make_float3((isect->type & PRIMITIVE_MOTION) ?
                                  motion_point(kg, sd->object, sd->prim, sd->time) :
                                  kernel_data_fetch(points, sd->prim));
  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    object_position_transform_auto(kg, sd, &center);
  }

  /* Normal */
  sd->Ng = normalize(sd->P - center);
  sd->N = sd->Ng;

#  ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
  sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
#  endif
}

#endif

CCL_NAMESPACE_END

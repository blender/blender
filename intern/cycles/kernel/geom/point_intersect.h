/*
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
 *
 * Based on Embree code, copyright 2009-2020 Intel Corporation.
 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Point primitive intersection functions. */

#ifdef __POINTCLOUD__

ccl_device_forceinline bool point_intersect_test(
    const float4 point, const float3 P, const float3 dir, const float tmax, ccl_private float *t)
{
  const float3 center = float4_to_float3(point);
  const float radius = point.w;

  const float rd2 = 1.0f / dot(dir, dir);

  const float3 c0 = center - P;
  const float projC0 = dot(c0, dir) * rd2;
  const float3 perp = c0 - projC0 * dir;
  const float l2 = dot(perp, perp);
  const float r2 = radius * radius;
  if (!(l2 <= r2)) {
    return false;
  }

  const float td = sqrt((r2 - l2) * rd2);
  const float t_front = projC0 - td;
  const bool valid_front = (0.0f <= t_front) & (t_front <= tmax);

  /* Always back-face culling for now. */
#  if 0
  const float t_back = projC0 + td;
  const bool valid_back = (0.0f <= t_back) & (t_back <= tmax);

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
                                            const float3 P,
                                            const float3 dir,
                                            const float tmax,
                                            const int object,
                                            const int prim,
                                            const float time,
                                            const int type)
{
  const float4 point = (type & PRIMITIVE_MOTION) ? motion_point(kg, object, prim, time) :
                                                   kernel_tex_fetch(__points, prim);

  if (!point_intersect_test(point, P, dir, tmax, &isect->t)) {
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
                                          ccl_private const Intersection *isect,
                                          ccl_private const Ray *ray)
{
  sd->shader = kernel_tex_fetch(__points_shader, isect->prim);
  sd->P = ray->P + ray->D * isect->t;

  /* Texture coordinates, zero for now. */
#  ifdef __UV__
  sd->u = isect->u;
  sd->v = isect->v;
#  endif

  /* Compute point center for normal. */
  float3 center = float4_to_float3((isect->type & PRIMITIVE_MOTION) ?
                                       motion_point(kg, sd->object, sd->prim, sd->time) :
                                       kernel_tex_fetch(__points, sd->prim));
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

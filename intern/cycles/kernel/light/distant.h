/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/geom/geom.h"

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline bool distant_light_sample(const ccl_global KernelLight *klight,
                                            const float randu,
                                            const float randv,
                                            ccl_private LightSample *ls)
{
  /* distant light */
  float3 lightD = klight->co;
  float3 D = lightD;
  float radius = klight->distant.radius;
  float invarea = klight->distant.invarea;

  if (radius > 0.0f) {
    D = normalize(D + disk_light_sample(D, randu, randv) * radius);
  }

  ls->P = D;
  ls->Ng = D;
  ls->D = -D;
  ls->t = FLT_MAX;

  float costheta = dot(lightD, D);
  ls->pdf = invarea / (costheta * costheta * costheta);
  ls->eval_fac = ls->pdf;

  return true;
}

/* Special intersection check.
 * Returns true if the distant_light_sample_from_intersection() for this light would return true.
 *
 * The intersection parameters t, u, v are optimized for the shadow ray towards a dedicated light:
 * u = v = 0, t = FLT_MAX.
 */
ccl_device bool distant_light_intersect(const ccl_global KernelLight *klight,
                                        const ccl_private Ray *ccl_restrict ray,
                                        ccl_private float *t,
                                        ccl_private float *u,
                                        ccl_private float *v)
{
  kernel_assert(klight->type == LIGHT_DISTANT);

  if (klight->distant.radius == 0.0f) {
    return false;
  }

  const float3 lightD = klight->co;
  const float costheta = dot(-lightD, ray->D);
  const float cosangle = klight->distant.cosangle;

  if (costheta < cosangle) {
    return false;
  }

  *t = FLT_MAX;
  *u = 0.0f;
  *v = 0.0f;

  return true;
}

ccl_device bool distant_light_sample_from_intersection(KernelGlobals kg,
                                                       const float3 ray_D,
                                                       const int lamp,
                                                       ccl_private LightSample *ccl_restrict ls)
{
  ccl_global const KernelLight *klight = &kernel_data_fetch(lights, lamp);
  const int shader = klight->shader_id;
  const float radius = klight->distant.radius;
  const LightType type = (LightType)klight->type;

  if (type != LIGHT_DISTANT) {
    return false;
  }
  if (!(shader & SHADER_USE_MIS)) {
    return false;
  }
  if (radius == 0.0f) {
    return false;
  }

  /* a distant light is infinitely far away, but equivalent to a disk
   * shaped light exactly 1 unit away from the current shading point.
   *
   *     radius              t^2/cos(theta)
   *  <---------->           t = sqrt(1^2 + tan(theta)^2)
   *       tan(th)           area = radius*radius*pi
   *       <----->
   *        \    |           (1 + tan(theta)^2)/cos(theta)
   *         \   |           (1 + tan(acos(cos(theta)))^2)/cos(theta)
   *       t  \th| 1         simplifies to
   *           \-|           1/(cos(theta)^3)
   *            \|           magic!
   *             P
   */

  float3 lightD = klight->co;
  float costheta = dot(-lightD, ray_D);
  float cosangle = klight->distant.cosangle;

  /* Workaround to prevent a hang in the classroom scene with AMD HIP drivers 22.10,
   * Remove when a compiler fix is available. */
#ifdef __HIP__
  ls->shader = klight->shader_id;
#endif

  if (costheta < cosangle)
    return false;

  ls->type = type;
#ifndef __HIP__
  ls->shader = klight->shader_id;
#endif
  ls->object = PRIM_NONE;
  ls->prim = PRIM_NONE;
  ls->lamp = lamp;
  /* todo: missing texture coordinates */
  ls->u = 0.0f;
  ls->v = 0.0f;
  ls->t = FLT_MAX;
  ls->P = -ray_D;
  ls->Ng = -ray_D;
  ls->D = ray_D;
  ls->group = lamp_lightgroup(kg, lamp);

  /* compute pdf */
  float invarea = klight->distant.invarea;
  ls->pdf = invarea / (costheta * costheta * costheta);
  ls->eval_fac = ls->pdf;

  return true;
}

ccl_device_forceinline bool distant_light_tree_parameters(const float3 centroid,
                                                          const float theta_e,
                                                          ccl_private float &cos_theta_u,
                                                          ccl_private float2 &distance,
                                                          ccl_private float3 &point_to_centroid)
{
  /* Treating it as a disk light 1 unit away */
  cos_theta_u = fast_cosf(theta_e);

  distance = make_float2(1.0f / cos_theta_u, 1.0f);

  point_to_centroid = -centroid;

  return true;
}

CCL_NAMESPACE_END

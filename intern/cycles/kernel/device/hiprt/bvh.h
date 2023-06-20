/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/device/hiprt/common.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline bool scene_intersect_valid(ccl_private const Ray *ray)
{
  return isfinite_safe(ray->P.x) && isfinite_safe(ray->D.x) && len_squared(ray->D) != 0.0f;
}

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          ccl_private const Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  isect->t = ray->tmax;
  isect->u = 0.0f;
  isect->v = 0.0f;
  isect->prim = PRIM_NONE;
  isect->object = OBJECT_NONE;
  isect->type = PRIMITIVE_NONE;
  if (!scene_intersect_valid(ray)) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;
    return false;
  }

  hiprtRay ray_hip;

  SET_HIPRT_RAY(ray_hip, ray)

  RayPayload payload;
  payload.self = ray->self;
  payload.kg = kg;
  payload.visibility = visibility;
  payload.prim_type = PRIMITIVE_NONE;
  payload.ray_time = ray->time;

  hiprtHit hit = {};

  GET_TRAVERSAL_STACK()

  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    GET_TRAVERSAL_ANY_HIT(table_closest_intersect, 0, ray->time)
    hit = traversal.getNextHit();
  }
  else {
    GET_TRAVERSAL_CLOSEST_HIT(table_closest_intersect, 0, ray->time)
    hit = traversal.getNextHit();
  }
  if (hit.hasHit()) {
    set_intersect_point(kg, hit, isect);
    if (isect->type > 1) {  // should be applied only for curves
      isect->type = payload.prim_type;
      isect->prim = hit.primID;
    }
    return true;
  }
  return false;
}

#ifdef __BVH_LOCAL__
ccl_device_intersect bool scene_intersect_local(KernelGlobals kg,
                                                ccl_private const Ray *ray,
                                                ccl_private LocalIntersection *local_isect,
                                                int local_object,
                                                ccl_private uint *lcg_state,
                                                int max_hits)
{
  if (!scene_intersect_valid(ray)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    return false;
  }

  float3 P = ray->P;
  float3 dir = bvh_clamp_direction(ray->D);
  float3 idir = bvh_inverse_direction(dir);

  if (local_isect != NULL) {
    local_isect->num_hits = 0;
  }

  const int object_flag = kernel_data_fetch(object_flag, local_object);
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {

#  if BVH_FEATURE(BVH_MOTION)
    bvh_instance_motion_push(kg, local_object, ray, &P, &dir, &idir);
#  else
    bvh_instance_push(kg, local_object, ray, &P, &dir, &idir);
#  endif
  }

  hiprtRay ray_hip;
  ray_hip.origin = P;
  ray_hip.direction = dir;
  ray_hip.maxT = ray->tmax;
  ray_hip.minT = ray->tmin;

  LocalPayload payload = {0};
  payload.kg = kg;
  payload.self = ray->self;
  payload.local_object = local_object;
  payload.max_hits = max_hits;
  payload.lcg_state = lcg_state;
  payload.local_isect = local_isect;

  GET_TRAVERSAL_STACK()

  void *local_geom = (void *)(kernel_data_fetch(blas_ptr, local_object));
  // we don't need custom intersection functions for SSR
#  ifdef HIPRT_SHARED_STACK
  hiprtGeomTraversalAnyHitCustomStack<Stack> traversal(local_geom,
                                                       ray_hip,
                                                       stack,
                                                       hiprtTraversalHintDefault,
                                                       &payload,
                                                       kernel_params.table_local_intersect,
                                                       2);
#  else
  hiprtGeomTraversalAnyHit traversal(
      local_geom, ray_hip, table, hiprtTraversalHintDefault, &payload);
#  endif
  hiprtHit hit = traversal.getNextHit();
  return hit.hasHit();
}
#endif  //__BVH_LOCAL__

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals kg,
                                                     IntegratorShadowState state,
                                                     ccl_private const Ray *ray,
                                                     uint visibility,
                                                     uint max_hits,
                                                     ccl_private uint *num_recorded_hits,
                                                     ccl_private float *throughput)
{
  *throughput = 1.0f;
  *num_recorded_hits = 0;

  if (!scene_intersect_valid(ray)) {
    return false;
  }

  hiprtRay ray_hip;

  SET_HIPRT_RAY(ray_hip, ray)
  ShadowPayload payload;
  payload.kg = kg;
  payload.self = ray->self;
  payload.in_state = state;
  payload.max_hits = max_hits;
  payload.visibility = visibility;
  payload.prim_type = PRIMITIVE_NONE;
  payload.ray_time = ray->time;
  payload.num_hits = 0;
  payload.r_num_recorded_hits = num_recorded_hits;
  payload.r_throughput = throughput;
  GET_TRAVERSAL_STACK()
  GET_TRAVERSAL_ANY_HIT(table_shadow_intersect, 1, ray->time)
  hiprtHit hit = traversal.getNextHit();
  num_recorded_hits = payload.r_num_recorded_hits;
  throughput = payload.r_throughput;
  return hit.hasHit();
}
#endif /* __SHADOW_RECORD_ALL__ */

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 ccl_private const Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  isect->t = ray->tmax;
  isect->u = 0.0f;
  isect->v = 0.0f;
  isect->prim = PRIM_NONE;
  isect->object = OBJECT_NONE;
  isect->type = PRIMITIVE_NONE;

  if (!scene_intersect_valid(ray)) {
    return false;
  }

  hiprtRay ray_hip;

  SET_HIPRT_RAY(ray_hip, ray)

  RayPayload payload;
  payload.self = ray->self;
  payload.kg = kg;
  payload.visibility = visibility;
  payload.prim_type = PRIMITIVE_NONE;
  payload.ray_time = ray->time;

  GET_TRAVERSAL_STACK()

  GET_TRAVERSAL_CLOSEST_HIT(table_volume_intersect, 3, ray->time)
  hiprtHit hit = traversal.getNextHit();
  // return hit.hasHit();
  if (hit.hasHit()) {
    set_intersect_point(kg, hit, isect);
    if (isect->type > 1) {  // should be applied only for curves
      isect->type = payload.prim_type;
      isect->prim = hit.primID;
    }
    return true;
  }
  else
    return false;
}
#endif /* __VOLUME__ */

CCL_NAMESPACE_END

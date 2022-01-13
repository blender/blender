/*
 * Copyright 2018, Blender Foundation.
 *
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
 */

#pragma once

#include <embree3/rtcore_ray.h>
#include <embree3/rtcore_scene.h>

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/bvh/util.h"

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

struct CCLIntersectContext {
  typedef enum {
    RAY_REGULAR = 0,
    RAY_SHADOW_ALL = 1,
    RAY_LOCAL = 2,
    RAY_SSS = 3,
    RAY_VOLUME_ALL = 4,
  } RayType;

  KernelGlobals kg;
  RayType type;

  /* For avoiding self intersections */
  const Ray *ray;

  /* for shadow rays */
  Intersection *isect_s;
  uint max_hits;
  uint num_hits;
  uint num_recorded_hits;
  float throughput;
  float max_t;
  bool opaque_hit;

  /* for SSS Rays: */
  LocalIntersection *local_isect;
  int local_object_id;
  uint *lcg_state;

  CCLIntersectContext(KernelGlobals kg_, RayType type_)
  {
    kg = kg_;
    type = type_;
    ray = NULL;
    max_hits = 1;
    num_hits = 0;
    num_recorded_hits = 0;
    throughput = 1.0f;
    max_t = FLT_MAX;
    opaque_hit = false;
    isect_s = NULL;
    local_isect = NULL;
    local_object_id = -1;
    lcg_state = NULL;
  }
};

class IntersectContext {
 public:
  IntersectContext(CCLIntersectContext *ctx)
  {
    rtcInitIntersectContext(&context);
    userRayExt = ctx;
  }
  RTCIntersectContext context;
  CCLIntersectContext *userRayExt;
};

ccl_device_inline void kernel_embree_setup_ray(const Ray &ray,
                                               RTCRay &rtc_ray,
                                               const uint visibility)
{
  rtc_ray.org_x = ray.P.x;
  rtc_ray.org_y = ray.P.y;
  rtc_ray.org_z = ray.P.z;
  rtc_ray.dir_x = ray.D.x;
  rtc_ray.dir_y = ray.D.y;
  rtc_ray.dir_z = ray.D.z;
  rtc_ray.tnear = 0.0f;
  rtc_ray.tfar = ray.t;
  rtc_ray.time = ray.time;
  rtc_ray.mask = visibility;
}

ccl_device_inline void kernel_embree_setup_rayhit(const Ray &ray,
                                                  RTCRayHit &rayhit,
                                                  const uint visibility)
{
  kernel_embree_setup_ray(ray, rayhit.ray, visibility);
  rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
  rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
}

ccl_device_inline bool kernel_embree_is_self_intersection(const KernelGlobals kg,
                                                          const RTCHit *hit,
                                                          const Ray *ray)
{
  bool status = false;
  if (hit->instID[0] != RTC_INVALID_GEOMETRY_ID) {
    const int oID = hit->instID[0] / 2;
    if ((ray->self.object == oID) || (ray->self.light_object == oID)) {
      RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
          rtcGetGeometry(kernel_data.bvh.scene, hit->instID[0]));
      const int pID = hit->primID +
                      (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(inst_scene, hit->geomID));
      status = intersection_skip_self_shadow(ray->self, oID, pID);
    }
  }
  else {
    const int oID = hit->geomID / 2;
    if ((ray->self.object == oID) || (ray->self.light_object == oID)) {
      const int pID = hit->primID + (intptr_t)rtcGetGeometryUserData(
                                        rtcGetGeometry(kernel_data.bvh.scene, hit->geomID));
      status = intersection_skip_self_shadow(ray->self, oID, pID);
    }
  }

  return status;
}

ccl_device_inline void kernel_embree_convert_hit(KernelGlobals kg,
                                                 const RTCRay *ray,
                                                 const RTCHit *hit,
                                                 Intersection *isect)
{
  isect->t = ray->tfar;
  if (hit->instID[0] != RTC_INVALID_GEOMETRY_ID) {
    RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
        rtcGetGeometry(kernel_data.bvh.scene, hit->instID[0]));
    isect->prim = hit->primID +
                  (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(inst_scene, hit->geomID));
    isect->object = hit->instID[0] / 2;
  }
  else {
    isect->prim = hit->primID + (intptr_t)rtcGetGeometryUserData(
                                    rtcGetGeometry(kernel_data.bvh.scene, hit->geomID));
    isect->object = hit->geomID / 2;
  }

  const bool is_hair = hit->geomID & 1;
  if (is_hair) {
    const KernelCurveSegment segment = kernel_tex_fetch(__curve_segments, isect->prim);
    isect->type = segment.type;
    isect->prim = segment.prim;
    isect->u = hit->u;
    isect->v = hit->v;
  }
  else {
    isect->type = kernel_tex_fetch(__objects, isect->object).primitive_type;
    isect->u = 1.0f - hit->v - hit->u;
    isect->v = hit->u;
  }
}

ccl_device_inline void kernel_embree_convert_sss_hit(
    KernelGlobals kg, const RTCRay *ray, const RTCHit *hit, Intersection *isect, int object)
{
  isect->u = 1.0f - hit->v - hit->u;
  isect->v = hit->u;
  isect->t = ray->tfar;
  RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
      rtcGetGeometry(kernel_data.bvh.scene, object * 2));
  isect->prim = hit->primID +
                (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(inst_scene, hit->geomID));
  isect->object = object;
  isect->type = kernel_tex_fetch(__objects, object).primitive_type;
}

CCL_NAMESPACE_END

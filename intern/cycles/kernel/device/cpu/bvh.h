/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

/* CPU Embree implementation of ray-scene intersection. */

#pragma once

#include <embree3/rtcore_ray.h>
#include <embree3/rtcore_scene.h>

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"
#include "kernel/geom/object.h"
#include "kernel/integrator/state.h"
#include "kernel/sample/lcg.h"

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

#define EMBREE_IS_HAIR(x) (x & 1)

/* Intersection context. */

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

/* Utilities. */

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
  rtc_ray.tnear = ray.tmin;
  rtc_ray.tfar = ray.tmax;
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
  int object, prim;

  if (hit->instID[0] != RTC_INVALID_GEOMETRY_ID) {
    object = hit->instID[0] / 2;
    if ((ray->self.object == object) || (ray->self.light_object == object)) {
      RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
          rtcGetGeometry(kernel_data.device_bvh, hit->instID[0]));
      prim = hit->primID +
             (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(inst_scene, hit->geomID));
    }
    else {
      return false;
    }
  }
  else {
    object = hit->geomID / 2;
    if ((ray->self.object == object) || (ray->self.light_object == object)) {
      prim = hit->primID +
             (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(kernel_data.device_bvh, hit->geomID));
    }
    else {
      return false;
    }
  }

  const bool is_hair = hit->geomID & 1;
  if (is_hair) {
    prim = kernel_data_fetch(curve_segments, prim).prim;
  }

  return intersection_skip_self_shadow(ray->self, object, prim);
}

ccl_device_inline void kernel_embree_convert_hit(KernelGlobals kg,
                                                 const RTCRay *ray,
                                                 const RTCHit *hit,
                                                 Intersection *isect)
{
  isect->t = ray->tfar;
  if (hit->instID[0] != RTC_INVALID_GEOMETRY_ID) {
    RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
        rtcGetGeometry(kernel_data.device_bvh, hit->instID[0]));
    isect->prim = hit->primID +
                  (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(inst_scene, hit->geomID));
    isect->object = hit->instID[0] / 2;
  }
  else {
    isect->prim = hit->primID + (intptr_t)rtcGetGeometryUserData(
                                    rtcGetGeometry(kernel_data.device_bvh, hit->geomID));
    isect->object = hit->geomID / 2;
  }

  const bool is_hair = hit->geomID & 1;
  if (is_hair) {
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, isect->prim);
    isect->type = segment.type;
    isect->prim = segment.prim;
    isect->u = hit->u;
    isect->v = hit->v;
  }
  else {
    isect->type = kernel_data_fetch(objects, isect->object).primitive_type;
    isect->u = hit->u;
    isect->v = hit->v;
  }
}

ccl_device_inline void kernel_embree_convert_sss_hit(
    KernelGlobals kg, const RTCRay *ray, const RTCHit *hit, Intersection *isect, int object)
{
  isect->u = hit->u;
  isect->v = hit->v;
  isect->t = ray->tfar;
  RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
      rtcGetGeometry(kernel_data.device_bvh, object * 2));
  isect->prim = hit->primID +
                (intptr_t)rtcGetGeometryUserData(rtcGetGeometry(inst_scene, hit->geomID));
  isect->object = object;
  isect->type = kernel_data_fetch(objects, object).primitive_type;
}

/* Ray filter functions. */

/* This gets called by Embree at every valid ray/object intersection.
 * Things like recording subsurface or shadow hits for later evaluation
 * as well as filtering for volume objects happen here.
 * Cycles' own BVH does that directly inside the traversal calls. */
ccl_device void kernel_embree_filter_intersection_func(const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  RTCHit *hit = (RTCHit *)args->hit;
  CCLIntersectContext *ctx = ((IntersectContext *)args->context)->userRayExt;
  const KernelGlobalsCPU *kg = ctx->kg;
  const Ray *cray = ctx->ray;

  if (kernel_embree_is_self_intersection(kg, hit, cray)) {
    *args->valid = 0;
  }
}

/* This gets called by Embree at every valid ray/object intersection.
 * Things like recording subsurface or shadow hits for later evaluation
 * as well as filtering for volume objects happen here.
 * Cycles' own BVH does that directly inside the traversal calls.
 */
ccl_device void kernel_embree_filter_occluded_func(const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;
  CCLIntersectContext *ctx = ((IntersectContext *)args->context)->userRayExt;
  const KernelGlobalsCPU *kg = ctx->kg;
  const Ray *cray = ctx->ray;

  switch (ctx->type) {
    case CCLIntersectContext::RAY_SHADOW_ALL: {
      Intersection current_isect;
      kernel_embree_convert_hit(kg, ray, hit, &current_isect);
      if (intersection_skip_self_shadow(cray->self, current_isect.object, current_isect.prim)) {
        *args->valid = 0;
        return;
      }
      /* If no transparent shadows or max number of hits exceeded, all light is blocked. */
      const int flags = intersection_get_shader_flags(kg, current_isect.prim, current_isect.type);
      if (!(flags & (SD_HAS_TRANSPARENT_SHADOW)) || ctx->num_hits >= ctx->max_hits) {
        ctx->opaque_hit = true;
        return;
      }

      ++ctx->num_hits;

      /* Always use baked shadow transparency for curves. */
      if (current_isect.type & PRIMITIVE_CURVE) {
        ctx->throughput *= intersection_curve_shadow_transparency(
            kg, current_isect.object, current_isect.prim, current_isect.u);

        if (ctx->throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
          ctx->opaque_hit = true;
          return;
        }
        else {
          *args->valid = 0;
          return;
        }
      }

      /* Test if we need to record this transparent intersection. */
      const uint max_record_hits = min(ctx->max_hits, INTEGRATOR_SHADOW_ISECT_SIZE);
      if (ctx->num_recorded_hits < max_record_hits || ray->tfar < ctx->max_t) {
        /* If maximum number of hits was reached, replace the intersection with the
         * highest distance. We want to find the N closest intersections. */
        const uint num_recorded_hits = min(ctx->num_recorded_hits, max_record_hits);
        uint isect_index = num_recorded_hits;
        if (num_recorded_hits + 1 >= max_record_hits) {
          float max_t = ctx->isect_s[0].t;
          uint max_recorded_hit = 0;

          for (uint i = 1; i < num_recorded_hits; ++i) {
            if (ctx->isect_s[i].t > max_t) {
              max_recorded_hit = i;
              max_t = ctx->isect_s[i].t;
            }
          }

          if (num_recorded_hits >= max_record_hits) {
            isect_index = max_recorded_hit;
          }

          /* Limit the ray distance and stop counting hits beyond this.
           * TODO: is there some way we can tell Embree to stop intersecting beyond
           * this distance when max number of hits is reached?. Or maybe it will
           * become irrelevant if we make max_hits a very high number on the CPU. */
          ctx->max_t = max(current_isect.t, max_t);
        }

        ctx->isect_s[isect_index] = current_isect;
      }

      /* Always increase the number of recorded hits, even beyond the maximum,
       * so that we can detect this and trace another ray if needed. */
      ++ctx->num_recorded_hits;

      /* This tells Embree to continue tracing. */
      *args->valid = 0;
      break;
    }
    case CCLIntersectContext::RAY_LOCAL:
    case CCLIntersectContext::RAY_SSS: {
      /* Check if it's hitting the correct object. */
      Intersection current_isect;
      if (ctx->type == CCLIntersectContext::RAY_SSS) {
        kernel_embree_convert_sss_hit(kg, ray, hit, &current_isect, ctx->local_object_id);
      }
      else {
        kernel_embree_convert_hit(kg, ray, hit, &current_isect);
        if (ctx->local_object_id != current_isect.object) {
          /* This tells Embree to continue tracing. */
          *args->valid = 0;
          break;
        }
      }
      if (intersection_skip_self_local(cray->self, current_isect.prim)) {
        *args->valid = 0;
        return;
      }

      /* No intersection information requested, just return a hit. */
      if (ctx->max_hits == 0) {
        break;
      }

      /* Ignore curves. */
      if (EMBREE_IS_HAIR(hit->geomID)) {
        /* This tells Embree to continue tracing. */
        *args->valid = 0;
        break;
      }

      LocalIntersection *local_isect = ctx->local_isect;
      int hit_idx = 0;

      if (ctx->lcg_state) {
        /* See triangle_intersect_subsurface() for the native equivalent. */
        for (int i = min((int)ctx->max_hits, local_isect->num_hits) - 1; i >= 0; --i) {
          if (local_isect->hits[i].t == ray->tfar) {
            /* This tells Embree to continue tracing. */
            *args->valid = 0;
            return;
          }
        }

        local_isect->num_hits++;

        if (local_isect->num_hits <= ctx->max_hits) {
          hit_idx = local_isect->num_hits - 1;
        }
        else {
          /* reservoir sampling: if we are at the maximum number of
           * hits, randomly replace element or skip it */
          hit_idx = lcg_step_uint(ctx->lcg_state) % local_isect->num_hits;

          if (hit_idx >= ctx->max_hits) {
            /* This tells Embree to continue tracing. */
            *args->valid = 0;
            return;
          }
        }
      }
      else {
        /* Record closest intersection only. */
        if (local_isect->num_hits && current_isect.t > local_isect->hits[0].t) {
          *args->valid = 0;
          return;
        }

        local_isect->num_hits = 1;
      }

      /* record intersection */
      local_isect->hits[hit_idx] = current_isect;
      local_isect->Ng[hit_idx] = normalize(make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z));
      /* This tells Embree to continue tracing. */
      *args->valid = 0;
      break;
    }
    case CCLIntersectContext::RAY_VOLUME_ALL: {
      /* Append the intersection to the end of the array. */
      if (ctx->num_hits < ctx->max_hits) {
        Intersection current_isect;
        kernel_embree_convert_hit(kg, ray, hit, &current_isect);
        if (intersection_skip_self(cray->self, current_isect.object, current_isect.prim)) {
          *args->valid = 0;
          return;
        }

        Intersection *isect = &ctx->isect_s[ctx->num_hits];
        ++ctx->num_hits;
        *isect = current_isect;
        /* Only primitives from volume object. */
        uint tri_object = isect->object;
        int object_flag = kernel_data_fetch(object_flag, tri_object);
        if ((object_flag & SD_OBJECT_HAS_VOLUME) == 0) {
          --ctx->num_hits;
        }
        /* This tells Embree to continue tracing. */
        *args->valid = 0;
      }
      break;
    }
    case CCLIntersectContext::RAY_REGULAR:
    default:
      if (kernel_embree_is_self_intersection(kg, hit, cray)) {
        *args->valid = 0;
        return;
      }
      break;
  }
}

ccl_device void kernel_embree_filter_func_backface_cull(const RTCFilterFunctionNArguments *args)
{
  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;

  /* Always ignore back-facing intersections. */
  if (dot(make_float3(ray->dir_x, ray->dir_y, ray->dir_z),
          make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z)) > 0.0f) {
    *args->valid = 0;
    return;
  }

  CCLIntersectContext *ctx = ((IntersectContext *)args->context)->userRayExt;
  const KernelGlobalsCPU *kg = ctx->kg;
  const Ray *cray = ctx->ray;

  if (kernel_embree_is_self_intersection(kg, hit, cray)) {
    *args->valid = 0;
  }
}

ccl_device void kernel_embree_filter_occluded_func_backface_cull(
    const RTCFilterFunctionNArguments *args)
{
  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;

  /* Always ignore back-facing intersections. */
  if (dot(make_float3(ray->dir_x, ray->dir_y, ray->dir_z),
          make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z)) > 0.0f) {
    *args->valid = 0;
    return;
  }

  kernel_embree_filter_occluded_func(args);
}

/* Scene intersection. */

ccl_device_intersect bool kernel_embree_intersect(KernelGlobals kg,
                                                  ccl_private const Ray *ray,
                                                  const uint visibility,
                                                  ccl_private Intersection *isect)
{
  isect->t = ray->tmax;
  CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_REGULAR);
  IntersectContext rtc_ctx(&ctx);
  RTCRayHit ray_hit;
  ctx.ray = ray;
  kernel_embree_setup_rayhit(*ray, ray_hit, visibility);
  rtcIntersect1(kernel_data.device_bvh, &rtc_ctx.context, &ray_hit);
  if (ray_hit.hit.geomID == RTC_INVALID_GEOMETRY_ID ||
      ray_hit.hit.primID == RTC_INVALID_GEOMETRY_ID) {
    return false;
  }

  kernel_embree_convert_hit(kg, &ray_hit.ray, &ray_hit.hit, isect);
  return true;
}

#ifdef __BVH_LOCAL__
ccl_device_intersect bool kernel_embree_intersect_local(KernelGlobals kg,
                                                        ccl_private const Ray *ray,
                                                        ccl_private LocalIntersection *local_isect,
                                                        int local_object,
                                                        ccl_private uint *lcg_state,
                                                        int max_hits)
{
  const bool has_bvh = !(kernel_data_fetch(object_flag, local_object) &
                         SD_OBJECT_TRANSFORM_APPLIED);
  CCLIntersectContext ctx(kg,
                          has_bvh ? CCLIntersectContext::RAY_SSS : CCLIntersectContext::RAY_LOCAL);
  ctx.lcg_state = lcg_state;
  ctx.max_hits = max_hits;
  ctx.ray = ray;
  ctx.local_isect = local_isect;
  if (local_isect) {
    local_isect->num_hits = 0;
  }
  ctx.local_object_id = local_object;
  IntersectContext rtc_ctx(&ctx);
  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, PATH_RAY_ALL_VISIBILITY);

  /* If this object has its own BVH, use it. */
  if (has_bvh) {
    RTCGeometry geom = rtcGetGeometry(kernel_data.device_bvh, local_object * 2);
    if (geom) {
      float3 P = ray->P;
      float3 dir = ray->D;
      float3 idir = ray->D;
      bvh_instance_motion_push(kg, local_object, ray, &P, &dir, &idir);

      rtc_ray.org_x = P.x;
      rtc_ray.org_y = P.y;
      rtc_ray.org_z = P.z;
      rtc_ray.dir_x = dir.x;
      rtc_ray.dir_y = dir.y;
      rtc_ray.dir_z = dir.z;
      rtc_ray.tnear = ray->tmin;
      rtc_ray.tfar = ray->tmax;
      RTCScene scene = (RTCScene)rtcGetGeometryUserData(geom);
      kernel_assert(scene);
      if (scene) {
        rtcOccluded1(scene, &rtc_ctx.context, &rtc_ray);
      }
    }
  }
  else {
    rtcOccluded1(kernel_data.device_bvh, &rtc_ctx.context, &rtc_ray);
  }

  /* rtcOccluded1 sets tfar to -inf if a hit was found. */
  return (local_isect && local_isect->num_hits > 0) || (rtc_ray.tfar < 0);
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool kernel_embree_intersect_shadow_all(KernelGlobals kg,
                                                             IntegratorShadowStateCPU *state,
                                                             ccl_private const Ray *ray,
                                                             uint visibility,
                                                             uint max_hits,
                                                             ccl_private uint *num_recorded_hits,
                                                             ccl_private float *throughput)
{
  CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_SHADOW_ALL);
  Intersection *isect_array = (Intersection *)state->shadow_isect;
  ctx.isect_s = isect_array;
  ctx.max_hits = max_hits;
  ctx.ray = ray;
  IntersectContext rtc_ctx(&ctx);
  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, visibility);
  rtcOccluded1(kernel_data.device_bvh, &rtc_ctx.context, &rtc_ray);

  *num_recorded_hits = ctx.num_recorded_hits;
  *throughput = ctx.throughput;
  return ctx.opaque_hit;
}
#endif

#ifdef __VOLUME__
ccl_device_intersect uint kernel_embree_intersect_volume(KernelGlobals kg,
                                                         ccl_private const Ray *ray,
                                                         ccl_private Intersection *isect,
                                                         const uint max_hits,
                                                         const uint visibility)
{
  CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_VOLUME_ALL);
  ctx.isect_s = isect;
  ctx.max_hits = max_hits;
  ctx.num_hits = 0;
  ctx.ray = ray;
  IntersectContext rtc_ctx(&ctx);
  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, visibility);
  rtcOccluded1(kernel_data.device_bvh, &rtc_ctx.context, &rtc_ray);
  return ctx.num_hits;
}
#endif

CCL_NAMESPACE_END

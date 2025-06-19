/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* CPU Embree implementation of ray-scene intersection. */

#pragma once

#include <embree4/rtcore_geometry.h>
#include <embree4/rtcore_ray.h>
#include <embree4/rtcore_scene.h>

#ifdef __KERNEL_ONEAPI__
#  include "kernel/device/oneapi/compat.h"
#  include "kernel/device/oneapi/globals.h"
#else
#  include "kernel/device/cpu/compat.h"
#  include "kernel/device/cpu/globals.h"
#endif

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"
#include "kernel/geom/object.h"
#include "kernel/integrator/state.h"
#include "kernel/integrator/state_util.h"
#include "kernel/sample/lcg.h"

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_ONEAPI__
using numhit_t = uint16_t;
#else
using numhit_t = uint32_t;
#endif

/* Before Embree 4.4, the so-called Traversable functionality was exposed through Scene API.
 * So, in order to simplify code between different versions, we are defining the traversable class
 * and calls for older Embree versions as well. */
#if RTC_VERSION < 40400
#  define RTCTraversable RTCScene
#  define rtcGetGeometryUserDataFromTraversable rtcGetGeometryUserDataFromScene
#  define rtcTraversableIntersect1 rtcIntersect1
#  define rtcTraversableOccluded1 rtcOccluded1
#endif

#ifdef __KERNEL_ONEAPI__
#  define CYCLES_EMBREE_USED_FEATURES \
    (kernel_handler.get_specialization_constant<oneapi_embree_features>())
#else
#  define CYCLES_EMBREE_USED_FEATURES \
    (RTCFeatureFlags)(RTC_FEATURE_FLAG_TRIANGLE | RTC_FEATURE_FLAG_INSTANCE | \
                      RTC_FEATURE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS | RTC_FEATURE_FLAG_POINT | \
                      RTC_FEATURE_FLAG_MOTION_BLUR | RTC_FEATURE_FLAG_ROUND_CATMULL_ROM_CURVE | \
                      RTC_FEATURE_FLAG_FLAT_CATMULL_ROM_CURVE)
#endif

#define EMBREE_IS_HAIR(x) (x & 1)

/* Intersection context. */

struct CCLFirstHitContext : public RTCRayQueryContext {
  KernelGlobals kg;
  /* For avoiding self intersections */
  const Ray *ray;
};

struct CCLShadowContext : public RTCRayQueryContext {
  KernelGlobals kg;
  const Ray *ray;
  IntegratorShadowState isect_s;
  float throughput;
  float max_t;
  bool opaque_hit;
  numhit_t max_transparent_hits;
  numhit_t num_transparent_hits;
  numhit_t num_recorded_hits;
};

struct CCLLocalContext : public RTCRayQueryContext {
  KernelGlobals kg;
  const Ray *ray;
  numhit_t max_hits;
  int local_object_id;
  LocalIntersection *local_isect;
  uint *lcg_state;
  bool is_sss;
};

struct CCLVolumeContext : public RTCRayQueryContext {
  KernelGlobals kg;
  const Ray *ray;
#ifdef __VOLUME_RECORD_ALL__
  numhit_t max_hits;
#endif
  numhit_t num_hits;
  Intersection *vol_isect;
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

ccl_device_inline int kernel_embree_get_hit_object(const RTCHit *hit)
{
  return (hit->instID[0] != RTC_INVALID_GEOMETRY_ID ? hit->instID[0] : hit->geomID) / 2;
}

ccl_device_inline bool kernel_embree_is_self_intersection(const KernelGlobals kg,
                                                          const RTCHit *hit,
                                                          const Ray *ray,
                                                          const intptr_t prim_offset)
{
  const int object = kernel_embree_get_hit_object(hit);

  int prim;
  if ((ray->self.object == object) || (ray->self.light_object == object)) {
    prim = hit->primID + prim_offset;
  }
  else {
    return false;
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
                                                 Intersection *isect,
                                                 const intptr_t prim_offset)
{
  isect->t = ray->tfar;
  isect->prim = hit->primID + prim_offset;
  isect->object = kernel_embree_get_hit_object(hit);

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

ccl_device_inline void kernel_embree_convert_hit(KernelGlobals kg,
                                                 const RTCRay *ray,
                                                 const RTCHit *hit,
                                                 Intersection *isect)
{
  intptr_t prim_offset;
  if (hit->instID[0] != RTC_INVALID_GEOMETRY_ID) {
    RTCTraversable inst_scene = (RTCTraversable)rtcGetGeometryUserDataFromTraversable(
        kernel_data.device_bvh, hit->instID[0]);
    prim_offset = intptr_t(rtcGetGeometryUserDataFromTraversable(inst_scene, hit->geomID));
  }
  else {
    prim_offset = intptr_t(
        rtcGetGeometryUserDataFromTraversable(kernel_data.device_bvh, hit->geomID));
  }
  kernel_embree_convert_hit(kg, ray, hit, isect, prim_offset);
}

ccl_device_inline void kernel_embree_convert_sss_hit(KernelGlobals kg,
                                                     const RTCRay *ray,
                                                     const RTCHit *hit,
                                                     Intersection *isect,
                                                     const int object,
                                                     const intptr_t prim_offset)
{
  isect->u = hit->u;
  isect->v = hit->v;
  isect->t = ray->tfar;
  isect->prim = hit->primID + prim_offset;
  isect->object = object;
  isect->type = kernel_data_fetch(objects, object).primitive_type;
}

/* Ray filter functions. */

/* This gets called by Embree at every valid ray/object intersection.
 * Things like recording subsurface or shadow hits for later evaluation
 * as well as filtering for volume objects happen here.
 * Cycles' own BVH does that directly inside the traversal calls. */
ccl_device_forceinline void kernel_embree_filter_intersection_func_impl(
    const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  RTCHit *hit = (RTCHit *)args->hit;
  CCLFirstHitContext *ctx = (CCLFirstHitContext *)(args->context);
#ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#endif
  const Ray *cray = ctx->ray;

  if (kernel_embree_is_self_intersection(
          kg, hit, cray, reinterpret_cast<intptr_t>(args->geometryUserPtr)))
  {
    *args->valid = 0;
    return;
  }

#ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, cray->self, kernel_embree_get_hit_object(hit))) {
    *args->valid = 0;
    return;
  }
#endif
}

/* This gets called by Embree at every valid ray/object intersection.
 * Things like recording subsurface or shadow hits for later evaluation
 * as well as filtering for volume objects happen here.
 * Cycles' own BVH does that directly inside the traversal calls.
 */
ccl_device_forceinline void kernel_embree_filter_occluded_shadow_all_func_impl(
    const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;
  CCLShadowContext *ctx = (CCLShadowContext *)(args->context);
#ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#endif
  const Ray *cray = ctx->ray;

  Intersection current_isect;
  kernel_embree_convert_hit(
      kg, ray, hit, &current_isect, reinterpret_cast<intptr_t>(args->geometryUserPtr));
  if (intersection_skip_self_shadow(cray->self, current_isect.object, current_isect.prim)) {
    *args->valid = 0;
    return;
  }

#ifdef __SHADOW_LINKING__
  if (intersection_skip_shadow_link(kg, cray->self, current_isect.object)) {
    *args->valid = 0;
    return;
  }
#endif

  /* If no transparent shadows, all light is blocked. */
  const int flags = intersection_get_shader_flags(kg, current_isect.prim, current_isect.type);
  if ((flags & SD_HAS_TRANSPARENT_SHADOW) == 0) {
    ctx->opaque_hit = true;
    return;
  }

  if (intersection_skip_shadow_already_recoded(
          ctx->isect_s, current_isect.object, current_isect.prim, ctx->num_recorded_hits))
  {
    *args->valid = 0;
    return;
  }

  /* Only count transparent bounces, volume bounds bounces are counted during shading. */
  ctx->num_transparent_hits += !(flags & SD_HAS_ONLY_VOLUME);
  if (ctx->num_transparent_hits > ctx->max_transparent_hits) {
    /* Max number of hits exceeded. */
    ctx->opaque_hit = true;
    return;
  }

  /* Always use baked shadow transparency for curves. */
  if (current_isect.type & PRIMITIVE_CURVE) {
    ctx->throughput *= intersection_curve_shadow_transparency(
        kg, current_isect.object, current_isect.prim, current_isect.type, current_isect.u);

    if (ctx->throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
      ctx->opaque_hit = true;
      return;
    }
    else {
      *args->valid = 0;
      return;
    }
  }

  numhit_t isect_index = ctx->num_recorded_hits;

  /* Always increase the number of recorded hits, even beyond the maximum,
   * so that we can detect this and trace another ray if needed.
   * More details about the related logic can be found in implementation of
   * "shadow_intersections_has_remaining" and "integrate_transparent_shadow"
   * functions. */
  ++ctx->num_recorded_hits;

  /* This tells Embree to continue tracing. */
  *args->valid = 0;

  const numhit_t max_record_hits = numhit_t(INTEGRATOR_SHADOW_ISECT_SIZE);
  /* If the maximum number of hits was reached, replace the furthest intersection
   * with a closer one so we get the N closest intersections. */
  if (isect_index >= max_record_hits) {
    /* When recording only N closest hits, max_t will always only decrease.
     * So let's test if we are already not meeting criteria and can skip max_t recalculation. */
    if (current_isect.t >= ctx->max_t) {
      return;
    }

    float max_t = INTEGRATOR_STATE_ARRAY(ctx->isect_s, shadow_isect, 0, t);
    numhit_t max_recorded_hit = numhit_t(0);
    float second_largest_t = 0.0f;

    for (numhit_t i = numhit_t(1); i < max_record_hits; ++i) {
      const float isect_t = INTEGRATOR_STATE_ARRAY(ctx->isect_s, shadow_isect, i, t);
      if (isect_t > max_t) {
        second_largest_t = max_t;
        max_recorded_hit = i;
        max_t = isect_t;
      }
      else if (isect_t > second_largest_t) {
        second_largest_t = isect_t;
      }
    }

    if (isect_index == max_record_hits && current_isect.t >= max_t) {
      /* `ctx->max_t` was initialized to `ray->tmax` before the index exceeds the limit. Now that
       * we have looped through the array, we can properly clamp `ctx->max_t`. */
      ctx->max_t = max_t;
      return;
    }

    isect_index = max_recorded_hit;

    /* After replacing `max_t` with `current_isect.t`, the new largest t would be either
     * `current_isect.t` or the second largest t before. */
    ctx->max_t = max(second_largest_t, current_isect.t);
  }

  integrator_state_write_shadow_isect(ctx->isect_s, &current_isect, isect_index);
}

ccl_device_forceinline void kernel_embree_filter_occluded_local_func_impl(
    const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;
  CCLLocalContext *ctx = (CCLLocalContext *)(args->context);
#ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#endif
  const Ray *cray = ctx->ray;

  /* Check if it's hitting the correct object. */
  Intersection current_isect;
  if (ctx->is_sss) {
    kernel_embree_convert_sss_hit(kg,
                                  ray,
                                  hit,
                                  &current_isect,
                                  ctx->local_object_id,
                                  reinterpret_cast<intptr_t>(args->geometryUserPtr));
  }
  else {
    kernel_embree_convert_hit(
        kg, ray, hit, &current_isect, reinterpret_cast<intptr_t>(args->geometryUserPtr));
    if (ctx->local_object_id != current_isect.object) {
      /* This tells Embree to continue tracing. */
      *args->valid = 0;
      return;
    }
  }
  if (intersection_skip_self_local(cray->self, current_isect.prim)) {
    *args->valid = 0;
    return;
  }

  /* No intersection information requested, just return a hit. */
  if (ctx->max_hits == 0) {
    return;
  }

  /* Ignore curves. */
  if (EMBREE_IS_HAIR(hit->geomID)) {
    /* This tells Embree to continue tracing. */
    *args->valid = 0;
    return;
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
}

ccl_device_forceinline void kernel_embree_filter_occluded_volume_all_func_impl(
    const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;
  CCLVolumeContext *ctx = (CCLVolumeContext *)(args->context);
#ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#endif
  const Ray *cray = ctx->ray;

#ifdef __VOLUME_RECORD_ALL__
  /* Append the intersection to the end of the array. */
  if (ctx->num_hits < ctx->max_hits) {
#endif
    Intersection current_isect;
    kernel_embree_convert_hit(
        kg, ray, hit, &current_isect, reinterpret_cast<intptr_t>(args->geometryUserPtr));
    if (intersection_skip_self(cray->self, current_isect.object, current_isect.prim)) {
      *args->valid = 0;
      return;
    }

    Intersection *isect = &ctx->vol_isect[ctx->num_hits];
    ++ctx->num_hits;
    *isect = current_isect;
    /* Only primitives from volume object. */
    uint tri_object = isect->object;
    int object_flag = kernel_data_fetch(object_flag, tri_object);
    if ((object_flag & SD_OBJECT_HAS_VOLUME) == 0) {
      --ctx->num_hits;
#ifndef __VOLUME_RECORD_ALL__
      /* Without __VOLUME_RECORD_ALL__ we need only a first counted hit, so we will
       * continue tracing only if a current hit is not counted. */
      *args->valid = 0;
#endif
    }
#ifdef __VOLUME_RECORD_ALL__
    /* This tells Embree to continue tracing. */
    *args->valid = 0;
  }
#endif
}

#ifdef __KERNEL_ONEAPI__
/* Static wrappers so we can call the callbacks from out side the ONEAPIKernelContext class */
RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_filter_intersection_func_static(const RTCFilterFunctionNArguments *args)
{
  RTCHit *hit = (RTCHit *)args->hit;
  CCLFirstHitContext *ctx = (CCLFirstHitContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_filter_intersection_func_impl(args);
}

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_filter_occluded_shadow_all_func_static(const RTCFilterFunctionNArguments *args)
{
  RTCHit *hit = (RTCHit *)args->hit;
  CCLShadowContext *ctx = (CCLShadowContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_filter_occluded_shadow_all_func_impl(args);
}

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_filter_occluded_local_func_static(const RTCFilterFunctionNArguments *args)
{
  RTCHit *hit = (RTCHit *)args->hit;
  CCLLocalContext *ctx = (CCLLocalContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_filter_occluded_local_func_impl(args);
}

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_filter_occluded_volume_all_func_static(const RTCFilterFunctionNArguments *args)
{
  RTCHit *hit = (RTCHit *)args->hit;
  CCLVolumeContext *ctx = (CCLVolumeContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_filter_occluded_volume_all_func_impl(args);
}

#  define kernel_embree_filter_intersection_func \
    ONEAPIKernelContext::kernel_embree_filter_intersection_func_static
#  define kernel_embree_filter_occluded_shadow_all_func \
    ONEAPIKernelContext::kernel_embree_filter_occluded_shadow_all_func_static
#  define kernel_embree_filter_occluded_local_func \
    ONEAPIKernelContext::kernel_embree_filter_occluded_local_func_static
#  define kernel_embree_filter_occluded_volume_all_func \
    ONEAPIKernelContext::kernel_embree_filter_occluded_volume_all_func_static
#else
#  define kernel_embree_filter_intersection_func kernel_embree_filter_intersection_func_impl
#  define kernel_embree_filter_occluded_shadow_all_func \
    kernel_embree_filter_occluded_shadow_all_func_impl
#  define kernel_embree_filter_occluded_local_func kernel_embree_filter_occluded_local_func_impl
#  define kernel_embree_filter_occluded_volume_all_func \
    kernel_embree_filter_occluded_volume_all_func_impl
#endif

/* Scene intersection. */

ccl_device_intersect bool kernel_embree_intersect(KernelGlobals kg,
                                                  const ccl_private Ray *ray,
                                                  const uint visibility,
                                                  ccl_private Intersection *isect)
{
  isect->t = ray->tmax;
  CCLFirstHitContext ctx;
  rtcInitRayQueryContext(&ctx);
#ifdef __KERNEL_ONEAPI__
  /* NOTE(sirgienko): Cycles GPU back-ends passes nullptr to KernelGlobals and
   * uses global device allocation (CUDA, Optix, HIP) or passes all needed data
   * as a class context (Metal, oneAPI). So we need to pass this context here
   * in order to have an access to it later in Embree filter functions on GPU. */
  ctx.kg = (KernelGlobals)this;
#else
  ctx.kg = kg;
#endif

  RTCRayHit ray_hit;
  ctx.ray = ray;
  kernel_embree_setup_rayhit(*ray, ray_hit, visibility);

  RTCIntersectArguments args;
  rtcInitIntersectArguments(&args);
  args.filter = reinterpret_cast<RTCFilterFunctionN>(kernel_embree_filter_intersection_func);
  args.feature_mask = CYCLES_EMBREE_USED_FEATURES;
  args.context = &ctx;
  rtcTraversableIntersect1(kernel_data.device_bvh, &ray_hit, &args);
  if (ray_hit.hit.geomID == RTC_INVALID_GEOMETRY_ID ||
      ray_hit.hit.primID == RTC_INVALID_GEOMETRY_ID)
  {
    return false;
  }

  kernel_embree_convert_hit(kg, &ray_hit.ray, &ray_hit.hit, isect);
  return true;
}

#ifdef __BVH_LOCAL__
ccl_device_intersect bool kernel_embree_intersect_local(KernelGlobals kg,
                                                        const ccl_private Ray *ray,
                                                        ccl_private LocalIntersection *local_isect,
                                                        const int local_object,
                                                        ccl_private uint *lcg_state,
                                                        const int max_hits)
{
  const bool has_bvh = !(kernel_data_fetch(object_flag, local_object) &
                         SD_OBJECT_TRANSFORM_APPLIED);
  CCLLocalContext ctx;
  rtcInitRayQueryContext(&ctx);
#  ifdef __KERNEL_ONEAPI__
  /* NOTE(sirgienko): Cycles GPU back-ends passes nullptr to KernelGlobals and
   * uses global device allocation (CUDA, Optix, HIP) or passes all needed data
   * as a class context (Metal, oneAPI). So we need to pass this context here
   * in order to have an access to it later in Embree filter functions on GPU. */
  ctx.kg = (KernelGlobals)this;
#  else
  ctx.kg = kg;
#  endif
  ctx.is_sss = has_bvh;
  ctx.lcg_state = lcg_state;
  ctx.max_hits = max_hits;
  ctx.ray = ray;
  ctx.local_isect = local_isect;
  if (local_isect) {
    local_isect->num_hits = 0;
  }
  ctx.local_object_id = local_object;
  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, PATH_RAY_ALL_VISIBILITY);

  RTCOccludedArguments args;
  rtcInitOccludedArguments(&args);
  args.filter = reinterpret_cast<RTCFilterFunctionN>(kernel_embree_filter_occluded_local_func);
  args.feature_mask = CYCLES_EMBREE_USED_FEATURES;
  args.context = &ctx;

  /* If this object has its own BVH, use it. */
  if (has_bvh) {
    float3 P = ray->P;
    float3 dir = ray->D;
    float3 idir = ray->D;
#  ifdef __OBJECT_MOTION__
    bvh_instance_motion_push(kg, local_object, ray, &P, &dir, &idir);
#  else
    bvh_instance_push(kg, local_object, ray, &P, &dir, &idir);
#  endif

    rtc_ray.org_x = P.x;
    rtc_ray.org_y = P.y;
    rtc_ray.org_z = P.z;
    rtc_ray.dir_x = dir.x;
    rtc_ray.dir_y = dir.y;
    rtc_ray.dir_z = dir.z;
    rtc_ray.tnear = ray->tmin;
    rtc_ray.tfar = ray->tmax;
    RTCTraversable scene = (RTCTraversable)rtcGetGeometryUserDataFromTraversable(
        kernel_data.device_bvh, local_object * 2);
    kernel_assert(scene);
    if (scene) {
      rtcTraversableOccluded1(scene, &rtc_ray, &args);
    }
  }
  else {
    rtcTraversableOccluded1(kernel_data.device_bvh, &rtc_ray, &args);
  }

  /* rtcOccluded1 sets tfar to -inf if a hit was found. */
  return (local_isect && local_isect->num_hits > 0) || (rtc_ray.tfar < 0);
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool kernel_embree_intersect_shadow_all(KernelGlobals kg,
                                                             IntegratorShadowState state,
                                                             const ccl_private Ray *ray,
                                                             const uint visibility,
                                                             const uint max_transparent_hits,
                                                             ccl_private uint *num_recorded_hits,
                                                             ccl_private float *throughput)
{
  CCLShadowContext ctx;
  rtcInitRayQueryContext(&ctx);
#  ifdef __KERNEL_ONEAPI__
  /* NOTE(sirgienko): Cycles GPU back-ends passes nullptr to KernelGlobals and
   * uses global device allocation (CUDA, Optix, HIP) or passes all needed data
   * as a class context (Metal, oneAPI). So we need to pass this context here
   * in order to have an access to it later in Embree filter functions on GPU. */
  ctx.kg = (KernelGlobals)this;
#  else
  ctx.kg = kg;
#  endif
  ctx.num_transparent_hits = ctx.num_recorded_hits = numhit_t(0);
  ctx.throughput = 1.0f;
  ctx.opaque_hit = false;
  ctx.isect_s = state;
  ctx.max_transparent_hits = numhit_t(max_transparent_hits);
  ctx.max_t = ray->tmax;
  ctx.ray = ray;
  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, visibility);
  RTCOccludedArguments args;
  rtcInitOccludedArguments(&args);
  args.filter = reinterpret_cast<RTCFilterFunctionN>(
      kernel_embree_filter_occluded_shadow_all_func);
  args.feature_mask = CYCLES_EMBREE_USED_FEATURES;
  args.context = &ctx;
  rtcTraversableOccluded1(kernel_data.device_bvh, &rtc_ray, &args);

  *num_recorded_hits = ctx.num_recorded_hits;
  *throughput = ctx.throughput;
  return ctx.opaque_hit;
}
#endif

#ifdef __VOLUME__
ccl_device_intersect uint kernel_embree_intersect_volume(KernelGlobals kg,
                                                         const ccl_private Ray *ray,
                                                         ccl_private Intersection *isect,
#  ifdef __VOLUME_RECORD_ALL__
                                                         const uint max_hits,
#  endif
                                                         const uint visibility)
{
  CCLVolumeContext ctx;
  rtcInitRayQueryContext(&ctx);
#  ifdef __KERNEL_ONEAPI__
  /* NOTE(sirgienko) Cycles GPU back-ends passes nullptr to KernelGlobals and
   * uses global device allocation (CUDA, Optix, HIP) or passes all needed data
   * as a class context (Metal, oneAPI). So we need to pass this context here
   * in order to have an access to it later in Embree filter functions on GPU. */
  ctx.kg = (KernelGlobals)this;
#  else
  ctx.kg = kg;
#  endif
  ctx.vol_isect = isect;
#  ifdef __VOLUME_RECORD_ALL__
  ctx.max_hits = numhit_t(max_hits);
#  endif
  ctx.num_hits = numhit_t(0);
  ctx.ray = ray;
  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, visibility);
  RTCOccludedArguments args;
  rtcInitOccludedArguments(&args);
  args.filter = reinterpret_cast<RTCFilterFunctionN>(
      kernel_embree_filter_occluded_volume_all_func);
  args.feature_mask = CYCLES_EMBREE_USED_FEATURES;
  args.context = &ctx;
  rtcTraversableOccluded1(kernel_data.device_bvh, &rtc_ray, &args);
  return ctx.num_hits;
}
#endif

CCL_NAMESPACE_END

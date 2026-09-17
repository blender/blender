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

#include "kernel/bvh/intersect_filter.h"
#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"
#include "kernel/geom/gsplat_intersect.h"
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
                      RTC_FEATURE_FLAG_FLAT_CATMULL_ROM_CURVE | \
                      RTC_FEATURE_FLAG_ROUND_LINEAR_CURVE | \
                      RTC_FEATURE_FLAG_USER_GEOMETRY_CALLBACK_IN_ARGUMENTS)
#endif

#define EMBREE_IS_HAIR(x) (x & 1)

/* Intersection context. */

struct CCLFirstHitContext : public RTCRayQueryContext {
  KernelGlobals kg;
  /* For avoiding self intersections */
  const Ray *ray;
};

struct CCLShadowContext : public RTCRayQueryContext {
#if defined(__KERNEL_ONEAPI__)
  ONEAPIKernelContext *oneapi_kernel_context;
#else
  KernelGlobals kg;
#endif

  BVHShadowAllPayload *payload;
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
                                                          const Ray *ray)
{
  const int object = kernel_embree_get_hit_object(hit);

  int prim;
  if ((ray->self.object == object) || (ray->self.light_object == object)) {
    const int prim_offset = kernel_data_fetch(object_prim_offset, object);
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
                                                 Intersection *isect)
{
  const int object = kernel_embree_get_hit_object(hit);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object);

  isect->t = ray->tfar;
  isect->prim = hit->primID + prim_offset;
  isect->object = object;

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
    KernelGlobals kg, const RTCRay *ray, const RTCHit *hit, Intersection *isect, const int object)
{
  const int prim_offset = kernel_data_fetch(object_prim_offset, object);

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
  kernel_assert(args->N == 1);

  RTCHit *hit = (RTCHit *)args->hit;
  CCLFirstHitContext *ctx = (CCLFirstHitContext *)(args->context);
#ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#endif
  const Ray *cray = ctx->ray;

  if (kernel_embree_is_self_intersection(kg, hit, cray)) {
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

ccl_device_forceinline int custom_intersect_get_object(const RTCRayQueryContext &ctx,
                                                       unsigned int geom_id)
{
  return ((ctx.instID[0] != RTC_INVALID_GEOMETRY_ID) ? ctx.instID[0] : geom_id) / 2;
}

ccl_device_forceinline void kernel_embree_custom_intersection_func_impl(
    const RTCIntersectFunctionNArguments *args)
{
  kernel_assert(args->N == 1);
  static_assert(RTC_MAX_INSTANCE_LEVEL_COUNT == 1);

#if defined(__GSPLATS__)
  CCLFirstHitContext *ctx = (CCLFirstHitContext *)(args->context);
  RTCRayHit *ray_hit = (RTCRayHit *)args->rayhit;

#  ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#  else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#  endif

  if (!args->valid[0]) {
    return;
  }

  const float3 ray_P = make_float3(ray_hit->ray.org_x, ray_hit->ray.org_y, ray_hit->ray.org_z);
  const float3 ray_D = make_float3(ray_hit->ray.dir_x, ray_hit->ray.dir_y, ray_hit->ray.dir_z);

  const uint object = custom_intersect_get_object(*ctx, args->geomID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object);
  const int prim = args->primID + prim_offset;

  /* Currently only gaussian splats are handled via the custom geometry. */
  const int primitive_type = kernel_data_fetch(objects, object).primitive_type;
  kernel_assert(primitive_type & PRIMITIVE_GSPLAT);

  float isect_t;
  if (!gsplat_intersect_test(kg,
                             ray_P,
                             ray_D,
                             ray_hit->ray.tnear,
                             ray_hit->ray.tfar,
                             object,
                             prim,
                             ray_hit->ray.time,
                             primitive_type,
                             isect_t))
  {
    args->valid[0] = 0;
    return;
  }

  RTCHit potential_hit;
  potential_hit.u = 0.0f;
  potential_hit.v = 0.0f;
  potential_hit.primID = args->primID;
  potential_hit.geomID = args->geomID;
  potential_hit.instID[0] = ctx->instID[0];

  const float old_tfar = ray_hit->ray.tfar;
  ray_hit->ray.tfar = isect_t;

  RTCFilterFunctionNArguments filter_args;
  int valid = -1;
  filter_args.valid = &valid;
  filter_args.geometryUserPtr = args->geometryUserPtr;
  filter_args.context = args->context;
  filter_args.ray = (RTCRayN *)&ray_hit->ray;
  filter_args.hit = (RTCHitN *)&potential_hit;
  filter_args.N = 1;
  kernel_embree_filter_intersection_func_impl(&filter_args);
  if (!valid) {
    ray_hit->ray.tfar = old_tfar;
    args->valid[0] = 0;
    return;
  }

  ray_hit->hit = potential_hit;
  args->valid[0] = -1;
#else
  args->valid[0] = 0;
#endif
}

ccl_device_forceinline void kernel_embree_custom_occluded_func_impl(
    const RTCOccludedFunctionNArguments *args)
{
  kernel_assert(args->N == 1);
  static_assert(RTC_MAX_INSTANCE_LEVEL_COUNT == 1);

#if defined(__GSPLATS__)
  if (!args->valid[0]) {
    return;
  }

  CCLShadowContext *ctx = (CCLShadowContext *)(args->context);
  RTCRay *ray = (RTCRay *)args->ray;
  BVHShadowAllPayload &payload = *ctx->payload;

#  ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#  else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#  endif

  const float3 ray_P = make_float3(ray->org_x, ray->org_y, ray->org_z);
  const float3 ray_D = make_float3(ray->dir_x, ray->dir_y, ray->dir_z);

  const uint object = custom_intersect_get_object(*ctx, args->geomID);
  const int prim_offset = kernel_data_fetch(object_prim_offset, object);
  const int prim = args->primID + prim_offset;

  /* Currently only gaussian splats are handled via the custom geometry. */
  const int primitive_type = kernel_data_fetch(objects, object).primitive_type;
  kernel_assert(primitive_type & PRIMITIVE_GSPLAT);

  Intersection isect;
  if (!gsplat_intersect(kg,
                        &isect,
                        ray_P,
                        ray_D,
                        ray->tnear,
                        ray->tfar,
                        object,
                        prim,
                        ray->time,
                        primitive_type))
  {
    args->valid[0] = 0;
    return;
  }

  if (bvh_shadow_all_anyhit_filter<ISECT_TEST_ALL & ~ISECT_TEST_VISIBILITY_FLAG>(
          kg, payload.state, payload, payload.base.ray_self, 0, isect))
  {
    args->valid[0] = 0;
    return;
  }

  payload.throughput = zero_float3();

  ray->tfar = -FLT_MAX;
  args->valid[0] = -1;
#else
  args->valid[0] = 0;
#endif
}

ccl_device_forceinline void kernel_embree_custom_occluded_local_func_impl(
    const RTCOccludedFunctionNArguments *args)
{
  kernel_assert(args->N == 1);

  /* Ignore intersections with custom primitives.
   * Currently the only custom primitive is the Gaussian splat, which requires extra design work
   * to integrate into local intersection test. */
  args->valid[0] = 0;
}

ccl_device_forceinline void kernel_embree_custom_occluded_volume_func_impl(
    const RTCOccludedFunctionNArguments *args)
{
  kernel_assert(args->N == 1);

  /* Ignore intersections with custom primitives.
   * Currently the only custom primitive is the Gaussian splat, which does not support volume
   * shaders. */
  args->valid[0] = 0;
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
  kernel_assert(args->N == 1);

  const RTCRay *ray = (RTCRay *)args->ray;
  const RTCHit *hit = (RTCHit *)args->hit;

  CCLShadowContext *ctx = (CCLShadowContext *)(args->context);
  BVHShadowAllPayload &payload = *ctx->payload;

#ifdef __KERNEL_ONEAPI__
  KernelGlobalsGPU *kg = nullptr;
#else
  const ThreadKernelGlobalsCPU *kg = ctx->kg;
#endif

  Intersection isect;
  kernel_embree_convert_hit(kg, ray, hit, &isect);

  if (!bvh_shadow_all_anyhit_filter<ISECT_TEST_ALL & ~ISECT_TEST_VISIBILITY_FLAG>(
          kg, payload.state, payload, payload.base.ray_self, 0, isect))
  {
    return;
  }

  *args->valid = 0;
}

ccl_device_forceinline void kernel_embree_filter_occluded_local_func_impl(
    const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  kernel_assert(args->N == 1);

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
    kernel_embree_convert_sss_hit(kg, ray, hit, &current_isect, ctx->local_object_id);
  }
  else {
    kernel_embree_convert_hit(kg, ray, hit, &current_isect);
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
  kernel_assert(args->N == 1);

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
    kernel_embree_convert_hit(kg, ray, hit, &current_isect);

    if (bvh_volume_anyhit_triangle_filter<false>(
            kg, current_isect.object, current_isect.prim, cray->self, 0))
    {
      *args->valid = 0;
      return;
    }

    Intersection *isect = &ctx->vol_isect[ctx->num_hits];
    ++ctx->num_hits;
    *isect = current_isect;
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
  ONEAPIKernelContext *context = ctx->oneapi_kernel_context;
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

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_custom_intersection_func_static(const RTCIntersectFunctionNArguments *args)
{
  CCLFirstHitContext *ctx = (CCLFirstHitContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_custom_intersection_func_impl(args);
}

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_custom_occluded_func_static(const RTCOccludedFunctionNArguments *args)
{
  CCLShadowContext *ctx = (CCLShadowContext *)(args->context);
  ONEAPIKernelContext *context = ctx->oneapi_kernel_context;
  context->kernel_embree_custom_occluded_func_impl(args);
}

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_custom_occluded_local_func_static(const RTCOccludedFunctionNArguments *args)
{
  CCLLocalContext *ctx = (CCLLocalContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_custom_occluded_local_func_impl(args);
}

RTC_SYCL_INDIRECTLY_CALLABLE static void ccl_always_inline
kernel_embree_custom_occluded_volume_func_static(const RTCOccludedFunctionNArguments *args)
{
  CCLVolumeContext *ctx = (CCLVolumeContext *)(args->context);
  ONEAPIKernelContext *context = static_cast<ONEAPIKernelContext *>(ctx->kg);
  context->kernel_embree_custom_occluded_volume_func_impl(args);
}

#  define kernel_embree_filter_intersection_func \
    ONEAPIKernelContext::kernel_embree_filter_intersection_func_static
#  define kernel_embree_filter_occluded_shadow_all_func \
    ONEAPIKernelContext::kernel_embree_filter_occluded_shadow_all_func_static
#  define kernel_embree_filter_occluded_local_func \
    ONEAPIKernelContext::kernel_embree_filter_occluded_local_func_static
#  define kernel_embree_filter_occluded_volume_all_func \
    ONEAPIKernelContext::kernel_embree_filter_occluded_volume_all_func_static
#  define kernel_embree_custom_intersection_func kernel_embree_custom_intersection_func_static
#  define kernel_embree_custom_occluded_func kernel_embree_custom_occluded_func_static
#  define kernel_embree_custom_occluded_local_func kernel_embree_custom_occluded_local_func_static
#  define kernel_embree_custom_occluded_volume_func \
    kernel_embree_custom_occluded_volume_func_static
#else
#  define kernel_embree_filter_intersection_func kernel_embree_filter_intersection_func_impl
#  define kernel_embree_filter_occluded_shadow_all_func \
    kernel_embree_filter_occluded_shadow_all_func_impl
#  define kernel_embree_filter_occluded_local_func kernel_embree_filter_occluded_local_func_impl
#  define kernel_embree_filter_occluded_volume_all_func \
    kernel_embree_filter_occluded_volume_all_func_impl
#  define kernel_embree_custom_intersection_func kernel_embree_custom_intersection_func_impl
#  define kernel_embree_custom_occluded_func kernel_embree_custom_occluded_func_impl
#  define kernel_embree_custom_occluded_local_func kernel_embree_custom_occluded_local_func_impl
#  define kernel_embree_custom_occluded_volume_func kernel_embree_custom_occluded_volume_func_impl
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
  args.intersect = reinterpret_cast<RTCIntersectFunctionN>(kernel_embree_custom_intersection_func);
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
  kernel_embree_setup_ray(*ray, rtc_ray, PATH_RAY_VISIBILITY_ALL);

  RTCOccludedArguments args;
  rtcInitOccludedArguments(&args);
  args.occluded = reinterpret_cast<RTCOccludedFunctionN>(kernel_embree_custom_occluded_local_func);
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

#ifdef __TRANSPARENT_SHADOWS__
ccl_device_intersect void kernel_embree_intersect_shadow_all(KernelGlobals kg,
                                                             const ccl_private Ray *ray,
                                                             BVHShadowAllPayload &payload)
{
  CCLShadowContext ctx;
  rtcInitRayQueryContext(&ctx);
#  if defined(__KERNEL_ONEAPI__)
  ctx.oneapi_kernel_context = this;
#  else
  ctx.kg = kg;
#  endif
  ctx.payload = &payload;

  RTCRay rtc_ray;
  kernel_embree_setup_ray(*ray, rtc_ray, payload.base.ray_visibility);

  RTCOccludedArguments args;
  rtcInitOccludedArguments(&args);
  args.filter = reinterpret_cast<RTCFilterFunctionN>(
      kernel_embree_filter_occluded_shadow_all_func);
  args.occluded = reinterpret_cast<RTCOccludedFunctionN>(kernel_embree_custom_occluded_func);
  args.feature_mask = CYCLES_EMBREE_USED_FEATURES;
  args.context = &ctx;

  rtcTraversableOccluded1(kernel_data.device_bvh, &rtc_ray, &args);
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
  args.occluded = reinterpret_cast<RTCOccludedFunctionN>(
      kernel_embree_custom_occluded_volume_func);
  args.filter = reinterpret_cast<RTCFilterFunctionN>(
      kernel_embree_filter_occluded_volume_all_func);
  args.feature_mask = CYCLES_EMBREE_USED_FEATURES;
  args.context = &ctx;
  rtcTraversableOccluded1(kernel_data.device_bvh, &rtc_ray, &args);
  return ctx.num_hits;
}
#endif

CCL_NAMESPACE_END

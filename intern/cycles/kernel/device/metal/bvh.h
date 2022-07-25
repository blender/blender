/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

/* MetalRT implementation of ray-scene intersection. */

#pragma once

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"

CCL_NAMESPACE_BEGIN

/* Payload types. */

struct MetalRTIntersectionPayload {
  RaySelfPrimitives self;
  uint visibility;
  float u, v;
  int prim;
  int type;
#if defined(__METALRT_MOTION__)
  float time;
#endif
};

struct MetalRTIntersectionLocalPayload {
  RaySelfPrimitives self;
  uint local_object;
  uint lcg_state;
  short max_hits;
  bool has_lcg_state;
  bool result;
  LocalIntersection local_isect;
};

struct MetalRTIntersectionShadowPayload {
  RaySelfPrimitives self;
  uint visibility;
#if defined(__METALRT_MOTION__)
  float time;
#endif
  int state;
  float throughput;
  short max_hits;
  short num_hits;
  short num_recorded_hits;
  bool result;
};

/* Intersection return types. */

/* For a bounding box intersection function. */
struct BoundingBoxIntersectionResult {
  bool accept [[accept_intersection]];
  bool continue_search [[continue_search]];
  float distance [[distance]];
};

/* For a triangle intersection function. */
struct TriangleIntersectionResult {
  bool accept [[accept_intersection]];
  bool continue_search [[continue_search]];
};

enum { METALRT_HIT_TRIANGLE, METALRT_HIT_BOUNDING_BOX };

/* Utilities. */

ccl_device_inline bool intersection_skip_self(ray_data const RaySelfPrimitives &self,
                                              const int object,
                                              const int prim)
{
  return (self.prim == prim) && (self.object == object);
}

ccl_device_inline bool intersection_skip_self_shadow(ray_data const RaySelfPrimitives &self,
                                                     const int object,
                                                     const int prim)
{
  return ((self.prim == prim) && (self.object == object)) ||
         ((self.light_prim == prim) && (self.light_object == object));
}

ccl_device_inline bool intersection_skip_self_local(ray_data const RaySelfPrimitives &self,
                                                    const int prim)
{
  return (self.prim == prim);
}

/* Hit functions. */

template<typename TReturn, uint intersection_type>
TReturn metalrt_local_hit(constant KernelParamsMetal &launch_params_metal,
                          ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload,
                          const uint object,
                          const uint primitive_id,
                          const float2 barycentrics,
                          const float ray_tmax)
{
  TReturn result;

#ifdef __BVH_LOCAL__
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);

  if ((object != payload.local_object) || intersection_skip_self_local(payload.self, prim)) {
    /* Only intersect with matching object and skip self-intersecton. */
    result.accept = false;
    result.continue_search = true;
    return result;
  }

  const short max_hits = payload.max_hits;
  if (max_hits == 0) {
    /* Special case for when no hit information is requested, just report that something was hit */
    payload.result = true;
    result.accept = true;
    result.continue_search = false;
    return result;
  }

  int hit = 0;
  if (payload.has_lcg_state) {
    for (short i = min(max_hits, short(payload.local_isect.num_hits)) - 1; i >= 0; --i) {
      if (ray_tmax == payload.local_isect.hits[i].t) {
        result.accept = false;
        result.continue_search = true;
        return result;
      }
    }

    hit = payload.local_isect.num_hits++;

    if (payload.local_isect.num_hits > max_hits) {
      hit = lcg_step_uint(&payload.lcg_state) % payload.local_isect.num_hits;
      if (hit >= max_hits) {
        result.accept = false;
        result.continue_search = true;
        return result;
      }
    }
  }
  else {
    if (payload.local_isect.num_hits && ray_tmax > payload.local_isect.hits[0].t) {
      /* Record closest intersection only. Do not terminate ray here, since there is no guarantee
       * about distance ordering in any-hit */
      result.accept = false;
      result.continue_search = true;
      return result;
    }

    payload.local_isect.num_hits = 1;
  }

  ray_data Intersection *isect = &payload.local_isect.hits[hit];
  isect->t = ray_tmax;
  isect->prim = prim;
  isect->object = object;
  isect->type = kernel_data_fetch(objects, object).primitive_type;

  isect->u = 1.0f - barycentrics.y - barycentrics.x;
  isect->v = barycentrics.x;

  /* Record geometric normal */
  const uint tri_vindex = kernel_data_fetch(tri_vindex, isect->prim).w;
  const float3 tri_a = float3(kernel_data_fetch(tri_verts, tri_vindex + 0));
  const float3 tri_b = float3(kernel_data_fetch(tri_verts, tri_vindex + 1));
  const float3 tri_c = float3(kernel_data_fetch(tri_verts, tri_vindex + 2));
  payload.local_isect.Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  /* Continue tracing (without this the trace call would return after the first hit) */
  result.accept = false;
  result.continue_search = true;
  return result;
#endif
}

[[intersection(triangle, triangle_data, METALRT_TAGS)]] TriangleIntersectionResult
__anyhit__cycles_metalrt_local_hit_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload [[payload]],
    uint instance_id [[user_instance_id]],
    uint primitive_id [[primitive_id]],
    float2 barycentrics [[barycentric_coord]],
    float ray_tmax [[distance]])
{
  return metalrt_local_hit<TriangleIntersectionResult, METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, instance_id, primitive_id, barycentrics, ray_tmax);
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_local_hit_box(const float ray_tmax [[max_distance]])
{
  /* unused function */
  BoundingBoxIntersectionResult result;
  result.distance = ray_tmax;
  result.accept = false;
  result.continue_search = false;
  return result;
}

template<uint intersection_type>
bool metalrt_shadow_all_hit(constant KernelParamsMetal &launch_params_metal,
                            ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload,
                            uint object,
                            uint prim,
                            const float2 barycentrics,
                            const float ray_tmax)
{
#ifdef __SHADOW_RECORD_ALL__
#  ifdef __VISIBILITY_FLAG__
  const uint visibility = payload.visibility;
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    /* continue search */
    return true;
  }
#  endif

  if (intersection_skip_self_shadow(payload.self, object, prim)) {
    /* continue search */
    return true;
  }

  float u = 0.0f, v = 0.0f;
  int type = 0;
  if (intersection_type == METALRT_HIT_TRIANGLE) {
    u = 1.0f - barycentrics.y - barycentrics.x;
    v = barycentrics.x;
    type = kernel_data_fetch(objects, object).primitive_type;
  }
#  ifdef __HAIR__
  else {
    u = barycentrics.x;
    v = barycentrics.y;

    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    type = segment.type;
    prim = segment.prim;

    /* Filter out curve endcaps */
    if (u == 0.0f || u == 1.0f) {
      /* continue search */
      return true;
    }
  }
#  endif

#  ifndef __TRANSPARENT_SHADOWS__
  /* No transparent shadows support compiled in, make opaque. */
  payload.result = true;
  /* terminate ray */
  return false;
#  else
  short max_hits = payload.max_hits;
  short num_hits = payload.num_hits;
  short num_recorded_hits = payload.num_recorded_hits;

  MetalKernelContext context(launch_params_metal);

  /* If no transparent shadows, all light is blocked and we can stop immediately. */
  if (num_hits >= max_hits ||
      !(context.intersection_get_shader_flags(NULL, prim, type) & SD_HAS_TRANSPARENT_SHADOW)) {
    payload.result = true;
    /* terminate ray */
    return false;
  }

  /* Always use baked shadow transparency for curves. */
  if (type & PRIMITIVE_CURVE) {
    float throughput = payload.throughput;
    throughput *= context.intersection_curve_shadow_transparency(nullptr, object, prim, u);
    payload.throughput = throughput;
    payload.num_hits += 1;

    if (throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
      /* Accept result and terminate if throughput is sufficiently low */
      payload.result = true;
      return false;
    }
    else {
      return true;
    }
  }

  payload.num_hits += 1;
  payload.num_recorded_hits += 1;

  uint record_index = num_recorded_hits;

  const IntegratorShadowState state = payload.state;

  const uint max_record_hits = min(uint(max_hits), INTEGRATOR_SHADOW_ISECT_SIZE);
  if (record_index >= max_record_hits) {
    /* If maximum number of hits reached, find a hit to replace. */
    float max_recorded_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, t);
    uint max_recorded_hit = 0;

    for (int i = 1; i < max_record_hits; i++) {
      const float isect_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, i, t);
      if (isect_t > max_recorded_t) {
        max_recorded_t = isect_t;
        max_recorded_hit = i;
      }
    }

    if (ray_tmax >= max_recorded_t) {
      /* Accept hit, so that we don't consider any more hits beyond the distance of the
       * current hit anymore. */
      payload.result = true;
      return true;
    }

    record_index = max_recorded_hit;
  }

  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, u) = u;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, v) = v;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, t) = ray_tmax;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, prim) = prim;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, object) = object;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, record_index, type) = type;

  /* Continue tracing. */
#  endif /* __TRANSPARENT_SHADOWS__ */
#endif   /* __SHADOW_RECORD_ALL__ */

  return true;
}

[[intersection(triangle, triangle_data, METALRT_TAGS)]] TriangleIntersectionResult
__anyhit__cycles_metalrt_shadow_all_hit_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload [[payload]],
    unsigned int object [[user_instance_id]],
    unsigned int primitive_id [[primitive_id]],
    float2 barycentrics [[barycentric_coord]],
    float ray_tmax [[distance]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);

  TriangleIntersectionResult result;
  result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, object, prim, barycentrics, ray_tmax);
  result.accept = !result.continue_search;
  return result;
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_shadow_all_hit_box(const float ray_tmax [[max_distance]])
{
  /* unused function */
  BoundingBoxIntersectionResult result;
  result.distance = ray_tmax;
  result.accept = false;
  result.continue_search = false;
  return result;
}

template<typename TReturnType, uint intersection_type>
inline TReturnType metalrt_visibility_test(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload,
    const uint object,
    const uint prim,
    const float u)
{
  TReturnType result;

#ifdef __HAIR__
  if (intersection_type == METALRT_HIT_BOUNDING_BOX) {
    /* Filter out curve endcaps. */
    if (u == 0.0f || u == 1.0f) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }
  }
#endif

  uint visibility = payload.visibility;
#ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    result.accept = false;
    result.continue_search = true;
    return result;
  }
#endif

  /* Shadow ray early termination. */
  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    if (intersection_skip_self_shadow(payload.self, object, prim)) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }
    else {
      result.accept = true;
      result.continue_search = false;
      return result;
    }
  }
  else {
    if (intersection_skip_self(payload.self, object, prim)) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }
  }

  result.accept = true;
  result.continue_search = true;
  return result;
}

[[intersection(triangle, triangle_data, METALRT_TAGS)]] TriangleIntersectionResult
__anyhit__cycles_metalrt_visibility_test_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
    unsigned int object [[user_instance_id]],
    unsigned int primitive_id [[primitive_id]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  TriangleIntersectionResult result =
      metalrt_visibility_test<TriangleIntersectionResult, METALRT_HIT_TRIANGLE>(
          launch_params_metal, payload, object, prim, 0.0f);
  if (result.accept) {
    payload.prim = prim;
    payload.type = kernel_data_fetch(objects, object).primitive_type;
  }
  return result;
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_visibility_test_box(const float ray_tmax [[max_distance]])
{
  /* Unused function */
  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;
  return result;
}

/* Primitive intersection functions. */

#ifdef __HAIR__
ccl_device_inline void metalrt_intersection_curve(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload,
    const uint object,
    const uint prim,
    const uint type,
    const float3 ray_P,
    const float3 ray_D,
    float time,
    const float ray_tmin,
    const float ray_tmax,
    thread BoundingBoxIntersectionResult &result)
{
#  ifdef __VISIBILITY_FLAG__
  const uint visibility = payload.visibility;
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return;
  }
#  endif

  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.curve_intersect(
          NULL, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type)) {
    result = metalrt_visibility_test<BoundingBoxIntersectionResult, METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, isect.u);
    if (result.accept) {
      result.distance = isect.t;
      payload.u = isect.u;
      payload.v = isect.v;
      payload.prim = prim;
      payload.type = type;
    }
  }
}

ccl_device_inline void metalrt_intersection_curve_shadow(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload,
    const uint object,
    const uint prim,
    const uint type,
    float time,
    const float ray_tmin,
    const float ray_tmax,
    thread BoundingBoxIntersectionResult &result)
{
  const uint visibility = payload.visibility;

  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.curve_intersect(
          NULL, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type)) {
    result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, float2(isect.u, isect.v), ray_tmax);
    result.accept = !result.continue_search;
  }
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__intersection__curve_ribbon(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                             ray_data MetalKernelContext::MetalRTIntersectionPayload &payload
                             [[payload]],
                             const uint object [[user_instance_id]],
                             const uint primitive_id [[primitive_id]],
                             const float3 ray_P [[origin]],
                             const float3 ray_D [[direction]],
                             const float ray_tmin [[min_distance]],
                             const float ray_tmax [[max_distance]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

  if (segment.type & PRIMITIVE_CURVE_RIBBON) {
    metalrt_intersection_curve(launch_params_metal,
                               payload,
                               object,
                               segment.prim,
                               segment.type,
                               ray_P,
                               ray_D,
#  if defined(__METALRT_MOTION__)
                               payload.time,
#  else
                               0.0f,
#  endif
                               ray_tmin,
                               ray_tmax,
                               result);
  }

  return result;
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__intersection__curve_ribbon_shadow(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload [[payload]],
    const uint object [[user_instance_id]],
    const uint primitive_id [[primitive_id]],
    const float3 ray_P [[origin]],
    const float3 ray_D [[direction]],
    const float ray_tmin [[min_distance]],
    const float ray_tmax [[max_distance]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

  if (segment.type & PRIMITIVE_CURVE_RIBBON) {
    metalrt_intersection_curve_shadow(launch_params_metal,
                                      payload,
                                      object,
                                      segment.prim,
                                      segment.type,
                                      ray_P,
                                      ray_D,
#  if defined(__METALRT_MOTION__)
                                      payload.time,
#  else
                                      0.0f,
#  endif
                                      ray_tmin,
                                      ray_tmax,
                                      result);
  }

  return result;
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__intersection__curve_all(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                          ray_data MetalKernelContext::MetalRTIntersectionPayload &payload
                          [[payload]],
                          const uint object [[user_instance_id]],
                          const uint primitive_id [[primitive_id]],
                          const float3 ray_P [[origin]],
                          const float3 ray_D [[direction]],
                          const float ray_tmin [[min_distance]],
                          const float ray_tmax [[max_distance]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;
  metalrt_intersection_curve(launch_params_metal,
                             payload,
                             object,
                             segment.prim,
                             segment.type,
                             ray_P,
                             ray_D,
#  if defined(__METALRT_MOTION__)
                             payload.time,
#  else
                             0.0f,
#  endif
                             ray_tmin,
                             ray_tmax,
                             result);

  return result;
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__intersection__curve_all_shadow(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload [[payload]],
    const uint object [[user_instance_id]],
    const uint primitive_id [[primitive_id]],
    const float3 ray_P [[origin]],
    const float3 ray_D [[direction]],
    const float ray_tmin [[min_distance]],
    const float ray_tmax [[max_distance]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

  metalrt_intersection_curve_shadow(launch_params_metal,
                                    payload,
                                    object,
                                    segment.prim,
                                    segment.type,
                                    ray_P,
                                    ray_D,
#  if defined(__METALRT_MOTION__)
                                    payload.time,
#  else
                                    0.0f,
#  endif
                                    ray_tmin,
                                    ray_tmax,
                                    result);

  return result;
}
#endif /* __HAIR__ */

#ifdef __POINTCLOUD__
ccl_device_inline void metalrt_intersection_point(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload,
    const uint object,
    const uint prim,
    const uint type,
    const float3 ray_P,
    const float3 ray_D,
    float time,
    const float ray_tmin,
    const float ray_tmax,
    thread BoundingBoxIntersectionResult &result)
{
#  ifdef __VISIBILITY_FLAG__
  const uint visibility = payload.visibility;
  if ((kernel_data_fetch(objects, object).visibility & visibility) == 0) {
    return;
  }
#  endif

  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          NULL, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type)) {
    result = metalrt_visibility_test<BoundingBoxIntersectionResult, METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, isect.u);
    if (result.accept) {
      result.distance = isect.t;
      payload.u = isect.u;
      payload.v = isect.v;
      payload.prim = prim;
      payload.type = type;
    }
  }
}

ccl_device_inline void metalrt_intersection_point_shadow(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload,
    const uint object,
    const uint prim,
    const uint type,
    const float3 ray_P,
    const float3 ray_D,
    float time,
    const float ray_tmin,
    const float ray_tmax,
    thread BoundingBoxIntersectionResult &result)
{
  const uint visibility = payload.visibility;

  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          NULL, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type)) {
    result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, float2(isect.u, isect.v), ray_tmax);
    result.accept = !result.continue_search;

    if (result.accept) {
      result.distance = isect.t;
    }
  }
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__intersection__point(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                      ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
                      const uint object [[user_instance_id]],
                      const uint primitive_id [[primitive_id]],
                      const float3 ray_origin [[origin]],
                      const float3 ray_direction [[direction]],
                      const float ray_tmin [[min_distance]],
                      const float ray_tmax [[max_distance]])
{
  const uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  const int type = kernel_data_fetch(objects, object).primitive_type;

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

  metalrt_intersection_point(launch_params_metal,
                             payload,
                             object,
                             prim,
                             type,
                             ray_origin,
                             ray_direction,
#  if defined(__METALRT_MOTION__)
                             payload.time,
#  else
                             0.0f,
#  endif
                             ray_tmin,
                             ray_tmax,
                             result);

  return result;
}

[[intersection(bounding_box, triangle_data, METALRT_TAGS)]] BoundingBoxIntersectionResult
__intersection__point_shadow(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                             ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload
                             [[payload]],
                             const uint object [[user_instance_id]],
                             const uint primitive_id [[primitive_id]],
                             const float3 ray_origin [[origin]],
                             const float3 ray_direction [[direction]],
                             const float ray_tmin [[min_distance]],
                             const float ray_tmax [[max_distance]])
{
  const uint prim = primitive_id + kernel_data_fetch(object_prim_offset, object);
  const int type = kernel_data_fetch(objects, object).primitive_type;

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

  metalrt_intersection_point_shadow(launch_params_metal,
                                    payload,
                                    object,
                                    prim,
                                    type,
                                    ray_origin,
                                    ray_direction,
#  if defined(__METALRT_MOTION__)
                                    payload.time,
#  else
                                    0.0f,
#  endif
                                    ray_tmin,
                                    ray_tmax,
                                    result);

  return result;
}
#endif /* __POINTCLOUD__ */

/* Scene intersection. */

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          ccl_private const Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  if (!scene_intersect_valid(ray)) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;
    return false;
  }

#if defined(__KERNEL_DEBUG__)
  if (is_null_instance_acceleration_structure(metal_ancillaries->accel_struct)) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;
    kernel_assert(!"Invalid metal_ancillaries->accel_struct pointer");
    return false;
  }

  if (is_null_intersection_function_table(metal_ancillaries->ift_default)) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;
    kernel_assert(!"Invalid ift_default");
    return false;
  }
#endif

  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;

  if (!kernel_data.bvh.have_curves) {
    metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);
  }

  MetalRTIntersectionPayload payload;
  payload.self = ray->self;
  payload.u = 0.0f;
  payload.v = 0.0f;
  payload.visibility = visibility;

  typename metalrt_intersector_type::result_type intersection;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
    /* No further intersector setup required: Default MetalRT behavior is any-hit. */
  }
  else if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    /* No further intersector setup required: Shadow ray early termination is controlled by the
     * intersection handler */
  }

#if defined(__METALRT_MOTION__)
  payload.time = ray->time;
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             ray_mask,
                                             ray->time,
                                             metal_ancillaries->ift_default,
                                             payload);
#else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ray_mask, metal_ancillaries->ift_default, payload);
#endif

  if (intersection.type == intersection_type::none) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;

    return false;
  }

  isect->t = intersection.distance;

  isect->prim = payload.prim;
  isect->type = payload.type;
  isect->object = intersection.user_instance_id;

  isect->t = intersection.distance;
  if (intersection.type == intersection_type::triangle) {
    isect->u = 1.0f - intersection.triangle_barycentric_coord.y -
               intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.x;
  }
  else {
    isect->u = payload.u;
    isect->v = payload.v;
  }

  return isect->type != PRIMITIVE_NONE;
}

#ifdef __BVH_LOCAL__
ccl_device_intersect bool scene_intersect_local(KernelGlobals kg,
                                                ccl_private const Ray *ray,
                                                ccl_private LocalIntersection *local_isect,
                                                int local_object,
                                                ccl_private uint *lcg_state,
                                                int max_hits)
{
  if (!intersection_ray_valid(ray)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    return false;
  }

#  if defined(__KERNEL_DEBUG__)
  if (is_null_instance_acceleration_structure(metal_ancillaries->accel_struct)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    kernel_assert(!"Invalid metal_ancillaries->accel_struct pointer");
    return false;
  }

  if (is_null_intersection_function_table(metal_ancillaries->ift_local)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    kernel_assert(!"Invalid ift_local");
    return false;
  }
#  endif

  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;

  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  if (!kernel_data.bvh.have_curves) {
    metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);
  }

  MetalRTIntersectionLocalPayload payload;
  payload.self = ray->self;
  payload.local_object = local_object;
  payload.max_hits = max_hits;
  payload.local_isect.num_hits = 0;
  if (lcg_state) {
    payload.has_lcg_state = true;
    payload.lcg_state = *lcg_state;
  }
  payload.result = false;

  typename metalrt_intersector_type::result_type intersection;

#  if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, 0xFF, ray->time, metal_ancillaries->ift_local, payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, 0xFF, metal_ancillaries->ift_local, payload);
#  endif

  if (lcg_state) {
    *lcg_state = payload.lcg_state;
  }
  *local_isect = payload.local_isect;

  return payload.result;
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals kg,
                                                     IntegratorShadowState state,
                                                     ccl_private const Ray *ray,
                                                     uint visibility,
                                                     uint max_hits,
                                                     ccl_private uint *num_recorded_hits,
                                                     ccl_private float *throughput)
{
  if (!intersection_ray_valid(ray)) {
    return false;
  }

#  if defined(__KERNEL_DEBUG__)
  if (is_null_instance_acceleration_structure(metal_ancillaries->accel_struct)) {
    kernel_assert(!"Invalid metal_ancillaries->accel_struct pointer");
    return false;
  }

  if (is_null_intersection_function_table(metal_ancillaries->ift_shadow)) {
    kernel_assert(!"Invalid ift_shadow");
    return false;
  }
#  endif

  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;

  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  if (!kernel_data.bvh.have_curves) {
    metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);
  }

  MetalRTIntersectionShadowPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;
  payload.max_hits = max_hits;
  payload.num_hits = 0;
  payload.num_recorded_hits = 0;
  payload.throughput = 1.0f;
  payload.result = false;
  payload.state = state;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

  typename metalrt_intersector_type::result_type intersection;

#  if defined(__METALRT_MOTION__)
  payload.time = ray->time;
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             ray_mask,
                                             ray->time,
                                             metal_ancillaries->ift_shadow,
                                             payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ray_mask, metal_ancillaries->ift_shadow, payload);
#  endif

  *num_recorded_hits = payload.num_recorded_hits;
  *throughput = payload.throughput;

  return payload.result;
}
#endif

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 ccl_private const Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  if (!intersection_ray_valid(ray)) {
    return false;
  }

#  if defined(__KERNEL_DEBUG__)
  if (is_null_instance_acceleration_structure(metal_ancillaries->accel_struct)) {
    kernel_assert(!"Invalid metal_ancillaries->accel_struct pointer");
    return false;
  }

  if (is_null_intersection_function_table(metal_ancillaries->ift_default)) {
    kernel_assert(!"Invalid ift_default");
    return false;
  }
#  endif

  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;

  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  if (!kernel_data.bvh.have_curves) {
    metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);
  }

  MetalRTIntersectionPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;

  typename metalrt_intersector_type::result_type intersection;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

#  if defined(__METALRT_MOTION__)
  payload.time = ray->time;
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             ray_mask,
                                             ray->time,
                                             metal_ancillaries->ift_default,
                                             payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ray_mask, metal_ancillaries->ift_default, payload);
#  endif

  if (intersection.type == intersection_type::none) {
    return false;
  }

  isect->prim = payload.prim;
  isect->type = payload.type;
  isect->object = intersection.user_instance_id;

  isect->t = intersection.distance;
  if (intersection.type == intersection_type::triangle) {
    isect->u = 1.0f - intersection.triangle_barycentric_coord.y -
               intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.x;
  }
  else {
    isect->u = payload.u;
    isect->v = payload.v;
  }

  return isect->type != PRIMITIVE_NONE;
}
#endif

CCL_NAMESPACE_END

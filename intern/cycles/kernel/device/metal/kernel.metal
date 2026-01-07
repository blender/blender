/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Metal kernel entry points. */

/* NOTE: Must come prior to other includes. */
#include "kernel/device/metal/compat.h"
#include "kernel/device/metal/globals.h"

/* NOTE: Must come prior to the kernel.h. */
#include "kernel/device/metal/function_constants.h"

/* NOTE: Must come prior to the rest of the includes. */
#include "kernel/device/gpu/kernel.h"

/* The rest of the includes. */
#include "kernel/bvh/intersect_filter.h"
#include "kernel/geom/geom_intersect.h"

/* MetalRT intersection handlers. */

#ifdef __KERNEL_METALRT__

/* Intersection return types. */

/* For a bounding box intersection function. */
struct BoundingBoxIntersectionResult {
  bool accept [[accept_intersection]];
  bool continue_search [[continue_search]];
  float distance [[distance]];
};

/* For a primitive intersection function. */
struct PrimitiveIntersectionResult {
  bool accept [[accept_intersection]];
  bool continue_search [[continue_search]];
};

enum { METALRT_HIT_TRIANGLE, METALRT_HIT_CURVE, METALRT_HIT_BOUNDING_BOX };

/* Hit functions. */

[[intersection(triangle, triangle_data, curve_data)]] PrimitiveIntersectionResult
__intersection__local_tri_single_hit(
    ray_data MetalKernelContext::MetalRTIntersectionLocalPayload_single_hit &payload [[payload]],
    uint primitive_id [[primitive_id]])
{
  PrimitiveIntersectionResult result;
  result.continue_search = true;
  result.accept = (payload.self_prim != primitive_id);
  return result;
}

[[intersection(
    triangle, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__local_tri_single_hit_mblur(
    ray_data MetalKernelContext::MetalRTIntersectionLocalPayload_single_hit &payload [[payload]],
#  if defined(__METALRT_MOTION__)
    uint object [[instance_id]],
#  endif
    uint primitive_id [[primitive_id]])
{
  PrimitiveIntersectionResult result;
  result.continue_search = true;
#  if defined(__METALRT_MOTION__)
  result.accept = (payload.self_prim != primitive_id) && (payload.self_object == object);
#  else
  result.accept = (payload.self_prim != primitive_id);
#  endif
  return result;
}

template<typename TReturn, uint intersection_type>
TReturn metalrt_local_hit(constant KernelParamsMetal &launch_params_metal,
                          ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload,
                          const uint prim,
                          const float2 barycentrics,
                          const float ray_tmax)
{
  TReturn result;

#  ifdef __BVH_LOCAL__
  if (payload.self_prim == prim) {
    /* Only intersect with matching object and skip self-intersection. */
    result.accept = false;
    result.continue_search = true;
    return result;
  }

  const int max_hits = payload.max_hits;
  if (max_hits == 0) {
    /* Special case for when no hit information is requested, just report that something was hit.
     */
    result.accept = true;
    result.continue_search = false;
    return result;
  }

  /* Make a copty of the lcg_state in the private address space, allowing to use utility function
   * to find the hit index to write the intersection to. This function is used from both HW-RT
   * code-path and non-HW-RT, making it hard to deal with the address spaces in the function
   * signature. Hopefully, compiler is smart enough to eliminate this temporary copy. */
  uint lcg_state = payload.lcg_state;

  MetalKernelContext context(launch_params_metal);
  const int hit_index = context.local_intersect_get_record_index(
      &payload, ray_tmax, payload.has_lcg_state ? &lcg_state : nullptr, max_hits);

  payload.lcg_state = lcg_state;

  if (hit_index == -1) {
    result.accept = false;
    result.continue_search = true;
    return result;
  }

  payload.hits[hit_index].prim = prim;
  payload.hits[hit_index].t = ray_tmax;
  payload.hits[hit_index].u = barycentrics.x;
  payload.hits[hit_index].v = barycentrics.y;

  /* Continue tracing (without this the trace call would return after the first hit). */
  result.accept = false;
  result.continue_search = true;
#  endif
  return result;
}

[[intersection(triangle, triangle_data, curve_data)]] PrimitiveIntersectionResult
__intersection__local_tri(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                          ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload
                          [[payload]],
                          uint primitive_id [[primitive_id]],
                          float2 barycentrics [[barycentric_coord]],
                          float ray_tmax [[distance]])
{
  /* instance_id, aka the user_id has been removed. If we take this function we optimized the
   * SSS for starting traversal from a primitive acceleration structure instead of the root of the
   * global AS. this means we will always be intersecting the correct object no need for the
   * user-id to check */
  return metalrt_local_hit<PrimitiveIntersectionResult, METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, primitive_id, barycentrics, ray_tmax);
}

[[intersection(
    triangle, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__local_tri_mblur(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload [[payload]],
    uint primitive_id [[primitive_id]],
#  if defined(__METALRT_MOTION__)
    uint object [[instance_id]],
#  endif
    float2 barycentrics [[barycentric_coord]],
    float ray_tmax [[distance]])
{
#  if defined(__METALRT_MOTION__)
  if (payload.self_object != object) {
    PrimitiveIntersectionResult result;
    result.continue_search = true;
    result.accept = false;
    return result;
  }
#  endif

  return metalrt_local_hit<PrimitiveIntersectionResult, METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, primitive_id, barycentrics, ray_tmax);
}

inline bool metalrt_curve_skip_end_cap(const int type, const float u)
{
  return ((u == 0.0f || u == 1.0f) && (type & PRIMITIVE_CURVE) != PRIMITIVE_CURVE_THICK_LINEAR);
}

inline Intersection get_intersection(constant KernelParamsMetal &launch_params_metal,
                                     const float t,
                                     const float2 uv,
                                     uint object,
                                     uint prim)
{
  Intersection isect;
  isect.t = t;
  isect.u = uv.x;
  isect.v = uv.y;
  isect.prim = prim;
  isect.object = object;
  isect.type = kernel_data_fetch(objects, object).primitive_type;

#  ifdef __HAIR__
  if (isect.type & PRIMITIVE_CURVE) {
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    isect.type = segment.type;
    isect.prim = segment.prim;
  }
#  endif

  if (isect.type & PRIMITIVE_POINT) {
    isect.u = 0.0f;
    isect.v = 0.0f;
  }

  return isect;
}

template<uint intersection_type>
bool metalrt_shadow_all_hit(constant KernelParamsMetal &launch_params_metal,
                            ray_data MetalKernelContext::BVHShadowAllPayload &payload,
                            uint object,
                            uint prim,
                            const float2 uv,
                            const float t,
                            const ccl_private Ray *ray = nullptr)
{
#  if defined(__TRANSPARENT_SHADOWS__)
  MetalKernelContext context(launch_params_metal);

  KernelGlobals kg = nullptr;

  const Intersection isect = get_intersection(launch_params_metal, t, uv, object, prim);

#    ifdef __HAIR__
  if constexpr (intersection_type == METALRT_HIT_CURVE) {
    /* Filter out curve end-caps. */
    if (metalrt_curve_skip_end_cap(isect.type, isect.u)) {
      return true;
    }

    if ((isect.type & PRIMITIVE_CURVE) == PRIMITIVE_CURVE_RIBBON) {
      if (!context.curve_ribbon_accept(
              nullptr, isect.u, isect.t, ray, object, isect.prim, isect.type))
      {
        return true;
      }
    }
  }
#    endif /* __HAIR__ */

  constexpr uint enabled_primitive_types = (intersection_type == METALRT_HIT_CURVE) ?
                                               PRIMITIVE_CURVE :
                                               (PRIMITIVE_ALL & ~PRIMITIVE_CURVE);
  return context.bvh_shadow_all_anyhit_filter<true, enabled_primitive_types>(
      kg, payload.state, payload, payload.base.ray_self, payload.base.ray_visibility, isect);

#  else  /* __TRANSPARENT_SHADOWS__ */
  payload.throughput = 0.0f;
  return false;
#  endif /* __TRANSPARENT_SHADOWS__ */
}

[[intersection(
    triangle, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__tri_shadow_all(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                               ray_data MetalKernelContext::BVHShadowAllPayload &payload
                               [[payload]],
                               const unsigned int object [[instance_id]],
                               const unsigned int primitive_id [[primitive_id]],
                               const uint primitive_id_offset [[user_instance_id]],
                               const float2 uv [[barycentric_coord]],
                               const float t [[distance]])
{
  uint prim = primitive_id + primitive_id_offset;

  PrimitiveIntersectionResult result;
  result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, object, prim, uv, t);
  result.accept = !result.continue_search;
  return result;
}

[[intersection(
    triangle, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__volume_tri(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                           ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload
                           [[payload]],
                           const unsigned int object [[instance_id]],
                           const unsigned int primitive_id [[primitive_id]],
                           const uint primitive_id_offset [[user_instance_id]])
{
  PrimitiveIntersectionResult result;
  result.continue_search = true;

  KernelGlobals kg = nullptr;
  MetalKernelContext context(launch_params_metal);

  uint prim = primitive_id + primitive_id_offset;

  if (context.bvh_volume_anyhit_triangle_filter(
          kg, object, prim, payload.self, payload.visibility))
  {
    result.accept = false;
    return result;
  }

  result.accept = true;
  return result;
}

template<typename TReturnType, uint intersection_type>
inline TReturnType metalrt_visibility_test(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload,
    const uint object,
    uint prim,
    const float u,
    const float t = 0.0f,
    const ccl_private Ray *ray = nullptr)
{
  TReturnType result;

  if ((kernel_data_fetch(objects, object).visibility & payload.visibility) == 0) {
    result.accept = false;
    result.continue_search = true;
    return result;
  }

#  ifdef __HAIR__
  if constexpr (intersection_type == METALRT_HIT_CURVE) {
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    int type = segment.type;
    prim = segment.prim;

    /* Filter out curve end-caps. */
    if (metalrt_curve_skip_end_cap(type, u)) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }

    if ((type & PRIMITIVE_CURVE) == PRIMITIVE_CURVE_RIBBON) {
      MetalKernelContext context(launch_params_metal);
      if (!context.curve_ribbon_accept(nullptr, u, t, ray, object, prim, type)) {
        result.accept = false;
        result.continue_search = true;
        return result;
      }
    }
  }
#  endif

  if (payload.self_object == object && payload.self_prim == prim) {
    result.accept = false;
    result.continue_search = true;
    return result;
  }
  result.accept = true;
  result.continue_search = true;
  return result;
}

template<typename TReturnType, uint intersection_type>
inline TReturnType metalrt_visibility_test_shadow(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload,
    const uint object,
    uint prim,
    const float u,
    const float t = 0.0f,
    const ccl_private Ray *ray = nullptr)
{
  TReturnType result;

  if ((kernel_data_fetch(objects, object).visibility & payload.visibility) == 0) {
    result.accept = false;
    return result;
  }

#  ifdef __HAIR__
  if constexpr (intersection_type == METALRT_HIT_CURVE) {
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    int type = segment.type;
    prim = segment.prim;

    /* Filter out curve end-caps. */
    if (metalrt_curve_skip_end_cap(type, u)) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }

    if ((type & PRIMITIVE_CURVE) == PRIMITIVE_CURVE_RIBBON) {
      MetalKernelContext context(launch_params_metal);
      if (!context.curve_ribbon_accept(nullptr, u, t, ray, object, prim, type)) {
        result.accept = false;
        result.continue_search = true;
        return result;
      }
    }
  }
#  endif

  MetalKernelContext context(launch_params_metal);

  /* Shadow ray early termination. */
#  ifdef __SHADOW_LINKING__
  if (context.intersection_skip_shadow_link(nullptr, payload.self, object)) {
    result.accept = false;
    result.continue_search = true;
    return result;
  }
#  endif

  if (context.intersection_skip_self_shadow(payload.self, object, prim)) {
    result.accept = false;
    result.continue_search = true;
    return result;
  }
  else {
    result.accept = true;
    result.continue_search = false;
    return result;
  }

  result.accept = true;
  result.continue_search = true;
  return result;
}

[[intersection(
    triangle, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__tri(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
                    const unsigned int object [[instance_id]],
                    const uint primitive_id_offset [[user_instance_id]],
                    const unsigned int primitive_id [[primitive_id]])
{
  PrimitiveIntersectionResult result;
  result.continue_search = true;

  if ((kernel_data_fetch(objects, object).visibility & payload.visibility) == 0) {
    result.accept = false;
    return result;
  }

  result.accept = (payload.self_object != object ||
                   payload.self_prim != (primitive_id + primitive_id_offset));
  return result;
}

[[intersection(
    triangle, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__tri_shadow(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                           ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload
                           [[payload]],
                           const unsigned int object [[instance_id]],
                           const uint primitive_id_offset [[user_instance_id]],
                           const unsigned int primitive_id [[primitive_id]])
{
  uint prim = primitive_id + primitive_id_offset;
  PrimitiveIntersectionResult result =
      metalrt_visibility_test_shadow<PrimitiveIntersectionResult, METALRT_HIT_TRIANGLE>(
          launch_params_metal, payload, object, prim, 0.0f);
  return result;
}

/* Primitive intersection functions. */

[[intersection(
    curve, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__curve(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                      ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
                      const uint object [[instance_id]],
                      const uint primitive_id [[primitive_id]],
                      const uint primitive_id_offset [[user_instance_id]],
                      float distance [[distance]],
                      const float3 ray_P [[origin]],
                      const float3 ray_D [[direction]],
                      float u [[curve_parameter]],
                      const float ray_tmin [[min_distance]],
                      const float ray_tmax [[max_distance]]
#  if defined(__METALRT_MOTION__)
                      ,
                      const float time [[time]]
#  endif
)
{
  uint prim = primitive_id + primitive_id_offset;

  Ray ray;
  ray.P = ray_P;
  ray.D = ray_D;
#  if defined(__METALRT_MOTION__)
  ray.time = time;
#  endif

  PrimitiveIntersectionResult result =
      metalrt_visibility_test<PrimitiveIntersectionResult, METALRT_HIT_CURVE>(
          launch_params_metal, payload, object, prim, u, distance, &ray);

  return result;
}

[[intersection(
    curve, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__curve_shadow(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                             ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload
                             [[payload]],
                             const uint object [[instance_id]],
                             const uint primitive_id [[primitive_id]],
                             const uint primitive_id_offset [[user_instance_id]],
                             float distance [[distance]],
                             const float3 ray_P [[origin]],
                             const float3 ray_D [[direction]],
                             float u [[curve_parameter]],
                             const float ray_tmin [[min_distance]],
                             const float ray_tmax [[max_distance]]
#  if defined(__METALRT_MOTION__)
                             ,
                             const float time [[time]]
#  endif
)
{
  uint prim = primitive_id + primitive_id_offset;

  Ray ray;
  ray.P = ray_P;
  ray.D = ray_D;
#  if defined(__METALRT_MOTION__)
  ray.time = time;
#  endif

  PrimitiveIntersectionResult result =
      metalrt_visibility_test_shadow<PrimitiveIntersectionResult, METALRT_HIT_CURVE>(
          launch_params_metal, payload, object, prim, u, distance, &ray);

  return result;
}

[[intersection(
    curve, triangle_data, curve_data, METALRT_TAGS METALRT_LIMITS)]] PrimitiveIntersectionResult
__intersection__curve_shadow_all(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                                 ray_data MetalKernelContext::BVHShadowAllPayload &payload
                                 [[payload]],
                                 const uint object [[instance_id]],
                                 const uint primitive_id [[primitive_id]],
                                 const uint primitive_id_offset [[user_instance_id]],
                                 const float3 ray_P [[origin]],
                                 const float3 ray_D [[direction]],
#  if defined(__METALRT_MOTION__)
                                 const float time [[time]],
#  endif
                                 float u [[curve_parameter]],
                                 float t [[distance]])
{
  uint prim = primitive_id + primitive_id_offset;

  PrimitiveIntersectionResult result;

  Ray ray;
  ray.P = ray_P;
  ray.D = ray_D;
#  if defined(__METALRT_MOTION__)
  /* TODO(sergey): The time is not really needed.
   * Only ray direction and origin are needed in curve_ribbon_accept(), so there might be a room
   * for cleanup here. */
  ray.time = time;
#  endif

  result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_CURVE>(
      launch_params_metal, payload, object, prim, float2(u, 0), t, &ray);
  result.accept = !result.continue_search;

  return result;
}

#  ifdef __POINTCLOUD__
ccl_device_inline void metalrt_intersection_point_shadow_all(
    constant KernelParamsMetal &launch_params_metal,
    ray_data MetalKernelContext::BVHShadowAllPayload &payload,
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
  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          nullptr, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type))
  {
    result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, float2(isect.u, isect.v), isect.t);
    result.accept = !result.continue_search;

    if (result.accept) {
      result.distance = isect.t;
    }
  }
}

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS METALRT_LIMITS)]] BoundingBoxIntersectionResult
__intersection__point(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                      ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
                      const uint object [[instance_id]],
                      const uint primitive_id [[primitive_id]],
                      const uint primitive_id_offset [[user_instance_id]],
                      const float3 ray_origin [[origin]],
                      const float3 ray_direction [[direction]],
#    if defined(__METALRT_MOTION__)
                      const float time [[time]],
#    endif
                      const float ray_tmin [[min_distance]],
                      const float ray_tmax [[max_distance]])
{
  const uint prim = primitive_id + primitive_id_offset;
  const int type = kernel_data_fetch(objects, object).primitive_type;

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

  Intersection isect;
  isect.t = ray_tmax;

#    ifndef __METALRT_MOTION__
  const float time = 0.0f;
#    endif

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          nullptr, &isect, ray_origin, ray_direction, ray_tmin, isect.t, object, prim, time, type))
  {
    result = metalrt_visibility_test<BoundingBoxIntersectionResult, METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, isect.u);
    if (result.accept) {
      result.distance = isect.t;
    }
  }
  return result;
}

#  endif /* __POINTCLOUD__ */

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS METALRT_LIMITS)]] BoundingBoxIntersectionResult
__intersection__point_shadow(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                             ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload
                             [[payload]],
                             const uint object [[instance_id]],
                             const uint primitive_id [[primitive_id]],
                             const uint primitive_id_offset [[user_instance_id]],
                             const float3 ray_origin [[origin]],
                             const float3 ray_direction [[direction]],
#  if defined(__METALRT_MOTION__)
                             const float time [[time]],
#  endif
                             const float ray_tmin [[min_distance]],
                             const float ray_tmax [[max_distance]])
{
  const uint prim = primitive_id + primitive_id_offset;
  const int type = kernel_data_fetch(objects, object).primitive_type;

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

#  ifdef __POINTCLOUD__

  Intersection isect;
  isect.t = ray_tmax;

#    ifndef __METALRT_MOTION__
  const float time = 0.0f;
#    endif

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          nullptr, &isect, ray_origin, ray_direction, ray_tmin, isect.t, object, prim, time, type))
  {
    result =
        metalrt_visibility_test_shadow<BoundingBoxIntersectionResult, METALRT_HIT_BOUNDING_BOX>(
            launch_params_metal, payload, object, prim, isect.u);
    if (result.accept) {
      result.distance = isect.t;
    }
  }

#  endif /* __POINTCLOUD__ */

  return result;
}

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS METALRT_LIMITS)]] BoundingBoxIntersectionResult
__intersection__point_shadow_all(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                                 ray_data MetalKernelContext::BVHShadowAllPayload &payload
                                 [[payload]],
                                 const uint object [[instance_id]],
                                 const uint primitive_id [[primitive_id]],
                                 const uint primitive_id_offset [[user_instance_id]],
                                 const float3 ray_origin [[origin]],
                                 const float3 ray_direction [[direction]],
#  if defined(__METALRT_MOTION__)
                                 const float time [[time]],
#  endif
                                 const float ray_tmin [[min_distance]],
                                 const float ray_tmax [[max_distance]])
{
  const uint prim = primitive_id + primitive_id_offset;
  const int type = kernel_data_fetch(objects, object).primitive_type;

  BoundingBoxIntersectionResult result;
  result.accept = false;
  result.continue_search = true;
  result.distance = ray_tmax;

#  ifdef __POINTCLOUD__

  metalrt_intersection_point_shadow_all(launch_params_metal,
                                        payload,
                                        object,
                                        prim,
                                        type,
                                        ray_origin,
                                        ray_direction,
#    if defined(__METALRT_MOTION__)
                                        time,
#    else
                                        0.0f,
#    endif
                                        ray_tmin,
                                        ray_tmax,
                                        result);

#  endif /* __POINTCLOUD__ */

  return result;
}

#endif /* __KERNEL_METALRT__ */

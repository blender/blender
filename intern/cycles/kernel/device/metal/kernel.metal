/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Metal kernel entry points. */

/* NOTE: Must come prior to other includes. */
#include "kernel/device/metal/compat.h"
#include "kernel/device/metal/globals.h"

/* NOTE: Must come prior to the kernel.h. */
#include "kernel/device/metal/function_constants.h"

#include "kernel/device/gpu/kernel.h"

/* MetalRT intersection handlers. */

#ifdef __METALRT__

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

template<typename TReturn, uint intersection_type>
TReturn metalrt_local_hit(constant KernelParamsMetal &launch_params_metal,
                          ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload,
                          const uint object,
                          const uint prim,
                          const float2 barycentrics,
                          const float ray_tmax)
{
  TReturn result;

#  ifdef __BVH_LOCAL__
  MetalKernelContext context(launch_params_metal);

  if ((object != payload.local_object) || context.intersection_skip_self_local(payload.self, prim))
  {
    /* Only intersect with matching object and skip self-intersection. */
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

  isect->u = barycentrics.x;
  isect->v = barycentrics.y;

  /* Record geometric normal */
  const packed_uint3 tri_vindex = kernel_data_fetch(tri_vindex, isect->prim);
  const float3 tri_a = float3(kernel_data_fetch(tri_verts, tri_vindex.x));
  const float3 tri_b = float3(kernel_data_fetch(tri_verts, tri_vindex.y));
  const float3 tri_c = float3(kernel_data_fetch(tri_verts, tri_vindex.z));
  payload.local_isect.Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  /* Continue tracing (without this the trace call would return after the first hit) */
  result.accept = false;
  result.continue_search = true;
  return result;
#  endif
}

[[intersection(triangle, triangle_data, curve_data)]] PrimitiveIntersectionResult
__anyhit__cycles_metalrt_local_hit_tri_prim(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload [[payload]],
    uint primitive_id [[primitive_id]],
    float2 barycentrics [[barycentric_coord]],
    float ray_tmax [[distance]])
{
  uint prim = primitive_id + kernel_data_fetch(object_prim_offset, payload.local_object);

  /* instance_id, aka the user_id has been removed. If we take this function we optimized the
   * SSS for starting traversal from a primitive acceleration structure instead of the root of the
   * global AS. this means we will always be intersecting the correct object no need for the
   * user-id to check */
  return metalrt_local_hit<PrimitiveIntersectionResult, METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, payload.local_object, prim, barycentrics, ray_tmax);
}
[[intersection(triangle,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] PrimitiveIntersectionResult
__anyhit__cycles_metalrt_local_hit_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionLocalPayload &payload [[payload]],
    uint instance_id [[instance_id]],
    uint primitive_id [[primitive_id]],
    uint primitive_id_offset [[user_instance_id]],
    float2 barycentrics [[barycentric_coord]],
    float ray_tmax [[distance]])
{
  return metalrt_local_hit<PrimitiveIntersectionResult, METALRT_HIT_TRIANGLE>(
      launch_params_metal,
      payload,
      instance_id,
      primitive_id + primitive_id_offset,
      barycentrics,
      ray_tmax);
}

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_local_hit_box(const float ray_tmax [[max_distance]])
{
  /* unused function */
  BoundingBoxIntersectionResult result;
  result.distance = ray_tmax;
  result.accept = false;
  result.continue_search = false;
  return result;
}

[[intersection(bounding_box, triangle_data, curve_data)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_local_hit_box_prim(const float ray_tmax [[max_distance]])
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
                            const float ray_tmax,
                            const float t = 0.0f,
                            ccl_private const Ray *ray = NULL)
{
#  ifdef __SHADOW_RECORD_ALL__
  float u = barycentrics.x;
  float v = barycentrics.y;
  const int prim_type = kernel_data_fetch(objects, object).primitive_type;
  int type;

#    ifdef __HAIR__
  if constexpr (intersection_type == METALRT_HIT_CURVE) {
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    type = segment.type;
    prim = segment.prim;

    /* Filter out curve end-caps. */
    if (u == 0.0f || u == 1.0f) {
      /* continue search */
      return true;
    }

    if (type & PRIMITIVE_CURVE_RIBBON) {
      MetalKernelContext context(launch_params_metal);
      if (!context.curve_ribbon_accept(NULL, u, t, ray, object, prim, type)) {
        /* continue search */
        return true;
      }
    }
  }
#    endif

  if constexpr (intersection_type == METALRT_HIT_BOUNDING_BOX) {
    /* Point. */
    type = kernel_data_fetch(objects, object).primitive_type;
    u = 0.0f;
    v = 0.0f;
  }

  if constexpr (intersection_type == METALRT_HIT_TRIANGLE) {
    type = prim_type;
  }

  MetalKernelContext context(launch_params_metal);

  if (context.intersection_skip_self_shadow(payload.self, object, prim)) {
    /* continue search */
    return true;
  }

#    ifdef __SHADOW_LINKING__
  if (context.intersection_skip_shadow_link(nullptr, payload.self, object)) {
    /* continue search */
    return true;
  }
#    endif

#    ifndef __TRANSPARENT_SHADOWS__
  /* No transparent shadows support compiled in, make opaque. */
  payload.result = true;
  /* terminate ray */
  return false;
#    else
  short max_hits = payload.max_hits;
  short num_hits = payload.num_hits;
  short num_recorded_hits = payload.num_recorded_hits;

  /* If no transparent shadows, all light is blocked and we can stop immediately. */
  if (num_hits >= max_hits ||
      !(context.intersection_get_shader_flags(NULL, prim, type) & SD_HAS_TRANSPARENT_SHADOW))
  {
    payload.result = true;
    /* terminate ray */
    return false;
  }

#      ifdef __HAIR__
  /* Always use baked shadow transparency for curves. */
  if constexpr (intersection_type == METALRT_HIT_CURVE) {
    float throughput = payload.throughput;
    throughput *= context.intersection_curve_shadow_transparency(nullptr, object, prim, type, u);
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
#      endif

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
      /* Ray hits are not guaranteed to be ordered by distance so don't exit early here.
       * Continue search. */
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
#    endif /* __TRANSPARENT_SHADOWS__ */
#  endif   /* __SHADOW_RECORD_ALL__ */

  return true;
}

[[intersection(triangle,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] PrimitiveIntersectionResult
__anyhit__cycles_metalrt_shadow_all_hit_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload [[payload]],
    const unsigned int object [[instance_id]],
    const unsigned int primitive_id [[primitive_id]],
    const uint primitive_id_offset [[user_instance_id]],
    const float2 barycentrics [[barycentric_coord]],
    const float ray_tmax [[distance]])
{
  uint prim = primitive_id + primitive_id_offset;

  PrimitiveIntersectionResult result;
  result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_TRIANGLE>(
      launch_params_metal, payload, object, prim, barycentrics, ray_tmax);
  result.accept = !result.continue_search;
  return result;
}

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_shadow_all_hit_box(const float ray_tmax [[max_distance]])
{
  /* unused function */
  BoundingBoxIntersectionResult result;
  result.distance = ray_tmax;
  result.accept = false;
  result.continue_search = false;
  return result;
}

[[intersection(triangle,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] PrimitiveIntersectionResult
__anyhit__cycles_metalrt_volume_test_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
    const unsigned int object [[instance_id]],
    const unsigned int primitive_id [[primitive_id]],
    const uint primitive_id_offset [[user_instance_id]])
{
  PrimitiveIntersectionResult result;
  result.continue_search = true;

#  ifdef __VISIBILITY_FLAG__
  if ((kernel_data_fetch(objects, object).visibility & payload.visibility) == 0) {
    result.accept = false;
    return result;
  }
#  endif

  if ((kernel_data_fetch(object_flag, object) & SD_OBJECT_HAS_VOLUME) == 0) {
    result.accept = false;
    return result;
  }

  uint prim = primitive_id + primitive_id_offset;
  MetalKernelContext context(launch_params_metal);
  if (context.intersection_skip_self(payload.self, object, prim)) {
    result.accept = false;
    return result;
  }

  result.accept = true;
  return result;
}

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] BoundingBoxIntersectionResult
__anyhit__cycles_metalrt_volume_test_box(const float ray_tmax [[max_distance]])
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
    uint prim,
    const float u,
    const float t = 0.0f,
    ccl_private const Ray *ray = NULL)
{
  TReturnType result;

#  ifdef __HAIR__
  if constexpr (intersection_type == METALRT_HIT_CURVE) {
    /* Filter out curve end-caps. */
    if (u == 0.0f || u == 1.0f) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }

    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    int type = segment.type;
    prim = segment.prim;

    if (type & PRIMITIVE_CURVE_RIBBON) {
      MetalKernelContext context(launch_params_metal);
      if (!context.curve_ribbon_accept(NULL, u, t, ray, object, prim, type)) {
        result.accept = false;
        result.continue_search = true;
        return result;
      }
    }
  }
#  endif

  uint visibility = payload.visibility;

  MetalKernelContext context(launch_params_metal);

  /* Shadow ray early termination. */
  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
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
  }
  else {
    if (context.intersection_skip_self(payload.self, object, prim)) {
      result.accept = false;
      result.continue_search = true;
      return result;
    }
  }

  result.accept = true;
  result.continue_search = true;
  return result;
}

[[intersection(triangle,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] PrimitiveIntersectionResult
__anyhit__cycles_metalrt_visibility_test_tri(
    constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
    ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
    const unsigned int object [[instance_id]],
    const uint primitive_id_offset [[user_instance_id]],
    const unsigned int primitive_id [[primitive_id]])
{
  uint prim = primitive_id + primitive_id_offset;
  PrimitiveIntersectionResult result =
      metalrt_visibility_test<PrimitiveIntersectionResult, METALRT_HIT_TRIANGLE>(
          launch_params_metal, payload, object, prim, 0.0f);
  return result;
}

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] BoundingBoxIntersectionResult
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

[[intersection(
    curve, triangle_data, curve_data, METALRT_TAGS, extended_limits)]] PrimitiveIntersectionResult
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
    curve, triangle_data, curve_data, METALRT_TAGS, extended_limits)]] PrimitiveIntersectionResult
__intersection__curve_shadow(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                             ray_data MetalKernelContext::MetalRTIntersectionShadowPayload &payload
                             [[payload]],
                             const uint object [[instance_id]],
                             const uint primitive_id [[primitive_id]],
                             const uint primitive_id_offset [[user_instance_id]],
                             const float3 ray_P [[origin]],
                             const float3 ray_D [[direction]],
                             float u [[curve_parameter]],
                             float t [[distance]],
#  if defined(__METALRT_MOTION__)
                             const float time [[time]],
#  endif
                             const float ray_tmin [[min_distance]],
                             const float ray_tmax [[max_distance]])
{
  uint prim = primitive_id + primitive_id_offset;

  PrimitiveIntersectionResult result;

  Ray ray;
  ray.P = ray_P;
  ray.D = ray_D;
#  if defined(__METALRT_MOTION__)
  ray.time = time;
#  endif

  result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_CURVE>(
      launch_params_metal, payload, object, prim, float2(u, 0), ray_tmax, t, &ray);
  result.accept = !result.continue_search;

  return result;
}

#  ifdef __POINTCLOUD__
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
  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          NULL, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type))
  {
    result = metalrt_visibility_test<BoundingBoxIntersectionResult, METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, isect.u);
    if (result.accept) {
      result.distance = isect.t;
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
  Intersection isect;
  isect.t = ray_tmax;

  MetalKernelContext context(launch_params_metal);
  if (context.point_intersect(
          NULL, &isect, ray_P, ray_D, ray_tmin, isect.t, object, prim, time, type))
  {
    result.continue_search = metalrt_shadow_all_hit<METALRT_HIT_BOUNDING_BOX>(
        launch_params_metal, payload, object, prim, float2(isect.u, isect.v), ray_tmax);
    result.accept = !result.continue_search;

    if (result.accept) {
      result.distance = isect.t;
    }
  }
}

#  endif /* __POINTCLOUD__ */

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] BoundingBoxIntersectionResult
__intersection__point(constant KernelParamsMetal &launch_params_metal [[buffer(1)]],
                      ray_data MetalKernelContext::MetalRTIntersectionPayload &payload [[payload]],
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

  metalrt_intersection_point(launch_params_metal,
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

[[intersection(bounding_box,
               triangle_data,
               curve_data,
               METALRT_TAGS,
               extended_limits)]] BoundingBoxIntersectionResult
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

  metalrt_intersection_point_shadow(launch_params_metal,
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

#endif /* __METALRT__ */

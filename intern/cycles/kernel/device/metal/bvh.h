/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* MetalRT implementation of ray-scene intersection. */

#pragma once

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"

CCL_NAMESPACE_BEGIN

/* Payload types. */

struct MetalRTIntersectionPayload {
  RaySelfPrimitives self;
  uint visibility;
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
  int state;
  float throughput;
  short max_hits;
  short num_hits;
  short num_recorded_hits;
  bool result;
};

ccl_device_forceinline bool curve_ribbon_accept(
    KernelGlobals kg, float u, float t, ccl_private const Ray *ray, int object, int prim, int type)
{
  KernelCurve kcurve = kernel_data_fetch(curves, prim);

  int k0 = kcurve.first_key + PRIMITIVE_UNPACK_SEGMENT(type);
  int k1 = k0 + 1;
  int ka = max(k0 - 1, kcurve.first_key);
  int kb = min(k1 + 1, kcurve.first_key + kcurve.num_keys - 1);

  /* We can ignore motion blur here because we don't need the positions, and it doesn't affect the
   * radius. */
  float radius[4];
  radius[0] = kernel_data_fetch(curve_keys, ka).w;
  radius[1] = kernel_data_fetch(curve_keys, k0).w;
  radius[2] = kernel_data_fetch(curve_keys, k1).w;
  radius[3] = kernel_data_fetch(curve_keys, kb).w;
  const float r = metal::catmull_rom(u, radius[0], radius[1], radius[2], radius[3]);

  /* MPJ TODO: Can we ignore motion and/or object transforms here? Depends on scaling? */
  float3 ray_P = ray->P;
  float3 ray_D = ray->D;
  if (!(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED)) {
    float3 idir;
#if defined(__METALRT_MOTION__)
    bvh_instance_motion_push(NULL, object, ray, &ray_P, &ray_D, &idir);
#else
    bvh_instance_push(NULL, object, ray, &ray_P, &ray_D, &idir);
#endif
  }

  /* ignore self intersections */
  const float avoidance_factor = 2.0f;
  return t * len(ray_D) > avoidance_factor * r;
}

ccl_device_forceinline float curve_ribbon_v(
    KernelGlobals kg, float u, float t, ccl_private const Ray *ray, int object, int prim, int type)
{
#if defined(__METALRT_MOTION__)
  float time = ray->time;
#else
  float time = 0.0f;
#endif

  const bool is_motion = (type & PRIMITIVE_MOTION);

  KernelCurve kcurve = kernel_data_fetch(curves, prim);

  int k0 = kcurve.first_key + PRIMITIVE_UNPACK_SEGMENT(type);
  int k1 = k0 + 1;
  int ka = max(k0 - 1, kcurve.first_key);
  int kb = min(k1 + 1, kcurve.first_key + kcurve.num_keys - 1);

  float4 curve[4];
  if (!is_motion) {
    curve[0] = kernel_data_fetch(curve_keys, ka);
    curve[1] = kernel_data_fetch(curve_keys, k0);
    curve[2] = kernel_data_fetch(curve_keys, k1);
    curve[3] = kernel_data_fetch(curve_keys, kb);
  }
  else {
    motion_curve_keys(kg, object, prim, time, ka, k0, k1, kb, curve);
  }

  float3 ray_P = ray->P;
  float3 ray_D = ray->D;
  if (!(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED)) {
    float3 idir;
#if defined(__METALRT_MOTION__)
    bvh_instance_motion_push(NULL, object, ray, &ray_P, &ray_D, &idir);
#else
    bvh_instance_push(NULL, object, ray, &ray_P, &ray_D, &idir);
#endif
  }

  const float4 P_curve4 = metal::catmull_rom(u, curve[0], curve[1], curve[2], curve[3]);
  const float r_curve = P_curve4.w;

  float3 P = ray_P + ray_D * t;
  const float3 P_curve = float4_to_float3(P_curve4);

  const float4 dPdu4 = metal::catmull_rom_derivative(u, curve[0], curve[1], curve[2], curve[3]);
  const float3 dPdu = float4_to_float3(dPdu4);

  const float3 tangent = normalize(dPdu);
  const float3 bitangent = normalize(cross(tangent, -ray_D));

  float v = dot(P - P_curve, bitangent) / r_curve;
  return clamp(v, -1.0, 1.0f);
}

/* Scene intersection. */

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          ccl_private const Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  if (!intersection_ray_valid(ray)) {
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
  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
    metalrt_intersect.accept_any_intersection(true);
  }

  MetalRTIntersectionPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;

  typename metalrt_intersector_type::result_type intersection;

#if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             visibility,
                                             ray->time,
                                             metal_ancillaries->ift_default,
                                             payload);
#else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, visibility, metal_ancillaries->ift_default, payload);
#endif

  if (intersection.type == intersection_type::none) {
    isect->t = ray->tmax;
    isect->type = PRIMITIVE_NONE;

    return false;
  }

  isect->object = intersection.instance_id;
  isect->t = intersection.distance;
  if (intersection.type == intersection_type::triangle) {
    isect->prim = intersection.primitive_id + intersection.user_instance_id;
    isect->type = kernel_data_fetch(objects, intersection.instance_id).primitive_type;
    isect->u = intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.y;
  }
  else if (kernel_data.bvh.have_curves && intersection.type == intersection_type::curve) {
    int prim = intersection.primitive_id + intersection.user_instance_id;
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    isect->prim = segment.prim;
    isect->type = segment.type;
    isect->u = intersection.curve_parameter;

    if (segment.type & PRIMITIVE_CURVE_RIBBON) {
      isect->v = curve_ribbon_v(kg,
                                intersection.curve_parameter,
                                intersection.distance,
                                ray,
                                intersection.instance_id,
                                segment.prim,
                                segment.type);
    }
    else {
      isect->v = 0.0f;
    }
  }
  else if (kernel_data.bvh.have_points && intersection.type == intersection_type::bounding_box) {
    const int object = intersection.instance_id;
    const uint prim = intersection.primitive_id + intersection.user_instance_id;
    const int prim_type = kernel_data_fetch(objects, object).primitive_type;

    if (!(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED)) {
      float3 idir;
#if defined(__METALRT_MOTION__)
      bvh_instance_motion_push(NULL, object, ray, &r.origin, &r.direction, &idir);
#else
      bvh_instance_push(NULL, object, ray, &r.origin, &r.direction, &idir);
#endif
    }

    if (prim_type & PRIMITIVE_POINT) {
      if (!point_intersect(NULL,
                           isect,
                           r.origin,
                           r.direction,
                           ray->tmin,
                           ray->tmax,
                           object,
                           prim,
                           ray->time,
                           prim_type))
      {
        /* Shouldn't get here */
        kernel_assert(!"Intersection mismatch");
        isect->t = ray->tmax;
        isect->type = PRIMITIVE_NONE;
        return false;
      }
      return true;
    }
  }

  return true;
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
  if (is_null_intersection_function_table(metal_ancillaries->ift_local_prim)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    kernel_assert(!"Invalid ift_local_prim");
    return false;
  }
#  endif

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

  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);

#  if defined(__METALRT_MOTION__)
  metalrt_intersector_type metalrt_intersect;
  typename metalrt_intersector_type::result_type intersection;
#  else
  metalrt_blas_intersector_type metalrt_intersect;
  typename metalrt_blas_intersector_type::result_type intersection;
#  endif

  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  // if we know we are going to get max one hit, like for random-sss-walk we can
  // optimize and accept the first hit
  if (max_hits == 1) {
    metalrt_intersect.accept_any_intersection(true);
  }

#  if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ~0, ray->time, metal_ancillaries->ift_local, payload);
#  else
  if (!(kernel_data_fetch(object_flag, local_object) & SD_OBJECT_TRANSFORM_APPLIED)) {
    // transform the ray into object's local space
    Transform itfm = kernel_data_fetch(objects, local_object).itfm;
    r.origin = transform_point(&itfm, r.origin);
    r.direction = transform_direction(&itfm, r.direction);
  }

  intersection = metalrt_intersect.intersect(
      r,
      metal_ancillaries->blas_accel_structs[local_object].blas,
      metal_ancillaries->ift_local_prim,
      payload);
#  endif

  if (lcg_state) {
    *lcg_state = payload.lcg_state;
  }
  if (local_isect) {
    *local_isect = payload.local_isect;
  }

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
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  MetalRTIntersectionShadowPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;
  payload.max_hits = max_hits;
  payload.num_hits = 0;
  payload.num_recorded_hits = 0;
  payload.throughput = 1.0f;
  payload.result = false;
  payload.state = state;

  typename metalrt_intersector_type::result_type intersection;

#  if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             visibility,
                                             ray->time,
                                             metal_ancillaries->ift_shadow,
                                             payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, visibility, metal_ancillaries->ift_shadow, payload);
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
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  MetalRTIntersectionPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;

  typename metalrt_intersector_type::result_type intersection;

#  if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             visibility,
                                             ray->time,
                                             metal_ancillaries->ift_default,
                                             payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, visibility, metal_ancillaries->ift_default, payload);
#  endif

  if (intersection.type == intersection_type::none) {
    return false;
  }
  else if (intersection.type == intersection_type::triangle) {
    isect->prim = intersection.primitive_id + intersection.user_instance_id;
    isect->type = kernel_data_fetch(objects, intersection.instance_id).primitive_type;
    isect->u = intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.y;
    isect->object = intersection.instance_id;
    isect->t = intersection.distance;
  }
  else if (kernel_data.bvh.have_curves && intersection.type == intersection_type::curve) {
    int prim = intersection.primitive_id + intersection.user_instance_id;
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    isect->prim = segment.prim;
    isect->type = segment.type;
    isect->u = intersection.curve_parameter;

    if (segment.type & PRIMITIVE_CURVE_RIBBON) {
      isect->v = curve_ribbon_v(kg,
                                intersection.curve_parameter,
                                intersection.distance,
                                ray,
                                intersection.instance_id,
                                segment.prim,
                                segment.type);
    }
    else {
      isect->v = 0.0f;
    }
  }
  else if (kernel_data.bvh.have_points && intersection.type == intersection_type::bounding_box) {
    const int object = intersection.instance_id;
    const uint prim = intersection.primitive_id + intersection.user_instance_id;
    const int prim_type = kernel_data_fetch(objects, intersection.instance_id).primitive_type;

    isect->object = object;

    if (!(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED)) {
      float3 idir;
#  if defined(__METALRT_MOTION__)
      bvh_instance_motion_push(NULL, object, ray, &r.origin, &r.direction, &idir);
#  else
      bvh_instance_push(NULL, object, ray, &r.origin, &r.direction, &idir);
#  endif
    }

    if (prim_type & PRIMITIVE_POINT) {
      if (!point_intersect(NULL,
                           isect,
                           r.origin,
                           r.direction,
                           ray->tmin,
                           ray->tmax,
                           intersection.instance_id,
                           prim,
                           ray->time,
                           prim_type))
      {
        /* Shouldn't get here */
        kernel_assert(!"Intersection mismatch");
        return false;
      }
      return true;
    }
  }

  return true;
}
#endif

CCL_NAMESPACE_END

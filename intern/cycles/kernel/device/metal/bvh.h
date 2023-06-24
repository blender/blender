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

  bool triangle_only = !kernel_data.bvh.have_curves && !kernel_data.bvh.have_points;
  if (triangle_only) {
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
    isect->u = intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.y;
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

  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  bool triangle_only = !kernel_data.bvh.have_curves && !kernel_data.bvh.have_points;
  if (triangle_only) {
    metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);
  }

  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, 0xFF, ray->time, metal_ancillaries->ift_local, payload);
#  else

  metalrt_blas_intersector_type metalrt_intersect;
  typename metalrt_blas_intersector_type::result_type intersection;

  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  bool triangle_only = !kernel_data.bvh.have_curves && !kernel_data.bvh.have_points;
  if (triangle_only) {
    metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);
  }

  // if we know we are going to get max one hit, like for random-sss-walk we can
  // optimize and accept the first hit
  if (max_hits == 1) {
    metalrt_intersect.accept_any_intersection(true);
  }

  int blas_index = metal_ancillaries->blas_userID_to_index_lookUp[local_object];

  if (!(kernel_data_fetch(object_flag, local_object) & SD_OBJECT_TRANSFORM_APPLIED)) {
    // transform the ray into object's local space
    Transform itfm = kernel_data_fetch(objects, local_object).itfm;
    r.origin = transform_point(&itfm, r.origin);
    r.direction = transform_direction(&itfm, r.direction);
  }

  intersection = metalrt_intersect.intersect(
      r,
      metal_ancillaries->blas_accel_structs[blas_index].blas,
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

  bool triangle_only = !kernel_data.bvh.have_curves && !kernel_data.bvh.have_points;
  if (triangle_only) {
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

  bool triangle_only = !kernel_data.bvh.have_curves && !kernel_data.bvh.have_points;
  if (triangle_only) {
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
    isect->u = intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.y;
  }
  else {
    isect->u = payload.u;
    isect->v = payload.v;
  }

  return isect->type != PRIMITIVE_NONE;
}
#endif

CCL_NAMESPACE_END

/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* MetalRT implementation of ray-scene intersection. */

#pragma once

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"

CCL_NAMESPACE_BEGIN

/* Payload types.
 *
 * Best practice is to minimize the size of MetalRT payloads to avoid heavy spilling during
 * intersection tests.
 */

struct MetalRTIntersectionPayload {
  int self_prim;
  int self_object;
  uint visibility;
};

struct MetalRTIntersectionLocalPayload_single_hit {
  int self_prim;
#if defined(__METALRT_MOTION__)
  int self_object;
#endif
};

struct MetalRTIntersectionLocalPayload {
  int self_prim;
#if defined(__METALRT_MOTION__)
  int self_object;
#endif
  uint lcg_state;
  uint hit_prim[LOCAL_MAX_HITS];
  float hit_t[LOCAL_MAX_HITS];
  float hit_u[LOCAL_MAX_HITS];
  float hit_v[LOCAL_MAX_HITS];
  uint max_hits : 3;
  uint num_hits : 3;
  uint has_lcg_state : 1;
};
static_assert(LOCAL_MAX_HITS < 8,
              "MetalRTIntersectionLocalPayload max_hits & num_hits bitfields are too small");

struct MetalRTIntersectionShadowPayload {
  RaySelfPrimitives self;
  uint visibility;
};

struct MetalRTIntersectionShadowAllPayload {
  RaySelfPrimitives self;
  uint visibility;
  int state;
  float throughput;
  short max_transparent_hits;
  short num_transparent_hits;
  short num_recorded_hits;
  bool result;
};

#ifdef __HAIR__
ccl_device_forceinline bool curve_ribbon_accept(KernelGlobals kg,
                                                const float u,
                                                float t,
                                                const ccl_private Ray *ray,
                                                const int object,
                                                const int prim,
                                                const int type)
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
#  if defined(__METALRT_MOTION__)
    bvh_instance_motion_push(nullptr, object, ray, &ray_P, &ray_D, &idir);
#  else
    bvh_instance_push(nullptr, object, ray, &ray_P, &ray_D, &idir);
#  endif
  }

  /* ignore self intersections */
  const float avoidance_factor = 2.0f;
  return t * len(ray_D) > avoidance_factor * r;
}

ccl_device_forceinline float curve_ribbon_v(KernelGlobals kg,
                                            const float u,
                                            float t,
                                            const ccl_private Ray *ray,
                                            const int object,
                                            const int prim,
                                            const int type)
{
#  if defined(__METALRT_MOTION__)
  float time = ray->time;
#  else
  float time = 0.0f;
#  endif

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
    motion_curve_keys(kg, object, time, ka, k0, k1, kb, curve);
  }

  float3 ray_P = ray->P;
  float3 ray_D = ray->D;
  if (!(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED)) {
    float3 idir;
#  if defined(__METALRT_MOTION__)
    bvh_instance_motion_push(nullptr, object, ray, &ray_P, &ray_D, &idir);
#  else
    bvh_instance_push(nullptr, object, ray, &ray_P, &ray_D, &idir);
#  endif
  }

  const float4 P_curve4 = metal::catmull_rom(u, curve[0], curve[1], curve[2], curve[3]);
  const float r_curve = P_curve4.w;

  float3 P = ray_P + ray_D * t;
  const float3 P_curve = make_float3(P_curve4);

  const float4 dPdu4 = metal::catmull_rom_derivative(u, curve[0], curve[1], curve[2], curve[3]);
  const float3 dPdu = make_float3(dPdu4);

  const float3 tangent = normalize(dPdu);
  const float3 bitangent = normalize(cross(tangent, -ray_D));

  float v = dot(P - P_curve, bitangent) / r_curve;
  return clamp(v, -1.0, 1.0f);
}
#endif /* __HAIR__ */

/* Scene intersection. */

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          const ccl_private Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;
  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  typename metalrt_intersector_type::result_type intersection;

  MetalRTIntersectionPayload payload;
  payload.self_prim = ray->self.prim;
  payload.self_object = ray->self.object;
  payload.visibility = visibility;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

#if defined(__METALRT_MOTION__)
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

  isect->object = intersection.instance_id;
  isect->t = intersection.distance;
  if (intersection.type == intersection_type::triangle) {
    isect->prim = intersection.primitive_id + intersection.user_instance_id;
    isect->type = kernel_data_fetch(objects, intersection.instance_id).primitive_type;
    isect->u = intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.y;
  }
#ifdef __HAIR__
  else if (kernel_data.bvh.have_curves && intersection.type == intersection_type::curve) {
    int prim = intersection.primitive_id + intersection.user_instance_id;
    const KernelCurveSegment segment = kernel_data_fetch(curve_segments, prim);
    isect->prim = segment.prim;
    isect->type = segment.type;
    isect->u = intersection.curve_parameter;

    if ((segment.type & PRIMITIVE_CURVE) == PRIMITIVE_CURVE_RIBBON) {
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
#endif /* __HAIR__ */
#ifdef __POINTCLOUD__
  else if (kernel_data.bvh.have_points && intersection.type == intersection_type::bounding_box) {
    const int object = intersection.instance_id;
    const uint prim = intersection.primitive_id + intersection.user_instance_id;
    const int prim_type = kernel_data_fetch(objects, object).primitive_type;

    if (!(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED)) {
      float3 idir;
#  if defined(__METALRT_MOTION__)
      bvh_instance_motion_push(nullptr, object, ray, &r.origin, &r.direction, &idir);
#  else
      bvh_instance_push(nullptr, object, ray, &r.origin, &r.direction, &idir);
#  endif
    }

    if (prim_type & PRIMITIVE_POINT) {
      if (!point_intersect(nullptr,
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
#endif /* __POINTCLOUD__ */

  return true;
}

ccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 const uint visibility)
{
  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;
  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  typename metalrt_intersector_type::result_type intersection;

  metalrt_intersect.accept_any_intersection(true);

  MetalRTIntersectionShadowPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

#if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             ray_mask,
                                             ray->time,
                                             metal_ancillaries->ift_shadow,
                                             payload);
#else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ray_mask, metal_ancillaries->ift_shadow, payload);
#endif
  return (intersection.type != intersection_type::none);
}

#ifdef __BVH_LOCAL__
template<bool single_hit = false>
ccl_device_intersect bool scene_intersect_local(KernelGlobals kg,
                                                const ccl_private Ray *ray,
                                                ccl_private LocalIntersection *local_isect,
                                                const int local_object,
                                                ccl_private uint *lcg_state,
                                                const int max_hits)
{
  uint primitive_id_offset = kernel_data_fetch(object_prim_offset, local_object);

  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);

#  if defined(__METALRT_MOTION__)
  metalrt_intersector_type metalrt_intersect;
  typename metalrt_intersector_type::result_type intersection;
#  else
  metalrt_blas_intersector_type metalrt_intersect;
  typename metalrt_blas_intersector_type::result_type intersection;

  if (!(kernel_data_fetch(object_flag, local_object) & SD_OBJECT_TRANSFORM_APPLIED)) {
    /* Transform the ray into object's local space. */
    Transform itfm = kernel_data_fetch(objects, local_object).itfm;
    r.origin = transform_point(&itfm, r.origin);
    r.direction = transform_direction(&itfm, r.direction);
  }
#  endif

  metalrt_intersect.assume_geometry_type(metal::raytracing::geometry_type::triangle);

  if (single_hit) {
    MetalRTIntersectionLocalPayload_single_hit payload;
    payload.self_prim = ray->self.prim - primitive_id_offset;

#  if defined(__METALRT_MOTION__)
    /* We can't skip over the top-level BVH in the motion blur case, so still need to do
     * the self-object check. */
    payload.self_object = local_object;
    metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
    intersection = metalrt_intersect.intersect(r,
                                               metal_ancillaries->accel_struct,
                                               ~0,
                                               ray->time,
                                               metal_ancillaries->ift_local_single_hit_mblur,
                                               payload);
#  else
    /* We only need custom intersection filtering (i.e. non_opaque) if we are performing a
     * self-primitive intersection check. */
    metalrt_intersect.force_opacity((ray->self.prim == PRIM_NONE) ?
                                        metal::raytracing::forced_opacity::opaque :
                                        metal::raytracing::forced_opacity::non_opaque);
    intersection = metalrt_intersect.intersect(
        r,
        metal_ancillaries->blas_accel_structs[local_object].blas,
        metal_ancillaries->ift_local_single_hit,
        payload);
#  endif

    if (intersection.type == intersection_type::none) {
      local_isect->num_hits = 0;
      return false;
    }

    uint prim = intersection.primitive_id + primitive_id_offset;
    int prim_type = kernel_data_fetch(objects, local_object).primitive_type;

    local_isect->num_hits = 1;
    local_isect->hits[0].prim = prim;
    local_isect->hits[0].type = prim_type;
    local_isect->hits[0].object = local_object;
    local_isect->hits[0].u = intersection.triangle_barycentric_coord.x;
    local_isect->hits[0].v = intersection.triangle_barycentric_coord.y;
    local_isect->hits[0].t = intersection.distance;

    const packed_uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
    const float3 tri_a = float3(kernel_data_fetch(tri_verts, tri_vindex.x));
    const float3 tri_b = float3(kernel_data_fetch(tri_verts, tri_vindex.y));
    const float3 tri_c = float3(kernel_data_fetch(tri_verts, tri_vindex.z));
    local_isect->Ng[0] = normalize(cross(tri_b - tri_a, tri_c - tri_a));
    return true;
  }
  else {
    MetalRTIntersectionLocalPayload payload;
    payload.self_prim = ray->self.prim - primitive_id_offset;
    payload.max_hits = max_hits;
    payload.num_hits = 0;
    if (lcg_state) {
      payload.has_lcg_state = 1;
      payload.lcg_state = *lcg_state;
    }

    metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);

#  if defined(__METALRT_MOTION__)
    /* We can't skip over the top-level BVH in the motion blur case, so still need to do
     * the self-object check. */
    payload.self_object = local_object;
    intersection = metalrt_intersect.intersect(r,
                                               metal_ancillaries->accel_struct,
                                               ~0,
                                               ray->time,
                                               metal_ancillaries->ift_local_mblur,
                                               payload);
#  else
    intersection = metalrt_intersect.intersect(
        r,
        metal_ancillaries->blas_accel_structs[local_object].blas,
        metal_ancillaries->ift_local,
        payload);
#  endif

    if (max_hits == 0) {
      /* Special case for when no hit information is requested, just report that something was hit
       */
      return (intersection.type != intersection_type::none);
    }

    if (lcg_state) {
      *lcg_state = payload.lcg_state;
    }

    const int num_hits = payload.num_hits;
    if (local_isect) {

      /* Record geometric normal */
      int prim_type = kernel_data_fetch(objects, local_object).primitive_type;

      local_isect->num_hits = num_hits;
      for (int hit = 0; hit < num_hits; hit++) {
        uint prim = payload.hit_prim[hit] + primitive_id_offset;
        local_isect->hits[hit].prim = prim;
        local_isect->hits[hit].t = payload.hit_t[hit];
        local_isect->hits[hit].u = payload.hit_u[hit];
        local_isect->hits[hit].v = payload.hit_v[hit];
        local_isect->hits[hit].object = local_object;
        local_isect->hits[hit].type = prim_type;

        const packed_uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
        const float3 tri_a = float3(kernel_data_fetch(tri_verts, tri_vindex.x));
        const float3 tri_b = float3(kernel_data_fetch(tri_verts, tri_vindex.y));
        const float3 tri_c = float3(kernel_data_fetch(tri_verts, tri_vindex.z));
        local_isect->Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));
      }
    }
    return num_hits > 0;
  }
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals kg,
                                                     IntegratorShadowState state,
                                                     const ccl_private Ray *ray,
                                                     const uint visibility,
                                                     const uint max_transparent_hits,
                                                     ccl_private uint *num_recorded_hits,
                                                     ccl_private float *throughput)
{
  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;
  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  MetalRTIntersectionShadowAllPayload payload;
  payload.self = ray->self;
  payload.max_transparent_hits = max_transparent_hits;
  payload.num_transparent_hits = 0;
  payload.num_recorded_hits = 0;
  payload.throughput = 1.0f;
  payload.result = false;
  payload.state = state;
  payload.visibility = visibility;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

  typename metalrt_intersector_type::result_type intersection;

#  if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             ray_mask,
                                             ray->time,
                                             metal_ancillaries->ift_shadow_all,
                                             payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ray_mask, metal_ancillaries->ift_shadow_all, payload);
#  endif

  *num_recorded_hits = payload.num_recorded_hits;
  *throughput = payload.throughput;

  return payload.result;
}
#endif

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  metal::raytracing::ray r(ray->P, ray->D, ray->tmin, ray->tmax);
  metalrt_intersector_type metalrt_intersect;
  metalrt_intersect.force_opacity(metal::raytracing::forced_opacity::non_opaque);
  metalrt_intersect.set_geometry_cull_mode(metal::raytracing::geometry_cull_mode::bounding_box |
                                           metal::raytracing::geometry_cull_mode::curve);
  metalrt_intersect.assume_geometry_type(
      metal::raytracing::geometry_type::triangle |
      (kernel_data.bvh.have_curves ? metal::raytracing::geometry_type::curve :
                                     metal::raytracing::geometry_type::none) |
      (kernel_data.bvh.have_points ? metal::raytracing::geometry_type::bounding_box :
                                     metal::raytracing::geometry_type::none));

  MetalRTIntersectionShadowPayload payload;
  payload.self = ray->self;
  payload.visibility = visibility;

  uint ray_mask = visibility & 0xFF;
  if (0 == ray_mask && (visibility & ~0xFF) != 0) {
    ray_mask = 0xFF;
  }

  typename metalrt_intersector_type::result_type intersection;

#  if defined(__METALRT_MOTION__)
  intersection = metalrt_intersect.intersect(r,
                                             metal_ancillaries->accel_struct,
                                             ray_mask,
                                             ray->time,
                                             metal_ancillaries->ift_volume,
                                             payload);
#  else
  intersection = metalrt_intersect.intersect(
      r, metal_ancillaries->accel_struct, ray_mask, metal_ancillaries->ift_volume, payload);
#  endif

  if (intersection.type == intersection_type::triangle) {
    isect->prim = intersection.primitive_id + intersection.user_instance_id;
    isect->type = kernel_data_fetch(objects, intersection.instance_id).primitive_type;
    isect->u = intersection.triangle_barycentric_coord.x;
    isect->v = intersection.triangle_barycentric_coord.y;
    isect->object = intersection.instance_id;
    isect->t = intersection.distance;
    return true;
  }
  return false;
}
#endif

CCL_NAMESPACE_END

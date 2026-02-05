/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/integrator/path_state.h"

#include "kernel/bvh/bvh.h"

#include "kernel/sample/mapping.h"

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

#ifdef __SHADER_RAYTRACE__

struct RaycastResult {
  float distance;
  float3 normal;
  bool self_hit;
};

ccl_device RaycastResult svm_raycast(KernelGlobals kg,
                                     ConstIntegratorState /*state*/,
                                     ccl_private ShaderData *sd,
                                     float3 position,
                                     float3 direction,
                                     float distance,
                                     bool only_local)
{
  RaycastResult result;
  result.distance = -1.0f;
  result.normal = make_float3(0.0f);
  result.self_hit = false;

  /* Early out if no sampling needed. */
  if (distance <= 0.0f || sd->object == OBJECT_NONE) {
    return result;
  }

  /* Can't ray-trace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return result;
  }

  const bool avoid_self_intersection = isequal(position, sd->P);

  /* Create ray. */
  Ray ray;
  ray.P = position;
  ray.D = direction;
  ray.tmin = 0.0f;
  ray.tmax = distance;
  ray.time = sd->time;
  ray.self.object = avoid_self_intersection ? sd->object : OBJECT_NONE;
  ray.self.prim = avoid_self_intersection ? sd->prim : PRIM_NONE;
  ray.self.light_object = OBJECT_NONE;
  ray.self.light_prim = PRIM_NONE;
  ray.dP = differential_zero_compact();
  ray.dD = differential_zero_compact();

  Intersection isect;
  if (only_local) {
    LocalIntersection local_isect;
    scene_intersect_local(kg, &ray, &local_isect, sd->object, nullptr, 1);
    if (local_isect.num_hits == 0) {
      return result;
    }
    isect = local_isect.hits[0];
  }
  else {
    /* Ray-trace, leaving out shadow opaque to avoid early exit. */
    const uint visibility = PATH_RAY_ALL_VISIBILITY - PATH_RAY_SHADOW_OPAQUE;
    if (!scene_intersect(kg, &ray, visibility, &isect)) {
      return result;
    }
  }

  result.distance = isect.t;

  /* Get geometric normal. */
  const int object = isect.object;
  const uint object_flag = kernel_data_fetch(object_flag, object);
  const int prim = isect.prim;
  const float u = isect.u;
  const float v = isect.v;

  result.self_hit = object == sd->object;

  float3 P;
  float3 Ng;
  int shader;

  triangle_point_normal(kg, object, prim, u, v, &P, &Ng, &shader);

  /* Compute smooth normal. */
  if (shader & SHADER_SMOOTH_NORMAL) {
    if (isect.type == PRIMITIVE_TRIANGLE) {
      result.normal = triangle_smooth_normal(kg, Ng, prim, u, v);
    }
#  ifdef __OBJECT_MOTION__
    else if (isect.type == PRIMITIVE_MOTION_TRIANGLE) {
      result.normal = motion_triangle_smooth_normal(kg, Ng, object, prim, u, v, sd->time);
    }
#  endif /* __OBJECT_MOTION__ */
  }
  else {
    result.normal = Ng;
  }

  /* Transform normals to world space. */
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    object_normal_transform(kg, sd, &result.normal);
  }

  return result;
}

template<uint node_feature_mask, typename ConstIntegratorGenericState>
#  if defined(__KERNEL_OPTIX__)
ccl_device_inline
#  else
ccl_device_noinline
#  endif
    void
    svm_node_raycast(KernelGlobals kg,
                     ConstIntegratorGenericState state,
                     ccl_private ShaderData *sd,
                     ccl_private float *stack,
                     const uint4 node)
{
  uint position_offset;
  uint direction_offset;
  uint distance_offset;
  uint is_hit_offset;
  svm_unpack_node_uchar4(
      node.y, &position_offset, &direction_offset, &distance_offset, &is_hit_offset);

  uint is_self_hit_offset;
  uint hit_distance_offset;
  uint hit_position_offset;
  uint hit_normal_offset;
  svm_unpack_node_uchar4(
      node.z, &is_self_hit_offset, &hit_distance_offset, &hit_position_offset, &hit_normal_offset);

  float distance = stack_load_float_default(stack, distance_offset, 0.0f);

  float is_hit = 0.0f;
  float is_self_hit = 0.0f;
  float hit_distance = distance;
  float3 hit_position = make_float3(0.0f);
  float3 hit_normal = make_float3(0.0f);

  IF_KERNEL_NODES_FEATURE(RAYTRACE)
  {
    uint only_local = node.w;

    float3 position = stack_load_float3(stack, position_offset);
    float3 direction = stack_load_float3(stack, direction_offset);
    RaycastResult result = svm_raycast(kg, state, sd, position, direction, distance, only_local);

    if (result.distance >= 0.0f) {
      is_hit = 1.0f;
      is_self_hit = result.self_hit ? 1.0f : 0.0f;
      hit_distance = result.distance;
      hit_position = position + direction * hit_distance;
      hit_normal = result.normal;
    }
  }

  if (stack_valid(is_hit_offset)) {
    stack_store_float(stack, is_hit_offset, is_hit);
  }
  if (stack_valid(is_self_hit_offset)) {
    stack_store_float(stack, is_self_hit_offset, is_self_hit);
  }
  if (stack_valid(hit_distance_offset)) {
    stack_store_float(stack, hit_distance_offset, hit_distance);
  }
  if (stack_valid(hit_position_offset)) {
    stack_store_float3(stack, hit_position_offset, hit_position);
  }
  if (stack_valid(hit_normal_offset)) {
    stack_store_float3(stack, hit_normal_offset, hit_normal);
  }
}

#endif /* __SHADER_RAYTRACE__ */

CCL_NAMESPACE_END

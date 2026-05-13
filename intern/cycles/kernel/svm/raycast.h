/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/integrator/path_state.h"

#include "kernel/bvh/bvh.h"

#include "kernel/sample/mapping.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "kernel/geom/shader_data.h"

CCL_NAMESPACE_BEGIN

#ifdef __SHADER_RAYTRACE__

ccl_device bool svm_raycast(KernelGlobals kg,
                            ConstIntegratorState /*state*/,
                            ccl_private ShaderData *sd,
                            const float3 position,
                            const float3 direction,
                            const float distance,
                            const bool only_local,
                            const float bump_filter_width,
                            ccl_private ShaderData &hit_sd)
{
  /* Early out if no sampling needed. */
  if (distance <= 0.0f || sd->object == OBJECT_NONE) {
    return false;
  }

  /* Can't ray-trace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return false;
  }

  float tmin = 0.0f;
  bool avoid_self_intersection = false;
  if (bump_filter_width > 0.0f) {
    /* If evaluating for bump mapping at a shifted position, increase min distance by slightly more
     * than the shift distance to avoid self intersections. */
    tmin = bump_filter_width * sd->dP * 1.1f;
  }
  else {
    avoid_self_intersection = isequal(position, sd->P);
  }

  /* Create ray. */
  Ray ray;
  ray.P = position;
  ray.D = direction;
  ray.tmin = tmin;
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
      return false;
    }
    isect = local_isect.hits[0];
  }
  else {
    /* Ray-trace, leaving out shadow opaque to avoid early exit. */
    const uint visibility = PATH_RAY_ALL_VISIBILITY - PATH_RAY_SHADOW_OPAQUE;
    if (!scene_intersect(kg, &ray, visibility, &isect)) {
      return false;
    }
  }

  shader_setup_from_ray(kg, &hit_sd, &ray, &isect);

  return true;
}

ccl_device_inline void svm_raycast_attr_eval_and_store(
    KernelGlobals kg,
    ccl_private float *stack,
    ccl_global const SVMNodeAttr &attribute_node,
    ccl_private ShaderData &hit_sd)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  const AttributeDescriptor desc = svm_node_attr_init(kg, &hit_sd, attribute_node, &type);

  const float3 data = svm_node_attr_surface_eval<float3>(kg, &hit_sd, attribute_node, type, desc);
  svm_node_attr_store(type, stack, attribute_node.out_offset, data);
}

template<uint node_feature_mask, typename ConstIntegratorGenericState>
#  if defined(__KERNEL_OPTIX__)
ccl_device_inline
#  else
ccl_device_noinline
#  endif
    int
    svm_node_raycast(KernelGlobals kg,
                     ConstIntegratorGenericState state,
                     ccl_private ShaderData *sd,
                     ccl_private float *ccl_restrict stack,
                     const ccl_global SVMNodeRaycast &ccl_restrict node,
                     int offset)
{
  const float distance = stack_load(stack, node.distance);

  float is_hit = 0.0f;
  float is_self_hit = 0.0f;
  float hit_distance = distance;
  float3 hit_position = make_float3(0.0f);
  float3 hit_normal = make_float3(0.0f);

  IF_KERNEL_NODES_FEATURE(RAYTRACE)
  {
    const float3 position = stack_load(stack, node.position);
    const float3 direction = stack_load(stack, node.direction);

    ShaderDataTinyStorage hit_sd_storage;
    ccl_private ShaderData &hit_sd = *AS_SHADER_DATA(&hit_sd_storage);

    if (svm_raycast(kg,
                    state,
                    sd,
                    position,
                    direction,
                    distance,
                    node.only_local,
                    node.bump_filter_width,
                    hit_sd))
    {
      is_hit = 1.0f;
      is_self_hit = (sd->object == hit_sd.object) ? 1.0f : 0.0f;
      hit_distance = hit_sd.ray_length;
      hit_position = position + direction * hit_distance;
      hit_normal = hit_sd.N;

      for (uint16_t i = 0; i < node.num_attributes; i++) {
        const uint node_type = kernel_data_fetch(svm_nodes, offset++);
        (void)node_type;
        kernel_assert(node_type == NODE_ATTR);

        const ccl_global auto &attribute_node = svm_node_get<SVMNodeAttr>(kg, &offset);
        svm_raycast_attr_eval_and_store(kg, stack, attribute_node, hit_sd);
      }
    }
  }

  if (is_hit == 0.0f) {
    for (uint16_t i = 0; i < node.num_attributes; i++) {
      const uint node_type = kernel_data_fetch(svm_nodes, offset++);
      (void)node_type;
      kernel_assert(node_type == NODE_ATTR);

      const ccl_global auto &attribute_node = svm_node_get<SVMNodeAttr>(kg, &offset);
      svm_node_attr_store(
          attribute_node.output_type, stack, attribute_node.out_offset, make_zero<float3>());
    }
  }

  if (stack_valid(node.is_hit_offset)) {
    stack_store_float(stack, node.is_hit_offset, is_hit);
  }
  if (stack_valid(node.is_self_hit_offset)) {
    stack_store_float(stack, node.is_self_hit_offset, is_self_hit);
  }
  if (stack_valid(node.hit_distance_offset)) {
    stack_store_float(stack, node.hit_distance_offset, hit_distance);
  }
  if (stack_valid(node.hit_position_offset)) {
    stack_store_float3(stack, node.hit_position_offset, hit_position);
  }
  if (stack_valid(node.hit_normal_offset)) {
    stack_store_float3(stack, node.hit_normal_offset, hit_normal);
  }

  return offset;
}

#endif /* __SHADER_RAYTRACE__ */

CCL_NAMESPACE_END

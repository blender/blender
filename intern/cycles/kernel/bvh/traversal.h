/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2009-2012 Intel Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT bvh_node_intersect
#else
#  define NODE_INTERSECT bvh_aligned_node_intersect
#endif

/* This is a template BVH traversal function, where various features can be
 * enabled/disabled. This way we can compile optimized versions for each case
 * without new features slowing things down.
 *
 * BVH_HAIR: hair curve rendering
 * BVH_POINTCLOUD: point cloud rendering
 * BVH_MOTION: motion blur rendering
 */

ccl_device_noinline bool BVH_FUNCTION_FULL_NAME(BVH)(KernelGlobals kg,
                                                     ccl_private const Ray *ray,
                                                     ccl_private Intersection *isect,
                                                     const uint visibility)
{
  /* todo:
   * - test if pushing distance on the stack helps (for non shadow rays)
   * - separate version for shadow rays
   * - likely and unlikely for if() statements
   * - test restrict attribute for pointers
   */

  /* traversal stack in CUDA thread-local memory */
  int traversal_stack[BVH_STACK_SIZE];
  traversal_stack[0] = ENTRYPOINT_SENTINEL;

  /* traversal variables in registers */
  int stack_ptr = 0;
  int node_addr = kernel_data.bvh.root;

  /* ray parameters in registers */
  float3 P = ray->P;
  float3 dir = bvh_clamp_direction(ray->D);
  float3 idir = bvh_inverse_direction(dir);
  const float tmin = ray->tmin;
  int object = OBJECT_NONE;

  isect->t = ray->tmax;
  isect->u = 0.0f;
  isect->v = 0.0f;
  isect->prim = PRIM_NONE;
  isect->object = OBJECT_NONE;

  /* traversal loop */
  do {
    do {
      /* traverse internal nodes */
      while (node_addr >= 0 && node_addr != ENTRYPOINT_SENTINEL) {
        int node_addr_child1, traverse_mask;
        float dist[2];
        float4 cnodes = kernel_data_fetch(bvh_nodes, node_addr + 0);

        {
          traverse_mask = NODE_INTERSECT(kg,
                                         P,
#if BVH_FEATURE(BVH_HAIR)
                                         dir,
#endif
                                         idir,
                                         tmin,
                                         isect->t,
                                         node_addr,
                                         visibility,
                                         dist);
        }

        node_addr = __float_as_int(cnodes.z);
        node_addr_child1 = __float_as_int(cnodes.w);

        if (traverse_mask == 3) {
          /* Both children were intersected, push the farther one. */
          bool is_closest_child1 = (dist[1] < dist[0]);
          if (is_closest_child1) {
            int tmp = node_addr;
            node_addr = node_addr_child1;
            node_addr_child1 = tmp;
          }

          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_STACK_SIZE);
          traversal_stack[stack_ptr] = node_addr_child1;
        }
        else {
          /* One child was intersected. */
          if (traverse_mask == 2) {
            node_addr = node_addr_child1;
          }
          else if (traverse_mask == 0) {
            /* Neither child was intersected. */
            node_addr = traversal_stack[stack_ptr];
            --stack_ptr;
          }
        }
      }

      /* if node is leaf, fetch triangle list */
      if (node_addr < 0) {
        float4 leaf = kernel_data_fetch(bvh_leaf_nodes, (-node_addr - 1));
        int prim_addr = __float_as_int(leaf.x);

        if (prim_addr >= 0) {
          const int prim_addr2 = __float_as_int(leaf.y);
          const uint type = __float_as_int(leaf.w);

          /* pop */
          node_addr = traversal_stack[stack_ptr];
          --stack_ptr;

          /* primitive intersection */
          for (; prim_addr < prim_addr2; prim_addr++) {
            kernel_assert(kernel_data_fetch(prim_type, prim_addr) == type);

            const int prim_object = (object == OBJECT_NONE) ?
                                        kernel_data_fetch(prim_object, prim_addr) :
                                        object;
            const int prim = kernel_data_fetch(prim_index, prim_addr);
            if (intersection_skip_self_shadow(ray->self, prim_object, prim)) {
              continue;
            }

#ifdef __SHADOW_LINKING__
            if (intersection_skip_shadow_link(kg, ray->self, prim_object)) {
              continue;
            }
#endif

            switch (type & PRIMITIVE_ALL) {
              case PRIMITIVE_TRIANGLE: {
                if (triangle_intersect(kg,
                                       isect,
                                       P,
                                       dir,
                                       tmin,
                                       isect->t,
                                       visibility,
                                       prim_object,
                                       prim,
                                       prim_addr))
                {
                  /* shadow ray early termination */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE)
                    return true;
                }
                break;
              }
#if BVH_FEATURE(BVH_MOTION)
              case PRIMITIVE_MOTION_TRIANGLE: {
                if (motion_triangle_intersect(kg,
                                              isect,
                                              P,
                                              dir,
                                              tmin,
                                              isect->t,
                                              ray->time,
                                              visibility,
                                              prim_object,
                                              prim,
                                              prim_addr))
                {
                  /* shadow ray early termination */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE)
                    return true;
                }
                break;
              }
#endif /* BVH_FEATURE(BVH_MOTION) */
#if BVH_FEATURE(BVH_HAIR)
              case PRIMITIVE_CURVE_THICK:
              case PRIMITIVE_MOTION_CURVE_THICK:
              case PRIMITIVE_CURVE_RIBBON:
              case PRIMITIVE_MOTION_CURVE_RIBBON: {
                if ((type & PRIMITIVE_MOTION) && kernel_data.bvh.use_bvh_steps) {
                  const float2 prim_time = kernel_data_fetch(prim_time, prim_addr);
                  if (ray->time < prim_time.x || ray->time > prim_time.y) {
                    break;
                  }
                }

                const int curve_type = kernel_data_fetch(prim_type, prim_addr);
                const bool hit = curve_intersect(
                    kg, isect, P, dir, tmin, isect->t, prim_object, prim, ray->time, curve_type);
                if (hit) {
                  /* shadow ray early termination */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE)
                    return true;
                }
                break;
              }
#endif /* BVH_FEATURE(BVH_HAIR) */
#if BVH_FEATURE(BVH_POINTCLOUD)
              case PRIMITIVE_POINT:
              case PRIMITIVE_MOTION_POINT: {
                if ((type & PRIMITIVE_MOTION) && kernel_data.bvh.use_bvh_steps) {
                  const float2 prim_time = kernel_data_fetch(prim_time, prim_addr);
                  if (ray->time < prim_time.x || ray->time > prim_time.y) {
                    break;
                  }
                }

                const int point_type = kernel_data_fetch(prim_type, prim_addr);
                const bool hit = point_intersect(
                    kg, isect, P, dir, tmin, isect->t, prim_object, prim, ray->time, point_type);
                if (hit) {
                  /* shadow ray early termination */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE)
                    return true;
                }
                break;
              }
#endif /* BVH_FEATURE(BVH_POINTCLOUD) */
            }
          }
        }
        else {
          /* instance push */
          object = kernel_data_fetch(prim_object, -prim_addr - 1);

#if BVH_FEATURE(BVH_MOTION)
          bvh_instance_motion_push(kg, object, ray, &P, &dir, &idir);
#else
          bvh_instance_push(kg, object, ray, &P, &dir, &idir);
#endif

          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_STACK_SIZE);
          traversal_stack[stack_ptr] = ENTRYPOINT_SENTINEL;

          node_addr = kernel_data_fetch(object_node, object);
        }
      }
    } while (node_addr != ENTRYPOINT_SENTINEL);

    if (stack_ptr >= 0) {
      kernel_assert(object != OBJECT_NONE);

      /* instance pop */
      bvh_instance_pop(ray, &P, &dir, &idir);

      object = OBJECT_NONE;
      node_addr = traversal_stack[stack_ptr];
      --stack_ptr;
    }
  } while (node_addr != ENTRYPOINT_SENTINEL);

  return (isect->prim != PRIM_NONE);
}

ccl_device_inline bool BVH_FUNCTION_NAME(KernelGlobals kg,
                                         ccl_private const Ray *ray,
                                         ccl_private Intersection *isect,
                                         const uint visibility)
{
  return BVH_FUNCTION_FULL_NAME(BVH)(kg, ray, isect, visibility);
}

#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES
#undef NODE_INTERSECT

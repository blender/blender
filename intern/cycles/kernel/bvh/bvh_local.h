/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation,
 * and code copyright 2009-2012 Intel Corporation
 *
 * Modifications Copyright 2011-2013, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT bvh_node_intersect
#else
#  define NODE_INTERSECT bvh_aligned_node_intersect
#endif

/* This is a template BVH traversal function for finding local intersections
 * around the shading point, for subsurface scattering and bevel. We disable
 * various features for performance, and for instanced objects avoid traversing
 * other parts of the scene.
 *
 * BVH_MOTION: motion blur rendering
 */

#ifndef __KERNEL_GPU__
ccl_device
#else
ccl_device_inline
#endif
    bool BVH_FUNCTION_FULL_NAME(BVH)(KernelGlobals *kg,
                                     const Ray *ray,
                                     LocalIntersection *local_isect,
                                     int local_object,
                                     uint *lcg_state,
                                     int max_hits)
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
  int node_addr = kernel_tex_fetch(__object_node, local_object);

  /* ray parameters in registers */
  float3 P = ray->P;
  float3 dir = bvh_clamp_direction(ray->D);
  float3 idir = bvh_inverse_direction(dir);
  int object = OBJECT_NONE;
  float isect_t = ray->t;

  if (local_isect != NULL) {
    local_isect->num_hits = 0;
  }
  kernel_assert((local_isect == NULL) == (max_hits == 0));

  const int object_flag = kernel_tex_fetch(__object_flag, local_object);
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
#if BVH_FEATURE(BVH_MOTION)
    Transform ob_itfm;
    isect_t = bvh_instance_motion_push(kg, local_object, ray, &P, &dir, &idir, isect_t, &ob_itfm);
#else
    isect_t = bvh_instance_push(kg, local_object, ray, &P, &dir, &idir, isect_t);
#endif
    object = local_object;
  }

  /* traversal loop */
  do {
    do {
      /* traverse internal nodes */
      while (node_addr >= 0 && node_addr != ENTRYPOINT_SENTINEL) {
        int node_addr_child1, traverse_mask;
        float dist[2];
        float4 cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 0);

        traverse_mask = NODE_INTERSECT(kg,
                                       P,
#if BVH_FEATURE(BVH_HAIR)
                                       dir,
#endif
                                       idir,
                                       isect_t,
                                       node_addr,
                                       PATH_RAY_ALL_VISIBILITY,
                                       dist);

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
        float4 leaf = kernel_tex_fetch(__bvh_leaf_nodes, (-node_addr - 1));
        int prim_addr = __float_as_int(leaf.x);

        const int prim_addr2 = __float_as_int(leaf.y);
        const uint type = __float_as_int(leaf.w);

        /* pop */
        node_addr = traversal_stack[stack_ptr];
        --stack_ptr;

        /* primitive intersection */
        switch (type & PRIMITIVE_ALL) {
          case PRIMITIVE_TRIANGLE: {
            /* intersect ray against primitive */
            for (; prim_addr < prim_addr2; prim_addr++) {
              kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
              if (triangle_intersect_local(kg,
                                           local_isect,
                                           P,
                                           dir,
                                           object,
                                           local_object,
                                           prim_addr,
                                           isect_t,
                                           lcg_state,
                                           max_hits)) {
                return true;
              }
            }
            break;
          }
#if BVH_FEATURE(BVH_MOTION)
          case PRIMITIVE_MOTION_TRIANGLE: {
            /* intersect ray against primitive */
            for (; prim_addr < prim_addr2; prim_addr++) {
              kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
              if (motion_triangle_intersect_local(kg,
                                                  local_isect,
                                                  P,
                                                  dir,
                                                  ray->time,
                                                  object,
                                                  local_object,
                                                  prim_addr,
                                                  isect_t,
                                                  lcg_state,
                                                  max_hits)) {
                return true;
              }
            }
            break;
          }
#endif
          default: {
            break;
          }
        }
      }
    } while (node_addr != ENTRYPOINT_SENTINEL);
  } while (node_addr != ENTRYPOINT_SENTINEL);

  return false;
}

ccl_device_inline bool BVH_FUNCTION_NAME(KernelGlobals *kg,
                                         const Ray *ray,
                                         LocalIntersection *local_isect,
                                         int local_object,
                                         uint *lcg_state,
                                         int max_hits)
{
  return BVH_FUNCTION_FULL_NAME(BVH)(kg, ray, local_isect, local_object, lcg_state, max_hits);
}

#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES
#undef NODE_INTERSECT

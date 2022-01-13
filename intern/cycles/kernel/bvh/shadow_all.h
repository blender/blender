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

/* This is a template BVH traversal function, where various features can be
 * enabled/disabled. This way we can compile optimized versions for each case
 * without new features slowing things down.
 *
 * BVH_HAIR: hair curve rendering
 * BVH_POINTCLOUD: point cloud rendering
 * BVH_MOTION: motion blur rendering
 */

#ifndef __KERNEL_GPU__
ccl_device
#else
ccl_device_inline
#endif
    bool BVH_FUNCTION_FULL_NAME(BVH)(KernelGlobals kg,
                                     ccl_private const Ray *ray,
                                     IntegratorShadowState state,
                                     const uint visibility,
                                     const uint max_hits,
                                     ccl_private uint *num_recorded_hits,
                                     ccl_private float *throughput)
{
  /* todo:
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
  int object = OBJECT_NONE;
  uint num_hits = 0;

#if BVH_FEATURE(BVH_MOTION)
  Transform ob_itfm;
#endif

  /* Max distance in world space. May be dynamically reduced when max number of
   * recorded hits is exceeded and we no longer need to find hits beyond the max
   * distance found. */
  float t_max_world = ray->t;
  /* Equal to t_max_world when traversing top level BVH, transformed into local
   * space when entering instances. */
  float t_max_current = t_max_world;
  /* Conversion from world to local space for the current instance if any, 1.0
   * otherwise. */
  float t_world_to_instance = 1.0f;

  *num_recorded_hits = 0;
  *throughput = 1.0f;

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
                                       t_max_current,
                                       node_addr,
                                       visibility,
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

        if (prim_addr >= 0) {
          const int prim_addr2 = __float_as_int(leaf.y);
          const uint type = __float_as_int(leaf.w);

          /* pop */
          node_addr = traversal_stack[stack_ptr];
          --stack_ptr;

          /* primitive intersection */
          for (; prim_addr < prim_addr2; prim_addr++) {
            kernel_assert((kernel_tex_fetch(__prim_type, prim_addr) & PRIMITIVE_ALL) ==
                          (type & PRIMITIVE_ALL));
            bool hit;

            /* todo: specialized intersect functions which don't fill in
             * isect unless needed and check SD_HAS_TRANSPARENT_SHADOW?
             * might give a few % performance improvement */
            Intersection isect ccl_optional_struct_init;

            const int prim_object = (object == OBJECT_NONE) ?
                                        kernel_tex_fetch(__prim_object, prim_addr) :
                                        object;
            const int prim = kernel_tex_fetch(__prim_index, prim_addr);
            if (intersection_skip_self_shadow(ray->self, prim_object, prim)) {
              continue;
            }

            switch (type & PRIMITIVE_ALL) {
              case PRIMITIVE_TRIANGLE: {
                hit = triangle_intersect(
                    kg, &isect, P, dir, t_max_current, visibility, prim_object, prim, prim_addr);
                break;
              }
#if BVH_FEATURE(BVH_MOTION)
              case PRIMITIVE_MOTION_TRIANGLE: {
                hit = motion_triangle_intersect(kg,
                                                &isect,
                                                P,
                                                dir,
                                                t_max_current,
                                                ray->time,
                                                visibility,
                                                prim_object,
                                                prim,
                                                prim_addr);
                break;
              }
#endif
#if BVH_FEATURE(BVH_HAIR)
              case PRIMITIVE_CURVE_THICK:
              case PRIMITIVE_MOTION_CURVE_THICK:
              case PRIMITIVE_CURVE_RIBBON:
              case PRIMITIVE_MOTION_CURVE_RIBBON: {
                if ((type & PRIMITIVE_MOTION) && kernel_data.bvh.use_bvh_steps) {
                  const float2 prim_time = kernel_tex_fetch(__prim_time, prim_addr);
                  if (ray->time < prim_time.x || ray->time > prim_time.y) {
                    hit = false;
                    break;
                  }
                }

                const int curve_type = kernel_tex_fetch(__prim_type, prim_addr);
                hit = curve_intersect(
                    kg, &isect, P, dir, t_max_current, prim_object, prim, ray->time, curve_type);

                break;
              }
#endif
#if BVH_FEATURE(BVH_POINTCLOUD)
              case PRIMITIVE_POINT:
              case PRIMITIVE_MOTION_POINT: {
                if ((type & PRIMITIVE_MOTION) && kernel_data.bvh.use_bvh_steps) {
                  const float2 prim_time = kernel_tex_fetch(__prim_time, prim_addr);
                  if (ray->time < prim_time.x || ray->time > prim_time.y) {
                    hit = false;
                    break;
                  }
                }

                const int point_type = kernel_tex_fetch(__prim_type, prim_addr);
                hit = point_intersect(
                    kg, &isect, P, dir, t_max_current, prim_object, prim, ray->time, point_type);
                break;
              }
#endif /* BVH_FEATURE(BVH_POINTCLOUD) */
              default: {
                hit = false;
                break;
              }
            }

            /* shadow ray early termination */
            if (hit) {
              /* Convert intersection distance to world space. */
              isect.t /= t_world_to_instance;

              /* detect if this surface has a shader with transparent shadows */
              /* todo: optimize so primitive visibility flag indicates if
               * the primitive has a transparent shadow shader? */
              const int flags = intersection_get_shader_flags(kg, isect.prim, isect.type);

              if (!(flags & SD_HAS_TRANSPARENT_SHADOW) || num_hits >= max_hits) {
                /* If no transparent shadows, all light is blocked and we can
                 * stop immediately. */
                return true;
              }

              num_hits++;

              bool record_intersection = true;

              /* Always use baked shadow transparency for curves. */
              if (isect.type & PRIMITIVE_CURVE) {
                *throughput *= intersection_curve_shadow_transparency(
                    kg, isect.object, isect.prim, isect.u);

                if (*throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
                  return true;
                }
                else {
                  record_intersection = false;
                }
              }

              if (record_intersection) {
                /* Increase the number of hits, possibly beyond max_hits, we will
                 * simply not record those and only keep the max_hits closest. */
                uint record_index = (*num_recorded_hits)++;

                const uint max_record_hits = min(max_hits, INTEGRATOR_SHADOW_ISECT_SIZE);
                if (record_index >= max_record_hits - 1) {
                  /* If maximum number of hits reached, find the intersection with
                   * the largest distance to potentially replace when another hit
                   * is found. */
                  const int num_recorded_hits = min(max_record_hits, record_index);
                  float max_recorded_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, t);
                  int max_recorded_hit = 0;

                  for (int i = 1; i < num_recorded_hits; i++) {
                    const float isect_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, i, t);
                    if (isect_t > max_recorded_t) {
                      max_recorded_t = isect_t;
                      max_recorded_hit = i;
                    }
                  }

                  if (record_index >= max_record_hits) {
                    record_index = max_recorded_hit;
                  }

                  /* Limit the ray distance and stop counting hits beyond this. */
                  t_max_world = max(max_recorded_t, isect.t);
                  t_max_current = t_max_world * t_world_to_instance;
                }

                integrator_state_write_shadow_isect(state, &isect, record_index);
              }
            }
          }
        }
        else {
          /* instance push */
          object = kernel_tex_fetch(__prim_object, -prim_addr - 1);

#if BVH_FEATURE(BVH_MOTION)
          t_world_to_instance = bvh_instance_motion_push(
              kg, object, ray, &P, &dir, &idir, &ob_itfm);
#else
          t_world_to_instance = bvh_instance_push(kg, object, ray, &P, &dir, &idir);
#endif

          /* Convert intersection to object space. */
          t_max_current *= t_world_to_instance;

          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_STACK_SIZE);
          traversal_stack[stack_ptr] = ENTRYPOINT_SENTINEL;

          node_addr = kernel_tex_fetch(__object_node, object);
        }
      }
    } while (node_addr != ENTRYPOINT_SENTINEL);

    if (stack_ptr >= 0) {
      kernel_assert(object != OBJECT_NONE);

      /* Instance pop. */
#if BVH_FEATURE(BVH_MOTION)
      bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, FLT_MAX, &ob_itfm);
#else
      bvh_instance_pop(kg, object, ray, &P, &dir, &idir, FLT_MAX);
#endif

      /* Restore world space ray length. */
      t_max_current = t_max_world;

      object = OBJECT_NONE;
      t_world_to_instance = 1.0f;
      node_addr = traversal_stack[stack_ptr];
      --stack_ptr;
    }
  } while (node_addr != ENTRYPOINT_SENTINEL);

  return false;
}

ccl_device_inline bool BVH_FUNCTION_NAME(KernelGlobals kg,
                                         ccl_private const Ray *ray,
                                         IntegratorShadowState state,
                                         const uint visibility,
                                         const uint max_hits,
                                         ccl_private uint *num_recorded_hits,
                                         ccl_private float *throughput)
{
  return BVH_FUNCTION_FULL_NAME(BVH)(
      kg, ray, state, visibility, max_hits, num_recorded_hits, throughput);
}

#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES
#undef NODE_INTERSECT

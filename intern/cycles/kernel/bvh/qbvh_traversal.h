/*
 * Copyright 2011-2013 Blender Foundation
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

/* This is a template BVH traversal function, where various features can be
 * enabled/disabled. This way we can compile optimized versions for each case
 * without new features slowing things down.
 *
 * BVH_INSTANCING: object instancing
 * BVH_HAIR: hair curve rendering
 * BVH_MOTION: motion blur rendering
 */

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT qbvh_node_intersect
#else
#  define NODE_INTERSECT qbvh_aligned_node_intersect
#endif

ccl_device bool BVH_FUNCTION_FULL_NAME(QBVH)(KernelGlobals *kg,
                                             const Ray *ray,
                                             Intersection *isect,
                                             const uint visibility)
{
  /* TODO(sergey):
   * - Test if pushing distance on the stack helps (for non shadow rays).
   * - Separate version for shadow rays.
   * - Likely and unlikely for if() statements.
   * - Test restrict attribute for pointers.
   */

  /* Traversal stack in CUDA thread-local memory. */
  QBVHStackItem traversal_stack[BVH_QSTACK_SIZE];
  traversal_stack[0].addr = ENTRYPOINT_SENTINEL;
  traversal_stack[0].dist = -FLT_MAX;

  /* Traversal variables in registers. */
  int stack_ptr = 0;
  int node_addr = kernel_data.bvh.root;
  float node_dist = -FLT_MAX;

  /* Ray parameters in registers. */
  float3 P = ray->P;
  float3 dir = bvh_clamp_direction(ray->D);
  float3 idir = bvh_inverse_direction(dir);
  int object = OBJECT_NONE;

#if BVH_FEATURE(BVH_MOTION)
  Transform ob_itfm;
#endif

  isect->t = ray->t;
  isect->u = 0.0f;
  isect->v = 0.0f;
  isect->prim = PRIM_NONE;
  isect->object = OBJECT_NONE;

  BVH_DEBUG_INIT();

  ssef tnear(0.0f), tfar(ray->t);
#if BVH_FEATURE(BVH_HAIR)
  sse3f dir4(ssef(dir.x), ssef(dir.y), ssef(dir.z));
#endif
  sse3f idir4(ssef(idir.x), ssef(idir.y), ssef(idir.z));

#ifdef __KERNEL_AVX2__
  float3 P_idir = P * idir;
  sse3f P_idir4 = sse3f(P_idir.x, P_idir.y, P_idir.z);
#endif
#if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
  sse3f org4 = sse3f(ssef(P.x), ssef(P.y), ssef(P.z));
#endif

  /* Offsets to select the side that becomes the lower or upper bound. */
  int near_x, near_y, near_z;
  int far_x, far_y, far_z;
  qbvh_near_far_idx_calc(idir, &near_x, &near_y, &near_z, &far_x, &far_y, &far_z);

  /* Traversal loop. */
  do {
    do {
      /* Traverse internal nodes. */
      while (node_addr >= 0 && node_addr != ENTRYPOINT_SENTINEL) {
        float4 inodes = kernel_tex_fetch(__bvh_nodes, node_addr + 0);
        (void)inodes;

        if (UNLIKELY(node_dist > isect->t)
#if BVH_FEATURE(BVH_MOTION)
            || UNLIKELY(ray->time < inodes.y) || UNLIKELY(ray->time > inodes.z)
#endif
#ifdef __VISIBILITY_FLAG__
            || (__float_as_uint(inodes.x) & visibility) == 0
#endif
        ) {
          /* Pop. */
          node_addr = traversal_stack[stack_ptr].addr;
          node_dist = traversal_stack[stack_ptr].dist;
          --stack_ptr;
          continue;
        }

        int child_mask;
        ssef dist;

        BVH_DEBUG_NEXT_NODE();

        {
          child_mask = NODE_INTERSECT(kg,
                                      tnear,
                                      tfar,
#ifdef __KERNEL_AVX2__
                                      P_idir4,
#endif
#if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
                                      org4,
#endif
#if BVH_FEATURE(BVH_HAIR)
                                      dir4,
#endif
                                      idir4,
                                      near_x,
                                      near_y,
                                      near_z,
                                      far_x,
                                      far_y,
                                      far_z,
                                      node_addr,
                                      &dist);
        }

        if (child_mask != 0) {
          float4 cnodes;
          /* TODO(sergey): Investigate whether moving cnodes upwards
           * gives a speedup (will be different cache pattern but will
           * avoid extra check here).
           */
#if BVH_FEATURE(BVH_HAIR)
          if (__float_as_uint(inodes.x) & PATH_RAY_NODE_UNALIGNED) {
            cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 13);
          }
          else
#endif
          {
            cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 7);
          }

          /* One child is hit, continue with that child. */
          int r = __bscf(child_mask);
          float d0 = ((float *)&dist)[r];
          if (child_mask == 0) {
            node_addr = __float_as_int(cnodes[r]);
            node_dist = d0;
            continue;
          }

          /* Two children are hit, push far child, and continue with
           * closer child.
           */
          int c0 = __float_as_int(cnodes[r]);
          r = __bscf(child_mask);
          int c1 = __float_as_int(cnodes[r]);
          float d1 = ((float *)&dist)[r];
          if (child_mask == 0) {
            if (d1 < d0) {
              node_addr = c1;
              node_dist = d1;
              ++stack_ptr;
              kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
              traversal_stack[stack_ptr].addr = c0;
              traversal_stack[stack_ptr].dist = d0;
              continue;
            }
            else {
              node_addr = c0;
              node_dist = d0;
              ++stack_ptr;
              kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
              traversal_stack[stack_ptr].addr = c1;
              traversal_stack[stack_ptr].dist = d1;
              continue;
            }
          }

          /* Here starts the slow path for 3 or 4 hit children. We push
           * all nodes onto the stack to sort them there.
           */
          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
          traversal_stack[stack_ptr].addr = c1;
          traversal_stack[stack_ptr].dist = d1;
          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
          traversal_stack[stack_ptr].addr = c0;
          traversal_stack[stack_ptr].dist = d0;

          /* Three children are hit, push all onto stack and sort 3
           * stack items, continue with closest child.
           */
          r = __bscf(child_mask);
          int c2 = __float_as_int(cnodes[r]);
          float d2 = ((float *)&dist)[r];
          if (child_mask == 0) {
            ++stack_ptr;
            kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
            traversal_stack[stack_ptr].addr = c2;
            traversal_stack[stack_ptr].dist = d2;
            qbvh_stack_sort(&traversal_stack[stack_ptr],
                            &traversal_stack[stack_ptr - 1],
                            &traversal_stack[stack_ptr - 2]);
            node_addr = traversal_stack[stack_ptr].addr;
            node_dist = traversal_stack[stack_ptr].dist;
            --stack_ptr;
            continue;
          }

          /* Four children are hit, push all onto stack and sort 4
           * stack items, continue with closest child.
           */
          r = __bscf(child_mask);
          int c3 = __float_as_int(cnodes[r]);
          float d3 = ((float *)&dist)[r];
          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
          traversal_stack[stack_ptr].addr = c3;
          traversal_stack[stack_ptr].dist = d3;
          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
          traversal_stack[stack_ptr].addr = c2;
          traversal_stack[stack_ptr].dist = d2;
          qbvh_stack_sort(&traversal_stack[stack_ptr],
                          &traversal_stack[stack_ptr - 1],
                          &traversal_stack[stack_ptr - 2],
                          &traversal_stack[stack_ptr - 3]);
        }

        node_addr = traversal_stack[stack_ptr].addr;
        node_dist = traversal_stack[stack_ptr].dist;
        --stack_ptr;
      }

      /* If node is leaf, fetch triangle list. */
      if (node_addr < 0) {
        float4 leaf = kernel_tex_fetch(__bvh_leaf_nodes, (-node_addr - 1));

#ifdef __VISIBILITY_FLAG__
        if (UNLIKELY((node_dist > isect->t) || ((__float_as_uint(leaf.z) & visibility) == 0)))
#else
        if (UNLIKELY((node_dist > isect->t)))
#endif
        {
          /* Pop. */
          node_addr = traversal_stack[stack_ptr].addr;
          node_dist = traversal_stack[stack_ptr].dist;
          --stack_ptr;
          continue;
        }

        int prim_addr = __float_as_int(leaf.x);

#if BVH_FEATURE(BVH_INSTANCING)
        if (prim_addr >= 0) {
#endif
          int prim_addr2 = __float_as_int(leaf.y);
          const uint type = __float_as_int(leaf.w);

          /* Pop. */
          node_addr = traversal_stack[stack_ptr].addr;
          node_dist = traversal_stack[stack_ptr].dist;
          --stack_ptr;

          /* Primitive intersection. */
          switch (type & PRIMITIVE_ALL) {
            case PRIMITIVE_TRIANGLE: {
              for (; prim_addr < prim_addr2; prim_addr++) {
                BVH_DEBUG_NEXT_INTERSECTION();
                kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
                if (triangle_intersect(kg, isect, P, dir, visibility, object, prim_addr)) {
                  tfar = ssef(isect->t);
                  /* Shadow ray early termination. */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
                    return true;
                  }
                }
              }
              break;
            }
#if BVH_FEATURE(BVH_MOTION)
            case PRIMITIVE_MOTION_TRIANGLE: {
              for (; prim_addr < prim_addr2; prim_addr++) {
                BVH_DEBUG_NEXT_INTERSECTION();
                kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
                if (motion_triangle_intersect(
                        kg, isect, P, dir, ray->time, visibility, object, prim_addr)) {
                  tfar = ssef(isect->t);
                  /* Shadow ray early termination. */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
                    return true;
                  }
                }
              }
              break;
            }
#endif /* BVH_FEATURE(BVH_MOTION) */
#if BVH_FEATURE(BVH_HAIR)
            case PRIMITIVE_CURVE:
            case PRIMITIVE_MOTION_CURVE: {
              for (; prim_addr < prim_addr2; prim_addr++) {
                BVH_DEBUG_NEXT_INTERSECTION();
                const uint curve_type = kernel_tex_fetch(__prim_type, prim_addr);
                kernel_assert((curve_type & PRIMITIVE_ALL) == (type & PRIMITIVE_ALL));
                bool hit = curve_intersect(
                    kg, isect, P, dir, visibility, object, prim_addr, ray->time, curve_type);
                if (hit) {
                  tfar = ssef(isect->t);
                  /* Shadow ray early termination. */
                  if (visibility & PATH_RAY_SHADOW_OPAQUE) {
                    return true;
                  }
                }
              }
              break;
            }
#endif /* BVH_FEATURE(BVH_HAIR) */
          }
        }
#if BVH_FEATURE(BVH_INSTANCING)
        else {
          /* Instance push. */
          object = kernel_tex_fetch(__prim_object, -prim_addr - 1);

#  if BVH_FEATURE(BVH_MOTION)
          qbvh_instance_motion_push(
              kg, object, ray, &P, &dir, &idir, &isect->t, &node_dist, &ob_itfm);
#  else
          qbvh_instance_push(kg, object, ray, &P, &dir, &idir, &isect->t, &node_dist);
#  endif

          qbvh_near_far_idx_calc(idir, &near_x, &near_y, &near_z, &far_x, &far_y, &far_z);
          tfar = ssef(isect->t);
#  if BVH_FEATURE(BVH_HAIR)
          dir4 = sse3f(ssef(dir.x), ssef(dir.y), ssef(dir.z));
#  endif
          idir4 = sse3f(ssef(idir.x), ssef(idir.y), ssef(idir.z));
#  ifdef __KERNEL_AVX2__
          P_idir = P * idir;
          P_idir4 = sse3f(P_idir.x, P_idir.y, P_idir.z);
#  endif
#  if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
          org4 = sse3f(ssef(P.x), ssef(P.y), ssef(P.z));
#  endif

          ++stack_ptr;
          kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
          traversal_stack[stack_ptr].addr = ENTRYPOINT_SENTINEL;
          traversal_stack[stack_ptr].dist = -FLT_MAX;

          node_addr = kernel_tex_fetch(__object_node, object);

          BVH_DEBUG_NEXT_INSTANCE();
        }
      }
#endif /* FEATURE(BVH_INSTANCING) */
    } while (node_addr != ENTRYPOINT_SENTINEL);

#if BVH_FEATURE(BVH_INSTANCING)
    if (stack_ptr >= 0) {
      kernel_assert(object != OBJECT_NONE);

      /* Instance pop. */
#  if BVH_FEATURE(BVH_MOTION)
      isect->t = bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, isect->t, &ob_itfm);
#  else
      isect->t = bvh_instance_pop(kg, object, ray, &P, &dir, &idir, isect->t);
#  endif

      qbvh_near_far_idx_calc(idir, &near_x, &near_y, &near_z, &far_x, &far_y, &far_z);
      tfar = ssef(isect->t);
#  if BVH_FEATURE(BVH_HAIR)
      dir4 = sse3f(ssef(dir.x), ssef(dir.y), ssef(dir.z));
#  endif
      idir4 = sse3f(ssef(idir.x), ssef(idir.y), ssef(idir.z));
#  ifdef __KERNEL_AVX2__
      P_idir = P * idir;
      P_idir4 = sse3f(P_idir.x, P_idir.y, P_idir.z);
#  endif
#  if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
      org4 = sse3f(ssef(P.x), ssef(P.y), ssef(P.z));
#  endif

      object = OBJECT_NONE;
      node_addr = traversal_stack[stack_ptr].addr;
      node_dist = traversal_stack[stack_ptr].dist;
      --stack_ptr;
    }
#endif /* FEATURE(BVH_INSTANCING) */
  } while (node_addr != ENTRYPOINT_SENTINEL);

  return (isect->prim != PRIM_NONE);
}

#undef NODE_INTERSECT

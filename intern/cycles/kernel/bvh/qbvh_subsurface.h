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

/* This is a template BVH traversal function for subsurface scattering, where
 * various features can be enabled/disabled. This way we can compile optimized
 * versions for each case without new features slowing things down.
 *
 * BVH_MOTION: motion blur rendering
 *
 */

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT qbvh_node_intersect
#else
#  define NODE_INTERSECT qbvh_aligned_node_intersect
#endif

ccl_device void BVH_FUNCTION_FULL_NAME(QBVH)(KernelGlobals *kg,
                                             const Ray *ray,
                                             SubsurfaceIntersection *ss_isect,
                                             int subsurface_object,
                                             uint *lcg_state,
                                             int max_hits)
{
	/* TODO(sergey):
	 * - Test if pushing distance on the stack helps (for non shadow rays).
	 * - Separate version for shadow rays.
	 * - Likely and unlikely for if() statements.
	 * - SSE for hair.
	 * - Test restrict attribute for pointers.
	 */

	/* Traversal stack in CUDA thread-local memory. */
	QBVHStackItem traversal_stack[BVH_QSTACK_SIZE];
	traversal_stack[0].addr = ENTRYPOINT_SENTINEL;

	/* Traversal variables in registers. */
	int stack_ptr = 0;
	int node_addr = kernel_tex_fetch(__object_node, subsurface_object);

	/* Ray parameters in registers. */
	float3 P = ray->P;
	float3 dir = bvh_clamp_direction(ray->D);
	float3 idir = bvh_inverse_direction(dir);
	int object = OBJECT_NONE;
	float isect_t = ray->t;

	ss_isect->num_hits = 0;

	const int object_flag = kernel_tex_fetch(__object_flag, subsurface_object);
	if(!(object_flag & SD_TRANSFORM_APPLIED)) {
#if BVH_FEATURE(BVH_MOTION)
		Transform ob_itfm;
		bvh_instance_motion_push(kg,
		                         subsurface_object,
		                         ray,
		                         &P,
		                         &dir,
		                         &idir,
		                         &isect_t,
		                         &ob_itfm);
#else
		bvh_instance_push(kg, subsurface_object, ray, &P, &dir, &idir, &isect_t);
#endif
		object = subsurface_object;
	}

#ifndef __KERNEL_SSE41__
	if(!isfinite(P.x)) {
		return;
	}
#endif

	ssef tnear(0.0f), tfar(isect_t);
#if BVH_FEATURE(BVH_HAIR)
	sse3f dir4(ssef(dir.x), ssef(dir.y), ssef(dir.z));
#endif
	sse3f idir4(ssef(idir.x), ssef(idir.y), ssef(idir.z));

#ifdef __KERNEL_AVX2__
	float3 P_idir = P*idir;
	sse3f P_idir4(P_idir.x, P_idir.y, P_idir.z);
#endif
#if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
	sse3f org4(ssef(P.x), ssef(P.y), ssef(P.z));
#endif

	/* Offsets to select the side that becomes the lower or upper bound. */
	int near_x, near_y, near_z;
	int far_x, far_y, far_z;
	qbvh_near_far_idx_calc(idir,
	                       &near_x, &near_y, &near_z,
	                       &far_x, &far_y, &far_z);

	IsectPrecalc isect_precalc;
	triangle_intersect_precalc(dir, &isect_precalc);

	/* Traversal loop. */
	do {
		do {
			/* Traverse internal nodes. */
			while(node_addr >= 0 && node_addr != ENTRYPOINT_SENTINEL) {
				ssef dist;
				int child_mask = NODE_INTERSECT(kg,
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
				                                near_x, near_y, near_z,
				                                far_x, far_y, far_z,
				                                node_addr,
				                                &dist);

				if(child_mask != 0) {
					float4 inodes = kernel_tex_fetch(__bvh_nodes, node_addr+0);
					float4 cnodes;
#if BVH_FEATURE(BVH_HAIR)
					if(__float_as_uint(inodes.x) & PATH_RAY_NODE_UNALIGNED) {
						cnodes = kernel_tex_fetch(__bvh_nodes, node_addr+13);
					}
					else
#endif
					{
						cnodes = kernel_tex_fetch(__bvh_nodes, node_addr+7);
					}

					/* One child is hit, continue with that child. */
					int r = __bscf(child_mask);
					if(child_mask == 0) {
						node_addr = __float_as_int(cnodes[r]);
						continue;
					}

					/* Two children are hit, push far child, and continue with
					 * closer child.
					 */
					int c0 = __float_as_int(cnodes[r]);
					float d0 = ((float*)&dist)[r];
					r = __bscf(child_mask);
					int c1 = __float_as_int(cnodes[r]);
					float d1 = ((float*)&dist)[r];
					if(child_mask == 0) {
						if(d1 < d0) {
							node_addr = c1;
							++stack_ptr;
							kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
							traversal_stack[stack_ptr].addr = c0;
							traversal_stack[stack_ptr].dist = d0;
							continue;
						}
						else {
							node_addr = c0;
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
					float d2 = ((float*)&dist)[r];
					if(child_mask == 0) {
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_QSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c2;
						traversal_stack[stack_ptr].dist = d2;
						qbvh_stack_sort(&traversal_stack[stack_ptr],
						                &traversal_stack[stack_ptr - 1],
						                &traversal_stack[stack_ptr - 2]);
						node_addr = traversal_stack[stack_ptr].addr;
						--stack_ptr;
						continue;
					}

					/* Four children are hit, push all onto stack and sort 4
					 * stack items, continue with closest child.
					 */
					r = __bscf(child_mask);
					int c3 = __float_as_int(cnodes[r]);
					float d3 = ((float*)&dist)[r];
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
				--stack_ptr;
			}

			/* If node is leaf, fetch triangle list. */
			if(node_addr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_leaf_nodes, (-node_addr-1));
				int prim_addr = __float_as_int(leaf.x);

				int prim_addr2 = __float_as_int(leaf.y);
				const uint type = __float_as_int(leaf.w);

				/* Pop. */
				node_addr = traversal_stack[stack_ptr].addr;
				--stack_ptr;

				/* Primitive intersection. */
				switch(type & PRIMITIVE_ALL) {
					case PRIMITIVE_TRIANGLE: {
						/* Intersect ray against primitive, */
						for(; prim_addr < prim_addr2; prim_addr++) {
							kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
							triangle_intersect_subsurface(kg,
							                              &isect_precalc,
							                              ss_isect,
							                              P,
							                              object,
							                              prim_addr,
							                              isect_t,
							                              lcg_state,
							                              max_hits);
						}
						break;
					}
#if BVH_FEATURE(BVH_MOTION)
					case PRIMITIVE_MOTION_TRIANGLE: {
						/* Intersect ray against primitive. */
						for(; prim_addr < prim_addr2; prim_addr++) {
							kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
							motion_triangle_intersect_subsurface(kg,
							                                     ss_isect,
							                                     P,
							                                     dir,
							                                     ray->time,
							                                     object,
							                                     prim_addr,
							                                     isect_t,
							                                     lcg_state,
							                                     max_hits);
						}
						break;
					}
#endif
					default:
						break;
				}
			}
		} while(node_addr != ENTRYPOINT_SENTINEL);
	} while(node_addr != ENTRYPOINT_SENTINEL);
}

#undef NODE_INTERSECT

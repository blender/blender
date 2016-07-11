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

#ifdef __QBVH__
#  include "qbvh_subsurface.h"
#endif

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT bvh_node_intersect
#else
#  define NODE_INTERSECT bvh_aligned_node_intersect
#endif

/* This is a template BVH traversal function for subsurface scattering, where
 * various features can be enabled/disabled. This way we can compile optimized
 * versions for each case without new features slowing things down.
 *
 * BVH_MOTION: motion blur rendering
 *
 */

ccl_device void BVH_FUNCTION_FULL_NAME(BVH)(KernelGlobals *kg,
                                            const Ray *ray,
                                            SubsurfaceIntersection *ss_isect,
                                            int subsurface_object,
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
	int traversalStack[BVH_STACK_SIZE];
	traversalStack[0] = ENTRYPOINT_SENTINEL;

	/* traversal variables in registers */
	int stackPtr = 0;
	int nodeAddr = kernel_tex_fetch(__object_node, subsurface_object);

	/* ray parameters in registers */
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

#if defined(__KERNEL_SSE2__)
	const shuffle_swap_t shuf_identity = shuffle_swap_identity();
	const shuffle_swap_t shuf_swap = shuffle_swap_swap();

	const ssef pn = cast(ssei(0, 0, 0x80000000, 0x80000000));
	ssef Psplat[3], idirsplat[3];
#  if BVH_FEATURE(BVH_HAIR)
	ssef tnear(0.0f), tfar(isect_t);
#  endif
	shuffle_swap_t shufflexyz[3];

	Psplat[0] = ssef(P.x);
	Psplat[1] = ssef(P.y);
	Psplat[2] = ssef(P.z);

	ssef tsplat(0.0f, 0.0f, -isect_t, -isect_t);

	gen_idirsplat_swap(pn, shuf_identity, shuf_swap, idir, idirsplat, shufflexyz);
#endif

	IsectPrecalc isect_precalc;
	triangle_intersect_precalc(dir, &isect_precalc);

	/* traversal loop */
	do {
		do {
			/* traverse internal nodes */
			while(nodeAddr >= 0 && nodeAddr != ENTRYPOINT_SENTINEL) {
				int nodeAddrChild1, traverse_mask;
				float dist[2];
				float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);

#if !defined(__KERNEL_SSE2__)
				traverse_mask = NODE_INTERSECT(kg,
				                               P,
#  if BVH_FEATURE(BVH_HAIR)
				                               dir,
#  endif
				                               idir,
				                               isect_t,
				                               nodeAddr,
				                               PATH_RAY_ALL_VISIBILITY,
				                               dist);
#else // __KERNEL_SSE2__
				traverse_mask = NODE_INTERSECT(kg,
				                               P,
				                               dir,
#  if BVH_FEATURE(BVH_HAIR)
				                               tnear,
				                               tfar,
#  endif
				                               tsplat,
				                               Psplat,
				                               idirsplat,
				                               shufflexyz,
				                               nodeAddr,
				                               PATH_RAY_ALL_VISIBILITY,
				                               dist);
#endif // __KERNEL_SSE2__

				nodeAddr = __float_as_int(cnodes.z);
				nodeAddrChild1 = __float_as_int(cnodes.w);

				if(traverse_mask == 3) {
					/* Both children were intersected, push the farther one. */
					bool closestChild1 = (dist[1] < dist[0]);

					if(closestChild1) {
						int tmp = nodeAddr;
						nodeAddr = nodeAddrChild1;
						nodeAddrChild1 = tmp;
					}

					++stackPtr;
					kernel_assert(stackPtr < BVH_STACK_SIZE);
					traversalStack[stackPtr] = nodeAddrChild1;
				}
				else {
					/* One child was intersected. */
					if(traverse_mask == 2) {
						nodeAddr = nodeAddrChild1;
					}
					else if(traverse_mask == 0) {
						/* Neither child was intersected. */
						nodeAddr = traversalStack[stackPtr];
						--stackPtr;
					}
				}
			}

			/* if node is leaf, fetch triangle list */
			if(nodeAddr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_leaf_nodes, (-nodeAddr-1));
				int primAddr = __float_as_int(leaf.x);

				const int primAddr2 = __float_as_int(leaf.y);
				const uint type = __float_as_int(leaf.w);

				/* pop */
				nodeAddr = traversalStack[stackPtr];
				--stackPtr;

				/* primitive intersection */
				switch(type & PRIMITIVE_ALL) {
					case PRIMITIVE_TRIANGLE: {
						/* intersect ray against primitive */
						for(; primAddr < primAddr2; primAddr++) {
							kernel_assert(kernel_tex_fetch(__prim_type, primAddr) == type);
							triangle_intersect_subsurface(kg,
							                              &isect_precalc,
							                              ss_isect,
							                              P,
							                              object,
							                              primAddr,
							                              isect_t,
							                              lcg_state,
							                              max_hits);
						}
						break;
					}
#if BVH_FEATURE(BVH_MOTION)
					case PRIMITIVE_MOTION_TRIANGLE: {
						/* intersect ray against primitive */
						for(; primAddr < primAddr2; primAddr++) {
							kernel_assert(kernel_tex_fetch(__prim_type, primAddr) == type);
							motion_triangle_intersect_subsurface(kg,
							                                     ss_isect,
							                                     P,
							                                     dir,
							                                     ray->time,
							                                     object,
							                                     primAddr,
							                                     isect_t,
							                                     lcg_state,
							                                     max_hits);
						}
						break;
					}
#endif
					default: {
						break;
					}
				}
			}
		} while(nodeAddr != ENTRYPOINT_SENTINEL);
	} while(nodeAddr != ENTRYPOINT_SENTINEL);
}

ccl_device_inline void BVH_FUNCTION_NAME(KernelGlobals *kg,
                                         const Ray *ray,
                                         SubsurfaceIntersection *ss_isect,
                                         int subsurface_object,
                                         uint *lcg_state,
                                         int max_hits)
{
#ifdef __QBVH__
	if(kernel_data.bvh.use_qbvh) {
		return BVH_FUNCTION_FULL_NAME(QBVH)(kg,
		                                    ray,
		                                    ss_isect,
		                                    subsurface_object,
		                                    lcg_state,
		                                    max_hits);
	}
	else
#endif
	{
		kernel_assert(kernel_data.bvh.use_qbvh == false);
		return BVH_FUNCTION_FULL_NAME(BVH)(kg,
		                                   ray,
		                                   ss_isect,
		                                   subsurface_object,
		                                   lcg_state,
		                                   max_hits);
	}
}

#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES
#undef NODE_INTERSECT

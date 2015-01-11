/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation,
 * and code copyright 2009-2012 Intel Corporation
 *
 * Modifications Copyright 2011-2014, Blender Foundation.
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
 * BVH_HAIR_MINIMUM_WIDTH: hair curve rendering with minimum width
 * BVH_MOTION: motion blur rendering
 *
 */

ccl_device bool BVH_FUNCTION_FULL_NAME(QBVH)(KernelGlobals *kg,
                                             const Ray *ray,
                                             Intersection *isect,
                                             const uint visibility
#if BVH_FEATURE(BVH_HAIR_MINIMUM_WIDTH)
                                             ,uint *lcg_state,
                                             float difl,
                                             float extmax
#endif
                                             )
{
	/* TODO(sergey):
	 * - Test if pushing distance on the stack helps (for non shadow rays).
	 * - Separate version for shadow rays.
	 * - Likely and unlikely for if() statements.
	 * - Test restrict attribute for pointers.
	 */

	/* Traversal stack in CUDA thread-local memory. */
	QBVHStackItem traversalStack[BVH_QSTACK_SIZE];
	traversalStack[0].addr = ENTRYPOINT_SENTINEL;
	traversalStack[0].dist = -FLT_MAX;

	/* Traversal variables in registers. */
	int stackPtr = 0;
	int nodeAddr = kernel_data.bvh.root;
	float nodeDist = -FLT_MAX;

	/* Ray parameters in registers. */
	float3 P = ray->P;
	float3 dir = bvh_clamp_direction(ray->D);
	float3 idir = bvh_inverse_direction(dir);
	int object = OBJECT_NONE;

#if BVH_FEATURE(BVH_MOTION)
	Transform ob_tfm;
#endif

	isect->t = ray->t;
	isect->u = 0.0f;
	isect->v = 0.0f;
	isect->prim = PRIM_NONE;
	isect->object = OBJECT_NONE;

#if defined(__KERNEL_DEBUG__)
	isect->num_traversal_steps = 0;
#endif

	ssef tnear(0.0f), tfar(ray->t);
	sse3f idir4(ssef(idir.x), ssef(idir.y), ssef(idir.z));

#ifdef __KERNEL_AVX2__
	float3 P_idir = P*idir;
	sse3f P_idir4 = sse3f(P_idir.x, P_idir.y, P_idir.z);
#else
	sse3f org = sse3f(ssef(P.x), ssef(P.y), ssef(P.z));
#endif

	/* Offsets to select the side that becomes the lower or upper bound. */
	int near_x, near_y, near_z;
	int far_x, far_y, far_z;

	if(idir.x >= 0.0f) { near_x = 0; far_x = 1; } else { near_x = 1; far_x = 0; }
	if(idir.y >= 0.0f) { near_y = 2; far_y = 3; } else { near_y = 3; far_y = 2; }
	if(idir.z >= 0.0f) { near_z = 4; far_z = 5; } else { near_z = 5; far_z = 4; }

	IsectPrecalc isect_precalc;
	triangle_intersect_precalc(dir, &isect_precalc);

	/* Traversal loop. */
	do {
		do {
			/* Traverse internal nodes. */
			while(nodeAddr >= 0 && nodeAddr != ENTRYPOINT_SENTINEL) {
				if(UNLIKELY(nodeDist > isect->t)) {
					/* Pop. */
					nodeAddr = traversalStack[stackPtr].addr;
					nodeDist = traversalStack[stackPtr].dist;
					--stackPtr;
					continue;
				}

				int traverseChild;
				ssef dist;

#if defined(__KERNEL_DEBUG__)
				isect->num_traversal_steps++;
#endif

#if BVH_FEATURE(BVH_HAIR_MINIMUM_WIDTH)
				if(difl != 0.0f) {
					/* NOTE: We extend all the child BB instead of fetching
					 * and checking visibility flags for each of the,
					 *
					 * Need to test if doing opposite would be any faster.
					 */
					traverseChild = qbvh_node_intersect_robust(kg,
					                                           tnear,
					                                           tfar,
#ifdef __KERNEL_AVX2__
					                                           P_idir4,
#else
					                                           org,
#endif
					                                           idir4,
					                                           near_x, near_y, near_z,
					                                           far_x, far_y, far_z,
					                                           nodeAddr,
					                                           difl,
					                                           &dist);
				}
				else
#endif
				{
					traverseChild = qbvh_node_intersect(kg,
					                                    tnear,
					                                    tfar,
#ifdef __KERNEL_AVX2__
					                                    P_idir4,
#else
					                                    org,
#endif
					                                    idir4,
					                                    near_x, near_y, near_z,
					                                    far_x, far_y, far_z,
					                                    nodeAddr,
					                                    &dist);
				}

				if(traverseChild != 0) {
					float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_QNODE_SIZE+6);

					/* One child is hit, continue with that child. */
					int r = __bscf(traverseChild);
					float d0 = ((float*)&dist)[r];
					if(traverseChild == 0) {
						nodeAddr = __float_as_int(cnodes[r]);
						nodeDist = d0;
						continue;
					}

					/* Two children are hit, push far child, and continue with
					 * closer child.
					 */
					int c0 = __float_as_int(cnodes[r]);
					r = __bscf(traverseChild);
					int c1 = __float_as_int(cnodes[r]);
					float d1 = ((float*)&dist)[r];
					if(traverseChild == 0) {
						if(d1 < d0) {
							nodeAddr = c1;
							++stackPtr;
							kernel_assert(stackPtr < BVH_QSTACK_SIZE);
							traversalStack[stackPtr].addr = c0;
							traversalStack[stackPtr].dist = d0;
							continue;
						}
						else {
							nodeAddr = c0;
							++stackPtr;
							kernel_assert(stackPtr < BVH_QSTACK_SIZE);
							traversalStack[stackPtr].addr = c1;
							traversalStack[stackPtr].dist = d1;
							continue;
						}
					}

					/* Here starts the slow path for 3 or 4 hit children. We push
					 * all nodes onto the stack to sort them there.
					 */
					++stackPtr;
					kernel_assert(stackPtr < BVH_QSTACK_SIZE);
					traversalStack[stackPtr].addr = c1;
					traversalStack[stackPtr].dist = d1;
					++stackPtr;
					kernel_assert(stackPtr < BVH_QSTACK_SIZE);
					traversalStack[stackPtr].addr = c0;
					traversalStack[stackPtr].dist = d0;

					/* Three children are hit, push all onto stack and sort 3
					 * stack items, continue with closest child.
					 */
					r = __bscf(traverseChild);
					int c2 = __float_as_int(cnodes[r]);
					float d2 = ((float*)&dist)[r];
					if(traverseChild == 0) {
						++stackPtr;
						kernel_assert(stackPtr < BVH_QSTACK_SIZE);
						traversalStack[stackPtr].addr = c2;
						traversalStack[stackPtr].dist = d2;
						qbvh_stack_sort(&traversalStack[stackPtr],
						                &traversalStack[stackPtr - 1],
						                &traversalStack[stackPtr - 2]);
						nodeAddr = traversalStack[stackPtr].addr;
						nodeDist = traversalStack[stackPtr].dist;
						--stackPtr;
						continue;
					}

					/* Four children are hit, push all onto stack and sort 4
					 * stack items, continue with closest child.
					 */
					r = __bscf(traverseChild);
					int c3 = __float_as_int(cnodes[r]);
					float d3 = ((float*)&dist)[r];
					++stackPtr;
					kernel_assert(stackPtr < BVH_QSTACK_SIZE);
					traversalStack[stackPtr].addr = c3;
					traversalStack[stackPtr].dist = d3;
					++stackPtr;
					kernel_assert(stackPtr < BVH_QSTACK_SIZE);
					traversalStack[stackPtr].addr = c2;
					traversalStack[stackPtr].dist = d2;
					qbvh_stack_sort(&traversalStack[stackPtr],
					                &traversalStack[stackPtr - 1],
					                &traversalStack[stackPtr - 2],
					                &traversalStack[stackPtr - 3]);
				}

				nodeAddr = traversalStack[stackPtr].addr;
				nodeDist = traversalStack[stackPtr].dist;
				--stackPtr;
			}

			/* If node is leaf, fetch triangle list. */
			if(nodeAddr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_nodes, (-nodeAddr-1)*BVH_QNODE_SIZE+6);

#ifdef __VISIBILITY_FLAG__
				if(UNLIKELY((nodeDist > isect->t) || ((__float_as_uint(leaf.z) & visibility) == 0)))
#else
				if(UNLIKELY((nodeDist > isect->t)))
#endif
				{
					/* Pop. */
					nodeAddr = traversalStack[stackPtr].addr;
					nodeDist = traversalStack[stackPtr].dist;
					--stackPtr;
					continue;
				}

				int primAddr = __float_as_int(leaf.x);

#if BVH_FEATURE(BVH_INSTANCING)
				if(primAddr >= 0) {
#endif
					int primAddr2 = __float_as_int(leaf.y);
					const uint type = __float_as_int(leaf.w);

					/* Pop. */
					nodeAddr = traversalStack[stackPtr].addr;
					nodeDist = traversalStack[stackPtr].dist;
					--stackPtr;

					/* Primitive intersection. */
					switch(type & PRIMITIVE_ALL) {
						case PRIMITIVE_TRIANGLE: {
							for(; primAddr < primAddr2; primAddr++) {
#if defined(__KERNEL_DEBUG__)
								isect->num_traversal_steps++;
#endif
								kernel_assert(kernel_tex_fetch(__prim_type, primAddr) == type);
								if(triangle_intersect(kg, &isect_precalc, isect, P, dir, visibility, object, primAddr)) {
									tfar = ssef(isect->t);
									/* Shadow ray early termination. */
									if(visibility == PATH_RAY_SHADOW_OPAQUE)
										return true;
								}
							}
							break;
						}
#if BVH_FEATURE(BVH_MOTION)
						case PRIMITIVE_MOTION_TRIANGLE: {
							for(; primAddr < primAddr2; primAddr++) {
#if defined(__KERNEL_DEBUG__)
								isect->num_traversal_steps++;
#endif
								kernel_assert(kernel_tex_fetch(__prim_type, primAddr) == type);
								if(motion_triangle_intersect(kg, isect, P, dir, ray->time, visibility, object, primAddr)) {
									tfar = ssef(isect->t);
									/* Shadow ray early termination. */
									if(visibility == PATH_RAY_SHADOW_OPAQUE)
										return true;
								}
							}
							break;
						}
#endif  /* BVH_FEATURE(BVH_MOTION) */
#if BVH_FEATURE(BVH_HAIR)
						case PRIMITIVE_CURVE:
						case PRIMITIVE_MOTION_CURVE: {
							for(; primAddr < primAddr2; primAddr++) {
#if defined(__KERNEL_DEBUG__)
								isect->num_traversal_steps++;
#endif
								kernel_assert(kernel_tex_fetch(__prim_type, primAddr) == type);
								bool hit;
								if(kernel_data.curve.curveflags & CURVE_KN_INTERPOLATE)
									hit = bvh_cardinal_curve_intersect(kg, isect, P, dir, visibility, object, primAddr, ray->time, type, lcg_state, difl, extmax);
								else
									hit = bvh_curve_intersect(kg, isect, P, dir, visibility, object, primAddr, ray->time, type, lcg_state, difl, extmax);
								if(hit) {
									tfar = ssef(isect->t);
									/* Shadow ray early termination. */
									if(visibility == PATH_RAY_SHADOW_OPAQUE)
										return true;
								}
							}
							break;
						}
#endif  /* BVH_FEATURE(BVH_HAIR) */
					}
				}
#if BVH_FEATURE(BVH_INSTANCING)
				else {
					/* Instance push. */
					object = kernel_tex_fetch(__prim_object, -primAddr-1);

#if BVH_FEATURE(BVH_MOTION)
					qbvh_instance_motion_push(kg, object, ray, &P, &dir, &idir, &isect->t, &nodeDist, &ob_tfm);
#else
					qbvh_instance_push(kg, object, ray, &P, &dir, &idir, &isect->t, &nodeDist);
#endif

					if(idir.x >= 0.0f) { near_x = 0; far_x = 1; } else { near_x = 1; far_x = 0; }
					if(idir.y >= 0.0f) { near_y = 2; far_y = 3; } else { near_y = 3; far_y = 2; }
					if(idir.z >= 0.0f) { near_z = 4; far_z = 5; } else { near_z = 5; far_z = 4; }
					tfar = ssef(isect->t);
					idir4 = sse3f(ssef(idir.x), ssef(idir.y), ssef(idir.z));
#ifdef __KERNEL_AVX2__
					P_idir = P*idir;
					P_idir4 = sse3f(P_idir.x, P_idir.y, P_idir.z);
#else
					org = sse3f(ssef(P.x), ssef(P.y), ssef(P.z));
#endif
					triangle_intersect_precalc(dir, &isect_precalc);

					++stackPtr;
					kernel_assert(stackPtr < BVH_QSTACK_SIZE);
					traversalStack[stackPtr].addr = ENTRYPOINT_SENTINEL;
					traversalStack[stackPtr].dist = -FLT_MAX;

					nodeAddr = kernel_tex_fetch(__object_node, object);
				}
			}
#endif  /* FEATURE(BVH_INSTANCING) */
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

#if BVH_FEATURE(BVH_INSTANCING)
		if(stackPtr >= 0) {
			kernel_assert(object != OBJECT_NONE);

			/* Instance pop. */
#if BVH_FEATURE(BVH_MOTION)
			bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, &isect->t, &ob_tfm);
#else
			bvh_instance_pop(kg, object, ray, &P, &dir, &idir, &isect->t);
#endif

			if(idir.x >= 0.0f) { near_x = 0; far_x = 1; } else { near_x = 1; far_x = 0; }
			if(idir.y >= 0.0f) { near_y = 2; far_y = 3; } else { near_y = 3; far_y = 2; }
			if(idir.z >= 0.0f) { near_z = 4; far_z = 5; } else { near_z = 5; far_z = 4; }
			tfar = ssef(isect->t);
			idir4 = sse3f(ssef(idir.x), ssef(idir.y), ssef(idir.z));
#ifdef __KERNEL_AVX2__
			P_idir = P*idir;
			P_idir4 = sse3f(P_idir.x, P_idir.y, P_idir.z);
#else
			org = sse3f(ssef(P.x), ssef(P.y), ssef(P.z));
#endif
			triangle_intersect_precalc(dir, &isect_precalc);

			object = OBJECT_NONE;
			nodeAddr = traversalStack[stackPtr].addr;
			nodeDist = traversalStack[stackPtr].dist;
			--stackPtr;
		}
#endif  /* FEATURE(BVH_INSTANCING) */
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return (isect->prim != PRIM_NONE);
}

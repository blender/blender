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

/* This is a template BVH traversal function for volumes, where
 * various features can be enabled/disabled. This way we can compile optimized
 * versions for each case without new features slowing things down.
 *
 * BVH_INSTANCING: object instancing
 * BVH_MOTION: motion blur rendering
 *
 */

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT obvh_node_intersect
#else
#  define NODE_INTERSECT obvh_aligned_node_intersect
#endif

ccl_device bool BVH_FUNCTION_FULL_NAME(OBVH)(KernelGlobals *kg,
                                             const Ray *ray,
                                             Intersection *isect,
                                             const uint visibility)
{
	/* Traversal stack in CUDA thread-local memory. */
	OBVHStackItem traversal_stack[BVH_OSTACK_SIZE];
	traversal_stack[0].addr = ENTRYPOINT_SENTINEL;

	/* Traversal variables in registers. */
	int stack_ptr = 0;
	int node_addr = kernel_data.bvh.root;

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

	avxf tnear(0.0f), tfar(ray->t);
#if BVH_FEATURE(BVH_HAIR)
	avx3f dir4(avxf(dir.x), avxf(dir.y), avxf(dir.z));
#endif
	avx3f idir4(avxf(idir.x), avxf(idir.y), avxf(idir.z));

#ifdef __KERNEL_AVX2__
	float3 P_idir = P*idir;
	avx3f P_idir4(P_idir.x, P_idir.y, P_idir.z);
#endif
#if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
	avx3f org4(avxf(P.x), avxf(P.y), avxf(P.z));
#endif

	/* Offsets to select the side that becomes the lower or upper bound. */
	int near_x, near_y, near_z;
	int far_x, far_y, far_z;
	obvh_near_far_idx_calc(idir,
	                       &near_x, &near_y, &near_z,
	                       &far_x, &far_y, &far_z);

	/* Traversal loop. */
	do {
		do {
			/* Traverse internal nodes. */
			while(node_addr >= 0 && node_addr != ENTRYPOINT_SENTINEL) {
				float4 inodes = kernel_tex_fetch(__bvh_nodes, node_addr+0);

#ifdef __VISIBILITY_FLAG__
				if((__float_as_uint(inodes.x) & visibility) == 0) {
					/* Pop. */
					node_addr = traversal_stack[stack_ptr].addr;
					--stack_ptr;
					continue;
				}
#endif

				avxf dist;
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
					avxf cnodes;
#if BVH_FEATURE(BVH_HAIR)
					if(__float_as_uint(inodes.x) & PATH_RAY_NODE_UNALIGNED) {
						cnodes = kernel_tex_fetch_avxf(__bvh_nodes, node_addr+26);
					}
					else
#endif
					{
						cnodes = kernel_tex_fetch_avxf(__bvh_nodes, node_addr+14);
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
							kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
							traversal_stack[stack_ptr].addr = c0;
							traversal_stack[stack_ptr].dist = d0;
							continue;
						}
						else {
							node_addr = c0;
							++stack_ptr;
							kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
							traversal_stack[stack_ptr].addr = c1;
							traversal_stack[stack_ptr].dist = d1;
							continue;
						}
					}

					/* Here starts the slow path for 3 or 4 hit children. We push
					 * all nodes onto the stack to sort them there.
					 */
					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c1;
					traversal_stack[stack_ptr].dist = d1;
					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
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
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c2;
						traversal_stack[stack_ptr].dist = d2;
						obvh_stack_sort(&traversal_stack[stack_ptr],
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
					if(child_mask == 0) {
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c3;
						traversal_stack[stack_ptr].dist = d3;
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c2;
						traversal_stack[stack_ptr].dist = d2;
						obvh_stack_sort(&traversal_stack[stack_ptr],
						                &traversal_stack[stack_ptr - 1],
						                &traversal_stack[stack_ptr - 2],
						                &traversal_stack[stack_ptr - 3]);
						node_addr = traversal_stack[stack_ptr].addr;
						--stack_ptr;
						continue;
					}

					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c3;
					traversal_stack[stack_ptr].dist = d3;
					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c2;
					traversal_stack[stack_ptr].dist = d2;

					/* Five children are hit, push all onto stack and sort 5
					 * stack items, continue with closest child
					 */
					r = __bscf(child_mask);
					int c4 = __float_as_int(cnodes[r]);
					float d4 = ((float*)&dist)[r];
					if(child_mask == 0) {
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c4;
						traversal_stack[stack_ptr].dist = d4;
						obvh_stack_sort(&traversal_stack[stack_ptr],
						                &traversal_stack[stack_ptr - 1],
						                &traversal_stack[stack_ptr - 2],
						                &traversal_stack[stack_ptr - 3],
						                &traversal_stack[stack_ptr - 4]);
						node_addr = traversal_stack[stack_ptr].addr;
						--stack_ptr;
						continue;
					}

					/* Six children are hit, push all onto stack and sort 6
					 * stack items, continue with closest child.
					 */
					r = __bscf(child_mask);
					int c5 = __float_as_int(cnodes[r]);
					float d5 = ((float*)&dist)[r];
					if(child_mask == 0) {
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c5;
						traversal_stack[stack_ptr].dist = d5;
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c4;
						traversal_stack[stack_ptr].dist = d4;
						obvh_stack_sort(&traversal_stack[stack_ptr],
						                &traversal_stack[stack_ptr - 1],
						                &traversal_stack[stack_ptr - 2],
						                &traversal_stack[stack_ptr - 3],
						                &traversal_stack[stack_ptr - 4],
						                &traversal_stack[stack_ptr - 5]);
						node_addr = traversal_stack[stack_ptr].addr;
						--stack_ptr;
						continue;
					}

					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c5;
					traversal_stack[stack_ptr].dist = d5;
					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c4;
					traversal_stack[stack_ptr].dist = d4;

					/* Seven children are hit, push all onto stack and sort 7
					 * stack items, continue with closest child.
					 */
					r = __bscf(child_mask);
					int c6 = __float_as_int(cnodes[r]);
					float d6 = ((float*)&dist)[r];
					if(child_mask == 0) {
						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = c6;
						traversal_stack[stack_ptr].dist = d6;
						obvh_stack_sort(&traversal_stack[stack_ptr],
						                &traversal_stack[stack_ptr - 1],
						                &traversal_stack[stack_ptr - 2],
						                &traversal_stack[stack_ptr - 3],
						                &traversal_stack[stack_ptr - 4],
						                &traversal_stack[stack_ptr - 5],
						                &traversal_stack[stack_ptr - 6]);
						node_addr = traversal_stack[stack_ptr].addr;
						--stack_ptr;
						continue;
					}

					/* Eight children are hit, push all onto stack and sort 8
					 * stack items, continue with closest child.
					 */
					r = __bscf(child_mask);
					int c7 = __float_as_int(cnodes[r]);
					float d7 = ((float*)&dist)[r];
					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c7;
					traversal_stack[stack_ptr].dist = d7;
					++stack_ptr;
					kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
					traversal_stack[stack_ptr].addr = c6;
					traversal_stack[stack_ptr].dist = d6;
					obvh_stack_sort(&traversal_stack[stack_ptr],
					                &traversal_stack[stack_ptr - 1],
					                &traversal_stack[stack_ptr - 2],
					                &traversal_stack[stack_ptr - 3],
					                &traversal_stack[stack_ptr - 4],
					                &traversal_stack[stack_ptr - 5],
					                &traversal_stack[stack_ptr - 6],
					                &traversal_stack[stack_ptr - 7]);
					node_addr = traversal_stack[stack_ptr].addr;
					--stack_ptr;
					continue;
				}

				node_addr = traversal_stack[stack_ptr].addr;
				--stack_ptr;
			}

			/* If node is leaf, fetch triangle list. */
			if(node_addr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_leaf_nodes, (-node_addr-1));

				if((__float_as_uint(leaf.z) & visibility) == 0) {
					/* Pop. */
					node_addr = traversal_stack[stack_ptr].addr;
					--stack_ptr;
					continue;
				}

				int prim_addr = __float_as_int(leaf.x);

#if BVH_FEATURE(BVH_INSTANCING)
				if(prim_addr >= 0) {
#endif
					int prim_addr2 = __float_as_int(leaf.y);
					const uint type = __float_as_int(leaf.w);
					const uint p_type = type & PRIMITIVE_ALL;

					/* Pop. */
					node_addr = traversal_stack[stack_ptr].addr;
					--stack_ptr;

					/* Primitive intersection. */
					switch(p_type) {
						case PRIMITIVE_TRIANGLE: {
							for(; prim_addr < prim_addr2; prim_addr++) {
								kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
								/* Only primitives from volume object. */
								uint tri_object = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, prim_addr): object;
								int object_flag = kernel_tex_fetch(__object_flag, tri_object);
								if((object_flag & SD_OBJECT_HAS_VOLUME) == 0) {
									continue;
								}
								/* Intersect ray against primitive. */
								triangle_intersect(kg, isect, P, dir, visibility, object, prim_addr);
							}
							break;
						}
#if BVH_FEATURE(BVH_MOTION)
						case PRIMITIVE_MOTION_TRIANGLE: {
							for(; prim_addr < prim_addr2; prim_addr++) {
								kernel_assert(kernel_tex_fetch(__prim_type, prim_addr) == type);
								/* Only primitives from volume object. */
								uint tri_object = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, prim_addr): object;
								int object_flag = kernel_tex_fetch(__object_flag, tri_object);
								if((object_flag & SD_OBJECT_HAS_VOLUME) == 0) {
									continue;
								}
								/* Intersect ray against primitive. */
								motion_triangle_intersect(kg, isect, P, dir, ray->time, visibility, object, prim_addr);
							}
							break;
						}
#endif
					}
				}
#if BVH_FEATURE(BVH_INSTANCING)
				else {
					/* Instance push. */
					object = kernel_tex_fetch(__prim_object, -prim_addr-1);
					int object_flag = kernel_tex_fetch(__object_flag, object);
					if(object_flag & SD_OBJECT_HAS_VOLUME) {
#  if BVH_FEATURE(BVH_MOTION)
						isect->t = bvh_instance_motion_push(kg, object, ray, &P, &dir, &idir, isect->t, &ob_itfm);
#  else
						isect->t = bvh_instance_push(kg, object, ray, &P, &dir, &idir, isect->t);
#  endif

						obvh_near_far_idx_calc(idir,
						                       &near_x, &near_y, &near_z,
						                       &far_x, &far_y, &far_z);
						tfar = avxf(isect->t);
#  if BVH_FEATURE(BVH_HAIR)
						dir4 = avx3f(avxf(dir.x), avxf(dir.y), avxf(dir.z));
#  endif
						idir4 = avx3f(avxf(idir.x), avxf(idir.y), avxf(idir.z));
#  ifdef __KERNEL_AVX2__
						P_idir = P*idir;
						P_idir4 = avx3f(P_idir.x, P_idir.y, P_idir.z);
#  endif
#  if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
						org4 = avx3f(avxf(P.x), avxf(P.y), avxf(P.z));
#  endif

						++stack_ptr;
						kernel_assert(stack_ptr < BVH_OSTACK_SIZE);
						traversal_stack[stack_ptr].addr = ENTRYPOINT_SENTINEL;

						node_addr = kernel_tex_fetch(__object_node, object);
					}
					else {
						/* Pop. */
						object = OBJECT_NONE;
						node_addr = traversal_stack[stack_ptr].addr;
						--stack_ptr;
					}
				}
			}
#endif  /* FEATURE(BVH_INSTANCING) */
		} while(node_addr != ENTRYPOINT_SENTINEL);

#if BVH_FEATURE(BVH_INSTANCING)
		if(stack_ptr >= 0) {
			kernel_assert(object != OBJECT_NONE);

			/* Instance pop. */
#  if BVH_FEATURE(BVH_MOTION)
			isect->t = bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, isect->t, &ob_itfm);
#  else
			isect->t = bvh_instance_pop(kg, object, ray, &P, &dir, &idir, isect->t);
#  endif

			obvh_near_far_idx_calc(idir,
			                       &near_x, &near_y, &near_z,
			                       &far_x, &far_y, &far_z);
			tfar = avxf(isect->t);
#  if BVH_FEATURE(BVH_HAIR)
			dir4 = avx3f(avxf(dir.x), avxf(dir.y), avxf(dir.z));
#  endif
			idir4 = avx3f(avxf(idir.x), avxf(idir.y), avxf(idir.z));
#  ifdef __KERNEL_AVX2__
			P_idir = P*idir;
			P_idir4 = avx3f(P_idir.x, P_idir.y, P_idir.z);
#  endif
#  if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
			org4 = avx3f(avxf(P.x), avxf(P.y), avxf(P.z));
#  endif

			object = OBJECT_NONE;
			node_addr = traversal_stack[stack_ptr].addr;
			--stack_ptr;
		}
#endif  /* FEATURE(BVH_INSTANCING) */
	} while(node_addr != ENTRYPOINT_SENTINEL);

	return (isect->prim != PRIM_NONE);
}

#undef NODE_INTERSECT

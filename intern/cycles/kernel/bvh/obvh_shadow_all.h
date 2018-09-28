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
 *
 */

#if BVH_FEATURE(BVH_HAIR)
#  define NODE_INTERSECT obvh_node_intersect
#else
#  define NODE_INTERSECT obvh_aligned_node_intersect
#endif

ccl_device bool BVH_FUNCTION_FULL_NAME(OBVH)(KernelGlobals *kg,
                                             const Ray *ray,
                                             Intersection *isect_array,
                                             const int skip_object,
                                             const uint max_hits,
                                             uint *num_hits)
{
	/* TODO(sergey):
	 *  - Test if pushing distance on the stack helps.
	 * - Likely and unlikely for if() statements.
	 * - Test restrict attribute for pointers.
	 */

	/* Traversal stack in CUDA thread-local memory. */
	OBVHStackItem traversal_stack[BVH_OSTACK_SIZE];
	traversal_stack[0].addr = ENTRYPOINT_SENTINEL;

	/* Traversal variables in registers. */
	int stack_ptr = 0;
	int node_addr = kernel_data.bvh.root;

	/* Ray parameters in registers. */
	const float tmax = ray->t;
	float3 P = ray->P;
	float3 dir = bvh_clamp_direction(ray->D);
	float3 idir = bvh_inverse_direction(dir);
	int object = OBJECT_NONE;
	float isect_t = tmax;

#if BVH_FEATURE(BVH_MOTION)
	Transform ob_itfm;
#endif

	*num_hits = 0;
	isect_array->t = tmax;

#if BVH_FEATURE(BVH_INSTANCING)
	int num_hits_in_instance = 0;
#endif

	avxf tnear(0.0f), tfar(isect_t);
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
				(void)inodes;

				if(false
#ifdef __VISIBILITY_FLAG__
				   || ((__float_as_uint(inodes.x) & PATH_RAY_SHADOW) == 0)
#endif
#if BVH_FEATURE(BVH_MOTION)
				   || UNLIKELY(ray->time < inodes.y)
				   || UNLIKELY(ray->time > inodes.z)
#endif
				) {
					/* Pop. */
					node_addr = traversal_stack[stack_ptr].addr;
					--stack_ptr;
					continue;
				}

				avxf dist;
				int child_mask = NODE_INTERSECT(kg,
				                                tnear,
				                                tfar,
#ifdef __KERNEL_AVX2__
					                            P_idir4,
#endif
#if BVH_FEATURE(BVH_HAIR) || !defined(__KERNEL_AVX2__)
//#if !defined(__KERNEL_AVX2__)
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
#ifdef __VISIBILITY_FLAG__
				if((__float_as_uint(leaf.z) & PATH_RAY_SHADOW) == 0) {
					/* Pop. */
					node_addr = traversal_stack[stack_ptr].addr;
					--stack_ptr;
					continue;
				}
#endif

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
					if(p_type == PRIMITIVE_TRIANGLE) {
						int prim_count = prim_addr2 - prim_addr;
						if(prim_count < 3) {
							while(prim_addr < prim_addr2) {
								kernel_assert((kernel_tex_fetch(__prim_type, prim_addr) & PRIMITIVE_ALL) == p_type);
								int hit = triangle_intersect(kg,
								                             isect_array,
								                             P,
								                             dir,
								                             PATH_RAY_SHADOW,
								                             object,
								                             prim_addr);
								/* Shadow ray early termination. */
								if(hit) {
									/* detect if this surface has a shader with transparent shadows */

									/* todo: optimize so primitive visibility flag indicates if
									 * the primitive has a transparent shadow shader? */
									int prim = kernel_tex_fetch(__prim_index, isect_array->prim);
									int shader = 0;

#ifdef __HAIR__
									if(kernel_tex_fetch(__prim_type, isect_array->prim) & PRIMITIVE_ALL_TRIANGLE)
#endif
									{
										shader = kernel_tex_fetch(__tri_shader, prim);
									}
#ifdef __HAIR__
									else {
										float4 str = kernel_tex_fetch(__curves, prim);
										shader = __float_as_int(str.z);
									}
#endif
									int flag = kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).flags;

									/* if no transparent shadows, all light is blocked */
									if(!(flag & SD_HAS_TRANSPARENT_SHADOW)) {
										return true;
									}
									/* if maximum number of hits reached, block all light */
									else if(*num_hits == max_hits) {
										return true;
									}

									/* move on to next entry in intersections array */
									isect_array++;
									(*num_hits)++;
#if BVH_FEATURE(BVH_INSTANCING)
									num_hits_in_instance++;
#endif

									isect_array->t = isect_t;
								}

								prim_addr++;
							} //while
					} else {
							kernel_assert((kernel_tex_fetch(__prim_type, (prim_addr)) & PRIMITIVE_ALL) == p_type);

#if BVH_FEATURE(BVH_INSTANCING)
							int* nhiptr = &num_hits_in_instance;
#else
							int nhi= 0;
							int *nhiptr = &nhi;
#endif

							int result = triangle_intersect8(kg,
							                                 &isect_array,
							                                 P,
							                                 dir,
							                                 PATH_RAY_SHADOW,
							                                 object,
							                                 prim_addr,
							                                 prim_count,
							                                 num_hits,
							                                 max_hits,
							                                 nhiptr,
							                                 isect_t);
							if(result == 2) {
								return true;
							}
						}   // prim_count
					}  // PRIMITIVE_TRIANGLE
					else {
							while(prim_addr < prim_addr2) {
							kernel_assert((kernel_tex_fetch(__prim_type, prim_addr) & PRIMITIVE_ALL) == p_type);

#ifdef __SHADOW_TRICKS__
							uint tri_object = (object == OBJECT_NONE)
									? kernel_tex_fetch(__prim_object, prim_addr)
									: object;
							if(tri_object == skip_object) {
								++prim_addr;
								continue;
							}
#endif

							bool hit;

							/* todo: specialized intersect functions which don't fill in
							 * isect unless needed and check SD_HAS_TRANSPARENT_SHADOW?
							 * might give a few % performance improvement */

							switch(p_type) {

#if BVH_FEATURE(BVH_MOTION)
								case PRIMITIVE_MOTION_TRIANGLE: {
									hit = motion_triangle_intersect(kg,
									                                isect_array,
									                                P,
									                                dir,
									                                ray->time,
									                                PATH_RAY_SHADOW,
									                                object,
									                                prim_addr);
									break;
								}
#endif
#if BVH_FEATURE(BVH_HAIR)
								case PRIMITIVE_CURVE:
								case PRIMITIVE_MOTION_CURVE: {
									const uint curve_type = kernel_tex_fetch(__prim_type, prim_addr);
									if(kernel_data.curve.curveflags & CURVE_KN_INTERPOLATE) {
										hit = cardinal_curve_intersect(kg,
										                               isect_array,
										                               P,
										                               dir,
										                               PATH_RAY_SHADOW,
										                               object,
										                               prim_addr,
										                               ray->time,
										                               curve_type,
										                               NULL,
										                               0, 0);
									}
									else {
										hit = curve_intersect(kg,
										                      isect_array,
										                      P,
										                      dir,
										                      PATH_RAY_SHADOW,
										                      object,
										                      prim_addr,
										                      ray->time,
										                      curve_type,
										                      NULL,
										                      0, 0);
									}
									break;
								}
#endif
								default: {
									hit = false;
									break;
								}
							}

							/* Shadow ray early termination. */
							if(hit) {
								/* detect if this surface has a shader with transparent shadows */

								/* todo: optimize so primitive visibility flag indicates if
								 * the primitive has a transparent shadow shader? */
								int prim = kernel_tex_fetch(__prim_index, isect_array->prim);
								int shader = 0;

#ifdef __HAIR__
								if(kernel_tex_fetch(__prim_type, isect_array->prim) & PRIMITIVE_ALL_TRIANGLE)
#endif
								{
									shader = kernel_tex_fetch(__tri_shader, prim);
								}
#ifdef __HAIR__
								else {
									float4 str = kernel_tex_fetch(__curves, prim);
									shader = __float_as_int(str.z);
								}
#endif
								int flag = kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).flags;

								/* if no transparent shadows, all light is blocked */
								if(!(flag & SD_HAS_TRANSPARENT_SHADOW)) {
									return true;
								}
								/* if maximum number of hits reached, block all light */
								else if(*num_hits == max_hits) {
									return true;
								}

								/* move on to next entry in intersections array */
								isect_array++;
								(*num_hits)++;
#if BVH_FEATURE(BVH_INSTANCING)
								num_hits_in_instance++;
#endif

								isect_array->t = isect_t;
							}

							prim_addr++;
						}//while prim
					}
				}
#if BVH_FEATURE(BVH_INSTANCING)
				else {
					/* Instance push. */
					object = kernel_tex_fetch(__prim_object, -prim_addr-1);

#  if BVH_FEATURE(BVH_MOTION)
					isect_t = bvh_instance_motion_push(kg, object, ray, &P, &dir, &idir, isect_t, &ob_itfm);
#  else
					isect_t = bvh_instance_push(kg, object, ray, &P, &dir, &idir, isect_t);
#  endif

					num_hits_in_instance = 0;
					isect_array->t = isect_t;

					obvh_near_far_idx_calc(idir,
					                       &near_x, &near_y, &near_z,
					                       &far_x, &far_y, &far_z);
					tfar = avxf(isect_t);
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
			}
#endif  /* FEATURE(BVH_INSTANCING) */
		} while(node_addr != ENTRYPOINT_SENTINEL);

#if BVH_FEATURE(BVH_INSTANCING)
		if(stack_ptr >= 0) {
			kernel_assert(object != OBJECT_NONE);

			/* Instance pop. */
			if(num_hits_in_instance) {
				float t_fac;
#  if BVH_FEATURE(BVH_MOTION)
				bvh_instance_motion_pop_factor(kg, object, ray, &P, &dir, &idir, &t_fac, &ob_itfm);
#  else
				bvh_instance_pop_factor(kg, object, ray, &P, &dir, &idir, &t_fac);
#  endif
				/* Scale isect->t to adjust for instancing. */
				for(int i = 0; i < num_hits_in_instance; i++) {
					(isect_array-i-1)->t *= t_fac;
				}
			}
			else {
#  if BVH_FEATURE(BVH_MOTION)
				bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, FLT_MAX, &ob_itfm);
#  else
				bvh_instance_pop(kg, object, ray, &P, &dir, &idir, FLT_MAX);
#  endif
			}

			isect_t = tmax;
			isect_array->t = isect_t;

			obvh_near_far_idx_calc(idir,
			                       &near_x, &near_y, &near_z,
			                       &far_x, &far_y, &far_z);
			tfar = avxf(isect_t);
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

	return false;
}

#undef NODE_INTERSECT

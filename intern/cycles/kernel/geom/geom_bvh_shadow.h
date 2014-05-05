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

/* This is a template BVH traversal function, where various features can be
 * enabled/disabled. This way we can compile optimized versions for each case
 * without new features slowing things down.
 *
 * BVH_INSTANCING: object instancing
 * BVH_HAIR: hair curve rendering
 * BVH_MOTION: motion blur rendering
 *
 */

#define FEATURE(f) (((BVH_FUNCTION_FEATURES) & (f)) != 0)

ccl_device bool BVH_FUNCTION_NAME
(KernelGlobals *kg, const Ray *ray, Intersection *isect_array, const uint max_hits, uint *num_hits)
{
	/* todo:
	 * - likely and unlikely for if() statements
	 * - test restrict attribute for pointers
	 */
	
	/* traversal stack in CUDA thread-local memory */
	int traversalStack[BVH_STACK_SIZE];
	traversalStack[0] = ENTRYPOINT_SENTINEL;

	/* traversal variables in registers */
	int stackPtr = 0;
	int nodeAddr = kernel_data.bvh.root;

	/* ray parameters in registers */
	const float tmax = ray->t;
	float3 P = ray->P;
	float3 dir = bvh_clamp_direction(ray->D);
	float3 idir = bvh_inverse_direction(dir);
	int object = OBJECT_NONE;
	float isect_t = tmax;

#if FEATURE(BVH_MOTION)
	Transform ob_tfm;
#endif

#if FEATURE(BVH_INSTANCING)
	int num_hits_in_instance = 0;
#endif

	*num_hits = 0;
	isect_array->t = tmax;

#if defined(__KERNEL_SSE2__)
	const shuffle_swap_t shuf_identity = shuffle_swap_identity();
	const shuffle_swap_t shuf_swap = shuffle_swap_swap();
	
	const __m128 pn = _mm_castsi128_ps(_mm_set_epi32(0x80000000, 0x80000000, 0, 0));
	__m128 Psplat[3], idirsplat[3];
	shuffle_swap_t shufflexyz[3];

	Psplat[0] = _mm_set_ps1(P.x);
	Psplat[1] = _mm_set_ps1(P.y);
	Psplat[2] = _mm_set_ps1(P.z);

	__m128 tsplat = _mm_set_ps(-isect_t, -isect_t, 0.0f, 0.0f);

	gen_idirsplat_swap(pn, shuf_identity, shuf_swap, idir, idirsplat, shufflexyz);
#endif

	/* traversal loop */
	do {
		do {
			/* traverse internal nodes */
			while(nodeAddr >= 0 && nodeAddr != ENTRYPOINT_SENTINEL) {
				bool traverseChild0, traverseChild1;
				int nodeAddrChild1;

#if !defined(__KERNEL_SSE2__)
				/* Intersect two child bounding boxes, non-SSE version */
				float t = isect_t;

				/* fetch node data */
				float4 node0 = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+0);
				float4 node1 = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+1);
				float4 node2 = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+2);
				float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+3);

				/* intersect ray against child nodes */
				NO_EXTENDED_PRECISION float c0lox = (node0.x - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c0hix = (node0.z - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c0loy = (node1.x - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c0hiy = (node1.z - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c0loz = (node2.x - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c0hiz = (node2.z - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c0min = max4(min(c0lox, c0hix), min(c0loy, c0hiy), min(c0loz, c0hiz), 0.0f);
				NO_EXTENDED_PRECISION float c0max = min4(max(c0lox, c0hix), max(c0loy, c0hiy), max(c0loz, c0hiz), t);

				NO_EXTENDED_PRECISION float c1lox = (node0.y - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c1hix = (node0.w - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c1loy = (node1.y - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c1hiy = (node1.w - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c1loz = (node2.y - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c1hiz = (node2.w - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c1min = max4(min(c1lox, c1hix), min(c1loy, c1hiy), min(c1loz, c1hiz), 0.0f);
				NO_EXTENDED_PRECISION float c1max = min4(max(c1lox, c1hix), max(c1loy, c1hiy), max(c1loz, c1hiz), t);

				/* decide which nodes to traverse next */
#ifdef __VISIBILITY_FLAG__
				/* this visibility test gives a 5% performance hit, how to solve? */
				traverseChild0 = (c0max >= c0min) && (__float_as_uint(cnodes.z) & PATH_RAY_SHADOW);
				traverseChild1 = (c1max >= c1min) && (__float_as_uint(cnodes.w) & PATH_RAY_SHADOW);
#else
				traverseChild0 = (c0max >= c0min);
				traverseChild1 = (c1max >= c1min);
#endif

#else // __KERNEL_SSE2__
				/* Intersect two child bounding boxes, SSE3 version adapted from Embree */

				/* fetch node data */
				const __m128 *bvh_nodes = (__m128*)kg->__bvh_nodes.data + nodeAddr*BVH_NODE_SIZE;
				const float4 cnodes = ((float4*)bvh_nodes)[3];

				/* intersect ray against child nodes */
				const __m128 tminmaxx = _mm_mul_ps(_mm_sub_ps(shuffle_swap(bvh_nodes[0], shufflexyz[0]), Psplat[0]), idirsplat[0]);
				const __m128 tminmaxy = _mm_mul_ps(_mm_sub_ps(shuffle_swap(bvh_nodes[1], shufflexyz[1]), Psplat[1]), idirsplat[1]);
				const __m128 tminmaxz = _mm_mul_ps(_mm_sub_ps(shuffle_swap(bvh_nodes[2], shufflexyz[2]), Psplat[2]), idirsplat[2]);

				/* calculate { c0min, c1min, -c0max, -c1max} */
				__m128 minmax = _mm_max_ps(_mm_max_ps(tminmaxx, tminmaxy), _mm_max_ps(tminmaxz, tsplat));
				const __m128 tminmax = _mm_xor_ps(minmax, pn);
				const __m128 lrhit = _mm_cmple_ps(tminmax, shuffle<2, 3, 0, 1>(tminmax));

				/* decide which nodes to traverse next */
#ifdef __VISIBILITY_FLAG__
				/* this visibility test gives a 5% performance hit, how to solve? */
				traverseChild0 = (_mm_movemask_ps(lrhit) & 1) && (__float_as_uint(cnodes.z) & PATH_RAY_SHADOW);
				traverseChild1 = (_mm_movemask_ps(lrhit) & 2) && (__float_as_uint(cnodes.w) & PATH_RAY_SHADOW);
#else
				traverseChild0 = (_mm_movemask_ps(lrhit) & 1);
				traverseChild1 = (_mm_movemask_ps(lrhit) & 2);
#endif
#endif // __KERNEL_SSE2__

				nodeAddr = __float_as_int(cnodes.x);
				nodeAddrChild1 = __float_as_int(cnodes.y);

				if(traverseChild0 && traverseChild1) {
					/* both children were intersected, push the farther one */
#if !defined(__KERNEL_SSE2__)
					bool closestChild1 = (c1min < c0min);
#else
					union { __m128 m128; float v[4]; } uminmax;
					uminmax.m128 = tminmax;
					bool closestChild1 = uminmax.v[1] < uminmax.v[0];
#endif

					if(closestChild1) {
						int tmp = nodeAddr;
						nodeAddr = nodeAddrChild1;
						nodeAddrChild1 = tmp;
					}

					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild1;
				}
				else {
					/* one child was intersected */
					if(traverseChild1) {
						nodeAddr = nodeAddrChild1;
					}
					else if(!traverseChild0) {
						/* neither child was intersected */
						nodeAddr = traversalStack[stackPtr];
						--stackPtr;
					}
				}
			}

			/* if node is leaf, fetch triangle list */
			if(nodeAddr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_nodes, (-nodeAddr-1)*BVH_NODE_SIZE+(BVH_NODE_SIZE-1));
				int primAddr = __float_as_int(leaf.x);

#if FEATURE(BVH_INSTANCING)
				if(primAddr >= 0) {
#endif
					int primAddr2 = __float_as_int(leaf.y);

					/* pop */
					nodeAddr = traversalStack[stackPtr];
					--stackPtr;

					/* primitive intersection */
					while(primAddr < primAddr2) {
						bool hit;
						uint type = kernel_tex_fetch(__prim_type, primAddr);

						/* todo: specialized intersect functions which don't fill in
						 * isect unless needed and check SD_HAS_TRANSPARENT_SHADOW?
						 * might give a few % performance improvement */

						switch(type & PRIMITIVE_ALL) {
							case PRIMITIVE_TRIANGLE: {
								hit = triangle_intersect(kg, isect_array, P, dir, PATH_RAY_SHADOW, object, primAddr);
								break;
							}
#if FEATURE(BVH_MOTION)
							case PRIMITIVE_MOTION_TRIANGLE: {
								hit = motion_triangle_intersect(kg, isect_array, P, dir, ray->time, PATH_RAY_SHADOW, object, primAddr);
								break;
							}
#endif
#if FEATURE(BVH_HAIR)
							case PRIMITIVE_CURVE:
							case PRIMITIVE_MOTION_CURVE: {
								if(kernel_data.curve.curveflags & CURVE_KN_INTERPOLATE) 
									hit = bvh_cardinal_curve_intersect(kg, isect_array, P, dir, PATH_RAY_SHADOW, object, primAddr, ray->time, type, NULL, 0, 0);
								else
									hit = bvh_curve_intersect(kg, isect_array, P, dir, PATH_RAY_SHADOW, object, primAddr, ray->time, type, NULL, 0, 0);
								break;
							}
#endif
							default: {
								hit = false;
								break;
							}
						}

						/* shadow ray early termination */
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
								float4 Ns = kernel_tex_fetch(__tri_normal, prim);
								shader = __float_as_int(Ns.w);
							}
#ifdef __HAIR__
							else {
								float4 str = kernel_tex_fetch(__curves, prim);
								shader = __float_as_int(str.z);
							}
#endif
							int flag = kernel_tex_fetch(__shader_flag, (shader & SHADER_MASK)*2);

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
#if FEATURE(BVH_INSTANCING)
							num_hits_in_instance++;
#endif

							isect_array->t = isect_t;
						}

						primAddr++;
					}
				}
#if FEATURE(BVH_INSTANCING)
				else {
					/* instance push */
					object = kernel_tex_fetch(__prim_object, -primAddr-1);

#if FEATURE(BVH_MOTION)
					bvh_instance_motion_push(kg, object, ray, &P, &dir, &idir, &isect_t, &ob_tfm);
#else
					bvh_instance_push(kg, object, ray, &P, &dir, &idir, &isect_t);
#endif

					num_hits_in_instance = 0;

#if defined(__KERNEL_SSE2__)
					Psplat[0] = _mm_set_ps1(P.x);
					Psplat[1] = _mm_set_ps1(P.y);
					Psplat[2] = _mm_set_ps1(P.z);

					isect_array->t = isect_t;
					tsplat = _mm_set_ps(-isect_t, -isect_t, 0.0f, 0.0f);

					gen_idirsplat_swap(pn, shuf_identity, shuf_swap, idir, idirsplat, shufflexyz);
#endif

					++stackPtr;
					traversalStack[stackPtr] = ENTRYPOINT_SENTINEL;

					nodeAddr = kernel_tex_fetch(__object_node, object);
				}
			}
#endif
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

#if FEATURE(BVH_INSTANCING)
		if(stackPtr >= 0) {
			kernel_assert(object != OBJECT_NONE);

			if(num_hits_in_instance) {
				float t_fac;

#if FEATURE(BVH_MOTION)
				bvh_instance_motion_pop_factor(kg, object, ray, &P, &dir, &idir, &t_fac, &ob_tfm);
#else
				bvh_instance_pop_factor(kg, object, ray, &P, &dir, &idir, &t_fac);
#endif

				/* scale isect->t to adjust for instancing */
				for(int i = 0; i < num_hits_in_instance; i++)
					(isect_array-i-1)->t *= t_fac;
			}
			else {
				float ignore_t = FLT_MAX;

#if FEATURE(BVH_MOTION)
				bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, &ignore_t, &ob_tfm);
#else
				bvh_instance_pop(kg, object, ray, &P, &dir, &idir, &ignore_t);
#endif
			}

#if defined(__KERNEL_SSE2__)
			Psplat[0] = _mm_set_ps1(P.x);
			Psplat[1] = _mm_set_ps1(P.y);
			Psplat[2] = _mm_set_ps1(P.z);

			isect_t = tmax;
			isect_array->t = isect_t;
			tsplat = _mm_set_ps(-isect_t, -isect_t, 0.0f, 0.0f);

			gen_idirsplat_swap(pn, shuf_identity, shuf_swap, idir, idirsplat, shufflexyz);
#endif

			object = OBJECT_NONE;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
#endif
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return false;
}

#undef FEATURE
#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES


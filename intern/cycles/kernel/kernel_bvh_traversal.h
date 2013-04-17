/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
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
 * BVH_SUBSURFACE: subsurface same object, random triangle intersection
 * BVH_MOTION: motion blur rendering
 *
 */

#define FEATURE(f) (((BVH_FUNCTION_FEATURES) & (f)) != 0)

__device bool BVH_FUNCTION_NAME
(KernelGlobals *kg, const Ray *ray, Intersection *isect
#if FEATURE(BVH_SUBSURFACE)
, int subsurface_object, float subsurface_random
#else
, const uint visibility
#endif
#if FEATURE(BVH_HAIR_MINIMUM_WIDTH) && !FEATURE(BVH_SUBSURFACE)
, uint *lcg_state = NULL, float difl = 0.0f, float extmax = 0.0f
#endif
)
{
	/* traversal stack in CUDA thread-local memory */
	int traversalStack[BVH_STACK_SIZE];
	traversalStack[0] = ENTRYPOINT_SENTINEL;

	/* traversal variables in registers */
	int stackPtr = 0;
	int nodeAddr = kernel_data.bvh.root;

	/* ray parameters in registers */
	const float tmax = ray->t;
	float3 P = ray->P;
	float3 idir = bvh_inverse_direction(ray->D);
	int object = ~0;

#if FEATURE(BVH_SUBSURFACE)
	const uint visibility = ~0;
	int num_hits = 0;
#endif

#if FEATURE(BVH_MOTION)
	Transform ob_tfm;
#endif

	isect->t = tmax;
	isect->object = ~0;
	isect->prim = ~0;
	isect->u = 0.0f;
	isect->v = 0.0f;

	/* traversal loop */
	do {
		do
		{
			/* traverse internal nodes */
			while(nodeAddr >= 0 && nodeAddr != ENTRYPOINT_SENTINEL)
			{
				bool traverseChild0, traverseChild1, closestChild1;
				int nodeAddrChild1;

#if FEATURE(BVH_HAIR_MINIMUM_WIDTH) && !FEATURE(BVH_SUBSURFACE)
				bvh_node_intersect(kg, &traverseChild0, &traverseChild1,
					&closestChild1, &nodeAddr, &nodeAddrChild1,
					P, idir, isect->t, visibility, nodeAddr, difl, extmax);
#else
				bvh_node_intersect(kg, &traverseChild0, &traverseChild1,
					&closestChild1, &nodeAddr, &nodeAddrChild1,
					P, idir, isect->t, visibility, nodeAddr);
#endif

				if(traverseChild0 != traverseChild1) {
					/* one child was intersected */
					if(traverseChild1) {
						nodeAddr = nodeAddrChild1;
					}
				}
				else {
					if(!traverseChild0) {
						/* neither child was intersected */
						nodeAddr = traversalStack[stackPtr];
						--stackPtr;
					}
					else {
						/* both children were intersected, push the farther one */
						if(closestChild1) {
							int tmp = nodeAddr;
							nodeAddr = nodeAddrChild1;
							nodeAddrChild1 = tmp;
						}

						++stackPtr;
						traversalStack[stackPtr] = nodeAddrChild1;
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
#if FEATURE(BVH_SUBSURFACE)
						/* only primitives from the same object */
						uint tri_object = (object == ~0)? kernel_tex_fetch(__prim_object, primAddr): object;

						if(tri_object == subsurface_object) {
#endif

							/* intersect ray against primitive */
#if FEATURE(BVH_HAIR)
							uint segment = kernel_tex_fetch(__prim_segment, primAddr);
#if !FEATURE(BVH_SUBSURFACE)
							if(segment != ~0) {
								if(kernel_data.curve_kernel_data.curveflags & CURVE_KN_INTERPOLATE) 
#if FEATURE(BVH_HAIR_MINIMUM_WIDTH)
									bvh_cardinal_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment, lcg_state, difl, extmax);
								else
									bvh_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment, lcg_state, difl, extmax);
#else
									bvh_cardinal_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment);
								else
									bvh_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment);
#endif
							}
							else
#endif
#endif
#if FEATURE(BVH_SUBSURFACE)
#if FEATURE(BVH_HAIR)
							if(segment == ~0)
#endif
								bvh_triangle_intersect_subsurface(kg, isect, P, idir, object, primAddr, tmax, &num_hits, subsurface_random);

						}
#else
								bvh_triangle_intersect(kg, isect, P, idir, visibility, object, primAddr);

							/* shadow ray early termination */
							if(visibility == PATH_RAY_SHADOW_OPAQUE && isect->prim != ~0)
								return true;
#endif

						primAddr++;
					}
				}
#if FEATURE(BVH_INSTANCING)
				else {
					/* instance push */
#if FEATURE(BVH_SUBSURFACE)
					if(subsurface_object == kernel_tex_fetch(__prim_object, -primAddr-1)) {
						object = subsurface_object;
#else
						object = kernel_tex_fetch(__prim_object, -primAddr-1);
#endif

#if FEATURE(BVH_MOTION)
						bvh_instance_motion_push(kg, object, ray, &P, &idir, &isect->t, &ob_tfm, tmax);
#else
						bvh_instance_push(kg, object, ray, &P, &idir, &isect->t, tmax);
#endif

						++stackPtr;
						traversalStack[stackPtr] = ENTRYPOINT_SENTINEL;

						nodeAddr = kernel_tex_fetch(__object_node, object);
#if FEATURE(BVH_SUBSURFACE)
					}
					else {
						/* pop */
						nodeAddr = traversalStack[stackPtr];
						--stackPtr;
					}
#endif
				}
			}
#endif
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

#if FEATURE(BVH_INSTANCING)
		if(stackPtr >= 0) {
			kernel_assert(object != ~0);

			/* instance pop */
#if FEATURE(BVH_MOTION)
			bvh_instance_motion_pop(kg, object, ray, &P, &idir, &isect->t, &ob_tfm, tmax);
#else
			bvh_instance_pop(kg, object, ray, &P, &idir, &isect->t, tmax);
#endif
			object = ~0;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
#endif
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

#if FEATURE(BVH_SUBSURAFACE)
	return (num_hits != 0);
#else
	return (isect->prim != ~0);
#endif
}

#undef FEATURE
#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES


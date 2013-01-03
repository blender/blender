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

CCL_NAMESPACE_BEGIN

/*
 * "Persistent while-while kernel" used in:
 *
 * "Understanding the Efficiency of Ray Traversal on GPUs",
 * Timo Aila and Samuli Laine,
 * Proc. High-Performance Graphics 2009
 */

/* bottom-most stack entry, indicating the end of traversal */

#define ENTRYPOINT_SENTINEL 0x76543210
/* 64 object BVH + 64 mesh BVH + 64 object node splitting */
#define BVH_STACK_SIZE 192
#define BVH_NODE_SIZE 4
#define TRI_NODE_SIZE 3

/* silly workaround for float extended precision that happens when compiling
 * without sse support on x86, it results in different results for float ops
 * that you would otherwise expect to compare correctly */
#if !defined(__i386__) || defined(__SSE__)
#define NO_EXTENDED_PRECISION
#else
#define NO_EXTENDED_PRECISION volatile
#endif

__device_inline float3 bvh_inverse_direction(float3 dir)
{
	/* avoid divide by zero (ooeps = exp2f(-80.0f)) */
	float ooeps = 0.00000000000000000000000082718061255302767487140869206996285356581211090087890625f;
	float3 idir;

	idir.x = 1.0f/((fabsf(dir.x) > ooeps)? dir.x: copysignf(ooeps, dir.x));
	idir.y = 1.0f/((fabsf(dir.y) > ooeps)? dir.y: copysignf(ooeps, dir.y));
	idir.z = 1.0f/((fabsf(dir.z) > ooeps)? dir.z: copysignf(ooeps, dir.z));

	return idir;
}

__device_inline void bvh_instance_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, const float tmax)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

	*P = transform_point(&tfm, ray->P);

	float3 dir = transform_direction(&tfm, ray->D);

	float len;
	dir = normalize_len(dir, &len);

	*idir = bvh_inverse_direction(dir);

	if(*t != FLT_MAX)
		*t *= len;
}

__device_inline void bvh_instance_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, const float tmax)
{
	if(*t != FLT_MAX) {
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
		*t *= len(transform_direction(&tfm, 1.0f/(*idir)));
	}

	*P = ray->P;
	*idir = bvh_inverse_direction(ray->D);
}

#ifdef __OBJECT_MOTION__
__device_inline void bvh_instance_motion_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, Transform *tfm, const float tmax)
{
	Transform itfm;
	*tfm = object_fetch_transform_motion_test(kg, object, ray->time, &itfm);

	*P = transform_point(&itfm, ray->P);

	float3 dir = transform_direction(&itfm, ray->D);

	float len;
	dir = normalize_len(dir, &len);

	*idir = bvh_inverse_direction(dir);

	if(*t != FLT_MAX)
		*t *= len;
}

__device_inline void bvh_instance_motion_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, Transform *tfm, const float tmax)
{
	if(*t != FLT_MAX)
		*t *= len(transform_direction(tfm, 1.0f/(*idir)));

	*P = ray->P;
	*idir = bvh_inverse_direction(ray->D);
}
#endif

/* intersect two bounding boxes */
__device_inline void bvh_node_intersect(KernelGlobals *kg,
	bool *traverseChild0, bool *traverseChild1,
	bool *closestChild1, int *nodeAddr0, int *nodeAddr1,
	float3 P, float3 idir, float t, uint visibility, int nodeAddr)
{
	/* fetch node data */
	float4 n0xy = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+0);
	float4 n1xy = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+1);
	float4 nz = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+2);
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+3);

	/* intersect ray against child nodes */
	float3 ood = P * idir;
	float c0lox = n0xy.x * idir.x - ood.x;
	float c0hix = n0xy.y * idir.x - ood.x;
	float c0loy = n0xy.z * idir.y - ood.y;
	float c0hiy = n0xy.w * idir.y - ood.y;
	float c0loz = nz.x * idir.z - ood.z;
	float c0hiz = nz.y * idir.z - ood.z;
	NO_EXTENDED_PRECISION float c0min = max4(min(c0lox, c0hix), min(c0loy, c0hiy), min(c0loz, c0hiz), 0.0f);
	NO_EXTENDED_PRECISION float c0max = min4(max(c0lox, c0hix), max(c0loy, c0hiy), max(c0loz, c0hiz), t);

	float c1loz = nz.z * idir.z - ood.z;
	float c1hiz = nz.w * idir.z - ood.z;
	float c1lox = n1xy.x * idir.x - ood.x;
	float c1hix = n1xy.y * idir.x - ood.x;
	float c1loy = n1xy.z * idir.y - ood.y;
	float c1hiy = n1xy.w * idir.y - ood.y;
	NO_EXTENDED_PRECISION float c1min = max4(min(c1lox, c1hix), min(c1loy, c1hiy), min(c1loz, c1hiz), 0.0f);
	NO_EXTENDED_PRECISION float c1max = min4(max(c1lox, c1hix), max(c1loy, c1hiy), max(c1loz, c1hiz), t);

	/* decide which nodes to traverse next */
#ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	*traverseChild0 = (c0max >= c0min) && (__float_as_int(cnodes.z) & visibility);
	*traverseChild1 = (c1max >= c1min) && (__float_as_int(cnodes.w) & visibility);
#else
	*traverseChild0 = (c0max >= c0min);
	*traverseChild1 = (c1max >= c1min);
#endif

	*nodeAddr0 = __float_as_int(cnodes.x);
	*nodeAddr1 = __float_as_int(cnodes.y);

	*closestChild1 = (c1min < c0min);
}

/* Sven Woop's algorithm */
__device_inline void bvh_triangle_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 idir, uint visibility, int object, int triAddr)
{
	/* compute and check intersection t-value */
	float4 v00 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+0);
	float4 v11 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+1);
	float3 dir = 1.0f/idir;

	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(dir.x*v00.x + dir.y*v00.y + dir.z*v00.z);
	float t = Oz * invDz;

	if(t > 0.0f && t < isect->t) {
		/* compute and check barycentric u */
		float Ox = v11.w + P.x*v11.x + P.y*v11.y + P.z*v11.z;
		float Dx = dir.x*v11.x + dir.y*v11.y + dir.z*v11.z;
		float u = Ox + t*Dx;

		if(u >= 0.0f) {
			/* compute and check barycentric v */
			float4 v22 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+2);
			float Oy = v22.w + P.x*v22.x + P.y*v22.y + P.z*v22.z;
			float Dy = dir.x*v22.x + dir.y*v22.y + dir.z*v22.z;
			float v = Oy + t*Dy;

			if(v >= 0.0f && u + v <= 1.0f) {
#ifdef __VISIBILITY_FLAG__
				/* visibility flag test. we do it here under the assumption
				 * that most triangles are culled by node flags */
				if(kernel_tex_fetch(__prim_visibility, triAddr) & visibility)
#endif
				{
					/* record intersection */
					isect->prim = triAddr;
					isect->object = object;
					isect->u = u;
					isect->v = v;
					isect->t = t;
				}
			}
		}
	}
}

#ifdef __HAIR__
__device_inline void bvh_curve_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 idir, uint visibility, int object, int curveAddr, int segment)
{
	/* curve Intersection check */
	
	int flags = kernel_data.curve_kernel_data.curveflags;

	int prim = kernel_tex_fetch(__prim_index, curveAddr);
	float4 v00 = kernel_tex_fetch(__curves, prim);

	int k0 = __float_as_int(v00.x) + segment;
	int k1 = k0 + 1;

	float4 P1 = kernel_tex_fetch(__curve_keys, k0);
	float4 P2 = kernel_tex_fetch(__curve_keys, k1);

	float l = len(P2 - P1);
	float r1 = P1.w;
	float r2 = P2.w;
	float mr = max(r1,r2);
	float3 p1 = float4_to_float3(P1);
	float3 p2 = float4_to_float3(P2);
	float3 dif = P - p1;
	float3 dir = 1.0f/idir;

	/* test bounding sphere intersection (introduce circular artifacts)*/
	/*float3 bvector = 0.5f * (p1 + p2) - P;
	float bvectorl_sq = len_squared(bvector);
	float dot_bv_dir = dot(bvector,dir);
	float maxdist = l * 0.5f + mr;
	if(bvectorl_sq - dot_bv_dir * dot_bv_dir > maxdist * maxdist)
		return;*/

	/* obtain parameters and test midpoint distance for suitable modes*/
	float3 tg = (p2 - p1) / l;
	float gd = (r2 - r1) / l;
	float dirz = dot(dir,tg);
	float difz = dot(dif,tg);

	float a = 1.0f - (dirz*dirz*(1 + gd*gd));
	float halfb = (dot(dir,dif) - dirz*(difz + gd*(difz*gd + r1)));

	float tcentre = -halfb/a;
	float zcentre = difz + (dirz * tcentre);

	if((tcentre > isect->t) && !(flags & CURVE_KN_ACCURATE))
		return;
	if((zcentre < 0 || zcentre > l) && !(flags & CURVE_KN_ACCURATE) && !(flags & CURVE_KN_INTERSECTCORRECTION))
		return;

	/* test minimum separation*/
	float3 cprod = cross(tg, dir);
	float3 cprod2 = cross(tg, dif);
	float cprodsq = len_squared(cprod);
	float cprod2sq = len_squared(cprod2);
	float distscaled = dot(cprod,dif);

	if(cprodsq == 0)
		distscaled = cprod2sq;
	else
		distscaled = (distscaled*distscaled)/cprodsq;

	if(distscaled > mr*mr)
		return;

	/* calculate true intersection*/
	float3 tdif = P - p1 + tcentre * dir;
	float tdifz = dot(tdif,tg);
	float tb = 2*(dot(dir,tdif) - dirz*(tdifz + gd*(tdifz*gd + r1)));
	float tc = dot(tdif,tdif) - tdifz * tdifz * (1 + gd*gd) - r1*r1 - 2*r1*tdifz*gd;
	float td = tb*tb - 4*a*tc;

	if (td<0)
		return;

	float rootd = 0.0f;
	float correction = 0.0f;
	if(flags & CURVE_KN_ACCURATE) {
		rootd = sqrtf(td);
		correction = ((-tb - rootd)/(2*a));
	}

	float t = tcentre + correction;

	if(t < isect->t) {

		if(flags & CURVE_KN_INTERSECTCORRECTION) {
			rootd = sqrtf(td);
			correction = ((-tb - rootd)/(2*a));
			t = tcentre + correction;
		}

		float z = zcentre + (dirz * correction);
		bool backface = false;

		if(flags & CURVE_KN_BACKFACING && (t < 0.0f || z < 0 || z > l)) {
			backface = true;
			correction = ((-tb + rootd)/(2*a));
			t = tcentre + correction;
			z = zcentre + (dirz * correction);
		}

		if(t > 0.0f && t < isect->t && z >= 0 && z <= l) {

			if (flags & CURVE_KN_ENCLOSEFILTER) {

				float enc_ratio = kernel_data.curve_kernel_data.encasing_ratio;
				if((dot(P - p1, tg) > -r1 * enc_ratio) && (dot(P - p2, tg) < r2 * enc_ratio)) {
					float a2 = 1.0f - (dirz*dirz*(1 + gd*gd*enc_ratio*enc_ratio));
					float c2 = dot(dif,dif) - difz * difz * (1 + gd*gd*enc_ratio*enc_ratio) - r1*r1*enc_ratio*enc_ratio - 2*r1*difz*gd*enc_ratio;
					if(a2*c2 < 0.0f)
						return;
				}
			}

			/*remove overlap - not functional yet*/
			/*if (flags & CURVE_KN_CURVEDATA) {
				float3 tg1 = float4_to_float3(kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+0));
				float3 tg2 = float4_to_float3(kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+1));
				if((dot(P + t * dir - p1, tg1) < 0.0f) || (dot(P + t * dir - p2, tg2) > 0.0f))
					return;
			}*/

#ifdef __VISIBILITY_FLAG__
			/* visibility flag test. we do it here under the assumption
			 * that most triangles are culled by node flags */
			if(kernel_tex_fetch(__prim_visibility, curveAddr) & visibility)
#endif
			{
				/* record intersection */
				isect->prim = curveAddr;
				isect->segment = segment;
				isect->object = object;
				isect->u = z/l;
				isect->v = td/(4*a*a);
				isect->t = t;

				if(backface) 
					isect->u = -isect->u;
			}
		}
	}
}
#endif

__device_inline bool bvh_intersect(KernelGlobals *kg, const Ray *ray, const uint visibility, Intersection *isect)
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

				bvh_node_intersect(kg, &traverseChild0, &traverseChild1,
					&closestChild1, &nodeAddr, &nodeAddrChild1,
					P, idir, isect->t, visibility, nodeAddr);

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

#ifdef __INSTANCING__
				if(primAddr >= 0) {
#endif
					int primAddr2 = __float_as_int(leaf.y);

					/* pop */
					nodeAddr = traversalStack[stackPtr];
					--stackPtr;

					/* primitive intersection */
					while(primAddr < primAddr2) {
						/* intersect ray against primitive */
#ifdef __HAIR__
						uint segment = kernel_tex_fetch(__prim_segment, primAddr);
						if(segment != ~0)
							bvh_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment);
						else
#endif
							bvh_triangle_intersect(kg, isect, P, idir, visibility, object, primAddr);

						/* shadow ray early termination */
						if(visibility == PATH_RAY_SHADOW_OPAQUE && isect->prim != ~0)
							return true;

						primAddr++;
					}
#ifdef __INSTANCING__
				}
				else {
					/* instance push */
					object = kernel_tex_fetch(__prim_object, -primAddr-1);
					bvh_instance_push(kg, object, ray, &P, &idir, &isect->t, tmax);

					++stackPtr;
					traversalStack[stackPtr] = ENTRYPOINT_SENTINEL;

					nodeAddr = kernel_tex_fetch(__object_node, object);
				}
#endif
			}
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

#ifdef __INSTANCING__
		if(stackPtr >= 0) {
			kernel_assert(object != ~0);

			/* instance pop */
			bvh_instance_pop(kg, object, ray, &P, &idir, &isect->t, tmax);
			object = ~0;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
#endif
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return (isect->prim != ~0);
}

#ifdef __OBJECT_MOTION__
__device_inline bool bvh_intersect_motion(KernelGlobals *kg, const Ray *ray, const uint visibility, Intersection *isect)
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

	Transform ob_tfm;

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

				bvh_node_intersect(kg, &traverseChild0, &traverseChild1,
					&closestChild1, &nodeAddr, &nodeAddrChild1,
					P, idir, isect->t, visibility, nodeAddr);

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

				if(primAddr >= 0) {
					int primAddr2 = __float_as_int(leaf.y);

					/* pop */
					nodeAddr = traversalStack[stackPtr];
					--stackPtr;

					/* primitive intersection */
					while(primAddr < primAddr2) {
						/* intersect ray against primitive */
#ifdef __HAIR__
						uint segment = kernel_tex_fetch(__prim_segment, primAddr);
						if(segment != ~0)
							bvh_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment);
						else
#endif
							bvh_triangle_intersect(kg, isect, P, idir, visibility, object, primAddr);

						/* shadow ray early termination */
						if(visibility == PATH_RAY_SHADOW_OPAQUE && isect->prim != ~0)
							return true;

						primAddr++;
					}
				}
				else {
					/* instance push */
					object = kernel_tex_fetch(__prim_object, -primAddr-1);
					bvh_instance_motion_push(kg, object, ray, &P, &idir, &isect->t, &ob_tfm, tmax);

					++stackPtr;
					traversalStack[stackPtr] = ENTRYPOINT_SENTINEL;

					nodeAddr = kernel_tex_fetch(__object_node, object);
				}
			}
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

		if(stackPtr >= 0) {
			kernel_assert(object != ~0);

			/* instance pop */
			bvh_instance_motion_pop(kg, object, ray, &P, &idir, &isect->t, &ob_tfm, tmax);
			object = ~0;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return (isect->prim != ~0);
}
#endif

__device_inline bool scene_intersect(KernelGlobals *kg, const Ray *ray, const uint visibility, Intersection *isect)
{
#ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion)
		return bvh_intersect_motion(kg, ray, visibility, isect);
	else
		return bvh_intersect(kg, ray, visibility, isect);
#else
	return bvh_intersect(kg, ray, visibility, isect);
#endif
}

__device_inline float3 ray_offset(float3 P, float3 Ng)
{
#ifdef __INTERSECTION_REFINE__
	const float epsilon_f = 1e-5f;
	/* ideally this should match epsilon_f, but instancing/mblur
	 * precision makes it problematic */
	const float epsilon_test = 1e-1f;
	const int epsilon_i = 32;

	float3 res;

	/* x component */
	if(fabsf(P.x) < epsilon_test) {
		res.x = P.x + Ng.x*epsilon_f;
	}
	else {
		uint ix = __float_as_uint(P.x);
		ix += ((ix ^ __float_as_uint(Ng.x)) >> 31)? -epsilon_i: epsilon_i;
		res.x = __uint_as_float(ix);
	}

	/* y component */
	if(fabsf(P.y) < epsilon_test) {
		res.y = P.y + Ng.y*epsilon_f;
	}
	else {
		uint iy = __float_as_uint(P.y);
		iy += ((iy ^ __float_as_uint(Ng.y)) >> 31)? -epsilon_i: epsilon_i;
		res.y = __uint_as_float(iy);
	}

	/* z component */
	if(fabsf(P.z) < epsilon_test) {
		res.z = P.z + Ng.z*epsilon_f;
	}
	else {
		uint iz = __float_as_uint(P.z);
		iz += ((iz ^ __float_as_uint(Ng.z)) >> 31)? -epsilon_i: epsilon_i;
		res.z = __uint_as_float(iz);
	}

	return res;
#else
	const float epsilon_f = 1e-4f;
	return P + epsilon_f*Ng;
#endif
}

__device_inline float3 bvh_triangle_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != ~0) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D*t);
		D = normalize_len(D, &t);
	}

	P = P + D*t;

	float4 v00 = kernel_tex_fetch(__tri_woop, isect->prim*TRI_NODE_SIZE+0);
	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(D.x*v00.x + D.y*v00.y + D.z*v00.z);
	float rt = Oz * invDz;

	P = P + D*rt;

	if(isect->object != ~0) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

#ifdef __HAIR__
__device_inline float3 bvh_curve_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray, float t)
{
	int flag = kernel_data.curve_kernel_data.curveflags;
	float3 P = ray->P;
	float3 D = ray->D;

	if(isect->object != ~0) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D*t);
		D = normalize_len(D, &t);
	}

	int prim = kernel_tex_fetch(__prim_index, isect->prim);
	float4 v00 = kernel_tex_fetch(__curves, prim);

	int k0 = __float_as_int(v00.x) + isect->segment;
	int k1 = k0 + 1;

	float4 P1 = kernel_tex_fetch(__curve_keys, k0);
	float4 P2 = kernel_tex_fetch(__curve_keys, k1);
	float l = len(P2 - P1);
	float r1 = P1.w;
	float r2 = P2.w;
	float3 tg = float4_to_float3(P2 - P1) / l;
	float3 dif = P - float4_to_float3(P1) + t * D;
	float gd = ((r2 - r1)/l);

	P = P + D*t;

	dif = P - float4_to_float3(P1);

	#ifdef __UV__
	sd->u = dot(dif,tg)/l;
	sd->v = 0.0f;
	#endif

	if (flag & CURVE_KN_TRUETANGENTGNORMAL) {
		sd->Ng = -(D - tg * (dot(tg,D) * kernel_data.curve_kernel_data.normalmix));
		sd->Ng = normalize(sd->Ng);
		if (flag & CURVE_KN_NORMALCORRECTION)
		{
			//sd->Ng = normalize(sd->Ng);
			sd->Ng = sd->Ng - gd * tg;
			sd->Ng = normalize(sd->Ng);
		}
	}
	else {
		sd->Ng = (dif - tg * sd->u * l) / (P1.w + sd->u * l * gd);
		if (gd != 0.0f) {
			sd->Ng = sd->Ng - gd * tg ;
			sd->Ng = normalize(sd->Ng);
		}
	}

	sd->N = sd->Ng;

	if (flag & CURVE_KN_TANGENTGNORMAL && !(flag & CURVE_KN_TRUETANGENTGNORMAL)) {
		sd->N = -(D - tg * (dot(tg,D) * kernel_data.curve_kernel_data.normalmix));
		sd->N = normalize(sd->N);
		if (flag & CURVE_KN_NORMALCORRECTION) {
			//sd->N = normalize(sd->N);
			sd->N = sd->N - gd * tg;
			sd->N = normalize(sd->N);
		}
	}
	if (!(flag & CURVE_KN_TANGENTGNORMAL) && flag & CURVE_KN_TRUETANGENTGNORMAL) {
		sd->N = (dif - tg * sd->u * l) / (P1.w + sd->u * l * gd);
		if (gd != 0.0f) {
			sd->N = sd->N - gd * tg ;
			sd->N = normalize(sd->N);
		}
	}

	#ifdef __DPDU__
	/* dPdu/dPdv */
	sd->dPdu = tg;
	sd->dPdv = cross(tg,sd->Ng);
	#endif

	if(isect->object != ~0) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
	}

	return P;
}
#endif

CCL_NAMESPACE_END


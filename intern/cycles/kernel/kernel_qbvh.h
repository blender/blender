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
#define QBVH_STACK_SIZE 192
#define QBVH_NODE_SIZE 8
#define TRI_NODE_SIZE 3

__device_inline float3 qbvh_inverse_direction(float3 dir)
{
	// Avoid divide by zero (ooeps = exp2f(-80.0f))
	float ooeps = 0.00000000000000000000000082718061255302767487140869206996285356581211090087890625f;
	float3 idir;

	idir.x = 1.0f/((fabsf(dir.x) > ooeps)? dir.x: copysignf(ooeps, dir.x));
	idir.y = 1.0f/((fabsf(dir.y) > ooeps)? dir.y: copysignf(ooeps, dir.y));
	idir.z = 1.0f/((fabsf(dir.z) > ooeps)? dir.z: copysignf(ooeps, dir.z));

	return idir;
}

__device_inline void qbvh_instance_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, const float tmax)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

	*P = transform_point(&tfm, ray->P);

	float3 dir = transform_direction(&tfm, ray->D);

	float len;
	dir = normalize_len(dir, &len);

	*idir = qbvh_inverse_direction(dir);

	if(*t != FLT_MAX)
		*t *= len;
}

__device_inline void qbvh_instance_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, const float tmax)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);

	if(*t != FLT_MAX)
		*t *= len(transform_direction(&tfm, 1.0f/(*idir)));

	*P = ray->P;
	*idir = qbvh_inverse_direction(ray->D);
}

#ifdef __KERNEL_CPU__

__device_inline void qbvh_node_intersect(KernelGlobals *kg, int *traverseChild,
	int nodeAddrChild[4], float3 P, float3 idir, float t, int nodeAddr)
{
	/* X axis */
	const __m128 bminx = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+0);
	const __m128 t0x = _mm_mul_ps(_mm_sub_ps(bminx, _mm_set_ps1(P.x)), _mm_set_ps1(idir.x));
	const __m128 bmaxx = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+1);
	const __m128 t1x = _mm_mul_ps(_mm_sub_ps(bmaxx, _mm_set_ps1(P.x)), _mm_set_ps1(idir.x));

	__m128 tmin = _mm_max_ps(_mm_min_ps(t0x, t1x), _mm_setzero_ps());
	__m128 tmax = _mm_min_ps(_mm_max_ps(t0x, t1x), _mm_set_ps1(t));

	/* Y axis */
	const __m128 bminy = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+2);
	const __m128 t0y = _mm_mul_ps(_mm_sub_ps(bminy, _mm_set_ps1(P.y)), _mm_set_ps1(idir.y));
	const __m128 bmaxy = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+3);
	const __m128 t1y = _mm_mul_ps(_mm_sub_ps(bmaxy, _mm_set_ps1(P.y)), _mm_set_ps1(idir.y));

	tmin = _mm_max_ps(_mm_min_ps(t0y, t1y), tmin);
	tmax = _mm_min_ps(_mm_max_ps(t0y, t1y), tmax);

	/* Z axis */
	const __m128 bminz = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+4);
	const __m128 t0z = _mm_mul_ps(_mm_sub_ps(bminz, _mm_set_ps1(P.z)), _mm_set_ps1(idir.z));
	const __m128 bmaxz = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+5);
	const __m128 t1z = _mm_mul_ps(_mm_sub_ps(bmaxz, _mm_set_ps1(P.z)), _mm_set_ps1(idir.z));

	tmin = _mm_max_ps(_mm_min_ps(t0z, t1z), tmin);
	tmax = _mm_min_ps(_mm_max_ps(t0z, t1z), tmax);

	/* compare and get mask */
	*traverseChild = _mm_movemask_ps(_mm_cmple_ps(tmin, tmax));

	/* get node addresses */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+6);

	nodeAddrChild[0] = __float_as_int(cnodes.x);
	nodeAddrChild[1] = __float_as_int(cnodes.y);
	nodeAddrChild[2] = __float_as_int(cnodes.z);
	nodeAddrChild[3] = __float_as_int(cnodes.w);
}

#else

__device_inline bool qbvh_bb_intersect(float3 bmin, float3 bmax, float3 P, float3 idir, float t)
{
	float t0x = (bmin.x - P.x)*idir.x;
	float t1x = (bmax.x - P.x)*idir.x;
	float t0y = (bmin.y - P.y)*idir.y;
	float t1y = (bmax.y - P.y)*idir.y;
	float t0z = (bmin.z - P.z)*idir.z;
	float t1z = (bmax.z - P.z)*idir.z;

	float minx = min(t0x, t1x);
	float maxx = max(t0x, t1x);
	float miny = min(t0y, t1y);
	float maxy = max(t0y, t1y);
	float minz = min(t0z, t1z);
	float maxz = max(t0z, t1z);

	float tmin = max4(0.0f, minx, miny, minz);
	float tmax = min4(t, maxx, maxy, maxz);

	return (tmin <= tmax);
}

/* intersect four bounding boxes */
__device_inline void qbvh_node_intersect(KernelGlobals *kg, int *traverseChild,
	int nodeAddrChild[4], float3 P, float3 idir, float t, int nodeAddr)
{
	/* fetch node data */
	float4 minx = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+0);
	float4 miny = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+2);
	float4 minz = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+4);
	float4 maxx = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+1);
	float4 maxy = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+3);
	float4 maxz = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+5);

	/* intersect bounding boxes */
	bool traverseChild0 = qbvh_bb_intersect(make_float3(minx.x, miny.x, minz.x), make_float3(maxx.x, maxy.x, maxz.x), P, idir, t);
	bool traverseChild1 = qbvh_bb_intersect(make_float3(minx.y, miny.y, minz.y), make_float3(maxx.y, maxy.y, maxz.y), P, idir, t);
	bool traverseChild2 = qbvh_bb_intersect(make_float3(minx.z, miny.z, minz.z), make_float3(maxx.z, maxy.z, maxz.z), P, idir, t);
	bool traverseChild3 = qbvh_bb_intersect(make_float3(minx.w, miny.w, minz.w), make_float3(maxx.w, maxy.w, maxz.w), P, idir, t);

	*traverseChild = 0;
	if(traverseChild0) *traverseChild |= 1;
	if(traverseChild1) *traverseChild |= 2;
	if(traverseChild2) *traverseChild |= 4;
	if(traverseChild3) *traverseChild |= 8;

	/* get node addresses */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr*QBVH_NODE_SIZE+6);

	nodeAddrChild[0] = __float_as_int(cnodes.x);
	nodeAddrChild[1] = __float_as_int(cnodes.y);
	nodeAddrChild[2] = __float_as_int(cnodes.z);
	nodeAddrChild[3] = __float_as_int(cnodes.w);
}

#endif

/* Sven Woop's algorithm */
__device_inline void qbvh_triangle_intersect(KernelGlobals *kg, Intersection *isect, float3 P, float3 idir, int object, int triAddr)
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

__device_inline bool scene_intersect(KernelGlobals *kg, const Ray *ray, const bool isshadowray, Intersection *isect)
{
	/* traversal stack in CUDA thread-local memory */
	int traversalStack[QBVH_STACK_SIZE];
	traversalStack[0] = ENTRYPOINT_SENTINEL;

	/* traversal variables in registers */
	int stackPtr = 0;
	int nodeAddr = kernel_data.bvh.root;

	/* ray parameters in registers */
	const float tmax = ray->t;
	float3 P = ray->P;
	float3 idir = qbvh_inverse_direction(ray->D);
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
				int traverseChild, nodeAddrChild[4];

				qbvh_node_intersect(kg, &traverseChild, nodeAddrChild,
					P, idir, isect->t, nodeAddr);

				if(traverseChild & 1) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[0];
				}

				if(traverseChild & 2) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[1];
				}
				if(traverseChild & 4) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[2];
				}

				if(traverseChild & 8) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[3];
				}

				nodeAddr = traversalStack[stackPtr];
				--stackPtr;
			}

			/* if node is leaf, fetch triangle list */
			if(nodeAddr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_nodes, (-nodeAddr-1)*QBVH_NODE_SIZE+(QBVH_NODE_SIZE-2));
				int primAddr = __float_as_int(leaf.x);

#ifdef __INSTANCING__
				if(primAddr >= 0) {
#endif
					int primAddr2 = __float_as_int(leaf.y);

					/* pop */
					nodeAddr = traversalStack[stackPtr];
					--stackPtr;

					/* triangle intersection */
					while(primAddr < primAddr2) {
						/* intersect ray against triangle */
						qbvh_triangle_intersect(kg, isect, P, idir, object, primAddr);

						/* shadow ray early termination */
						if(isshadowray && isect->prim != ~0)
							return true;

						primAddr++;
					}
#ifdef __INSTANCING__
				}
				else {
					/* instance push */
					object = kernel_tex_fetch(__prim_object, -primAddr-1);

					qbvh_instance_push(kg, object, ray, &P, &idir, &isect->t, tmax);

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
			qbvh_instance_pop(kg, object, ray, &P, &idir, &isect->t, tmax);
			object = ~0;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
#endif
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return (isect->prim != ~0);
}

__device_inline float3 ray_offset(float3 P, float3 Ng)
{
#ifdef __INTERSECTION_REFINE__
	const float epsilon_f = 1e-5f;
	const int epsilon_i = 32;

	float3 res;

	/* x component */
	if(fabsf(P.x) < epsilon_f) {
		res.x = P.x + Ng.x*epsilon_f;
	}
	else {
		uint ix = __float_as_uint(P.x);
		ix += ((ix ^ __float_as_uint(Ng.x)) >> 31)? -epsilon_i: epsilon_i;
		res.x = __uint_as_float(ix);
	}

	/* y component */
	if(fabsf(P.y) < epsilon_f) {
		res.y = P.y + Ng.y*epsilon_f;
	}
	else {
		uint iy = __float_as_uint(P.y);
		iy += ((iy ^ __float_as_uint(Ng.y)) >> 31)? -epsilon_i: epsilon_i;
		res.y = __uint_as_float(iy);
	}

	/* z component */
	if(fabsf(P.z) < epsilon_f) {
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

__device_inline float3 bvh_triangle_refine(KernelGlobals *kg, const Intersection *isect, const Ray *ray)
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != ~0) {
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);

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
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

CCL_NAMESPACE_END


/*
 * Copyright 2014, Blender Foundation.
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

/* Triangle/Ray intersections .
 *
 * For BVH ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage.
 */

CCL_NAMESPACE_BEGIN

/* Workaroudn stupidness of CUDA/OpenCL which doesn't allow to access indexed
 * component of float3 value.
 */
#ifndef __KERNEL_CPU__
#  define IDX(vec, idx) \
    ((idx == 0) ? ((vec).x) : ( (idx == 1) ? ((vec).y) : ((vec).z) ))
#else
#  define IDX(vec, idx) ((vec)[idx])
#endif

/* Ray-Triangle intersection for BVH traversal
 *
 * Sven Woop
 * Watertight Ray/Triangle Intersection
 *
 * http://jcgt.org/published/0002/01/05/paper.pdf
 */

/* Precalculated data for the ray->tri intersection. */
typedef struct IsectPrecalc {
	/* Maximal dimension kz, and orthogonal dimensions. */
	int kx, ky, kz;

	/* Shear constants. */
	float Sx, Sy, Sz;
} IsectPrecalc;

ccl_device_inline void triangle_intersect_precalc(float3 dir,
                                                  IsectPrecalc *isect_precalc)
{
	/* Calculate dimesion where the ray direction is maximal. */
	int kz = util_max_axis(make_float3(fabsf(dir.x),
	                                   fabsf(dir.y),
	                                   fabsf(dir.z)));
	int kx = kz + 1; if(kx == 3) kx = 0;
	int ky = kx + 1; if(ky == 3) ky = 0;

	/* Swap kx and ky dimensions to preserve winding direction of triangles. */
	if(IDX(dir, kz) < 0.0f) {
		int tmp = kx;
		kx = ky;
		ky = tmp;
	}

	/* Calculate the shear constants. */
	float inf_dir_z = 1.0f / IDX(dir, kz);
	isect_precalc->Sx = IDX(dir, kx) * inf_dir_z;
	isect_precalc->Sy = IDX(dir, ky) * inf_dir_z;
	isect_precalc->Sz = inf_dir_z;

	/* Store the dimensions. */
	isect_precalc->kx = kx;
	isect_precalc->ky = ky;
	isect_precalc->kz = kz;
}

/* TODO(sergey): Make it general utility function. */
ccl_device_inline float xor_signmast(float x, int y)
{
	return __int_as_float(__float_as_int(x) ^ y);
}

ccl_device_inline bool triangle_intersect(KernelGlobals *kg,
                                          const IsectPrecalc *isect_precalc,
                                          Intersection *isect,
                                          float3 P,
                                          float3 dir,
                                          uint visibility,
                                          int object,
                                          int triAddr)
{
	const int kx = isect_precalc->kx;
	const int ky = isect_precalc->ky;
	const int kz = isect_precalc->kz;
	const float Sx = isect_precalc->Sx;
	const float Sy = isect_precalc->Sy;
	const float Sz = isect_precalc->Sz;

	/* Calculate vertices relative to ray origin. */
	float3 tri[3];
	tri[0] = float4_to_float3(kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+0));
	tri[1] = float4_to_float3(kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+1));
	tri[2] = float4_to_float3(kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+2));

	const float3 A = tri[0] - P;
	const float3 B = tri[1] - P;
	const float3 C = tri[2] - P;

	const float A_kx = IDX(A, kx), A_ky = IDX(A, ky), A_kz = IDX(A, kz);
	const float B_kx = IDX(B, kx), B_ky = IDX(B, ky), B_kz = IDX(B, kz);
	const float C_kx = IDX(C, kx), C_ky = IDX(C, ky), C_kz = IDX(C, kz);

	/* Perform shear and scale of vertices. */
	const float Ax = A_kx - Sx * A_kz;
	const float Ay = A_ky - Sy * A_kz;
	const float Bx = B_kx - Sx * B_kz;
	const float By = B_ky - Sy * B_kz;
	const float Cx = C_kx - Sx * C_kz;
	const float Cy = C_ky - Sy * C_kz;

	/* Calculate scaled barycentric coordinates. */
	float U = Cx * By - Cy * Bx;
	int sign_mask = (__float_as_int(U) & 0x80000000);
	float V = Ax * Cy - Ay * Cx;
	if(sign_mask != (__float_as_int(V) & 0x80000000)) {
		return false;
	}
	float W = Bx * Ay - By * Ax;
	if(sign_mask != (__float_as_int(W) & 0x80000000)) {
		return false;
	}

	/* Calculate determinant. */
	float det = U + V + W;
	if(UNLIKELY(det == 0.0f)) {
		return false;
	}

	/* Calculate scaled z−coordinates of vertices and use them to calculate
	 * the hit distance.
	 */
	const float Az = Sz * A_kz;
	const float Bz = Sz * B_kz;
	const float Cz = Sz * C_kz;
	const float T = U * Az + V * Bz + W * Cz;

	if ((xor_signmast(T, sign_mask) < 0.0f) ||
	    (xor_signmast(T, sign_mask) > isect->t * xor_signmast(det, sign_mask)))
	{
		return false;
	}

#ifdef __VISIBILITY_FLAG__
	/* visibility flag test. we do it here under the assumption
	 * that most triangles are culled by node flags */
	if(kernel_tex_fetch(__prim_visibility, triAddr) & visibility)
#endif
	{
		/* Normalize U, V, W, and T. */
		const float inv_det = 1.0f / det;
		isect->prim = triAddr;
		isect->object = object;
		isect->type = PRIMITIVE_TRIANGLE;
		isect->u = U * inv_det;
		isect->v = V * inv_det;
		isect->t = T * inv_det;
		return true;
	}
	return false;
}

/* Special ray intersection routines for subsurface scattering. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point.
 */

#ifdef __SUBSURFACE__
ccl_device_inline void triangle_intersect_subsurface(
        KernelGlobals *kg,
        const IsectPrecalc *isect_precalc,
        Intersection *isect_array,
        float3 P,
        float3 dir,
        int object,
        int triAddr,
        float tmax,
        uint *num_hits,
        uint *lcg_state,
        int max_hits)
{
	const int kx = isect_precalc->kx;
	const int ky = isect_precalc->ky;
	const int kz = isect_precalc->kz;
	const float Sx = isect_precalc->Sx;
	const float Sy = isect_precalc->Sy;
	const float Sz = isect_precalc->Sz;

	/* Calculate vertices relative to ray origin. */
	float3 tri[3];
	int prim = kernel_tex_fetch(__prim_index, triAddr);
	triangle_vertices(kg, prim, tri);

	const float3 A = tri[0] - P;
	const float3 B = tri[1] - P;
	const float3 C = tri[2] - P;

	const float A_kx = IDX(A, kx), A_ky = IDX(A, ky), A_kz = IDX(A, kz);
	const float B_kx = IDX(B, kx), B_ky = IDX(B, ky), B_kz = IDX(B, kz);
	const float C_kx = IDX(C, kx), C_ky = IDX(C, ky), C_kz = IDX(C, kz);

	/* Perform shear and scale of vertices. */
	const float Ax = A_kx - Sx * A_kz;
	const float Ay = A_ky - Sy * A_kz;
	const float Bx = B_kx - Sx * B_kz;
	const float By = B_ky - Sy * B_kz;
	const float Cx = C_kx - Sx * C_kz;
	const float Cy = C_ky - Sy * C_kz;

	/* Calculate scaled barycentric coordinates. */
	float U = Cx * By - Cy * Bx;
	int sign_mask = (__float_as_int(U) & 0x80000000);
	float V = Ax * Cy - Ay * Cx;
	if(sign_mask != (__float_as_int(V) & 0x80000000)) {
		return;
	}
	float W = Bx * Ay - By * Ax;
	if(sign_mask != (__float_as_int(W) & 0x80000000)) {
		return;
	}

	/* Calculate determinant. */
	float det = U + V + W;
	if(UNLIKELY(det == 0.0f)) {
		return;
	}

	/* Calculate scaled z−coordinates of vertices and use them to calculate
	 * the hit distance.
	 */
	const float Az = Sz * A_kz;
	const float Bz = Sz * B_kz;
	const float Cz = Sz * C_kz;
	const float T = U * Az + V * Bz + W * Cz;

	if ((xor_signmast(T, sign_mask) < 0.0f) ||
	    (xor_signmast(T, sign_mask) > tmax * xor_signmast(det, sign_mask)))
	{
		return;
	}

	/* Normalize U, V, W, and T. */
	const float inv_det = 1.0f / det;

	(*num_hits)++;
	int hit;

	if(*num_hits <= max_hits) {
		hit = *num_hits - 1;
	}
	else {
		/* reservoir sampling: if we are at the maximum number of
		 * hits, randomly replace element or skip it */
		hit = lcg_step_uint(lcg_state) % *num_hits;

		if(hit >= max_hits)
			return;
	}

	/* record intersection */
	Intersection *isect = &isect_array[hit];
	isect->prim = triAddr;
	isect->object = object;
	isect->type = PRIMITIVE_TRIANGLE;
	isect->u = U * inv_det;
	isect->v = V * inv_det;
	isect->t = T * inv_det;
}
#endif

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance. */

/* Reintersections uses the paper:
 *
 * Tomas Moeller
 * Fast, minimum storage ray/triangle intersection
 * http://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
 */

ccl_device_inline float3 triangle_refine(KernelGlobals *kg,
                                         ShaderData *sd,
                                         const Intersection *isect,
                                         const Ray *ray)
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
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

	float3 tri[3];
	tri[0] = float4_to_float3(kernel_tex_fetch(__tri_woop, isect->prim*TRI_NODE_SIZE+0));
	tri[1] = float4_to_float3(kernel_tex_fetch(__tri_woop, isect->prim*TRI_NODE_SIZE+1));
	tri[2] = float4_to_float3(kernel_tex_fetch(__tri_woop, isect->prim*TRI_NODE_SIZE+2));

	float3 edge1 = tri[0] - tri[2];
	float3 edge2 = tri[1] - tri[2];
	float3 tvec = P - tri[2];
	float3 qvec = cross(tvec, edge1);
	float3 pvec = cross(D, edge2);
	float rt = dot(edge2, qvec) / dot(edge1, pvec);

	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
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

/* Same as above, except that isect->t is assumed to be in object space for
 * instancing.
 */
ccl_device_inline float3 triangle_refine_subsurface(KernelGlobals *kg,
                                                    ShaderData *sd,
                                                    const Intersection *isect,
                                                    const Ray *ray)
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#else
		Transform tfm = object_fetch_transform(kg,
		                                       isect->object,
		                                       OBJECT_INVERSE_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D);
		D = normalize(D);
	}

	P = P + D*t;

	float3 tri[3];
	int prim = kernel_tex_fetch(__prim_index, isect->prim);
	triangle_vertices(kg, prim, tri);

	float3 edge1 = tri[0] - tri[2];
	float3 edge2 = tri[1] - tri[2];
	float3 tvec = P - tri[2];
	float3 qvec = cross(tvec, edge1);
	float3 pvec = cross(D, edge2);
	float rt = dot(edge2, qvec) / dot(edge1, pvec);

	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#else
		Transform tfm = object_fetch_transform(kg,
		                                       isect->object,
		                                       OBJECT_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

#undef IDX

CCL_NAMESPACE_END

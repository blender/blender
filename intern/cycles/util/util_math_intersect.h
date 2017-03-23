/*
 * Copyright 2011-2017 Blender Foundation
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

#ifndef __UTIL_MATH_INTERSECT_H__
#define __UTIL_MATH_INTERSECT_H__

CCL_NAMESPACE_BEGIN

/* Ray Intersection */

ccl_device bool ray_sphere_intersect(
        float3 ray_P, float3 ray_D, float ray_t,
        float3 sphere_P, float sphere_radius,
        float3 *isect_P, float *isect_t)
{
	const float3 d = sphere_P - ray_P;
	const float radiussq = sphere_radius*sphere_radius;
	const float tsq = dot(d, d);

	if(tsq > radiussq) {
		/* Ray origin outside sphere. */
		const float tp = dot(d, ray_D);
		if(tp < 0.0f) {
			/* Ray  points away from sphere. */
			return false;
		}
		const float dsq = tsq - tp*tp;  /* pythagoras */
		if(dsq > radiussq)  {
			/* Closest point on ray outside sphere. */
			return false;
		}
		const float t = tp - sqrtf(radiussq - dsq);  /* pythagoras */
		if(t < ray_t) {
			*isect_t = t;
			*isect_P = ray_P + ray_D*t;
			return true;
		}
	}
	return false;
}

ccl_device bool ray_aligned_disk_intersect(
        float3 ray_P, float3 ray_D, float ray_t,
        float3 disk_P, float disk_radius,
        float3 *isect_P, float *isect_t)
{
	/* Aligned disk normal. */
	float disk_t;
	const float3 disk_N = normalize_len(ray_P - disk_P, &disk_t);
	const float div = dot(ray_D, disk_N);
	if(UNLIKELY(div == 0.0f)) {
		return false;
	}
	/* Compute t to intersection point. */
	const float t = -disk_t/div;
	if(t < 0.0f || t > ray_t) {
		return false;
	}
	/* Test if within radius. */
	float3 P = ray_P + ray_D*t;
	if(len_squared(P - disk_P) > disk_radius*disk_radius) {
		return false;
	}
	*isect_P = P;
	*isect_t = t;
	return true;
}

/* Optimized watertight ray-triangle intersection.
 *
 * Sven Woop
 * Watertight Ray/Triangle Intersection
 *
 * http://jcgt.org/published/0002/01/05/paper.pdf
 */

/* Precalculated data for the ray->tri intersection. */
typedef struct TriangleIsectPrecalc {
	/* Maximal dimension kz, and orthogonal dimensions. */
	int kx, ky, kz;

	/* Shear constants. */
	float Sx, Sy, Sz;
} TriangleIsectPrecalc;

/* Workaround stupidness of CUDA/OpenCL which doesn't allow to access indexed
 * component of float3 value.
 */
#ifdef __KERNEL_GPU__
#  define IDX(vec, idx) \
    ((idx == 0) ? ((vec).x) : ( (idx == 1) ? ((vec).y) : ((vec).z) ))
#else
#  define IDX(vec, idx) ((vec)[idx])
#endif

#if (defined(__KERNEL_OPENCL_APPLE__)) || \
    (defined(__KERNEL_CUDA__) && (defined(i386) || defined(_M_IX86)))
ccl_device_noinline
#else
ccl_device_inline
#endif
void ray_triangle_intersect_precalc(float3 dir,
                                    TriangleIsectPrecalc *isect_precalc)
{
	/* Calculate dimension where the ray direction is maximal. */
#ifndef __KERNEL_SSE__
	int kz = util_max_axis(make_float3(fabsf(dir.x),
	                                   fabsf(dir.y),
	                                   fabsf(dir.z)));
	int kx = kz + 1; if(kx == 3) kx = 0;
	int ky = kx + 1; if(ky == 3) ky = 0;
#else
	int kx, ky, kz;
	/* Avoiding mispredicted branch on direction. */
	kz = util_max_axis(fabs(dir));
	static const char inc_xaxis[] = {1, 2, 0, 55};
	static const char inc_yaxis[] = {2, 0, 1, 55};
	kx = inc_xaxis[kz];
	ky = inc_yaxis[kz];
#endif

	float dir_kz = IDX(dir, kz);

	/* Swap kx and ky dimensions to preserve winding direction of triangles. */
	if(dir_kz < 0.0f) {
		int tmp = kx;
		kx = ky;
		ky = tmp;
	}

	/* Calculate the shear constants. */
	float inv_dir_z = 1.0f / dir_kz;
	isect_precalc->Sx = IDX(dir, kx) * inv_dir_z;
	isect_precalc->Sy = IDX(dir, ky) * inv_dir_z;
	isect_precalc->Sz = inv_dir_z;

	/* Store the dimensions. */
	isect_precalc->kx = kx;
	isect_precalc->ky = ky;
	isect_precalc->kz = kz;
}

ccl_device_forceinline bool ray_triangle_intersect(
        const TriangleIsectPrecalc *isect_precalc,
        float3 ray_P, float ray_t,
#if defined(__KERNEL_AVX2__) && defined(__KERNEL_SSE__)
        const ssef *ssef_verts,
#else
        const float3 tri_a, const float3 tri_b, const float3 tri_c,
#endif
        float *isect_u, float *isect_v, float *isect_t)
{
	const int kx = isect_precalc->kx;
	const int ky = isect_precalc->ky;
	const int kz = isect_precalc->kz;
	const float Sx = isect_precalc->Sx;
	const float Sy = isect_precalc->Sy;
	const float Sz = isect_precalc->Sz;

#if defined(__KERNEL_AVX2__) && defined(__KERNEL_SSE__)
	const avxf avxf_P(ray_P.m128, ray_P.m128);
	const avxf tri_ab(_mm256_loadu_ps((float *)(ssef_verts)));
	const avxf tri_bc(_mm256_loadu_ps((float *)(ssef_verts + 1)));

	const avxf AB = tri_ab - avxf_P;
	const avxf BC = tri_bc - avxf_P;

	const __m256i permute_mask = _mm256_set_epi32(0x3, kz, ky, kx, 0x3, kz, ky, kx);

	const avxf AB_k = shuffle(AB, permute_mask);
	const avxf BC_k = shuffle(BC, permute_mask);

	/* Akz, Akz, Bkz, Bkz, Bkz, Bkz, Ckz, Ckz */
	const avxf ABBC_kz = shuffle<2>(AB_k, BC_k);

	/* Akx, Aky, Bkx, Bky, Bkx,Bky, Ckx, Cky */
	const avxf ABBC_kxy = shuffle<0,1,0,1>(AB_k, BC_k);

	const avxf Sxy(Sy, Sx, Sy, Sx);

	/* Ax, Ay, Bx, By, Bx, By, Cx, Cy */
	const avxf ABBC_xy = nmadd(ABBC_kz, Sxy, ABBC_kxy);

	float ABBC_kz_array[8];
	_mm256_storeu_ps((float*)&ABBC_kz_array, ABBC_kz);

	const float A_kz = ABBC_kz_array[0];
	const float B_kz = ABBC_kz_array[2];
	const float C_kz = ABBC_kz_array[6];

	/* By, Bx, Cy, Cx, By, Bx, Ay, Ax */
	const avxf BCBA_yx = permute<3,2,7,6,3,2,1,0>(ABBC_xy);

	const avxf neg_mask(0,0,0,0,0x80000000, 0x80000000, 0x80000000, 0x80000000);

	/* W           U                             V
	 * (AxBy-AyBx) (BxCy-ByCx) XX XX (BxBy-ByBx) (CxAy-CyAx) XX XX
	 */
	const avxf WUxxxxVxx_neg = _mm256_hsub_ps(ABBC_xy * BCBA_yx, neg_mask /* Dont care */);

	const avxf WUVWnegWUVW = permute<0,1,5,0,0,1,5,0>(WUxxxxVxx_neg) ^ neg_mask;

	/* Calculate scaled barycentric coordinates. */
	float WUVW_array[4];
	_mm_storeu_ps((float*)&WUVW_array, _mm256_castps256_ps128 (WUVWnegWUVW));

	const float W = WUVW_array[0];
	const float U = WUVW_array[1];
	const float V = WUVW_array[2];

	const int WUVW_mask = 0x7 & _mm256_movemask_ps(WUVWnegWUVW);
	const int WUVW_zero = 0x7 & _mm256_movemask_ps(_mm256_cmp_ps(WUVWnegWUVW,
	                                               _mm256_setzero_ps(), 0));

	if(!((WUVW_mask == 7) || (WUVW_mask == 0)) && ((WUVW_mask | WUVW_zero) != 7)) {
		return false;
	}
#else
	/* Calculate vertices relative to ray origin. */
	const float3 A = make_float3(tri_a.x - ray_P.x, tri_a.y - ray_P.y, tri_a.z - ray_P.z);
	const float3 B = make_float3(tri_b.x - ray_P.x, tri_b.y - ray_P.y, tri_b.z - ray_P.z);
	const float3 C = make_float3(tri_c.x - ray_P.x, tri_c.y - ray_P.y, tri_c.z - ray_P.z);

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
	float V = Ax * Cy - Ay * Cx;
	float W = Bx * Ay - By * Ax;
	if((U < 0.0f || V < 0.0f || W < 0.0f) &&
	   (U > 0.0f || V > 0.0f || W > 0.0f))
	{
		return false;
	}
#endif

	/* Calculate determinant. */
	float det = U + V + W;
	if(UNLIKELY(det == 0.0f)) {
		return false;
	}

	/* Calculate scaled z-coordinates of vertices and use them to calculate
	 * the hit distance.
	 */
	const float T = (U * A_kz + V * B_kz + W * C_kz) * Sz;
	const int sign_det = (__float_as_int(det) & 0x80000000);
	const float sign_T = xor_signmask(T, sign_det);
	if((sign_T < 0.0f) ||
	   (sign_T > ray_t * xor_signmask(det, sign_det)))
	{
		return false;
	}

	/* Workaround precision error on CUDA. */
#ifdef __KERNEL_CUDA__
	if(A == B && B == C) {
		return false;
	}
#endif
	const float inv_det = 1.0f / det;
	*isect_u = U * inv_det;
	*isect_v = V * inv_det;
	*isect_t = T * inv_det;
	return true;
}

#undef IDX

ccl_device bool ray_quad_intersect(float3 ray_P, float3 ray_D,
                                   float ray_mint, float ray_maxt,
                                   float3 quad_P,
                                   float3 quad_u, float3 quad_v, float3 quad_n,
                                   float3 *isect_P, float *isect_t,
                                   float *isect_u, float *isect_v)
{
	/* Perform intersection test. */
	float t = -(dot(ray_P, quad_n) - dot(quad_P, quad_n)) / dot(ray_D, quad_n);
	if(t < ray_mint || t > ray_maxt) {
		return false;
	}
	const float3 hit = ray_P + t*ray_D;
	const float3 inplane = hit - quad_P;
	const float u = dot(inplane, quad_u) / dot(quad_u, quad_u) + 0.5f;
	if(u < 0.0f || u > 1.0f) {
		return false;
	}
	const float v = dot(inplane, quad_v) / dot(quad_v, quad_v) + 0.5f;
	if(v < 0.0f || v > 1.0f) {
		return false;
	}
	/* Store the result. */
	/* TODO(sergey): Check whether we can avoid some checks here. */
	if(isect_P != NULL) *isect_P = hit;
	if(isect_t != NULL) *isect_t = t;
	if(isect_u != NULL) *isect_u = u;
	if(isect_v != NULL) *isect_v = v;
	return true;
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INTERSECT_H__ */

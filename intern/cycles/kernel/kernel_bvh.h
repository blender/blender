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

ccl_device_inline float3 bvh_inverse_direction(float3 dir)
{
	/* avoid divide by zero (ooeps = exp2f(-80.0f)) */
	float ooeps = 0.00000000000000000000000082718061255302767487140869206996285356581211090087890625f;
	float3 idir;

	idir.x = 1.0f/((fabsf(dir.x) > ooeps)? dir.x: copysignf(ooeps, dir.x));
	idir.y = 1.0f/((fabsf(dir.y) > ooeps)? dir.y: copysignf(ooeps, dir.y));
	idir.z = 1.0f/((fabsf(dir.z) > ooeps)? dir.z: copysignf(ooeps, dir.z));

	return idir;
}

ccl_device_inline void bvh_instance_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, const float tmax)
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

ccl_device_inline void bvh_instance_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, const float tmax)
{
	if(*t != FLT_MAX) {
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
		*t *= len(transform_direction(&tfm, 1.0f/(*idir)));
	}

	*P = ray->P;
	*idir = bvh_inverse_direction(ray->D);
}

#ifdef __OBJECT_MOTION__
ccl_device_inline void bvh_instance_motion_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, Transform *tfm, const float tmax)
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

ccl_device_inline void bvh_instance_motion_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *idir, float *t, Transform *tfm, const float tmax)
{
	if(*t != FLT_MAX)
		*t *= len(transform_direction(tfm, 1.0f/(*idir)));

	*P = ray->P;
	*idir = bvh_inverse_direction(ray->D);
}
#endif

/* Sven Woop's algorithm */
ccl_device_inline bool bvh_triangle_intersect(KernelGlobals *kg, Intersection *isect,
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
					return true;
				}
			}
		}
	}

	return false;
}

#ifdef __HAIR__
ccl_device_inline void curvebounds(float *lower, float *upper, float *extremta, float *extrema, float *extremtb, float *extremb, float p0, float p1, float p2, float p3)
{
	float halfdiscroot = (p2 * p2 - 3 * p3 * p1);
	float ta = -1.0f;
	float tb = -1.0f;
	*extremta = -1.0f;
	*extremtb = -1.0f;
	*upper = p0;
	*lower = p0 + p1 + p2 + p3;
	*extrema = *upper;
	*extremb = *lower;
	if(*lower >= *upper) {
		*upper = *lower;
		*lower = p0;
	}

	if(halfdiscroot >= 0) {
		halfdiscroot = sqrt(halfdiscroot);
		ta = (-p2 - halfdiscroot) / (3 * p3);
		tb = (-p2 + halfdiscroot) / (3 * p3);
	}

	float t2;
	float t3;
	if(ta > 0.0f && ta < 1.0f) {
		t2 = ta * ta;
		t3 = t2 * ta;
		*extremta = ta;
		*extrema = p3 * t3 + p2 * t2 + p1 * ta + p0;
		if(*extrema > *upper) {
			*upper = *extrema;
		}
		if(*extrema < *lower) {
			*lower = *extrema;
		}
	}
	if(tb > 0.0f && tb < 1.0f) {
		t2 = tb * tb;
		t3 = t2 * tb;
		*extremtb = tb;
		*extremb = p3 * t3 + p2 * t2 + p1 * tb + p0;
		if(*extremb >= *upper) {
			*upper = *extremb;
		}
		if(*extremb <= *lower) {
			*lower = *extremb;
		}
	}
}

#ifdef __KERNEL_SSE2__
ccl_device_inline __m128 transform_point_T3(const __m128 t[3], const __m128 &a)
{
	return fma(broadcast<0>(a), t[0], fma(broadcast<1>(a), t[1], _mm_mul_ps(broadcast<2>(a), t[2])));
}
#endif

#ifdef __KERNEL_SSE2__
/* Pass P and idir by reference to aligned vector */
ccl_device_inline bool bvh_cardinal_curve_intersect(KernelGlobals *kg, Intersection *isect,
	const float3 &P, const float3 &idir, uint visibility, int object, int curveAddr, int segment, uint *lcg_state, float difl, float extmax)
#else
ccl_device_inline bool bvh_cardinal_curve_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 idir, uint visibility, int object, int curveAddr, int segment, uint *lcg_state, float difl, float extmax)
#endif
{
	float epsilon = 0.0f;
	float r_st, r_en;

	int depth = kernel_data.curve.subdivisions;
	int flags = kernel_data.curve.curveflags;
	int prim = kernel_tex_fetch(__prim_index, curveAddr);

#ifdef __KERNEL_SSE2__
	__m128 vdir = _mm_div_ps(_mm_set1_ps(1.0f), (__m128 &)idir);
	__m128 vcurve_coef[4];
	const float3 *curve_coef = (float3 *)vcurve_coef;
	
	{
		__m128 dtmp = _mm_mul_ps(vdir, vdir);
		__m128 d_ss = _mm_sqrt_ss(_mm_add_ss(dtmp, broadcast<2>(dtmp)));
		__m128 rd_ss = _mm_div_ss(_mm_set_ss(1.0f), d_ss);

		__m128i v00vec = _mm_load_si128((__m128i *)&kg->__curves.data[prim]);
		int2 &v00 = (int2 &)v00vec;

		int k0 = v00.x + segment;
		int k1 = k0 + 1;
		int ka = max(k0 - 1, v00.x);
		int kb = min(k1 + 1, v00.x + v00.y - 1);

		__m128 P0 = _mm_load_ps(&kg->__curve_keys.data[ka].x);
		__m128 P1 = _mm_load_ps(&kg->__curve_keys.data[k0].x);
		__m128 P2 = _mm_load_ps(&kg->__curve_keys.data[k1].x);
		__m128 P3 = _mm_load_ps(&kg->__curve_keys.data[kb].x);

		__m128 rd_sgn = set_sign_bit<0, 1, 1, 1>(broadcast<0>(rd_ss));
		__m128 mul_zxxy = _mm_mul_ps(shuffle<2, 0, 0, 1>(vdir), rd_sgn);
		__m128 mul_yz = _mm_mul_ps(shuffle<1, 2, 1, 2>(vdir), mul_zxxy);
		__m128 mul_shuf = shuffle<0, 1, 2, 3>(mul_zxxy, mul_yz);
		__m128 vdir0 = _mm_and_ps(vdir, _mm_castsi128_ps(_mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0)));

		__m128 htfm0 = shuffle<0, 2, 0, 3>(mul_shuf, vdir0);
		__m128 htfm1 = shuffle<1, 0, 1, 3>(_mm_set_ss(_mm_cvtss_f32(d_ss)), vdir0);
		__m128 htfm2 = shuffle<1, 3, 2, 3>(mul_shuf, vdir0);

		__m128 htfm[] = { htfm0, htfm1, htfm2 };
		__m128 p0 = transform_point_T3(htfm, _mm_sub_ps(P0, (__m128 &)P));
		__m128 p1 = transform_point_T3(htfm, _mm_sub_ps(P1, (__m128 &)P));
		__m128 p2 = transform_point_T3(htfm, _mm_sub_ps(P2, (__m128 &)P));
		__m128 p3 = transform_point_T3(htfm, _mm_sub_ps(P3, (__m128 &)P));

		float fc = 0.71f;
		__m128 vfc = _mm_set1_ps(fc);
		__m128 vfcxp3 = _mm_mul_ps(vfc, p3);

		vcurve_coef[0] = p1;
		vcurve_coef[1] = _mm_mul_ps(vfc, _mm_sub_ps(p2, p0));
		vcurve_coef[2] = fma(_mm_set1_ps(fc * 2.0f), p0, fma(_mm_set1_ps(fc - 3.0f), p1, fms(_mm_set1_ps(3.0f - 2.0f * fc), p2, vfcxp3)));
		vcurve_coef[3] = fms(_mm_set1_ps(fc - 2.0f), _mm_sub_ps(p2, p1), fms(vfc, p0, vfcxp3));

		r_st = ((float4 &)P1).w;
		r_en = ((float4 &)P2).w;
	}
#else
	float3 curve_coef[4];

	/* curve Intersection check */
	float3 dir = 1.0f/idir;

	/* obtain curve parameters */
	{
		/* ray transform created - this should be created at beginning of intersection loop */
		Transform htfm;
		float d = sqrtf(dir.x * dir.x + dir.z * dir.z);
		htfm = make_transform(
			dir.z / d, 0, -dir.x /d, 0,
			-dir.x * dir.y /d, d, -dir.y * dir.z /d, 0,
			dir.x, dir.y, dir.z, 0,
			0, 0, 0, 1);

		float4 v00 = kernel_tex_fetch(__curves, prim);

		int k0 = __float_as_int(v00.x) + segment;
		int k1 = k0 + 1;

		int ka = max(k0 - 1,__float_as_int(v00.x));
		int kb = min(k1 + 1,__float_as_int(v00.x) + __float_as_int(v00.y) - 1);

		float4 P0 = kernel_tex_fetch(__curve_keys, ka);
		float4 P1 = kernel_tex_fetch(__curve_keys, k0);
		float4 P2 = kernel_tex_fetch(__curve_keys, k1);
		float4 P3 = kernel_tex_fetch(__curve_keys, kb);

		float3 p0 = transform_point(&htfm, float4_to_float3(P0) - P);
		float3 p1 = transform_point(&htfm, float4_to_float3(P1) - P);
		float3 p2 = transform_point(&htfm, float4_to_float3(P2) - P);
		float3 p3 = transform_point(&htfm, float4_to_float3(P3) - P);

		float fc = 0.71f;
		curve_coef[0] = p1;
		curve_coef[1] = -fc*p0 + fc*p2;
		curve_coef[2] = 2.0f * fc * p0 + (fc - 3.0f) * p1 + (3.0f - 2.0f * fc) * p2 - fc * p3;
		curve_coef[3] = -fc * p0 + (2.0f - fc) * p1 + (fc - 2.0f) * p2 + fc * p3;
		r_st = P1.w;
		r_en = P2.w;
	}
#endif

	float r_curr = max(r_st, r_en);

	if((flags & CURVE_KN_RIBBONS) || !(flags & CURVE_KN_BACKFACING))
		epsilon = 2 * r_curr;

	/* find bounds - this is slow for cubic curves */
	float upper, lower;

	float zextrem[4];
	curvebounds(&lower, &upper, &zextrem[0], &zextrem[1], &zextrem[2], &zextrem[3], curve_coef[0].z, curve_coef[1].z, curve_coef[2].z, curve_coef[3].z);
	if(lower - r_curr > isect->t || upper + r_curr < epsilon)
		return false;

	/* minimum width extension */
	float mw_extension = min(difl * fabsf(upper), extmax);
	float r_ext = mw_extension + r_curr;

	float xextrem[4];
	curvebounds(&lower, &upper, &xextrem[0], &xextrem[1], &xextrem[2], &xextrem[3], curve_coef[0].x, curve_coef[1].x, curve_coef[2].x, curve_coef[3].x);
	if(lower > r_ext || upper < -r_ext)
		return false;

	float yextrem[4];
	curvebounds(&lower, &upper, &yextrem[0], &yextrem[1], &yextrem[2], &yextrem[3], curve_coef[0].y, curve_coef[1].y, curve_coef[2].y, curve_coef[3].y);
	if(lower > r_ext || upper < -r_ext)
		return false;

	/* setup recurrent loop */
	int level = 1 << depth;
	int tree = 0;
	float resol = 1.0f / (float)level;
	bool hit = false;

	/* begin loop */
	while(!(tree >> (depth))) {
		float i_st = tree * resol;
		float i_en = i_st + (level * resol);
#ifdef __KERNEL_SSE2__
		__m128 vi_st = _mm_set1_ps(i_st), vi_en = _mm_set1_ps(i_en);
		__m128 vp_st = fma(fma(fma(vcurve_coef[3], vi_st, vcurve_coef[2]), vi_st, vcurve_coef[1]), vi_st, vcurve_coef[0]);
		__m128 vp_en = fma(fma(fma(vcurve_coef[3], vi_en, vcurve_coef[2]), vi_en, vcurve_coef[1]), vi_en, vcurve_coef[0]);

		__m128 vbmin = _mm_min_ps(vp_st, vp_en);
		__m128 vbmax = _mm_max_ps(vp_st, vp_en);

		float3 &bmin = (float3 &)vbmin, &bmax = (float3 &)vbmax;
		float &bminx = bmin.x, &bminy = bmin.y, &bminz = bmin.z;
		float &bmaxx = bmax.x, &bmaxy = bmax.y, &bmaxz = bmax.z;
		float3 &p_st = (float3 &)vp_st, &p_en = (float3 &)vp_en;
#else
		float3 p_st = ((curve_coef[3] * i_st + curve_coef[2]) * i_st + curve_coef[1]) * i_st + curve_coef[0];
		float3 p_en = ((curve_coef[3] * i_en + curve_coef[2]) * i_en + curve_coef[1]) * i_en + curve_coef[0];
		
		float bminx = min(p_st.x, p_en.x);
		float bmaxx = max(p_st.x, p_en.x);
		float bminy = min(p_st.y, p_en.y);
		float bmaxy = max(p_st.y, p_en.y);
		float bminz = min(p_st.z, p_en.z);
		float bmaxz = max(p_st.z, p_en.z);
#endif

		if(xextrem[0] >= i_st && xextrem[0] <= i_en) {
			bminx = min(bminx,xextrem[1]);
			bmaxx = max(bmaxx,xextrem[1]);
		}
		if(xextrem[2] >= i_st && xextrem[2] <= i_en) {
			bminx = min(bminx,xextrem[3]);
			bmaxx = max(bmaxx,xextrem[3]);
		}
		if(yextrem[0] >= i_st && yextrem[0] <= i_en) {
			bminy = min(bminy,yextrem[1]);
			bmaxy = max(bmaxy,yextrem[1]);
		}
		if(yextrem[2] >= i_st && yextrem[2] <= i_en) {
			bminy = min(bminy,yextrem[3]);
			bmaxy = max(bmaxy,yextrem[3]);
		}
		if(zextrem[0] >= i_st && zextrem[0] <= i_en) {
			bminz = min(bminz,zextrem[1]);
			bmaxz = max(bmaxz,zextrem[1]);
		}
		if(zextrem[2] >= i_st && zextrem[2] <= i_en) {
			bminz = min(bminz,zextrem[3]);
			bmaxz = max(bmaxz,zextrem[3]);
		}

		float r1 = r_st + (r_en - r_st) * i_st;
		float r2 = r_st + (r_en - r_st) * i_en;
		r_curr = max(r1, r2);

		mw_extension = min(difl * fabsf(bmaxz), extmax);
		float r_ext = mw_extension + r_curr;
		float coverage = 1.0f;

		if (bminz - r_curr > isect->t || bmaxz + r_curr < epsilon || bminx > r_ext|| bmaxx < -r_ext|| bminy > r_ext|| bmaxy < -r_ext) {
			/* the bounding box does not overlap the square centered at O */
			tree += level;
			level = tree & -tree;
		}
		else if (level == 1) {

			/* the maximum recursion depth is reached.
			* check if dP0.(Q-P0)>=0 and dPn.(Pn-Q)>=0.
			* dP* is reversed if necessary.*/
			float t = isect->t;
			float u = 0.0f;
			if(flags & CURVE_KN_RIBBONS) {
				float3 tg = (p_en - p_st);
				float w = tg.x * tg.x + tg.y * tg.y;
				if (w == 0) {
					tree++;
					level = tree & -tree;
					continue;
				}
				w = -(p_st.x * tg.x + p_st.y * tg.y) / w;
				w = clamp((float)w, 0.0f, 1.0f);

				/* compute u on the curve segment */
				u = i_st * (1 - w) + i_en * w;
				r_curr = r_st + (r_en - r_st) * u;
				/* compare x-y distances */
				float3 p_curr = ((curve_coef[3] * u + curve_coef[2]) * u + curve_coef[1]) * u + curve_coef[0];

				float3 dp_st = (3 * curve_coef[3] * i_st + 2 * curve_coef[2]) * i_st + curve_coef[1];
				if (dot(tg, dp_st)< 0)
					dp_st *= -1;
				if (dot(dp_st, -p_st) + p_curr.z * dp_st.z < 0) {
					tree++;
					level = tree & -tree;
					continue;
				}
				float3 dp_en = (3 * curve_coef[3] * i_en + 2 * curve_coef[2]) * i_en + curve_coef[1];
				if (dot(tg, dp_en) < 0)
					dp_en *= -1;
				if (dot(dp_en, p_en) - p_curr.z * dp_en.z < 0) {
					tree++;
					level = tree & -tree;
					continue;
				}

				/* compute coverage */
				float r_ext = r_curr;
				coverage = 1.0f;
				if(difl != 0.0f) {
					mw_extension = min(difl * fabsf(bmaxz), extmax);
					r_ext = mw_extension + r_curr;
					float d = sqrtf(p_curr.x * p_curr.x + p_curr.y * p_curr.y);
					float d0 = d - r_curr;
					float d1 = d + r_curr;
					if (d0 >= 0)
						coverage = (min(d1 / mw_extension, 1.0f) - min(d0 / mw_extension, 1.0f)) * 0.5f;
					else // inside
						coverage = (min(d1 / mw_extension, 1.0f) + min(-d0 / mw_extension, 1.0f)) * 0.5f;
				}
				
				if (p_curr.x * p_curr.x + p_curr.y * p_curr.y >= r_ext * r_ext || p_curr.z <= epsilon || isect->t < p_curr.z) {
					tree++;
					level = tree & -tree;
					continue;
				}

				t = p_curr.z;
			}
			else {
				float l = len(p_en - p_st);
				/* minimum width extension */
				float or1 = r1;
				float or2 = r2;
				if(difl != 0.0f) {
					mw_extension = min(len(p_st - P) * difl, extmax);
					or1 = r1 < mw_extension ? mw_extension : r1;
					mw_extension = min(len(p_en - P) * difl, extmax);
					or2 = r2 < mw_extension ? mw_extension : r2;
				}
				/* --- */
				float3 tg = (p_en - p_st) / l;
				float gd = (or2 - or1) / l;
				float difz = -dot(p_st,tg);
				float cyla = 1.0f - (tg.z * tg.z * (1 + gd*gd));
				float halfb = (-p_st.z - tg.z*(difz + gd*(difz*gd + or1)));
				float tcentre = -halfb/cyla;
				float zcentre = difz + (tg.z * tcentre);
				float3 tdif = - p_st;
				tdif.z += tcentre;
				float tdifz = dot(tdif,tg);
				float tb = 2*(tdif.z - tg.z*(tdifz + gd*(tdifz*gd + or1)));
				float tc = dot(tdif,tdif) - tdifz * tdifz * (1 + gd*gd) - or1*or1 - 2*or1*tdifz*gd;
				float td = tb*tb - 4*cyla*tc;
				if (td < 0.0f) {
					tree++;
					level = tree & -tree;
					continue;
				}
				
				float rootd = sqrtf(td);
				float correction = ((-tb - rootd)/(2*cyla));
				t = tcentre + correction;

				float3 dp_st = (3 * curve_coef[3] * i_st + 2 * curve_coef[2]) * i_st + curve_coef[1];
				if (dot(tg, dp_st)< 0)
					dp_st *= -1;
				float3 dp_en = (3 * curve_coef[3] * i_en + 2 * curve_coef[2]) * i_en + curve_coef[1];
				if (dot(tg, dp_en) < 0)
					dp_en *= -1;

				if(flags & CURVE_KN_BACKFACING && (dot(dp_st, -p_st) + t * dp_st.z < 0 || dot(dp_en, p_en) - t * dp_en.z < 0 || isect->t < t || t <= 0.0f)) {
					correction = ((-tb + rootd)/(2*cyla));
					t = tcentre + correction;
				}			

				if (dot(dp_st, -p_st) + t * dp_st.z < 0 || dot(dp_en, p_en) - t * dp_en.z < 0 || isect->t < t || t <= 0.0f) {
					tree++;
					level = tree & -tree;
					continue;
				}

				float w = (zcentre + (tg.z * correction))/l;
				w = clamp((float)w, 0.0f, 1.0f);
				/* compute u on the curve segment */
				u = i_st * (1 - w) + i_en * w;
				r_curr = r1 + (r2 - r1) * w;
				r_ext = or1 + (or2 - or1) * w;
				coverage = r_curr/r_ext;

			}
			/* we found a new intersection */

			/* stochastic fade from minimum width */
			if(lcg_state && coverage != 1.0f) {
				if(lcg_step_float(lcg_state) > coverage)
					return hit;
			}

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
				isect->u = u;
				isect->v = 0.0f;
				/*isect->v = 1.0f - coverage; */
				isect->t = t;
				hit = true;
			}
			
			tree++;
			level = tree & -tree;
		}
		else {
			/* split the curve into two curves and process */
			level = level >> 1;
		}
	}

	return hit;
}

ccl_device_inline bool bvh_curve_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 idir, uint visibility, int object, int curveAddr, int segment, uint *lcg_state, float difl, float extmax)
{
	/* curve Intersection check */
	int flags = kernel_data.curve.curveflags;

	int prim = kernel_tex_fetch(__prim_index, curveAddr);
	float4 v00 = kernel_tex_fetch(__curves, prim);

	int cnum = __float_as_int(v00.x);
	int k0 = cnum + segment;
	int k1 = k0 + 1;

	float4 P1 = kernel_tex_fetch(__curve_keys, k0);
	float4 P2 = kernel_tex_fetch(__curve_keys, k1);

	float or1 = P1.w;
	float or2 = P2.w;
	float3 p1 = float4_to_float3(P1);
	float3 p2 = float4_to_float3(P2);

	/* minimum width extension */
	float r1 = or1;
	float r2 = or2;
	if(difl != 0.0f) {
		float pixelsize = min(len(p1 - P) * difl, extmax);
		r1 = or1 < pixelsize ? pixelsize : or1;
		pixelsize = min(len(p2 - P) * difl, extmax);
		r2 = or2 < pixelsize ? pixelsize : or2;
	}
	/* --- */

	float mr = max(r1,r2);
	float3 dif = P - p1;
	float3 dir = 1.0f/idir;
	float l = len(p2 - p1);

	float sp_r = mr + 0.5f * l;
	float3 sphere_dif = P - ((p1 + p2) * 0.5f);
	float sphere_b = dot(dir,sphere_dif);
	sphere_dif = sphere_dif - sphere_b * dir;
	sphere_b = dot(dir,sphere_dif);
	float sdisc = sphere_b * sphere_b - len_squared(sphere_dif) + sp_r * sp_r;
	if(sdisc < 0.0f)
		return false;

	/* obtain parameters and test midpoint distance for suitable modes */
	float3 tg = (p2 - p1) / l;
	float gd = (r2 - r1) / l;
	float dirz = dot(dir,tg);
	float difz = dot(dif,tg);

	float a = 1.0f - (dirz*dirz*(1 + gd*gd));
	float halfb = dot(dir,dif) - dirz*(difz + gd*(difz*gd + r1));

	float tcentre = -halfb/a;
	float zcentre = difz + (dirz * tcentre);

	if((tcentre > isect->t) && !(flags & CURVE_KN_ACCURATE))
		return false;
	if((zcentre < 0 || zcentre > l) && !(flags & CURVE_KN_ACCURATE) && !(flags & CURVE_KN_INTERSECTCORRECTION))
		return false;

	/* test minimum separation */
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
		return false;

	/* calculate true intersection */
	float3 tdif = P - p1 + tcentre * dir;
	float tdifz = dot(tdif,tg);
	float tb = 2*(dot(dir,tdif) - dirz*(tdifz + gd*(tdifz*gd + r1)));
	float tc = dot(tdif,tdif) - tdifz * tdifz * (1 + gd*gd) - r1*r1 - 2*r1*tdifz*gd;
	float td = tb*tb - 4*a*tc;

	if (td < 0.0f)
		return false;

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

		/* stochastic fade from minimum width */
		float adjradius = or1 + z * (or2 - or1) / l;
		adjradius = adjradius / (r1 + z * gd);
		if(lcg_state && adjradius != 1.0f) {
			if(lcg_step_float(lcg_state) > adjradius)
				return false;
		}
		/* --- */

		if(t > 0.0f && t < isect->t && z >= 0 && z <= l) {

			if (flags & CURVE_KN_ENCLOSEFILTER) {
				float enc_ratio = 1.01f;
				if((dot(P - p1, tg) > -r1 * enc_ratio) && (dot(P - p2, tg) < r2 * enc_ratio)) {
					float a2 = 1.0f - (dirz*dirz*(1 + gd*gd*enc_ratio*enc_ratio));
					float c2 = dot(dif,dif) - difz * difz * (1 + gd*gd*enc_ratio*enc_ratio) - r1*r1*enc_ratio*enc_ratio - 2*r1*difz*gd*enc_ratio;
					if(a2*c2 < 0.0f)
						return false;
				}
			}

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
				/*isect->v = 1.0f - adjradius;*/
				isect->t = t;

				if(backface) 
					isect->u = -isect->u;
				
				return true;
			}
		}
	}

	return false;
}
#endif

#ifdef __SUBSURFACE__
/* Special ray intersection routines for subsurface scattering. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point. */

ccl_device_inline void bvh_triangle_intersect_subsurface(KernelGlobals *kg, Intersection *isect_array,
	float3 P, float3 idir, int object, int triAddr, float tmax, uint *num_hits, uint *lcg_state, int max_hits)
{
	/* compute and check intersection t-value */
	float4 v00 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+0);
	float4 v11 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+1);
	float3 dir = 1.0f/idir;

	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(dir.x*v00.x + dir.y*v00.y + dir.z*v00.z);
	float t = Oz * invDz;

	if(t > 0.0f && t < tmax) {
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
				isect->u = u;
				isect->v = v;
				isect->t = t;
			}
		}
	}
}
#endif

/* BVH intersection function variations */

#define BVH_INSTANCING			1
#define BVH_MOTION				2
#define BVH_HAIR				4
#define BVH_HAIR_MINIMUM_WIDTH	8

#define BVH_FUNCTION_NAME bvh_intersect
#define BVH_FUNCTION_FEATURES 0
#include "kernel_bvh_traversal.h"

#if defined(__INSTANCING__)
#define BVH_FUNCTION_NAME bvh_intersect_instancing
#define BVH_FUNCTION_FEATURES BVH_INSTANCING
#include "kernel_bvh_traversal.h"
#endif

#if defined(__HAIR__)
#define BVH_FUNCTION_NAME bvh_intersect_hair
#define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR|BVH_HAIR_MINIMUM_WIDTH
#include "kernel_bvh_traversal.h"
#endif

#if defined(__OBJECT_MOTION__)
#define BVH_FUNCTION_NAME bvh_intersect_motion
#define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_MOTION
#include "kernel_bvh_traversal.h"
#endif

#if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#define BVH_FUNCTION_NAME bvh_intersect_hair_motion
#define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR|BVH_HAIR_MINIMUM_WIDTH|BVH_MOTION
#include "kernel_bvh_traversal.h"
#endif

#if defined(__SUBSURFACE__)
#define BVH_FUNCTION_NAME bvh_intersect_subsurface
#define BVH_FUNCTION_FEATURES 0
#include "kernel_bvh_subsurface.h"
#endif

#if defined(__SUBSURFACE__) && defined(__INSTANCING__)
#define BVH_FUNCTION_NAME bvh_intersect_subsurface_instancing
#define BVH_FUNCTION_FEATURES BVH_INSTANCING
#include "kernel_bvh_subsurface.h"
#endif

#if defined(__SUBSURFACE__) && defined(__HAIR__)
#define BVH_FUNCTION_NAME bvh_intersect_subsurface_hair
#define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR
#include "kernel_bvh_subsurface.h"
#endif

#if defined(__SUBSURFACE__) && defined(__OBJECT_MOTION__)
#define BVH_FUNCTION_NAME bvh_intersect_subsurface_motion
#define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_MOTION
#include "kernel_bvh_subsurface.h"
#endif

#if defined(__SUBSURFACE__) && defined(__HAIR__) && defined(__OBJECT_MOTION__)
#define BVH_FUNCTION_NAME bvh_intersect_subsurface_hair_motion
#define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR|BVH_MOTION
#include "kernel_bvh_subsurface.h"
#endif

/* to work around titan bug when using arrays instead of textures */
#if !defined(__KERNEL_CUDA__) || defined(__KERNEL_CUDA_TEX_STORAGE__)
ccl_device_inline
#else
ccl_device_noinline
#endif
#ifdef __HAIR__ 
bool scene_intersect(KernelGlobals *kg, const Ray *ray, const uint visibility, Intersection *isect, uint *lcg_state, float difl, float extmax)
#else
bool scene_intersect(KernelGlobals *kg, const Ray *ray, const uint visibility, Intersection *isect)
#endif
{
#ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
#ifdef __HAIR__
		if(kernel_data.bvh.have_curves)
			return bvh_intersect_hair_motion(kg, ray, isect, visibility, lcg_state, difl, extmax);
#endif /* __HAIR__ */

		return bvh_intersect_motion(kg, ray, isect, visibility);
	}
#endif /* __OBJECT_MOTION__ */

#ifdef __HAIR__ 
	if(kernel_data.bvh.have_curves)
		return bvh_intersect_hair(kg, ray, isect, visibility, lcg_state, difl, extmax);
#endif /* __HAIR__ */

#ifdef __KERNEL_CPU__

#ifdef __INSTANCING__
	if(kernel_data.bvh.have_instancing)
		return bvh_intersect_instancing(kg, ray, isect, visibility);
#endif /* __INSTANCING__ */

	return bvh_intersect(kg, ray, isect, visibility);
#else /* __KERNEL_CPU__ */

#ifdef __INSTANCING__
	return bvh_intersect_instancing(kg, ray, isect, visibility);
#else
	return bvh_intersect(kg, ray, isect, visibility);
#endif /* __INSTANCING__ */

#endif /* __KERNEL_CPU__ */
}

/* to work around titan bug when using arrays instead of textures */
#ifdef __SUBSURFACE__
#if !defined(__KERNEL_CUDA__) || defined(__KERNEL_CUDA_TEX_STORAGE__)
ccl_device_inline
#else
ccl_device_noinline
#endif
uint scene_intersect_subsurface(KernelGlobals *kg, const Ray *ray, Intersection *isect, int subsurface_object, uint *lcg_state, int max_hits)
{
#ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
#ifdef __HAIR__
		if(kernel_data.bvh.have_curves)
			return bvh_intersect_subsurface_hair_motion(kg, ray, isect, subsurface_object, lcg_state, max_hits);
#endif /* __HAIR__ */

		return bvh_intersect_subsurface_motion(kg, ray, isect, subsurface_object, lcg_state, max_hits);
	}
#endif /* __OBJECT_MOTION__ */

#ifdef __HAIR__ 
	if(kernel_data.bvh.have_curves)
		return bvh_intersect_subsurface_hair(kg, ray, isect, subsurface_object, lcg_state, max_hits);
#endif /* __HAIR__ */

#ifdef __KERNEL_CPU__

#ifdef __INSTANCING__
	if(kernel_data.bvh.have_instancing)
		return bvh_intersect_subsurface_instancing(kg, ray, isect, subsurface_object, lcg_state, max_hits);
#endif /* __INSTANCING__ */

	return bvh_intersect_subsurface(kg, ray, isect, subsurface_object, lcg_state, max_hits);
#else /* __KERNEL_CPU__ */

#ifdef __INSTANCING__
	return bvh_intersect_subsurface_instancing(kg, ray, isect, subsurface_object, lcg_state, max_hits);
#else
	return bvh_intersect_subsurface(kg, ray, isect, subsurface_object, lcg_state, max_hits);
#endif /* __INSTANCING__ */

#endif /* __KERNEL_CPU__ */
}
#endif

/* Ray offset to avoid self intersection */

ccl_device_inline float3 ray_offset(float3 P, float3 Ng)
{
#ifdef __INTERSECTION_REFINE__
	const float epsilon_f = 1e-5f;
	/* ideally this should match epsilon_f, but instancing/mblur
	 * precision makes it problematic */
	const float epsilon_test = 1.0f;
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

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance. */

ccl_device_inline float3 bvh_triangle_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
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

/* same as above, except that isect->t is assumed to be in object space for instancing */
ccl_device_inline float3 bvh_triangle_refine_subsurface(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
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
		D = transform_direction(&tfm, D);
		D = normalize(D);
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

ccl_device_inline float3 curvetangent(float t, float3 p0, float3 p1, float3 p2, float3 p3)
{
	float fc = 0.71f;
	float data[4];
	float t2 = t * t;
	data[0] = -3.0f * fc          * t2  + 4.0f * fc * t                  - fc;
	data[1] =  3.0f * (2.0f - fc) * t2  + 2.0f * (fc - 3.0f) * t;
	data[2] =  3.0f * (fc - 2.0f) * t2  + 2.0f * (3.0f - 2.0f * fc) * t  + fc;
	data[3] =  3.0f * fc          * t2  - 2.0f * fc * t;
	return data[0] * p0 + data[1] * p1 + data[2] * p2 + data[3] * p3;
}

ccl_device_inline float3 curvepoint(float t, float3 p0, float3 p1, float3 p2, float3 p3)
{
	float data[4];
	float fc = 0.71f;
	float t2 = t * t;
	float t3 = t2 * t;
	data[0] = -fc          * t3  + 2.0f * fc          * t2 - fc * t;
	data[1] =  (2.0f - fc) * t3  + (fc - 3.0f)        * t2 + 1.0f;
	data[2] =  (fc - 2.0f) * t3  + (3.0f - 2.0f * fc) * t2 + fc * t;
	data[3] =  fc          * t3  - fc * t2;
	return data[0] * p0 + data[1] * p1 + data[2] * p2 + data[3] * p3;
}

ccl_device_inline float3 bvh_curve_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
{
	int flag = kernel_data.curve.curveflags;
	float t = isect->t;
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
	float l = 1.0f;
	float3 tg = normalize_len(float4_to_float3(P2 - P1), &l);
	float r1 = P1.w;
	float r2 = P2.w;
	float gd = ((r2 - r1)/l);
	
	P = P + D*t;

	if(flag & CURVE_KN_INTERPOLATE) {
		int ka = max(k0 - 1,__float_as_int(v00.x));
		int kb = min(k1 + 1,__float_as_int(v00.x) + __float_as_int(v00.y) - 1);

		float4 P0 = kernel_tex_fetch(__curve_keys, ka);
		float4 P3 = kernel_tex_fetch(__curve_keys, kb);

		float3 p[4];
		p[0] = float4_to_float3(P0);
		p[1] = float4_to_float3(P1);
		p[2] = float4_to_float3(P2);
		p[3] = float4_to_float3(P3);

#ifdef __UV__
		sd->u = isect->u;
		sd->v = 0.0f;
#endif
	
		tg = normalize(curvetangent(isect->u, p[0], p[1], p[2], p[3]));

		if(kernel_data.curve.curveflags & CURVE_KN_RIBBONS)
			sd->Ng = normalize(-(D - tg * (dot(tg, D))));
		else {
			float3 p_curr = curvepoint(isect->u, p[0], p[1], p[2], p[3]);	
			sd->Ng = normalize(P - p_curr);
			sd->Ng = sd->Ng - gd * tg;
			sd->Ng = normalize(sd->Ng);
		}
		sd->N = sd->Ng;
	}
	else {
		float3 dif = P - float4_to_float3(P1);

#ifdef __UV__
		sd->u = dot(dif,tg)/l;
		sd->v = 0.0f;
#endif

		if (flag & CURVE_KN_TRUETANGENTGNORMAL) {
			sd->Ng = -(D - tg * dot(tg, D));
			sd->Ng = normalize(sd->Ng);
		}
		else {
			sd->Ng = (dif - tg * sd->u * l) / (P1.w + sd->u * l * gd);
			if (gd != 0.0f) {
				sd->Ng = sd->Ng - gd * tg ;
				sd->Ng = normalize(sd->Ng);
			}
		}

		sd->N = sd->Ng;
	}

#ifdef __DPDU__
	/* dPdu/dPdv */
	sd->dPdu = tg;
	sd->dPdv = cross(tg, sd->Ng);
#endif

	/*add fading parameter for minimum pixel width with transparency bsdf*/
	/*sd->curve_transparency = isect->v;*/
	/*sd->curve_radius = sd->u * gd * l + r1;*/

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


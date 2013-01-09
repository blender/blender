/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

/* Light Sample result */

typedef struct LightSample {
	float3 P;			/* position on light, or direction for distant light */
	float3 Ng;			/* normal on light */
	float3 D;			/* direction from shading point to light */
	float t;			/* distance to light (FLT_MAX for distant light) */
	float pdf;			/* light sampling probability density function */
	float eval_fac;		/* intensity multiplier */
	int object;			/* object id for triangle/curve lights */
	int prim;			/* primitive id for triangle/curve ligths */
	int shader;			/* shader id */
	int lamp;			/* lamp id */
	int use_mis;		/* for lamps with size zero */
	LightType type;		/* type of light */
} LightSample;

/* Background Light */

#ifdef __BACKGROUND_MIS__

__device float3 background_light_sample(KernelGlobals *kg, float randu, float randv, float *pdf)
{
	/* for the following, the CDF values are actually a pair of floats, with the
	 * function value as X and the actual CDF as Y.  The last entry's function
	 * value is the CDF total. */
	int res = kernel_data.integrator.pdf_background_res;
	int cdf_count = res + 1;

	/* this is basically std::lower_bound as used by pbrt */
	int first = 0;
	int count = res;

	while(count > 0) {
		int step = count >> 1;
		int middle = first + step;

		if(kernel_tex_fetch(__light_background_marginal_cdf, middle).y < randv) {
			first = middle + 1;
			count -= step + 1;
		}
		else
			count = step;
	}

	int index_v = max(0, first - 1);
	kernel_assert(index_v >= 0 && index_v < res);

	float2 cdf_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v);
	float2 cdf_next_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v + 1);
	float2 cdf_last_v = kernel_tex_fetch(__light_background_marginal_cdf, res);

	/* importance-sampled V direction */
	float dv = (randv - cdf_v.y) / (cdf_next_v.y - cdf_v.y);
	float v = (index_v + dv) / res;

	/* this is basically std::lower_bound as used by pbrt */
	first = 0;
	count = res;
	while(count > 0) {
		int step = count >> 1;
		int middle = first + step;

		if(kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_count + middle).y < randu) {
			first = middle + 1;
			count -= step + 1;
		}
		else
			count = step;
	}

	int index_u = max(0, first - 1);
	kernel_assert(index_u >= 0 && index_u < res);

	float2 cdf_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_count + index_u);
	float2 cdf_next_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_count + index_u + 1);
	float2 cdf_last_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_count + res);

	/* importance-sampled U direction */
	float du = (randu - cdf_u.y) / (cdf_next_u.y - cdf_u.y);
	float u = (index_u + du) / res;

	/* compute pdf */
	float denom = cdf_last_u.x * cdf_last_v.x;
	float sin_theta = sinf(M_PI_F * v);

	if(sin_theta == 0.0f || denom == 0.0f)
		*pdf = 0.0f;
	else
		*pdf = (cdf_u.x * cdf_v.x)/(2.0f * M_PI_F * M_PI_F * sin_theta * denom);

	*pdf *= kernel_data.integrator.pdf_lights;

	/* compute direction */
	return -equirectangular_to_direction(u, v);
}

__device float background_light_pdf(KernelGlobals *kg, float3 direction)
{
	float2 uv = direction_to_equirectangular(direction);
	int res = kernel_data.integrator.pdf_background_res;

	float sin_theta = sinf(uv.y * M_PI_F);

	if(sin_theta == 0.0f)
		return 0.0f;

	int index_u = clamp((int)(uv.x * res), 0, res - 1);
	int index_v = clamp((int)(uv.y * res), 0, res - 1);

	/* pdfs in V direction */
	float2 cdf_last_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * (res + 1) + res);
	float2 cdf_last_v = kernel_tex_fetch(__light_background_marginal_cdf, res);

	float denom = cdf_last_u.x * cdf_last_v.x;

	if(denom == 0.0f)
		return 0.0f;

	/* pdfs in U direction */
	float2 cdf_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * (res + 1) + index_u);
	float2 cdf_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v);

	float pdf = (cdf_u.x * cdf_v.x)/(2.0f * M_PI_F * M_PI_F * sin_theta * denom);

	return pdf * kernel_data.integrator.pdf_lights;
}
#endif

/* Regular Light */

__device float3 disk_light_sample(float3 v, float randu, float randv)
{
	float3 ru, rv;

	make_orthonormals(v, &ru, &rv);
	to_unit_disk(&randu, &randv);

	return ru*randu + rv*randv;
}

__device float3 distant_light_sample(float3 D, float radius, float randu, float randv)
{
	return normalize(D + disk_light_sample(D, randu, randv)*radius);
}

__device float3 sphere_light_sample(float3 P, float3 center, float radius, float randu, float randv)
{
	return disk_light_sample(normalize(P - center), randu, randv)*radius;
}

__device float3 area_light_sample(float3 axisu, float3 axisv, float randu, float randv)
{
	randu = randu - 0.5f;
	randv = randv - 0.5f;

	return axisu*randu + axisv*randv;
}

__device float spot_light_attenuation(float4 data1, float4 data2, LightSample *ls)
{
	float3 dir = make_float3(data2.y, data2.z, data2.w);
	float3 I = ls->Ng;

	float spot_angle = data1.w;
	float spot_smooth = data2.x;

	float attenuation = dot(dir, I);

	if(attenuation <= spot_angle) {
		attenuation = 0.0f;
	}
	else {
		float t = attenuation - spot_angle;

		if(t < spot_smooth && spot_smooth != 0.0f)
			attenuation *= smoothstepf(t/spot_smooth);
	}

	return attenuation;
}

__device float lamp_light_pdf(KernelGlobals *kg, const float3 Ng, const float3 I, float t)
{
	float cos_pi = dot(Ng, I);

	if(cos_pi <= 0.0f)
		return 0.0f;
	
	return t*t/cos_pi;
}

__device void lamp_light_sample(KernelGlobals *kg, int lamp,
	float randu, float randv, float3 P, LightSample *ls)
{
	float4 data0 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 0);
	float4 data1 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 1);

	LightType type = (LightType)__float_as_int(data0.x);
	ls->type = type;
#ifdef __LAMP_MIS__
	ls->use_mis = true;
#else
	ls->use_mis = false;
#endif

	if(type == LIGHT_DISTANT) {
		/* distant light */
		float3 lightD = make_float3(data0.y, data0.z, data0.w);
		float3 D = lightD;
		float radius = data1.y;
		float invarea = data1.w;

		if(radius > 0.0f)
			D = distant_light_sample(D, radius, randu, randv);
		else
			ls->use_mis = false;

		ls->P = D;
		ls->Ng = D;
		ls->D = -D;
		ls->t = FLT_MAX;

		float costheta = dot(lightD, D);
		ls->pdf = invarea/(costheta*costheta*costheta);
		ls->eval_fac = ls->pdf;
	}
#ifdef __BACKGROUND_MIS__
	else if(type == LIGHT_BACKGROUND) {
		/* infinite area light (e.g. light dome or env light) */
		float3 D = background_light_sample(kg, randu, randv, &ls->pdf);

		ls->P = D;
		ls->Ng = D;
		ls->D = -D;
		ls->t = FLT_MAX;
		ls->eval_fac = 1.0f;
	}
#endif
	else {
		ls->P = make_float3(data0.y, data0.z, data0.w);

		if(type == LIGHT_POINT || type == LIGHT_SPOT) {
			float radius = data1.y;

			if(radius > 0.0f)
				/* sphere light */
				ls->P += sphere_light_sample(P, ls->P, radius, randu, randv);
			else
				ls->use_mis = false;

			ls->D = normalize_len(ls->P - P, &ls->t);
			ls->Ng = -ls->D;

			float invarea = data1.z;
			ls->eval_fac = (0.25f*M_1_PI_F)*invarea;
			ls->pdf = invarea;

			if(type == LIGHT_SPOT) {
				/* spot light attentuation */
				float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
				ls->eval_fac *= spot_light_attenuation(data1, data2, ls);
			}
		}
		else {
			/* area light */
			float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
			float4 data3 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 3);

			float3 axisu = make_float3(data1.y, data1.z, data1.w);
			float3 axisv = make_float3(data2.y, data2.z, data2.w);
			float3 D = make_float3(data3.y, data3.z, data3.w);

			ls->P += area_light_sample(axisu, axisv, randu, randv);
			ls->Ng = D;
			ls->D = normalize_len(ls->P - P, &ls->t);

			float invarea = data2.x;

			if(invarea == 0.0f) {
				ls->use_mis = false;
				invarea = 1.0f;
			}

			ls->pdf = invarea;
			ls->eval_fac = 0.25f*ls->pdf;
		}
	}

	ls->shader = __float_as_int(data1.x);
	ls->object = ~0;
	ls->prim = ~0;
	ls->lamp = lamp;

	/* compute pdf */
	if(ls->t != FLT_MAX)
		ls->pdf *= lamp_light_pdf(kg, ls->Ng, -ls->D, ls->t);
	
	/* this is a bit weak, but we don't want this as part of the pdf for
	 * multiple importance sampling */
	ls->eval_fac *= kernel_data.integrator.inv_pdf_lights;
}

__device bool lamp_light_eval(KernelGlobals *kg, int lamp, float3 P, float3 D, float t, LightSample *ls)
{
	float4 data0 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 0);
	float4 data1 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 1);

	LightType type = (LightType)__float_as_int(data0.x);
	ls->type = type;
	ls->shader = __float_as_int(data1.x);
	ls->object = ~0;
	ls->prim = ~0;
	ls->lamp = lamp;
	ls->use_mis = false; /* flag not used for eval */

	if(type == LIGHT_DISTANT) {
		/* distant light */
		float radius = data1.y;

		if(radius == 0.0f)
			return false;
		if(t != FLT_MAX)
			return false;

		/* a distant light is infinitely far away, but equivalent to a disk
		 * shaped light exactly 1 unit away from the current shading point.
		 *
		 *     radius              t^2/cos(theta)
		 *  <---------->           t = sqrt(1^2 + tan(theta)^2)
		 *       tan(th)           area = radius*radius*pi
		 *       <----->
		 *        \    |           (1 + tan(theta)^2)/cos(theta)
		 *         \   |           (1 + tan(acos(cos(theta)))^2)/cos(theta)
		 *       t  \th| 1         simplifies to
		 *           \-|           1/(cos(theta)^3)
		 *            \|           magic!
		 *             P
		 */

		float3 lightD = make_float3(data0.y, data0.z, data0.w);
		float costheta = dot(-lightD, D);
		float cosangle = data1.z;

		if(costheta < cosangle)
			return false;

		ls->P = -D;
		ls->Ng = -D;
		ls->D = D;
		ls->t = FLT_MAX;

		float invarea = data1.w;
		ls->pdf = invarea/(costheta*costheta*costheta);
		ls->eval_fac = ls->pdf;
	}
	else if(type == LIGHT_POINT || type == LIGHT_SPOT) {
		float3 lightP = make_float3(data0.y, data0.z, data0.w);
		float radius = data1.y;

		/* sphere light */
		if(radius == 0.0f)
			return false;

		if(!ray_aligned_disk_intersect(P, D, t,
			lightP, radius, &ls->P, &ls->t))
			return false;

		ls->Ng = -D;
		ls->D = D;

		float invarea = data1.z;
		ls->eval_fac = (0.25f*M_1_PI_F)*invarea;
		ls->pdf = invarea;

		if(type == LIGHT_SPOT) {
			/* spot light attentuation */
			float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
			ls->eval_fac *= spot_light_attenuation(data1, data2, ls);

			if(ls->eval_fac == 0.0f)
				return false;
		}
	}
	else if(type == LIGHT_AREA) {
		/* area light */
		float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
		float4 data3 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 3);

		float invarea = data2.x;
		if(invarea == 0.0f)
			return false;

		float3 axisu = make_float3(data1.y, data1.z, data1.w);
		float3 axisv = make_float3(data2.y, data2.z, data2.w);
		float3 Ng = make_float3(data3.y, data3.z, data3.w);

		/* one sided */
		if(dot(D, Ng) >= 0.0f)
			return false;

		ls->P = make_float3(data0.y, data0.z, data0.w);

		if(!ray_quad_intersect(P, D, t,
			ls->P, axisu, axisv, &ls->P, &ls->t))
			return false;

		ls->D = D;
		ls->Ng = Ng;
		ls->pdf = invarea;
		ls->eval_fac = 0.25f*ls->pdf;
	}
	else
		return false;

	/* compute pdf */
	if(ls->t != FLT_MAX)
		ls->pdf *= lamp_light_pdf(kg, ls->Ng, -ls->D, ls->t);
	ls->eval_fac *= kernel_data.integrator.inv_pdf_lights;

	return true;
}

/* Triangle Light */

__device void object_transform_light_sample(KernelGlobals *kg, LightSample *ls, int object, float time)
{
#ifdef __INSTANCING__
	/* instance transform */
	if(object >= 0) {
#ifdef __OBJECT_MOTION__
		Transform itfm;
		Transform tfm = object_fetch_transform_motion_test(kg, object, time, &itfm);
#else
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
		Transform itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
#endif

		ls->P = transform_point(&tfm, ls->P);
		ls->Ng = normalize(transform_direction(&tfm, ls->Ng));
	}
#endif
}

__device void triangle_light_sample(KernelGlobals *kg, int prim, int object,
	float randu, float randv, float time, LightSample *ls)
{
	/* triangle, so get position, normal, shader */
	ls->P = triangle_sample_MT(kg, prim, randu, randv);
	ls->Ng = triangle_normal_MT(kg, prim, &ls->shader);
	ls->object = object;
	ls->prim = prim;
	ls->lamp = ~0;
	ls->use_mis = true;
	ls->t = 0.0f;
	ls->type = LIGHT_AREA;
	ls->eval_fac = 1.0f;

	object_transform_light_sample(kg, ls, object, time);
}

__device float triangle_light_pdf(KernelGlobals *kg,
	const float3 Ng, const float3 I, float t)
{
	float pdf = kernel_data.integrator.pdf_triangles;
	float cos_pi = fabsf(dot(Ng, I));

	if(cos_pi == 0.0f)
		return 0.0f;
	
	return t*t*pdf/cos_pi;
}

/* Curve Light */

#ifdef __HAIR__

__device void curve_segment_light_sample(KernelGlobals *kg, int prim, int object,
	int segment, float randu, float randv, float time, LightSample *ls)
{
	/* this strand code needs completion */
	float4 v00 = kernel_tex_fetch(__curves, prim);

	int k0 = __float_as_int(v00.x) + segment;
	int k1 = k0 + 1;

	float4 P1 = kernel_tex_fetch(__curve_keys, k0);
	float4 P2 = kernel_tex_fetch(__curve_keys, k1);

	float l = len(P2 - P1);

	float r1 = P1.w;
	float r2 = P2.w;
	float3 tg = float4_to_float3(P2 - P1) / l;
	float3 xc = make_float3(tg.x * tg.z, tg.y * tg.z, -(tg.x * tg.x + tg.y * tg.y));
	if (dot(xc, xc) == 0.0f)
		xc = make_float3(tg.x * tg.y, -(tg.x * tg.x + tg.z * tg.z), tg.z * tg.y);
	xc = normalize(xc);
	float3 yc = cross(tg, xc);
	float gd = ((r2 - r1)/l);

	/* normal currently ignores gradient */
	ls->Ng = sinf(2 * M_PI_F * randv) * xc + cosf(2 * M_PI_F * randv) * yc;
	ls->P = randu * l * tg + (gd * l + r1) * ls->Ng;
	ls->object = object;
	ls->prim = prim;
	ls->lamp = ~0;
	ls->use_mis = true;
	ls->t = 0.0f;
	ls->type = LIGHT_STRAND;
	ls->eval_fac = 1.0f;
	ls->shader = __float_as_int(v00.z);

	object_transform_light_sample(kg, ls, object, time);
}

#endif

/* Light Distribution */

__device int light_distribution_sample(KernelGlobals *kg, float randt)
{
	/* this is basically std::upper_bound as used by pbrt, to find a point light or
	 * triangle to emit from, proportional to area. a good improvement would be to
	 * also sample proportional to power, though it's not so well defined with
	 * OSL shaders. */
	int first = 0;
	int len = kernel_data.integrator.num_distribution + 1;

	while(len > 0) {
		int half_len = len >> 1;
		int middle = first + half_len;

		if(randt < kernel_tex_fetch(__light_distribution, middle).x) {
			len = half_len;
		}
		else {
			first = middle + 1;
			len = len - half_len - 1;
		}
	}

	/* clamping should not be needed but float rounding errors seem to
	 * make this fail on rare occasions */
	return clamp(first-1, 0, kernel_data.integrator.num_distribution-1);
}

/* Generic Light */

__device void light_sample(KernelGlobals *kg, float randt, float randu, float randv, float time, float3 P, LightSample *ls)
{
	/* sample index */
	int index = light_distribution_sample(kg, randt);

	/* fetch light data */
	float4 l = kernel_tex_fetch(__light_distribution, index);
	int prim = __float_as_int(l.y);

	if(prim >= 0) {
		int object = __float_as_int(l.w);
#ifdef __HAIR__
		int segment = __float_as_int(l.z);
#endif

#ifdef __HAIR__
		if (segment != ~0)
			curve_segment_light_sample(kg, prim, object, segment, randu, randv, time, ls);
		else
#endif
			triangle_light_sample(kg, prim, object, randu, randv, time, ls);

		/* compute incoming direction, distance and pdf */
		ls->D = normalize_len(ls->P - P, &ls->t);
		ls->pdf = triangle_light_pdf(kg, ls->Ng, -ls->D, ls->t);
	}
	else {
		int lamp = -prim-1;
		lamp_light_sample(kg, lamp, randu, randv, P, ls);
	}
}

__device int light_select_num_samples(KernelGlobals *kg, int index)
{
	float4 data3 = kernel_tex_fetch(__light_data, index*LIGHT_SIZE + 3);
	return __float_as_int(data3.x);
}

__device void light_select(KernelGlobals *kg, int index, float randu, float randv, float3 P, LightSample *ls)
{
	lamp_light_sample(kg, index, randu, randv, P, ls);
}

__device int lamp_light_eval_sample(KernelGlobals *kg, float randt)
{
	/* sample index */
	int index = light_distribution_sample(kg, randt);

	/* fetch light data */
	float4 l = kernel_tex_fetch(__light_distribution, index);
	int prim = __float_as_int(l.y);

	if(prim < 0) {
		int lamp = -prim-1;
		return lamp;
	}
	else
		return ~0;
}

CCL_NAMESPACE_END


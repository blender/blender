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

CCL_NAMESPACE_BEGIN

/* Light Sample result */

typedef struct LightSample {
	float3 P;			/* position on light, or direction for distant light */
	float3 Ng;			/* normal on light */
	float3 D;			/* direction from shading point to light */
	float t;			/* distance to light (FLT_MAX for distant light) */
	float u, v;			/* parametric coordinate on primitive */
	float pdf;			/* light sampling probability density function */
	float eval_fac;		/* intensity multiplier */
	int object;			/* object id for triangle/curve lights */
	int prim;			/* primitive id for triangle/curve lights */
	int shader;			/* shader id */
	int lamp;			/* lamp id */
	LightType type;		/* type of light */
} LightSample;

/* Background Light */

#ifdef __BACKGROUND_MIS__

ccl_device float3 background_light_sample(KernelGlobals *kg, float randu, float randv, float *pdf)
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
		*pdf = (cdf_u.x * cdf_v.x)/(M_2PI_F * M_PI_F * sin_theta * denom);

	*pdf *= kernel_data.integrator.pdf_lights;

	/* compute direction */
	return -equirectangular_to_direction(u, v);
}

ccl_device float background_light_pdf(KernelGlobals *kg, float3 direction)
{
	float2 uv = direction_to_equirectangular(direction);
	int res = kernel_data.integrator.pdf_background_res;

	float sin_theta = sinf(uv.y * M_PI_F);

	if(sin_theta == 0.0f)
		return 0.0f;

	int index_u = clamp(float_to_int(uv.x * res), 0, res - 1);
	int index_v = clamp(float_to_int(uv.y * res), 0, res - 1);

	/* pdfs in V direction */
	float2 cdf_last_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * (res + 1) + res);
	float2 cdf_last_v = kernel_tex_fetch(__light_background_marginal_cdf, res);

	float denom = cdf_last_u.x * cdf_last_v.x;

	if(denom == 0.0f)
		return 0.0f;

	/* pdfs in U direction */
	float2 cdf_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * (res + 1) + index_u);
	float2 cdf_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v);

	float pdf = (cdf_u.x * cdf_v.x)/(M_2PI_F * M_PI_F * sin_theta * denom);

	return pdf * kernel_data.integrator.pdf_lights;
}
#endif

/* Regular Light */

ccl_device float3 disk_light_sample(float3 v, float randu, float randv)
{
	float3 ru, rv;

	make_orthonormals(v, &ru, &rv);
	to_unit_disk(&randu, &randv);

	return ru*randu + rv*randv;
}

ccl_device float3 distant_light_sample(float3 D, float radius, float randu, float randv)
{
	return normalize(D + disk_light_sample(D, randu, randv)*radius);
}

ccl_device float3 sphere_light_sample(float3 P, float3 center, float radius, float randu, float randv)
{
	return disk_light_sample(normalize(P - center), randu, randv)*radius;
}

/* Uses the following paper:
 *
 * Carlos Urena et al.
 * An Area-Preserving Parametrization for Spherical Rectangles.
 *
 * https://www.solidangle.com/research/egsr2013_spherical_rectangle.pdf
 */
ccl_device float3 area_light_sample(float3 P,
                                    float3 light_p,
                                    float3 axisu, float3 axisv,
                                    float randu, float randv,
                                    float *pdf)
{
	/* In our name system we're using P for the center,
	 * which is o in the paper.
	 */

	float3 corner = light_p - axisu * 0.5f - axisv * 0.5f;
	float axisu_len, axisv_len;
	/* Compute local reference system R. */
	float3 x = normalize_len(axisu, &axisu_len);
	float3 y = normalize_len(axisv, &axisv_len);
	float3 z = cross(x, y);
	/* Compute rectangle coords in local reference system. */
	float3 dir = corner - P;
	float z0 = dot(dir, z);
	/* Flip 'z' to make it point against Q. */
	if(z0 > 0.0f) {
		z *= -1.0f;
		z0 *= -1.0f;
	}
	float z0sq = z0 * z0;
	float x0 = dot(dir, x);
	float y0 = dot(dir, y);
	float x1 = x0 + axisu_len;
	float y1 = y0 + axisv_len;
	float y0sq = y0 * y0;
	float y1sq = y1 * y1;
	/* Create vectors to four vertices. */
	float3 v00 = make_float3(x0, y0, z0);
	float3 v01 = make_float3(x0, y1, z0);
	float3 v10 = make_float3(x1, y0, z0);
	float3 v11 = make_float3(x1, y1, z0);
	/* Compute normals to edges. */
	float3 n0 = normalize(cross(v00, v10));
	float3 n1 = normalize(cross(v10, v11));
	float3 n2 = normalize(cross(v11, v01));
	float3 n3 = normalize(cross(v01, v00));
	/* Compute internal angles (gamma_i). */
	float g0 = safe_acosf(-dot(n0, n1));
	float g1 = safe_acosf(-dot(n1, n2));
	float g2 = safe_acosf(-dot(n2, n3));
	float g3 = safe_acosf(-dot(n3, n0));
	/* Compute predefined constants. */
	float b0 = n0.z;
	float b1 = n2.z;
	float b0sq = b0 * b0;
	float k = M_2PI_F - g2 - g3;
	/* Compute solid angle from internal angles. */
	float S = g0 + g1 - k;

	/* Compute cu. */
	float au = randu * S + k;
	float fu = (cosf(au) * b0 - b1) / sinf(au);
	float cu = 1.0f / sqrtf(fu * fu + b0sq) * (fu > 0.0f ? 1.0f : -1.0f);
	cu = clamp(cu, -1.0f, 1.0f);
	/* Compute xu. */
	float xu = -(cu * z0) / sqrtf(1.0f - cu * cu);
	xu = clamp(xu, x0, x1);
	/* Compute yv. */
	float d = sqrtf(xu * xu + z0sq);
	float h0 = y0 / sqrtf(d * d + y0sq);
	float h1 = y1 / sqrtf(d * d + y1sq);
	float hv = h0 + randv * (h1 - h0), hv2 = hv * hv;
	float yv = (hv2 < 1.0f - 1e-6f) ? (hv * d) / sqrtf(1.0f - hv2) : y1;

	if(S != 0.0f)
		*pdf = 1.0f / S;
	else
		*pdf = 0.0f;

	/* Transform (xu, yv, z0) to world coords. */
	return P + xu * x + yv * y + z0 * z;
}

/* TODO(sergey): This is actually a duplicated code from above, but how to avoid
 * this without having some nasty function with loads of parameters?
 */
ccl_device float area_light_pdf(float3 P,
                                float3 light_p,
                                float3 axisu, float3 axisv)
{
	/* In our name system we're using P for the center,
	 * which is o in the paper.
	 */

	float3 corner = light_p - axisu * 0.5f - axisv * 0.5f;
	float axisu_len, axisv_len;
	/* Compute local reference system R. */
	float3 x = normalize_len(axisu, &axisu_len);
	float3 y = normalize_len(axisv, &axisv_len);
	float3 z = cross(x, y);
	/* Compute rectangle coords in local reference system. */
	float3 dir = corner - P;
	float z0 = dot(dir, z);
	/* Flip 'z' to make it point against Q. */
	if(z0 > 0.0f) {
		z *= -1.0f;
		z0 *= -1.0f;
	}
	float x0 = dot(dir, x);
	float y0 = dot(dir, y);
	float x1 = x0 + axisu_len;
	float y1 = y0 + axisv_len;
	/* Create vectors to four vertices. */
	float3 v00 = make_float3(x0, y0, z0);
	float3 v01 = make_float3(x0, y1, z0);
	float3 v10 = make_float3(x1, y0, z0);
	float3 v11 = make_float3(x1, y1, z0);
	/* Compute normals to edges. */
	float3 n0 = normalize(cross(v00, v10));
	float3 n1 = normalize(cross(v10, v11));
	float3 n2 = normalize(cross(v11, v01));
	float3 n3 = normalize(cross(v01, v00));
	/* Compute internal angles (gamma_i). */
	float g0 = safe_acosf(-dot(n0, n1));
	float g1 = safe_acosf(-dot(n1, n2));
	float g2 = safe_acosf(-dot(n2, n3));
	float g3 = safe_acosf(-dot(n3, n0));
	/* Compute predefined constants. */
	float k = M_2PI_F - g2 - g3;
	/* Compute solid angle from internal angles. */
	float S = g0 + g1 - k;
	if(S != 0.0f)
		return 1.0f / S;
	else
		return 0.0f;
}

ccl_device float spot_light_attenuation(float4 data1, float4 data2, LightSample *ls)
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

ccl_device float lamp_light_pdf(KernelGlobals *kg, const float3 Ng, const float3 I, float t)
{
	float cos_pi = dot(Ng, I);

	if(cos_pi <= 0.0f)
		return 0.0f;
	
	return t*t/cos_pi;
}

ccl_device void lamp_light_sample(KernelGlobals *kg, int lamp,
	float randu, float randv, float3 P, LightSample *ls)
{
	float4 data0 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 0);
	float4 data1 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 1);

	LightType type = (LightType)__float_as_int(data0.x);
	ls->type = type;
	ls->shader = __float_as_int(data1.x);
	ls->object = PRIM_NONE;
	ls->prim = PRIM_NONE;
	ls->lamp = lamp;
	ls->u = randu;
	ls->v = randv;

	if(type == LIGHT_DISTANT) {
		/* distant light */
		float3 lightD = make_float3(data0.y, data0.z, data0.w);
		float3 D = lightD;
		float radius = data1.y;
		float invarea = data1.w;

		if(radius > 0.0f)
			D = distant_light_sample(D, radius, randu, randv);

		ls->P = D;
		ls->Ng = D;
		ls->D = -D;
		ls->t = FLT_MAX;

		float costheta = dot(lightD, D);
		ls->pdf = invarea/(costheta*costheta*costheta);
		ls->eval_fac = ls->pdf*kernel_data.integrator.inv_pdf_lights;
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

			ls->D = normalize_len(ls->P - P, &ls->t);
			ls->Ng = -ls->D;

			float invarea = data1.z;
			ls->eval_fac = (0.25f*M_1_PI_F)*invarea;
			ls->pdf = invarea;

			if(type == LIGHT_SPOT) {
				/* spot light attenuation */
				float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
				ls->eval_fac *= spot_light_attenuation(data1, data2, ls);
			}
			ls->pdf *= lamp_light_pdf(kg, ls->Ng, -ls->D, ls->t);
		}
		else {
			/* area light */
			float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
			float4 data3 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 3);

			float3 axisu = make_float3(data1.y, data1.z, data1.w);
			float3 axisv = make_float3(data2.y, data2.z, data2.w);
			float3 D = make_float3(data3.y, data3.z, data3.w);

			ls->P = area_light_sample(P, ls->P,
			                          axisu, axisv,
			                          randu, randv,
			                          &ls->pdf);

			ls->Ng = D;
			ls->D = normalize_len(ls->P - P, &ls->t);

			float invarea = data2.x;
			ls->eval_fac = 0.25f*invarea;

			if(dot(ls->D, D) > 0.0f)
				ls->pdf = 0.0f;
		}

		ls->eval_fac *= kernel_data.integrator.inv_pdf_lights;
	}
}

ccl_device bool lamp_light_eval(KernelGlobals *kg, int lamp, float3 P, float3 D, float t, LightSample *ls)
{
	float4 data0 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 0);
	float4 data1 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 1);

	LightType type = (LightType)__float_as_int(data0.x);
	ls->type = type;
	ls->shader = __float_as_int(data1.x);
	ls->object = PRIM_NONE;
	ls->prim = PRIM_NONE;
	ls->lamp = lamp;
	/* todo: missing texture coordinates */
	ls->u = 0.0f;
	ls->v = 0.0f;

	if(!(ls->shader & SHADER_USE_MIS))
		return false;

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

		/* compute pdf */
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
			/* spot light attenuation */
			float4 data2 = kernel_tex_fetch(__light_data, lamp*LIGHT_SIZE + 2);
			ls->eval_fac *= spot_light_attenuation(data1, data2, ls);

			if(ls->eval_fac == 0.0f)
				return false;
		}

		/* compute pdf */
		if(ls->t != FLT_MAX)
			ls->pdf *= lamp_light_pdf(kg, ls->Ng, -ls->D, ls->t);
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
		ls->pdf = area_light_pdf(P, ls->P, axisu, axisv);
		ls->eval_fac = 0.25f*invarea;
	}
	else
		return false;

	return true;
}

/* Triangle Light */

ccl_device void object_transform_light_sample(KernelGlobals *kg, LightSample *ls, int object, float time)
{
#ifdef __INSTANCING__
	/* instance transform */
	if(object >= 0) {
#ifdef __OBJECT_MOTION__
		Transform itfm;
		Transform tfm = object_fetch_transform_motion_test(kg, object, time, &itfm);
#else
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif

		ls->P = transform_point(&tfm, ls->P);
		ls->Ng = normalize(transform_direction(&tfm, ls->Ng));
	}
#endif
}

ccl_device void triangle_light_sample(KernelGlobals *kg, int prim, int object,
	float randu, float randv, float time, LightSample *ls)
{
	float u, v;

	/* compute random point in triangle */
	randu = sqrtf(randu);

	u = 1.0f - randu;
	v = randv*randu;

	/* triangle, so get position, normal, shader */
	triangle_point_normal(kg, object, prim, u, v, &ls->P, &ls->Ng, &ls->shader);
	ls->object = object;
	ls->prim = prim;
	ls->lamp = LAMP_NONE;
	ls->shader |= SHADER_USE_MIS;
	ls->t = 0.0f;
	ls->u = u;
	ls->v = v;
	ls->type = LIGHT_TRIANGLE;
	ls->eval_fac = 1.0f;

	object_transform_light_sample(kg, ls, object, time);
}

ccl_device float triangle_light_pdf(KernelGlobals *kg,
	const float3 Ng, const float3 I, float t)
{
	float pdf = kernel_data.integrator.pdf_triangles;
	float cos_pi = fabsf(dot(Ng, I));

	if(cos_pi == 0.0f)
		return 0.0f;
	
	return t*t*pdf/cos_pi;
}

/* Light Distribution */

ccl_device int light_distribution_sample(KernelGlobals *kg, float randt)
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

ccl_device bool light_select_reached_max_bounces(KernelGlobals *kg, int index, int bounce)
{
	float4 data4 = kernel_tex_fetch(__light_data, index*LIGHT_SIZE + 4);
	return (bounce > __float_as_int(data4.x));
}

ccl_device void light_sample(KernelGlobals *kg, float randt, float randu, float randv, float time, float3 P, int bounce, LightSample *ls)
{
	/* sample index */
	int index = light_distribution_sample(kg, randt);

	/* fetch light data */
	float4 l = kernel_tex_fetch(__light_distribution, index);
	int prim = __float_as_int(l.y);

	if(prim >= 0) {
		int object = __float_as_int(l.w);
		int shader_flag = __float_as_int(l.z);

		triangle_light_sample(kg, prim, object, randu, randv, time, ls);

		/* compute incoming direction, distance and pdf */
		ls->D = normalize_len(ls->P - P, &ls->t);
		ls->pdf = triangle_light_pdf(kg, ls->Ng, -ls->D, ls->t);
		ls->shader |= shader_flag;
	}
	else {
		int lamp = -prim-1;

		if(UNLIKELY(light_select_reached_max_bounces(kg, lamp, bounce))) {
			ls->pdf = 0.0f;
			return;
		}

		lamp_light_sample(kg, lamp, randu, randv, P, ls);
	}
}

ccl_device int light_select_num_samples(KernelGlobals *kg, int index)
{
	float4 data3 = kernel_tex_fetch(__light_data, index*LIGHT_SIZE + 3);
	return __float_as_int(data3.x);
}

CCL_NAMESPACE_END


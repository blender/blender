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

/* Area light sampling */

/* Uses the following paper:
 *
 * Carlos Urena et al.
 * An Area-Preserving Parametrization for Spherical Rectangles.
 *
 * https://www.solidangle.com/research/egsr2013_spherical_rectangle.pdf
 *
 * Note: light_p is modified when sample_coord is true.
 */
ccl_device_inline float rect_light_sample(float3 P,
                                          float3 *light_p,
                                          float3 axisu, float3 axisv,
                                          float randu, float randv,
                                          bool sample_coord)
{
	/* In our name system we're using P for the center,
	 * which is o in the paper.
	 */

	float3 corner = *light_p - axisu * 0.5f - axisv * 0.5f;
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
	/* Compute internal angles (gamma_i). */
	float4 diff = make_float4(x0, y1, x1, y0) - make_float4(x1, y0, x0, y1);
	float4 nz = make_float4(y0, x1, y1, x0) * diff;
	nz = nz / sqrt(sqr(z0 * diff) + sqr(nz));
	float g0 = safe_acosf(-nz.x * nz.y);
	float g1 = safe_acosf(-nz.y * nz.z);
	float g2 = safe_acosf(-nz.z * nz.w);
	float g3 = safe_acosf(-nz.w * nz.x);
	/* Compute predefined constants. */
	float b0 = nz.x;
	float b1 = nz.z;
	float b0sq = b0 * b0;
	float k = M_2PI_F - g2 - g3;
	/* Compute solid angle from internal angles. */
	float S = g0 + g1 - k;

	if(sample_coord) {
		/* Compute cu. */
		float au = randu * S + k;
		float fu = (cosf(au) * b0 - b1) / sinf(au);
		float cu = 1.0f / sqrtf(fu * fu + b0sq) * (fu > 0.0f ? 1.0f : -1.0f);
		cu = clamp(cu, -1.0f, 1.0f);
		/* Compute xu. */
		float xu = -(cu * z0) / max(sqrtf(1.0f - cu * cu), 1e-7f);
		xu = clamp(xu, x0, x1);
		/* Compute yv. */
		float z0sq = z0 * z0;
		float y0sq = y0 * y0;
		float y1sq = y1 * y1;
		float d = sqrtf(xu * xu + z0sq);
		float h0 = y0 / sqrtf(d * d + y0sq);
		float h1 = y1 / sqrtf(d * d + y1sq);
		float hv = h0 + randv * (h1 - h0), hv2 = hv * hv;
		float yv = (hv2 < 1.0f - 1e-6f) ? (hv * d) / sqrtf(1.0f - hv2) : y1;

		/* Transform (xu, yv, z0) to world coords. */
		*light_p = P + xu * x + yv * y + z0 * z;
	}

	/* return pdf */
	if(S != 0.0f)
		return 1.0f / S;
	else
		return 0.0f;
}

ccl_device_inline float3 ellipse_sample(float3 ru, float3 rv, float randu, float randv)
{
	to_unit_disk(&randu, &randv);
	return ru*randu + rv*randv;
}

ccl_device float3 disk_light_sample(float3 v, float randu, float randv)
{
	float3 ru, rv;

	make_orthonormals(v, &ru, &rv);

	return ellipse_sample(ru, rv, randu, randv);
}

ccl_device float3 distant_light_sample(float3 D, float radius, float randu, float randv)
{
	return normalize(D + disk_light_sample(D, randu, randv)*radius);
}

ccl_device float3 sphere_light_sample(float3 P, float3 center, float radius, float randu, float randv)
{
	return disk_light_sample(normalize(P - center), randu, randv)*radius;
}

ccl_device float spot_light_attenuation(float3 dir, float spot_angle, float spot_smooth, LightSample *ls)
{
	float3 I = ls->Ng;

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

/* Background Light */

#ifdef __BACKGROUND_MIS__

/* TODO(sergey): In theory it should be all fine to use noinline for all
 * devices, but we're so close to the release so better not screw things
 * up for CPU at least.
 */
#ifdef __KERNEL_GPU__
ccl_device_noinline
#else
ccl_device
#endif
float3 background_map_sample(KernelGlobals *kg, float randu, float randv, float *pdf)
{
	/* for the following, the CDF values are actually a pair of floats, with the
	 * function value as X and the actual CDF as Y.  The last entry's function
	 * value is the CDF total. */
	int res_x = kernel_data.integrator.pdf_background_res_x;
	int res_y = kernel_data.integrator.pdf_background_res_y;
	int cdf_width = res_x + 1;

	/* this is basically std::lower_bound as used by pbrt */
	int first = 0;
	int count = res_y;

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
	kernel_assert(index_v >= 0 && index_v < res_y);

	float2 cdf_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v);
	float2 cdf_next_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v + 1);
	float2 cdf_last_v = kernel_tex_fetch(__light_background_marginal_cdf, res_y);

	/* importance-sampled V direction */
	float dv = inverse_lerp(cdf_v.y, cdf_next_v.y, randv);
	float v = (index_v + dv) / res_y;

	/* this is basically std::lower_bound as used by pbrt */
	first = 0;
	count = res_x;
	while(count > 0) {
		int step = count >> 1;
		int middle = first + step;

		if(kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_width + middle).y < randu) {
			first = middle + 1;
			count -= step + 1;
		}
		else
			count = step;
	}

	int index_u = max(0, first - 1);
	kernel_assert(index_u >= 0 && index_u < res_x);

	float2 cdf_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_width + index_u);
	float2 cdf_next_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_width + index_u + 1);
	float2 cdf_last_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_width + res_x);

	/* importance-sampled U direction */
	float du = inverse_lerp(cdf_u.y, cdf_next_u.y, randu);
	float u = (index_u + du) / res_x;

	/* compute pdf */
	float denom = cdf_last_u.x * cdf_last_v.x;
	float sin_theta = sinf(M_PI_F * v);

	if(sin_theta == 0.0f || denom == 0.0f)
		*pdf = 0.0f;
	else
		*pdf = (cdf_u.x * cdf_v.x)/(M_2PI_F * M_PI_F * sin_theta * denom);

	/* compute direction */
	return equirectangular_to_direction(u, v);
}

/* TODO(sergey): Same as above, after the release we should consider using
 * 'noinline' for all devices.
 */
#ifdef __KERNEL_GPU__
ccl_device_noinline
#else
ccl_device
#endif
float background_map_pdf(KernelGlobals *kg, float3 direction)
{
	float2 uv = direction_to_equirectangular(direction);
	int res_x = kernel_data.integrator.pdf_background_res_x;
	int res_y = kernel_data.integrator.pdf_background_res_y;
	int cdf_width = res_x + 1;

	float sin_theta = sinf(uv.y * M_PI_F);

	if(sin_theta == 0.0f)
		return 0.0f;

	int index_u = clamp(float_to_int(uv.x * res_x), 0, res_x - 1);
	int index_v = clamp(float_to_int(uv.y * res_y), 0, res_y - 1);

	/* pdfs in V direction */
	float2 cdf_last_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_width + res_x);
	float2 cdf_last_v = kernel_tex_fetch(__light_background_marginal_cdf, res_y);

	float denom = cdf_last_u.x * cdf_last_v.x;

	if(denom == 0.0f)
		return 0.0f;

	/* pdfs in U direction */
	float2 cdf_u = kernel_tex_fetch(__light_background_conditional_cdf, index_v * cdf_width + index_u);
	float2 cdf_v = kernel_tex_fetch(__light_background_marginal_cdf, index_v);

	return (cdf_u.x * cdf_v.x)/(M_2PI_F * M_PI_F * sin_theta * denom);
}

ccl_device_inline bool background_portal_data_fetch_and_check_side(KernelGlobals *kg,
                                                                   float3 P,
                                                                   int index,
                                                                   float3 *lightpos,
                                                                   float3 *dir)
{
	int portal = kernel_data.integrator.portal_offset + index;
	const ccl_global KernelLight *klight = &kernel_tex_fetch(__lights, portal);

	*lightpos = make_float3(klight->co[0], klight->co[1], klight->co[2]);
	*dir = make_float3(klight->area.dir[0], klight->area.dir[1], klight->area.dir[2]);

	/* Check whether portal is on the right side. */
	if(dot(*dir, P - *lightpos) > 1e-4f)
		return true;

	return false;
}

ccl_device_inline float background_portal_pdf(KernelGlobals *kg,
                                              float3 P,
                                              float3 direction,
                                              int ignore_portal,
                                              bool *is_possible)
{
	float portal_pdf = 0.0f;

	int num_possible = 0;
	for(int p = 0; p < kernel_data.integrator.num_portals; p++) {
		if(p == ignore_portal)
			continue;

		float3 lightpos, dir;
		if(!background_portal_data_fetch_and_check_side(kg, P, p, &lightpos, &dir))
			continue;

		/* There's a portal that could be sampled from this position. */
		if(is_possible) {
			*is_possible = true;
		}
		num_possible++;

		int portal = kernel_data.integrator.portal_offset + p;
		const ccl_global KernelLight *klight = &kernel_tex_fetch(__lights, portal);
		float3 axisu = make_float3(klight->area.axisu[0], klight->area.axisu[1], klight->area.axisu[2]);
		float3 axisv = make_float3(klight->area.axisv[0], klight->area.axisv[1], klight->area.axisv[2]);
		bool is_round = (klight->area.invarea < 0.0f);

		if(!ray_quad_intersect(P, direction, 1e-4f, FLT_MAX, lightpos, axisu, axisv, dir, NULL, NULL, NULL, NULL, is_round))
			continue;

		if(is_round) {
			float t;
			float3 D = normalize_len(lightpos - P, &t);
			portal_pdf += fabsf(klight->area.invarea) * lamp_light_pdf(kg, dir, -D, t);
		}
		else {
			portal_pdf += rect_light_sample(P, &lightpos, axisu, axisv, 0.0f, 0.0f, false);
		}
	}

	if(ignore_portal >= 0) {
		/* We have skipped a portal that could be sampled as well. */
		num_possible++;
	}

	return (num_possible > 0)? portal_pdf / num_possible: 0.0f;
}

ccl_device int background_num_possible_portals(KernelGlobals *kg, float3 P)
{
	int num_possible_portals = 0;
	for(int p = 0; p < kernel_data.integrator.num_portals; p++) {
		float3 lightpos, dir;
		if(background_portal_data_fetch_and_check_side(kg, P, p, &lightpos, &dir))
			num_possible_portals++;
	}
	return num_possible_portals;
}

ccl_device float3 background_portal_sample(KernelGlobals *kg,
                                           float3 P,
                                           float randu,
                                           float randv,
                                           int num_possible,
                                           int *sampled_portal,
                                           float *pdf)
{
	/* Pick a portal, then re-normalize randv. */
	randv *= num_possible;
	int portal = (int)randv;
	randv -= portal;

	/* TODO(sergey): Some smarter way of finding portal to sample
	 * is welcome.
	 */
	for(int p = 0; p < kernel_data.integrator.num_portals; p++) {
		/* Search for the sampled portal. */
		float3 lightpos, dir;
		if(!background_portal_data_fetch_and_check_side(kg, P, p, &lightpos, &dir))
			continue;

		if(portal == 0) {
			/* p is the portal to be sampled. */
			int portal = kernel_data.integrator.portal_offset + p;
			const ccl_global KernelLight *klight = &kernel_tex_fetch(__lights, portal);
			float3 axisu = make_float3(klight->area.axisu[0], klight->area.axisu[1], klight->area.axisu[2]);
			float3 axisv = make_float3(klight->area.axisv[0], klight->area.axisv[1], klight->area.axisv[2]);
			bool is_round = (klight->area.invarea < 0.0f);

			float3 D;
			if(is_round) {
				lightpos += ellipse_sample(axisu*0.5f, axisv*0.5f, randu, randv);
				float t;
				D = normalize_len(lightpos - P, &t);
				*pdf = fabsf(klight->area.invarea) * lamp_light_pdf(kg, dir, -D, t);
			}
			else {
				*pdf = rect_light_sample(P, &lightpos,
				                         axisu, axisv,
				                         randu, randv,
				                         true);
				D = normalize(lightpos - P);
			}

			*pdf /= num_possible;
			*sampled_portal = p;
			return D;
		}

		portal--;
	}

	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline float3 background_light_sample(KernelGlobals *kg,
                                                 float3 P,
                                                 float randu, float randv,
                                                 float *pdf)
{
	/* Probability of sampling portals instead of the map. */
	float portal_sampling_pdf = kernel_data.integrator.portal_pdf;

	/* Check if there are portals in the scene which we can sample. */
	if(portal_sampling_pdf > 0.0f) {
		int num_portals = background_num_possible_portals(kg, P);
		if(num_portals > 0) {
			if(portal_sampling_pdf == 1.0f || randu < portal_sampling_pdf) {
				if(portal_sampling_pdf < 1.0f) {
					randu /= portal_sampling_pdf;
				}
				int portal;
				float3 D = background_portal_sample(kg, P, randu, randv, num_portals, &portal, pdf);
				if(num_portals > 1) {
					/* Ignore the chosen portal, its pdf is already included. */
					*pdf += background_portal_pdf(kg, P, D, portal, NULL);
				}
				/* We could also have sampled the map, so combine with MIS. */
				if(portal_sampling_pdf < 1.0f) {
					float cdf_pdf = background_map_pdf(kg, D);
					*pdf = (portal_sampling_pdf * (*pdf)
					     + (1.0f - portal_sampling_pdf) * cdf_pdf);
				}
				return D;
			}
			else {
				/* Sample map, but with nonzero portal_sampling_pdf for MIS. */
				randu = (randu - portal_sampling_pdf) / (1.0f - portal_sampling_pdf);
			}
		}
		else {
			/* We can't sample a portal.
			 * Check if we can sample the map instead.
			 */
			if(portal_sampling_pdf == 1.0f) {
				/* Use uniform as a fallback if we can't sample the map. */
				*pdf = 1.0f / M_4PI_F;
				return sample_uniform_sphere(randu, randv);
			}
			else {
				portal_sampling_pdf = 0.0f;
			}
		}
	}

	float3 D = background_map_sample(kg, randu, randv, pdf);
	/* Use MIS if portals could be sampled as well. */
	if(portal_sampling_pdf > 0.0f) {
		float portal_pdf = background_portal_pdf(kg, P, D, -1, NULL);
		*pdf = (portal_sampling_pdf * portal_pdf
		     + (1.0f - portal_sampling_pdf) * (*pdf));
	}
	return D;
}

ccl_device float background_light_pdf(KernelGlobals *kg, float3 P, float3 direction)
{
	/* Probability of sampling portals instead of the map. */
	float portal_sampling_pdf = kernel_data.integrator.portal_pdf;

	float portal_pdf = 0.0f, map_pdf = 0.0f;
	if(portal_sampling_pdf > 0.0f) {
		/* Evaluate PDF of sampling this direction by portal sampling. */
		bool is_possible = false;
		portal_pdf = background_portal_pdf(kg, P, direction, -1, &is_possible) * portal_sampling_pdf;
		if(!is_possible) {
			/* Portal sampling is not possible here because all portals point to the wrong side.
			 * If map sampling is possible, it would be used instead, otherwise fallback sampling is used. */
			if(portal_sampling_pdf == 1.0f) {
				return kernel_data.integrator.pdf_lights / M_4PI_F;
			}
			else {
				/* Force map sampling. */
				portal_sampling_pdf = 0.0f;
			}
		}
	}
	if(portal_sampling_pdf < 1.0f) {
		/* Evaluate PDF of sampling this direction by map sampling. */
		map_pdf = background_map_pdf(kg, direction) * (1.0f - portal_sampling_pdf);
	}
	return (portal_pdf + map_pdf) * kernel_data.integrator.pdf_lights;
}
#endif

/* Regular Light */

ccl_device_inline bool lamp_light_sample(KernelGlobals *kg,
                                         int lamp,
                                         float randu, float randv,
                                         float3 P,
                                         LightSample *ls)
{
	const ccl_global KernelLight *klight = &kernel_tex_fetch(__lights, lamp);
	LightType type = (LightType)klight->type;
	ls->type = type;
	ls->shader = klight->shader_id;
	ls->object = PRIM_NONE;
	ls->prim = PRIM_NONE;
	ls->lamp = lamp;
	ls->u = randu;
	ls->v = randv;

	if(type == LIGHT_DISTANT) {
		/* distant light */
		float3 lightD = make_float3(klight->co[0], klight->co[1], klight->co[2]);
		float3 D = lightD;
		float radius = klight->distant.radius;
		float invarea = klight->distant.invarea;

		if(radius > 0.0f)
			D = distant_light_sample(D, radius, randu, randv);

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
		float3 D = -background_light_sample(kg, P, randu, randv, &ls->pdf);

		ls->P = D;
		ls->Ng = D;
		ls->D = -D;
		ls->t = FLT_MAX;
		ls->eval_fac = 1.0f;
	}
#endif
	else {
		ls->P = make_float3(klight->co[0], klight->co[1], klight->co[2]);

		if(type == LIGHT_POINT || type == LIGHT_SPOT) {
			float radius = klight->spot.radius;

			if(radius > 0.0f)
				/* sphere light */
				ls->P += sphere_light_sample(P, ls->P, radius, randu, randv);

			ls->D = normalize_len(ls->P - P, &ls->t);
			ls->Ng = -ls->D;

			float invarea = klight->spot.invarea;
			ls->eval_fac = (0.25f*M_1_PI_F)*invarea;
			ls->pdf = invarea;

			if(type == LIGHT_SPOT) {
				/* spot light attenuation */
				float3 dir = make_float3(klight->spot.dir[0],
                                         klight->spot.dir[1],
				                         klight->spot.dir[2]);
				ls->eval_fac *= spot_light_attenuation(dir,
				                                       klight->spot.spot_angle,
				                                       klight->spot.spot_smooth,
				                                       ls);
				if(ls->eval_fac == 0.0f) {
					return false;
				}
			}
			float2 uv = map_to_sphere(ls->Ng);
			ls->u = uv.x;
			ls->v = uv.y;

			ls->pdf *= lamp_light_pdf(kg, ls->Ng, -ls->D, ls->t);
		}
		else {
			/* area light */
			float3 axisu = make_float3(klight->area.axisu[0],
			                           klight->area.axisu[1],
			                           klight->area.axisu[2]);
			float3 axisv = make_float3(klight->area.axisv[0],
			                           klight->area.axisv[1],
			                           klight->area.axisv[2]);
			float3 D = make_float3(klight->area.dir[0],
			                       klight->area.dir[1],
			                       klight->area.dir[2]);
			float invarea = fabsf(klight->area.invarea);
			bool is_round = (klight->area.invarea < 0.0f);

			if(dot(ls->P - P, D) > 0.0f) {
				return false;
			}

			float3 inplane;

			if(is_round) {
				inplane = ellipse_sample(axisu*0.5f, axisv*0.5f, randu, randv);
				ls->P += inplane;
				ls->pdf = invarea;
			}
			else {
				inplane = ls->P;
				ls->pdf = rect_light_sample(P, &ls->P,
				                            axisu, axisv,
				                            randu, randv,
				                            true);
				inplane = ls->P - inplane;
			}

			ls->u = dot(inplane, axisu) * (1.0f / dot(axisu, axisu)) + 0.5f;
			ls->v = dot(inplane, axisv) * (1.0f / dot(axisv, axisv)) + 0.5f;

			ls->Ng = D;
			ls->D = normalize_len(ls->P - P, &ls->t);

			ls->eval_fac = 0.25f*invarea;
			if(is_round) {
				ls->pdf *= lamp_light_pdf(kg, D, -ls->D, ls->t);
			}
		}
	}

	ls->pdf *= kernel_data.integrator.pdf_lights;

	return (ls->pdf > 0.0f);
}

ccl_device bool lamp_light_eval(KernelGlobals *kg, int lamp, float3 P, float3 D, float t, LightSample *ls)
{
	const ccl_global KernelLight *klight = &kernel_tex_fetch(__lights, lamp);
	LightType type = (LightType)klight->type;
	ls->type = type;
	ls->shader = klight->shader_id;
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
		float radius = klight->distant.radius;

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

		float3 lightD = make_float3(klight->co[0], klight->co[1], klight->co[2]);
		float costheta = dot(-lightD, D);
		float cosangle = klight->distant.cosangle;

		if(costheta < cosangle)
			return false;

		ls->P = -D;
		ls->Ng = -D;
		ls->D = D;
		ls->t = FLT_MAX;

		/* compute pdf */
		float invarea = klight->distant.invarea;
		ls->pdf = invarea/(costheta*costheta*costheta);
		ls->eval_fac = ls->pdf;
	}
	else if(type == LIGHT_POINT || type == LIGHT_SPOT) {
		float3 lightP = make_float3(klight->co[0], klight->co[1], klight->co[2]);

		float radius = klight->spot.radius;

		/* sphere light */
		if(radius == 0.0f)
			return false;

		if(!ray_aligned_disk_intersect(P, D, t,
		                               lightP, radius, &ls->P, &ls->t))
		{
			return false;
		}

		ls->Ng = -D;
		ls->D = D;

		float invarea = klight->spot.invarea;
		ls->eval_fac = (0.25f*M_1_PI_F)*invarea;
		ls->pdf = invarea;

		if(type == LIGHT_SPOT) {
			/* spot light attenuation */
			float3 dir = make_float3(klight->spot.dir[0],
			                         klight->spot.dir[1],
			                         klight->spot.dir[2]);
			ls->eval_fac *= spot_light_attenuation(dir,
			                                       klight->spot.spot_angle,
			                                       klight->spot.spot_smooth,
			                                       ls);

			if(ls->eval_fac == 0.0f)
				return false;
		}
		float2 uv = map_to_sphere(ls->Ng);
		ls->u = uv.x;
		ls->v = uv.y;

		/* compute pdf */
		if(ls->t != FLT_MAX)
			ls->pdf *= lamp_light_pdf(kg, ls->Ng, -ls->D, ls->t);
	}
	else if(type == LIGHT_AREA) {
		/* area light */
		float invarea = fabsf(klight->area.invarea);
		bool is_round = (klight->area.invarea < 0.0f);
		if(invarea == 0.0f)
			return false;

		float3 axisu = make_float3(klight->area.axisu[0],
		                           klight->area.axisu[1],
		                           klight->area.axisu[2]);
		float3 axisv = make_float3(klight->area.axisv[0],
		                           klight->area.axisv[1],
		                           klight->area.axisv[2]);
		float3 Ng = make_float3(klight->area.dir[0],
		                        klight->area.dir[1],
		                        klight->area.dir[2]);

		/* one sided */
		if(dot(D, Ng) >= 0.0f)
			return false;

		float3 light_P = make_float3(klight->co[0], klight->co[1], klight->co[2]);

		if(!ray_quad_intersect(P, D, 0.0f, t, light_P,
		                       axisu, axisv, Ng,
		                       &ls->P, &ls->t,
		                       &ls->u, &ls->v,
		                       is_round))
		{
			return false;
		}

		ls->D = D;
		ls->Ng = Ng;
		if(is_round) {
			ls->pdf = invarea * lamp_light_pdf(kg, Ng, -D, ls->t);
		}
		else {
			ls->pdf = rect_light_sample(P, &light_P, axisu, axisv, 0, 0, false);
		}
		ls->eval_fac = 0.25f*invarea;
	}
	else {
		return false;
	}

	ls->pdf *= kernel_data.integrator.pdf_lights;

	return true;
}

/* Triangle Light */

/* returns true if the triangle is has motion blur or an instancing transform applied */
ccl_device_inline bool triangle_world_space_vertices(KernelGlobals *kg, int object, int prim, float time, float3 V[3])
{
	bool has_motion = false;
	const int object_flag = kernel_tex_fetch(__object_flag, object);

	if(object_flag & SD_OBJECT_HAS_VERTEX_MOTION && time >= 0.0f) {
		motion_triangle_vertices(kg, object, prim, time, V);
		has_motion = true;
	}
	else {
		triangle_vertices(kg, prim, V);
	}

#ifdef __INSTANCING__
	if(!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
#  ifdef __OBJECT_MOTION__
		float object_time = (time >= 0.0f) ? time : 0.5f;
		Transform tfm = object_fetch_transform_motion_test(kg, object, object_time, NULL);
#  else
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#  endif
		V[0] = transform_point(&tfm, V[0]);
		V[1] = transform_point(&tfm, V[1]);
		V[2] = transform_point(&tfm, V[2]);
		has_motion = true;
	}
#endif
	return has_motion;
}

ccl_device_inline float triangle_light_pdf_area(KernelGlobals *kg, const float3 Ng, const float3 I, float t)
{
	float pdf = kernel_data.integrator.pdf_triangles;
	float cos_pi = fabsf(dot(Ng, I));

	if(cos_pi == 0.0f)
		return 0.0f;

	return t*t*pdf/cos_pi;
}

ccl_device_forceinline float triangle_light_pdf(KernelGlobals *kg, ShaderData *sd, float t)
{
	/* A naive heuristic to decide between costly solid angle sampling
	 * and simple area sampling, comparing the distance to the triangle plane
	 * to the length of the edges of the triangle. */

	float3 V[3];
	bool has_motion = triangle_world_space_vertices(kg, sd->object, sd->prim, sd->time, V);

	const float3 e0 = V[1] - V[0];
	const float3 e1 = V[2] - V[0];
	const float3 e2 = V[2] - V[1];
	const float longest_edge_squared = max(len_squared(e0), max(len_squared(e1), len_squared(e2)));
	const float3 N = cross(e0, e1);
	const float distance_to_plane = fabsf(dot(N, sd->I * t))/dot(N, N);

	if(longest_edge_squared > distance_to_plane*distance_to_plane) {
		/* sd contains the point on the light source
		 * calculate Px, the point that we're shading */
		const float3 Px = sd->P + sd->I * t;
		const float3 v0_p = V[0] - Px;
		const float3 v1_p = V[1] - Px;
		const float3 v2_p = V[2] - Px;

		const float3 u01 = safe_normalize(cross(v0_p, v1_p));
		const float3 u02 = safe_normalize(cross(v0_p, v2_p));
		const float3 u12 = safe_normalize(cross(v1_p, v2_p));

		const float alpha = fast_acosf(dot(u02, u01));
		const float beta = fast_acosf(-dot(u01, u12));
		const float gamma = fast_acosf(dot(u02, u12));
		const float solid_angle =  alpha + beta + gamma - M_PI_F;

		/* pdf_triangles is calculated over triangle area, but we're not sampling over its area */
		if(UNLIKELY(solid_angle == 0.0f)) {
			return 0.0f;
		}
		else {
			float area = 1.0f;
			if(has_motion) {
				/* get the center frame vertices, this is what the PDF was calculated from */
				triangle_world_space_vertices(kg, sd->object, sd->prim, -1.0f, V);
				area = triangle_area(V[0], V[1], V[2]);
			}
			else {
				area = 0.5f * len(N);
			}
			const float pdf = area * kernel_data.integrator.pdf_triangles;
			return pdf / solid_angle;
		}
	}
	else {
		float pdf = triangle_light_pdf_area(kg, sd->Ng, sd->I, t);
		if(has_motion) {
			const float	area = 0.5f * len(N);
			if(UNLIKELY(area == 0.0f)) {
				return 0.0f;
			}
			/* scale the PDF.
			 * area = the area the sample was taken from
			 * area_pre = the are from which pdf_triangles was calculated from */
			triangle_world_space_vertices(kg, sd->object, sd->prim, -1.0f, V);
			const float area_pre = triangle_area(V[0], V[1], V[2]);
			pdf = pdf * area_pre / area;
		}
		return pdf;
	}
}

ccl_device_forceinline void triangle_light_sample(KernelGlobals *kg, int prim, int object,
	float randu, float randv, float time, LightSample *ls, const float3 P)
{
	/* A naive heuristic to decide between costly solid angle sampling
	 * and simple area sampling, comparing the distance to the triangle plane
	 * to the length of the edges of the triangle. */

	float3 V[3];
	bool has_motion = triangle_world_space_vertices(kg, object, prim, time, V);

	const float3 e0 = V[1] - V[0];
	const float3 e1 = V[2] - V[0];
	const float3 e2 = V[2] - V[1];
	const float longest_edge_squared = max(len_squared(e0), max(len_squared(e1), len_squared(e2)));
	const float3 N0 = cross(e0, e1);
	float Nl = 0.0f;
	ls->Ng = safe_normalize_len(N0, &Nl);
	float area = 0.5f * Nl;

	/* flip normal if necessary */
	const int object_flag = kernel_tex_fetch(__object_flag, object);
	if(object_flag & SD_OBJECT_NEGATIVE_SCALE_APPLIED) {
		ls->Ng = -ls->Ng;
	}
	ls->eval_fac = 1.0f;
	ls->shader = kernel_tex_fetch(__tri_shader, prim);
	ls->object = object;
	ls->prim = prim;
	ls->lamp = LAMP_NONE;
	ls->shader |= SHADER_USE_MIS;
	ls->type = LIGHT_TRIANGLE;

	float distance_to_plane = fabsf(dot(N0, V[0] - P)/dot(N0, N0));

	if(longest_edge_squared > distance_to_plane*distance_to_plane) {
		/* see James Arvo, "Stratified Sampling of Spherical Triangles"
		 * http://www.graphics.cornell.edu/pubs/1995/Arv95c.pdf */

		/* project the triangle to the unit sphere
		 * and calculate its edges and angles */
		const float3 v0_p = V[0] - P;
		const float3 v1_p = V[1] - P;
		const float3 v2_p = V[2] - P;

		const float3 u01 = safe_normalize(cross(v0_p, v1_p));
		const float3 u02 = safe_normalize(cross(v0_p, v2_p));
		const float3 u12 = safe_normalize(cross(v1_p, v2_p));

		const float3 A = safe_normalize(v0_p);
		const float3 B = safe_normalize(v1_p);
		const float3 C = safe_normalize(v2_p);

		const float cos_alpha = dot(u02, u01);
		const float cos_beta = -dot(u01, u12);
		const float cos_gamma = dot(u02, u12);

		/* calculate dihedral angles */
		const float alpha = fast_acosf(cos_alpha);
		const float beta = fast_acosf(cos_beta);
		const float gamma = fast_acosf(cos_gamma);
		/* the area of the unit spherical triangle = solid angle */
		const float solid_angle =  alpha + beta + gamma - M_PI_F;

		/* precompute a few things
		 * these could be re-used to take several samples
		 * as they are independent of randu/randv */
		const float cos_c = dot(A, B);
		const float sin_alpha = fast_sinf(alpha);
		const float product = sin_alpha * cos_c;

		/* Select a random sub-area of the spherical triangle
		 * and calculate the third vertex C_ of that new triangle */
		const float phi = randu * solid_angle - alpha;
		float s, t;
		fast_sincosf(phi, &s, &t);
		const float u = t - cos_alpha;
		const float v = s + product;

		const float3 U = safe_normalize(C - dot(C, A) * A);

		float q = 1.0f;
		const float det = ((v * s + u * t) * sin_alpha);
		if(det != 0.0f) {
			q = ((v * t - u * s) * cos_alpha - v) / det;
		}
		const float temp = max(1.0f - q*q, 0.0f);

		const float3 C_ = safe_normalize(q * A + sqrtf(temp) * U);

		/* Finally, select a random point along the edge of the new triangle
		 * That point on the spherical triangle is the sampled ray direction */
		const float z = 1.0f - randv * (1.0f - dot(C_, B));
		ls->D = z * B + safe_sqrtf(1.0f - z*z) * safe_normalize(C_ - dot(C_, B) * B);

		/* calculate intersection with the planar triangle */
		if(!ray_triangle_intersect(P, ls->D, FLT_MAX,
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
		                           (ssef*)V,
#else
		                           V[0], V[1], V[2],
#endif
		                           &ls->u, &ls->v, &ls->t)) {
			ls->pdf = 0.0f;
			return;
		}

		ls->P = P + ls->D * ls->t;

		/* pdf_triangles is calculated over triangle area, but we're sampling over solid angle */
		if(UNLIKELY(solid_angle == 0.0f)) {
			ls->pdf = 0.0f;
			return;
		}
		else {
			if(has_motion) {
				/* get the center frame vertices, this is what the PDF was calculated from */
				triangle_world_space_vertices(kg, object, prim, -1.0f, V);
				area = triangle_area(V[0], V[1], V[2]);
			}
			const float pdf = area * kernel_data.integrator.pdf_triangles;
			ls->pdf = pdf / solid_angle;
		}
	}
	else {
		/* compute random point in triangle */
		randu = sqrtf(randu);

		const float u = 1.0f - randu;
		const float v = randv*randu;
		const float t = 1.0f - u - v;
		ls->P = u * V[0] + v * V[1] + t * V[2];
		/* compute incoming direction, distance and pdf */
		ls->D = normalize_len(ls->P - P, &ls->t);
		ls->pdf = triangle_light_pdf_area(kg, ls->Ng, -ls->D, ls->t);
		if(has_motion && area != 0.0f) {
			/* scale the PDF.
			 * area = the area the sample was taken from
			 * area_pre = the are from which pdf_triangles was calculated from */
			triangle_world_space_vertices(kg, object, prim, -1.0f, V);
			const float area_pre = triangle_area(V[0], V[1], V[2]);
			ls->pdf = ls->pdf * area_pre / area;
		}
		ls->u = u;
		ls->v = v;
	}
}

/* Light Distribution */

ccl_device int light_distribution_sample(KernelGlobals *kg, float *randu)
{
	/* This is basically std::upper_bound as used by pbrt, to find a point light or
	 * triangle to emit from, proportional to area. a good improvement would be to
	 * also sample proportional to power, though it's not so well defined with
	 * arbitrary shaders. */
	int first = 0;
	int len = kernel_data.integrator.num_distribution + 1;
	float r = *randu;

	while(len > 0) {
		int half_len = len >> 1;
		int middle = first + half_len;

		if(r < kernel_tex_fetch(__light_distribution, middle).totarea) {
			len = half_len;
		}
		else {
			first = middle + 1;
			len = len - half_len - 1;
		}
	}

	/* Clamping should not be needed but float rounding errors seem to
	 * make this fail on rare occasions. */
	int index = clamp(first-1, 0, kernel_data.integrator.num_distribution-1);

	/* Rescale to reuse random number. this helps the 2D samples within
	 * each area light be stratified as well. */
	float distr_min = kernel_tex_fetch(__light_distribution, index).totarea;
	float distr_max = kernel_tex_fetch(__light_distribution, index+1).totarea;
	*randu = (r - distr_min)/(distr_max - distr_min);

	return index;
}

/* Generic Light */

ccl_device bool light_select_reached_max_bounces(KernelGlobals *kg, int index, int bounce)
{
	return (bounce > kernel_tex_fetch(__lights, index).max_bounces);
}

ccl_device_noinline bool light_sample(KernelGlobals *kg,
                                      float randu,
                                      float randv,
                                      float time,
                                      float3 P,
                                      int bounce,
                                      LightSample *ls)
{
	/* sample index */
	int index = light_distribution_sample(kg, &randu);

	/* fetch light data */
	const ccl_global KernelLightDistribution *kdistribution = &kernel_tex_fetch(__light_distribution, index);
	int prim = kdistribution->prim;

	if(prim >= 0) {
		int object = kdistribution->mesh_light.object_id;
		int shader_flag = kdistribution->mesh_light.shader_flag;

		triangle_light_sample(kg, prim, object, randu, randv, time, ls, P);
		ls->shader |= shader_flag;
		return (ls->pdf > 0.0f);
	}
	else {
		int lamp = -prim-1;

		if(UNLIKELY(light_select_reached_max_bounces(kg, lamp, bounce))) {
			return false;
		}

		return lamp_light_sample(kg, lamp, randu, randv, P, ls);
	}
}

ccl_device int light_select_num_samples(KernelGlobals *kg, int index)
{
	return kernel_tex_fetch(__lights, index).samples;
}

CCL_NAMESPACE_END

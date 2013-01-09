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

/* Direction Emission */

__device float3 direct_emissive_eval(KernelGlobals *kg, float rando,
	LightSample *ls, float u, float v, float3 I, float t, float time)
{
	/* setup shading at emitter */
	ShaderData sd;
	float3 eval;

#ifdef __BACKGROUND_MIS__
	if(ls->type == LIGHT_BACKGROUND) {
		Ray ray;
		ray.D = ls->D;
		ray.P = ls->P;
		ray.t = 1.0f;
#ifdef __OBJECT_MOTION__
		ray.time = time;
#endif
		ray.dP.dx = make_float3(0.0f, 0.0f, 0.0f);
		ray.dP.dy = make_float3(0.0f, 0.0f, 0.0f);
#ifdef __CAMERA_MOTION__
		ray.time = time;
#endif
		shader_setup_from_background(kg, &sd, &ray);
		eval = shader_eval_background(kg, &sd, 0, SHADER_CONTEXT_EMISSION);
	}
	else
#endif
	{
#ifdef __HAIR__
		if(ls->type == LIGHT_STRAND)
			shader_setup_from_sample(kg, &sd, ls->P, ls->Ng, I, ls->shader, ls->object, ls->prim, u, v, t, time, ls->prim);
		else
#endif
			shader_setup_from_sample(kg, &sd, ls->P, ls->Ng, I, ls->shader, ls->object, ls->prim, u, v, t, time);

		ls->Ng = sd.Ng;

		/* no path flag, we're evaluating this for all closures. that's weak but
		 * we'd have to do multiple evaluations otherwise */
		shader_eval_surface(kg, &sd, rando, 0, SHADER_CONTEXT_EMISSION);

		/* evaluate emissive closure */
		if(sd.flag & SD_EMISSION)
			eval = shader_emissive_eval(kg, &sd);
		else
			eval = make_float3(0.0f, 0.0f, 0.0f);
	}
	
	eval *= ls->eval_fac;

	shader_release(kg, &sd);

	return eval;
}

__device bool direct_emission(KernelGlobals *kg, ShaderData *sd, int lindex,
	float randt, float rando, float randu, float randv, Ray *ray, BsdfEval *eval,
	int *lamp)
{
	LightSample ls;

#ifdef __NON_PROGRESSIVE__
	if(lindex != -1) {
		/* sample position on a specified light */
		light_select(kg, lindex, randu, randv, sd->P, &ls);
	}
	else
#endif
	{
		/* sample a light and position on int */
		light_sample(kg, randt, randu, randv, sd->time, sd->P, &ls);
	}

	/* return lamp index for MIS */
	if(ls.use_mis)
		*lamp = ls.lamp;
	else
		*lamp= ~0;

	if(ls.pdf == 0.0f)
		return false;

	/* evaluate closure */
	float3 light_eval = direct_emissive_eval(kg, rando, &ls, randu, randv, -ls.D, ls.t, sd->time);

	if(is_zero(light_eval))
		return false;

	/* todo: use visbility flag to skip lights */

	/* evaluate BSDF at shading point */
	float bsdf_pdf;

	shader_bsdf_eval(kg, sd, ls.D, eval, &bsdf_pdf);

	if(ls.use_mis) {
		/* multiple importance sampling */
		float mis_weight = power_heuristic(ls.pdf, bsdf_pdf);
		light_eval *= mis_weight;
	}
	
	bsdf_eval_mul(eval, light_eval/ls.pdf);

	if(bsdf_eval_is_zero(eval))
		return false;

	if(ls.shader & SHADER_CAST_SHADOW) {
		/* setup ray */
		bool transmit = (dot(sd->Ng, ls.D) < 0.0f);
		ray->P = ray_offset(sd->P, (transmit)? -sd->Ng: sd->Ng);

		if(ls.t == FLT_MAX) {
			/* distant light */
			ray->D = ls.D;
			ray->t = ls.t;
		}
		else {
			/* other lights, avoid self-intersection */
			ray->D = ray_offset(ls.P, ls.Ng) - ray->P;
			ray->D = normalize_len(ray->D, &ray->t);
		}
	}
	else {
		/* signal to not cast shadow ray */
		ray->t = 0.0f;
	}

	return true;
}

/* Indirect Primitive Emission */

__device float3 indirect_primitive_emission(KernelGlobals *kg, ShaderData *sd, float t, int path_flag, float bsdf_pdf)
{
	/* evaluate emissive closure */
	float3 L = shader_emissive_eval(kg, sd);

#ifdef __HAIR__
	if(!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_SAMPLE_AS_LIGHT) && (sd->segment == ~0)) {
#else
	if(!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_SAMPLE_AS_LIGHT)) {
#endif
		/* multiple importance sampling, get triangle light pdf,
		 * and compute weight with respect to BSDF pdf */
		float pdf = triangle_light_pdf(kg, sd->Ng, sd->I, t);
		float mis_weight = power_heuristic(bsdf_pdf, pdf);

		return L*mis_weight;
	}

	return L;
}

/* Indirect Lamp Emission */

__device bool indirect_lamp_emission(KernelGlobals *kg, Ray *ray, int path_flag, float bsdf_pdf, float randt, float3 *emission)
{
	LightSample ls;
	int lamp = lamp_light_eval_sample(kg, randt);

	if(lamp == ~0)
		return false;

	if(!lamp_light_eval(kg, lamp, ray->P, ray->D, ray->t, &ls))
		return false;
	
	/* todo: missing texture coordinates */
	float u = 0.0f;
	float v = 0.0f;
	float3 L = direct_emissive_eval(kg, 0.0f, &ls, u, v, -ray->D, ls.t, ray->time);

	if(!(path_flag & PATH_RAY_MIS_SKIP)) {
		/* multiple importance sampling, get regular light pdf,
		 * and compute weight with respect to BSDF pdf */
		float mis_weight = power_heuristic(bsdf_pdf, ls.pdf);
		L *= mis_weight;
	}

	*emission = L;
	return true;
}

/* Indirect Background */

__device float3 indirect_background(KernelGlobals *kg, Ray *ray, int path_flag, float bsdf_pdf)
{
#ifdef __BACKGROUND__
	/* evaluate background closure */
	ShaderData sd;
	shader_setup_from_background(kg, &sd, ray);
	float3 L = shader_eval_background(kg, &sd, path_flag, SHADER_CONTEXT_EMISSION);
	shader_release(kg, &sd);

#ifdef __BACKGROUND_MIS__
	/* check if background light exists or if we should skip pdf */
	int res = kernel_data.integrator.pdf_background_res;

	if(!(path_flag & PATH_RAY_MIS_SKIP) && res) {
		/* multiple importance sampling, get background light pdf for ray
		 * direction, and compute weight with respect to BSDF pdf */
		float pdf = background_light_pdf(kg, ray->D);
		float mis_weight = power_heuristic(bsdf_pdf, pdf);

		return L*mis_weight;
	}
#endif

	return L;
#else
	return make_float3(0.8f, 0.8f, 0.8f);
#endif
}

CCL_NAMESPACE_END


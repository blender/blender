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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* Direction Emission */

ccl_device_noinline float3 direct_emissive_eval(KernelGlobals *kg, float rando,
	LightSample *ls, float3 I, differential3 dI, float t, float time, int bounce)
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
		ray.dP = differential3_zero();
		ray.dD = dI;
#ifdef __CAMERA_MOTION__
		ray.time = time;
#endif
		shader_setup_from_background(kg, &sd, &ray, bounce+1);
		eval = shader_eval_background(kg, &sd, 0, SHADER_CONTEXT_EMISSION);
	}
	else
#endif
	{
#ifdef __HAIR__
		if(ls->type == LIGHT_STRAND)
			shader_setup_from_sample(kg, &sd, ls->P, ls->Ng, I, ls->shader, ls->object, ls->prim, ls->u, ls->v, t, time, bounce+1, ls->prim);
		else
#endif
			shader_setup_from_sample(kg, &sd, ls->P, ls->Ng, I, ls->shader, ls->object, ls->prim, ls->u, ls->v, t, time, bounce+1, ~0);

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

	return eval;
}

ccl_device_noinline bool direct_emission(KernelGlobals *kg, ShaderData *sd, int lindex,
	float randt, float rando, float randu, float randv, Ray *ray, BsdfEval *eval,
	bool *is_lamp, int bounce)
{
	LightSample ls;

#ifdef __BRANCHED_PATH__
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

	if(ls.pdf == 0.0f)
		return false;

	/* todo: implement */
	differential3 dD = differential3_zero();

	/* evaluate closure */
	float3 light_eval = direct_emissive_eval(kg, rando, &ls, -ls.D, dD, ls.t, sd->time, bounce);

	if(is_zero(light_eval))
		return false;

	/* evaluate BSDF at shading point */
	float bsdf_pdf;

#ifdef __VOLUME__
	if(sd->prim != ~0)
		shader_bsdf_eval(kg, sd, ls.D, eval, &bsdf_pdf);
	else
		shader_volume_phase_eval(kg, sd, ls.D, eval, &bsdf_pdf);
#else
	shader_bsdf_eval(kg, sd, ls.D, eval, &bsdf_pdf);
#endif

	if(ls.shader & SHADER_USE_MIS) {
		/* multiple importance sampling */
		float mis_weight = power_heuristic(ls.pdf, bsdf_pdf);
		light_eval *= mis_weight;
	}
	
	bsdf_eval_mul(eval, light_eval/ls.pdf);

#ifdef __PASSES__
	/* use visibility flag to skip lights */
	if(ls.shader & SHADER_EXCLUDE_ANY) {
		if(ls.shader & SHADER_EXCLUDE_DIFFUSE)
			eval->diffuse = make_float3(0.0f, 0.0f, 0.0f);
		if(ls.shader & SHADER_EXCLUDE_GLOSSY)
			eval->glossy = make_float3(0.0f, 0.0f, 0.0f);
		if(ls.shader & SHADER_EXCLUDE_TRANSMIT)
			eval->transmission = make_float3(0.0f, 0.0f, 0.0f);
	}
#endif

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

		ray->dP = sd->dP;
		ray->dD = differential3_zero();
	}
	else {
		/* signal to not cast shadow ray */
		ray->t = 0.0f;
	}

	/* return if it's a lamp for shadow pass */
	*is_lamp = (ls.prim == ~0 && ls.type != LIGHT_BACKGROUND);

	return true;
}

/* Indirect Primitive Emission */

ccl_device_noinline float3 indirect_primitive_emission(KernelGlobals *kg, ShaderData *sd, float t, int path_flag, float bsdf_pdf)
{
	/* evaluate emissive closure */
	float3 L = shader_emissive_eval(kg, sd);

#ifdef __HAIR__
	if(!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_USE_MIS) && (sd->segment == ~0)) {
#else
	if(!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_USE_MIS)) {
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

ccl_device_noinline bool indirect_lamp_emission(KernelGlobals *kg, Ray *ray, int path_flag, float bsdf_pdf, float randt, float3 *emission, int bounce)
{
	LightSample ls;
	int lamp = lamp_light_eval_sample(kg, randt);

	if(lamp == ~0)
		return false;

	if(!lamp_light_eval(kg, lamp, ray->P, ray->D, ray->t, &ls))
		return false;

#ifdef __PASSES__
	/* use visibility flag to skip lights */
	if(ls.shader & SHADER_EXCLUDE_ANY) {
		if(((ls.shader & SHADER_EXCLUDE_DIFFUSE) && (path_flag & PATH_RAY_DIFFUSE)) ||
		   ((ls.shader & SHADER_EXCLUDE_GLOSSY) && (path_flag & PATH_RAY_GLOSSY)) ||
		   ((ls.shader & SHADER_EXCLUDE_TRANSMIT) && (path_flag & PATH_RAY_TRANSMIT)))
			return false;
	}
#endif

	float3 L = direct_emissive_eval(kg, 0.0f, &ls, -ray->D, ray->dD, ls.t, ray->time, bounce);

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

ccl_device_noinline float3 indirect_background(KernelGlobals *kg, Ray *ray, int path_flag, float bsdf_pdf, int bounce)
{
#ifdef __BACKGROUND__
	int shader = kernel_data.background.surface_shader;

	/* use visibility flag to skip lights */
	if(shader & SHADER_EXCLUDE_ANY) {
		if(((shader & SHADER_EXCLUDE_DIFFUSE) && (path_flag & PATH_RAY_DIFFUSE)) ||
		   ((shader & SHADER_EXCLUDE_GLOSSY) && (path_flag & PATH_RAY_GLOSSY)) ||
		   ((shader & SHADER_EXCLUDE_TRANSMIT) && (path_flag & PATH_RAY_TRANSMIT)) ||
		   ((shader & SHADER_EXCLUDE_CAMERA) && (path_flag & PATH_RAY_CAMERA)))
			return make_float3(0.0f, 0.0f, 0.0f);
	}

	/* evaluate background closure */
	ShaderData sd;
	shader_setup_from_background(kg, &sd, ray, bounce+1);

	float3 L = shader_eval_background(kg, &sd, path_flag, SHADER_CONTEXT_EMISSION);

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


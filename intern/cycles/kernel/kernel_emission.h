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
	LightSample *ls, float u, float v, float3 I)
{
	/* setup shading at emitter */
	ShaderData sd;

	shader_setup_from_sample(kg, &sd, ls->P, ls->Ng, I, ls->shader, ls->object, ls->prim, u, v);
	ls->Ng = sd.Ng;

	/* no path flag, we're evaluating this for all closures. that's weak but
	   we'd have to do multiple evaluations otherwise */
	shader_eval_surface(kg, &sd, rando, 0);
	
	float3 eval;

	/* evaluate emissive closure */
	if(sd.flag & SD_EMISSION)
		eval = shader_emissive_eval(kg, &sd);
	else
		eval = make_float3(0.0f, 0.0f, 0.0f);

	shader_release(kg, &sd);

	return eval;
}

__device bool direct_emission(KernelGlobals *kg, ShaderData *sd, int lindex,
	float randt, float rando, float randu, float randv, Ray *ray, float3 *eval)
{
	LightSample ls;

#ifdef __MULTI_LIGHT__
	if(lindex != -1) {
		/* sample position on a specified light */
		light_select(kg, lindex, randu, randv, sd->P, &ls);
	}
	else
#endif
	{
		/* sample a light and position on int */
		light_sample(kg, randt, randu, randv, sd->P, &ls);
	}

	/* compute pdf */
	float pdf = light_sample_pdf(kg, &ls, -ls.D, ls.t);

	/* evaluate closure */
	*eval = direct_emissive_eval(kg, rando, &ls, randu, randv, -ls.D);

	if(is_zero(*eval) || pdf == 0.0f)
		return false;

	/* todo: use visbility flag to skip lights */

	/* evaluate BSDF at shading point */
	float bsdf_pdf;
	float3 bsdf_eval = shader_bsdf_eval(kg, sd, ls.D, &bsdf_pdf);

	*eval *= bsdf_eval/pdf;

	if(is_zero(*eval))
		return false;

	if(ls.prim != ~0) {
		/* multiple importance sampling */
		float mis_weight = power_heuristic(pdf, bsdf_pdf);
		*eval *= mis_weight;
	}
	/* todo: clean up these weights */
	else if(ls.shader & SHADER_AREA_LIGHT)
		*eval *= 0.25f; /* area lamp */
	else if(ls.t != FLT_MAX)
		*eval *= 0.25f*M_1_PI_F; /* point lamp */

	if(ls.shader & SHADER_CAST_SHADOW) {
		/* setup ray */
		ray->P = ray_offset(sd->P, sd->Ng);

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

/* Indirect Emission */

__device float3 indirect_emission(KernelGlobals *kg, ShaderData *sd, float t, int path_flag, float bsdf_pdf)
{
	/* evaluate emissive closure */
	float3 L = shader_emissive_eval(kg, sd);

	if(!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_SAMPLE_AS_LIGHT)) {
		/* multiple importance sampling */
		float pdf = triangle_light_pdf(kg, sd->Ng, sd->I, t);
		float mis_weight = power_heuristic(bsdf_pdf, pdf);

		return L*mis_weight;
	}

	return L;
}

CCL_NAMESPACE_END


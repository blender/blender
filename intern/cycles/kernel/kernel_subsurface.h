/*
 * Copyright 2013, Blender Foundation.
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

#include "closure/bssrdf.h"

/* NEW BSSRDF: See "BSSRDF Importance Sampling", SIGGRAPH 2013 */

/* TODO:
 * - test using power heuristic for combing bssrdfs
 * - try to reduce one sample model variance
 * - possible shade all hits for progressive integrator
 * - cubic and gaussian scale difference tweak
 */

#define BSSRDF_MULTI_EVAL

__device ShaderClosure *subsurface_scatter_pick_closure(KernelGlobals *kg, ShaderData *sd, float *probability)
{
	/* sum sample weights of bssrdf and bsdf */
	float bsdf_sum = 0.0f;
	float bssrdf_sum = 0.0f;

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];
		
		if(CLOSURE_IS_BSDF(sc->type))
			bsdf_sum += sc->sample_weight;
		else if(CLOSURE_IS_BSSRDF(sc->type))
			bssrdf_sum += sc->sample_weight;
	}

	/* use bsdf or bssrdf? */
	float r = sd->randb_closure*(bsdf_sum + bssrdf_sum);

	if(r < bsdf_sum) {
		/* use bsdf, and adjust randb so we can reuse it for picking a bsdf */
		sd->randb_closure = r/bsdf_sum;
		*probability = (bsdf_sum > 0.0f)? (bsdf_sum + bssrdf_sum)/bsdf_sum: 1.0f;
		return NULL;
	}

	/* use bssrdf */
	r -= bsdf_sum;

	float sum = 0.0f;

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];
		
		if(CLOSURE_IS_BSSRDF(sc->type)) {
			sum += sc->sample_weight;

			if(r <= sum) {
				sd->randb_closure = (r - (sum - sc->sample_weight))/sc->sample_weight;

#ifdef BSSRDF_MULTI_EVAL
				*probability = (bssrdf_sum > 0.0f)? (bsdf_sum + bssrdf_sum)/bssrdf_sum: 1.0f;
#else
				*probability = (bssrdf_sum > 0.0f)? (bsdf_sum + bssrdf_sum)/sc->sample_weight: 1.0f;
#endif
				return sc;
			}
		}
	}

	/* should never happen */
	sd->randb_closure = 0.0f;
	*probability = 1.0f;
	return NULL;
}

__device float3 subsurface_scatter_eval(ShaderData *sd, ShaderClosure *sc, float disk_r, float r, bool all)
{
#ifdef BSSRDF_MULTI_EVAL
	/* this is the veach one-sample model with balance heuristic, some pdf
	 * factors drop out when using balance heuristic weighting */
	float3 eval_sum = make_float3(0.0f, 0.0f, 0.0f);
	float pdf_sum = 0.0f;
	float sample_weight_sum = 0.0f;
	int num_bssrdf = 0;

	for(int i = 0; i < sd->num_closure; i++) {
		sc = &sd->closure[i];
		
		if(CLOSURE_IS_BSSRDF(sc->type)) {
			float sample_weight = (all)? 1.0f: sc->sample_weight;
			sample_weight_sum += sample_weight;
		}
	}

	float sample_weight_inv = 1.0f/sample_weight_sum;

	//printf("num closures %d\n", sd->num_closure);

	for(int i = 0; i < sd->num_closure; i++) {
		sc = &sd->closure[i];
		
		if(CLOSURE_IS_BSSRDF(sc->type)) {
			/* in case of non-progressive integrate we sample all bssrdf's once,
			 * for progressive we pick one, so adjust pdf for that */
			float sample_weight = (all)? 1.0f: sc->sample_weight * sample_weight_inv;

			/* compute pdf */
			float pdf = bssrdf_pdf(sc, r);
			float disk_pdf = bssrdf_pdf(sc, disk_r);

			/* TODO power heuristic is not working correct here */
			eval_sum += sc->weight*pdf; //*sample_weight*disk_pdf;
			pdf_sum += sample_weight*disk_pdf; //*sample_weight*disk_pdf;

			num_bssrdf++;
		}
	}

	return (pdf_sum > 0.0f)? eval_sum / pdf_sum : make_float3(0.0f, 0.0f, 0.0f);
#else
	float pdf = bssrdf_pdf(pick_sc, r);
	float disk_pdf = bssrdf_pdf(pick_sc, disk_r);

	return pick_sc->weight * pdf / disk_pdf;
#endif
}

/* replace closures with a single diffuse bsdf closure after scatter step */
__device void subsurface_scatter_setup_diffuse_bsdf(ShaderData *sd, float3 weight, bool hit, float3 N)
{
	sd->flag &= ~SD_CLOSURE_FLAGS;
	sd->randb_closure = 0.0f;

	if(hit) {
		ShaderClosure *sc = &sd->closure[0];
		sd->num_closure = 1;

		sc->weight = weight;
		sc->sample_weight = 1.0f;
		sc->data0 = 0.0f;
		sc->data1 = 0.0f;
		sc->N = N;
		sd->flag |= bsdf_diffuse_setup(sc);

		/* replace CLOSURE_BSDF_DIFFUSE_ID with this special ID so render passes
		 * can recognize it as not being a regular diffuse closure */
		sc->type = CLOSURE_BSDF_BSSRDF_ID;
	}
	else
		sd->num_closure = 0;
}

/* optionally do blurring of color and/or bump mapping, at the cost of a shader evaluation */
__device float3 subsurface_color_pow(float3 color, float exponent)
{
	color = max(color, make_float3(0.0f, 0.0f, 0.0f));

	if(exponent == 1.0f) {
		/* nothing to do */
	}
	else if(exponent == 0.5f) {
		color.x = sqrtf(color.x);
		color.y = sqrtf(color.y);
		color.z = sqrtf(color.z);
	}
	else {
		color.x = powf(color.x, exponent);
		color.y = powf(color.y, exponent);
		color.z = powf(color.z, exponent);
	}

	return color;
}

__device void subsurface_color_bump_blur(KernelGlobals *kg, ShaderData *out_sd, ShaderData *in_sd, int state_flag, float3 *eval, float3 *N)
{
	/* average color and texture blur at outgoing point */
	float texture_blur;
	float3 out_color = shader_bssrdf_sum(out_sd, NULL, &texture_blur);

	/* do we have bump mapping? */
	bool bump = (out_sd->flag & SD_HAS_BSSRDF_BUMP) != 0;

	if(bump || texture_blur > 0.0f) {
		/* average color and normal at incoming point */
		shader_eval_surface(kg, in_sd, 0.0f, state_flag, SHADER_CONTEXT_SSS);
		float3 in_color = shader_bssrdf_sum(in_sd, (bump)? N: NULL, NULL);

		/* we simply divide out the average color and multiply with the average
		 * of the other one. we could try to do this per closure but it's quite
		 * tricky to match closures between shader evaluations, their number and
		 * order may change, this is simpler */
		if(texture_blur > 0.0f) {
			out_color = subsurface_color_pow(out_color, texture_blur);
			in_color = subsurface_color_pow(in_color, texture_blur);

			*eval *= safe_divide_color(in_color, out_color);
		}
	}
}

/* subsurface scattering step, from a point on the surface to other nearby points on the same object */
__device int subsurface_scatter_multi_step(KernelGlobals *kg, ShaderData *sd, ShaderData bssrdf_sd[BSSRDF_MAX_HITS],
	int state_flag, ShaderClosure *sc, uint *lcg_state, float disk_u, float disk_v, bool all)
{
	/* pick random axis in local frame and point on disk */
	float3 disk_N, disk_T, disk_B;
	float pick_pdf_N, pick_pdf_T, pick_pdf_B;
	
	disk_N = sd->Ng;
	make_orthonormals(disk_N, &disk_T, &disk_B);

	if(disk_u < 0.5f) {
		pick_pdf_N = 0.5f;
		pick_pdf_T = 0.25f;
		pick_pdf_B = 0.25f;
		disk_u *= 2.0f;
	}
	else if(disk_u < 0.75f) {
		float3 tmp = disk_N;
		disk_N = disk_T;
		disk_T = tmp;
		pick_pdf_N = 0.25f;
		pick_pdf_T = 0.5f;
		pick_pdf_B = 0.25f;
		disk_u = (disk_u - 0.5f)*4.0f;
	}
	else {
		float3 tmp = disk_N;
		disk_N = disk_B;
		disk_B = tmp;
		pick_pdf_N = 0.25f;
		pick_pdf_T = 0.25f;
		pick_pdf_B = 0.5f;
		disk_u = (disk_u - 0.75f)*4.0f;
	}

	/* sample point on disk */
    float phi = M_2PI_F * disk_u;
    float disk_r = disk_v;
	float disk_height;

	bssrdf_sample(sc, disk_r, &disk_r, &disk_height);

	float3 disk_P = (disk_r*cosf(phi)) * disk_T + (disk_r*sinf(phi)) * disk_B;

	/* create ray */
	Ray ray;
	ray.P = sd->P + disk_N*disk_height + disk_P;
	ray.D = -disk_N;
	ray.t = 2.0f*disk_height;
	ray.dP = sd->dP;
	ray.dD = differential3_zero();
	ray.time = sd->time;

	/* intersect with the same object. if multiple intersections are found it
	 * will use at most BSSRDF_MAX_HITS hits, a random subset of all hits */
	Intersection isect[BSSRDF_MAX_HITS];
	uint num_hits = scene_intersect_subsurface(kg, &ray, isect, sd->object, lcg_state, BSSRDF_MAX_HITS);

	/* evaluate bssrdf */
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);
	int num_eval_hits = min(num_hits, BSSRDF_MAX_HITS);

	for(int hit = 0; hit < num_eval_hits; hit++) {
		ShaderData *bsd = &bssrdf_sd[hit];

		/* setup new shading point */
		*bsd = *sd;
		shader_setup_from_subsurface(kg, bsd, &isect[hit], &ray);

		/* probability densities for local frame axes */
		float pdf_N = pick_pdf_N * fabsf(dot(disk_N, bsd->Ng));
		float pdf_T = pick_pdf_T * fabsf(dot(disk_T, bsd->Ng));
		float pdf_B = pick_pdf_B * fabsf(dot(disk_B, bsd->Ng));
		
		/* multiple importance sample between 3 axes, power heuristic
		 * found to be slightly better than balance heuristic */
		float mis_weight = power_heuristic_3(pdf_N, pdf_T, pdf_B);

		/* real distance to sampled point */
		float r = len(bsd->P - sd->P);

		/* evaluate */
		float w = mis_weight / pdf_N;
		if(num_hits > BSSRDF_MAX_HITS)
			w *= num_hits/(float)BSSRDF_MAX_HITS;
		eval = subsurface_scatter_eval(bsd, sc, disk_r, r, all) * w;

		/* optionally blur colors and bump mapping */
		float3 N = bsd->N;
		subsurface_color_bump_blur(kg, sd, bsd, state_flag, &eval, &N);

		/* setup diffuse bsdf */
		subsurface_scatter_setup_diffuse_bsdf(bsd, eval, true, N);
	}

	return num_eval_hits;
}

/* subsurface scattering step, from a point on the surface to another nearby point on the same object */
__device void subsurface_scatter_step(KernelGlobals *kg, ShaderData *sd,
	int state_flag, ShaderClosure *sc, uint *lcg_state, float disk_u, float disk_v, bool all)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);
	uint num_hits = 0;

	/* pick random axis in local frame and point on disk */
	float3 disk_N, disk_T, disk_B;
	float pick_pdf_N, pick_pdf_T, pick_pdf_B;
	
	disk_N = sd->Ng;
	make_orthonormals(disk_N, &disk_T, &disk_B);

	if(disk_u < 0.5f) {
		pick_pdf_N = 0.5f;
		pick_pdf_T = 0.25f;
		pick_pdf_B = 0.25f;
		disk_u *= 2.0f;
	}
	else if(disk_u < 0.75f) {
		float3 tmp = disk_N;
		disk_N = disk_T;
		disk_T = tmp;
		pick_pdf_N = 0.25f;
		pick_pdf_T = 0.5f;
		pick_pdf_B = 0.25f;
		disk_u = (disk_u - 0.5f)*4.0f;
	}
	else {
		float3 tmp = disk_N;
		disk_N = disk_B;
		disk_B = tmp;
		pick_pdf_N = 0.25f;
		pick_pdf_T = 0.25f;
		pick_pdf_B = 0.5f;
		disk_u = (disk_u - 0.75f)*4.0f;
	}

	/* sample point on disk */
	float phi = M_2PI_F * disk_u;
	float disk_r = disk_v;
	float disk_height;

	bssrdf_sample(sc, disk_r, &disk_r, &disk_height);

	float3 disk_P = (disk_r*cosf(phi)) * disk_T + (disk_r*sinf(phi)) * disk_B;

	/* create ray */
	Ray ray;
	ray.P = sd->P + disk_N*disk_height + disk_P;
	ray.D = -disk_N;
	ray.t = 2.0f*disk_height;
	ray.dP = sd->dP;
	ray.dD = differential3_zero();
	ray.time = sd->time;

	/* intersect with the same object. if multiple intersections are
	 * found it will randomly pick one of them */
	Intersection isect;
	num_hits = scene_intersect_subsurface(kg, &ray, &isect, sd->object, lcg_state, 1);

	/* evaluate bssrdf */
	if(num_hits > 0) {
		float3 origP = sd->P;

		/* setup new shading point */
		shader_setup_from_subsurface(kg, sd, &isect, &ray);

		/* probability densities for local frame axes */
		float pdf_N = pick_pdf_N * fabsf(dot(disk_N, sd->Ng));
		float pdf_T = pick_pdf_T * fabsf(dot(disk_T, sd->Ng));
		float pdf_B = pick_pdf_B * fabsf(dot(disk_B, sd->Ng));
		
		/* multiple importance sample between 3 axes, power heuristic
		 * found to be slightly better than balance heuristic */
		float mis_weight = power_heuristic_3(pdf_N, pdf_T, pdf_B);

		/* real distance to sampled point */
		float r = len(sd->P - origP);

		/* evaluate */
		float w = (mis_weight * num_hits) / pdf_N;
		eval = subsurface_scatter_eval(sd, sc, disk_r, r, all) * w;
	}

	/* optionally blur colors and bump mapping */
	float3 N = sd->N;
	subsurface_color_bump_blur(kg, sd, sd, state_flag, &eval, &N);

	/* setup diffuse bsdf */
	subsurface_scatter_setup_diffuse_bsdf(sd, eval, (num_hits > 0), N);
}


/* OLD BSSRDF */

__device float old_bssrdf_sample_distance(KernelGlobals *kg, float radius, float refl, float u)
{
	int table_offset = kernel_data.bssrdf.table_offset;
	float r = lookup_table_read_2D(kg, u, refl, table_offset, BSSRDF_RADIUS_TABLE_SIZE, BSSRDF_REFL_TABLE_SIZE);

	return r*radius;
}

#ifdef BSSRDF_MULTI_EVAL
__device float old_bssrdf_pdf(KernelGlobals *kg, float radius, float refl, float r)
{
	if(r >= radius)
		return 0.0f;

	/* todo: when we use the real BSSRDF this will need to be divided by the maximum
	 * radius instead of the average radius */
	float t = r/radius;

	int table_offset = kernel_data.bssrdf.table_offset + BSSRDF_PDF_TABLE_OFFSET;
	float pdf = lookup_table_read_2D(kg, t, refl, table_offset, BSSRDF_RADIUS_TABLE_SIZE, BSSRDF_REFL_TABLE_SIZE);

	pdf /= radius;

	return pdf;
}
#endif

#ifdef BSSRDF_MULTI_EVAL
__device float3 old_subsurface_scatter_multi_eval(KernelGlobals *kg, ShaderData *sd, bool hit, float refl, float *r, int num_r, bool all)
{
	/* compute pdf */
	float3 eval_sum = make_float3(0.0f, 0.0f, 0.0f);
	float pdf_sum = 0.0f;
	float sample_weight_sum = 0.0f;
	int num_bssrdf = 0;

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];
		
		if(CLOSURE_IS_BSSRDF(sc->type)) {
			float sample_weight = (all)? 1.0f: sc->sample_weight;

			/* compute pdf */
			float pdf = 1.0f;
			for(int i = 0; i < num_r; i++)
				pdf *= old_bssrdf_pdf(kg, sc->data0, refl, r[i]);

			eval_sum += sc->weight*pdf;
			pdf_sum += sample_weight*pdf;

			sample_weight_sum += sample_weight;
			num_bssrdf++;
		}
	}

	float inv_pdf_sum;
	
	if(pdf_sum > 0.0f) {
		/* in case of non-progressive integrate we sample all bssrdf's once,
		 * for progressive we pick one, so adjust pdf for that */
		if(all)
			inv_pdf_sum = 1.0f/pdf_sum;
		else
			inv_pdf_sum = sample_weight_sum/pdf_sum;
	}
	else
		inv_pdf_sum = 0.0f;

	float3 weight = eval_sum * inv_pdf_sum;

	return weight;
}
#endif

/* subsurface scattering step, from a point on the surface to another nearby point on the same object */
__device void old_subsurface_scatter_step(KernelGlobals *kg, ShaderData *sd, int state_flag, ShaderClosure *sc, uint *lcg_state, bool all)
{
	float radius = sc->data0;
	float refl = max(average(sc->weight)*3.0f, 0.0f);
	float r = 0.0f;
	bool hit = false;
	float3 weight = make_float3(1.0f, 1.0f, 1.0f);
#ifdef BSSRDF_MULTI_EVAL
	float r_attempts[BSSRDF_MAX_ATTEMPTS];
#endif
	int num_attempts;

	/* attempt to find a hit a given number of times before giving up */
	for(num_attempts = 0; num_attempts < kernel_data.bssrdf.num_attempts; num_attempts++) {
		/* random numbers for sampling */
		float u1 = lcg_step_float(lcg_state);
		float u2 = lcg_step_float(lcg_state);
		float u3 = lcg_step_float(lcg_state);
		float u4 = lcg_step_float(lcg_state);
		float u5 = lcg_step_float(lcg_state);

		r = old_bssrdf_sample_distance(kg, radius, refl, u5);
#ifdef BSSRDF_MULTI_EVAL
		r_attempts[num_attempts] = r;
#endif

		float3 p1 = sd->P + sample_uniform_sphere(u1, u2)*r;
		float3 p2 = sd->P + sample_uniform_sphere(u3, u4)*r;

		/* create ray */
		Ray ray;
		ray.P = p1;
		ray.D = normalize_len(p2 - p1, &ray.t);
		ray.dP = sd->dP;
		ray.dD = differential3_zero();
		ray.time = sd->time;

		/* intersect with the same object. if multiple intersections are
		 * found it will randomly pick one of them */
		Intersection isect;
		if(scene_intersect_subsurface(kg, &ray, &isect, sd->object, lcg_state, 1) == 0)
			continue;

		/* setup new shading point */
		shader_setup_from_subsurface(kg, sd, &isect, &ray);

		hit = true;
		num_attempts++;
		break;
	}

	/* evaluate subsurface scattering closures */
#ifdef BSSRDF_MULTI_EVAL
	weight *= old_subsurface_scatter_multi_eval(kg, sd, hit, refl, r_attempts, num_attempts, all);
#else
	weight *= sc->weight;
#endif

	if(!hit)
		weight = make_float3(0.0f, 0.0f, 0.0f);

	/* optionally blur colors and bump mapping */
	float3 N = sd->N;
	subsurface_color_bump_blur(kg, sd, sd, state_flag, &weight, &N);

	/* replace closures with a single diffuse BSDF */
	subsurface_scatter_setup_diffuse_bsdf(sd, weight, hit, N);
}

__device bool old_subsurface_scatter_use(ShaderData *sd)
{
	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];
		
		if(sc->type == CLOSURE_BSSRDF_COMPATIBLE_ID)
			return true;
	}

	return false;
}

CCL_NAMESPACE_END


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

#define BSSRDF_MULTI_EVAL
#define BSSRDF_SKIP_NO_HIT

__device float bssrdf_sample_distance(KernelGlobals *kg, float radius, float refl, float u)
{
	int table_offset = kernel_data.bssrdf.table_offset;
	float r = lookup_table_read_2D(kg, u, refl, table_offset, BSSRDF_RADIUS_TABLE_SIZE, BSSRDF_REFL_TABLE_SIZE);

	return r*radius;
}

#ifdef BSSRDF_MULTI_EVAL
__device float bssrdf_pdf(KernelGlobals *kg, float radius, float refl, float r)
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

	/* pick a random bsdf or bssrdf */
	float r = sd->randb_closure*(bsdf_sum + bssrdf_sum);
	float sum = 0.0f;

	int sampled;

	for(sampled = 0; sampled < sd->num_closure; sampled++) {
		ShaderClosure *sc = &sd->closure[sampled];
		
		if(CLOSURE_IS_BSDF(sc->type) || CLOSURE_IS_BSSRDF(sc->type)) {
			sum += sc->sample_weight;

			if(r <= sum) {
				/* if we picked a bssrdf, return the closure. also return probability
				 * to adjust throughput depending if we picked a bsdf or bssrdf */
				if(CLOSURE_IS_BSSRDF(sc->type)) {
					sd->randb_closure = 0.0f; /* not needed anymore */
#ifdef BSSRDF_MULTI_EVAL
					*probability = (bssrdf_sum > 0.0f)? (bsdf_sum + bssrdf_sum)/bssrdf_sum: 1.0f;
#else
					*probability = (bssrdf_sum > 0.0f)? (bsdf_sum + bssrdf_sum)/sc->sample_weight: 1.0f;
#endif
					return sc;
				}
				else {
					/* reuse randb for picking a bsdf */
					sd->randb_closure = (sd->randb_closure - sum - sc->sample_weight)/sc->sample_weight;
					*probability = (bsdf_sum > 0.0f)? (bsdf_sum + bssrdf_sum)/bsdf_sum: 1.0f;
					return NULL;
				}
			}
		}
	}

	*probability = 1.0f;
	return NULL;
}

#ifdef BSSRDF_MULTI_EVAL
__device float3 subsurface_scatter_multi_eval(KernelGlobals *kg, ShaderData *sd, bool hit, float refl, float *r, int num_r)
{
	/* compute pdf */
	float3 eval_sum = make_float3(0.0f, 0.0f, 0.0f);
	float pdf_sum = 0.0f;
	float sample_weight_sum = 0.0f;

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];
		
		if(CLOSURE_IS_BSSRDF(sc->type)) {
			float pdf = 1.0f;
			for(int i = 0; i < num_r; i++)
				pdf *= bssrdf_pdf(kg, sc->data0, refl, r[i]);
			//float pdf = bssrdf_pdf(kg, sc->data0, refl, r[num_r-1]);

			eval_sum += sc->weight*pdf;
			pdf_sum += sc->sample_weight*pdf;

			sample_weight_sum += sc->sample_weight;
		}
	}

	float inv_pdf_sum = (pdf_sum > 0.0f)? sample_weight_sum/pdf_sum: 0.0f;
	float3 weight = eval_sum * inv_pdf_sum;

	return weight;
}
#endif

/* replace closures with a single diffuse bsdf closure after scatter step */
__device void subsurface_scatter_setup_diffuse_bsdf(ShaderData *sd, float3 weight)
{
	ShaderClosure *sc = &sd->closure[0];
	sd->num_closure = 1;

	sc->weight = weight;
	sc->sample_weight = 1.0f;
	sc->data0 = 0.0f;
	sc->data1 = 0.0f;
	sc->N = sd->N;
	sd->flag |= bsdf_diffuse_setup(sc);
	sd->randb_closure = 0.0f;

	/* todo: evaluate shading to get blurred textures and bump mapping */
	/* shader_eval_surface(kg, sd, 0.0f, state_flag, SHADER_CONTEXT_SSS); */
}

/* subsurface scattering step, from a point on the surface to another nearby point on the same object */
__device void subsurface_scatter_step(KernelGlobals *kg, ShaderData *sd, int state_flag, ShaderClosure *sc, uint *lcg_state)
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
		float u1 = lcg_step(lcg_state);
		float u2 = lcg_step(lcg_state);
		float u3 = lcg_step(lcg_state);
		float u4 = lcg_step(lcg_state);
		float u5 = lcg_step(lcg_state);
		float u6 = lcg_step(lcg_state);

		r = bssrdf_sample_distance(kg, radius, refl, u5);
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
		ray.dD.dx = make_float3(0.0f, 0.0f, 0.0f);
		ray.dD.dy = make_float3(0.0f, 0.0f, 0.0f);
		ray.time = sd->time;

		/* intersect with the same object. if multiple intersections are
		 * found it will randomly pick one of them */
		Intersection isect;
		if(scene_intersect_subsurface(kg, &ray, &isect, sd->object, u6) == 0)
			continue;

		/* setup new shading point */
		shader_setup_from_subsurface(kg, sd, &isect, &ray);

		hit = true;
		num_attempts++;
		break;
	}

	/* evaluate subsurface scattering closures */
#ifdef BSSRDF_MULTI_EVAL
	weight *= subsurface_scatter_multi_eval(kg, sd, hit, refl, r_attempts, num_attempts);
#else
	weight *= sc->weight;
#endif

#ifdef BSSRDF_SKIP_NO_HIT
	if(!hit)
		weight = make_float3(0.0f, 0.0f, 0.0f);
#endif

	/* replace closures with a single diffuse BSDF */
	subsurface_scatter_setup_diffuse_bsdf(sd, weight);
}

CCL_NAMESPACE_END


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

#include "kernel_differential.h"
#include "kernel_montecarlo.h"
#include "kernel_triangle.h"
#include "kernel_object.h"
#ifdef __QBVH__
#include "kernel_qbvh.h"
#else
#include "kernel_bvh.h"
#endif
#include "kernel_accumulate.h"
#include "kernel_camera.h"
#include "kernel_shader.h"
#include "kernel_light.h"
#include "kernel_emission.h"
#include "kernel_random.h"
#include "kernel_passes.h"

CCL_NAMESPACE_BEGIN

typedef struct PathState {
	uint flag;
	int bounce;

	int diffuse_bounce;
	int glossy_bounce;
	int transmission_bounce;
	int transparent_bounce;
} PathState;

__device_inline void path_state_init(PathState *state)
{
	state->flag = PATH_RAY_CAMERA|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP;
	state->bounce = 0;
	state->diffuse_bounce = 0;
	state->glossy_bounce = 0;
	state->transmission_bounce = 0;
	state->transparent_bounce = 0;
}

__device_inline void path_state_next(KernelGlobals *kg, PathState *state, int label)
{
	/* ray through transparent keeps same flags from previous ray and is
	   not counted as a regular bounce, transparent has separate max */
	if(label & LABEL_TRANSPARENT) {
		state->flag |= PATH_RAY_TRANSPARENT;
		state->transparent_bounce++;

		if(!kernel_data.integrator.transparent_shadows)
			state->flag |= PATH_RAY_MIS_SKIP;

		return;
	}

	state->bounce++;

	/* reflection/transmission */
	if(label & LABEL_REFLECT) {
		state->flag |= PATH_RAY_REFLECT;
		state->flag &= ~(PATH_RAY_TRANSMIT|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT);

		if(label & LABEL_DIFFUSE)
			state->diffuse_bounce++;
		else
			state->glossy_bounce++;
	}
	else {
		kernel_assert(label & LABEL_TRANSMIT);

		state->flag |= PATH_RAY_TRANSMIT;
		state->flag &= ~(PATH_RAY_REFLECT|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT);

		state->transmission_bounce++;
	}

	/* diffuse/glossy/singular */
	if(label & LABEL_DIFFUSE) {
		state->flag |= PATH_RAY_DIFFUSE;
		state->flag &= ~(PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);
	}
	else if(label & LABEL_GLOSSY) {
		state->flag |= PATH_RAY_GLOSSY;
		state->flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);
	}
	else {
		kernel_assert(label & LABEL_SINGULAR);

		state->flag |= PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP;
		state->flag &= ~PATH_RAY_DIFFUSE;
	}
}

__device_inline uint path_state_ray_visibility(KernelGlobals *kg, PathState *state)
{
	uint flag = state->flag;

	/* for visibility, diffuse/glossy are for reflection only */
	if(flag & PATH_RAY_TRANSMIT)
		flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_GLOSSY);
	/* for camera visibility, use render layer flags */
	if(flag & PATH_RAY_CAMERA)
		flag |= kernel_data.integrator.layer_flag;

	return flag;
}

__device_inline float path_state_terminate_probability(KernelGlobals *kg, PathState *state, const float3 throughput)
{
	if(state->flag & PATH_RAY_TRANSPARENT) {
		/* transparent rays treated separately */
		if(state->transparent_bounce >= kernel_data.integrator.transparent_max_bounce)
			return 0.0f;
		else if(state->transparent_bounce <= kernel_data.integrator.transparent_min_bounce)
			return 1.0f;
	}
	else {
		/* other rays */
		if((state->bounce >= kernel_data.integrator.max_bounce) ||
		   (state->diffuse_bounce >= kernel_data.integrator.max_diffuse_bounce) ||
		   (state->glossy_bounce >= kernel_data.integrator.max_glossy_bounce) ||
		   (state->transmission_bounce >= kernel_data.integrator.max_transmission_bounce))
			return 0.0f;
		else if(state->bounce <= kernel_data.integrator.min_bounce)
			return 1.0f;
	}

	/* probalistic termination */
	return average(throughput);
}

__device_inline bool shadow_blocked(KernelGlobals *kg, PathState *state, Ray *ray, float3 *shadow)
{
	*shadow = make_float3(1.0f, 1.0f, 1.0f);

	if(ray->t == 0.0f)
		return false;
	
	Intersection isect;
	bool result = scene_intersect(kg, ray, PATH_RAY_SHADOW_OPAQUE, &isect);

#ifdef __TRANSPARENT_SHADOWS__
	if(result && kernel_data.integrator.transparent_shadows) {
		/* transparent shadows work in such a way to try to minimize overhead
		   in cases where we don't need them. after a regular shadow ray is
		   cast we check if the hit primitive was potentially transparent, and
		   only in that case start marching. this gives on extra ray cast for
		   the cases were we do want transparency.
		   
		   also note that for this to work correct, multi close sampling must
		   be used, since we don't pass a random number to shader_eval_surface */
		if(shader_transparent_shadow(kg, &isect)) {
			float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
			float3 Pend = ray->P + ray->D*ray->t;
			int bounce = state->transparent_bounce;

			for(;;) {
				if(bounce >= kernel_data.integrator.transparent_max_bounce) {
					return true;
				}
				else if(bounce >= kernel_data.integrator.transparent_min_bounce) {
					/* todo: get random number somewhere for probabilistic terminate */
#if 0
					float probability = average(throughput);
					float terminate = 0.0f;

					if(terminate >= probability)
						return true;

					throughput /= probability;
#endif
				}

				if(!scene_intersect(kg, ray, PATH_RAY_SHADOW_TRANSPARENT, &isect)) {
					*shadow *= throughput;
					return false;
				}

				if(!shader_transparent_shadow(kg, &isect))
					return true;

				ShaderData sd;
				shader_setup_from_ray(kg, &sd, &isect, ray);
				shader_eval_surface(kg, &sd, 0.0f, PATH_RAY_SHADOW);

				throughput *= shader_bsdf_transparency(kg, &sd);

				ray->P = ray_offset(sd.P, -sd.Ng);
				if(ray->t != FLT_MAX)
					ray->D = normalize_len(Pend - ray->P, &ray->t);

				bounce++;
			}
		}
	}
#endif

	return result;
}

__device float4 kernel_path_integrate(KernelGlobals *kg, RNG *rng, int sample, Ray ray, __global float *buffer)
{
	/* initialize */
	PathRadiance L;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float L_transparent = 0.0f;

	path_radiance_init(&L, kernel_data.film.use_light_pass);

	float ray_pdf = 0.0f;
	PathState state;
	int rng_offset = PRNG_BASE_NUM;

	path_state_init(&state);

	/* path iteration */
	for(;; rng_offset += PRNG_BOUNCE_NUM) {
		/* intersect scene */
		Intersection isect;
		uint visibility = path_state_ray_visibility(kg, &state);

		if(!scene_intersect(kg, &ray, visibility, &isect)) {
			/* eval background shader if nothing hit */
			if(kernel_data.background.transparent && (state.flag & PATH_RAY_CAMERA)) {
				L_transparent += average(throughput);
			}
#ifdef __BACKGROUND__
			else {
				/* sample background shader */
				float3 L_background = indirect_background(kg, &ray, state.flag, ray_pdf);
				path_radiance_accum_background(&L, throughput, L_background, state.bounce);
			}
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray);
		float rbsdf = path_rng(kg, rng, sample, rng_offset + PRNG_BSDF);
		shader_eval_surface(kg, &sd, rbsdf, state.flag);

		kernel_write_data_passes(kg, buffer, &L, &sd, sample, state.flag, throughput);

#ifdef __HOLDOUT__
		if((sd.flag & SD_HOLDOUT) && (state.flag & PATH_RAY_CAMERA)) {
			float3 holdout_weight = shader_holdout_eval(kg, &sd);

			if(kernel_data.background.transparent)
				/* any throughput is ok, should all be identical here */
				L_transparent += average(holdout_weight*throughput);
		}
#endif

#ifdef __EMISSION__
		/* emission */
		if(sd.flag & SD_EMISSION) {
			float3 emission = indirect_emission(kg, &sd, isect.t, state.flag, ray_pdf);
			path_radiance_accum_emission(&L, throughput, emission, state.bounce);
		}
#endif

		/* path termination. this is a strange place to put the termination, it's
		   mainly due to the mixed in MIS that we use. gives too many unneeded
		   shader evaluations, only need emission if we are going to terminate */
		float probability = path_state_terminate_probability(kg, &state, throughput);
		float terminate = path_rng(kg, rng, sample, rng_offset + PRNG_TERMINATE);

		if(terminate >= probability)
			break;

		throughput /= probability;

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion) {
			/* todo: solve correlation */
			float bsdf_u = path_rng(kg, rng, sample, rng_offset + PRNG_BSDF_U);
			float bsdf_v = path_rng(kg, rng, sample, rng_offset + PRNG_BSDF_V);

			float3 ao_D;
			float ao_pdf;

			sample_cos_hemisphere(sd.N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

			if(dot(sd.Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
				Ray light_ray;
				float3 ao_shadow;

				light_ray.P = ray_offset(sd.P, sd.Ng);
				light_ray.D = ao_D;
				light_ray.t = kernel_data.background.ao_distance;

				if(!shadow_blocked(kg, &state, &light_ray, &ao_shadow)) {
					float3 ao_bsdf = shader_bsdf_diffuse(kg, &sd)*kernel_data.background.ao_factor;
					path_radiance_accum_ao(&L, throughput, ao_bsdf, ao_shadow, state.bounce);
				}
			}
		}
#endif

#ifdef __EMISSION__
		if(kernel_data.integrator.use_direct_light) {
			/* sample illumination from lights to find path contribution */
			if(sd.flag & SD_BSDF_HAS_EVAL) {
				float light_t = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT);
				float light_o = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT_F);
				float light_u = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT_U);
				float light_v = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT_V);

				Ray light_ray;
				BsdfEval L_light;
				bool is_lamp;

#ifdef __MULTI_LIGHT__
				/* index -1 means randomly sample from distribution */
				int i = (kernel_data.integrator.num_distribution)? -1: 0;

				for(; i < kernel_data.integrator.num_all_lights; i++) {
#else
				const int i = -1;
#endif
					if(direct_emission(kg, &sd, i, light_t, light_o, light_u, light_v, &light_ray, &L_light, &is_lamp)) {
						/* trace shadow ray */
						float3 shadow;

						if(!shadow_blocked(kg, &state, &light_ray, &shadow)) {
							/* accumulate */
							path_radiance_accum_light(&L, throughput, &L_light, shadow, state.bounce, is_lamp);
						}
					}
#ifdef __MULTI_LIGHT__
				}
#endif
			}
		}
#endif

		/* no BSDF? we can stop here */
		if(!(sd.flag & SD_BSDF))
			break;

		/* sample BSDF */
		float bsdf_pdf;
		BsdfEval bsdf_eval;
		float3 bsdf_omega_in;
		differential3 bsdf_domega_in;
		float bsdf_u = path_rng(kg, rng, sample, rng_offset + PRNG_BSDF_U);
		float bsdf_v = path_rng(kg, rng, sample, rng_offset + PRNG_BSDF_V);
		int label;

		label = shader_bsdf_sample(kg, &sd, bsdf_u, bsdf_v, &bsdf_eval,
			&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

		shader_release(kg, &sd);

		if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
			break;

		/* modify throughput */
		path_radiance_bsdf_bounce(&L, &throughput, &bsdf_eval, bsdf_pdf, state.bounce, label);

		/* set labels */
		if(!(label & LABEL_TRANSPARENT))
			ray_pdf = bsdf_pdf;

		/* update path state */
		path_state_next(kg, &state, label);

		/* setup ray */
		ray.P = ray_offset(sd.P, (label & LABEL_TRANSMIT)? -sd.Ng: sd.Ng);
		ray.D = bsdf_omega_in;
		ray.t = FLT_MAX;
#ifdef __RAY_DIFFERENTIALS__
		ray.dP = sd.dP;
		ray.dD = bsdf_domega_in;
#endif
	}

	float3 L_sum = path_radiance_sum(&L);

#ifdef __CLAMP_SAMPLE__
	path_radiance_clamp(&L, &L_sum, kernel_data.integrator.sample_clamp);
#endif

	kernel_write_light_passes(kg, buffer, &L, sample);

	return make_float4(L_sum.x, L_sum.y, L_sum.z, 1.0f - L_transparent);
}

__device void kernel_path_trace(KernelGlobals *kg,
	__global float *buffer, __global uint *rng_state,
	int sample, int x, int y, int offset, int stride)
{
	/* buffer offset */
	int index = offset + x + y*stride;
	int pass_stride = kernel_data.film.pass_stride;

	rng_state += index;
	buffer += index*pass_stride;

	/* initialize random numbers */
	RNG rng;

	float filter_u;
	float filter_v;

	path_rng_init(kg, rng_state, sample, &rng, x, y, &filter_u, &filter_v);

	/* sample camera ray */
	Ray ray;

	float lens_u = path_rng(kg, &rng, sample, PRNG_LENS_U);
	float lens_v = path_rng(kg, &rng, sample, PRNG_LENS_V);

	camera_sample(kg, x, y, filter_u, filter_v, lens_u, lens_v, &ray);

	/* integrate */
	float4 L = kernel_path_integrate(kg, &rng, sample, ray, buffer);

	/* accumulate result in output buffer */
	kernel_write_pass_float4(buffer, sample, L);

	path_rng_end(kg, rng_state, rng);
}

CCL_NAMESPACE_END


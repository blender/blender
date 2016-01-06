/*
 * Copyright 2011-2015 Blender Foundation
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

#include "kernel_split_common.h"

/* Note on kernel_lamp_emission
 * This is the 3rd kernel in the ray-tracing logic. This is the second of the
 * path-iteration kernels. This kernel takes care of the indirect lamp emission logic.
 * This kernel operates on QUEUE_ACTIVE_AND_REGENERATED_RAYS. It processes rays of state RAY_ACTIVE
 * and RAY_HIT_BACKGROUND.
 * We will empty QUEUE_ACTIVE_AND_REGENERATED_RAYS queue in this kernel.
 * The input/output of the kernel is as follows,
 * Throughput_coop ------------------------------------|--- kernel_lamp_emission --|--- PathRadiance_coop
 * Ray_coop -------------------------------------------|                           |--- Queue_data(QUEUE_ACTIVE_AND_REGENERATED_RAYS)
 * PathState_coop -------------------------------------|                           |--- Queue_index(QUEUE_ACTIVE_AND_REGENERATED_RAYS)
 * kg (globals) ---------------------------------------|                           |
 * Intersection_coop ----------------------------------|                           |
 * ray_state ------------------------------------------|                           |
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATED_RAYS) -----|                           |
 * Queue_index (QUEUE_ACTIVE_AND_REGENERATED_RAYS) ----|                           |
 * queuesize ------------------------------------------|                           |
 * use_queues_flag ------------------------------------|                           |
 * sw -------------------------------------------------|                           |
 * sh -------------------------------------------------|                           |
 * parallel_samples -----------------------------------|                           |
 *
 * note : sd is neither input nor output. Its just filled and consumed in the same, kernel_lamp_emission, kernel.
 */
ccl_device void kernel_lamp_emission(
        KernelGlobals *kg,
        ShaderData *sd,                        /* Required for lamp emission */
        ccl_global float3 *throughput_coop,    /* Required for lamp emission */
        PathRadiance *PathRadiance_coop,       /* Required for lamp emission */
        ccl_global Ray *Ray_coop,              /* Required for lamp emission */
        ccl_global PathState *PathState_coop,  /* Required for lamp emission */
        Intersection *Intersection_coop,       /* Required for lamp emission */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        int sw, int sh,
        ccl_global char *use_queues_flag,      /* Used to decide if this kernel should use
                                                * queues to fetch ray index
                                                */
        int parallel_samples,                  /* Number of samples to be processed in parallel */
        int ray_index)
{
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE) ||
	   IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND))
	{
		PathRadiance *L = &PathRadiance_coop[ray_index];
		ccl_global PathState *state = &PathState_coop[ray_index];

		float3 throughput = throughput_coop[ray_index];
		Ray ray = Ray_coop[ray_index];

#ifdef __LAMP_MIS__
		if(kernel_data.integrator.use_lamp_mis && !(state->flag & PATH_RAY_CAMERA)) {
			/* ray starting from previous non-transparent bounce */
			Ray light_ray;

			light_ray.P = ray.P - state->ray_t*ray.D;
			state->ray_t += Intersection_coop[ray_index].t;
			light_ray.D = ray.D;
			light_ray.t = state->ray_t;
			light_ray.time = ray.time;
			light_ray.dD = ray.dD;
			light_ray.dP = ray.dP;
			/* intersect with lamp */
			float3 emission;

			if(indirect_lamp_emission(kg, state, &light_ray, &emission, sd)) {
				path_radiance_accum_emission(L, throughput, emission, state->bounce);
			}
		}
#endif  /* __LAMP_MIS__ */

		/* __VOLUME__ feature is disabled */
#if 0
#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state->volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			bool heterogeneous = volume_stack_is_heterogeneous(kg, state->volume_stack);

#ifdef __VOLUME_DECOUPLED__
			int sampling_method = volume_stack_sampling_method(kg, state->volume_stack);
			bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, true, sampling_method);

			if(decoupled) {
				/* cache steps along volume for repeated sampling */
				VolumeSegment volume_segment;
				ShaderData volume_sd;

				shader_setup_from_volume(kg, &volume_sd, &volume_ray);
				kernel_volume_decoupled_record(kg, state,
					&volume_ray, &volume_sd, &volume_segment, heterogeneous);

				volume_segment.sampling_method = sampling_method;

				/* emission */
				if(volume_segment.closure_flag & SD_EMISSION)
					path_radiance_accum_emission(&L, throughput, volume_segment.accum_emission, state->bounce);

				/* scattering */
				VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

				if(volume_segment.closure_flag & SD_SCATTER) {
					bool all = false;

					/* direct light sampling */
					kernel_branched_path_volume_connect_light(kg, rng, &volume_sd,
						throughput, state, &L, 1.0f, all, &volume_ray, &volume_segment);

					/* indirect sample. if we use distance sampling and take just
					 * one sample for direct and indirect light, we could share
					 * this computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, rng, state, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, rng, state, PRNG_SCATTER_DISTANCE);

					result = kernel_volume_decoupled_scatter(kg,
						state, &volume_ray, &volume_sd, &throughput,
						rphase, rscatter, &volume_segment, NULL, true);
				}

				if(result != VOLUME_PATH_SCATTERED)
					throughput *= volume_segment.accum_transmittance;

				/* free cached steps */
				kernel_volume_decoupled_free(kg, &volume_segment);

				if(result == VOLUME_PATH_SCATTERED) {
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, state, &L, &ray))
						continue;
					else
						break;
				}
			}
			else
#endif  /* __VOLUME_DECOUPLED__ */
			{
				/* integrate along volume segment with distance sampling */
				ShaderData volume_sd;
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, state, &volume_sd, &volume_ray, &L, &throughput, rng, heterogeneous);

#ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg, rng, &volume_sd, throughput, state, &L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, state, &L, &ray))
						continue;
					else
						break;
				}
#endif  /* __VOLUME_SCATTER__ */
			}
		}
#endif  /* __VOLUME__ */
#endif
	}
}

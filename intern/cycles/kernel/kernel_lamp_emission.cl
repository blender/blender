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

#include "kernel_split.h"

/*
 * Note on kernel_ocl_path_trace_lamp_emission
 * This is the 3rd kernel in the ray-tracing logic. This is the second of the
 * path-iteration kernels. This kernel takes care of the indirect lamp emission logic.
 * This kernel operates on QUEUE_ACTIVE_AND_REGENERATED_RAYS. It processes rays of state RAY_ACTIVE
 * and RAY_HIT_BACKGROUND.
 * We will empty QUEUE_ACTIVE_AND_REGENERATED_RAYS queue in this kernel.
 * The input/output of the kernel is as follows,
 * Throughput_coop ------------------------------------|--- kernel_ocl_path_trace_lamp_emission --|--- PathRadiance_coop
 * Ray_coop -------------------------------------------|                                          |--- Queue_data(QUEUE_ACTIVE_AND_REGENERATED_RAYS)
 * PathState_coop -------------------------------------|                                          |--- Queue_index(QUEUE_ACTIVE_AND_REGENERATED_RAYS)
 * kg (globals + data) --------------------------------|                                          |
 * Intersection_coop ----------------------------------|                                          |
 * ray_state ------------------------------------------|                                          |
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATED_RAYS) -----|                                          |
 * Queue_index (QUEUE_ACTIVE_AND_REGENERATED_RAYS) ----|                                          |
 * queuesize ------------------------------------------|                                          |
 * use_queues_flag ------------------------------------|                                          |
 * sw -------------------------------------------------|                                          |
 * sh -------------------------------------------------|                                          |
 * parallel_samples -----------------------------------|                                          |
 *
 * note : shader_data is neither input nor output. Its just filled and consumed in the same, kernel_ocl_path_trace_lamp_emission, kernel.
 */
__kernel void kernel_ocl_path_trace_lamp_emission(
	ccl_global char *globals,
	ccl_constant KernelData *data,
	ccl_global char *shader_data,               /* Required for lamp emission */
	ccl_global float3 *throughput_coop,         /* Required for lamp emission */
	PathRadiance *PathRadiance_coop, /* Required for lamp emission */
	ccl_global Ray *Ray_coop,                   /* Required for lamp emission */
	ccl_global PathState *PathState_coop,       /* Required for lamp emission */
	Intersection *Intersection_coop, /* Required for lamp emission */
	ccl_global char *ray_state,                 /* Denotes the state of each ray */
	int sw, int sh,
	ccl_global int *Queue_data,                 /* Memory for queues */
	ccl_global int *Queue_index,                /* Tracks the number of elements in queues */
	int queuesize,                              /* Size (capacity) of queues */
	ccl_global char *use_queues_flag,           /* used to decide if this kernel should use queues to fetch ray index */
	int parallel_samples                        /* Number of samples to be processed in parallel */
	)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	/* We will empty this queue in this kernel */
	if(get_global_id(0) == 0 && get_global_id(1) == 0) {
		Queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
	}

	/* Fetch use_queues_flag */
	ccl_local char local_use_queues_flag;
	if(get_local_id(0) == 0 && get_local_id(1) == 0) {
		local_use_queues_flag = use_queues_flag[0];
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	int ray_index;
	if(local_use_queues_flag) {
		int thread_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
		ray_index = get_ray_index(thread_index, QUEUE_ACTIVE_AND_REGENERATED_RAYS, Queue_data, queuesize, 1);

		if(ray_index == QUEUE_EMPTY_SLOT) {
			return;
		}
	} else {
		if(x < (sw * parallel_samples) && y < sh){
			ray_index = x + y * (sw * parallel_samples);
		} else {
			return;
		}
	}

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE) || IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND)) {
		KernelGlobals *kg = (KernelGlobals *)globals;
		ShaderData *sd = (ShaderData *)shader_data;
		PathRadiance *L = &PathRadiance_coop[ray_index];

		float3 throughput = throughput_coop[ray_index];
		Ray ray = Ray_coop[ray_index];
		PathState state = PathState_coop[ray_index];

#ifdef __LAMP_MIS__
		if(kernel_data.integrator.use_lamp_mis && !(state.flag & PATH_RAY_CAMERA)) {
			/* ray starting from previous non-transparent bounce */
			Ray light_ray;

			light_ray.P = ray.P - state.ray_t*ray.D;
			state.ray_t += Intersection_coop[ray_index].t;
			light_ray.D = ray.D;
			light_ray.t = state.ray_t;
			light_ray.time = ray.time;
			light_ray.dD = ray.dD;
			light_ray.dP = ray.dP;
			/* intersect with lamp */
			float3 emission;

			if(indirect_lamp_emission(kg, &state, &light_ray, &emission, sd)) {
				path_radiance_accum_emission(L, throughput, emission, state.bounce);
			}
		}
#endif
		/* __VOLUME__ feature is disabled */
#if 0
#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			bool heterogeneous = volume_stack_is_heterogeneous(kg, state.volume_stack);

#ifdef __VOLUME_DECOUPLED__
			int sampling_method = volume_stack_sampling_method(kg, state.volume_stack);
			bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, true, sampling_method);

			if(decoupled) {
				/* cache steps along volume for repeated sampling */
				VolumeSegment volume_segment;
				ShaderData volume_sd;

				shader_setup_from_volume(kg, &volume_sd, &volume_ray, state.bounce, state.transparent_bounce);
				kernel_volume_decoupled_record(kg, &state,
					&volume_ray, &volume_sd, &volume_segment, heterogeneous);

				volume_segment.sampling_method = sampling_method;

				/* emission */
				if(volume_segment.closure_flag & SD_EMISSION)
					path_radiance_accum_emission(&L, throughput, volume_segment.accum_emission, state.bounce);

				/* scattering */
				VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

				if(volume_segment.closure_flag & SD_SCATTER) {
					bool all = false;

					/* direct light sampling */
					kernel_branched_path_volume_connect_light(kg, rng, &volume_sd,
						throughput, &state, &L, 1.0f, all, &volume_ray, &volume_segment);

					/* indirect sample. if we use distance sampling and take just
					 * one sample for direct and indirect light, we could share
					 * this computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_SCATTER_DISTANCE);

					result = kernel_volume_decoupled_scatter(kg,
						&state, &volume_ray, &volume_sd, &throughput,
						rphase, rscatter, &volume_segment, NULL, true);
				}

				if(result != VOLUME_PATH_SCATTERED)
					throughput *= volume_segment.accum_transmittance;

				/* free cached steps */
				kernel_volume_decoupled_free(kg, &volume_segment);

				if(result == VOLUME_PATH_SCATTERED) {
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, &state, &L, &ray))
						continue;
					else
						break;
				}
			}
			else
#endif
			{
				/* integrate along volume segment with distance sampling */
				ShaderData volume_sd;
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, &state, &volume_sd, &volume_ray, &L, &throughput, rng, heterogeneous);

#ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg, rng, &volume_sd, throughput, &state, &L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, &state, &L, &ray))
						continue;
					else
						break;
				}
#endif
			}
		}
#endif
#endif
	}
}

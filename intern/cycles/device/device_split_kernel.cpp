/*
 * Copyright 2011-2016 Blender Foundation
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

#include "device_split_kernel.h"

#include "kernel_types.h"
#include "kernel_split_data.h"

#include "util_time.h"

CCL_NAMESPACE_BEGIN

static const double alpha = 0.1; /* alpha for rolling average */

DeviceSplitKernel::DeviceSplitKernel(Device *device) : device(device)
{
	current_max_closure = -1;
	first_tile = true;

	avg_time_per_sample = 0.0;

	kernel_path_init = NULL;
	kernel_scene_intersect = NULL;
	kernel_lamp_emission = NULL;
	kernel_queue_enqueue = NULL;
	kernel_background_buffer_update = NULL;
	kernel_shader_eval = NULL;
	kernel_holdout_emission_blurring_pathtermination_ao = NULL;
	kernel_direct_lighting = NULL;
	kernel_shadow_blocked = NULL;
	kernel_next_iteration_setup = NULL;
}

DeviceSplitKernel::~DeviceSplitKernel()
{
	device->mem_free(split_data);
	device->mem_free(ray_state);
	device->mem_free(use_queues_flag);
	device->mem_free(queue_index);
	device->mem_free(work_pool_wgs);

	delete kernel_path_init;
	delete kernel_scene_intersect;
	delete kernel_lamp_emission;
	delete kernel_queue_enqueue;
	delete kernel_background_buffer_update;
	delete kernel_shader_eval;
	delete kernel_holdout_emission_blurring_pathtermination_ao;
	delete kernel_direct_lighting;
	delete kernel_shadow_blocked;
	delete kernel_next_iteration_setup;
}

bool DeviceSplitKernel::load_kernels(const DeviceRequestedFeatures& requested_features)
{
#define LOAD_KERNEL(name) \
		kernel_##name = get_split_kernel_function(#name, requested_features); \
		if(!kernel_##name) { \
			return false; \
		}

	LOAD_KERNEL(path_init);
	LOAD_KERNEL(scene_intersect);
	LOAD_KERNEL(lamp_emission);
	LOAD_KERNEL(queue_enqueue);
	LOAD_KERNEL(background_buffer_update);
	LOAD_KERNEL(shader_eval);
	LOAD_KERNEL(holdout_emission_blurring_pathtermination_ao);
	LOAD_KERNEL(direct_lighting);
	LOAD_KERNEL(shadow_blocked);
	LOAD_KERNEL(next_iteration_setup);

#undef LOAD_KERNEL

	current_max_closure = requested_features.max_closure;

	return true;
}

size_t DeviceSplitKernel::max_elements_for_max_buffer_size(device_memory& kg, device_memory& data, size_t max_buffer_size)
{
	size_t size_per_element = state_buffer_size(kg, data, 1024) / 1024;
	return max_buffer_size / size_per_element;
}

bool DeviceSplitKernel::path_trace(DeviceTask *task,
                                   RenderTile& tile,
                                   device_memory& kgbuffer,
                                   device_memory& kernel_data)
{
	if(device->have_error()) {
		return false;
	}

	/* Get local size */
	size_t local_size[2];
	{
		int2 lsize = split_kernel_local_size();
		local_size[0] = lsize[0];
		local_size[1] = lsize[1];
	}

	/* Set gloabl size */
	size_t global_size[2];
	{
		int2 gsize = split_kernel_global_size(kgbuffer, kernel_data, task);

		/* Make sure that set work size is a multiple of local
		 * work size dimensions.
		 */
		global_size[0] = round_up(gsize[0], local_size[0]);
		global_size[1] = round_up(gsize[1], local_size[1]);
	}

	/* Number of elements in the global state buffer */
	int num_global_elements = global_size[0] * global_size[1];

	/* Allocate all required global memory once. */
	if(first_tile) {
		first_tile = false;

		/* Calculate max groups */

		/* Denotes the maximum work groups possible w.r.t. current requested tile size. */
		unsigned int max_work_groups = num_global_elements / WORK_POOL_SIZE + 1;

		/* Allocate work_pool_wgs memory. */
		work_pool_wgs.resize(max_work_groups * sizeof(unsigned int));
		device->mem_alloc("work_pool_wgs", work_pool_wgs, MEM_READ_WRITE);

		queue_index.resize(NUM_QUEUES * sizeof(int));
		device->mem_alloc("queue_index", queue_index, MEM_READ_WRITE);

		use_queues_flag.resize(sizeof(char));
		device->mem_alloc("use_queues_flag", use_queues_flag, MEM_READ_WRITE);

		ray_state.resize(num_global_elements);
		device->mem_alloc("ray_state", ray_state, MEM_READ_WRITE);

		split_data.resize(state_buffer_size(kgbuffer, kernel_data, num_global_elements));
		device->mem_alloc("split_data", split_data, MEM_READ_WRITE);
	}

#define ENQUEUE_SPLIT_KERNEL(name, global_size, local_size) \
		if(device->have_error()) { \
			return false; \
		} \
		if(!kernel_##name->enqueue(KernelDimensions(global_size, local_size), kgbuffer, kernel_data)) { \
			return false; \
		}

	tile.sample = tile.start_sample;

	/* for exponential increase between tile updates */
	int time_multiplier = 1;

	while(tile.sample < tile.start_sample + tile.num_samples) {
		/* to keep track of how long it takes to run a number of samples */
		double start_time = time_dt();

		/* initial guess to start rolling average */
		const int initial_num_samples = 1;
		/* approx number of samples per second */
		int samples_per_second = (avg_time_per_sample > 0.0) ?
		                         int(double(time_multiplier) / avg_time_per_sample) + 1 : initial_num_samples;

		RenderTile subtile = tile;
		subtile.start_sample = tile.sample;
		subtile.num_samples = min(samples_per_second, tile.start_sample + tile.num_samples - tile.sample);

		if(device->have_error()) {
			return false;
		}

		/* reset state memory here as global size for data_init
		 * kernel might not be large enough to do in kernel
		 */
		device->mem_zero(work_pool_wgs);
		device->mem_zero(split_data);

		if(!enqueue_split_kernel_data_init(KernelDimensions(global_size, local_size),
		                                           subtile,
		                                           num_global_elements,
		                                           kgbuffer,
		                                           kernel_data,
		                                           split_data,
		                                           ray_state,
		                                           queue_index,
		                                           use_queues_flag,
		                                           work_pool_wgs
		                                           ))
		{
			return false;
		}

		ENQUEUE_SPLIT_KERNEL(path_init, global_size, local_size);

		bool activeRaysAvailable = true;

		while(activeRaysAvailable) {
			/* Twice the global work size of other kernels for
			 * ckPathTraceKernel_shadow_blocked_direct_lighting. */
			size_t global_size_shadow_blocked[2];
			global_size_shadow_blocked[0] = global_size[0] * 2;
			global_size_shadow_blocked[1] = global_size[1];

			/* Do path-iteration in host [Enqueue Path-iteration kernels. */
			for(int PathIter = 0; PathIter < 16; PathIter++) {
				ENQUEUE_SPLIT_KERNEL(scene_intersect, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(lamp_emission, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(queue_enqueue, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(background_buffer_update, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shader_eval, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(holdout_emission_blurring_pathtermination_ao, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(direct_lighting, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shadow_blocked, global_size_shadow_blocked, local_size);
				ENQUEUE_SPLIT_KERNEL(next_iteration_setup, global_size, local_size);

				if(task->get_cancel()) {
					return true;
				}
			}

			/* Decide if we should exit path-iteration in host. */
			device->mem_copy_from(ray_state, 0, global_size[0] * global_size[1] * sizeof(char), 1, 1);

			activeRaysAvailable = false;

			for(int rayStateIter = 0; rayStateIter < global_size[0] * global_size[1]; ++rayStateIter) {
				if(int8_t(ray_state.get_data()[rayStateIter]) != RAY_INACTIVE) {
					/* Not all rays are RAY_INACTIVE. */
					activeRaysAvailable = true;
					break;
				}
			}

			if(task->get_cancel()) {
				return true;
			}
		}

		double time_per_sample = ((time_dt()-start_time) / subtile.num_samples);

		if(avg_time_per_sample == 0.0) {
			/* start rolling average */
			avg_time_per_sample = time_per_sample;
		}
		else {
			avg_time_per_sample = alpha*time_per_sample + (1.0-alpha)*avg_time_per_sample;
		}

#undef ENQUEUE_SPLIT_KERNEL

		tile.sample += subtile.num_samples;
		task->update_progress(&tile, tile.w*tile.h*subtile.num_samples);

		time_multiplier = min(time_multiplier << 1, 10);

		if(task->get_cancel()) {
			return true;
		}
	}

	return true;
}

CCL_NAMESPACE_END



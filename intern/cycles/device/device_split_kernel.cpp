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

#include "device/device_split_kernel.h"

#include "kernel/kernel_types.h"
#include "kernel/split/kernel_split_data_types.h"

#include "util/util_logging.h"
#include "util/util_time.h"

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
	kernel_do_volume = NULL;
	kernel_queue_enqueue = NULL;
	kernel_indirect_background = NULL;
	kernel_shader_setup = NULL;
	kernel_shader_sort = NULL;
	kernel_shader_eval = NULL;
	kernel_holdout_emission_blurring_pathtermination_ao = NULL;
	kernel_subsurface_scatter = NULL;
	kernel_direct_lighting = NULL;
	kernel_shadow_blocked_ao = NULL;
	kernel_shadow_blocked_dl = NULL;
	kernel_enqueue_inactive = NULL;
	kernel_next_iteration_setup = NULL;
	kernel_indirect_subsurface = NULL;
	kernel_buffer_update = NULL;
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
	delete kernel_do_volume;
	delete kernel_queue_enqueue;
	delete kernel_indirect_background;
	delete kernel_shader_setup;
	delete kernel_shader_sort;
	delete kernel_shader_eval;
	delete kernel_holdout_emission_blurring_pathtermination_ao;
	delete kernel_subsurface_scatter;
	delete kernel_direct_lighting;
	delete kernel_shadow_blocked_ao;
	delete kernel_shadow_blocked_dl;
	delete kernel_enqueue_inactive;
	delete kernel_next_iteration_setup;
	delete kernel_indirect_subsurface;
	delete kernel_buffer_update;
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
	LOAD_KERNEL(do_volume);
	LOAD_KERNEL(queue_enqueue);
	LOAD_KERNEL(indirect_background);
	LOAD_KERNEL(shader_setup);
	LOAD_KERNEL(shader_sort);
	LOAD_KERNEL(shader_eval);
	LOAD_KERNEL(holdout_emission_blurring_pathtermination_ao);
	LOAD_KERNEL(subsurface_scatter);
	LOAD_KERNEL(direct_lighting);
	LOAD_KERNEL(shadow_blocked_ao);
	LOAD_KERNEL(shadow_blocked_dl);
	LOAD_KERNEL(enqueue_inactive);
	LOAD_KERNEL(next_iteration_setup);
	LOAD_KERNEL(indirect_subsurface);
	LOAD_KERNEL(buffer_update);

#undef LOAD_KERNEL

	current_max_closure = requested_features.max_closure;

	return true;
}

size_t DeviceSplitKernel::max_elements_for_max_buffer_size(device_memory& kg, device_memory& data, uint64_t max_buffer_size)
{
	uint64_t size_per_element = state_buffer_size(kg, data, 1024) / 1024;
	VLOG(1) << "Split state element size: "
	        << string_human_readable_number(size_per_element) << " bytes. ("
	        << string_human_readable_size(size_per_element) << ").";
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

	/* Number of elements in the global state buffer */
	int num_global_elements = global_size[0] * global_size[1];

	/* Allocate all required global memory once. */
	if(first_tile) {
		first_tile = false;

		/* Set gloabl size */
		{
			int2 gsize = split_kernel_global_size(kgbuffer, kernel_data, task);

			/* Make sure that set work size is a multiple of local
			 * work size dimensions.
			 */
			global_size[0] = round_up(gsize[0], local_size[0]);
			global_size[1] = round_up(gsize[1], local_size[1]);
		}

		num_global_elements = global_size[0] * global_size[1];
		assert(num_global_elements % WORK_POOL_SIZE == 0);

		/* Calculate max groups */

		/* Denotes the maximum work groups possible w.r.t. current requested tile size. */
		unsigned int work_pool_size = (device->info.type == DEVICE_CPU) ? WORK_POOL_SIZE_CPU : WORK_POOL_SIZE_GPU;
		unsigned int max_work_groups = num_global_elements / work_pool_size + 1;

		/* Allocate work_pool_wgs memory. */
		work_pool_wgs.resize(max_work_groups);
		device->mem_alloc("work_pool_wgs", work_pool_wgs, MEM_READ_WRITE);

		queue_index.resize(NUM_QUEUES);
		device->mem_alloc("queue_index", queue_index, MEM_READ_WRITE);

		use_queues_flag.resize(1);
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
		device->mem_zero(ray_state);

		if(!enqueue_split_kernel_data_init(KernelDimensions(global_size, local_size),
		                                   subtile,
		                                   num_global_elements,
		                                   kgbuffer,
		                                   kernel_data,
		                                   split_data,
		                                   ray_state,
		                                   queue_index,
		                                   use_queues_flag,
		                                   work_pool_wgs))
		{
			return false;
		}

		ENQUEUE_SPLIT_KERNEL(path_init, global_size, local_size);

		bool activeRaysAvailable = true;
		double cancel_time = DBL_MAX;

		while(activeRaysAvailable) {
			/* Do path-iteration in host [Enqueue Path-iteration kernels. */
			for(int PathIter = 0; PathIter < 16; PathIter++) {
				ENQUEUE_SPLIT_KERNEL(scene_intersect, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(lamp_emission, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(do_volume, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(queue_enqueue, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(indirect_background, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shader_setup, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shader_sort, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shader_eval, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(holdout_emission_blurring_pathtermination_ao, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(subsurface_scatter, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(queue_enqueue, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(direct_lighting, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shadow_blocked_ao, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shadow_blocked_dl, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(enqueue_inactive, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(next_iteration_setup, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(indirect_subsurface, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(queue_enqueue, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(buffer_update, global_size, local_size);

				if(task->get_cancel() && cancel_time == DBL_MAX) {
					/* Wait up to twice as many seconds for current samples to finish 
					 * to avoid artifacts in render result from ending too soon.
					 */
					cancel_time = time_dt() + 2.0 * time_multiplier;
				}

				if(time_dt() > cancel_time) {
					return true;
				}
			}

			/* Decide if we should exit path-iteration in host. */
			device->mem_copy_from(ray_state, 0, global_size[0] * global_size[1] * sizeof(char), 1, 1);

			activeRaysAvailable = false;

			for(int rayStateIter = 0; rayStateIter < global_size[0] * global_size[1]; ++rayStateIter) {
				if(!IS_STATE(ray_state.get_data(), rayStateIter, RAY_INACTIVE)) {
					if(IS_STATE(ray_state.get_data(), rayStateIter, RAY_INVALID)) {
						/* Something went wrong, abort to avoid looping endlessly. */
						device->set_error("Split kernel error: invalid ray state");
						return false;
					}

					/* Not all rays are RAY_INACTIVE. */
					activeRaysAvailable = true;
					break;
				}
			}

			if(time_dt() > cancel_time) {
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



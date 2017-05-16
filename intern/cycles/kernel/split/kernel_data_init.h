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

CCL_NAMESPACE_BEGIN

/* This kernel Initializes structures needed in path-iteration kernels.
 *
 * Note on Queues:
 * All slots in queues are initialized to queue empty slot;
 * The number of elements in the queues is initialized to 0;
 */

/* Distributes an amount of work across all threads
 * note: work done inside the loop may not show up to all threads till after
 * the current kernel has completed
 */
#define parallel_for(kg, iter_name, work_size) \
	for(size_t _size = (work_size), \
	    _global_size = ccl_global_size(0) * ccl_global_size(1), \
	    _n = _size / _global_size, \
		_thread = ccl_global_id(0) + ccl_global_id(1) * ccl_global_size(0), \
	    iter_name = (_n > 0) ? (_thread * _n) : (_thread) \
		; \
		(iter_name < (_thread+1) * _n) || (iter_name == _n * _global_size + _thread && _thread < _size % _global_size) \
		; \
		iter_name = (iter_name != (_thread+1) * _n - 1) ? (iter_name + 1) : (_n * _global_size + _thread) \
	)

#ifndef __KERNEL_CPU__
ccl_device void kernel_data_init(
#else
void KERNEL_FUNCTION_FULL_NAME(data_init)(
#endif
        KernelGlobals *kg,
        ccl_constant KernelData *data,
        ccl_global void *split_data_buffer,
        int num_elements,
        ccl_global char *ray_state,
        ccl_global uint *rng_state,

#ifdef __KERNEL_OPENCL__
#define KERNEL_TEX(type, ttype, name)                                   \
        ccl_global type *name,
#include "kernel/kernel_textures.h"
#endif

        int start_sample,
        int end_sample,
        int sx, int sy, int sw, int sh, int offset, int stride,
        ccl_global int *Queue_index,                 /* Tracks the number of elements in queues */
        int queuesize,                               /* size (capacity) of the queue */
        ccl_global char *use_queues_flag,            /* flag to decide if scene-intersect kernel should use queues to fetch ray index */
        ccl_global unsigned int *work_pools,      /* Work pool for each work group */
        unsigned int num_samples,
        ccl_global float *buffer)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, data_init);
#else

#ifdef __KERNEL_OPENCL__
	kg->data = data;
#endif

	kernel_split_params.x = sx;
	kernel_split_params.y = sy;
	kernel_split_params.w = sw;
	kernel_split_params.h = sh;

	kernel_split_params.offset = offset;
	kernel_split_params.stride = stride;

	kernel_split_params.rng_state = rng_state;

	kernel_split_params.start_sample = start_sample;
	kernel_split_params.end_sample = end_sample;

	kernel_split_params.work_pools = work_pools;
	kernel_split_params.num_samples = num_samples;

	kernel_split_params.queue_index = Queue_index;
	kernel_split_params.queue_size = queuesize;
	kernel_split_params.use_queues_flag = use_queues_flag;

	kernel_split_params.buffer = buffer;

	split_data_init(kg, &kernel_split_state, num_elements, split_data_buffer, ray_state);

#ifdef __KERNEL_OPENCL__
#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "kernel/kernel_textures.h"
#endif

	int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);

	/* Initialize queue data and queue index. */
	if(thread_index < queuesize) {
		for(int i = 0; i < NUM_QUEUES; i++) {
			kernel_split_state.queue_data[i * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		}
	}

	if(thread_index == 0) {
		for(int i = 0; i < NUM_QUEUES; i++) {
			Queue_index[i] = 0;
		}

		/* The scene-intersect kernel should not use the queues very first time.
		 * since the queue would be empty.
		 */
		*use_queues_flag = 0;
	}

	/* zero the tiles pixels and initialize rng_state if this is the first sample */
	if(start_sample == 0) {
		parallel_for(kg, i, sw * sh * kernel_data.film.pass_stride) {
			int pixel = i / kernel_data.film.pass_stride;
			int pass = i % kernel_data.film.pass_stride;

			int x = sx + pixel % sw;
			int y = sy + pixel / sw;

			int index = (offset + x + y*stride) * kernel_data.film.pass_stride + pass;

			*(buffer + index) = 0.0f;
		}

		parallel_for(kg, i, sw * sh) {
			int x = sx + i % sw;
			int y = sy + i / sw;

			int index = (offset + x + y*stride);
			*(rng_state + index) = hash_int_2d(x, y);
		}
	}

#endif  /* KERENL_STUB */
}

CCL_NAMESPACE_END

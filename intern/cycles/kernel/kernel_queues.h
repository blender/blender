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

#ifndef __KERNEL_QUEUE_H__
#define __KERNEL_QUEUE_H__

/*
 * Queue utility functions for split kernel
 */

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable

/*
 * Enqueue ray index into the queue
 */
ccl_device void enqueue_ray_index(
        int ray_index,                /* Ray index to be enqueued. */
        int queue_number,             /* Queue in which the ray index should be enqueued. */
        ccl_global int *queues,       /* Buffer of all queues. */
        int queue_size,               /* Size of each queue. */
        ccl_global int *queue_index) /* Array of size num_queues; Used for atomic increment. */
{
	/* This thread's queue index. */
	int my_queue_index = atomic_inc(&queue_index[queue_number]) + (queue_number * queue_size);
	queues[my_queue_index] = ray_index;
}

/*
 * Get the ray index for this thread
 * Returns a positive ray_index for threads that have to do some work;
 * Returns 'QUEUE_EMPTY_SLOT' for threads that don't have any work
 * i.e All ray's in the queue has been successfully allocated and there
 * is no more ray to allocate to other threads.
 */
ccl_device int get_ray_index(
        int thread_index,       /* Global thread index. */
        int queue_number,       /* Queue to operate on. */
        ccl_global int *queues, /* Buffer of all queues. */
        int queuesize,          /* Size of a queue. */
        int empty_queue)        /* Empty the queue slot as soon as we fetch the ray index. */
{
	int ray_index = queues[queue_number * queuesize + thread_index];
	if(empty_queue && ray_index != QUEUE_EMPTY_SLOT) {
		queues[queue_number * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
	}
	return ray_index;
}

/* The following functions are to realize Local memory variant of enqueue ray index function. */

/* All threads should call this function. */
ccl_device void enqueue_ray_index_local(
        int ray_index,                               /* Ray index to enqueue. */
        int queue_number,                            /* Queue in which to enqueue ray index. */
        char enqueue_flag,                           /* True for threads whose ray index has to be enqueued. */
        int queuesize,                               /* queue size. */
        ccl_local unsigned int *local_queue_atomics,   /* To to local queue atomics. */
        ccl_global int *Queue_data,                  /* Queues. */
        ccl_global int *Queue_index)                 /* To do global queue atomics. */
{
	int lidx = get_local_id(1) * get_local_size(0) + get_local_id(0);

	/* Get local queue id .*/
	unsigned int lqidx;
	if(enqueue_flag) {
		lqidx = atomic_inc(local_queue_atomics);
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	/* Get global queue offset. */
	if(lidx == 0) {
		*local_queue_atomics = atomic_add(&Queue_index[queue_number], *local_queue_atomics);
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	/* Get global queue index and enqueue ray. */
	if(enqueue_flag) {
		unsigned int my_gqidx = queue_number * queuesize + (*local_queue_atomics) + lqidx;
		Queue_data[my_gqidx] = ray_index;
	}
}

ccl_device unsigned int get_local_queue_index(
        int queue_number, /* Queue in which to enqueue the ray; -1 if no queue */
        ccl_local unsigned int *local_queue_atomics)
{
	int my_lqidx = atomic_inc(&local_queue_atomics[queue_number]);
	return my_lqidx;
}

ccl_device unsigned int get_global_per_queue_offset(
        int queue_number,
        ccl_local unsigned int *local_queue_atomics,
        ccl_global int* global_queue_atomics)
{
	unsigned int queue_offset = atomic_add(&global_queue_atomics[queue_number],
	                                       local_queue_atomics[queue_number]);
	return queue_offset;
}

ccl_device unsigned int get_global_queue_index(
    int queue_number,
    int queuesize,
    unsigned int lqidx,
    ccl_local unsigned int * global_per_queue_offset)
{
	int my_gqidx = queuesize * queue_number + lqidx + global_per_queue_offset[queue_number];
	return my_gqidx;
}

#endif // __KERNEL_QUEUE_H__

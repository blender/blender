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

#ifdef __KERNEL_OPENCL__
    KERNEL_BUFFER_PARAMS,
#endif

    int start_sample,
    int end_sample,
    int sx,
    int sy,
    int sw,
    int sh,
    int offset,
    int stride,
    ccl_global int *Queue_index,      /* Tracks the number of elements in queues */
    int queuesize,                    /* size (capacity) of the queue */
    ccl_global char *use_queues_flag, /* flag to decide if scene-intersect kernel should use queues
                                         to fetch ray index */
    ccl_global unsigned int *work_pools, /* Work pool for each work group */
    unsigned int num_samples,
    ccl_global float *buffer)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, data_init);
#else

#  ifdef __KERNEL_OPENCL__
  kg->data = data;
#  endif

  kernel_split_params.tile.x = sx;
  kernel_split_params.tile.y = sy;
  kernel_split_params.tile.w = sw;
  kernel_split_params.tile.h = sh;

  kernel_split_params.tile.start_sample = start_sample;
  kernel_split_params.tile.num_samples = num_samples;

  kernel_split_params.tile.offset = offset;
  kernel_split_params.tile.stride = stride;

  kernel_split_params.tile.buffer = buffer;

  kernel_split_params.total_work_size = sw * sh * num_samples;

  kernel_split_params.work_pools = work_pools;

  kernel_split_params.queue_index = Queue_index;
  kernel_split_params.queue_size = queuesize;
  kernel_split_params.use_queues_flag = use_queues_flag;

  split_data_init(kg, &kernel_split_state, num_elements, split_data_buffer, ray_state);

#  ifdef __KERNEL_OPENCL__
  kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
  kernel_set_buffer_info(kg);
#  endif

  int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);

  /* Initialize queue data and queue index. */
  if (thread_index < queuesize) {
    for (int i = 0; i < NUM_QUEUES; i++) {
      kernel_split_state.queue_data[i * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
    }
  }

  if (thread_index == 0) {
    for (int i = 0; i < NUM_QUEUES; i++) {
      Queue_index[i] = 0;
    }

    /* The scene-intersect kernel should not use the queues very first time.
     * since the queue would be empty.
     */
    *use_queues_flag = 0;
  }
#endif /* KERENL_STUB */
}

CCL_NAMESPACE_END

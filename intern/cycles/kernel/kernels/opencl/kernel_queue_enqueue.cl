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

#include "split/kernel_queue_enqueue.h"

__kernel void kernel_ocl_path_trace_queue_enqueue(
        ccl_global int *Queue_data,   /* Queue memory */
        ccl_global int *Queue_index,  /* Tracks the number of elements in each queue */
        ccl_global char *ray_state,   /* Denotes the state of each ray */
        int queuesize)                /* Size (capacity) of each queue */
{
	kernel_queue_enqueue(Queue_data,
	                     Queue_index,
	                     ray_state,
	                     queuesize);
}

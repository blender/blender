/*
 * Copyright 2011-2017 Blender Foundation
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

#define KERNEL_NAME_JOIN(a, b) a ## _ ## b
#define KERNEL_NAME_EVAL(a, b) KERNEL_NAME_JOIN(a, b)

__kernel void KERNEL_NAME_EVAL(kernel_ocl_path_trace, KERNEL_NAME)(
		ccl_global char *kg_global,
		ccl_constant KernelData *data,

		ccl_global void *split_data_buffer,
		ccl_global char *ray_state,

		KERNEL_BUFFER_PARAMS,

		ccl_global int *queue_index,
		ccl_global char *use_queues_flag,
		ccl_global unsigned int *work_pools,
		ccl_global float *buffer
	)
{
#ifdef LOCALS_TYPE
	ccl_local LOCALS_TYPE locals;
#endif

	KernelGlobals *kg = (KernelGlobals*)kg_global;

	if(ccl_local_id(0) + ccl_local_id(1) == 0) {
		kg->data = data;

		kernel_split_params.queue_index = queue_index;
		kernel_split_params.use_queues_flag = use_queues_flag;
		kernel_split_params.work_pools = work_pools;
		kernel_split_params.tile.buffer = buffer;

		split_data_init(kg, &kernel_split_state, ccl_global_size(0)*ccl_global_size(1), split_data_buffer, ray_state);

	}

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);

	KERNEL_NAME_EVAL(kernel, KERNEL_NAME)(
			kg
#ifdef LOCALS_TYPE
			, &locals
#endif
		);
}

#undef KERNEL_NAME_JOIN
#undef KERNEL_NAME_EVAL

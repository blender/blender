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

#ifndef __KERNEL_SPLIT_DATA_H__
#define __KERNEL_SPLIT_DATA_H__

#include "kernel/split/kernel_split_data_types.h"
#include "kernel/kernel_globals.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline uint64_t split_data_buffer_size(KernelGlobals *kg, size_t num_elements)
{
	(void)kg;  /* Unused on CPU. */

	uint64_t size = 0;
#define SPLIT_DATA_ENTRY(type, name, num) + align_up(num_elements * num * sizeof(type), 16)
	size = size SPLIT_DATA_ENTRIES;
#undef SPLIT_DATA_ENTRY

	uint64_t closure_size = sizeof(ShaderClosure) * (kernel_data.integrator.max_closures-1);

#ifdef __BRANCHED_PATH__
	size += align_up(num_elements * (sizeof(ShaderData) + closure_size), 16);
#endif

	size += align_up(num_elements * (sizeof(ShaderData) + closure_size), 16);

	return size;
}

ccl_device_inline void split_data_init(KernelGlobals *kg,
                                       ccl_global SplitData *split_data,
                                       size_t num_elements,
                                       ccl_global void *data,
                                       ccl_global char *ray_state)
{
	(void)kg;  /* Unused on CPU. */

	ccl_global char *p = (ccl_global char*)data;

#define SPLIT_DATA_ENTRY(type, name, num) \
	split_data->name = (type*)p; p += align_up(num_elements * num * sizeof(type), 16);
	SPLIT_DATA_ENTRIES;
#undef SPLIT_DATA_ENTRY

	uint64_t closure_size = sizeof(ShaderClosure) * (kernel_data.integrator.max_closures-1);

#ifdef __BRANCHED_PATH__
	split_data->_branched_state_sd = (ShaderData*)p;
	p += align_up(num_elements * (sizeof(ShaderData) + closure_size), 16);
#endif

	split_data->_sd = (ShaderData*)p;
	p += align_up(num_elements * (sizeof(ShaderData) + closure_size), 16);

	split_data->ray_state = ray_state;
}

CCL_NAMESPACE_END

#endif  /* __KERNEL_SPLIT_DATA_H__ */

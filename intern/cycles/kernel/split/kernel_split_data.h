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

#include "kernel_split_data_types.h"
#include "kernel_globals.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline uint64_t split_data_buffer_size(KernelGlobals *kg, size_t num_elements)
{
	(void)kg;  /* Unused on CPU. */

	uint64_t size = 0;
#define SPLIT_DATA_ENTRY(type, name, num) + align_up(num_elements * num * sizeof(type), 16)
	size = size SPLIT_DATA_ENTRIES;
#undef SPLIT_DATA_ENTRY

#ifdef __SUBSURFACE__
	size += align_up(num_elements * sizeof(SubsurfaceIndirectRays), 16); /* ss_rays */
#endif

#ifdef __VOLUME__
	size += align_up(2 * num_elements * sizeof(PathState), 16); /* state_shadow */
#endif

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

#ifdef __SUBSURFACE__
	split_data->ss_rays = (ccl_global SubsurfaceIndirectRays*)p;
	p += align_up(num_elements * sizeof(SubsurfaceIndirectRays), 16);
#endif

#ifdef __VOLUME__
	split_data->state_shadow = (ccl_global PathState*)p;
	p += align_up(2 * num_elements * sizeof(PathState), 16);
#endif

	split_data->ray_state = ray_state;
}

CCL_NAMESPACE_END

#endif  /* __KERNEL_SPLIT_DATA_H__ */

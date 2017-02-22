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

CCL_NAMESPACE_BEGIN

/* parameters used by the split kernels, we use a single struct to avoid passing these to each kernel */

typedef struct SplitParams {
	int x;
	int y;
	int w;
	int h;

	int offset;
	int stride;

	ccl_global uint *rng_state;

	int start_sample;
	int end_sample;

	ccl_global unsigned int *work_pools;
	unsigned int num_samples;

	ccl_global int *queue_index;
	int queue_size;
	ccl_global char *use_queues_flag;

	ccl_global float *buffer;
} SplitParams;

/* Global memory variables [porting]; These memory is used for
 * co-operation between different kernels; Data written by one
 * kernel will be available to another kernel via this global
 * memory.
 */

/* SPLIT_DATA_ENTRY(type, name, num) */

#if defined(WITH_CYCLES_DEBUG) || defined(__KERNEL_DEBUG__)
/* DebugData memory */
#  define SPLIT_DATA_DEBUG_ENTRIES \
	SPLIT_DATA_ENTRY(DebugData, debug_data, 1)
#else
#  define SPLIT_DATA_DEBUG_ENTRIES
#endif

#define SPLIT_DATA_ENTRIES \
	SPLIT_DATA_ENTRY(ccl_global RNG, rng, 1) \
	SPLIT_DATA_ENTRY(ccl_global float3, throughput, 1) \
	SPLIT_DATA_ENTRY(ccl_global float, L_transparent, 1) \
	SPLIT_DATA_ENTRY(PathRadiance, path_radiance, 1) \
	SPLIT_DATA_ENTRY(ccl_global Ray, ray, 1) \
	SPLIT_DATA_ENTRY(ccl_global PathState, path_state, 1) \
	SPLIT_DATA_ENTRY(Intersection, isect, 1) \
	SPLIT_DATA_ENTRY(ccl_global float3, ao_alpha, 1) \
	SPLIT_DATA_ENTRY(ccl_global float3, ao_bsdf, 1) \
	SPLIT_DATA_ENTRY(ccl_global Ray, ao_light_ray, 1) \
	SPLIT_DATA_ENTRY(ccl_global BsdfEval, bsdf_eval, 1) \
	SPLIT_DATA_ENTRY(ccl_global int, is_lamp, 1) \
	SPLIT_DATA_ENTRY(ccl_global Ray, light_ray, 1) \
	SPLIT_DATA_ENTRY(Intersection, isect_shadow, 2) \
	SPLIT_DATA_ENTRY(ccl_global int, queue_data, (NUM_QUEUES*2)) /* TODO(mai): this is too large? */ \
	SPLIT_DATA_ENTRY(ccl_global uint, work_array, 1) \
	SPLIT_DATA_DEBUG_ENTRIES \

/* struct that holds pointers to data in the shared state buffer */
typedef struct SplitData {
#define SPLIT_DATA_ENTRY(type, name, num) type *name;
	SPLIT_DATA_ENTRIES
#undef SPLIT_DATA_ENTRY

	/* size calculation for these is non trivial, so they are left out of SPLIT_DATA_ENTRIES and handled separately */
	ShaderData *sd;
	ShaderData *sd_DL_shadow;
	ccl_global float *per_sample_output_buffers;

	/* this is actually in a separate buffer from the rest of the split state data (so it can be read back from
	 * the host easily) but is still used the same as the other data so we have it here in this struct as well
	 */
	ccl_global char *ray_state;
} SplitData;

#define SIZEOF_SD(max_closure) (sizeof(ShaderData) - (sizeof(ShaderClosure) * (MAX_CLOSURE - (max_closure))))

ccl_device_inline size_t split_data_buffer_size(size_t num_elements,
                                                size_t max_closure,
                                                size_t per_thread_output_buffer_size)
{
	size_t size = 0;
#define SPLIT_DATA_ENTRY(type, name, num) + align_up(num_elements * num * sizeof(type), 16)
	size = size SPLIT_DATA_ENTRIES;
#undef SPLIT_DATA_ENTRY

	/* TODO(sergey): This will actually over-allocate if
	 * particular kernel does not support multiclosure.
	 */
	size += align_up(num_elements * SIZEOF_SD(max_closure), 16); /* sd */
	size += align_up(2 * num_elements * SIZEOF_SD(max_closure), 16); /* sd_DL_shadow */
	size += align_up(num_elements * per_thread_output_buffer_size, 16); /* per_sample_output_buffers */

	return size;
}

ccl_device_inline void split_data_init(ccl_global SplitData *split_data,
                                       size_t num_elements,
                                       ccl_global void *data,
                                       ccl_global char *ray_state)
{
	ccl_global char *p = (ccl_global char*)data;

#define SPLIT_DATA_ENTRY(type, name, num) \
	split_data->name = (type*)p; p += align_up(num_elements * num * sizeof(type), 16);
	SPLIT_DATA_ENTRIES
#undef SPLIT_DATA_ENTRY

	split_data->sd = (ShaderData*)p;
	p += align_up(num_elements * SIZEOF_SD(MAX_CLOSURE), 16);

	split_data->sd_DL_shadow = (ShaderData*)p;
	p += align_up(2 * num_elements * SIZEOF_SD(MAX_CLOSURE), 16);

	split_data->per_sample_output_buffers = (ccl_global float*)p;
	//p += align_up(num_elements * per_thread_output_buffer_size, 16);

	split_data->ray_state = ray_state;
}

#define kernel_split_state (kg->split_data)
#define kernel_split_params (kg->split_param_data)

CCL_NAMESPACE_END

#endif  /* __KERNEL_SPLIT_DATA_H__ */




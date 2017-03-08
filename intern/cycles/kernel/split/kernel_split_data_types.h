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

#ifndef __KERNEL_SPLIT_DATA_TYPES_H__
#define __KERNEL_SPLIT_DATA_TYPES_H__

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
	SPLIT_DATA_ENTRY(ccl_global Intersection, isect, 1) \
	SPLIT_DATA_ENTRY(ccl_global float3, ao_alpha, 1) \
	SPLIT_DATA_ENTRY(ccl_global float3, ao_bsdf, 1) \
	SPLIT_DATA_ENTRY(ccl_global Ray, ao_light_ray, 1) \
	SPLIT_DATA_ENTRY(ccl_global BsdfEval, bsdf_eval, 1) \
	SPLIT_DATA_ENTRY(ccl_global int, is_lamp, 1) \
	SPLIT_DATA_ENTRY(ccl_global Ray, light_ray, 1) \
	SPLIT_DATA_ENTRY(ccl_global int, queue_data, (NUM_QUEUES*2)) /* TODO(mai): this is too large? */ \
	SPLIT_DATA_ENTRY(ccl_global uint, work_array, 1) \
	SPLIT_DATA_ENTRY(ShaderData, sd, 1) \
	SPLIT_DATA_ENTRY(ShaderData, sd_DL_shadow, 2) \
	SPLIT_DATA_DEBUG_ENTRIES \

/* struct that holds pointers to data in the shared state buffer */
typedef struct SplitData {
#define SPLIT_DATA_ENTRY(type, name, num) type *name;
	SPLIT_DATA_ENTRIES
#undef SPLIT_DATA_ENTRY

#ifdef __SUBSURFACE__
	ccl_global SubsurfaceIndirectRays *ss_rays;
#endif

#ifdef __VOLUME__
	ccl_global PathState *state_shadow;
#endif

	/* this is actually in a separate buffer from the rest of the split state data (so it can be read back from
	 * the host easily) but is still used the same as the other data so we have it here in this struct as well
	 */
	ccl_global char *ray_state;
} SplitData;

#ifndef __KERNEL_CUDA__
#  define kernel_split_state (kg->split_data)
#  define kernel_split_params (kg->split_param_data)
#else
__device__ SplitData __split_data;
#  define kernel_split_state (__split_data)
__device__ SplitParams __split_param_data;
#  define kernel_split_params (__split_param_data)
#endif  /* __KERNEL_CUDA__ */

CCL_NAMESPACE_END

#endif  /* __KERNEL_SPLIT_DATA_TYPES_H__ */

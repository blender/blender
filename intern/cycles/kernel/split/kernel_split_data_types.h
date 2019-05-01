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

/* parameters used by the split kernels, we use a single struct to avoid passing these to each
 * kernel */

typedef struct SplitParams {
  WorkTile tile;
  uint total_work_size;

  ccl_global unsigned int *work_pools;

  ccl_global int *queue_index;
  int queue_size;
  ccl_global char *use_queues_flag;

  /* Place for storing sd->flag. AMD GPU OpenCL compiler workaround */
  int dummy_sd_flag;
} SplitParams;

/* Global memory variables [porting]; These memory is used for
 * co-operation between different kernels; Data written by one
 * kernel will be available to another kernel via this global
 * memory.
 */

/* SPLIT_DATA_ENTRY(type, name, num) */

#ifdef __BRANCHED_PATH__

typedef ccl_global struct SplitBranchedState {
  /* various state that must be kept and restored after an indirect loop */
  PathState path_state;
  float3 throughput;
  Ray ray;

  Intersection isect;

  char ray_state;

  /* indirect loop state */
  int next_closure;
  int next_sample;

#  ifdef __SUBSURFACE__
  int ss_next_closure;
  int ss_next_sample;
  int next_hit;
  int num_hits;

  uint lcg_state;
  LocalIntersection ss_isect;
#  endif /*__SUBSURFACE__ */

  int shared_sample_count; /* number of branched samples shared with other threads */
  int original_ray;        /* index of original ray when sharing branched samples */
  bool waiting_on_shared_samples;
} SplitBranchedState;

#  define SPLIT_DATA_BRANCHED_ENTRIES \
    SPLIT_DATA_ENTRY(SplitBranchedState, branched_state, 1) \
    SPLIT_DATA_ENTRY(ShaderData, _branched_state_sd, 0)
#else
#  define SPLIT_DATA_BRANCHED_ENTRIES
#endif /* __BRANCHED_PATH__ */

#ifdef __SUBSURFACE__
#  define SPLIT_DATA_SUBSURFACE_ENTRIES \
    SPLIT_DATA_ENTRY(ccl_global SubsurfaceIndirectRays, ss_rays, 1)
#else
#  define SPLIT_DATA_SUBSURFACE_ENTRIES
#endif /* __SUBSURFACE__ */

#ifdef __VOLUME__
#  define SPLIT_DATA_VOLUME_ENTRIES SPLIT_DATA_ENTRY(ccl_global PathState, state_shadow, 1)
#else
#  define SPLIT_DATA_VOLUME_ENTRIES
#endif /* __VOLUME__ */

#define SPLIT_DATA_ENTRIES \
  SPLIT_DATA_ENTRY(ccl_global float3, throughput, 1) \
  SPLIT_DATA_ENTRY(PathRadiance, path_radiance, 1) \
  SPLIT_DATA_ENTRY(ccl_global Ray, ray, 1) \
  SPLIT_DATA_ENTRY(ccl_global PathState, path_state, 1) \
  SPLIT_DATA_ENTRY(ccl_global Intersection, isect, 1) \
  SPLIT_DATA_ENTRY(ccl_global BsdfEval, bsdf_eval, 1) \
  SPLIT_DATA_ENTRY(ccl_global int, is_lamp, 1) \
  SPLIT_DATA_ENTRY(ccl_global Ray, light_ray, 1) \
  SPLIT_DATA_ENTRY( \
      ccl_global int, queue_data, (NUM_QUEUES * 2)) /* TODO(mai): this is too large? */ \
  SPLIT_DATA_ENTRY(ccl_global uint, buffer_offset, 1) \
  SPLIT_DATA_ENTRY(ShaderDataTinyStorage, sd_DL_shadow, 1) \
  SPLIT_DATA_SUBSURFACE_ENTRIES \
  SPLIT_DATA_VOLUME_ENTRIES \
  SPLIT_DATA_BRANCHED_ENTRIES \
  SPLIT_DATA_ENTRY(ShaderData, _sd, 0)

/* Entries to be copied to inactive rays when sharing branched samples
 * (TODO: which are actually needed?) */
#define SPLIT_DATA_ENTRIES_BRANCHED_SHARED \
  SPLIT_DATA_ENTRY(ccl_global float3, throughput, 1) \
  SPLIT_DATA_ENTRY(PathRadiance, path_radiance, 1) \
  SPLIT_DATA_ENTRY(ccl_global Ray, ray, 1) \
  SPLIT_DATA_ENTRY(ccl_global PathState, path_state, 1) \
  SPLIT_DATA_ENTRY(ccl_global Intersection, isect, 1) \
  SPLIT_DATA_ENTRY(ccl_global BsdfEval, bsdf_eval, 1) \
  SPLIT_DATA_ENTRY(ccl_global int, is_lamp, 1) \
  SPLIT_DATA_ENTRY(ccl_global Ray, light_ray, 1) \
  SPLIT_DATA_ENTRY(ShaderDataTinyStorage, sd_DL_shadow, 1) \
  SPLIT_DATA_SUBSURFACE_ENTRIES \
  SPLIT_DATA_VOLUME_ENTRIES \
  SPLIT_DATA_BRANCHED_ENTRIES \
  SPLIT_DATA_ENTRY(ShaderData, _sd, 0)

/* struct that holds pointers to data in the shared state buffer */
typedef struct SplitData {
#define SPLIT_DATA_ENTRY(type, name, num) type *name;
  SPLIT_DATA_ENTRIES
#undef SPLIT_DATA_ENTRY

  /* this is actually in a separate buffer from the rest of the split state data (so it can be read
   * back from the host easily) but is still used the same as the other data so we have it here in
   * this struct as well
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
#endif /* __KERNEL_CUDA__ */

#define kernel_split_sd(sd, ray_index) \
  ((ShaderData *)(((ccl_global char *)kernel_split_state._##sd) + \
                  (sizeof(ShaderData) + \
                   sizeof(ShaderClosure) * (kernel_data.integrator.max_closures - 1)) * \
                      (ray_index)))

/* Local storage for queue_enqueue kernel. */
typedef struct QueueEnqueueLocals {
  uint queue_atomics[2];
} QueueEnqueueLocals;

/* Local storage for holdout_emission_blurring_pathtermination_ao kernel. */
typedef struct BackgroundAOLocals {
  uint queue_atomics_bg;
  uint queue_atomics_ao;
} BackgroundAOLocals;

typedef struct ShaderSortLocals {
  uint local_value[SHADER_SORT_BLOCK_SIZE];
  ushort local_index[SHADER_SORT_BLOCK_SIZE];
} ShaderSortLocals;

CCL_NAMESPACE_END

#endif /* __KERNEL_SPLIT_DATA_TYPES_H__ */

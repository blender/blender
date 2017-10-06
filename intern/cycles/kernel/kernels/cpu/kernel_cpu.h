/*
 * Copyright 2011-2013 Blender Foundation
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

/* Templated common declaration part of all CPU kernels. */

void KERNEL_FUNCTION_FULL_NAME(path_trace)(KernelGlobals *kg,
                                           float *buffer,
                                           int sample,
                                           int x, int y,
                                           int offset,
                                           int stride);

void KERNEL_FUNCTION_FULL_NAME(convert_to_byte)(KernelGlobals *kg,
                                                uchar4 *rgba,
                                                float *buffer,
                                                float sample_scale,
                                                int x, int y,
                                                int offset, int stride);

void KERNEL_FUNCTION_FULL_NAME(convert_to_half_float)(KernelGlobals *kg,
                                                      uchar4 *rgba,
                                                      float *buffer,
                                                      float sample_scale,
                                                      int x, int y,
                                                      int offset,
                                                      int stride);

void KERNEL_FUNCTION_FULL_NAME(shader)(KernelGlobals *kg,
                                       uint4 *input,
                                       float4 *output,
                                       int type,
                                       int filter,
                                       int i,
                                       int offset,
                                       int sample);

/* Split kernels */

void KERNEL_FUNCTION_FULL_NAME(data_init)(
        KernelGlobals *kg,
        ccl_constant KernelData *data,
        ccl_global void *split_data_buffer,
        int num_elements,
        ccl_global char *ray_state,
        int start_sample,
        int end_sample,
        int sx, int sy, int sw, int sh, int offset, int stride,
        ccl_global int *Queue_index,
        int queuesize,
        ccl_global char *use_queues_flag,
        ccl_global unsigned int *work_pool_wgs,
        unsigned int num_samples,
        ccl_global float *buffer);

#define DECLARE_SPLIT_KERNEL_FUNCTION(name) \
	void KERNEL_FUNCTION_FULL_NAME(name)(KernelGlobals *kg, KernelData *data);

DECLARE_SPLIT_KERNEL_FUNCTION(path_init)
DECLARE_SPLIT_KERNEL_FUNCTION(scene_intersect)
DECLARE_SPLIT_KERNEL_FUNCTION(lamp_emission)
DECLARE_SPLIT_KERNEL_FUNCTION(do_volume)
DECLARE_SPLIT_KERNEL_FUNCTION(queue_enqueue)
DECLARE_SPLIT_KERNEL_FUNCTION(indirect_background)
DECLARE_SPLIT_KERNEL_FUNCTION(shader_setup)
DECLARE_SPLIT_KERNEL_FUNCTION(shader_sort)
DECLARE_SPLIT_KERNEL_FUNCTION(shader_eval)
DECLARE_SPLIT_KERNEL_FUNCTION(holdout_emission_blurring_pathtermination_ao)
DECLARE_SPLIT_KERNEL_FUNCTION(subsurface_scatter)
DECLARE_SPLIT_KERNEL_FUNCTION(direct_lighting)
DECLARE_SPLIT_KERNEL_FUNCTION(shadow_blocked_ao)
DECLARE_SPLIT_KERNEL_FUNCTION(shadow_blocked_dl)
DECLARE_SPLIT_KERNEL_FUNCTION(enqueue_inactive)
DECLARE_SPLIT_KERNEL_FUNCTION(next_iteration_setup)
DECLARE_SPLIT_KERNEL_FUNCTION(indirect_subsurface)
DECLARE_SPLIT_KERNEL_FUNCTION(buffer_update)

#undef KERNEL_ARCH

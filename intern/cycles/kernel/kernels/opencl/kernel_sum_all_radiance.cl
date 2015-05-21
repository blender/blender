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

#include "split/kernel_sum_all_radiance.h"

__kernel void kernel_ocl_path_trace_sum_all_radiance(
        ccl_constant KernelData *data,               /* To get pass_stride to offet into buffer */
        ccl_global float *buffer,                    /* Output buffer of RenderTile */
        ccl_global float *per_sample_output_buffer,  /* Radiance contributed by all samples */
        int parallel_samples, int sw, int sh, int stride,
        int buffer_offset_x,
        int buffer_offset_y,
        int buffer_stride,
        int start_sample)
{
	kernel_sum_all_radiance(data,
	                        buffer,
	                        per_sample_output_buffer,
	                        parallel_samples,
	                        sw, sh, stride,
	                        buffer_offset_x,
	                        buffer_offset_y,
	                        buffer_stride,
	                        start_sample);
}

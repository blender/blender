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

/* Since we process various samples in parallel; The output radiance of different samples
 * are stored in different locations; This kernel combines the output radiance contributed
 * by all different samples and stores them in the RenderTile's output buffer.
 */

ccl_device void kernel_sum_all_radiance(KernelGlobals *kg)
{
	int x = ccl_global_id(0);
	int y = ccl_global_id(1);

	ccl_global float *buffer = kernel_split_params.buffer;
	int sw = kernel_split_params.w;
	int sh = kernel_split_params.h;
	int stride = kernel_split_params.stride;
	int start_sample = kernel_split_params.start_sample;

	if(x < sw && y < sh) {
		ccl_global float *per_sample_output_buffer = kernel_split_state.per_sample_output_buffers;
		per_sample_output_buffer += (x + y * stride) * (kernel_data.film.pass_stride);

		x += kernel_split_params.x;
		y += kernel_split_params.y;

		buffer += (kernel_split_params.offset + x + y*stride) * (kernel_data.film.pass_stride);

		int pass_stride_iterator = 0;
		int num_floats = kernel_data.film.pass_stride;

		for(pass_stride_iterator = 0; pass_stride_iterator < num_floats; pass_stride_iterator++) {
			*(buffer + pass_stride_iterator) =
			        (start_sample == 0)
			                ? *(per_sample_output_buffer + pass_stride_iterator)
			                : *(buffer + pass_stride_iterator) + *(per_sample_output_buffer + pass_stride_iterator);
		}
	}
}

CCL_NAMESPACE_END


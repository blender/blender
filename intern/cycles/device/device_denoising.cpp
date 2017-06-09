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

#include "device/device_denoising.h"

#include "kernel/filter/filter_defines.h"

CCL_NAMESPACE_BEGIN

void DenoisingTask::init_from_devicetask(const DeviceTask &task)
{
	radius = task.denoising_radius;
	nlm_k_2 = powf(2.0f, lerp(-5.0f, 3.0f, task.denoising_strength));
	if(task.denoising_relative_pca) {
		pca_threshold = -powf(10.0f, lerp(-8.0f, 0.0f, task.denoising_feature_strength));
	}
	else {
		pca_threshold = powf(10.0f, lerp(-5.0f, 3.0f, task.denoising_feature_strength));
	}

	render_buffer.pass_stride = task.pass_stride;
	render_buffer.denoising_data_offset  = task.pass_denoising_data;
	render_buffer.denoising_clean_offset = task.pass_denoising_clean;

	/* Expand filter_area by radius pixels and clamp the result to the extent of the neighboring tiles */
	rect = make_int4(max(tiles->x[0], filter_area.x - radius),
	                 max(tiles->y[0], filter_area.y - radius),
	                 min(tiles->x[3], filter_area.x + filter_area.z + radius),
	                 min(tiles->y[3], filter_area.y + filter_area.w + radius));
}

void DenoisingTask::tiles_from_rendertiles(RenderTile *rtiles)
{
	tiles = (TilesInfo*) tiles_mem.resize(sizeof(TilesInfo)/sizeof(int));

	device_ptr buffers[9];
	for(int i = 0; i < 9; i++) {
		buffers[i] = rtiles[i].buffer;
		tiles->offsets[i] = rtiles[i].offset;
		tiles->strides[i] = rtiles[i].stride;
	}
	tiles->x[0] = rtiles[3].x;
	tiles->x[1] = rtiles[4].x;
	tiles->x[2] = rtiles[5].x;
	tiles->x[3] = rtiles[5].x + rtiles[5].w;
	tiles->y[0] = rtiles[1].y;
	tiles->y[1] = rtiles[4].y;
	tiles->y[2] = rtiles[7].y;
	tiles->y[3] = rtiles[7].y + rtiles[7].h;

	render_buffer.offset = rtiles[4].offset;
	render_buffer.stride = rtiles[4].stride;
	render_buffer.ptr    = rtiles[4].buffer;

	functions.set_tiles(buffers);
}

bool DenoisingTask::run_denoising()
{
	/* Allocate denoising buffer. */
	buffer.passes = 14;
	buffer.w = align_up(rect.z - rect.x, 4);
	buffer.h = rect.w - rect.y;
	buffer.pass_stride = align_up(buffer.w * buffer.h, divide_up(device->mem_address_alignment(), sizeof(float)));
	buffer.mem.resize(buffer.pass_stride * buffer.passes);
	device->mem_alloc("Denoising Pixel Buffer", buffer.mem, MEM_READ_WRITE);

	device_ptr null_ptr = (device_ptr) 0;

	/* Prefilter shadow feature. */
	{
		device_sub_ptr unfiltered_a   (device, buffer.mem, 0,                    buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr unfiltered_b   (device, buffer.mem, 1*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr sample_var     (device, buffer.mem, 2*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr sample_var_var (device, buffer.mem, 3*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr buffer_var     (device, buffer.mem, 5*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr filtered_var   (device, buffer.mem, 6*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr nlm_temporary_1(device, buffer.mem, 7*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr nlm_temporary_2(device, buffer.mem, 8*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr nlm_temporary_3(device, buffer.mem, 9*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);

		nlm_state.temporary_1_ptr = *nlm_temporary_1;
		nlm_state.temporary_2_ptr = *nlm_temporary_2;
		nlm_state.temporary_3_ptr = *nlm_temporary_3;

		/* Get the A/B unfiltered passes, the combined sample variance, the estimated variance of the sample variance and the buffer variance. */
		functions.divide_shadow(*unfiltered_a, *unfiltered_b, *sample_var, *sample_var_var, *buffer_var);

		/* Smooth the (generally pretty noisy) buffer variance using the spatial information from the sample variance. */
		nlm_state.set_parameters(6, 3, 4.0f, 1.0f);
		functions.non_local_means(*buffer_var, *sample_var, *sample_var_var, *filtered_var);

		/* Reuse memory, the previous data isn't needed anymore. */
		device_ptr filtered_a = *buffer_var,
		           filtered_b = *sample_var;
		/* Use the smoothed variance to filter the two shadow half images using each other for weight calculation. */
		nlm_state.set_parameters(5, 3, 1.0f, 0.25f);
		functions.non_local_means(*unfiltered_a, *unfiltered_b, *filtered_var, filtered_a);
		functions.non_local_means(*unfiltered_b, *unfiltered_a, *filtered_var, filtered_b);

		device_ptr residual_var = *sample_var_var;
		/* Estimate the residual variance between the two filtered halves. */
		functions.combine_halves(filtered_a, filtered_b, null_ptr, residual_var, 2, rect);

		device_ptr final_a = *unfiltered_a,
		           final_b = *unfiltered_b;
		/* Use the residual variance for a second filter pass. */
		nlm_state.set_parameters(4, 2, 1.0f, 0.5f);
		functions.non_local_means(filtered_a, filtered_b, residual_var, final_a);
		functions.non_local_means(filtered_b, filtered_a, residual_var, final_b);

		/* Combine the two double-filtered halves to a final shadow feature. */
		device_sub_ptr shadow_pass(device, buffer.mem, 4*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		functions.combine_halves(final_a, final_b, *shadow_pass, null_ptr, 0, rect);
	}

	/* Prefilter general features. */
	{
		device_sub_ptr unfiltered     (device, buffer.mem,  8*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr variance       (device, buffer.mem,  9*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr nlm_temporary_1(device, buffer.mem, 10*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr nlm_temporary_2(device, buffer.mem, 11*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr nlm_temporary_3(device, buffer.mem, 12*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);

		nlm_state.temporary_1_ptr = *nlm_temporary_1;
		nlm_state.temporary_2_ptr = *nlm_temporary_2;
		nlm_state.temporary_3_ptr = *nlm_temporary_3;

		int mean_from[]     = { 0, 1, 2, 12, 6,  7, 8 };
		int variance_from[] = { 3, 4, 5, 13, 9, 10, 11};
		int pass_to[]       = { 1, 2, 3, 0,  5,  6,  7};
		for(int pass = 0; pass < 7; pass++) {
			device_sub_ptr feature_pass(device, buffer.mem, pass_to[pass]*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
			/* Get the unfiltered pass and its variance from the RenderBuffers. */
			functions.get_feature(mean_from[pass], variance_from[pass], *unfiltered, *variance);
			/* Smooth the pass and store the result in the denoising buffers. */
			nlm_state.set_parameters(2, 2, 1.0f, 0.25f);
			functions.non_local_means(*unfiltered, *unfiltered, *variance, *feature_pass);
		}
	}

	/* Copy color passes. */
	{
		int mean_from[]     = {20, 21, 22};
		int variance_from[] = {23, 24, 25};
		int mean_to[]       = { 8,  9, 10};
		int variance_to[]   = {11, 12, 13};
		int num_color_passes = 3;

		device_only_memory<float> temp_color;
		temp_color.resize(3*buffer.pass_stride);
		device->mem_alloc("Denoising temporary color", temp_color, MEM_READ_WRITE);

		for(int pass = 0; pass < num_color_passes; pass++) {
			device_sub_ptr color_pass(device, temp_color, pass*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
			device_sub_ptr color_var_pass(device, buffer.mem, variance_to[pass]*buffer.pass_stride, buffer.pass_stride, MEM_READ_WRITE);
			functions.get_feature(mean_from[pass], variance_from[pass], *color_pass, *color_var_pass);
		}

		{
			device_sub_ptr depth_pass    (device, buffer.mem,                                 0,   buffer.pass_stride, MEM_READ_WRITE);
			device_sub_ptr color_var_pass(device, buffer.mem, variance_to[0]*buffer.pass_stride, 3*buffer.pass_stride, MEM_READ_WRITE);
			device_sub_ptr output_pass   (device, buffer.mem,     mean_to[0]*buffer.pass_stride, 3*buffer.pass_stride, MEM_READ_WRITE);
			functions.detect_outliers(temp_color.device_pointer, *color_var_pass, *depth_pass, *output_pass);
		}

		device->mem_free(temp_color);
	}

	storage.w = filter_area.z;
	storage.h = filter_area.w;
	storage.transform.resize(storage.w*storage.h*TRANSFORM_SIZE);
	storage.rank.resize(storage.w*storage.h);
	device->mem_alloc("Denoising Transform", storage.transform, MEM_READ_WRITE);
	device->mem_alloc("Denoising Rank", storage.rank, MEM_READ_WRITE);

	functions.construct_transform();

	device_only_memory<float> temporary_1;
	device_only_memory<float> temporary_2;
	temporary_1.resize(buffer.w*buffer.h);
	temporary_2.resize(buffer.w*buffer.h);
	device->mem_alloc("Denoising NLM temporary 1", temporary_1, MEM_READ_WRITE);
	device->mem_alloc("Denoising NLM temporary 2", temporary_2, MEM_READ_WRITE);
	reconstruction_state.temporary_1_ptr = temporary_1.device_pointer;
	reconstruction_state.temporary_2_ptr = temporary_2.device_pointer;

	storage.XtWX.resize(storage.w*storage.h*XTWX_SIZE);
	storage.XtWY.resize(storage.w*storage.h*XTWY_SIZE);
	device->mem_alloc("Denoising XtWX", storage.XtWX, MEM_READ_WRITE);
	device->mem_alloc("Denoising XtWY", storage.XtWY, MEM_READ_WRITE);

	reconstruction_state.filter_rect = make_int4(filter_area.x-rect.x, filter_area.y-rect.y, storage.w, storage.h);
	int tile_coordinate_offset = filter_area.y*render_buffer.stride + filter_area.x;
	reconstruction_state.buffer_params = make_int4(render_buffer.offset + tile_coordinate_offset,
	                                               render_buffer.stride,
	                                               render_buffer.pass_stride,
	                                               render_buffer.denoising_clean_offset);
	reconstruction_state.source_w = rect.z-rect.x;
	reconstruction_state.source_h = rect.w-rect.y;

	{
		device_sub_ptr color_ptr    (device, buffer.mem,  8*buffer.pass_stride, 3*buffer.pass_stride, MEM_READ_WRITE);
		device_sub_ptr color_var_ptr(device, buffer.mem, 11*buffer.pass_stride, 3*buffer.pass_stride, MEM_READ_WRITE);
		functions.reconstruct(*color_ptr, *color_var_ptr, render_buffer.ptr);
	}

	device->mem_free(storage.XtWX);
	device->mem_free(storage.XtWY);
	device->mem_free(storage.transform);
	device->mem_free(storage.rank);
	device->mem_free(temporary_1);
	device->mem_free(temporary_2);
	device->mem_free(buffer.mem);
	device->mem_free(tiles_mem);
	return true;
}

CCL_NAMESPACE_END

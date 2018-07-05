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

DenoisingTask::DenoisingTask(Device *device, const DeviceTask &task)
: tile_info_mem(device, "denoising tile info mem", MEM_READ_WRITE),
  storage(device),
  buffer(device),
  device(device)
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
	render_buffer.offset = task.pass_denoising_data;

	target_buffer.pass_stride = task.pass_stride;
	target_buffer.denoising_clean_offset = task.pass_denoising_clean;

	functions.map_neighbor_tiles = function_bind(task.map_neighbor_tiles, _1, device);
	functions.unmap_neighbor_tiles = function_bind(task.unmap_neighbor_tiles, _1, device);
}

DenoisingTask::~DenoisingTask()
{
	storage.XtWX.free();
	storage.XtWY.free();
	storage.transform.free();
	storage.rank.free();
	storage.temporary_1.free();
	storage.temporary_2.free();
	storage.temporary_color.free();
	buffer.mem.free();
	tile_info_mem.free();
}

void DenoisingTask::set_render_buffer(RenderTile *rtiles)
{
	tile_info = (TileInfo*) tile_info_mem.alloc(sizeof(TileInfo)/sizeof(int));

	for(int i = 0; i < 9; i++) {
		tile_info->offsets[i] = rtiles[i].offset;
		tile_info->strides[i] = rtiles[i].stride;
		tile_info->buffers[i] = rtiles[i].buffer;
	}
	tile_info->x[0] = rtiles[3].x;
	tile_info->x[1] = rtiles[4].x;
	tile_info->x[2] = rtiles[5].x;
	tile_info->x[3] = rtiles[5].x + rtiles[5].w;
	tile_info->y[0] = rtiles[1].y;
	tile_info->y[1] = rtiles[4].y;
	tile_info->y[2] = rtiles[7].y;
	tile_info->y[3] = rtiles[7].y + rtiles[7].h;

	target_buffer.offset = rtiles[9].offset;
	target_buffer.stride = rtiles[9].stride;
	target_buffer.ptr    = rtiles[9].buffer;

	tile_info_mem.copy_to_device();
}

void DenoisingTask::setup_denoising_buffer()
{
	/* Expand filter_area by radius pixels and clamp the result to the extent of the neighboring tiles */
	rect = rect_from_shape(filter_area.x, filter_area.y, filter_area.z, filter_area.w);
	rect = rect_expand(rect, radius);
	rect = rect_clip(rect, make_int4(tile_info->x[0], tile_info->y[0], tile_info->x[3], tile_info->y[3]));

	buffer.passes = 14;
	buffer.width = rect.z - rect.x;
	buffer.stride = align_up(buffer.width, 4);
	buffer.h = rect.w - rect.y;
	int alignment_floats = divide_up(device->mem_sub_ptr_alignment(), sizeof(float));
	buffer.pass_stride = align_up(buffer.stride * buffer.h, alignment_floats);
	/* Pad the total size by four floats since the SIMD kernels might go a bit over the end. */
	int mem_size = align_up(buffer.pass_stride * buffer.passes + 4, alignment_floats);
	buffer.mem.alloc_to_device(mem_size, false);
}

void DenoisingTask::prefilter_shadowing()
{
	device_ptr null_ptr = (device_ptr) 0;

	device_sub_ptr unfiltered_a   (buffer.mem, 0,                    buffer.pass_stride);
	device_sub_ptr unfiltered_b   (buffer.mem, 1*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr sample_var     (buffer.mem, 2*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr sample_var_var (buffer.mem, 3*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr buffer_var     (buffer.mem, 5*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr filtered_var   (buffer.mem, 6*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_1(buffer.mem, 7*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_2(buffer.mem, 8*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_3(buffer.mem, 9*buffer.pass_stride, buffer.pass_stride);

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
	device_sub_ptr shadow_pass(buffer.mem, 4*buffer.pass_stride, buffer.pass_stride);
	functions.combine_halves(final_a, final_b, *shadow_pass, null_ptr, 0, rect);
}

void DenoisingTask::prefilter_features()
{
	device_sub_ptr unfiltered     (buffer.mem,  8*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr variance       (buffer.mem,  9*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_1(buffer.mem, 10*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_2(buffer.mem, 11*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_3(buffer.mem, 12*buffer.pass_stride, buffer.pass_stride);

	nlm_state.temporary_1_ptr = *nlm_temporary_1;
	nlm_state.temporary_2_ptr = *nlm_temporary_2;
	nlm_state.temporary_3_ptr = *nlm_temporary_3;

	int mean_from[]     = { 0, 1, 2, 12, 6,  7, 8 };
	int variance_from[] = { 3, 4, 5, 13, 9, 10, 11};
	int pass_to[]       = { 1, 2, 3, 0,  5,  6,  7};
	for(int pass = 0; pass < 7; pass++) {
		device_sub_ptr feature_pass(buffer.mem, pass_to[pass]*buffer.pass_stride, buffer.pass_stride);
		/* Get the unfiltered pass and its variance from the RenderBuffers. */
		functions.get_feature(mean_from[pass], variance_from[pass], *unfiltered, *variance);
		/* Smooth the pass and store the result in the denoising buffers. */
		nlm_state.set_parameters(2, 2, 1.0f, 0.25f);
		functions.non_local_means(*unfiltered, *unfiltered, *variance, *feature_pass);
	}
}

void DenoisingTask::prefilter_color()
{
	int mean_from[]     = {20, 21, 22};
	int variance_from[] = {23, 24, 25};
	int mean_to[]       = { 8,  9, 10};
	int variance_to[]   = {11, 12, 13};
	int num_color_passes = 3;

	storage.temporary_color.alloc_to_device(3*buffer.pass_stride, false);
	device_sub_ptr nlm_temporary_1(storage.temporary_color, 0*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_2(storage.temporary_color, 1*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_3(storage.temporary_color, 2*buffer.pass_stride, buffer.pass_stride);

	nlm_state.temporary_1_ptr = *nlm_temporary_1;
	nlm_state.temporary_2_ptr = *nlm_temporary_2;
	nlm_state.temporary_3_ptr = *nlm_temporary_3;

	for(int pass = 0; pass < num_color_passes; pass++) {
		device_sub_ptr color_pass(storage.temporary_color, pass*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr color_var_pass(buffer.mem, variance_to[pass]*buffer.pass_stride, buffer.pass_stride);
		functions.get_feature(mean_from[pass], variance_from[pass], *color_pass, *color_var_pass);
	}

	device_sub_ptr depth_pass    (buffer.mem,                                 0,   buffer.pass_stride);
	device_sub_ptr color_var_pass(buffer.mem, variance_to[0]*buffer.pass_stride, 3*buffer.pass_stride);
	device_sub_ptr output_pass   (buffer.mem,     mean_to[0]*buffer.pass_stride, 3*buffer.pass_stride);
	functions.detect_outliers(storage.temporary_color.device_pointer, *color_var_pass, *depth_pass, *output_pass);

	storage.temporary_color.free();
}

void DenoisingTask::construct_transform()
{
	storage.w = filter_area.z;
	storage.h = filter_area.w;

	storage.transform.alloc_to_device(storage.w*storage.h*TRANSFORM_SIZE, false);
	storage.rank.alloc_to_device(storage.w*storage.h, false);

	functions.construct_transform();
}

void DenoisingTask::reconstruct()
{

	device_only_memory<float> temporary_1(device, "Denoising NLM temporary 1");
	device_only_memory<float> temporary_2(device, "Denoising NLM temporary 2");
	temporary_1.alloc_to_device(buffer.pass_stride, false);
	temporary_2.alloc_to_device(buffer.pass_stride, false);
	reconstruction_state.temporary_1_ptr = temporary_1.device_pointer;
	reconstruction_state.temporary_2_ptr = temporary_2.device_pointer;

	storage.XtWX.alloc_to_device(storage.w*storage.h*XTWX_SIZE, false);
	storage.XtWY.alloc_to_device(storage.w*storage.h*XTWY_SIZE, false);

	reconstruction_state.filter_window = rect_from_shape(filter_area.x-rect.x, filter_area.y-rect.y, storage.w, storage.h);
	int tile_coordinate_offset = filter_area.y*target_buffer.stride + filter_area.x;
	reconstruction_state.buffer_params = make_int4(target_buffer.offset + tile_coordinate_offset,
	                                               target_buffer.stride,
	                                               target_buffer.pass_stride,
	                                               target_buffer.denoising_clean_offset);
	reconstruction_state.source_w = rect.z-rect.x;
	reconstruction_state.source_h = rect.w-rect.y;

	device_sub_ptr color_ptr    (buffer.mem,  8*buffer.pass_stride, 3*buffer.pass_stride);
	device_sub_ptr color_var_ptr(buffer.mem, 11*buffer.pass_stride, 3*buffer.pass_stride);
	functions.reconstruct(*color_ptr, *color_var_ptr, target_buffer.ptr);
}

void DenoisingTask::run_denoising(RenderTile *tile)
{
	RenderTile rtiles[10];
	rtiles[4] = *tile;
	functions.map_neighbor_tiles(rtiles);
	set_render_buffer(rtiles);

	setup_denoising_buffer();

	prefilter_shadowing();
	prefilter_features();
	prefilter_color();

	construct_transform();
	reconstruct();

	functions.unmap_neighbor_tiles(rtiles);
}

CCL_NAMESPACE_END

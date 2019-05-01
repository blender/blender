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
      profiler(NULL),
      storage(device),
      buffer(device),
      device(device)
{
  radius = task.denoising.radius;
  nlm_k_2 = powf(2.0f, lerp(-5.0f, 3.0f, task.denoising.strength));
  if (task.denoising.relative_pca) {
    pca_threshold = -powf(10.0f, lerp(-8.0f, 0.0f, task.denoising.feature_strength));
  }
  else {
    pca_threshold = powf(10.0f, lerp(-5.0f, 3.0f, task.denoising.feature_strength));
  }

  render_buffer.frame_stride = task.frame_stride;
  render_buffer.pass_stride = task.pass_stride;
  render_buffer.offset = task.pass_denoising_data;

  target_buffer.pass_stride = task.target_pass_stride;
  target_buffer.denoising_clean_offset = task.pass_denoising_clean;
  target_buffer.offset = 0;

  functions.map_neighbor_tiles = function_bind(task.map_neighbor_tiles, _1, device);
  functions.unmap_neighbor_tiles = function_bind(task.unmap_neighbor_tiles, _1, device);

  tile_info = (TileInfo *)tile_info_mem.alloc(sizeof(TileInfo) / sizeof(int));
  tile_info->from_render = task.denoising_from_render ? 1 : 0;

  tile_info->frames[0] = 0;
  tile_info->num_frames = min(task.denoising_frames.size() + 1, DENOISE_MAX_FRAMES);
  for (int i = 1; i < tile_info->num_frames; i++) {
    tile_info->frames[i] = task.denoising_frames[i - 1];
  }

  write_passes = task.denoising_write_passes;
  do_filter = task.denoising_do_filter;
}

DenoisingTask::~DenoisingTask()
{
  storage.XtWX.free();
  storage.XtWY.free();
  storage.transform.free();
  storage.rank.free();
  buffer.mem.free();
  buffer.temporary_mem.free();
  tile_info_mem.free();
}

void DenoisingTask::set_render_buffer(RenderTile *rtiles)
{
  for (int i = 0; i < 9; i++) {
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
  target_buffer.ptr = rtiles[9].buffer;

  if (write_passes && rtiles[9].buffers) {
    target_buffer.denoising_output_offset =
        rtiles[9].buffers->params.get_denoising_prefiltered_offset();
  }
  else {
    target_buffer.denoising_output_offset = 0;
  }

  tile_info_mem.copy_to_device();
}

void DenoisingTask::setup_denoising_buffer()
{
  /* Expand filter_area by radius pixels and clamp the result to the extent of the neighboring
   * tiles */
  rect = rect_from_shape(filter_area.x, filter_area.y, filter_area.z, filter_area.w);
  rect = rect_expand(rect, radius);
  rect = rect_clip(rect,
                   make_int4(tile_info->x[0], tile_info->y[0], tile_info->x[3], tile_info->y[3]));

  buffer.use_intensity = write_passes || (tile_info->num_frames > 1);
  buffer.passes = buffer.use_intensity ? 15 : 14;
  buffer.width = rect.z - rect.x;
  buffer.stride = align_up(buffer.width, 4);
  buffer.h = rect.w - rect.y;
  int alignment_floats = divide_up(device->mem_sub_ptr_alignment(), sizeof(float));
  buffer.pass_stride = align_up(buffer.stride * buffer.h, alignment_floats);
  buffer.frame_stride = buffer.pass_stride * buffer.passes;
  /* Pad the total size by four floats since the SIMD kernels might go a bit over the end. */
  int mem_size = align_up(tile_info->num_frames * buffer.frame_stride + 4, alignment_floats);
  buffer.mem.alloc_to_device(mem_size, false);
  buffer.use_time = (tile_info->num_frames > 1);

  /* CPUs process shifts sequentially while GPUs process them in parallel. */
  int num_layers;
  if (buffer.gpu_temporary_mem) {
    /* Shadowing prefiltering uses a radius of 6, so allocate at least that much. */
    int max_radius = max(radius, 6);
    int num_shifts = (2 * max_radius + 1) * (2 * max_radius + 1);
    num_layers = 2 * num_shifts + 1;
  }
  else {
    num_layers = 3;
  }
  /* Allocate two layers per shift as well as one for the weight accumulation. */
  buffer.temporary_mem.alloc_to_device(num_layers * buffer.pass_stride);
}

void DenoisingTask::prefilter_shadowing()
{
  device_ptr null_ptr = (device_ptr)0;

  device_sub_ptr unfiltered_a(buffer.mem, 0, buffer.pass_stride);
  device_sub_ptr unfiltered_b(buffer.mem, 1 * buffer.pass_stride, buffer.pass_stride);
  device_sub_ptr sample_var(buffer.mem, 2 * buffer.pass_stride, buffer.pass_stride);
  device_sub_ptr sample_var_var(buffer.mem, 3 * buffer.pass_stride, buffer.pass_stride);
  device_sub_ptr buffer_var(buffer.mem, 5 * buffer.pass_stride, buffer.pass_stride);
  device_sub_ptr filtered_var(buffer.mem, 6 * buffer.pass_stride, buffer.pass_stride);

  /* Get the A/B unfiltered passes, the combined sample variance, the estimated variance of the
   * sample variance and the buffer variance. */
  functions.divide_shadow(*unfiltered_a, *unfiltered_b, *sample_var, *sample_var_var, *buffer_var);

  /* Smooth the (generally pretty noisy) buffer variance using the spatial information from the
   * sample variance. */
  nlm_state.set_parameters(6, 3, 4.0f, 1.0f, false);
  functions.non_local_means(*buffer_var, *sample_var, *sample_var_var, *filtered_var);

  /* Reuse memory, the previous data isn't needed anymore. */
  device_ptr filtered_a = *buffer_var, filtered_b = *sample_var;
  /* Use the smoothed variance to filter the two shadow half images using each other for weight
   * calculation. */
  nlm_state.set_parameters(5, 3, 1.0f, 0.25f, false);
  functions.non_local_means(*unfiltered_a, *unfiltered_b, *filtered_var, filtered_a);
  functions.non_local_means(*unfiltered_b, *unfiltered_a, *filtered_var, filtered_b);

  device_ptr residual_var = *sample_var_var;
  /* Estimate the residual variance between the two filtered halves. */
  functions.combine_halves(filtered_a, filtered_b, null_ptr, residual_var, 2, rect);

  device_ptr final_a = *unfiltered_a, final_b = *unfiltered_b;
  /* Use the residual variance for a second filter pass. */
  nlm_state.set_parameters(4, 2, 1.0f, 0.5f, false);
  functions.non_local_means(filtered_a, filtered_b, residual_var, final_a);
  functions.non_local_means(filtered_b, filtered_a, residual_var, final_b);

  /* Combine the two double-filtered halves to a final shadow feature. */
  device_sub_ptr shadow_pass(buffer.mem, 4 * buffer.pass_stride, buffer.pass_stride);
  functions.combine_halves(final_a, final_b, *shadow_pass, null_ptr, 0, rect);
}

void DenoisingTask::prefilter_features()
{
  device_sub_ptr unfiltered(buffer.mem, 8 * buffer.pass_stride, buffer.pass_stride);
  device_sub_ptr variance(buffer.mem, 9 * buffer.pass_stride, buffer.pass_stride);

  int mean_from[] = {0, 1, 2, 12, 6, 7, 8};
  int variance_from[] = {3, 4, 5, 13, 9, 10, 11};
  int pass_to[] = {1, 2, 3, 0, 5, 6, 7};
  for (int pass = 0; pass < 7; pass++) {
    device_sub_ptr feature_pass(
        buffer.mem, pass_to[pass] * buffer.pass_stride, buffer.pass_stride);
    /* Get the unfiltered pass and its variance from the RenderBuffers. */
    functions.get_feature(mean_from[pass],
                          variance_from[pass],
                          *unfiltered,
                          *variance,
                          1.0f / render_buffer.samples);
    /* Smooth the pass and store the result in the denoising buffers. */
    nlm_state.set_parameters(2, 2, 1.0f, 0.25f, false);
    functions.non_local_means(*unfiltered, *unfiltered, *variance, *feature_pass);
  }
}

void DenoisingTask::prefilter_color()
{
  int mean_from[] = {20, 21, 22};
  int variance_from[] = {23, 24, 25};
  int mean_to[] = {8, 9, 10};
  int variance_to[] = {11, 12, 13};
  int num_color_passes = 3;

  device_only_memory<float> temporary_color(device, "denoising temporary color");
  temporary_color.alloc_to_device(3 * buffer.pass_stride, false);

  for (int pass = 0; pass < num_color_passes; pass++) {
    device_sub_ptr color_pass(temporary_color, pass * buffer.pass_stride, buffer.pass_stride);
    device_sub_ptr color_var_pass(
        buffer.mem, variance_to[pass] * buffer.pass_stride, buffer.pass_stride);
    functions.get_feature(mean_from[pass],
                          variance_from[pass],
                          *color_pass,
                          *color_var_pass,
                          1.0f / render_buffer.samples);
  }

  device_sub_ptr depth_pass(buffer.mem, 0, buffer.pass_stride);
  device_sub_ptr color_var_pass(
      buffer.mem, variance_to[0] * buffer.pass_stride, 3 * buffer.pass_stride);
  device_sub_ptr output_pass(buffer.mem, mean_to[0] * buffer.pass_stride, 3 * buffer.pass_stride);
  functions.detect_outliers(
      temporary_color.device_pointer, *color_var_pass, *depth_pass, *output_pass);

  if (buffer.use_intensity) {
    device_sub_ptr intensity_pass(buffer.mem, 14 * buffer.pass_stride, buffer.pass_stride);
    nlm_state.set_parameters(radius, 4, 2.0f, nlm_k_2 * 4.0f, true);
    functions.non_local_means(*output_pass, *output_pass, *color_var_pass, *intensity_pass);
  }
}

void DenoisingTask::load_buffer()
{
  device_ptr null_ptr = (device_ptr)0;

  int original_offset = render_buffer.offset;

  int num_passes = buffer.use_intensity ? 15 : 14;
  for (int i = 0; i < tile_info->num_frames; i++) {
    for (int pass = 0; pass < num_passes; pass++) {
      device_sub_ptr to_pass(
          buffer.mem, i * buffer.frame_stride + pass * buffer.pass_stride, buffer.pass_stride);
      bool is_variance = (pass >= 11) && (pass <= 13);
      functions.get_feature(
          pass, -1, *to_pass, null_ptr, is_variance ? (1.0f / render_buffer.samples) : 1.0f);
    }
    render_buffer.offset += render_buffer.frame_stride;
  }

  render_buffer.offset = original_offset;
}

void DenoisingTask::write_buffer()
{
  reconstruction_state.buffer_params = make_int4(target_buffer.offset,
                                                 target_buffer.stride,
                                                 target_buffer.pass_stride,
                                                 target_buffer.denoising_clean_offset);
  int num_passes = buffer.use_intensity ? 15 : 14;
  for (int pass = 0; pass < num_passes; pass++) {
    device_sub_ptr from_pass(buffer.mem, pass * buffer.pass_stride, buffer.pass_stride);
    int out_offset = pass + target_buffer.denoising_output_offset;
    functions.write_feature(out_offset, *from_pass, target_buffer.ptr);
  }
}

void DenoisingTask::construct_transform()
{
  storage.w = filter_area.z;
  storage.h = filter_area.w;

  storage.transform.alloc_to_device(storage.w * storage.h * TRANSFORM_SIZE, false);
  storage.rank.alloc_to_device(storage.w * storage.h, false);

  functions.construct_transform();
}

void DenoisingTask::reconstruct()
{
  storage.XtWX.alloc_to_device(storage.w * storage.h * XTWX_SIZE, false);
  storage.XtWY.alloc_to_device(storage.w * storage.h * XTWY_SIZE, false);
  storage.XtWX.zero_to_device();
  storage.XtWY.zero_to_device();

  reconstruction_state.filter_window = rect_from_shape(
      filter_area.x - rect.x, filter_area.y - rect.y, storage.w, storage.h);
  int tile_coordinate_offset = filter_area.y * target_buffer.stride + filter_area.x;
  reconstruction_state.buffer_params = make_int4(target_buffer.offset + tile_coordinate_offset,
                                                 target_buffer.stride,
                                                 target_buffer.pass_stride,
                                                 target_buffer.denoising_clean_offset);
  reconstruction_state.source_w = rect.z - rect.x;
  reconstruction_state.source_h = rect.w - rect.y;

  device_sub_ptr color_ptr(buffer.mem, 8 * buffer.pass_stride, 3 * buffer.pass_stride);
  device_sub_ptr color_var_ptr(buffer.mem, 11 * buffer.pass_stride, 3 * buffer.pass_stride);
  for (int f = 0; f < tile_info->num_frames; f++) {
    device_ptr scale_ptr = 0;
    device_sub_ptr *scale_sub_ptr = NULL;
    if (tile_info->frames[f] != 0 && (tile_info->num_frames > 1)) {
      scale_sub_ptr = new device_sub_ptr(buffer.mem, 14 * buffer.pass_stride, buffer.pass_stride);
      scale_ptr = **scale_sub_ptr;
    }

    functions.accumulate(*color_ptr, *color_var_ptr, scale_ptr, f);
    delete scale_sub_ptr;
  }
  functions.solve(target_buffer.ptr);
}

void DenoisingTask::run_denoising(RenderTile *tile)
{
  RenderTile rtiles[10];
  rtiles[4] = *tile;
  functions.map_neighbor_tiles(rtiles);
  set_render_buffer(rtiles);

  setup_denoising_buffer();

  if (tile_info->from_render) {
    prefilter_shadowing();
    prefilter_features();
    prefilter_color();
  }
  else {
    load_buffer();
  }

  if (do_filter) {
    construct_transform();
    reconstruct();
  }

  if (write_passes) {
    write_buffer();
  }

  functions.unmap_neighbor_tiles(rtiles);
}

CCL_NAMESPACE_END

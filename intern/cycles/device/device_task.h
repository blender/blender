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

#ifndef __DEVICE_TASK_H__
#define __DEVICE_TASK_H__

#include "device/device_memory.h"

#include "util/util_function.h"
#include "util/util_list.h"
#include "util/util_task.h"

CCL_NAMESPACE_BEGIN

/* Device Task */

class Device;
class RenderBuffers;
class RenderTile;
class Tile;

class DenoiseParams {
 public:
  /* Pixel radius for neighboring pixels to take into account. */
  int radius;
  /* Controls neighbor pixel weighting for the denoising filter. */
  float strength;
  /* Preserve more or less detail based on feature passes. */
  float feature_strength;
  /* When removing pixels that don't carry information,
   * use a relative threshold instead of an absolute one. */
  bool relative_pca;
  /* How many frames before and after the current center frame are included. */
  int neighbor_frames;
  /* Clamp the input to the range of +-1e8. Should be enough for any legitimate data. */
  bool clamp_input;

  DenoiseParams()
  {
    radius = 8;
    strength = 0.5f;
    feature_strength = 0.5f;
    relative_pca = false;
    neighbor_frames = 2;
    clamp_input = true;
  }
};

class DeviceTask : public Task {
 public:
  typedef enum { RENDER, FILM_CONVERT, SHADER } Type;
  Type type;

  int x, y, w, h;
  device_ptr rgba_byte;
  device_ptr rgba_half;
  device_ptr buffer;
  int sample;
  int num_samples;
  int offset, stride;

  device_ptr shader_input;
  device_ptr shader_output;
  int shader_eval_type;
  int shader_filter;
  int shader_x, shader_w;

  int passes_size;

  explicit DeviceTask(Type type = RENDER);

  int get_subtask_count(int num, int max_size = 0);
  void split(list<DeviceTask> &tasks, int num, int max_size = 0);

  void update_progress(RenderTile *rtile, int pixel_samples = -1);

  function<bool(Device *device, RenderTile &)> acquire_tile;
  function<void(long, int)> update_progress_sample;
  function<void(RenderTile &)> update_tile_sample;
  function<void(RenderTile &)> release_tile;
  function<bool()> get_cancel;
  function<void(RenderTile *, Device *)> map_neighbor_tiles;
  function<void(RenderTile *, Device *)> unmap_neighbor_tiles;

  DenoiseParams denoising;
  bool denoising_from_render;
  vector<int> denoising_frames;

  bool denoising_do_filter;
  bool denoising_write_passes;

  int pass_stride;
  int frame_stride;
  int target_pass_stride;
  int pass_denoising_data;
  int pass_denoising_clean;

  bool need_finish_queue;
  bool integrator_branched;
  int2 requested_tile_size;

 protected:
  double last_update_time;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_TASK_H__ */

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

CCL_NAMESPACE_BEGIN

/* Device Task */

class Device;
class RenderBuffers;
class RenderTile;
class Tile;

enum DenoiserType {
  DENOISER_NLM = 1,
  DENOISER_OPTIX = 2,
  DENOISER_NUM,

  DENOISER_NONE = 0,
  DENOISER_ALL = ~0,
};

typedef int DenoiserTypeMask;

class DenoiseParams {
 public:
  /* Apply denoiser to image. */
  bool use;
  /* Output denoising data passes (possibly without applying the denoiser). */
  bool store_passes;

  /* Denoiser type. */
  DenoiserType type;

  /* Viewport start sample. */
  int start_sample;

  /** Native Denoiser **/

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

  /** Optix Denoiser **/

  /* Passes handed over to the OptiX denoiser (default to color + albedo). */
  int optix_input_passes;

  DenoiseParams()
  {
    use = false;
    store_passes = false;

    type = DENOISER_NLM;

    radius = 8;
    strength = 0.5f;
    feature_strength = 0.5f;
    relative_pca = false;
    neighbor_frames = 2;
    clamp_input = true;

    optix_input_passes = 2;

    start_sample = 0;
  }

  /* Test if a denoising task needs to run, also to prefilter passes for the native
   * denoiser when we are not applying denoising to the combined image. */
  bool need_denoising_task() const
  {
    return (use || (store_passes && type == DENOISER_NLM));
  }
};

class AdaptiveSampling {
 public:
  AdaptiveSampling();

  int align_static_samples(int samples) const;
  int align_dynamic_samples(int offset, int samples) const;
  bool need_filter(int sample) const;

  bool use;
  int adaptive_step;
  int min_samples;
};

class DeviceTask {
 public:
  typedef enum { RENDER, FILM_CONVERT, SHADER, DENOISE_BUFFER } Type;
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

  RenderBuffers *buffers;

  explicit DeviceTask(Type type = RENDER);

  int get_subtask_count(int num, int max_size = 0) const;
  void split(list<DeviceTask> &tasks, int num, int max_size = 0) const;

  void update_progress(RenderTile *rtile, int pixel_samples = -1);

  function<bool(Device *device, RenderTile &, uint)> acquire_tile;
  function<void(long, int)> update_progress_sample;
  function<void(RenderTile &)> update_tile_sample;
  function<void(RenderTile &)> release_tile;
  function<bool()> get_cancel;
  function<void(RenderTile *, Device *)> map_neighbor_tiles;
  function<void(RenderTile *, Device *)> unmap_neighbor_tiles;

  uint tile_types;
  DenoiseParams denoising;
  bool denoising_from_render;
  vector<int> denoising_frames;

  bool denoising_do_filter;
  bool denoising_do_prefilter;

  int pass_stride;
  int frame_stride;
  int target_pass_stride;
  int pass_denoising_data;
  int pass_denoising_clean;

  bool need_finish_queue;
  bool integrator_branched;
  AdaptiveSampling adaptive_sampling;

 protected:
  double last_update_time;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_TASK_H__ */

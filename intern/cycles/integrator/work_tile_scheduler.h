/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

#include "integrator/tile.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class BufferParams;

struct KernelWorkTile;

/* Scheduler of device work tiles.
 * Takes care of feeding multiple devices running in parallel a work which needs to be done. */
class WorkTileScheduler {
 public:
  WorkTileScheduler();

  /* MAximum path states which are allowed to be used by a single scheduled work tile.
   *
   * Affects the scheduled work size: the work size will be as big as possible, but will not exceed
   * this number of states. */
  void set_max_num_path_states(int max_num_path_states);

  /* Scheduling will happen for pixels within a big tile denotes by its parameters. */
  void reset(const BufferParams &buffer_params,
             int sample_start,
             int samples_num,
             float scrambling_distance);

  /* Get work for a device.
   * Returns true if there is still work to be done and initialize the work tile to all
   * parameters of this work. If there is nothing remaining to be done, returns false and the
   * work tile is kept unchanged.
   *
   * Optionally pass max_work_size to do nothing if there is no tile small enough. */
  bool get_work(KernelWorkTile *work_tile, const int max_work_size = 0);

 protected:
  void reset_scheduler_state();

  /* Maximum allowed path states to be used.
   *
   * TODO(sergey): Naming can be improved. The fact that this is a limiting factor based on the
   * number of path states is kind of a detail. Is there a more generic term from the scheduler
   * point of view? */
  int max_num_path_states_ = 0;

  /* Offset in pixels within a global buffer. */
  int2 image_full_offset_px_ = make_int2(0, 0);

  /* dimensions of the currently rendering image in pixels. */
  int2 image_size_px_ = make_int2(0, 0);

  /* Offset and stride of the buffer within which scheduling is happening.
   * Will be passed over to the KernelWorkTile. */
  int offset_, stride_;

  /* Scrambling Distance requires adapted tile size */
  float scrambling_distance_;

  /* Start sample of index and number of samples which are to be rendered.
   * The scheduler will cover samples range of [start, start + num] over the entire image
   * (splitting into a smaller work tiles). */
  int sample_start_ = 0;
  int samples_num_ = 0;

  /* Tile size which be scheduled for rendering. */
  TileSize tile_size_;

  /* Number of tiles in X and Y axis of the image. */
  int num_tiles_x_, num_tiles_y_;

  /* Total number of tiles on the image.
   * Pre-calculated as `num_tiles_x_ * num_tiles_y_` and re-used in the `get_work()`.
   *
   * TODO(sergey): Is this an over-optimization? Maybe it's unmeasurable to calculate the value
   * in the `get_work()`? */
  int total_tiles_num_ = 0;

  /* In the case when the number of samples in the `tile_size_` is lower than samples_num_ denotes
   * how many tiles are to be "stacked" to cover the entire requested range of samples. */
  int num_tiles_per_sample_range_ = 0;

  int next_work_index_ = 0;
  int total_work_size_ = 0;
};

CCL_NAMESPACE_END

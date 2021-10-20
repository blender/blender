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

#include "integrator/work_tile_scheduler.h"

#include "device/device_queue.h"
#include "integrator/tile.h"
#include "render/buffers.h"
#include "util/util_atomic.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

WorkTileScheduler::WorkTileScheduler()
{
}

void WorkTileScheduler::set_max_num_path_states(int max_num_path_states)
{
  max_num_path_states_ = max_num_path_states;
}

void WorkTileScheduler::reset(const BufferParams &buffer_params, int sample_start, int samples_num)
{
  /* Image buffer parameters. */
  image_full_offset_px_.x = buffer_params.full_x;
  image_full_offset_px_.y = buffer_params.full_y;

  image_size_px_ = make_int2(buffer_params.width, buffer_params.height);

  offset_ = buffer_params.offset;
  stride_ = buffer_params.stride;

  /* Samples parameters. */
  sample_start_ = sample_start;
  samples_num_ = samples_num;

  /* Initialize new scheduling. */
  reset_scheduler_state();
}

void WorkTileScheduler::reset_scheduler_state()
{
  tile_size_ = tile_calculate_best_size(image_size_px_, samples_num_, max_num_path_states_);

  VLOG(3) << "Will schedule tiles of size " << tile_size_;

  if (VLOG_IS_ON(3)) {
    /* The logging is based on multiple tiles scheduled, ignoring overhead of multi-tile scheduling
     * and purely focusing on the number of used path states. */
    const int num_path_states_in_tile = tile_size_.width * tile_size_.height *
                                        tile_size_.num_samples;
    const int num_tiles = max_num_path_states_ / num_path_states_in_tile;
    VLOG(3) << "Number of unused path states: "
            << max_num_path_states_ - num_tiles * num_path_states_in_tile;
  }

  num_tiles_x_ = divide_up(image_size_px_.x, tile_size_.width);
  num_tiles_y_ = divide_up(image_size_px_.y, tile_size_.height);

  total_tiles_num_ = num_tiles_x_ * num_tiles_y_;
  num_tiles_per_sample_range_ = divide_up(samples_num_, tile_size_.num_samples);

  next_work_index_ = 0;
  total_work_size_ = total_tiles_num_ * num_tiles_per_sample_range_;
}

bool WorkTileScheduler::get_work(KernelWorkTile *work_tile_, const int max_work_size)
{
  /* Note that the `max_work_size` can be higher than the `max_num_path_states_`: this is because
   * the path trace work can decide to use smaller tile sizes and greedily schedule multiple tiles,
   * improving overall device occupancy.
   * So the `max_num_path_states_` is a "scheduling unit", and the `max_work_size` is a "scheduling
   * limit". */

  DCHECK_NE(max_num_path_states_, 0);

  const int work_index = next_work_index_++;
  if (work_index >= total_work_size_) {
    return false;
  }

  const int sample_range_index = work_index % num_tiles_per_sample_range_;
  const int start_sample = sample_range_index * tile_size_.num_samples;
  const int tile_index = work_index / num_tiles_per_sample_range_;
  const int tile_y = tile_index / num_tiles_x_;
  const int tile_x = tile_index - tile_y * num_tiles_x_;

  KernelWorkTile work_tile;
  work_tile.x = tile_x * tile_size_.width;
  work_tile.y = tile_y * tile_size_.height;
  work_tile.w = tile_size_.width;
  work_tile.h = tile_size_.height;
  work_tile.start_sample = sample_start_ + start_sample;
  work_tile.num_samples = min(tile_size_.num_samples, samples_num_ - start_sample);
  work_tile.offset = offset_;
  work_tile.stride = stride_;

  work_tile.w = min(work_tile.w, image_size_px_.x - work_tile.x);
  work_tile.h = min(work_tile.h, image_size_px_.y - work_tile.y);

  work_tile.x += image_full_offset_px_.x;
  work_tile.y += image_full_offset_px_.y;

  const int tile_work_size = work_tile.w * work_tile.h * work_tile.num_samples;

  DCHECK_GT(tile_work_size, 0);

  if (max_work_size && tile_work_size > max_work_size) {
    /* The work did not fit into the requested limit of the work size. Unschedule the tile,
     * so it can be picked up again later. */
    next_work_index_--;
    return false;
  }

  *work_tile_ = work_tile;

  return true;
}

CCL_NAMESPACE_END

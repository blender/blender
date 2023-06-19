/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/work_tile_scheduler.h"

#include "device/queue.h"
#include "integrator/tile.h"
#include "session/buffers.h"
#include "util/atomic.h"
#include "util/log.h"

CCL_NAMESPACE_BEGIN

WorkTileScheduler::WorkTileScheduler() {}

void WorkTileScheduler::set_accelerated_rt(bool accelerated_rt)
{
  accelerated_rt_ = accelerated_rt;
}

void WorkTileScheduler::set_max_num_path_states(int max_num_path_states)
{
  max_num_path_states_ = max_num_path_states;
}

void WorkTileScheduler::reset(const BufferParams &buffer_params,
                              int sample_start,
                              int samples_num,
                              int sample_offset,
                              float scrambling_distance)
{
  /* Image buffer parameters. */
  image_full_offset_px_.x = buffer_params.full_x;
  image_full_offset_px_.y = buffer_params.full_y;

  image_size_px_ = make_int2(buffer_params.width, buffer_params.height);
  scrambling_distance_ = scrambling_distance;

  offset_ = buffer_params.offset;
  stride_ = buffer_params.stride;

  /* Samples parameters. */
  sample_start_ = sample_start;
  samples_num_ = samples_num;
  sample_offset_ = sample_offset;

  /* Initialize new scheduling. */
  reset_scheduler_state();
}

void WorkTileScheduler::reset_scheduler_state()
{
  tile_size_ = tile_calculate_best_size(
      accelerated_rt_, image_size_px_, samples_num_, max_num_path_states_, scrambling_distance_);

  VLOG_WORK << "Will schedule tiles of size " << tile_size_;

  const int num_path_states_in_tile = tile_size_.width * tile_size_.height *
                                      tile_size_.num_samples;

  if (num_path_states_in_tile == 0) {
    num_tiles_x_ = 0;
    num_tiles_y_ = 0;
    num_tiles_per_sample_range_ = 0;
  }
  else {
    if (VLOG_IS_ON(3)) {
      /* The logging is based on multiple tiles scheduled, ignoring overhead of multi-tile
       * scheduling and purely focusing on the number of used path states. */
      const int num_tiles = max_num_path_states_ / num_path_states_in_tile;
      VLOG_WORK << "Number of unused path states: "
                << max_num_path_states_ - num_tiles * num_path_states_in_tile;
    }

    num_tiles_x_ = divide_up(image_size_px_.x, tile_size_.width);
    num_tiles_y_ = divide_up(image_size_px_.y, tile_size_.height);
    num_tiles_per_sample_range_ = divide_up(samples_num_, tile_size_.num_samples);
  }

  total_tiles_num_ = num_tiles_x_ * num_tiles_y_;

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
  work_tile.sample_offset = sample_offset_;
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

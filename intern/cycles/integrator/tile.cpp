/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/tile.h"

#include "util/log.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

std::ostream &operator<<(std::ostream &os, const TileSize &tile_size)
{
  os << "size: (" << tile_size.width << ", " << tile_size.height << ")";
  os << ", num_samples: " << tile_size.num_samples;
  return os;
}

ccl_device_inline uint round_down_to_power_of_two(uint x)
{
  if (is_power_of_two(x)) {
    return x;
  }

  return prev_power_of_two(x);
}

ccl_device_inline uint round_up_to_power_of_two(uint x)
{
  if (is_power_of_two(x)) {
    return x;
  }

  return next_power_of_two(x);
}

TileSize tile_calculate_best_size(const bool accel_rt,
                                  const int2 &image_size,
                                  const int num_samples,
                                  const int max_num_path_states,
                                  const float scrambling_distance)
{
  if (max_num_path_states == 1) {
    /* Simple case: avoid any calculation, which could cause rounding issues. */
    return TileSize(1, 1, 1);
  }

  const int64_t num_pixels = image_size.x * image_size.y;
  const int64_t num_pixel_samples = num_pixels * num_samples;

  if (max_num_path_states >= num_pixel_samples) {
    /* Image fully fits into the state (could be border render, for example). */
    return TileSize(image_size.x, image_size.y, num_samples);
  }

  /* The idea here is to keep number of samples per tile as much as possible to improve coherency
   * across threads.
   *
   * Some general ideas:
   *  - Prefer smaller tiles with more samples, which improves spatial coherency of paths.
   *  - Keep values a power of two, for more integer fit into the maximum number of paths. */

  TileSize tile_size;
  const int num_path_states_per_sample = max_num_path_states / num_samples;
  if (scrambling_distance < 0.9f && accel_rt) {
    /* Prefer large tiles for scrambling distance, bounded by max num path states. */
    tile_size.width = min(image_size.x, max_num_path_states);
    tile_size.height = min(image_size.y, max(max_num_path_states / tile_size.width, 1));
  }
  else {
    /* Calculate tile size as if it is the most possible one to fit an entire range of samples.
     * The idea here is to keep tiles as small as possible, and keep device occupied by scheduling
     * multiple tiles with the same coordinates rendering different samples. */

    if (num_path_states_per_sample != 0) {
      tile_size.width = round_down_to_power_of_two(lround(sqrt(num_path_states_per_sample)));
      tile_size.height = tile_size.width;
    }
    else {
      tile_size.width = tile_size.height = 1;
    }
  }

  if (num_samples == 1) {
    tile_size.num_samples = 1;
  }
  else {
    /* Heuristic here is to have more uniform division of the sample range: for example prefer
     * [32 <38 times>, 8] over [1024, 200]. This allows to greedily add more tiles early on. */
    tile_size.num_samples = min(round_up_to_power_of_two(lround(sqrt(num_samples / 2))),
                                static_cast<uint>(num_samples));

    const int tile_area = tile_size.width * tile_size.height;
    tile_size.num_samples = min(tile_size.num_samples, max_num_path_states / tile_area);
  }

  DCHECK_GE(tile_size.width, 1);
  DCHECK_GE(tile_size.height, 1);
  DCHECK_GE(tile_size.num_samples, 1);
  DCHECK_LE(tile_size.width * tile_size.height * tile_size.num_samples, max_num_path_states);

  return tile_size;
}

CCL_NAMESPACE_END

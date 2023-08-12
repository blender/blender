/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <iosfwd>

#include "util/types.h"

CCL_NAMESPACE_BEGIN

struct TileSize {
  TileSize() = default;

  inline TileSize(int width, int height, int num_samples)
      : width(width), height(height), num_samples(num_samples)
  {
  }

  inline bool operator==(const TileSize &other) const
  {
    return width == other.width && height == other.height && num_samples == other.num_samples;
  }
  inline bool operator!=(const TileSize &other) const
  {
    return !(*this == other);
  }

  int width = 0, height = 0;
  int num_samples = 0;
};

std::ostream &operator<<(std::ostream &os, const TileSize &tile_size);

/* Calculate tile size which is best suitable for rendering image of a given size with given number
 * of active path states.
 * Will attempt to provide best guess to keep path tracing threads of a device as localized as
 * possible, and have as many threads active for every tile as possible. */
TileSize tile_calculate_best_size(const bool accel_rt,
                                  const int2 &image_size,
                                  const int num_samples,
                                  const int max_num_path_states,
                                  const float scrambling_distance);

CCL_NAMESPACE_END

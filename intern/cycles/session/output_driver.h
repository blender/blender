/*
 * Copyright 2021 Blender Foundation
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

#include "util/math.h"
#include "util/string.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Output driver for reading render buffers.
 *
 * Host applications implement this interface for outputting render buffers for offline rendering.
 * Drivers can be used to copy the buffers into the host application or write them directly to
 * disk. This interface may also be used for interactive display, however the DisplayDriver is more
 * efficient for that purpose.
 */
class OutputDriver {
 public:
  OutputDriver() = default;
  virtual ~OutputDriver() = default;

  class Tile {
   public:
    Tile(const int2 offset,
         const int2 size,
         const int2 full_size,
         const string_view layer,
         const string_view view)
        : offset(offset), size(size), full_size(full_size), layer(layer), view(view)
    {
    }
    virtual ~Tile() = default;

    const int2 offset;
    const int2 size;
    const int2 full_size;
    const string layer;
    const string view;

    virtual bool get_pass_pixels(const string_view pass_name,
                                 const int num_channels,
                                 float *pixels) const = 0;
    virtual bool set_pass_pixels(const string_view pass_name,
                                 const int num_channels,
                                 const float *pixels) const = 0;
  };

  /* Write tile once it has finished rendering. */
  virtual void write_render_tile(const Tile &tile) = 0;

  /* Update tile while rendering is in progress. Return true if any update
   * was performed. */
  virtual bool update_render_tile(const Tile & /* tile */)
  {
    return false;
  }

  /* For baking, read render pass PASS_BAKE_PRIMITIVE and PASS_BAKE_DIFFERENTIAL
   * to determine which shading points to use for baking at each pixel. Return
   * true if any data was read. */
  virtual bool read_render_tile(const Tile & /* tile */)
  {
    return false;
  }
};

CCL_NAMESPACE_END

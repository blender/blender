/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string.h>

#include "COM_MultiThreadedOperation.h"

#include "DNA_movieclip_types.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

namespace blender::compositor {

/**
 * Class with implementation of green screen gradient rasterization
 */
class KeyingScreenOperation : public MultiThreadedOperation {
 protected:
  struct MarkerPoint {
    float2 position;
    float4 color;
  };

  MovieClip *movie_clip_;
  float smoothness_;
  int framenumber_;
  Array<MarkerPoint> *cached_marker_points_;
  char tracking_object_[64];

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  Array<MarkerPoint> *compute_marker_points();

 public:
  KeyingScreenOperation();

  void init_execution() override;
  void deinit_execution() override;

  void set_movie_clip(MovieClip *clip)
  {
    movie_clip_ = clip;
  }
  void set_tracking_object(const char *object)
  {
    BLI_strncpy(tracking_object_, object, sizeof(tracking_object_));
  }
  void set_smoothness(float smoothness)
  {
    smoothness_ = math::interpolate(0.15f, 1.0f, smoothness);
  }
  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

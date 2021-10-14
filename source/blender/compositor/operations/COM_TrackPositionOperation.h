/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2012, Blender Foundation.
 */

#pragma once

#include <string.h>

#include "COM_ConstantOperation.h"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

namespace blender::compositor {

/**
 * Class with implementation of green screen gradient rasterization
 */
class TrackPositionOperation : public ConstantOperation {
 protected:
  MovieClip *movie_clip_;
  int framenumber_;
  char tracking_object_name_[64];
  char track_name_[64];
  int axis_;
  int position_;
  int relative_frame_;
  bool speed_output_;

  int width_, height_;
  float marker_pos_[2];
  float relative_pos_[2];
  float track_position_;
  bool is_track_position_calculated_;

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 public:
  TrackPositionOperation();

  void set_movie_clip(MovieClip *clip)
  {
    movie_clip_ = clip;
  }
  void set_tracking_object(char *object)
  {
    BLI_strncpy(tracking_object_name_, object, sizeof(tracking_object_name_));
  }
  void set_track_name(char *track)
  {
    BLI_strncpy(track_name_, track, sizeof(track_name_));
  }
  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }
  void set_axis(int value)
  {
    axis_ = value;
  }
  void set_position(int value)
  {
    position_ = value;
  }
  void set_relative_frame(int value)
  {
    relative_frame_ = value;
  }
  void set_speed_output(bool speed_output)
  {
    speed_output_ = speed_output;
  }

  void init_execution() override;

  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  const float *get_constant_elem() override;

 private:
  void calc_track_position();
};

}  // namespace blender::compositor

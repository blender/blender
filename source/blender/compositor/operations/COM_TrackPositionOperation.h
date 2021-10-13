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
  MovieClip *movieClip_;
  int framenumber_;
  char trackingObjectName_[64];
  char trackName_[64];
  int axis_;
  int position_;
  int relativeFrame_;
  bool speed_output_;

  int width_, height_;
  float markerPos_[2];
  float relativePos_[2];
  float track_position_;
  bool is_track_position_calculated_;

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 public:
  TrackPositionOperation();

  void setMovieClip(MovieClip *clip)
  {
    movieClip_ = clip;
  }
  void setTrackingObject(char *object)
  {
    BLI_strncpy(trackingObjectName_, object, sizeof(trackingObjectName_));
  }
  void setTrackName(char *track)
  {
    BLI_strncpy(trackName_, track, sizeof(trackName_));
  }
  void setFramenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }
  void setAxis(int value)
  {
    axis_ = value;
  }
  void setPosition(int value)
  {
    position_ = value;
  }
  void setRelativeFrame(int value)
  {
    relativeFrame_ = value;
  }
  void setSpeedOutput(bool speed_output)
  {
    speed_output_ = speed_output;
  }

  void initExecution() override;

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  const float *get_constant_elem() override;

 private:
  void calc_track_position();
};

}  // namespace blender::compositor

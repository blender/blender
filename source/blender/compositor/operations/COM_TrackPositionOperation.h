/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  CMPNodeTrackPositionMode position_;
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
  void set_position(CMPNodeTrackPositionMode value)
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

  const float *get_constant_elem() override;

 private:
  void calc_track_position();
};

}  // namespace blender::compositor

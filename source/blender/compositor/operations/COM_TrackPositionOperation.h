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
  MovieClip *m_movieClip;
  int m_framenumber;
  char m_trackingObjectName[64];
  char m_trackName[64];
  int m_axis;
  int m_position;
  int m_relativeFrame;
  bool m_speed_output;

  int m_width, m_height;
  float m_markerPos[2];
  float m_relativePos[2];
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
    m_movieClip = clip;
  }
  void setTrackingObject(char *object)
  {
    BLI_strncpy(m_trackingObjectName, object, sizeof(m_trackingObjectName));
  }
  void setTrackName(char *track)
  {
    BLI_strncpy(m_trackName, track, sizeof(m_trackName));
  }
  void setFramenumber(int framenumber)
  {
    m_framenumber = framenumber;
  }
  void setAxis(int value)
  {
    m_axis = value;
  }
  void setPosition(int value)
  {
    m_position = value;
  }
  void setRelativeFrame(int value)
  {
    m_relativeFrame = value;
  }
  void setSpeedOutput(bool speed_output)
  {
    m_speed_output = speed_output;
  }

  void initExecution() override;

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  const float *get_constant_elem() override;

 private:
  void calc_track_position();
};

}  // namespace blender::compositor

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
 * Copyright 2013, Blender Foundation.
 */

#pragma once

#include <string.h>

#include "COM_PlaneDistortCommonOperation.h"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

namespace blender::compositor {

class PlaneTrackCommon {
 protected:
  MovieClip *m_movieClip;
  int m_framenumber;
  char m_trackingObjectName[64];
  char m_planeTrackName[64];

  /* NOTE: this class is not an operation itself (to prevent virtual inheritance issues)
   * implementation classes must make wrappers to use these methods, see below.
   */
  void read_and_calculate_corners(PlaneDistortBaseOperation *distort_op);
  void determine_canvas(const rcti &preferred_area, rcti &r_area);

 public:
  PlaneTrackCommon();

  void setMovieClip(MovieClip *clip)
  {
    this->m_movieClip = clip;
  }
  void setTrackingObject(char *object)
  {
    BLI_strncpy(this->m_trackingObjectName, object, sizeof(this->m_trackingObjectName));
  }
  void setPlaneTrackName(char *plane_track)
  {
    BLI_strncpy(this->m_planeTrackName, plane_track, sizeof(this->m_planeTrackName));
  }
  void setFramenumber(int framenumber)
  {
    this->m_framenumber = framenumber;
  }

 private:
  void readCornersFromTrack(float corners[4][2], float frame);
};

class PlaneTrackMaskOperation : public PlaneDistortMaskOperation, public PlaneTrackCommon {
 public:
  PlaneTrackMaskOperation()
  {
  }

  void init_data() override;

  void initExecution() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override
  {
    PlaneTrackCommon::determine_canvas(preferred_area, r_area);

    rcti unused;
    rcti &preferred = r_area;
    NodeOperation::determine_canvas(preferred, unused);
  }
};

class PlaneTrackWarpImageOperation : public PlaneDistortWarpImageOperation,
                                     public PlaneTrackCommon {
 public:
  PlaneTrackWarpImageOperation() : PlaneTrackCommon()
  {
  }

  void init_data() override;

  void initExecution() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override
  {
    PlaneTrackCommon::determine_canvas(preferred_area, r_area);

    rcti unused;
    rcti &preferred = r_area;
    NodeOperation::determine_canvas(preferred, unused);
  }
};

}  // namespace blender::compositor

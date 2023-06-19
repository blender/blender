/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  MovieClip *movie_clip_;
  int framenumber_;
  char tracking_object_name_[64];
  char plane_track_name_[64];

  /* NOTE: this class is not an operation itself (to prevent virtual inheritance issues)
   * implementation classes must make wrappers to use these methods, see below.
   */
  void read_and_calculate_corners(PlaneDistortBaseOperation *distort_op);
  void determine_canvas(const rcti &preferred_area, rcti &r_area);

 public:
  PlaneTrackCommon();

  void set_movie_clip(MovieClip *clip)
  {
    movie_clip_ = clip;
  }
  void set_tracking_object(char *object)
  {
    BLI_strncpy(tracking_object_name_, object, sizeof(tracking_object_name_));
  }
  void set_plane_track_name(char *plane_track)
  {
    BLI_strncpy(plane_track_name_, plane_track, sizeof(plane_track_name_));
  }
  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }

 private:
  void read_corners_from_track(float corners[4][2], float frame);
};

class PlaneTrackMaskOperation : public PlaneDistortMaskOperation, public PlaneTrackCommon {
 public:
  PlaneTrackMaskOperation() {}

  void init_data() override;

  void init_execution() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override
  {
    PlaneTrackCommon::determine_canvas(preferred_area, r_area);

    rcti unused = COM_AREA_NONE;
    rcti &preferred = r_area;
    NodeOperation::determine_canvas(preferred, unused);
  }
};

class PlaneTrackWarpImageOperation : public PlaneDistortWarpImageOperation,
                                     public PlaneTrackCommon {
 public:
  PlaneTrackWarpImageOperation() : PlaneTrackCommon() {}

  void init_data() override;

  void init_execution() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override
  {
    PlaneTrackCommon::determine_canvas(preferred_area, r_area);

    rcti unused = COM_AREA_NONE;
    rcti &preferred = r_area;
    NodeOperation::determine_canvas(preferred, unused);
  }
};

}  // namespace blender::compositor

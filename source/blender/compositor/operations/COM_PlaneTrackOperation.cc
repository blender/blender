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

#include "COM_PlaneTrackOperation.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

namespace blender::compositor {

/* ******** PlaneTrackCommon ******** */

PlaneTrackCommon::PlaneTrackCommon()
{
  movie_clip_ = nullptr;
  framenumber_ = 0;
  tracking_object_name_[0] = '\0';
  plane_track_name_[0] = '\0';
}

void PlaneTrackCommon::read_and_calculate_corners(PlaneDistortBaseOperation *distort_op)
{
  float corners[4][2];
  if (distort_op->motion_blur_samples_ == 1) {
    read_corners_from_track(corners, framenumber_);
    distort_op->calculate_corners(corners, true, 0);
  }
  else {
    const float frame = (float)framenumber_ - distort_op->motion_blur_shutter_;
    const float frame_step = (distort_op->motion_blur_shutter_ * 2.0f) /
                             distort_op->motion_blur_samples_;
    float frame_iter = frame;
    for (int sample = 0; sample < distort_op->motion_blur_samples_; sample++) {
      read_corners_from_track(corners, frame_iter);
      distort_op->calculate_corners(corners, true, sample);
      frame_iter += frame_step;
    }
  }
}

void PlaneTrackCommon::read_corners_from_track(float corners[4][2], float frame)
{
  MovieTracking *tracking;
  MovieTrackingObject *object;

  if (!movie_clip_) {
    return;
  }

  tracking = &movie_clip_->tracking;

  object = BKE_tracking_object_get_named(tracking, tracking_object_name_);
  if (object) {
    MovieTrackingPlaneTrack *plane_track;
    plane_track = BKE_tracking_plane_track_get_named(tracking, object, plane_track_name_);
    if (plane_track) {
      float clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movie_clip_, frame);
      BKE_tracking_plane_marker_get_subframe_corners(plane_track, clip_framenr, corners);
    }
  }
}

void PlaneTrackCommon::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = COM_AREA_NONE;
  if (movie_clip_) {
    int width, height;
    MovieClipUser user = {0};
    BKE_movieclip_user_set_frame(&user, framenumber_);
    BKE_movieclip_get_size(movie_clip_, &user, &width, &height);
    r_area = preferred_area;
    r_area.xmax = r_area.xmin + width;
    r_area.ymax = r_area.ymin + height;
  }
}

/* ******** PlaneTrackMaskOperation ******** */

void PlaneTrackMaskOperation::init_data()
{
  PlaneDistortMaskOperation::init_data();
  if (execution_model_ == eExecutionModel::FullFrame) {
    PlaneTrackCommon::read_and_calculate_corners(this);
  }
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void PlaneTrackMaskOperation::init_execution()
{
  PlaneDistortMaskOperation::init_execution();
  if (execution_model_ == eExecutionModel::Tiled) {
    PlaneTrackCommon::read_and_calculate_corners(this);
  }
}

/* ******** PlaneTrackWarpImageOperation ******** */

void PlaneTrackWarpImageOperation::init_data()
{
  PlaneDistortWarpImageOperation::init_data();
  if (execution_model_ == eExecutionModel::FullFrame) {
    PlaneTrackCommon::read_and_calculate_corners(this);
  }
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void PlaneTrackWarpImageOperation::init_execution()
{
  PlaneDistortWarpImageOperation::init_execution();
  if (execution_model_ == eExecutionModel::Tiled) {
    PlaneTrackCommon::read_and_calculate_corners(this);
  }
}

}  // namespace blender::compositor

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

#include "COM_TrackPositionOperation.h"

#include "DNA_defaults.h"

#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

namespace blender::compositor {

TrackPositionOperation::TrackPositionOperation()
{
  this->add_output_socket(DataType::Value);
  movie_clip_ = nullptr;
  framenumber_ = 0;
  tracking_object_name_[0] = 0;
  track_name_[0] = 0;
  axis_ = 0;
  position_ = CMP_TRACKPOS_ABSOLUTE;
  relative_frame_ = 0;
  speed_output_ = false;
  flags_.is_set_operation = true;
  is_track_position_calculated_ = false;
}

void TrackPositionOperation::init_execution()
{
  if (!is_track_position_calculated_) {
    calc_track_position();
  }
}

void TrackPositionOperation::calc_track_position()
{
  is_track_position_calculated_ = true;
  MovieTracking *tracking = nullptr;
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  MovieTrackingObject *object;

  track_position_ = 0;
  zero_v2(marker_pos_);
  zero_v2(relative_pos_);

  if (!movie_clip_) {
    return;
  }

  tracking = &movie_clip_->tracking;

  BKE_movieclip_user_set_frame(&user, framenumber_);
  BKE_movieclip_get_size(movie_clip_, &user, &width_, &height_);

  object = BKE_tracking_object_get_named(tracking, tracking_object_name_);
  if (object) {
    MovieTrackingTrack *track;

    track = BKE_tracking_track_get_named(tracking, object, track_name_);

    if (track) {
      MovieTrackingMarker *marker;
      int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movie_clip_, framenumber_);

      marker = BKE_tracking_marker_get(track, clip_framenr);

      copy_v2_v2(marker_pos_, marker->pos);

      if (speed_output_) {
        int relative_clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movie_clip_,
                                                                            relative_frame_);

        marker = BKE_tracking_marker_get_exact(track, relative_clip_framenr);
        if (marker != nullptr && (marker->flag & MARKER_DISABLED) == 0) {
          copy_v2_v2(relative_pos_, marker->pos);
        }
        else {
          copy_v2_v2(relative_pos_, marker_pos_);
        }
        if (relative_frame_ < framenumber_) {
          swap_v2_v2(relative_pos_, marker_pos_);
        }
      }
      else if (position_ == CMP_TRACKPOS_RELATIVE_START) {
        int i;

        for (i = 0; i < track->markersnr; i++) {
          marker = &track->markers[i];

          if ((marker->flag & MARKER_DISABLED) == 0) {
            copy_v2_v2(relative_pos_, marker->pos);

            break;
          }
        }
      }
      else if (position_ == CMP_TRACKPOS_RELATIVE_FRAME) {
        int relative_clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movie_clip_,
                                                                            relative_frame_);

        marker = BKE_tracking_marker_get(track, relative_clip_framenr);
        copy_v2_v2(relative_pos_, marker->pos);
      }
    }
  }

  track_position_ = marker_pos_[axis_] - relative_pos_[axis_];
  if (axis_ == 0) {
    track_position_ *= width_;
  }
  else {
    track_position_ *= height_;
  }
}

void TrackPositionOperation::execute_pixel_sampled(float output[4],
                                                   float /*x*/,
                                                   float /*y*/,
                                                   PixelSampler /*sampler*/)
{
  output[0] = marker_pos_[axis_] - relative_pos_[axis_];

  if (axis_ == 0) {
    output[0] *= width_;
  }
  else {
    output[0] *= height_;
  }
}

const float *TrackPositionOperation::get_constant_elem()
{
  if (!is_track_position_calculated_) {
    calc_track_position();
  }
  return &track_position_;
}

void TrackPositionOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
}

}  // namespace blender::compositor

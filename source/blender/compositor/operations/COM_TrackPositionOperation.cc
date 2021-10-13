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

#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

namespace blender::compositor {

TrackPositionOperation::TrackPositionOperation()
{
  this->addOutputSocket(DataType::Value);
  m_movieClip = nullptr;
  m_framenumber = 0;
  m_trackingObjectName[0] = 0;
  m_trackName[0] = 0;
  m_axis = 0;
  m_position = CMP_TRACKPOS_ABSOLUTE;
  m_relativeFrame = 0;
  m_speed_output = false;
  flags.is_set_operation = true;
  is_track_position_calculated_ = false;
}

void TrackPositionOperation::initExecution()
{
  if (!is_track_position_calculated_) {
    calc_track_position();
  }
}

void TrackPositionOperation::calc_track_position()
{
  is_track_position_calculated_ = true;
  MovieTracking *tracking = nullptr;
  MovieClipUser user = {0};
  MovieTrackingObject *object;

  track_position_ = 0;
  zero_v2(m_markerPos);
  zero_v2(m_relativePos);

  if (!m_movieClip) {
    return;
  }

  tracking = &m_movieClip->tracking;

  BKE_movieclip_user_set_frame(&user, m_framenumber);
  BKE_movieclip_get_size(m_movieClip, &user, &m_width, &m_height);

  object = BKE_tracking_object_get_named(tracking, m_trackingObjectName);
  if (object) {
    MovieTrackingTrack *track;

    track = BKE_tracking_track_get_named(tracking, object, m_trackName);

    if (track) {
      MovieTrackingMarker *marker;
      int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(m_movieClip, m_framenumber);

      marker = BKE_tracking_marker_get(track, clip_framenr);

      copy_v2_v2(m_markerPos, marker->pos);

      if (m_speed_output) {
        int relative_clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(m_movieClip,
                                                                            m_relativeFrame);

        marker = BKE_tracking_marker_get_exact(track, relative_clip_framenr);
        if (marker != nullptr && (marker->flag & MARKER_DISABLED) == 0) {
          copy_v2_v2(m_relativePos, marker->pos);
        }
        else {
          copy_v2_v2(m_relativePos, m_markerPos);
        }
        if (m_relativeFrame < m_framenumber) {
          swap_v2_v2(m_relativePos, m_markerPos);
        }
      }
      else if (m_position == CMP_TRACKPOS_RELATIVE_START) {
        int i;

        for (i = 0; i < track->markersnr; i++) {
          marker = &track->markers[i];

          if ((marker->flag & MARKER_DISABLED) == 0) {
            copy_v2_v2(m_relativePos, marker->pos);

            break;
          }
        }
      }
      else if (m_position == CMP_TRACKPOS_RELATIVE_FRAME) {
        int relative_clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(m_movieClip,
                                                                            m_relativeFrame);

        marker = BKE_tracking_marker_get(track, relative_clip_framenr);
        copy_v2_v2(m_relativePos, marker->pos);
      }
    }
  }

  track_position_ = m_markerPos[m_axis] - m_relativePos[m_axis];
  if (m_axis == 0) {
    track_position_ *= m_width;
  }
  else {
    track_position_ *= m_height;
  }
}

void TrackPositionOperation::executePixelSampled(float output[4],
                                                 float /*x*/,
                                                 float /*y*/,
                                                 PixelSampler /*sampler*/)
{
  output[0] = m_markerPos[m_axis] - m_relativePos[m_axis];

  if (m_axis == 0) {
    output[0] *= m_width;
  }
  else {
    output[0] *= m_height;
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

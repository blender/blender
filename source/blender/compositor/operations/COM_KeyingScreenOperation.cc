/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KeyingScreenOperation.h"

#include "DNA_defaults.h"

#include "BLI_array.hh"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

namespace blender::compositor {

KeyingScreenOperation::KeyingScreenOperation()
{
  this->add_output_socket(DataType::Color);
  movie_clip_ = nullptr;
  framenumber_ = 0;
  tracking_object_[0] = 0;
  flags_.complex = true;
  cached_marker_points_ = nullptr;
}

void KeyingScreenOperation::init_execution()
{
  init_mutex();
  if (execution_model_ == eExecutionModel::FullFrame) {
    BLI_assert(cached_marker_points_ == nullptr);
    if (movie_clip_) {
      cached_marker_points_ = compute_marker_points();
    }
  }
  else {
    cached_marker_points_ = nullptr;
  }
}

void KeyingScreenOperation::deinit_execution()
{
  delete cached_marker_points_;
  cached_marker_points_ = nullptr;
}

Array<KeyingScreenOperation::MarkerPoint> *KeyingScreenOperation::compute_marker_points()
{
  MovieTracking *tracking = &movie_clip_->tracking;

  const MovieTrackingObject *tracking_object = nullptr;
  if (tracking_object_[0]) {
    tracking_object = BKE_tracking_object_get_named(tracking, tracking_object_);
    if (!tracking_object) {
      return nullptr;
    }
  }
  else {
    tracking_object = BKE_tracking_object_get_active(tracking);
  }
  BLI_assert(tracking_object != nullptr);

  int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(movie_clip_, framenumber_);

  /* count sites */
  int sites_total = 0;
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_frame);

    if (marker->flag & MARKER_DISABLED) {
      continue;
    }

    float pos[2];
    add_v2_v2v2(pos, marker->pos, track->offset);

    if (!IN_RANGE_INCL(pos[0], 0.0f, 1.0f) || !IN_RANGE_INCL(pos[1], 0.0f, 1.0f)) {
      continue;
    }

    sites_total++;
  }

  if (!sites_total) {
    return nullptr;
  }

  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  BKE_movieclip_user_set_frame(&user, clip_frame);
  ImBuf *ibuf = BKE_movieclip_get_ibuf(movie_clip_, &user);

  if (!ibuf) {
    return nullptr;
  }

  Array<MarkerPoint> *marker_points = new Array<MarkerPoint>(sites_total);

  int track_index = 0;
  LISTBASE_FOREACH_INDEX (MovieTrackingTrack *, track, &tracking_object->tracks, track_index) {
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_frame);
    if (marker->flag & MARKER_DISABLED) {
      continue;
    }

    const float2 position = float2(marker->pos) + float2(track->offset);

    if (!IN_RANGE_INCL(position.x, 0.0f, 1.0f) || !IN_RANGE_INCL(position.y, 0.0f, 1.0f)) {
      continue;
    }

    ImBuf *pattern_ibuf = BKE_tracking_get_pattern_imbuf(ibuf, track, marker, true, false);

    MarkerPoint &marker_point = (*marker_points)[track_index];
    marker_point.position = position;

    marker_point.color = float4(0.0f);
    if (pattern_ibuf) {
      for (int j = 0; j < pattern_ibuf->x * pattern_ibuf->y; j++) {
        if (pattern_ibuf->float_buffer.data) {
          marker_point.color += float4(&pattern_ibuf->float_buffer.data[4 * j]);
        }
        else {
          uchar *rrgb = pattern_ibuf->byte_buffer.data;
          marker_point.color += float4(srgb_to_linearrgb(float(rrgb[4 * j + 0]) / 255.0f),
                                       srgb_to_linearrgb(float(rrgb[4 * j + 1]) / 255.0f),
                                       srgb_to_linearrgb(float(rrgb[4 * j + 2]) / 255.0f),
                                       srgb_to_linearrgb(float(rrgb[4 * j + 3]) / 255.0f));
        }
      }

      marker_point.color /= pattern_ibuf->x * pattern_ibuf->y;
      IMB_freeImBuf(pattern_ibuf);
    }
  }

  IMB_freeImBuf(ibuf);

  return marker_points;
}

void *KeyingScreenOperation::initialize_tile_data(rcti * /*rect*/)
{
  if (movie_clip_ == nullptr) {
    return nullptr;
  }

  if (!cached_marker_points_) {
    lock_mutex();
    if (cached_marker_points_ == nullptr) {
      cached_marker_points_ = compute_marker_points();
    }
    unlock_mutex();
  }

  return nullptr;
}

void KeyingScreenOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = COM_AREA_NONE;

  if (movie_clip_) {
    MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
    int width, height;
    int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(movie_clip_, framenumber_);

    BKE_movieclip_user_set_frame(&user, clip_frame);
    BKE_movieclip_get_size(movie_clip_, &user, &width, &height);
    r_area = preferred_area;
    r_area.xmax = r_area.xmin + width;
    r_area.ymax = r_area.ymin + height;
  }
}

void KeyingScreenOperation::execute_pixel(float output[4], int x, int y, void * /* data */)
{
  if (!cached_marker_points_) {
    copy_v4_fl(output, 0.0f);
    return;
  }

  const int2 size = int2(this->get_width(), this->get_height());
  const float2 normalized_pixel_location = float2(x, y) / float2(size);
  const float squared_shape_parameter = math::square(1.0f / smoothness_);

  float4 weighted_sum = float4(0.0f);
  float sum_of_weights = 0.0f;
  for (const MarkerPoint &marker_point : *cached_marker_points_) {
    const float2 difference = normalized_pixel_location - marker_point.position;
    const float squared_distance = math::dot(difference, difference);
    const float gaussian = math::exp(-squared_distance * squared_shape_parameter);
    weighted_sum += marker_point.color * gaussian;
    sum_of_weights += gaussian;
  }
  weighted_sum /= sum_of_weights;

  copy_v4_v4(output, weighted_sum);
}

void KeyingScreenOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  if (!cached_marker_points_) {
    output->fill(area, COM_COLOR_TRANSPARENT);
    return;
  }

  const int2 size = int2(this->get_width(), this->get_height());
  const float squared_shape_parameter = math::square(1.0f / smoothness_);

  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float2 normalized_pixel_location = float2(it.x, it.y) / float2(size);

    float4 weighted_sum = float4(0.0f);
    float sum_of_weights = 0.0f;
    for (const MarkerPoint &marker_point : *cached_marker_points_) {
      const float2 difference = normalized_pixel_location - marker_point.position;
      const float squared_distance = math::dot(difference, difference);
      const float gaussian = math::exp(-squared_distance * squared_shape_parameter);
      weighted_sum += marker_point.color * gaussian;
      sum_of_weights += gaussian;
    }
    weighted_sum /= sum_of_weights;

    copy_v4_v4(it.out, weighted_sum);
  }
}

}  // namespace blender::compositor

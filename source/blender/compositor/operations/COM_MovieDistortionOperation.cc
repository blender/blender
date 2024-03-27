/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MovieDistortionOperation.h"

#include "DNA_defaults.h"

#include "BKE_movieclip.h"

namespace blender::compositor {

MovieDistortionOperation::MovieDistortionOperation(bool distortion)
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  movie_clip_ = nullptr;
  apply_ = distortion;

  flags_.can_be_constant = true;
}

void MovieDistortionOperation::init_data()
{
  if (movie_clip_) {
    MovieTracking *tracking = &movie_clip_->tracking;
    MovieClipUser clip_user = *DNA_struct_default_get(MovieClipUser);
    int calibration_width, calibration_height;

    BKE_movieclip_user_set_frame(&clip_user, framenumber_);
    BKE_movieclip_get_size(movie_clip_, &clip_user, &calibration_width, &calibration_height);

    float delta[2];
    rcti full_frame;
    full_frame.xmin = full_frame.ymin = 0;
    full_frame.xmax = this->get_width();
    full_frame.ymax = this->get_height();
    BKE_tracking_max_distortion_delta_across_bound(
        tracking, this->get_width(), this->get_height(), &full_frame, !apply_, delta);

    /* 5 is just in case we didn't hit real max of distortion in
     * BKE_tracking_max_undistortion_delta_across_bound
     */
    margin_[0] = delta[0] + 5;
    margin_[1] = delta[1] + 5;

    calibration_width_ = calibration_width;
    calibration_height_ = calibration_height;
    pixel_aspect_ = tracking->camera.pixel_aspect;
  }
  else {
    margin_[0] = margin_[1] = 0;
  }
}

void MovieDistortionOperation::init_execution()
{
  if (movie_clip_) {
    MovieTracking *tracking = &movie_clip_->tracking;
    distortion_ = BKE_tracking_distortion_new(tracking, calibration_width_, calibration_height_);
  }
  else {
    distortion_ = nullptr;
  }
}

void MovieDistortionOperation::deinit_execution()
{
  movie_clip_ = nullptr;
  if (distortion_ != nullptr) {
    BKE_tracking_distortion_free(distortion_);
  }
}

void MovieDistortionOperation::get_area_of_interest(const int input_idx,
                                                    const rcti &output_area,
                                                    rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - margin_[0];
  r_input_area.ymin = output_area.ymin - margin_[1];
  r_input_area.xmax = output_area.xmax + margin_[0];
  r_input_area.ymax = output_area.ymax + margin_[1];
}

void MovieDistortionOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  if (distortion_ == nullptr) {
    output->copy_from(input_img, area);
    return;
  }

  /* `float overscan = 0.0f;` */
  const float pixel_aspect = pixel_aspect_;
  const float w = float(this->get_width()) /* `/ (1 + overscan)` */;
  const float h = float(this->get_height()) /* `/ (1 + overscan)` */;
  const float aspx = w / float(calibration_width_);
  const float aspy = h / float(calibration_height_);
  float xy[2];
  float distorted_xy[2];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    xy[0] = (it.x + 0.5f /* `- 0.5 * overscan * w` */) / aspx;
    xy[1] = (it.y + 0.5f /* `- 0.5 * overscan * h` */) / aspy / pixel_aspect;

    if (apply_) {
      BKE_tracking_distortion_undistort_v2(distortion_, xy, distorted_xy);
    }
    else {
      BKE_tracking_distortion_distort_v2(distortion_, xy, distorted_xy);
    }

    const float u = distorted_xy[0] * aspx /* `+ 0.5 * overscan * w` */;
    const float v = (distorted_xy[1] * aspy /* `+ 0.5 * overscan * h` */) * pixel_aspect;
    input_img->read_elem_bilinear(u - 0.5f, v - 0.5f, it.out);
  }
}

}  // namespace blender::compositor

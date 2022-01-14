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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_MovieDistortionOperation.h"

#include "DNA_defaults.h"

#include "BKE_movieclip.h"

namespace blender::compositor {

MovieDistortionOperation::MovieDistortionOperation(bool distortion)
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
  movie_clip_ = nullptr;
  apply_ = distortion;
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
  input_operation_ = this->get_input_socket_reader(0);
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
  input_operation_ = nullptr;
  movie_clip_ = nullptr;
  if (distortion_ != nullptr) {
    BKE_tracking_distortion_free(distortion_);
  }
}

void MovieDistortionOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler /*sampler*/)
{
  if (distortion_ != nullptr) {
    /* float overscan = 0.0f; */
    const float pixel_aspect = pixel_aspect_;
    const float w = (float)this->get_width() /* / (1 + overscan) */;
    const float h = (float)this->get_height() /* / (1 + overscan) */;
    const float aspx = w / (float)calibration_width_;
    const float aspy = h / (float)calibration_height_;
    float in[2];
    float out[2];

    in[0] = (x /* - 0.5 * overscan * w */) / aspx;
    in[1] = (y /* - 0.5 * overscan * h */) / aspy / pixel_aspect;

    if (apply_) {
      BKE_tracking_distortion_undistort_v2(distortion_, in, out);
    }
    else {
      BKE_tracking_distortion_distort_v2(distortion_, in, out);
    }

    float u = out[0] * aspx /* + 0.5 * overscan * w */,
          v = (out[1] * aspy /* + 0.5 * overscan * h */) * pixel_aspect;

    input_operation_->read_sampled(output, u, v, PixelSampler::Bilinear);
  }
  else {
    input_operation_->read_sampled(output, x, y, PixelSampler::Bilinear);
  }
}

bool MovieDistortionOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  new_input.xmin = input->xmin - margin_[0];
  new_input.ymin = input->ymin - margin_[1];
  new_input.xmax = input->xmax + margin_[0];
  new_input.ymax = input->ymax + margin_[1];
  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
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
  const float w = (float)this->get_width() /* `/ (1 + overscan)` */;
  const float h = (float)this->get_height() /* `/ (1 + overscan)` */;
  const float aspx = w / (float)calibration_width_;
  const float aspy = h / (float)calibration_height_;
  float xy[2];
  float distorted_xy[2];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    xy[0] = (it.x /* `- 0.5 * overscan * w` */) / aspx;
    xy[1] = (it.y /* `- 0.5 * overscan * h` */) / aspy / pixel_aspect;

    if (apply_) {
      BKE_tracking_distortion_undistort_v2(distortion_, xy, distorted_xy);
    }
    else {
      BKE_tracking_distortion_distort_v2(distortion_, xy, distorted_xy);
    }

    const float u = distorted_xy[0] * aspx /* `+ 0.5 * overscan * w` */;
    const float v = (distorted_xy[1] * aspy /* `+ 0.5 * overscan * h` */) * pixel_aspect;
    input_img->read_elem_bilinear(u, v, it.out);
  }
}

}  // namespace blender::compositor

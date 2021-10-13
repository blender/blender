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

#include "BKE_movieclip.h"

namespace blender::compositor {

MovieDistortionOperation::MovieDistortionOperation(bool distortion)
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  this->set_canvas_input_index(0);
  this->m_inputOperation = nullptr;
  this->m_movieClip = nullptr;
  this->m_apply = distortion;
}

void MovieDistortionOperation::init_data()
{
  if (this->m_movieClip) {
    MovieTracking *tracking = &this->m_movieClip->tracking;
    MovieClipUser clipUser = {0};
    int calibration_width, calibration_height;

    BKE_movieclip_user_set_frame(&clipUser, this->m_framenumber);
    BKE_movieclip_get_size(this->m_movieClip, &clipUser, &calibration_width, &calibration_height);

    float delta[2];
    rcti full_frame;
    full_frame.xmin = full_frame.ymin = 0;
    full_frame.xmax = this->getWidth();
    full_frame.ymax = this->getHeight();
    BKE_tracking_max_distortion_delta_across_bound(
        tracking, this->getWidth(), this->getHeight(), &full_frame, !this->m_apply, delta);

    /* 5 is just in case we didn't hit real max of distortion in
     * BKE_tracking_max_undistortion_delta_across_bound
     */
    m_margin[0] = delta[0] + 5;
    m_margin[1] = delta[1] + 5;

    this->m_calibration_width = calibration_width;
    this->m_calibration_height = calibration_height;
    this->m_pixel_aspect = tracking->camera.pixel_aspect;
  }
  else {
    m_margin[0] = m_margin[1] = 0;
  }
}

void MovieDistortionOperation::initExecution()
{
  m_inputOperation = this->getInputSocketReader(0);
  if (m_movieClip) {
    MovieTracking *tracking = &m_movieClip->tracking;
    m_distortion = BKE_tracking_distortion_new(
        tracking, m_calibration_width, m_calibration_height);
  }
  else {
    m_distortion = nullptr;
  }
}

void MovieDistortionOperation::deinitExecution()
{
  this->m_inputOperation = nullptr;
  this->m_movieClip = nullptr;
  if (this->m_distortion != nullptr) {
    BKE_tracking_distortion_free(this->m_distortion);
  }
}

void MovieDistortionOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler /*sampler*/)
{
  if (this->m_distortion != nullptr) {
    /* float overscan = 0.0f; */
    const float pixel_aspect = this->m_pixel_aspect;
    const float w = (float)this->getWidth() /* / (1 + overscan) */;
    const float h = (float)this->getHeight() /* / (1 + overscan) */;
    const float aspx = w / (float)this->m_calibration_width;
    const float aspy = h / (float)this->m_calibration_height;
    float in[2];
    float out[2];

    in[0] = (x /* - 0.5 * overscan * w */) / aspx;
    in[1] = (y /* - 0.5 * overscan * h */) / aspy / pixel_aspect;

    if (this->m_apply) {
      BKE_tracking_distortion_undistort_v2(this->m_distortion, in, out);
    }
    else {
      BKE_tracking_distortion_distort_v2(this->m_distortion, in, out);
    }

    float u = out[0] * aspx /* + 0.5 * overscan * w */,
          v = (out[1] * aspy /* + 0.5 * overscan * h */) * pixel_aspect;

    this->m_inputOperation->readSampled(output, u, v, PixelSampler::Bilinear);
  }
  else {
    this->m_inputOperation->readSampled(output, x, y, PixelSampler::Bilinear);
  }
}

bool MovieDistortionOperation::determineDependingAreaOfInterest(rcti *input,
                                                                ReadBufferOperation *readOperation,
                                                                rcti *output)
{
  rcti newInput;
  newInput.xmin = input->xmin - m_margin[0];
  newInput.ymin = input->ymin - m_margin[1];
  newInput.xmax = input->xmax + m_margin[0];
  newInput.ymax = input->ymax + m_margin[1];
  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void MovieDistortionOperation::get_area_of_interest(const int input_idx,
                                                    const rcti &output_area,
                                                    rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - m_margin[0];
  r_input_area.ymin = output_area.ymin - m_margin[1];
  r_input_area.xmax = output_area.xmax + m_margin[0];
  r_input_area.ymax = output_area.ymax + m_margin[1];
}

void MovieDistortionOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  if (this->m_distortion == nullptr) {
    output->copy_from(input_img, area);
    return;
  }

  /* `float overscan = 0.0f;` */
  const float pixel_aspect = this->m_pixel_aspect;
  const float w = (float)this->getWidth() /* `/ (1 + overscan)` */;
  const float h = (float)this->getHeight() /* `/ (1 + overscan)` */;
  const float aspx = w / (float)this->m_calibration_width;
  const float aspy = h / (float)this->m_calibration_height;
  float xy[2];
  float distorted_xy[2];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    xy[0] = (it.x /* `- 0.5 * overscan * w` */) / aspx;
    xy[1] = (it.y /* `- 0.5 * overscan * h` */) / aspy / pixel_aspect;

    if (this->m_apply) {
      BKE_tracking_distortion_undistort_v2(this->m_distortion, xy, distorted_xy);
    }
    else {
      BKE_tracking_distortion_distort_v2(this->m_distortion, xy, distorted_xy);
    }

    const float u = distorted_xy[0] * aspx /* `+ 0.5 * overscan * w` */;
    const float v = (distorted_xy[1] * aspy /* `+ 0.5 * overscan * h` */) * pixel_aspect;
    input_img->read_elem_bilinear(u, v, it.out);
  }
}

}  // namespace blender::compositor

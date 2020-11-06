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
#include "BKE_tracking.h"

#include "BLI_linklist.h"

MovieDistortionOperation::MovieDistortionOperation(bool distortion)
{
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = nullptr;
  this->m_movieClip = nullptr;
  this->m_apply = distortion;
}

void MovieDistortionOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  if (this->m_movieClip) {
    MovieTracking *tracking = &this->m_movieClip->tracking;
    MovieClipUser clipUser = {0};
    int calibration_width, calibration_height;

    BKE_movieclip_user_set_frame(&clipUser, this->m_framenumber);
    BKE_movieclip_get_size(this->m_movieClip, &clipUser, &calibration_width, &calibration_height);

    float delta[2];
    rcti full_frame;
    full_frame.xmin = full_frame.ymin = 0;
    full_frame.xmax = this->m_width;
    full_frame.ymax = this->m_height;
    BKE_tracking_max_distortion_delta_across_bound(
        tracking, this->m_width, this->m_height, &full_frame, !this->m_apply, delta);

    /* 5 is just in case we didn't hit real max of distortion in
     * BKE_tracking_max_undistortion_delta_across_bound
     */
    m_margin[0] = delta[0] + 5;
    m_margin[1] = delta[1] + 5;

    this->m_distortion = BKE_tracking_distortion_new(
        tracking, calibration_width, calibration_height);
    this->m_calibration_width = calibration_width;
    this->m_calibration_height = calibration_height;
    this->m_pixel_aspect = tracking->camera.pixel_aspect;
  }
  else {
    m_margin[0] = m_margin[1] = 0;
    this->m_distortion = nullptr;
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
    const float w = (float)this->m_width /* / (1 + overscan) */;
    const float h = (float)this->m_height /* / (1 + overscan) */;
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

    this->m_inputOperation->readSampled(output, u, v, COM_PS_BILINEAR);
  }
  else {
    this->m_inputOperation->readSampled(output, x, y, COM_PS_BILINEAR);
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

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

#include "COM_MovieClipAttributeOperation.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

namespace blender::compositor {

MovieClipAttributeOperation::MovieClipAttributeOperation()
{
  this->addOutputSocket(DataType::Value);
  m_framenumber = 0;
  m_attribute = MCA_X;
  m_invert = false;
  needs_canvas_to_get_constant_ = true;
  is_value_calculated_ = false;
  stabilization_resolution_socket_ = nullptr;
}

void MovieClipAttributeOperation::initExecution()
{
  if (!is_value_calculated_) {
    calc_value();
  }
}

void MovieClipAttributeOperation::calc_value()
{
  BLI_assert(this->get_flags().is_canvas_set);
  is_value_calculated_ = true;
  if (m_clip == nullptr) {
    return;
  }
  float loc[2], scale, angle;
  loc[0] = 0.0f;
  loc[1] = 0.0f;
  scale = 1.0f;
  angle = 0.0f;
  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(m_clip, m_framenumber);
  NodeOperation &stabilization_operation =
      stabilization_resolution_socket_ ?
          stabilization_resolution_socket_->getLink()->getOperation() :
          *this;
  BKE_tracking_stabilization_data_get(m_clip,
                                      clip_framenr,
                                      stabilization_operation.getWidth(),
                                      stabilization_operation.getHeight(),
                                      loc,
                                      &scale,
                                      &angle);
  switch (m_attribute) {
    case MCA_SCALE:
      m_value = scale;
      break;
    case MCA_ANGLE:
      m_value = angle;
      break;
    case MCA_X:
      m_value = loc[0];
      break;
    case MCA_Y:
      m_value = loc[1];
      break;
  }
  if (m_invert) {
    if (m_attribute != MCA_SCALE) {
      m_value = -m_value;
    }
    else {
      m_value = 1.0f / m_value;
    }
  }
}

void MovieClipAttributeOperation::executePixelSampled(float output[4],
                                                      float /*x*/,
                                                      float /*y*/,
                                                      PixelSampler /*sampler*/)
{
  output[0] = m_value;
}

void MovieClipAttributeOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
}

const float *MovieClipAttributeOperation::get_constant_elem()
{
  if (!is_value_calculated_) {
    calc_value();
  }
  return &m_value;
}

}  // namespace blender::compositor

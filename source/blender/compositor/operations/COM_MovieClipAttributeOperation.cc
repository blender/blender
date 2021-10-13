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
  this->add_output_socket(DataType::Value);
  framenumber_ = 0;
  attribute_ = MCA_X;
  invert_ = false;
  needs_canvas_to_get_constant_ = true;
  is_value_calculated_ = false;
  stabilization_resolution_socket_ = nullptr;
}

void MovieClipAttributeOperation::init_execution()
{
  if (!is_value_calculated_) {
    calc_value();
  }
}

void MovieClipAttributeOperation::calc_value()
{
  BLI_assert(this->get_flags().is_canvas_set);
  is_value_calculated_ = true;
  if (clip_ == nullptr) {
    return;
  }
  float loc[2], scale, angle;
  loc[0] = 0.0f;
  loc[1] = 0.0f;
  scale = 1.0f;
  angle = 0.0f;
  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip_, framenumber_);
  NodeOperation &stabilization_operation =
      stabilization_resolution_socket_ ?
          stabilization_resolution_socket_->get_link()->get_operation() :
          *this;
  BKE_tracking_stabilization_data_get(clip_,
                                      clip_framenr,
                                      stabilization_operation.get_width(),
                                      stabilization_operation.get_height(),
                                      loc,
                                      &scale,
                                      &angle);
  switch (attribute_) {
    case MCA_SCALE:
      value_ = scale;
      break;
    case MCA_ANGLE:
      value_ = angle;
      break;
    case MCA_X:
      value_ = loc[0];
      break;
    case MCA_Y:
      value_ = loc[1];
      break;
  }
  if (invert_) {
    if (attribute_ != MCA_SCALE) {
      value_ = -value_;
    }
    else {
      value_ = 1.0f / value_;
    }
  }
}

void MovieClipAttributeOperation::execute_pixel_sampled(float output[4],
                                                        float /*x*/,
                                                        float /*y*/,
                                                        PixelSampler /*sampler*/)
{
  output[0] = value_;
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
  return &value_;
}

}  // namespace blender::compositor

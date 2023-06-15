/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SetVectorOperation.h"

namespace blender::compositor {

SetVectorOperation::SetVectorOperation()
{
  this->add_output_socket(DataType::Vector);
  flags_.is_set_operation = true;
}

void SetVectorOperation::execute_pixel_sampled(float output[4],
                                               float /*x*/,
                                               float /*y*/,
                                               PixelSampler /*sampler*/)
{
  output[0] = vector_.x;
  output[1] = vector_.y;
  output[2] = vector_.z;
}

void SetVectorOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
}

}  // namespace blender::compositor

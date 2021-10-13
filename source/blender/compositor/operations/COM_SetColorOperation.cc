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

#include "COM_SetColorOperation.h"

namespace blender::compositor {

SetColorOperation::SetColorOperation()
{
  this->add_output_socket(DataType::Color);
  flags_.is_set_operation = true;
}

void SetColorOperation::execute_pixel_sampled(float output[4],
                                              float /*x*/,
                                              float /*y*/,
                                              PixelSampler /*sampler*/)
{
  copy_v4_v4(output, color_);
}

void SetColorOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
}

}  // namespace blender::compositor

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

#pragma once

#include "COM_ReadBufferOperation.h"

namespace blender::compositor {

class WrapOperation : public ReadBufferOperation {
 private:
  int wrapping_type_;

 public:
  WrapOperation(DataType datatype);
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void set_wrapping(int wrapping_type);
  float get_wrapped_original_xpos(float x);
  float get_wrapped_original_ypos(float y);

  void setFactorXY(float factorX, float factorY);
};

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

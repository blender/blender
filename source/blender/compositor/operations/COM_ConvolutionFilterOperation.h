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

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class ConvolutionFilterOperation : public MultiThreadedOperation {
 protected:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int FACTOR_INPUT_INDEX = 1;

 private:
  int filterWidth_;
  int filterHeight_;

 protected:
  SocketReader *inputOperation_;
  SocketReader *inputValueOperation_;
  float filter_[9];

 public:
  ConvolutionFilterOperation();
  void set3x3Filter(
      float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9);
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixel(float output[4], int x, int y, void *data) override;

  void initExecution() override;
  void deinitExecution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

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

class FlipOperation : public MultiThreadedOperation {
 private:
  SocketReader *inputOperation_;
  bool flipX_;
  bool flipY_;

 public:
  FlipOperation();
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;
  void setFlipX(bool flipX)
  {
    flipX_ = flipX;
  }
  void setFlipY(bool flipY)
  {
    flipY_ = flipY;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

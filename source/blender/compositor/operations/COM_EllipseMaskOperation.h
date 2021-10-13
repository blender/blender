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

class EllipseMaskOperation : public MultiThreadedOperation {
 private:
  using MaskFunc = std::function<float(bool is_inside, const float *mask, const float *value)>;

  /**
   * Cached reference to the inputProgram
   */
  SocketReader *inputMask_;
  SocketReader *inputValue_;

  float sine_;
  float cosine_;
  float aspectRatio_;
  int maskType_;

  NodeEllipseMask *data_;

 public:
  EllipseMaskOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  void setData(NodeEllipseMask *data)
  {
    data_ = data;
  }

  void setMaskType(int maskType)
  {
    maskType_ = maskType;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void apply_mask(MemoryBuffer *output,
                  const rcti &area,
                  Span<MemoryBuffer *> inputs,
                  MaskFunc mask_func);
};

}  // namespace blender::compositor

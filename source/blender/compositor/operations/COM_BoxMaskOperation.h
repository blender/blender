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

class BoxMaskOperation : public MultiThreadedOperation {
 private:
  using MaskFunc = std::function<float(bool is_inside, const float *mask, const float *value)>;

  /**
   * Cached reference to the inputProgram
   */
  SocketReader *m_inputMask;
  SocketReader *m_inputValue;

  float m_sine;
  float m_cosine;
  float m_aspectRatio;
  int m_maskType;

  NodeBoxMask *m_data;

 public:
  BoxMaskOperation();

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

  void setData(NodeBoxMask *data)
  {
    m_data = data;
  }

  void setMaskType(int maskType)
  {
    m_maskType = maskType;
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

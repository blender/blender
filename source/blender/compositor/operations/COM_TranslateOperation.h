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

#include "COM_ConstantOperation.h"
#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class TranslateOperation : public MultiThreadedOperation {
 private:
  SocketReader *m_inputOperation;
  SocketReader *m_inputXOperation;
  SocketReader *m_inputYOperation;
  float m_deltaX;
  float m_deltaY;
  bool m_isDeltaSet;
  float m_factorX;
  float m_factorY;
  MemoryBufferExtend x_extend_mode_;
  MemoryBufferExtend y_extend_mode_;

 public:
  TranslateOperation();
  TranslateOperation(DataType data_type);
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;

  float getDeltaX()
  {
    return this->m_deltaX * this->m_factorX;
  }
  float getDeltaY()
  {
    return this->m_deltaY * this->m_factorY;
  }

  inline void ensureDelta()
  {
    if (!this->m_isDeltaSet) {
      if (execution_model_ == eExecutionModel::Tiled) {
        float tempDelta[4];
        this->m_inputXOperation->readSampled(tempDelta, 0, 0, PixelSampler::Nearest);
        this->m_deltaX = tempDelta[0];
        this->m_inputYOperation->readSampled(tempDelta, 0, 0, PixelSampler::Nearest);
        this->m_deltaY = tempDelta[0];
      }
      else {
        this->m_deltaX = 0;
        NodeOperation *x_op = getInputOperation(1);
        if (x_op->get_flags().is_constant_operation) {
          this->m_deltaX = ((ConstantOperation *)x_op)->get_constant_elem()[0];
        }
        this->m_deltaY = 0;
        NodeOperation *y_op = getInputOperation(2);
        if (y_op->get_flags().is_constant_operation) {
          this->m_deltaY = ((ConstantOperation *)y_op)->get_constant_elem()[0];
        }
      }

      this->m_isDeltaSet = true;
    }
  }

  void setFactorXY(float factorX, float factorY);
  void set_wrapping(int wrapping_type);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

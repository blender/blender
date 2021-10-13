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
 protected:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int X_INPUT_INDEX = 1;
  static constexpr int Y_INPUT_INDEX = 2;

 private:
  SocketReader *inputOperation_;
  SocketReader *inputXOperation_;
  SocketReader *inputYOperation_;
  float deltaX_;
  float deltaY_;
  bool isDeltaSet_;
  float factorX_;
  float factorY_;

 protected:
  MemoryBufferExtend x_extend_mode_;
  MemoryBufferExtend y_extend_mode_;

 public:
  TranslateOperation();
  TranslateOperation(DataType data_type, ResizeMode mode = ResizeMode::Center);
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;

  float getDeltaX()
  {
    return deltaX_ * factorX_;
  }
  float getDeltaY()
  {
    return deltaY_ * factorY_;
  }

  inline void ensureDelta()
  {
    if (!isDeltaSet_) {
      if (execution_model_ == eExecutionModel::Tiled) {
        float tempDelta[4];
        inputXOperation_->readSampled(tempDelta, 0, 0, PixelSampler::Nearest);
        deltaX_ = tempDelta[0];
        inputYOperation_->readSampled(tempDelta, 0, 0, PixelSampler::Nearest);
        deltaY_ = tempDelta[0];
      }
      else {
        deltaX_ = get_input_operation(X_INPUT_INDEX)->get_constant_value_default(0.0f);
        deltaY_ = get_input_operation(Y_INPUT_INDEX)->get_constant_value_default(0.0f);
      }

      isDeltaSet_ = true;
    }
  }

  void setFactorXY(float factorX, float factorY);
  void set_wrapping(int wrapping_type);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class TranslateCanvasOperation : public TranslateOperation {
 public:
  TranslateCanvasOperation();
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
};

}  // namespace blender::compositor

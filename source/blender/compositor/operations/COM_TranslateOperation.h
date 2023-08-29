/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  SocketReader *input_operation_;
  SocketReader *input_xoperation_;
  SocketReader *input_yoperation_;
  float delta_x_;
  float delta_y_;
  bool is_delta_set_;
  float factor_x_;
  float factor_y_;

 protected:
  MemoryBufferExtend x_extend_mode_;
  MemoryBufferExtend y_extend_mode_;

 public:
  TranslateOperation();
  TranslateOperation(DataType data_type, ResizeMode mode = ResizeMode::Center);
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void init_execution() override;
  void deinit_execution() override;

  float getDeltaX()
  {
    return delta_x_ * factor_x_;
  }
  float getDeltaY()
  {
    return delta_y_ * factor_y_;
  }

  inline void ensure_delta()
  {
    if (!is_delta_set_) {
      if (execution_model_ == eExecutionModel::Tiled) {
        float temp_delta[4];
        input_xoperation_->read_sampled(temp_delta, 0, 0, PixelSampler::Nearest);
        delta_x_ = temp_delta[0];
        input_yoperation_->read_sampled(temp_delta, 0, 0, PixelSampler::Nearest);
        delta_y_ = temp_delta[0];
      }
      else {
        delta_x_ = get_input_operation(X_INPUT_INDEX)->get_constant_value_default(0.0f);
        delta_y_ = get_input_operation(Y_INPUT_INDEX)->get_constant_value_default(0.0f);
      }

      is_delta_set_ = true;
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

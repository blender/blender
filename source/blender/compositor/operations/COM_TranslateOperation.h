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
  bool is_relative_;

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

  float get_delta_x()
  {
    return delta_x_;
  }
  float get_delta_y()
  {
    return delta_y_;
  }

  void set_is_relative(const bool is_relative)
  {
    is_relative_ = is_relative;
  }
  bool get_is_relative()
  {
    return is_relative_;
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
        if (get_is_relative()) {
          const int input_width = BLI_rcti_size_x(&input_operation_->get_canvas());
          const int input_height = BLI_rcti_size_y(&input_operation_->get_canvas());
          delta_x_ *= input_width;
          delta_y_ *= input_height;
        }
      }
      else {
        delta_x_ = get_input_operation(X_INPUT_INDEX)->get_constant_value_default(0.0f);
        delta_y_ = get_input_operation(Y_INPUT_INDEX)->get_constant_value_default(0.0f);
        if (get_is_relative()) {
          const int input_width = BLI_rcti_size_x(
              &get_input_operation(IMAGE_INPUT_INDEX)->get_canvas());
          const int input_height = BLI_rcti_size_y(
              &get_input_operation(IMAGE_INPUT_INDEX)->get_canvas());
          delta_x_ *= input_width;
          delta_y_ *= input_height;
        }
      }

      is_delta_set_ = true;
    }
  }
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

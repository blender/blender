/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConstantOperation.h"
#include "COM_MultiThreadedOperation.h"

#include <mutex>

namespace blender::compositor {

class TranslateOperation : public MultiThreadedOperation {
 protected:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int X_INPUT_INDEX = 1;
  static constexpr int Y_INPUT_INDEX = 2;

 private:
  float delta_x_;
  float delta_y_;
  bool is_delta_set_;
  bool is_relative_;

  std::mutex mutex_;

 protected:
  MemoryBufferExtend x_extend_mode_;
  MemoryBufferExtend y_extend_mode_;

 public:
  TranslateOperation();
  TranslateOperation(DataType data_type, ResizeMode mode = ResizeMode::Center);

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
      std::unique_lock lock(mutex_);
      if (is_delta_set_) {
        return;
      }

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

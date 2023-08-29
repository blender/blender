/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class ConvolutionFilterOperation : public MultiThreadedOperation {
 protected:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int FACTOR_INPUT_INDEX = 1;

 private:
  int filter_width_;
  int filter_height_;

 protected:
  SocketReader *input_operation_;
  SocketReader *input_value_operation_;
  float filter_[9];

 public:
  ConvolutionFilterOperation();
  void set3x3Filter(
      float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9);
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void init_execution() override;
  void deinit_execution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

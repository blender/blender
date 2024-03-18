/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvolutionFilterOperation.h"

namespace blender::compositor {

ConvolutionFilterOperation::ConvolutionFilterOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
  flags_.complex = true;
  flags_.can_be_constant = true;
}
void ConvolutionFilterOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
  input_value_operation_ = this->get_input_socket_reader(1);
}

void ConvolutionFilterOperation::set3x3Filter(
    float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9)
{
  filter_[0] = f1;
  filter_[1] = f2;
  filter_[2] = f3;
  filter_[3] = f4;
  filter_[4] = f5;
  filter_[5] = f6;
  filter_[6] = f7;
  filter_[7] = f8;
  filter_[8] = f9;
  filter_height_ = 3;
  filter_width_ = 3;
}

void ConvolutionFilterOperation::deinit_execution()
{
  input_operation_ = nullptr;
  input_value_operation_ = nullptr;
}

void ConvolutionFilterOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  float in1[4];
  float in2[4];
  int x1 = x - 1;
  int x2 = x;
  int x3 = x + 1;
  int y1 = y - 1;
  int y2 = y;
  int y3 = y + 1;
  CLAMP(x1, 0, get_width() - 1);
  CLAMP(x2, 0, get_width() - 1);
  CLAMP(x3, 0, get_width() - 1);
  CLAMP(y1, 0, get_height() - 1);
  CLAMP(y2, 0, get_height() - 1);
  CLAMP(y3, 0, get_height() - 1);
  float value[4];
  input_value_operation_->read(value, x2, y2, nullptr);
  const float mval = 1.0f - value[0];

  zero_v4(output);
  input_operation_->read(in1, x1, y1, nullptr);
  madd_v4_v4fl(output, in1, filter_[0]);
  input_operation_->read(in1, x2, y1, nullptr);
  madd_v4_v4fl(output, in1, filter_[1]);
  input_operation_->read(in1, x3, y1, nullptr);
  madd_v4_v4fl(output, in1, filter_[2]);
  input_operation_->read(in1, x1, y2, nullptr);
  madd_v4_v4fl(output, in1, filter_[3]);
  input_operation_->read(in2, x2, y2, nullptr);
  madd_v4_v4fl(output, in2, filter_[4]);
  input_operation_->read(in1, x3, y2, nullptr);
  madd_v4_v4fl(output, in1, filter_[5]);
  input_operation_->read(in1, x1, y3, nullptr);
  madd_v4_v4fl(output, in1, filter_[6]);
  input_operation_->read(in1, x2, y3, nullptr);
  madd_v4_v4fl(output, in1, filter_[7]);
  input_operation_->read(in1, x3, y3, nullptr);
  madd_v4_v4fl(output, in1, filter_[8]);

  output[0] = output[0] * value[0] + in2[0] * mval;
  output[1] = output[1] * value[0] + in2[1] * mval;
  output[2] = output[2] * value[0] + in2[2] * mval;
  output[3] = output[3] * value[0] + in2[3] * mval;

  /* Make sure we don't return negative color. */
  output[0] = std::max(output[0], 0.0f);
  output[1] = std::max(output[1], 0.0f);
  output[2] = std::max(output[2], 0.0f);
  output[3] = std::max(output[3], 0.0f);
}

bool ConvolutionFilterOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  int addx = (filter_width_ - 1) / 2 + 1;
  int addy = (filter_height_ - 1) / 2 + 1;
  new_input.xmax = input->xmax + addx;
  new_input.xmin = input->xmin - addx;
  new_input.ymax = input->ymax + addy;
  new_input.ymin = input->ymin - addy;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void ConvolutionFilterOperation::get_area_of_interest(const int input_idx,
                                                      const rcti &output_area,
                                                      rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      const int add_x = (filter_width_ - 1) / 2 + 1;
      const int add_y = (filter_height_ - 1) / 2 + 1;
      r_input_area.xmin = output_area.xmin - add_x;
      r_input_area.xmax = output_area.xmax + add_x;
      r_input_area.ymin = output_area.ymin - add_y;
      r_input_area.ymax = output_area.ymax + add_y;
      break;
    }
    case FACTOR_INPUT_INDEX: {
      r_input_area = output_area;
      break;
    }
  }
}

void ConvolutionFilterOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *image = inputs[IMAGE_INPUT_INDEX];
  const int last_x = get_width() - 1;
  const int last_y = get_height() - 1;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int left_offset = (it.x == 0) ? 0 : -image->elem_stride;
    const int right_offset = (it.x == last_x) ? 0 : image->elem_stride;
    const int down_offset = (it.y == 0) ? 0 : -image->row_stride;
    const int up_offset = (it.y == last_y) ? 0 : image->row_stride;

    const float *center_color = it.in(IMAGE_INPUT_INDEX);
    zero_v4(it.out);
    madd_v4_v4fl(it.out, center_color + down_offset + left_offset, filter_[0]);
    madd_v4_v4fl(it.out, center_color + down_offset, filter_[1]);
    madd_v4_v4fl(it.out, center_color + down_offset + right_offset, filter_[2]);
    madd_v4_v4fl(it.out, center_color + left_offset, filter_[3]);
    madd_v4_v4fl(it.out, center_color, filter_[4]);
    madd_v4_v4fl(it.out, center_color + right_offset, filter_[5]);
    madd_v4_v4fl(it.out, center_color + up_offset + left_offset, filter_[6]);
    madd_v4_v4fl(it.out, center_color + up_offset, filter_[7]);
    madd_v4_v4fl(it.out, center_color + up_offset + right_offset, filter_[8]);

    const float factor = *it.in(FACTOR_INPUT_INDEX);
    const float factor_ = 1.0f - factor;
    it.out[0] = it.out[0] * factor + center_color[0] * factor_;
    it.out[1] = it.out[1] * factor + center_color[1] * factor_;
    it.out[2] = it.out[2] * factor + center_color[2] * factor_;
    it.out[3] = it.out[3] * factor + center_color[3] * factor_;

    /* Make sure we don't return negative color. */
    CLAMP4_MIN(it.out, 0.0f);
  }
}

}  // namespace blender::compositor

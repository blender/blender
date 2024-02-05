/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvolutionEdgeFilterOperation.h"

namespace blender::compositor {

void ConvolutionEdgeFilterOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  float in1[4], in2[4], res1[4] = {0.0}, res2[4] = {0.0};

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
  float mval = 1.0f - value[0];

  input_operation_->read(in1, x1, y1, nullptr);
  madd_v3_v3fl(res1, in1, filter_[0]);
  madd_v3_v3fl(res2, in1, filter_[0]);

  input_operation_->read(in1, x2, y1, nullptr);
  madd_v3_v3fl(res1, in1, filter_[1]);
  madd_v3_v3fl(res2, in1, filter_[3]);

  input_operation_->read(in1, x3, y1, nullptr);
  madd_v3_v3fl(res1, in1, filter_[2]);
  madd_v3_v3fl(res2, in1, filter_[6]);

  input_operation_->read(in1, x1, y2, nullptr);
  madd_v3_v3fl(res1, in1, filter_[3]);
  madd_v3_v3fl(res2, in1, filter_[1]);

  input_operation_->read(in2, x2, y2, nullptr);
  madd_v3_v3fl(res1, in2, filter_[4]);
  madd_v3_v3fl(res2, in2, filter_[4]);

  input_operation_->read(in1, x3, y2, nullptr);
  madd_v3_v3fl(res1, in1, filter_[5]);
  madd_v3_v3fl(res2, in1, filter_[7]);

  input_operation_->read(in1, x1, y3, nullptr);
  madd_v3_v3fl(res1, in1, filter_[6]);
  madd_v3_v3fl(res2, in1, filter_[2]);

  input_operation_->read(in1, x2, y3, nullptr);
  madd_v3_v3fl(res1, in1, filter_[7]);
  madd_v3_v3fl(res2, in1, filter_[5]);

  input_operation_->read(in1, x3, y3, nullptr);
  madd_v3_v3fl(res1, in1, filter_[8]);
  madd_v3_v3fl(res2, in1, filter_[8]);

  output[0] = sqrt(res1[0] * res1[0] + res2[0] * res2[0]);
  output[1] = sqrt(res1[1] * res1[1] + res2[1] * res2[1]);
  output[2] = sqrt(res1[2] * res1[2] + res2[2] * res2[2]);

  output[0] = output[0] * value[0] + in2[0] * mval;
  output[1] = output[1] * value[0] + in2[1] * mval;
  output[2] = output[2] * value[0] + in2[2] * mval;

  output[3] = in2[3];

  /* Make sure we don't return negative color. */
  output[0] = std::max(output[0], 0.0f);
  output[1] = std::max(output[1], 0.0f);
  output[2] = std::max(output[2], 0.0f);
  output[3] = std::max(output[3], 0.0f);
}

void ConvolutionEdgeFilterOperation::update_memory_buffer_partial(MemoryBuffer *output,
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
    float res1[4] = {0};
    float res2[4] = {0};

    const float *color = center_color + down_offset + left_offset;
    madd_v3_v3fl(res1, color, filter_[0]);
    copy_v3_v3(res2, res1);

    color = center_color + down_offset;
    madd_v3_v3fl(res1, color, filter_[1]);
    madd_v3_v3fl(res2, color, filter_[3]);

    color = center_color + down_offset + right_offset;
    madd_v3_v3fl(res1, color, filter_[2]);
    madd_v3_v3fl(res2, color, filter_[6]);

    color = center_color + left_offset;
    madd_v3_v3fl(res1, color, filter_[3]);
    madd_v3_v3fl(res2, color, filter_[1]);

    {
      float rgb_filtered[3];
      mul_v3_v3fl(rgb_filtered, center_color, filter_[4]);
      add_v3_v3(res1, rgb_filtered);
      add_v3_v3(res2, rgb_filtered);
    }

    color = center_color + right_offset;
    madd_v3_v3fl(res1, color, filter_[5]);
    madd_v3_v3fl(res2, color, filter_[7]);

    color = center_color + up_offset + left_offset;
    madd_v3_v3fl(res1, color, filter_[6]);
    madd_v3_v3fl(res2, color, filter_[2]);

    color = center_color + up_offset;
    madd_v3_v3fl(res1, color, filter_[7]);
    madd_v3_v3fl(res2, color, filter_[5]);

    {
      color = center_color + up_offset + right_offset;
      float rgb_filtered[3];
      mul_v3_v3fl(rgb_filtered, color, filter_[8]);
      add_v3_v3(res1, rgb_filtered);
      add_v3_v3(res2, rgb_filtered);
    }

    it.out[0] = sqrt(res1[0] * res1[0] + res2[0] * res2[0]);
    it.out[1] = sqrt(res1[1] * res1[1] + res2[1] * res2[1]);
    it.out[2] = sqrt(res1[2] * res1[2] + res2[2] * res2[2]);

    const float factor = *it.in(FACTOR_INPUT_INDEX);
    const float factor_ = 1.0f - factor;
    it.out[0] = it.out[0] * factor + center_color[0] * factor_;
    it.out[1] = it.out[1] * factor + center_color[1] * factor_;
    it.out[2] = it.out[2] * factor + center_color[2] * factor_;

    it.out[3] = center_color[3];

    /* Make sure we don't return negative color. */
    CLAMP4_MIN(it.out, 0.0f);
  }
}

}  // namespace blender::compositor

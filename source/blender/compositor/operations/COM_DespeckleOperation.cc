/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "COM_DespeckleOperation.h"

namespace blender::compositor {

DespeckleOperation::DespeckleOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
  flags_.complex = true;
}
void DespeckleOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
  input_value_operation_ = this->get_input_socket_reader(1);
}

void DespeckleOperation::deinit_execution()
{
  input_operation_ = nullptr;
  input_value_operation_ = nullptr;
}

BLI_INLINE int color_diff(const float a[3], const float b[3], const float threshold)
{
  return ((fabsf(a[0] - b[0]) > threshold) || (fabsf(a[1] - b[1]) > threshold) ||
          (fabsf(a[2] - b[2]) > threshold));
}

void DespeckleOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  float w = 0.0f;
  float color_org[4];
  float color_mid[4];
  float color_mid_ok[4];
  float in1[4];
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
  // const float mval = 1.0f - value[0];

  input_operation_->read(color_org, x2, y2, nullptr);

#define TOT_DIV_ONE 1.0f
#define TOT_DIV_CNR float(M_SQRT1_2)

#define WTOT (TOT_DIV_ONE * 4 + TOT_DIV_CNR * 4)

#define COLOR_ADD(fac) \
  { \
    madd_v4_v4fl(color_mid, in1, fac); \
    if (color_diff(in1, color_org, threshold_)) { \
      w += fac; \
      madd_v4_v4fl(color_mid_ok, in1, fac); \
    } \
  }

  zero_v4(color_mid);
  zero_v4(color_mid_ok);

  input_operation_->read(in1, x1, y1, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  input_operation_->read(in1, x2, y1, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  input_operation_->read(in1, x3, y1, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  input_operation_->read(in1, x1, y2, nullptr);
  COLOR_ADD(TOT_DIV_ONE)

#if 0
  input_operation_->read(in2, x2, y2, nullptr);
  madd_v4_v4fl(color_mid, in2, filter_[4]);
#endif

  input_operation_->read(in1, x3, y2, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  input_operation_->read(in1, x1, y3, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  input_operation_->read(in1, x2, y3, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  input_operation_->read(in1, x3, y3, nullptr);
  COLOR_ADD(TOT_DIV_CNR)

  mul_v4_fl(color_mid, 1.0f / (4.0f + (4.0f * float(M_SQRT1_2))));
  // mul_v4_fl(color_mid, 1.0f / w);

  if ((w != 0.0f) && ((w / WTOT) > (threshold_neighbor_)) &&
      color_diff(color_mid, color_org, threshold_))
  {
    mul_v4_fl(color_mid_ok, 1.0f / w);
    interp_v4_v4v4(output, color_org, color_mid_ok, value[0]);
  }
  else {
    copy_v4_v4(output, color_org);
  }

#undef TOT_DIV_ONE
#undef TOT_DIV_CNR
#undef WTOT
#undef COLOR_ADD
}

bool DespeckleOperation::determine_depending_area_of_interest(rcti *input,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  rcti new_input;
  int addx = 2;  //(filter_width_ - 1) / 2 + 1;
  int addy = 2;  //(filter_height_ - 1) / 2 + 1;
  new_input.xmax = input->xmax + addx;
  new_input.xmin = input->xmin - addx;
  new_input.ymax = input->ymax + addy;
  new_input.ymin = input->ymin - addy;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void DespeckleOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      const int add_x = 2;  //(filter_width_ - 1) / 2 + 1;
      const int add_y = 2;  //(filter_height_ - 1) / 2 + 1;
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

void DespeckleOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *image = inputs[IMAGE_INPUT_INDEX];
  const int last_x = get_width() - 1;
  const int last_y = get_height() - 1;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int x1 = MAX2(it.x - 1, 0);
    const int x2 = it.x;
    const int x3 = MIN2(it.x + 1, last_x);
    const int y1 = MAX2(it.y - 1, 0);
    const int y2 = it.y;
    const int y3 = MIN2(it.y + 1, last_y);

    float w = 0.0f;
    const float *color_org = it.in(IMAGE_INPUT_INDEX);
    float color_mid[4];
    float color_mid_ok[4];
    const float *in1 = nullptr;

#define TOT_DIV_ONE 1.0f
#define TOT_DIV_CNR float(M_SQRT1_2)

#define WTOT (TOT_DIV_ONE * 4 + TOT_DIV_CNR * 4)

#define COLOR_ADD(fac) \
  { \
    madd_v4_v4fl(color_mid, in1, fac); \
    if (color_diff(in1, color_org, threshold_)) { \
      w += fac; \
      madd_v4_v4fl(color_mid_ok, in1, fac); \
    } \
  }

    zero_v4(color_mid);
    zero_v4(color_mid_ok);

    in1 = image->get_elem(x1, y1);
    COLOR_ADD(TOT_DIV_CNR)
    in1 = image->get_elem(x2, y1);
    COLOR_ADD(TOT_DIV_ONE)
    in1 = image->get_elem(x3, y1);
    COLOR_ADD(TOT_DIV_CNR)
    in1 = image->get_elem(x1, y2);
    COLOR_ADD(TOT_DIV_ONE)

#if 0
  const float* in2 = image->get_elem(x2, y2);
  madd_v4_v4fl(color_mid, in2, filter_[4]);
#endif

    in1 = image->get_elem(x3, y2);
    COLOR_ADD(TOT_DIV_ONE)
    in1 = image->get_elem(x1, y3);
    COLOR_ADD(TOT_DIV_CNR)
    in1 = image->get_elem(x2, y3);
    COLOR_ADD(TOT_DIV_ONE)
    in1 = image->get_elem(x3, y3);
    COLOR_ADD(TOT_DIV_CNR)

    mul_v4_fl(color_mid, 1.0f / (4.0f + (4.0f * float(M_SQRT1_2))));
    // mul_v4_fl(color_mid, 1.0f / w);

    if ((w != 0.0f) && ((w / WTOT) > (threshold_neighbor_)) &&
        color_diff(color_mid, color_org, threshold_))
    {
      const float factor = *it.in(FACTOR_INPUT_INDEX);
      mul_v4_fl(color_mid_ok, 1.0f / w);
      interp_v4_v4v4(it.out, color_org, color_mid_ok, factor);
    }
    else {
      copy_v4_v4(it.out, color_org);
    }

#undef TOT_DIV_ONE
#undef TOT_DIV_CNR
#undef WTOT
#undef COLOR_ADD
  }
}

}  // namespace blender::compositor

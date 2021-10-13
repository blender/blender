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

#include "MEM_guardedalloc.h"

#include "COM_DespeckleOperation.h"

namespace blender::compositor {

DespeckleOperation::DespeckleOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  this->set_canvas_input_index(0);
  m_inputOperation = nullptr;
  this->flags.complex = true;
}
void DespeckleOperation::initExecution()
{
  m_inputOperation = this->getInputSocketReader(0);
  m_inputValueOperation = this->getInputSocketReader(1);
}

void DespeckleOperation::deinitExecution()
{
  m_inputOperation = nullptr;
  m_inputValueOperation = nullptr;
}

BLI_INLINE int color_diff(const float a[3], const float b[3], const float threshold)
{
  return ((fabsf(a[0] - b[0]) > threshold) || (fabsf(a[1] - b[1]) > threshold) ||
          (fabsf(a[2] - b[2]) > threshold));
}

void DespeckleOperation::executePixel(float output[4], int x, int y, void * /*data*/)
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
  CLAMP(x1, 0, getWidth() - 1);
  CLAMP(x2, 0, getWidth() - 1);
  CLAMP(x3, 0, getWidth() - 1);
  CLAMP(y1, 0, getHeight() - 1);
  CLAMP(y2, 0, getHeight() - 1);
  CLAMP(y3, 0, getHeight() - 1);
  float value[4];
  m_inputValueOperation->read(value, x2, y2, nullptr);
  // const float mval = 1.0f - value[0];

  m_inputOperation->read(color_org, x2, y2, nullptr);

#define TOT_DIV_ONE 1.0f
#define TOT_DIV_CNR (float)M_SQRT1_2

#define WTOT (TOT_DIV_ONE * 4 + TOT_DIV_CNR * 4)

#define COLOR_ADD(fac) \
  { \
    madd_v4_v4fl(color_mid, in1, fac); \
    if (color_diff(in1, color_org, m_threshold)) { \
      w += fac; \
      madd_v4_v4fl(color_mid_ok, in1, fac); \
    } \
  }

  zero_v4(color_mid);
  zero_v4(color_mid_ok);

  m_inputOperation->read(in1, x1, y1, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  m_inputOperation->read(in1, x2, y1, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  m_inputOperation->read(in1, x3, y1, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  m_inputOperation->read(in1, x1, y2, nullptr);
  COLOR_ADD(TOT_DIV_ONE)

#if 0
  m_inputOperation->read(in2, x2, y2, nullptr);
  madd_v4_v4fl(color_mid, in2, m_filter[4]);
#endif

  m_inputOperation->read(in1, x3, y2, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  m_inputOperation->read(in1, x1, y3, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  m_inputOperation->read(in1, x2, y3, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  m_inputOperation->read(in1, x3, y3, nullptr);
  COLOR_ADD(TOT_DIV_CNR)

  mul_v4_fl(color_mid, 1.0f / (4.0f + (4.0f * (float)M_SQRT1_2)));
  // mul_v4_fl(color_mid, 1.0f / w);

  if ((w != 0.0f) && ((w / WTOT) > (m_threshold_neighbor)) &&
      color_diff(color_mid, color_org, m_threshold)) {
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

bool DespeckleOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti newInput;
  int addx = 2;  //(m_filterWidth - 1) / 2 + 1;
  int addy = 2;  //(m_filterHeight - 1) / 2 + 1;
  newInput.xmax = input->xmax + addx;
  newInput.xmin = input->xmin - addx;
  newInput.ymax = input->ymax + addy;
  newInput.ymin = input->ymin - addy;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void DespeckleOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      const int add_x = 2;  //(m_filterWidth - 1) / 2 + 1;
      const int add_y = 2;  //(m_filterHeight - 1) / 2 + 1;
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
  const int last_x = getWidth() - 1;
  const int last_y = getHeight() - 1;
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
#define TOT_DIV_CNR (float)M_SQRT1_2

#define WTOT (TOT_DIV_ONE * 4 + TOT_DIV_CNR * 4)

#define COLOR_ADD(fac) \
  { \
    madd_v4_v4fl(color_mid, in1, fac); \
    if (color_diff(in1, color_org, m_threshold)) { \
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
  madd_v4_v4fl(color_mid, in2, m_filter[4]);
#endif

    in1 = image->get_elem(x3, y2);
    COLOR_ADD(TOT_DIV_ONE)
    in1 = image->get_elem(x1, y3);
    COLOR_ADD(TOT_DIV_CNR)
    in1 = image->get_elem(x2, y3);
    COLOR_ADD(TOT_DIV_ONE)
    in1 = image->get_elem(x3, y3);
    COLOR_ADD(TOT_DIV_CNR)

    mul_v4_fl(color_mid, 1.0f / (4.0f + (4.0f * (float)M_SQRT1_2)));
    // mul_v4_fl(color_mid, 1.0f / w);

    if ((w != 0.0f) && ((w / WTOT) > (m_threshold_neighbor)) &&
        color_diff(color_mid, color_org, m_threshold)) {
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

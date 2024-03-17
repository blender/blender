/* SPDX-FileCopyrightText: 2011 Blender Authors
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
  flags_.can_be_constant = true;
}

BLI_INLINE int color_diff(const float a[3], const float b[3], const float threshold)
{
  return ((fabsf(a[0] - b[0]) > threshold) || (fabsf(a[1] - b[1]) > threshold) ||
          (fabsf(a[2] - b[2]) > threshold));
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
    const int x1 = std::max(it.x - 1, 0);
    const int x2 = it.x;
    const int x3 = std::min(it.x + 1, last_x);
    const int y1 = std::max(it.y - 1, 0);
    const int y2 = it.y;
    const int y3 = std::min(it.y + 1, last_y);

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
    const float *in2 = image->get_elem(x2, y2);
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

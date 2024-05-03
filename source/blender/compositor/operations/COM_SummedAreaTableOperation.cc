/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "COM_SummedAreaTableOperation.h"

namespace blender::compositor {

SummedAreaTableOperation::SummedAreaTableOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  mode_ = eMode::Identity;

  this->flags_.can_be_constant = true;
}

void SummedAreaTableOperation::get_area_of_interest(int input_idx,
                                                    const rcti & /*output_area*/,
                                                    rcti &r_input_area)
{
  r_input_area = get_input_operation(input_idx)->get_canvas();
}

void SummedAreaTableOperation::update_memory_buffer(MemoryBuffer *output,
                                                    const rcti &area,
                                                    Span<MemoryBuffer *> inputs)
{
  /* NOTE: although this is a single threaded call, multithreading is used. */
  MemoryBuffer *image = inputs[0];

  /* First pass: copy input to output and sum horizontally. */
  threading::parallel_for(
      IndexRange(area.ymin, area.ymax - area.ymin), 1, [&](const IndexRange range_y) {
        for (const int y : range_y) {
          float4 accumulated_color = float4(0.0f);
          for (const int x : IndexRange(area.xmin, area.xmax - area.xmin)) {
            const float4 color = float4(image->get_elem(x, y));
            accumulated_color += mode_ == eMode::Squared ? color * color : color;
            copy_v4_v4(output->get_elem(x, y), accumulated_color);
          }
        }
      });

  /* Second pass: vertical sum. */
  threading::parallel_for(
      IndexRange(area.xmin, area.xmax - area.xmin), 1, [&](const IndexRange range_x) {
        for (const int x : range_x) {
          float4 accumulated_color = float4(0.0f);
          for (const int y : IndexRange(area.ymin, area.ymax - area.ymin)) {
            const float4 color = float4(output->get_elem(x, y));
            accumulated_color += color;
            copy_v4_v4(output->get_elem(x, y), accumulated_color);
          }
        }
      });
}

void SummedAreaTableOperation::set_mode(eMode mode)
{
  mode_ = mode;
}

SummedAreaTableOperation::eMode SummedAreaTableOperation::get_mode()
{
  return mode_;
}

float4 summed_area_table_sum(MemoryBuffer *buffer, const rcti &area)
{
  /*
   * a, b, c and d are the bounding box of the given area. They are defined as follows:
   *
   *     y
   *     ▲
   *     │
   *     ├──────x───────x
   *     │      │c     d│
   *     ├──────x───────x
   *     │      │a     b│
   *     └──────┴───────┴──────► x
   *
   * NOTE: this is the same definition as in https://en.wikipedia.org/wiki/Summed-area_table
   * but using the blender convention with the origin being at the lower left.
   */

  BLI_assert(area.xmin <= area.xmax && area.ymin <= area.ymax);

  int2 lower_bound(area.xmin, area.ymin);
  int2 upper_bound(area.xmax, area.ymax);

  int2 corrected_lower_bound = lower_bound - int2(1, 1);
  int2 corrected_upper_bound;
  corrected_upper_bound[0] = math::min(buffer->get_width() - 1, upper_bound[0]);
  corrected_upper_bound[1] = math::min(buffer->get_height() - 1, upper_bound[1]);

  float4 a, b, c, d, addend, substrahend;
  buffer->read_elem_checked(corrected_upper_bound[0], corrected_upper_bound[1], a);
  buffer->read_elem_checked(corrected_lower_bound[0], corrected_lower_bound[1], d);
  addend = a + d;

  buffer->read_elem_checked(corrected_lower_bound[0], corrected_upper_bound[1], b);
  buffer->read_elem_checked(corrected_upper_bound[0], corrected_lower_bound[1], c);
  substrahend = b + c;

  float4 sum = addend - substrahend;

  return sum;
}

}  // namespace blender::compositor

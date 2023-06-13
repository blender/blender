/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>

#include "COM_WrapOperation.h"

namespace blender::compositor {

WrapOperation::WrapOperation(DataType datatype) : ReadBufferOperation(datatype)
{
  wrapping_type_ = CMP_NODE_WRAP_NONE;
}

inline float WrapOperation::get_wrapped_original_xpos(float x)
{
  if (this->get_width() == 0) {
    return 0;
  }
  while (x < 0) {
    x += this->get_width();
  }
  return fmodf(x, this->get_width());
}

inline float WrapOperation::get_wrapped_original_ypos(float y)
{
  if (this->get_height() == 0) {
    return 0;
  }
  while (y < 0) {
    y += this->get_height();
  }
  return fmodf(y, this->get_height());
}

void WrapOperation::execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler)
{
  float nx, ny;
  nx = x;
  ny = y;
  MemoryBufferExtend extend_x = MemoryBufferExtend::Clip, extend_y = MemoryBufferExtend::Clip;
  switch (wrapping_type_) {
    case CMP_NODE_WRAP_NONE:
      /* Intentionally empty, original_xpos and original_ypos have been set before. */
      break;
    case CMP_NODE_WRAP_X:
      /* Wrap only on the x-axis. */
      nx = this->get_wrapped_original_xpos(x);
      extend_x = MemoryBufferExtend::Repeat;
      break;
    case CMP_NODE_WRAP_Y:
      /* Wrap only on the y-axis. */
      ny = this->get_wrapped_original_ypos(y);
      extend_y = MemoryBufferExtend::Repeat;
      break;
    case CMP_NODE_WRAP_XY:
      /* Wrap on both. */
      nx = this->get_wrapped_original_xpos(x);
      ny = this->get_wrapped_original_ypos(y);
      extend_x = MemoryBufferExtend::Repeat;
      extend_y = MemoryBufferExtend::Repeat;
      break;
  }

  execute_pixel_extend(output, nx, ny, sampler, extend_x, extend_y);
}

bool WrapOperation::determine_depending_area_of_interest(rcti *input,
                                                         ReadBufferOperation *read_operation,
                                                         rcti *output)
{
  rcti new_input;
  new_input.xmin = input->xmin;
  new_input.xmax = input->xmax;
  new_input.ymin = input->ymin;
  new_input.ymax = input->ymax;

  if (ELEM(wrapping_type_, CMP_NODE_WRAP_X, CMP_NODE_WRAP_XY)) {
    /* Wrap only on the x-axis if tile is wrapping. */
    new_input.xmin = get_wrapped_original_xpos(input->xmin);
    new_input.xmax = roundf(get_wrapped_original_xpos(input->xmax));
    if (new_input.xmin >= new_input.xmax) {
      new_input.xmin = 0;
      new_input.xmax = this->get_width();
    }
  }
  if (ELEM(wrapping_type_, CMP_NODE_WRAP_Y, CMP_NODE_WRAP_XY)) {
    /* Wrap only on the y-axis if tile is wrapping. */
    new_input.ymin = get_wrapped_original_ypos(input->ymin);
    new_input.ymax = roundf(get_wrapped_original_ypos(input->ymax));
    if (new_input.ymin >= new_input.ymax) {
      new_input.ymin = 0;
      new_input.ymax = this->get_height();
    }
  }

  return ReadBufferOperation::determine_depending_area_of_interest(
      &new_input, read_operation, output);
}

void WrapOperation::set_wrapping(int wrapping_type)
{
  wrapping_type_ = wrapping_type;
}

}  // namespace blender::compositor

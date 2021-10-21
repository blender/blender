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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_DisplaceSimpleOperation.h"

namespace blender::compositor {

DisplaceSimpleOperation::DisplaceSimpleOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Vector);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  input_color_program_ = nullptr;
  input_vector_program_ = nullptr;
  input_scale_xprogram_ = nullptr;
  input_scale_yprogram_ = nullptr;
}

void DisplaceSimpleOperation::init_execution()
{
  input_color_program_ = this->get_input_socket_reader(0);
  input_vector_program_ = this->get_input_socket_reader(1);
  input_scale_xprogram_ = this->get_input_socket_reader(2);
  input_scale_yprogram_ = this->get_input_socket_reader(3);

  width_x4_ = this->get_width() * 4;
  height_x4_ = this->get_height() * 4;
}

/* minimum distance (in pixels) a pixel has to be displaced
 * in order to take effect */
// #define DISPLACE_EPSILON    0.01f

void DisplaceSimpleOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float in_vector[4];
  float in_scale[4];

  float p_dx, p_dy; /* main displacement in pixel space */
  float u, v;

  input_scale_xprogram_->read_sampled(in_scale, x, y, sampler);
  float xs = in_scale[0];
  input_scale_yprogram_->read_sampled(in_scale, x, y, sampler);
  float ys = in_scale[0];

  /* clamp x and y displacement to triple image resolution -
   * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
  CLAMP(xs, -width_x4_, width_x4_);
  CLAMP(ys, -height_x4_, height_x4_);

  input_vector_program_->read_sampled(in_vector, x, y, sampler);
  p_dx = in_vector[0] * xs;
  p_dy = in_vector[1] * ys;

  /* displaced pixel in uv coords, for image sampling */
  /* clamp nodes to avoid glitches */
  u = x - p_dx + 0.5f;
  v = y - p_dy + 0.5f;
  CLAMP(u, 0.0f, this->get_width() - 1.0f);
  CLAMP(v, 0.0f, this->get_height() - 1.0f);

  input_color_program_->read_sampled(output, u, v, sampler);
}

void DisplaceSimpleOperation::deinit_execution()
{
  input_color_program_ = nullptr;
  input_vector_program_ = nullptr;
  input_scale_xprogram_ = nullptr;
  input_scale_yprogram_ = nullptr;
}

bool DisplaceSimpleOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti color_input;
  NodeOperation *operation = nullptr;

  /* the vector buffer only needs a 2x2 buffer. The image needs whole buffer */
  /* image */
  operation = get_input_operation(0);
  color_input.xmax = operation->get_width();
  color_input.xmin = 0;
  color_input.ymax = operation->get_height();
  color_input.ymin = 0;
  if (operation->determine_depending_area_of_interest(&color_input, read_operation, output)) {
    return true;
  }

  /* vector */
  if (operation->determine_depending_area_of_interest(input, read_operation, output)) {
    return true;
  }

  /* scale x */
  operation = get_input_operation(2);
  if (operation->determine_depending_area_of_interest(input, read_operation, output)) {
    return true;
  }

  /* scale y */
  operation = get_input_operation(3);
  if (operation->determine_depending_area_of_interest(input, read_operation, output)) {
    return true;
  }

  return false;
}

void DisplaceSimpleOperation::get_area_of_interest(const int input_idx,
                                                   const rcti &output_area,
                                                   rcti &r_input_area)
{
  switch (input_idx) {
    case 0: {
      r_input_area = get_input_operation(input_idx)->get_canvas();
      break;
    }
    default: {
      r_input_area = output_area;
      break;
    }
  }
}

void DisplaceSimpleOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const float width = this->get_width();
  const float height = this->get_height();
  const MemoryBuffer *input_color = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with(inputs.drop_front(1), area); !it.is_end();
       ++it) {
    float scale_x = *it.in(1);
    float scale_y = *it.in(2);

    /* Clamp x and y displacement to triple image resolution -
     * to prevent hangs from huge values mistakenly plugged in eg. z buffers. */
    CLAMP(scale_x, -width_x4_, width_x4_);
    CLAMP(scale_y, -height_x4_, height_x4_);

    /* Main displacement in pixel space. */
    const float *vector = it.in(0);
    const float p_dx = vector[0] * scale_x;
    const float p_dy = vector[1] * scale_y;

    /* Displaced pixel in uv coords, for image sampling. */
    /* Clamp nodes to avoid glitches. */
    float u = it.x - p_dx + 0.5f;
    float v = it.y - p_dy + 0.5f;
    CLAMP(u, 0.0f, width - 1.0f);
    CLAMP(v, 0.0f, height - 1.0f);

    input_color->read_elem_checked(u, v, it.out);
  }
}

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DisplaceOperation.h"

namespace blender::compositor {

DisplaceOperation::DisplaceOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Vector);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.complex = true;

  input_color_program_ = nullptr;
}

void DisplaceOperation::init_execution()
{
  input_color_program_ = this->get_input_socket_reader(0);
  NodeOperation *vector = this->get_input_socket_reader(1);
  NodeOperation *scale_x = this->get_input_socket_reader(2);
  NodeOperation *scale_y = this->get_input_socket_reader(3);
  if (execution_model_ == eExecutionModel::Tiled) {
    vector_read_fn_ = [=](float x, float y, float *out) {
      vector->read_sampled(out, x, y, PixelSampler::Bilinear);
    };
    scale_x_read_fn_ = [=](float x, float y, float *out) {
      scale_x->read_sampled(out, x, y, PixelSampler::Nearest);
    };
    scale_y_read_fn_ = [=](float x, float y, float *out) {
      scale_y->read_sampled(out, x, y, PixelSampler::Nearest);
    };
  }

  width_x4_ = this->get_width() * 4;
  height_x4_ = this->get_height() * 4;
  input_vector_width_ = vector->get_width();
  input_vector_height_ = vector->get_height();
}

void DisplaceOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler /*sampler*/)
{
  float xy[2] = {x, y};
  float uv[2], deriv[2][2];

  pixel_transform(xy, uv, deriv);
  if (is_zero_v2(deriv[0]) && is_zero_v2(deriv[1])) {
    input_color_program_->read_sampled(output, uv[0], uv[1], PixelSampler::Bilinear);
  }
  else {
    /* EWA filtering (without nearest it gets blurry with NO distortion) */
    input_color_program_->read_filtered(output, uv[0], uv[1], deriv[0], deriv[1]);
  }
}

bool DisplaceOperation::read_displacement(
    float x, float y, float xscale, float yscale, const float origin[2], float &r_u, float &r_v)
{
  float width = input_vector_width_;
  float height = input_vector_height_;
  if (x < 0.0f || x >= width || y < 0.0f || y >= height) {
    r_u = 0.0f;
    r_v = 0.0f;
    return false;
  }

  float col[4];
  vector_read_fn_(x, y, col);
  r_u = origin[0] - col[0] * xscale;
  r_v = origin[1] - col[1] * yscale;
  return true;
}

void DisplaceOperation::pixel_transform(const float xy[2], float r_uv[2], float r_deriv[2][2])
{
  float col[4];
  float uv[2]; /* temporary variables for derivative estimation */
  int num;

  scale_x_read_fn_(xy[0], xy[1], col);
  float xs = col[0];
  scale_y_read_fn_(xy[0], xy[1], col);
  float ys = col[0];
  /* clamp x and y displacement to triple image resolution -
   * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
  CLAMP(xs, -width_x4_, width_x4_);
  CLAMP(ys, -height_x4_, height_x4_);

  /* displaced pixel in uv coords, for image sampling */
  read_displacement(xy[0], xy[1], xs, ys, xy, r_uv[0], r_uv[1]);

  /* Estimate partial derivatives using 1-pixel offsets */
  const float epsilon[2] = {1.0f, 1.0f};

  zero_v2(r_deriv[0]);
  zero_v2(r_deriv[1]);

  num = 0;
  if (read_displacement(xy[0] + epsilon[0], xy[1], xs, ys, xy, uv[0], uv[1])) {
    r_deriv[0][0] += uv[0] - r_uv[0];
    r_deriv[1][0] += uv[1] - r_uv[1];
    num++;
  }
  if (read_displacement(xy[0] - epsilon[0], xy[1], xs, ys, xy, uv[0], uv[1])) {
    r_deriv[0][0] += r_uv[0] - uv[0];
    r_deriv[1][0] += r_uv[1] - uv[1];
    num++;
  }
  if (num > 0) {
    float numinv = 1.0f / float(num);
    r_deriv[0][0] *= numinv;
    r_deriv[1][0] *= numinv;
  }

  num = 0;
  if (read_displacement(xy[0], xy[1] + epsilon[1], xs, ys, xy, uv[0], uv[1])) {
    r_deriv[0][1] += uv[0] - r_uv[0];
    r_deriv[1][1] += uv[1] - r_uv[1];
    num++;
  }
  if (read_displacement(xy[0], xy[1] - epsilon[1], xs, ys, xy, uv[0], uv[1])) {
    r_deriv[0][1] += r_uv[0] - uv[0];
    r_deriv[1][1] += r_uv[1] - uv[1];
    num++;
  }
  if (num > 0) {
    float numinv = 1.0f / float(num);
    r_deriv[0][1] *= numinv;
    r_deriv[1][1] *= numinv;
  }
}

void DisplaceOperation::deinit_execution()
{
  input_color_program_ = nullptr;
  vector_read_fn_ = nullptr;
  scale_x_read_fn_ = nullptr;
  scale_y_read_fn_ = nullptr;
}

bool DisplaceOperation::determine_depending_area_of_interest(rcti *input,
                                                             ReadBufferOperation *read_operation,
                                                             rcti *output)
{
  rcti color_input;
  rcti vector_input;
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
  operation = get_input_operation(1);
  vector_input.xmax = input->xmax + 1;
  vector_input.xmin = input->xmin - 1;
  vector_input.ymax = input->ymax + 1;
  vector_input.ymin = input->ymin - 1;
  if (operation->determine_depending_area_of_interest(&vector_input, read_operation, output)) {
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

void DisplaceOperation::get_area_of_interest(const int input_idx,
                                             const rcti &output_area,
                                             rcti &r_input_area)
{
  switch (input_idx) {
    case 0: {
      r_input_area = get_input_operation(input_idx)->get_canvas();
      break;
    }
    case 1: {
      r_input_area = output_area;
      expand_area_for_sampler(r_input_area, PixelSampler::Bilinear);
      break;
    }
    default: {
      r_input_area = output_area;
      break;
    }
  }
}

void DisplaceOperation::update_memory_buffer_started(MemoryBuffer * /*output*/,
                                                     const rcti & /*area*/,
                                                     Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *vector = inputs[1];
  MemoryBuffer *scale_x = inputs[2];
  MemoryBuffer *scale_y = inputs[3];
  vector_read_fn_ = [=](float x, float y, float *out) { vector->read_elem_bilinear(x, y, out); };
  scale_x_read_fn_ = [=](float x, float y, float *out) { scale_x->read_elem_checked(x, y, out); };
  scale_y_read_fn_ = [=](float x, float y, float *out) { scale_y->read_elem_checked(x, y, out); };
}

void DisplaceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_color = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const float xy[2] = {float(it.x), float(it.y)};
    float uv[2];
    float deriv[2][2];

    pixel_transform(xy, uv, deriv);
    if (is_zero_v2(deriv[0]) && is_zero_v2(deriv[1])) {
      input_color->read_elem_bilinear(uv[0], uv[1], it.out);
    }
    else {
      /* EWA filtering (without nearest it gets blurry with NO distortion). */
      input_color->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], it.out);
    }
  }
}

}  // namespace blender::compositor

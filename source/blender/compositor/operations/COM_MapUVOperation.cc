/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MapUVOperation.h"

namespace blender::compositor {

MapUVOperation::MapUVOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Color);
  alpha_ = 0.0f;
  flags_.complex = true;
  set_canvas_input_index(UV_INPUT_INDEX);

  inputUVProgram_ = nullptr;
  input_color_program_ = nullptr;
}

void MapUVOperation::init_data()
{
  NodeOperation *image_input = get_input_operation(IMAGE_INPUT_INDEX);
  image_width_ = image_input->get_width();
  image_height_ = image_input->get_height();

  NodeOperation *uv_input = get_input_operation(UV_INPUT_INDEX);
  uv_width_ = uv_input->get_width();
  uv_height_ = uv_input->get_height();
}

void MapUVOperation::init_execution()
{
  input_color_program_ = this->get_input_socket_reader(0);
  inputUVProgram_ = this->get_input_socket_reader(1);
  if (execution_model_ == eExecutionModel::Tiled) {
    uv_input_read_fn_ = [=](float x, float y, float *out) {
      inputUVProgram_->read_sampled(out, x, y, PixelSampler::Bilinear);
    };
  }
}

void MapUVOperation::execute_pixel_sampled(float output[4],
                                           float x,
                                           float y,
                                           PixelSampler /*sampler*/)
{
  float xy[2] = {x, y};
  float uv[2], deriv[2][2], alpha;

  pixel_transform(xy, uv, deriv, alpha);
  if (alpha == 0.0f) {
    zero_v4(output);
    return;
  }

  /* EWA filtering */
  input_color_program_->read_filtered(output, uv[0], uv[1], deriv[0], deriv[1]);

  /* UV to alpha threshold */
  const float threshold = alpha_ * 0.05f;
  /* XXX alpha threshold is used to fade out pixels on boundaries with invalid derivatives.
   * this calculation is not very well defined, should be looked into if it becomes a problem ...
   */
  float du = len_v2(deriv[0]);
  float dv = len_v2(deriv[1]);
  float factor = 1.0f - threshold * (du / input_color_program_->get_width() +
                                     dv / input_color_program_->get_height());
  if (factor < 0.0f) {
    alpha = 0.0f;
  }
  else {
    alpha *= factor;
  }

  /* "premul" */
  if (alpha < 1.0f) {
    mul_v4_fl(output, alpha);
  }
}

bool MapUVOperation::read_uv(float x, float y, float &r_u, float &r_v, float &r_alpha)
{
  if (x < 0.0f || x >= uv_width_ || y < 0.0f || y >= uv_height_) {
    r_u = 0.0f;
    r_v = 0.0f;
    r_alpha = 0.0f;
    return false;
  }

  float vector[3];
  uv_input_read_fn_(x, y, vector);
  r_u = vector[0] * image_width_;
  r_v = vector[1] * image_height_;
  r_alpha = vector[2];
  return true;
}

void MapUVOperation::pixel_transform(const float xy[2],
                                     float r_uv[2],
                                     float r_deriv[2][2],
                                     float &r_alpha)
{
  float uv[2], alpha; /* temporary variables for derivative estimation */
  int num;

  read_uv(xy[0], xy[1], r_uv[0], r_uv[1], r_alpha);

  /* Estimate partial derivatives using 1-pixel offsets */
  const float epsilon[2] = {1.0f, 1.0f};

  zero_v2(r_deriv[0]);
  zero_v2(r_deriv[1]);

  num = 0;
  if (read_uv(xy[0] + epsilon[0], xy[1], uv[0], uv[1], alpha)) {
    r_deriv[0][0] += uv[0] - r_uv[0];
    r_deriv[1][0] += uv[1] - r_uv[1];
    num++;
  }
  if (read_uv(xy[0] - epsilon[0], xy[1], uv[0], uv[1], alpha)) {
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
  if (read_uv(xy[0], xy[1] + epsilon[1], uv[0], uv[1], alpha)) {
    r_deriv[0][1] += uv[0] - r_uv[0];
    r_deriv[1][1] += uv[1] - r_uv[1];
    num++;
  }
  if (read_uv(xy[0], xy[1] - epsilon[1], uv[0], uv[1], alpha)) {
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

void MapUVOperation::deinit_execution()
{
  inputUVProgram_ = nullptr;
  input_color_program_ = nullptr;
}

bool MapUVOperation::determine_depending_area_of_interest(rcti *input,
                                                          ReadBufferOperation *read_operation,
                                                          rcti *output)
{
  rcti color_input;
  rcti uv_input;
  NodeOperation *operation = nullptr;

  /* the uv buffer only needs a 3x3 buffer. The image needs whole buffer */

  operation = get_input_operation(0);
  color_input.xmax = operation->get_width();
  color_input.xmin = 0;
  color_input.ymax = operation->get_height();
  color_input.ymin = 0;
  if (operation->determine_depending_area_of_interest(&color_input, read_operation, output)) {
    return true;
  }

  operation = get_input_operation(1);
  uv_input.xmax = input->xmax + 1;
  uv_input.xmin = input->xmin - 1;
  uv_input.ymax = input->ymax + 1;
  uv_input.ymin = input->ymin - 1;
  if (operation->determine_depending_area_of_interest(&uv_input, read_operation, output)) {
    return true;
  }

  return false;
}

void MapUVOperation::get_area_of_interest(const int input_idx,
                                          const rcti &output_area,
                                          rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      r_input_area = get_input_operation(IMAGE_INPUT_INDEX)->get_canvas();
      break;
    }
    case UV_INPUT_INDEX: {
      r_input_area = output_area;
      expand_area_for_sampler(r_input_area, PixelSampler::Bilinear);
      break;
    }
  }
}

void MapUVOperation::update_memory_buffer_started(MemoryBuffer * /*output*/,
                                                  const rcti & /*area*/,
                                                  Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *uv_input = inputs[UV_INPUT_INDEX];
  uv_input_read_fn_ = [=](float x, float y, float *out) {
    uv_input->read_elem_bilinear(x, y, out);
  };
}

void MapUVOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_image = inputs[IMAGE_INPUT_INDEX];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float xy[2] = {float(it.x), float(it.y)};
    float uv[2];
    float deriv[2][2];
    float alpha;
    pixel_transform(xy, uv, deriv, alpha);
    if (alpha == 0.0f) {
      zero_v4(it.out);
      continue;
    }

    /* EWA filtering. */
    input_image->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], it.out);

    /* UV to alpha threshold. */
    const float threshold = alpha_ * 0.05f;
    /* XXX alpha threshold is used to fade out pixels on boundaries with invalid derivatives.
     * this calculation is not very well defined, should be looked into if it becomes a problem ...
     */
    const float du = len_v2(deriv[0]);
    const float dv = len_v2(deriv[1]);
    const float factor = 1.0f - threshold * (du / image_width_ + dv / image_height_);
    if (factor < 0.0f) {
      alpha = 0.0f;
    }
    else {
      alpha *= factor;
    }

    /* "premul" */
    if (alpha < 1.0f) {
      mul_v4_fl(it.out, alpha);
    }
  }
}

}  // namespace blender::compositor

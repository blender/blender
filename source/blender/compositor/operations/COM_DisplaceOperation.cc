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

#include "COM_DisplaceOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

namespace blender::compositor {

DisplaceOperation::DisplaceOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Vector);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;

  this->m_inputColorProgram = nullptr;
}

void DisplaceOperation::initExecution()
{
  this->m_inputColorProgram = this->getInputSocketReader(0);
  NodeOperation *vector = this->getInputSocketReader(1);
  NodeOperation *scale_x = this->getInputSocketReader(2);
  NodeOperation *scale_y = this->getInputSocketReader(3);
  if (execution_model_ == eExecutionModel::Tiled) {
    vector_read_fn_ = [=](float x, float y, float *out) {
      vector->readSampled(out, x, y, PixelSampler::Bilinear);
    };
    scale_x_read_fn_ = [=](float x, float y, float *out) {
      scale_x->readSampled(out, x, y, PixelSampler::Nearest);
    };
    scale_y_read_fn_ = [=](float x, float y, float *out) {
      scale_y->readSampled(out, x, y, PixelSampler::Nearest);
    };
  }

  this->m_width_x4 = this->getWidth() * 4;
  this->m_height_x4 = this->getHeight() * 4;
  input_vector_width_ = vector->getWidth();
  input_vector_height_ = vector->getHeight();
}

void DisplaceOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler /*sampler*/)
{
  float xy[2] = {x, y};
  float uv[2], deriv[2][2];

  pixelTransform(xy, uv, deriv);
  if (is_zero_v2(deriv[0]) && is_zero_v2(deriv[1])) {
    this->m_inputColorProgram->readSampled(output, uv[0], uv[1], PixelSampler::Bilinear);
  }
  else {
    /* EWA filtering (without nearest it gets blurry with NO distortion) */
    this->m_inputColorProgram->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1]);
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

void DisplaceOperation::pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2])
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
  CLAMP(xs, -m_width_x4, m_width_x4);
  CLAMP(ys, -m_height_x4, m_height_x4);

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
    float numinv = 1.0f / (float)num;
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
    float numinv = 1.0f / (float)num;
    r_deriv[0][1] *= numinv;
    r_deriv[1][1] *= numinv;
  }
}

void DisplaceOperation::deinitExecution()
{
  this->m_inputColorProgram = nullptr;
  vector_read_fn_ = nullptr;
  scale_x_read_fn_ = nullptr;
  scale_y_read_fn_ = nullptr;
}

bool DisplaceOperation::determineDependingAreaOfInterest(rcti *input,
                                                         ReadBufferOperation *readOperation,
                                                         rcti *output)
{
  rcti colorInput;
  rcti vectorInput;
  NodeOperation *operation = nullptr;

  /* the vector buffer only needs a 2x2 buffer. The image needs whole buffer */
  /* image */
  operation = getInputOperation(0);
  colorInput.xmax = operation->getWidth();
  colorInput.xmin = 0;
  colorInput.ymax = operation->getHeight();
  colorInput.ymin = 0;
  if (operation->determineDependingAreaOfInterest(&colorInput, readOperation, output)) {
    return true;
  }

  /* vector */
  operation = getInputOperation(1);
  vectorInput.xmax = input->xmax + 1;
  vectorInput.xmin = input->xmin - 1;
  vectorInput.ymax = input->ymax + 1;
  vectorInput.ymin = input->ymin - 1;
  if (operation->determineDependingAreaOfInterest(&vectorInput, readOperation, output)) {
    return true;
  }

  /* scale x */
  operation = getInputOperation(2);
  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
    return true;
  }

  /* scale y */
  operation = getInputOperation(3);
  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
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
      r_input_area = getInputOperation(input_idx)->get_canvas();
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

void DisplaceOperation::update_memory_buffer_started(MemoryBuffer *UNUSED(output),
                                                     const rcti &UNUSED(area),
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
    const float xy[2] = {(float)it.x, (float)it.y};
    float uv[2];
    float deriv[2][2];

    pixelTransform(xy, uv, deriv);
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

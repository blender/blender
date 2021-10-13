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

#include "COM_RotateOperation.h"

namespace blender::compositor {

RotateOperation::RotateOperation()
{
  this->addInputSocket(DataType::Color, ResizeMode::None);
  this->addInputSocket(DataType::Value, ResizeMode::None);
  this->addOutputSocket(DataType::Color);
  this->set_canvas_input_index(0);
  this->m_imageSocket = nullptr;
  this->m_degreeSocket = nullptr;
  this->m_doDegree2RadConversion = false;
  this->m_isDegreeSet = false;
  sampler_ = PixelSampler::Bilinear;
}

void RotateOperation::get_rotation_center(const rcti &area, float &r_x, float &r_y)
{
  r_x = (BLI_rcti_size_x(&area) - 1) / 2.0;
  r_y = (BLI_rcti_size_y(&area) - 1) / 2.0;
}

void RotateOperation::get_rotation_offset(const rcti &input_canvas,
                                          const rcti &rotate_canvas,
                                          float &r_offset_x,
                                          float &r_offset_y)
{
  r_offset_x = (BLI_rcti_size_x(&input_canvas) - BLI_rcti_size_x(&rotate_canvas)) / 2.0f;
  r_offset_y = (BLI_rcti_size_y(&input_canvas) - BLI_rcti_size_y(&rotate_canvas)) / 2.0f;
}

void RotateOperation::get_area_rotation_bounds(const rcti &area,
                                               const float center_x,
                                               const float center_y,
                                               const float sine,
                                               const float cosine,
                                               rcti &r_bounds)
{
  const float dxmin = area.xmin - center_x;
  const float dymin = area.ymin - center_y;
  const float dxmax = area.xmax - center_x;
  const float dymax = area.ymax - center_y;

  const float x1 = center_x + (cosine * dxmin + (-sine) * dymin);
  const float x2 = center_x + (cosine * dxmax + (-sine) * dymin);
  const float x3 = center_x + (cosine * dxmin + (-sine) * dymax);
  const float x4 = center_x + (cosine * dxmax + (-sine) * dymax);
  const float y1 = center_y + (sine * dxmin + cosine * dymin);
  const float y2 = center_y + (sine * dxmax + cosine * dymin);
  const float y3 = center_y + (sine * dxmin + cosine * dymax);
  const float y4 = center_y + (sine * dxmax + cosine * dymax);
  const float minx = MIN2(x1, MIN2(x2, MIN2(x3, x4)));
  const float maxx = MAX2(x1, MAX2(x2, MAX2(x3, x4)));
  const float miny = MIN2(y1, MIN2(y2, MIN2(y3, y4)));
  const float maxy = MAX2(y1, MAX2(y2, MAX2(y3, y4)));

  r_bounds.xmin = floor(minx);
  r_bounds.xmax = ceil(maxx);
  r_bounds.ymin = floor(miny);
  r_bounds.ymax = ceil(maxy);
}

void RotateOperation::get_area_rotation_bounds_inverted(const rcti &area,
                                                        const float center_x,
                                                        const float center_y,
                                                        const float sine,
                                                        const float cosine,
                                                        rcti &r_bounds)
{
  get_area_rotation_bounds(area, center_x, center_y, -sine, cosine, r_bounds);
}

void RotateOperation::get_rotation_area_of_interest(const rcti &input_canvas,
                                                    const rcti &rotate_canvas,
                                                    const float sine,
                                                    const float cosine,
                                                    const rcti &output_area,
                                                    rcti &r_input_area)
{
  float center_x, center_y;
  get_rotation_center(input_canvas, center_x, center_y);

  float rotate_offset_x, rotate_offset_y;
  get_rotation_offset(input_canvas, rotate_canvas, rotate_offset_x, rotate_offset_y);

  r_input_area = output_area;
  BLI_rcti_translate(&r_input_area, rotate_offset_x, rotate_offset_y);
  get_area_rotation_bounds_inverted(r_input_area, center_x, center_y, sine, cosine, r_input_area);
}

void RotateOperation::get_rotation_canvas(const rcti &input_canvas,
                                          const float sine,
                                          const float cosine,
                                          rcti &r_canvas)
{
  float center_x, center_y;
  get_rotation_center(input_canvas, center_x, center_y);

  rcti rot_bounds;
  get_area_rotation_bounds(input_canvas, center_x, center_y, sine, cosine, rot_bounds);

  float offset_x, offset_y;
  get_rotation_offset(input_canvas, rot_bounds, offset_x, offset_y);
  r_canvas = rot_bounds;
  BLI_rcti_translate(&r_canvas, -offset_x, -offset_y);
}

void RotateOperation::init_data()
{
  if (execution_model_ == eExecutionModel::Tiled) {
    get_rotation_center(get_canvas(), m_centerX, m_centerY);
  }
}

void RotateOperation::initExecution()
{
  this->m_imageSocket = this->getInputSocketReader(0);
  this->m_degreeSocket = this->getInputSocketReader(1);
}

void RotateOperation::deinitExecution()
{
  this->m_imageSocket = nullptr;
  this->m_degreeSocket = nullptr;
}

inline void RotateOperation::ensureDegree()
{
  if (!this->m_isDegreeSet) {
    float degree[4];
    switch (execution_model_) {
      case eExecutionModel::Tiled:
        this->m_degreeSocket->readSampled(degree, 0, 0, PixelSampler::Nearest);
        break;
      case eExecutionModel::FullFrame:
        degree[0] = get_input_operation(DEGREE_INPUT_INDEX)->get_constant_value_default(0.0f);
        break;
    }

    double rad;
    if (this->m_doDegree2RadConversion) {
      rad = DEG2RAD((double)degree[0]);
    }
    else {
      rad = degree[0];
    }
    this->m_cosine = cos(rad);
    this->m_sine = sin(rad);

    this->m_isDegreeSet = true;
  }
}

void RotateOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  ensureDegree();
  const float dy = y - this->m_centerY;
  const float dx = x - this->m_centerX;
  const float nx = this->m_centerX + (this->m_cosine * dx + this->m_sine * dy);
  const float ny = this->m_centerY + (-this->m_sine * dx + this->m_cosine * dy);
  this->m_imageSocket->readSampled(output, nx, ny, sampler);
}

bool RotateOperation::determineDependingAreaOfInterest(rcti *input,
                                                       ReadBufferOperation *readOperation,
                                                       rcti *output)
{
  ensureDegree();
  rcti newInput;

  const float dxmin = input->xmin - this->m_centerX;
  const float dymin = input->ymin - this->m_centerY;
  const float dxmax = input->xmax - this->m_centerX;
  const float dymax = input->ymax - this->m_centerY;

  const float x1 = this->m_centerX + (this->m_cosine * dxmin + this->m_sine * dymin);
  const float x2 = this->m_centerX + (this->m_cosine * dxmax + this->m_sine * dymin);
  const float x3 = this->m_centerX + (this->m_cosine * dxmin + this->m_sine * dymax);
  const float x4 = this->m_centerX + (this->m_cosine * dxmax + this->m_sine * dymax);
  const float y1 = this->m_centerY + (-this->m_sine * dxmin + this->m_cosine * dymin);
  const float y2 = this->m_centerY + (-this->m_sine * dxmax + this->m_cosine * dymin);
  const float y3 = this->m_centerY + (-this->m_sine * dxmin + this->m_cosine * dymax);
  const float y4 = this->m_centerY + (-this->m_sine * dxmax + this->m_cosine * dymax);
  const float minx = MIN2(x1, MIN2(x2, MIN2(x3, x4)));
  const float maxx = MAX2(x1, MAX2(x2, MAX2(x3, x4)));
  const float miny = MIN2(y1, MIN2(y2, MIN2(y3, y4)));
  const float maxy = MAX2(y1, MAX2(y2, MAX2(y3, y4)));

  newInput.xmax = ceil(maxx) + 1;
  newInput.xmin = floor(minx) - 1;
  newInput.ymax = ceil(maxy) + 1;
  newInput.ymin = floor(miny) - 1;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void RotateOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (execution_model_ == eExecutionModel::Tiled) {
    NodeOperation::determine_canvas(preferred_area, r_area);
    return;
  }

  const bool image_determined =
      getInputSocket(IMAGE_INPUT_INDEX)->determine_canvas(preferred_area, r_area);
  if (image_determined) {
    rcti input_canvas = r_area;
    rcti unused;
    getInputSocket(DEGREE_INPUT_INDEX)->determine_canvas(input_canvas, unused);

    ensureDegree();

    get_rotation_canvas(input_canvas, m_sine, m_cosine, r_area);
  }
}

void RotateOperation::get_area_of_interest(const int input_idx,
                                           const rcti &output_area,
                                           rcti &r_input_area)
{
  if (input_idx == DEGREE_INPUT_INDEX) {
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
    return;
  }

  ensureDegree();

  const rcti &input_image_canvas = get_input_operation(IMAGE_INPUT_INDEX)->get_canvas();
  get_rotation_area_of_interest(
      input_image_canvas, this->get_canvas(), m_sine, m_cosine, output_area, r_input_area);
  expand_area_for_sampler(r_input_area, sampler_);
}

void RotateOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[IMAGE_INPUT_INDEX];

  NodeOperation *image_op = get_input_operation(IMAGE_INPUT_INDEX);
  float center_x, center_y;
  get_rotation_center(image_op->get_canvas(), center_x, center_y);
  float rotate_offset_x, rotate_offset_y;
  get_rotation_offset(
      image_op->get_canvas(), this->get_canvas(), rotate_offset_x, rotate_offset_y);

  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float x = rotate_offset_x + it.x + canvas_.xmin;
    float y = rotate_offset_y + it.y + canvas_.ymin;
    rotate_coords(x, y, center_x, center_y, m_sine, m_cosine);
    input_img->read_elem_sampled(x - canvas_.xmin, y - canvas_.ymin, sampler_, it.out);
  }
}

}  // namespace blender::compositor

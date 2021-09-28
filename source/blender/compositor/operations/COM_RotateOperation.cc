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
#include "COM_ConstantOperation.h"

#include "BLI_math.h"

namespace blender::compositor {

RotateOperation::RotateOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  this->set_canvas_input_index(0);
  this->m_imageSocket = nullptr;
  this->m_degreeSocket = nullptr;
  this->m_doDegree2RadConversion = false;
  this->m_isDegreeSet = false;
  sampler_ = PixelSampler::Bilinear;
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

  const float x1 = center_x + (cosine * dxmin + sine * dymin);
  const float x2 = center_x + (cosine * dxmax + sine * dymin);
  const float x3 = center_x + (cosine * dxmin + sine * dymax);
  const float x4 = center_x + (cosine * dxmax + sine * dymax);
  const float y1 = center_y + (-sine * dxmin + cosine * dymin);
  const float y2 = center_y + (-sine * dxmax + cosine * dymin);
  const float y3 = center_y + (-sine * dxmin + cosine * dymax);
  const float y4 = center_y + (-sine * dxmax + cosine * dymax);
  const float minx = MIN2(x1, MIN2(x2, MIN2(x3, x4)));
  const float maxx = MAX2(x1, MAX2(x2, MAX2(x3, x4)));
  const float miny = MIN2(y1, MIN2(y2, MIN2(y3, y4)));
  const float maxy = MAX2(y1, MAX2(y2, MAX2(y3, y4)));

  r_bounds.xmin = floor(minx);
  r_bounds.xmax = ceil(maxx);
  r_bounds.ymin = floor(miny);
  r_bounds.ymax = ceil(maxy);
}

void RotateOperation::init_data()
{
  this->m_centerX = (getWidth() - 1) / 2.0;
  this->m_centerY = (getHeight() - 1) / 2.0;
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
        NodeOperation *degree_op = getInputOperation(DEGREE_INPUT_INDEX);
        const bool is_constant_degree = degree_op->get_flags().is_constant_operation;
        degree[0] = is_constant_degree ?
                        static_cast<ConstantOperation *>(degree_op)->get_constant_elem()[0] :
                        0.0f;
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

void RotateOperation::get_area_of_interest(const int input_idx,
                                           const rcti &output_area,
                                           rcti &r_input_area)
{
  if (input_idx == DEGREE_INPUT_INDEX) {
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
    return;
  }

  ensureDegree();
  get_area_rotation_bounds(output_area, m_centerX, m_centerY, m_sine, m_cosine, r_input_area);
  expand_area_for_sampler(r_input_area, sampler_);
}

void RotateOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  ensureDegree();
  const MemoryBuffer *input_img = inputs[IMAGE_INPUT_INDEX];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float x = it.x;
    float y = it.y;
    rotate_coords(x, y, m_centerX, m_centerY, m_sine, m_cosine);
    input_img->read_elem_sampled(x, y, sampler_, it.out);
  }
}

}  // namespace blender::compositor

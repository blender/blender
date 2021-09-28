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
#include "BLI_math.h"
#include "BLI_utildefines.h"

namespace blender::compositor {

DisplaceSimpleOperation::DisplaceSimpleOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Vector);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);

  this->m_inputColorProgram = nullptr;
  this->m_inputVectorProgram = nullptr;
  this->m_inputScaleXProgram = nullptr;
  this->m_inputScaleYProgram = nullptr;
}

void DisplaceSimpleOperation::initExecution()
{
  this->m_inputColorProgram = this->getInputSocketReader(0);
  this->m_inputVectorProgram = this->getInputSocketReader(1);
  this->m_inputScaleXProgram = this->getInputSocketReader(2);
  this->m_inputScaleYProgram = this->getInputSocketReader(3);

  this->m_width_x4 = this->getWidth() * 4;
  this->m_height_x4 = this->getHeight() * 4;
}

/* minimum distance (in pixels) a pixel has to be displaced
 * in order to take effect */
// #define DISPLACE_EPSILON    0.01f

void DisplaceSimpleOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float inVector[4];
  float inScale[4];

  float p_dx, p_dy; /* main displacement in pixel space */
  float u, v;

  this->m_inputScaleXProgram->readSampled(inScale, x, y, sampler);
  float xs = inScale[0];
  this->m_inputScaleYProgram->readSampled(inScale, x, y, sampler);
  float ys = inScale[0];

  /* clamp x and y displacement to triple image resolution -
   * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
  CLAMP(xs, -this->m_width_x4, this->m_width_x4);
  CLAMP(ys, -this->m_height_x4, this->m_height_x4);

  this->m_inputVectorProgram->readSampled(inVector, x, y, sampler);
  p_dx = inVector[0] * xs;
  p_dy = inVector[1] * ys;

  /* displaced pixel in uv coords, for image sampling */
  /* clamp nodes to avoid glitches */
  u = x - p_dx + 0.5f;
  v = y - p_dy + 0.5f;
  CLAMP(u, 0.0f, this->getWidth() - 1.0f);
  CLAMP(v, 0.0f, this->getHeight() - 1.0f);

  this->m_inputColorProgram->readSampled(output, u, v, sampler);
}

void DisplaceSimpleOperation::deinitExecution()
{
  this->m_inputColorProgram = nullptr;
  this->m_inputVectorProgram = nullptr;
  this->m_inputScaleXProgram = nullptr;
  this->m_inputScaleYProgram = nullptr;
}

bool DisplaceSimpleOperation::determineDependingAreaOfInterest(rcti *input,
                                                               ReadBufferOperation *readOperation,
                                                               rcti *output)
{
  rcti colorInput;
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
  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
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
  const float width = this->getWidth();
  const float height = this->getHeight();
  const MemoryBuffer *input_color = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with(inputs.drop_front(1), area); !it.is_end();
       ++it) {
    float scale_x = *it.in(1);
    float scale_y = *it.in(2);

    /* Clamp x and y displacement to triple image resolution -
     * to prevent hangs from huge values mistakenly plugged in eg. z buffers. */
    CLAMP(scale_x, -m_width_x4, m_width_x4);
    CLAMP(scale_y, -m_height_x4, m_height_x4);

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

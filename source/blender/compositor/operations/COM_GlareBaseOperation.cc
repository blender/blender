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

#include "COM_GlareBaseOperation.h"
#include "BLI_math.h"

namespace blender::compositor {

GlareBaseOperation::GlareBaseOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  this->m_settings = nullptr;
  flags.is_fullframe_operation = true;
  is_output_rendered_ = false;
}
void GlareBaseOperation::initExecution()
{
  SingleThreadedOperation::initExecution();
  this->m_inputProgram = getInputSocketReader(0);
}

void GlareBaseOperation::deinitExecution()
{
  this->m_inputProgram = nullptr;
  SingleThreadedOperation::deinitExecution();
}

MemoryBuffer *GlareBaseOperation::createMemoryBuffer(rcti *rect2)
{
  MemoryBuffer *tile = (MemoryBuffer *)this->m_inputProgram->initializeTileData(rect2);
  rcti rect;
  rect.xmin = 0;
  rect.ymin = 0;
  rect.xmax = getWidth();
  rect.ymax = getHeight();
  MemoryBuffer *result = new MemoryBuffer(DataType::Color, rect);
  float *data = result->getBuffer();
  this->generateGlare(data, tile, this->m_settings);
  return result;
}

bool GlareBaseOperation::determineDependingAreaOfInterest(rcti * /*input*/,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  if (isCached()) {
    return false;
  }

  rcti newInput;
  newInput.xmax = this->getWidth();
  newInput.xmin = 0;
  newInput.ymax = this->getHeight();
  newInput.ymin = 0;
  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void GlareBaseOperation::get_area_of_interest(const int input_idx,
                                              const rcti &UNUSED(output_area),
                                              rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = 0;
  r_input_area.xmax = this->getWidth();
  r_input_area.ymin = 0;
  r_input_area.ymax = this->getHeight();
}

void GlareBaseOperation::update_memory_buffer(MemoryBuffer *output,
                                              const rcti &UNUSED(area),
                                              Span<MemoryBuffer *> inputs)
{
  if (!is_output_rendered_) {
    MemoryBuffer *input = inputs[0];
    const bool is_input_inflated = input->is_a_single_elem();
    if (is_input_inflated) {
      input = input->inflate();
    }

    this->generateGlare(output->getBuffer(), input, m_settings);
    is_output_rendered_ = true;

    if (is_input_inflated) {
      delete input;
    }
  }
}

}  // namespace blender::compositor

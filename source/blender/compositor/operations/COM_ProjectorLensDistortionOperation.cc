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

#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_ConstantOperation.h"

namespace blender::compositor {

ProjectorLensDistortionOperation::ProjectorLensDistortionOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;
  this->m_inputProgram = nullptr;
  this->m_dispersionAvailable = false;
  this->m_dispersion = 0.0f;
}

void ProjectorLensDistortionOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    NodeOperation *dispersion_input = get_input_operation(1);
    if (dispersion_input->get_flags().is_constant_operation) {
      this->m_dispersion =
          static_cast<ConstantOperation *>(dispersion_input)->get_constant_elem()[0];
    }
    this->m_kr = 0.25f * max_ff(min_ff(this->m_dispersion, 1.0f), 0.0f);
    this->m_kr2 = this->m_kr * 20;
  }
}

void ProjectorLensDistortionOperation::initExecution()
{
  this->initMutex();
  this->m_inputProgram = this->getInputSocketReader(0);
}

void *ProjectorLensDistortionOperation::initializeTileData(rcti * /*rect*/)
{
  updateDispersion();
  void *buffer = this->m_inputProgram->initializeTileData(nullptr);
  return buffer;
}

void ProjectorLensDistortionOperation::executePixel(float output[4], int x, int y, void *data)
{
  float inputValue[4];
  const float height = this->getHeight();
  const float width = this->getWidth();
  const float v = (y + 0.5f) / height;
  const float u = (x + 0.5f) / width;
  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  inputBuffer->readBilinear(inputValue, (u * width + this->m_kr2) - 0.5f, v * height - 0.5f);
  output[0] = inputValue[0];
  inputBuffer->read(inputValue, x, y);
  output[1] = inputValue[1];
  inputBuffer->readBilinear(inputValue, (u * width - this->m_kr2) - 0.5f, v * height - 0.5f);
  output[2] = inputValue[2];
  output[3] = 1.0f;
}

void ProjectorLensDistortionOperation::deinitExecution()
{
  this->deinitMutex();
  this->m_inputProgram = nullptr;
}

bool ProjectorLensDistortionOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;
  if (this->m_dispersionAvailable) {
    newInput.ymax = input->ymax;
    newInput.ymin = input->ymin;
    newInput.xmin = input->xmin - this->m_kr2 - 2;
    newInput.xmax = input->xmax + this->m_kr2 + 2;
  }
  else {
    rcti dispInput;
    BLI_rcti_init(&dispInput, 0, 5, 0, 5);
    if (this->getInputOperation(1)->determineDependingAreaOfInterest(
            &dispInput, readOperation, output)) {
      return true;
    }
    newInput.xmin = input->xmin - 7; /* (0.25f * 20 * 1) + 2 == worse case dispersion */
    newInput.ymin = input->ymin;
    newInput.ymax = input->ymax;
    newInput.xmax = input->xmax + 7; /* (0.25f * 20 * 1) + 2 == worse case dispersion */
  }
  if (this->getInputOperation(0)->determineDependingAreaOfInterest(
          &newInput, readOperation, output)) {
    return true;
  }
  return false;
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void ProjectorLensDistortionOperation::updateDispersion()
{
  if (this->m_dispersionAvailable) {
    return;
  }
  this->lockMutex();
  if (!this->m_dispersionAvailable) {
    float result[4];
    this->getInputSocketReader(1)->readSampled(result, 1, 1, PixelSampler::Nearest);
    this->m_dispersion = result[0];
    this->m_kr = 0.25f * max_ff(min_ff(this->m_dispersion, 1.0f), 0.0f);
    this->m_kr2 = this->m_kr * 20;
    this->m_dispersionAvailable = true;
  }
  this->unlockMutex();
}

void ProjectorLensDistortionOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  switch (execution_model_) {
    case eExecutionModel::FullFrame: {
      set_determined_canvas_modifier([=](rcti &canvas) {
        /* Ensure screen space. */
        BLI_rcti_translate(&canvas, -canvas.xmin, -canvas.ymin);
      });
      break;
    }
    default:
      break;
  }

  NodeOperation::determine_canvas(preferred_area, r_area);
}

void ProjectorLensDistortionOperation::get_area_of_interest(const int input_idx,
                                                            const rcti &output_area,
                                                            rcti &r_input_area)
{
  if (input_idx == 1) {
    /* Dispersion input is used as constant only. */
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
    return;
  }

  r_input_area.ymax = output_area.ymax;
  r_input_area.ymin = output_area.ymin;
  r_input_area.xmin = output_area.xmin - this->m_kr2 - 2;
  r_input_area.xmax = output_area.xmax + this->m_kr2 + 2;
}

void ProjectorLensDistortionOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                    const rcti &area,
                                                                    Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_image = inputs[0];
  const float height = this->getHeight();
  const float width = this->getWidth();
  float color[4];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const float v = (it.y + 0.5f) / height;
    const float u = (it.x + 0.5f) / width;
    input_image->read_elem_bilinear((u * width + this->m_kr2) - 0.5f, v * height - 0.5f, color);
    it.out[0] = color[0];
    input_image->read_elem(it.x, it.y, color);
    it.out[1] = color[1];
    input_image->read_elem_bilinear((u * width - this->m_kr2) - 0.5f, v * height - 0.5f, color);
    it.out[2] = color[2];
    it.out[3] = 1.0f;
  }
}

}  // namespace blender::compositor

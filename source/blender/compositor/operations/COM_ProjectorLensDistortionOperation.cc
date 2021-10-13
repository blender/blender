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
  inputProgram_ = nullptr;
  dispersionAvailable_ = false;
  dispersion_ = 0.0f;
}

void ProjectorLensDistortionOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    NodeOperation *dispersion_input = get_input_operation(1);
    if (dispersion_input->get_flags().is_constant_operation) {
      dispersion_ = static_cast<ConstantOperation *>(dispersion_input)->get_constant_elem()[0];
    }
    kr_ = 0.25f * max_ff(min_ff(dispersion_, 1.0f), 0.0f);
    kr2_ = kr_ * 20;
  }
}

void ProjectorLensDistortionOperation::initExecution()
{
  this->initMutex();
  inputProgram_ = this->getInputSocketReader(0);
}

void *ProjectorLensDistortionOperation::initializeTileData(rcti * /*rect*/)
{
  updateDispersion();
  void *buffer = inputProgram_->initializeTileData(nullptr);
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
  inputBuffer->readBilinear(inputValue, (u * width + kr2_) - 0.5f, v * height - 0.5f);
  output[0] = inputValue[0];
  inputBuffer->read(inputValue, x, y);
  output[1] = inputValue[1];
  inputBuffer->readBilinear(inputValue, (u * width - kr2_) - 0.5f, v * height - 0.5f);
  output[2] = inputValue[2];
  output[3] = 1.0f;
}

void ProjectorLensDistortionOperation::deinitExecution()
{
  this->deinitMutex();
  inputProgram_ = nullptr;
}

bool ProjectorLensDistortionOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;
  if (dispersionAvailable_) {
    newInput.ymax = input->ymax;
    newInput.ymin = input->ymin;
    newInput.xmin = input->xmin - kr2_ - 2;
    newInput.xmax = input->xmax + kr2_ + 2;
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
  if (dispersionAvailable_) {
    return;
  }
  this->lockMutex();
  if (!dispersionAvailable_) {
    float result[4];
    this->getInputSocketReader(1)->readSampled(result, 1, 1, PixelSampler::Nearest);
    dispersion_ = result[0];
    kr_ = 0.25f * max_ff(min_ff(dispersion_, 1.0f), 0.0f);
    kr2_ = kr_ * 20;
    dispersionAvailable_ = true;
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
  r_input_area.xmin = output_area.xmin - kr2_ - 2;
  r_input_area.xmax = output_area.xmax + kr2_ + 2;
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
    input_image->read_elem_bilinear((u * width + kr2_) - 0.5f, v * height - 0.5f, color);
    it.out[0] = color[0];
    input_image->read_elem(it.x, it.y, color);
    it.out[1] = color[1];
    input_image->read_elem_bilinear((u * width - kr2_) - 0.5f, v * height - 0.5f, color);
    it.out[2] = color[2];
    it.out[3] = 1.0f;
  }
}

}  // namespace blender::compositor

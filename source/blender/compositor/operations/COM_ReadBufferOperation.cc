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

#include "COM_ReadBufferOperation.h"

#include "COM_ExecutionGroup.h"
#include "COM_WriteBufferOperation.h"

namespace blender::compositor {

ReadBufferOperation::ReadBufferOperation(DataType datatype)
{
  this->addOutputSocket(datatype);
  single_value_ = false;
  offset_ = 0;
  buffer_ = nullptr;
  flags.is_read_buffer_operation = true;
}

void *ReadBufferOperation::initializeTileData(rcti * /*rect*/)
{
  return buffer_;
}

void ReadBufferOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (memoryProxy_ != nullptr) {
    WriteBufferOperation *operation = memoryProxy_->getWriteBufferOperation();
    operation->determine_canvas(preferred_area, r_area);
    operation->set_canvas(r_area);

    /** \todo may not occur! But does with blur node. */
    if (memoryProxy_->getExecutor()) {
      uint resolution[2] = {static_cast<uint>(BLI_rcti_size_x(&r_area)),
                            static_cast<uint>(BLI_rcti_size_y(&r_area))};
      memoryProxy_->getExecutor()->setResolution(resolution);
    }

    single_value_ = operation->isSingleValue();
  }
}
void ReadBufferOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  if (single_value_) {
    /* write buffer has a single value stored at (0,0) */
    buffer_->read(output, 0, 0);
  }
  else {
    switch (sampler) {
      case PixelSampler::Nearest:
        buffer_->read(output, x, y);
        break;
      case PixelSampler::Bilinear:
      default:
        buffer_->readBilinear(output, x, y);
        break;
      case PixelSampler::Bicubic:
        buffer_->readBilinear(output, x, y);
        break;
    }
  }
}

void ReadBufferOperation::executePixelExtend(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler,
                                             MemoryBufferExtend extend_x,
                                             MemoryBufferExtend extend_y)
{
  if (single_value_) {
    /* write buffer has a single value stored at (0,0) */
    buffer_->read(output, 0, 0);
  }
  else if (sampler == PixelSampler::Nearest) {
    buffer_->read(output, x, y, extend_x, extend_y);
  }
  else {
    buffer_->readBilinear(output, x, y, extend_x, extend_y);
  }
}

void ReadBufferOperation::executePixelFiltered(
    float output[4], float x, float y, float dx[2], float dy[2])
{
  if (single_value_) {
    /* write buffer has a single value stored at (0,0) */
    buffer_->read(output, 0, 0);
  }
  else {
    const float uv[2] = {x, y};
    const float deriv[2][2] = {{dx[0], dx[1]}, {dy[0], dy[1]}};
    buffer_->readEWA(output, uv, deriv);
  }
}

bool ReadBufferOperation::determineDependingAreaOfInterest(rcti *input,
                                                           ReadBufferOperation *readOperation,
                                                           rcti *output)
{
  if (this == readOperation) {
    BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
    return true;
  }
  return false;
}

void ReadBufferOperation::readResolutionFromWriteBuffer()
{
  if (memoryProxy_ != nullptr) {
    WriteBufferOperation *operation = memoryProxy_->getWriteBufferOperation();
    this->setWidth(operation->getWidth());
    this->setHeight(operation->getHeight());
  }
}

void ReadBufferOperation::updateMemoryBuffer()
{
  buffer_ = this->getMemoryProxy()->getBuffer();
}

}  // namespace blender::compositor

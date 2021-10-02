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
 * Copyright 2021, Blender Foundation.
 */

#include "COM_BufferOperation.h"

namespace blender::compositor {

BufferOperation::BufferOperation(MemoryBuffer *buffer, DataType data_type)
{
  buffer_ = buffer;
  inflated_buffer_ = nullptr;
  set_canvas(buffer->get_rect());
  addOutputSocket(data_type);
  flags.is_constant_operation = buffer_->is_a_single_elem();
  flags.is_fullframe_operation = false;
}

const float *BufferOperation::get_constant_elem()
{
  BLI_assert(buffer_->is_a_single_elem());
  return buffer_->getBuffer();
}

void BufferOperation::initExecution()
{
  if (buffer_->is_a_single_elem()) {
    initMutex();
  }
}

void *BufferOperation::initializeTileData(rcti * /*rect*/)
{
  if (buffer_->is_a_single_elem() == false) {
    return buffer_;
  }

  lockMutex();
  if (!inflated_buffer_) {
    inflated_buffer_ = buffer_->inflate();
  }
  unlockMutex();
  return inflated_buffer_;
}

void BufferOperation::deinitExecution()
{
  if (buffer_->is_a_single_elem()) {
    deinitMutex();
  }
  delete inflated_buffer_;
}

void BufferOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  switch (sampler) {
    case PixelSampler::Nearest:
      buffer_->read(output, x, y);
      break;
    case PixelSampler::Bilinear:
    default:
      buffer_->readBilinear(output, x, y);
      break;
    case PixelSampler::Bicubic:
      /* No bicubic. Same implementation as ReadBufferOperation. */
      buffer_->readBilinear(output, x, y);
      break;
  }
}

void BufferOperation::executePixelFiltered(
    float output[4], float x, float y, float dx[2], float dy[2])
{
  const float uv[2] = {x, y};
  const float deriv[2][2] = {{dx[0], dx[1]}, {dy[0], dy[1]}};
  buffer_->readEWA(output, uv, deriv);
}

}  // namespace blender::compositor

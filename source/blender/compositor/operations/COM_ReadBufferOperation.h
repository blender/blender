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

#pragma once

#include "COM_MemoryBuffer.h"
#include "COM_MemoryProxy.h"
#include "COM_NodeOperation.h"

namespace blender::compositor {

class ReadBufferOperation : public NodeOperation {
 private:
  MemoryProxy *memoryProxy_;
  bool single_value_; /* single value stored in buffer, copied from associated write operation */
  unsigned int offset_;
  MemoryBuffer *buffer_;

 public:
  ReadBufferOperation(DataType datatype);
  void setMemoryProxy(MemoryProxy *memoryProxy)
  {
    memoryProxy_ = memoryProxy;
  }

  MemoryProxy *getMemoryProxy() const
  {
    return memoryProxy_;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void *initializeTileData(rcti *rect) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
  void executePixelExtend(float output[4],
                          float x,
                          float y,
                          PixelSampler sampler,
                          MemoryBufferExtend extend_x,
                          MemoryBufferExtend extend_y);
  void executePixelFiltered(float output[4], float x, float y, float dx[2], float dy[2]) override;
  void setOffset(unsigned int offset)
  {
    offset_ = offset;
  }
  unsigned int getOffset() const
  {
    return offset_;
  }
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  MemoryBuffer *getInputMemoryBuffer(MemoryBuffer **memoryBuffers) override
  {
    return memoryBuffers[offset_];
  }
  void readResolutionFromWriteBuffer();
  void updateMemoryBuffer();
};

}  // namespace blender::compositor

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

#include "COM_MemoryProxy.h"
#include "COM_NodeOperation.h"

namespace blender::compositor {

class OpenCLDevice;
class MemoryProxy;

/**
 * \brief NodeOperation to write to a tile
 * \ingroup Operation
 */
class WriteBufferOperation : public NodeOperation {
  MemoryProxy *memoryProxy_;
  bool single_value_; /* single value stored in buffer */
  NodeOperation *input_;

 public:
  WriteBufferOperation(DataType datatype);
  ~WriteBufferOperation();
  MemoryProxy *getMemoryProxy()
  {
    return memoryProxy_;
  }
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
  bool isSingleValue() const
  {
    return single_value_;
  }

  void executeRegion(rcti *rect, unsigned int tileNumber) override;
  void initExecution() override;
  void deinitExecution() override;
  void executeOpenCLRegion(OpenCLDevice *device,
                           rcti *rect,
                           unsigned int chunkNumber,
                           MemoryBuffer **memoryBuffers,
                           MemoryBuffer *outputBuffer) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void readResolutionFromInputSocket();
  inline NodeOperation *getInput()
  {
    return input_;
  }
};

}  // namespace blender::compositor

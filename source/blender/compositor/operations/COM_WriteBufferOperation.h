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
  MemoryProxy *memory_proxy_;
  bool single_value_; /* single value stored in buffer */
  NodeOperation *input_;

 public:
  WriteBufferOperation(DataType datatype);
  ~WriteBufferOperation();
  MemoryProxy *get_memory_proxy()
  {
    return memory_proxy_;
  }
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  bool is_single_value() const
  {
    return single_value_;
  }

  void execute_region(rcti *rect, unsigned int tile_number) override;
  void init_execution() override;
  void deinit_execution() override;
  void execute_opencl_region(OpenCLDevice *device,
                             rcti *rect,
                             unsigned int chunk_number,
                             MemoryBuffer **memory_buffers,
                             MemoryBuffer *output_buffer) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void read_resolution_from_input_socket();
  inline NodeOperation *get_input()
  {
    return input_;
  }
};

}  // namespace blender::compositor

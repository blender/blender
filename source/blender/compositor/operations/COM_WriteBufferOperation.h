/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

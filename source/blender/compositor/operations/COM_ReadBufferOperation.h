/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MemoryBuffer.h"
#include "COM_MemoryProxy.h"
#include "COM_NodeOperation.h"

namespace blender::compositor {

class ReadBufferOperation : public NodeOperation {
 private:
  MemoryProxy *memory_proxy_;
  bool single_value_; /* single value stored in buffer, copied from associated write operation */
  unsigned int offset_;
  MemoryBuffer *buffer_;

 public:
  ReadBufferOperation(DataType datatype);
  void set_memory_proxy(MemoryProxy *memory_proxy)
  {
    memory_proxy_ = memory_proxy;
  }

  MemoryProxy *get_memory_proxy() const
  {
    return memory_proxy_;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void *initialize_tile_data(rcti *rect) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  void execute_pixel_extend(float output[4],
                            float x,
                            float y,
                            PixelSampler sampler,
                            MemoryBufferExtend extend_x,
                            MemoryBufferExtend extend_y);
  void execute_pixel_filtered(
      float output[4], float x, float y, float dx[2], float dy[2]) override;
  void set_offset(unsigned int offset)
  {
    offset_ = offset;
  }
  unsigned int get_offset() const
  {
    return offset_;
  }
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  MemoryBuffer *get_input_memory_buffer(MemoryBuffer **memory_buffers) override
  {
    return memory_buffers[offset_];
  }
  void read_resolution_from_write_buffer();
  void update_memory_buffer();
};

}  // namespace blender::compositor

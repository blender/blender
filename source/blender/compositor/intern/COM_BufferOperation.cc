/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BufferOperation.h"

namespace blender::compositor {

BufferOperation::BufferOperation(MemoryBuffer *buffer, DataType data_type)
{
  buffer_ = buffer;
  inflated_buffer_ = nullptr;
  set_canvas(buffer->get_rect());
  add_output_socket(data_type);
  flags_.is_constant_operation = buffer_->is_a_single_elem();
  flags_.is_fullframe_operation = false;
}

const float *BufferOperation::get_constant_elem()
{
  BLI_assert(buffer_->is_a_single_elem());
  return buffer_->get_buffer();
}

void BufferOperation::init_execution()
{
  if (buffer_->is_a_single_elem()) {
    init_mutex();
  }
}

void *BufferOperation::initialize_tile_data(rcti * /*rect*/)
{
  if (buffer_->is_a_single_elem() == false) {
    return buffer_;
  }

  lock_mutex();
  if (!inflated_buffer_) {
    inflated_buffer_ = buffer_->inflate();
  }
  unlock_mutex();
  return inflated_buffer_;
}

void BufferOperation::deinit_execution()
{
  if (buffer_->is_a_single_elem()) {
    deinit_mutex();
  }
  delete inflated_buffer_;
}

void BufferOperation::execute_pixel_sampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  switch (sampler) {
    case PixelSampler::Nearest:
      buffer_->read(output, x, y);
      break;
    case PixelSampler::Bilinear:
    default:
      buffer_->read_bilinear(output, x, y);
      break;
    case PixelSampler::Bicubic:
      /* No bicubic. Same implementation as ReadBufferOperation. */
      buffer_->read_bilinear(output, x, y);
      break;
  }
}

void BufferOperation::execute_pixel_filtered(
    float output[4], float x, float y, float dx[2], float dy[2])
{
  const float uv[2] = {x, y};
  const float deriv[2][2] = {{dx[0], dx[1]}, {dy[0], dy[1]}};
  buffer_->readEWA(output, uv, deriv);
}

}  // namespace blender::compositor

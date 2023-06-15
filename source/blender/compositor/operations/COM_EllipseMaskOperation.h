/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class EllipseMaskOperation : public MultiThreadedOperation {
 private:
  using MaskFunc = std::function<float(bool is_inside, const float *mask, const float *value)>;

  /**
   * Cached reference to the input_program
   */
  SocketReader *input_mask_;
  SocketReader *input_value_;

  float sine_;
  float cosine_;
  float aspect_ratio_;
  int mask_type_;

  NodeEllipseMask *data_;

 public:
  EllipseMaskOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_data(NodeEllipseMask *data)
  {
    data_ = data;
  }

  void set_mask_type(int mask_type)
  {
    mask_type_ = mask_type;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void apply_mask(MemoryBuffer *output,
                  const rcti &area,
                  Span<MemoryBuffer *> inputs,
                  MaskFunc mask_func);
};

}  // namespace blender::compositor

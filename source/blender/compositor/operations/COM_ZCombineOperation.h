/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MixOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ZCombineOperation : public MultiThreadedOperation {
 protected:
  SocketReader *image1Reader_;
  SocketReader *depth1Reader_;
  SocketReader *image2Reader_;
  SocketReader *depth2Reader_;

 public:
  /**
   * Default constructor
   */
  ZCombineOperation();

  void init_execution() override;
  void deinit_execution() override;

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class ZCombineAlphaOperation : public ZCombineOperation {
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class ZCombineMaskOperation : public MultiThreadedOperation {
 protected:
  SocketReader *mask_reader_;
  SocketReader *image1Reader_;
  SocketReader *image2Reader_;

 public:
  ZCombineMaskOperation();

  void init_execution() override;
  void deinit_execution() override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};
class ZCombineMaskAlphaOperation : public ZCombineMaskOperation {
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

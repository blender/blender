/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_CurveBaseOperation.h"

namespace blender::compositor {

class ColorCurveOperation : public CurveBaseOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_fac_program_;
  SocketReader *input_image_program_;
  SocketReader *input_black_program_;
  SocketReader *input_white_program_;

 public:
  ColorCurveOperation();

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

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class ConstantLevelColorCurveOperation : public CurveBaseOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_fac_program_;
  SocketReader *input_image_program_;
  float black_[3];
  float white_[3];

 public:
  ConstantLevelColorCurveOperation();

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

  void set_black_level(float black[3])
  {
    copy_v3_v3(black_, black);
  }
  void set_white_level(float white[3])
  {
    copy_v3_v3(white_, white);
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

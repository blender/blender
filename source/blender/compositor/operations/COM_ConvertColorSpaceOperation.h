/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConvertColorSpaceNode.h"
#include "COM_MultiThreadedOperation.h"
#include "IMB_colormanagement.h"

namespace blender::compositor {

class ConvertColorSpaceOperation : public MultiThreadedOperation {
 private:
  SocketReader *input_program_;
  NodeConvertColorSpace *settings_;
  ColormanageProcessor *color_processor_;

 public:
  ConvertColorSpaceOperation();

  void set_settings(NodeConvertColorSpace *node_color_space);
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

}  // namespace blender::compositor

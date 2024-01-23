/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ChannelMatteOperation : public MultiThreadedOperation {
 private:
  SocketReader *input_image_program_;

  // int color_space_; /* node->custom1 */ /* UNUSED */ /* TODO? */
  int matte_channel_; /* node->custom2 */
  int limit_method_;  /* node->algorithm */
  int limit_channel_; /* node->channel */
  float limit_max_;   /* node->storage->t1 */
  float limit_min_;   /* node->storage->t2 */

  float limit_range_;

  /**
   * ids to use for the operations (max and simple)
   * alpha = in[ids[0]] - std::max(in[ids[1]], in[ids[2]])
   * the simple operation is using:
   * alpha = in[ids[0]] - in[ids[1]]
   * but to use the same formula and operation for both we do:
   * ids[2] = ids[1]
   * alpha = in[ids[0]] - std::max(in[ids[1]], in[ids[2]])
   */
  int ids_[3];

 public:
  /**
   * Default constructor
   */
  ChannelMatteOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void init_execution() override;
  void deinit_execution() override;

  void set_settings(NodeChroma *node_chroma, const int custom2)
  {
    limit_max_ = node_chroma->t1;
    limit_min_ = node_chroma->t2;
    limit_method_ = node_chroma->algorithm;
    limit_channel_ = node_chroma->channel;
    matte_channel_ = custom2;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

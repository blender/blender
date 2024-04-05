/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

class VectorBlurOperation : public NodeOperation {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int DEPTH_INPUT_INDEX = 1;
  static constexpr int VELOCITY_INPUT_INDEX = 2;

  const NodeBlurData *settings_;

 public:
  VectorBlurOperation();

  void set_vector_blur_settings(const NodeBlurData *settings)
  {
    settings_ = settings;
  }

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
};

}  // namespace blender::compositor

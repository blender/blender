/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"
#include "DNA_node_types.h"

namespace blender::compositor {

class VectorBlurOperation : public NodeOperation, public QualityStepHelper {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int Z_INPUT_INDEX = 1;
  static constexpr int SPEED_INPUT_INDEX = 2;

  /**
   * \brief settings of the glare node.
   */
  const NodeBlurData *settings_;

  float *cached_instance_;

 public:
  VectorBlurOperation();

  void init_execution() override;
  void deinit_execution() override;

  void set_vector_blur_settings(const NodeBlurData *settings)
  {
    settings_ = settings;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 protected:
  void generate_vector_blur(float *data,
                            MemoryBuffer *input_image,
                            MemoryBuffer *input_speed,
                            MemoryBuffer *inputZ);
};

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_light_types.h"

namespace blender::compositor {

class GlareThresholdOperation : public MultiThreadedOperation {
 private:
  /**
   * \brief settings of the glare node.
   */
  const NodeGlare *settings_;

 public:
  GlareThresholdOperation();

  void set_glare_settings(const NodeGlare *settings)
  {
    settings_ = settings;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

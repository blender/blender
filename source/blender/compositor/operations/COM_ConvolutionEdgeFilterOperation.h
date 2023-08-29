/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConvolutionFilterOperation.h"

namespace blender::compositor {

class ConvolutionEdgeFilterOperation : public ConvolutionFilterOperation {
 public:
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

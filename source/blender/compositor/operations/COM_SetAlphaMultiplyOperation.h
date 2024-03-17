/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * This operation will apply a mask to its input image.
 *
 * `output color.rgba = input color.rgba * input alpha`
 */
class SetAlphaMultiplyOperation : public MultiThreadedOperation {
 public:
  SetAlphaMultiplyOperation();

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

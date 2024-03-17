/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class KuwaharaClassicOperation : public MultiThreadedOperation {
  bool high_precision_;

 public:
  KuwaharaClassicOperation();

  void set_high_precision(bool high_precision)
  {
    high_precision_ = high_precision;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

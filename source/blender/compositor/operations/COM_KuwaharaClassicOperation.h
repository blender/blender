/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class KuwaharaClassicOperation : public MultiThreadedOperation {
  const NodeKuwaharaData *data_;

 public:
  KuwaharaClassicOperation();

  void set_data(const NodeKuwaharaData *data)
  {
    data_ = data;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

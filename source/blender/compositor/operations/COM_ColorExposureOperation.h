/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

class ExposureOperation : public MultiThreadedRowOperation {
 public:
  ExposureOperation();

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConvertColorSpaceNode.h"
#include "COM_MultiThreadedOperation.h"
#include "IMB_colormanagement.hh"

namespace blender::compositor {

class ConvertColorSpaceOperation : public MultiThreadedOperation {
 private:
  NodeConvertColorSpace *settings_;
  ColormanageProcessor *color_processor_;

 public:
  ConvertColorSpaceOperation();

  void set_settings(NodeConvertColorSpace *node_color_space);

  void init_execution() override;
  void deinit_execution() override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

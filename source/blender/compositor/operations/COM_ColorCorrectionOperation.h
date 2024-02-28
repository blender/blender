/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

class ColorCorrectionOperation : public MultiThreadedRowOperation {
 private:
  NodeColorCorrection *data_;

  bool red_channel_enabled_;
  bool green_channel_enabled_;
  bool blue_channel_enabled_;

 public:
  ColorCorrectionOperation();

  void set_data(NodeColorCorrection *data)
  {
    data_ = data;
  }
  void set_red_channel_enabled(bool enabled)
  {
    red_channel_enabled_ = enabled;
  }
  void set_green_channel_enabled(bool enabled)
  {
    green_channel_enabled_ = enabled;
  }
  void set_blue_channel_enabled(bool enabled)
  {
    blue_channel_enabled_ = enabled;
  }

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor

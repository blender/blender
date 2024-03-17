/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_texture_types.h"

namespace blender::compositor {

class ColorRampOperation : public MultiThreadedOperation {
 private:
  ColorBand *color_band_;

 public:
  ColorRampOperation();

  void set_color_band(ColorBand *color_band)
  {
    color_band_ = color_band;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

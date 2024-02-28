/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_CurveBaseOperation.h"

namespace blender::compositor {

class ColorCurveOperation : public CurveBaseOperation {
 public:
  ColorCurveOperation();

  void init_execution() override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class ConstantLevelColorCurveOperation : public CurveBaseOperation {
 private:
  float black_[3];
  float white_[3];

 public:
  ConstantLevelColorCurveOperation();

  void init_execution() override;

  void set_black_level(float black[3])
  {
    copy_v3_v3(black_, black);
  }
  void set_white_level(float white[3])
  {
    copy_v3_v3(white_, white);
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

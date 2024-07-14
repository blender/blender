/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

class ColorBalanceWhitepointOperation : public MultiThreadedRowOperation {
 protected:
  float input_temperature_;
  float input_tint_;
  float output_temperature_;
  float output_tint_;

  float4x4 matrix_;

 public:
  ColorBalanceWhitepointOperation();

  virtual void init_execution() override;

  void set_parameters(const float input_temperature,
                      const float input_tint,
                      const float output_temperature,
                      const float output_tint)
  {
    input_temperature_ = input_temperature;
    input_tint_ = input_tint;
    output_temperature_ = output_temperature;
    output_tint_ = output_tint;
  }

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor

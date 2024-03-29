/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "paint_intern.hh"

#include "BLI_math_vector.hh"

namespace blender::ed::sculpt_paint {

struct InputSample {
  float2 mouse_position;
  float pressure;
};

class GreasePencilStrokeOperation {
 public:
  virtual ~GreasePencilStrokeOperation() = default;
  virtual void on_stroke_begin(const bContext &C, const InputSample &start_sample) = 0;
  virtual void on_stroke_extended(const bContext &C, const InputSample &extension_sample) = 0;
  virtual void on_stroke_done(const bContext &C) = 0;
};

namespace greasepencil {

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_erase_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_tint_operation();

}  // namespace greasepencil

}  // namespace blender::ed::sculpt_paint

/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "paint_intern.hh"

#include "BLI_math_vector.hh"

namespace blender::ed::sculpt_paint {

struct StrokeExtension {
  bool is_first;
  float2 mouse_position;
  float pressure;
};

class GreasePencilStrokeOperation {
 public:
  virtual ~GreasePencilStrokeOperation() = default;
  virtual void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) = 0;
  virtual void on_stroke_done(const bContext &C) = 0;
};

namespace greasepencil {

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation();

}  // namespace greasepencil

}  // namespace blender::ed::sculpt_paint

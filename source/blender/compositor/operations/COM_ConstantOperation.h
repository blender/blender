/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

/* TODO(manzanilla): After removing tiled implementation, implement a default #determine_resolution
 * for all constant operations and make all initialization and deinitilization methods final. */
/**
 * Base class for operations that are always constant. Operations that can be constant only when
 * all their inputs are so, are evaluated into primitive constants (Color/Vector/Value) during
 * constant folding.
 */
class ConstantOperation : public NodeOperation {
 protected:
  bool needs_canvas_to_get_constant_;

 public:
  ConstantOperation();

  /** May require resolution to be already determined. */
  virtual const float *get_constant_elem() = 0;
  bool can_get_constant_elem() const;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) final;
};

}  // namespace blender::compositor

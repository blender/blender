/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

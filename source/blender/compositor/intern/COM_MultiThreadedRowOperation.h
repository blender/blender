/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.h"

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * Executes buffer updates per row. To be inherited only by operations with correlated coordinates
 * between inputs and output.
 */
class MultiThreadedRowOperation : public MultiThreadedOperation {
 protected:
  struct PixelCursor {
    float *out;
    int out_stride;
    const float *row_end;
    Array<const float *> ins;
    Array<int> in_strides;

   public:
    PixelCursor(int num_inputs);

    void next()
    {
      BLI_assert(out < row_end);
      out += out_stride;
      for (int i = 0; i < ins.size(); i++) {
        ins[i] += in_strides[i];
      }
    }
  };

 protected:
  virtual void update_memory_buffer_row(PixelCursor &p) = 0;

 private:
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) final;
};

}  // namespace blender::compositor

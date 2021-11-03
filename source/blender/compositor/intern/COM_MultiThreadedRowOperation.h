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

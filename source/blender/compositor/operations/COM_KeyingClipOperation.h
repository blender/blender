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
 * Copyright 2012, Blender Foundation.
 */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * Class with implementation of black/white clipping for keying node
 */
class KeyingClipOperation : public MultiThreadedOperation {
 protected:
  float clipBlack_;
  float clipWhite_;

  int kernelRadius_;
  float kernelTolerance_;

  bool isEdgeMatte_;

 public:
  KeyingClipOperation();

  void setClipBlack(float value)
  {
    clipBlack_ = value;
  }
  void setClipWhite(float value)
  {
    clipWhite_ = value;
  }

  void setKernelRadius(int value)
  {
    kernelRadius_ = value;
  }
  void setKernelTolerance(float value)
  {
    kernelTolerance_ = value;
  }

  void setIsEdgeMatte(bool value)
  {
    isEdgeMatte_ = value;
  }

  void *initializeTileData(rcti *rect) override;

  void executePixel(float output[4], int x, int y, void *data) override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(const int input_idx,
                            const rcti &output_area,
                            rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

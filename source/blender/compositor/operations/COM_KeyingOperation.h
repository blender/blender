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

#include <string.h>

#include "COM_MultiThreadedOperation.h"

#include "BLI_listbase.h"

namespace blender::compositor {

/**
 * Class with implementation of keying node
 */
class KeyingOperation : public MultiThreadedOperation {
 protected:
  SocketReader *pixel_reader_;
  SocketReader *screen_reader_;

  float screen_balance_;

 public:
  KeyingOperation();

  void init_execution() override;
  void deinit_execution() override;

  void set_screen_balance(float value)
  {
    screen_balance_ = value;
  }

  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

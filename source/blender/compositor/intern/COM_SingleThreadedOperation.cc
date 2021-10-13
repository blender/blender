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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_SingleThreadedOperation.h"

namespace blender::compositor {

SingleThreadedOperation::SingleThreadedOperation()
{
  cached_instance_ = nullptr;
  flags_.complex = true;
  flags_.single_threaded = true;
}

void SingleThreadedOperation::init_execution()
{
  init_mutex();
}

void SingleThreadedOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  cached_instance_->read_no_check(output, x, y);
}

void SingleThreadedOperation::deinit_execution()
{
  deinit_mutex();
  if (cached_instance_) {
    delete cached_instance_;
    cached_instance_ = nullptr;
  }
}
void *SingleThreadedOperation::initialize_tile_data(rcti *rect)
{
  if (cached_instance_) {
    return cached_instance_;
  }

  lock_mutex();
  if (cached_instance_ == nullptr) {
    //
    cached_instance_ = create_memory_buffer(rect);
  }
  unlock_mutex();
  return cached_instance_;
}

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

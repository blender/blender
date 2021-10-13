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
  cachedInstance_ = nullptr;
  flags.complex = true;
  flags.single_threaded = true;
}

void SingleThreadedOperation::initExecution()
{
  initMutex();
}

void SingleThreadedOperation::executePixel(float output[4], int x, int y, void * /*data*/)
{
  cachedInstance_->readNoCheck(output, x, y);
}

void SingleThreadedOperation::deinitExecution()
{
  deinitMutex();
  if (cachedInstance_) {
    delete cachedInstance_;
    cachedInstance_ = nullptr;
  }
}
void *SingleThreadedOperation::initializeTileData(rcti *rect)
{
  if (cachedInstance_) {
    return cachedInstance_;
  }

  lockMutex();
  if (cachedInstance_ == nullptr) {
    //
    cachedInstance_ = createMemoryBuffer(rect);
  }
  unlockMutex();
  return cachedInstance_;
}

}  // namespace blender::compositor

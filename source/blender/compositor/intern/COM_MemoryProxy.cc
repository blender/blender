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

#include "COM_MemoryProxy.h"
#include "COM_MemoryBuffer.h"

namespace blender::compositor {

MemoryProxy::MemoryProxy(DataType datatype)
{
  write_buffer_operation_ = nullptr;
  executor_ = nullptr;
  datatype_ = datatype;
}

void MemoryProxy::allocate(unsigned int width, unsigned int height)
{
  rcti result;
  result.xmin = 0;
  result.xmax = width;
  result.ymin = 0;
  result.ymax = height;

  buffer_ = new MemoryBuffer(this, result, MemoryBufferState::Default);
}

void MemoryProxy::free()
{
  if (buffer_) {
    delete buffer_;
    buffer_ = nullptr;
  }
}

}  // namespace blender::compositor

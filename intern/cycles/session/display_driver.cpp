/* SPDX-FileCopyrightText: 2021-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "session/display_driver.h"

#ifdef _WIN32
#  include "util/windows.h"
#else
#  include <unistd.h>
#endif

CCL_NAMESPACE_BEGIN

GraphicsInteropBuffer::~GraphicsInteropBuffer()
{
  clear();
}

void GraphicsInteropBuffer::assign(GraphicsInteropDevice::Type type, int64_t handle, size_t size)
{
  clear();

  type_ = type;
  handle_ = handle;
  own_handle_ = true;
  size_ = size;
}

bool GraphicsInteropBuffer::is_empty() const
{
  return handle_ == 0;
}

void GraphicsInteropBuffer::zero()
{
  need_zero_ = true;
}

void GraphicsInteropBuffer::clear()
{
  if (type_ == GraphicsInteropDevice::VULKAN && handle_ && own_handle_) {
#ifdef _WIN32
    CloseHandle(HANDLE(handle_));
#else
    close(handle_);
#endif
  }

  type_ = GraphicsInteropDevice::NONE;
  handle_ = 0;
  size_ = 0;
  need_zero_ = false;
  own_handle_ = false;
}

GraphicsInteropDevice::Type GraphicsInteropBuffer::get_type() const
{
  return type_;
}

size_t GraphicsInteropBuffer::get_size() const
{
  return size_;
}

bool GraphicsInteropBuffer::has_new_handle() const
{
  return own_handle_;
}

bool GraphicsInteropBuffer::take_zero()
{
  bool need_zero = need_zero_;
  need_zero_ = false;
  return need_zero;
}

int64_t GraphicsInteropBuffer::take_handle()
{
  assert(own_handle_);
  own_handle_ = false;
  return handle_;
}

CCL_NAMESPACE_END

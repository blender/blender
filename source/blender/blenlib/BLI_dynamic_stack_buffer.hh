/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_assert.h"

#include "MEM_guardedalloc.h"

namespace blender {

/**
 * A dynamic stack buffer can be used instead of #alloca when one wants to allocate a dynamic
 * amount of memory on the stack. Using this class has some advantages:
 *  - It falls back to heap allocation, when the size is too large.
 *  - It can be used in loops safely.
 *  - If the buffer is heap allocated, it is free automatically in the destructor.
 */
template<size_t ReservedSize = 64, size_t ReservedAlignment = 64>
class alignas(ReservedAlignment) DynamicStackBuffer {
 private:
  /* Don't create an empty array. This causes problems with some compilers. */
  char reserved_buffer_[(ReservedSize > 0) ? ReservedSize : 1];
  void *buffer_;

 public:
  DynamicStackBuffer(const int64_t size, const int64_t alignment)
  {
    BLI_assert(size >= 0);
    BLI_assert(alignment >= 0);
    if (size <= ReservedSize && alignment <= ReservedAlignment) {
      buffer_ = reserved_buffer_;
    }
    else {
      buffer_ = MEM_new_uninitialized_aligned(size, alignment, __func__);
    }
  }
  ~DynamicStackBuffer()
  {
    if (buffer_ != reserved_buffer_) {
      MEM_delete_void(buffer_);
    }
  }

  /* Don't allow any copying or moving of this type. */
  DynamicStackBuffer(const DynamicStackBuffer &other) = delete;
  DynamicStackBuffer(DynamicStackBuffer &&other) = delete;
  DynamicStackBuffer &operator=(const DynamicStackBuffer &other) = delete;
  DynamicStackBuffer &operator=(DynamicStackBuffer &&other) = delete;

  void *buffer() const
  {
    return buffer_;
  }
};

}  // namespace blender

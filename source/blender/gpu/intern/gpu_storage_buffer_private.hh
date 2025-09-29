/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"

namespace blender::gpu {

class StorageBuf;
class VertBuf;

#ifndef NDEBUG
#  define DEBUG_NAME_LEN 64
#else
#  define DEBUG_NAME_LEN 8
#endif

/**
 * Implementation of Storage Buffers.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class StorageBuf {
 protected:
  /** Data size in bytes. Doesn't need to match actual allocation size due to alignment rules. */
  size_t size_in_bytes_ = -1;
  size_t usage_size_in_bytes_ = -1;
  /** Continuous memory block to copy to GPU. This data is owned by the StorageBuf. */
  void *data_ = nullptr;
  /** Debugging name */
  char name_[DEBUG_NAME_LEN] = {};

 public:
  StorageBuf(size_t size, const char *name);
  virtual ~StorageBuf();
  void usage_size_set(size_t size);
  size_t usage_size_get() const
  {
    return usage_size_in_bytes_;
  }
  virtual void update(const void *data) = 0;
  virtual void bind(int slot) = 0;
  virtual void unbind() = 0;
  virtual void clear(uint32_t clear_value) = 0;
  virtual void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) = 0;
  virtual void read(void *data) = 0;
  virtual void async_flush_to_host() = 0;
  virtual void sync_as_indirect_buffer() = 0;
};

#undef DEBUG_NAME_LEN

}  // namespace blender::gpu

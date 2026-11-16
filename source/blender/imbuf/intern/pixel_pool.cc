/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>

#include "BLI_implicit_sharing.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.hh"

#include "pixel_pool.hh"

namespace blender::imbuf {

static constexpr size_t minimum_size_for_pool = 8 * 1024 * 1024;

struct Pool {
  struct Entry {
    void *data;
    size_t size;
    size_t alignment;
  };

  Vector<Entry> entries;
  Mutex mutex;
  size_t bytes = 0;
  size_t limit = 0;

  ~Pool()
  {
    Pool::free_entries(this->entries);
  }

  static void free_entries(const Span<Entry> entries)
  {
    for (const Entry &entry : entries) {
      MEM_delete_void(entry.data);
    }
  }

  void release(const Entry entry)
  {
    Vector<Entry> evicted;
    {
      std::lock_guard lock(this->mutex);
      if (entry.size <= this->limit) {
        this->entries.append(entry);
        this->bytes += entry.size;
        this->trim(this->limit, evicted);
      }
      else {
        evicted.append(entry);
      }
    }
    Pool::free_entries(evicted);
  }

  void trim(const size_t max_unused_bytes, Vector<Entry> &r_evicted)
  {
    while (this->bytes > max_unused_bytes) {
      const Entry entry = this->entries[0];
      this->entries.remove(0);
      this->bytes -= entry.size;
      r_evicted.append(entry);
    }
  }
};

static Pool &pool()
{
  static Pool instance;
  return instance;
}

class PooledSharingInfo : public ImplicitSharingInfo {
 private:
  Pool::Entry entry_;

 public:
  explicit PooledSharingInfo(const Pool::Entry entry) : entry_(entry) {}

 private:
  void delete_data_only() override
  {
    if (entry_.data) {
      pool().release(entry_);
      entry_.data = nullptr;
    }
  }

  void delete_self_with_data() override
  {
    this->delete_data_only();
    MEM_delete(this);
  }
};

PixelPoolAllocation pixel_pool_allocate(const size_t size,
                                        const size_t alignment,
                                        bool initialize_pixels,
                                        const bool use_pool)
{
  void *data = nullptr;
  bool pooled = false;
  if (use_pool && size >= minimum_size_for_pool) {
    Pool &cache = pool();
    std::lock_guard lock(cache.mutex);
    pooled = size <= cache.limit;
    if (pooled) {
      /* Prefer the most recently returned compatible storage. */
      for (int64_t i = cache.entries.size() - 1; i >= 0; i--) {
        const Pool::Entry &entry = cache.entries[i];
        if (entry.size == size && entry.alignment == alignment) {
          data = entry.data;
          cache.entries.remove(i);
          cache.bytes -= size;
          break;
        }
      }
    }
  }

  if (!data) {
    data = alignment         ? MEM_new_uninitialized_aligned(size, alignment, "ImBuf pixels") :
           initialize_pixels ? MEM_new_zeroed(size, "ImBuf pixels") :
                               MEM_new_uninitialized(size, "ImBuf pixels");
    if (!alignment) {
      initialize_pixels = false;
    }
  }
  if (!data) {
    return {};
  }
  if (initialize_pixels) {
    memset(data, 0, size);
  }

  const ImplicitSharingInfo *sharing_info =
      pooled ? MEM_new<PooledSharingInfo>(__func__, Pool::Entry{data, size, alignment}) :
               implicit_sharing::info_for_mem_free(static_cast<uint8_t *>(data));
  return {data, ImplicitSharingPtr<>(sharing_info)};
}

}  // namespace blender::imbuf

namespace blender {

size_t IMB_pixel_pool_unused_bytes_get()
{
  using namespace imbuf;
  Pool &cache = pool();
  std::lock_guard lock(cache.mutex);
  return cache.bytes;
}

void IMB_pixel_pool_set_limit(const size_t limit)
{
  using namespace imbuf;
  Pool &cache = pool();
  Vector<Pool::Entry> evicted;
  {
    std::lock_guard lock(cache.mutex);
    cache.limit = limit;
    cache.trim(limit, evicted);
    if (limit == 0) {
      cache.entries.clear_and_shrink();
    }
  }
  Pool::free_entries(evicted);
}

void IMB_pixel_pool_trim(const size_t max_unused_bytes)
{
  using namespace imbuf;
  Pool &cache = pool();
  Vector<Pool::Entry> evicted;
  {
    std::lock_guard lock(cache.mutex);
    cache.trim(max_unused_bytes, evicted);
  }
  Pool::free_entries(evicted);
}

void IMB_pixel_pool_clear()
{
  using namespace imbuf;
  Pool &cache = pool();
  Vector<Pool::Entry> evicted;
  {
    std::lock_guard lock(cache.mutex);
    evicted = std::move(cache.entries);
    cache.bytes = 0;
  }
  Pool::free_entries(evicted);
}

}  // namespace blender

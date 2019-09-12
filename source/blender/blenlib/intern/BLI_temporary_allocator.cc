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
 */

#include <mutex>
#include <stack>

#include "BLI_temporary_allocator.h"
#include "BLI_stack_cxx.h"

using namespace BLI;

constexpr uint ALIGNMENT = BLI_TEMPORARY_BUFFER_ALIGNMENT;
constexpr uint SMALL_BUFFER_SIZE = 64 * 1024;
constexpr uintptr_t ALIGNMENT_MASK = ~(uintptr_t)(ALIGNMENT - 1);

enum TemporaryBufferType {
  Small,
  Large,
};

struct MemHead {
  void *raw_ptr;
  TemporaryBufferType type;
};

static MemHead &get_memhead(void *aligned_ptr)
{
  return *((MemHead *)aligned_ptr - 1);
}

static void *raw_allocate(uint size)
{
  uint total_allocation_size = size + ALIGNMENT + sizeof(MemHead);

  uintptr_t raw_ptr = (uintptr_t)malloc(total_allocation_size);
  uintptr_t aligned_ptr = (raw_ptr + ALIGNMENT + sizeof(MemHead)) & ALIGNMENT_MASK;

  MemHead &memhead = get_memhead((void *)aligned_ptr);
  memhead.raw_ptr = (void *)raw_ptr;
  return (void *)aligned_ptr;
}

static void raw_deallocate(void *ptr)
{
  BLI_assert(((uintptr_t)ptr & ~ALIGNMENT_MASK) == 0);
  MemHead &memhead = get_memhead(ptr);
  void *raw_ptr = memhead.raw_ptr;
  free(raw_ptr);
}

struct ThreadLocalBuffers {
  uint allocated_amount = 0;
  Stack<void *, 32, RawAllocator> buffers;

  ~ThreadLocalBuffers()
  {
    for (void *ptr : buffers) {
      raw_deallocate(ptr);
    }
  }
};

thread_local ThreadLocalBuffers local_storage;

void *BLI_temporary_allocate(uint size)
{
  /* The total amount of allocated buffers using this allocator should be limited by a constant. If
   * it grows unbounded, there is likely a memory leak somewhere. */
  BLI_assert(local_storage.allocated_amount < 100);

  if (size <= SMALL_BUFFER_SIZE) {
    auto &buffers = local_storage.buffers;
    if (buffers.empty()) {
      void *ptr = raw_allocate(SMALL_BUFFER_SIZE);
      MemHead &memhead = get_memhead(ptr);
      memhead.type = TemporaryBufferType::Small;
      local_storage.allocated_amount++;
      return ptr;
    }
    else {
      return buffers.pop();
    }
  }
  else {
    void *ptr = raw_allocate(size);
    MemHead &memhead = get_memhead(ptr);
    memhead.type = TemporaryBufferType::Large;
    return ptr;
  }
}

void BLI_temporary_deallocate(void *buffer)
{
  MemHead &memhead = get_memhead(buffer);
  if (memhead.type == TemporaryBufferType::Small) {
    auto &buffers = local_storage.buffers;
    buffers.push(buffer);
  }
  else {
    raw_deallocate(buffer);
  }
}

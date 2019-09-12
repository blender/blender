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

/** \file
 * \ingroup bli
 *
 * This file offers a couple of memory allocators that can be used with containers such as Vector
 * and Map. Note that these allocators are not designed to work with standard containers like
 * std::vector.
 *
 * Also see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2271.html for why the standard
 * allocators are not a good fit applications like Blender. The current implementations in this
 * file are fairly simple still, more complexity can be added when necessary. For now they do their
 * job good enough.
 */

#pragma once

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_base.h"
#include "BLI_temporary_allocator.h"

namespace BLI {

/**
 * Use Blenders guarded allocator (aka MEM_malloc). This should always be used except there is a
 * good reason not to use it.
 */
class GuardedAllocator {
 public:
  void *allocate(uint size, const char *name)
  {
    return MEM_mallocN(size, name);
  }

  void *allocate_aligned(uint size, uint alignment, const char *name)
  {
    alignment = std::max<uint>(alignment, 8);
    return MEM_mallocN_aligned(size, alignment, name);
  }

  void deallocate(void *ptr)
  {
    MEM_freeN(ptr);
  }
};

/**
 * This is a simple wrapper around malloc/free. Only use this when the GuardedAllocator cannot be
 * used. This can be the case when the allocated element might live longer than Blenders Allocator.
 */
class RawAllocator {
 private:
  struct MemHead {
    int offset;
  };

 public:
  void *allocate(uint size, const char *UNUSED(name))
  {
    void *ptr = malloc(size + sizeof(MemHead));
    ((MemHead *)ptr)->offset = sizeof(MemHead);
    return POINTER_OFFSET(ptr, sizeof(MemHead));
  }

  void *allocate_aligned(uint size, uint alignment, const char *UNUSED(name))
  {
    BLI_assert(is_power_of_2_i(alignment));
    void *ptr = malloc(size + alignment + sizeof(MemHead));
    void *used_ptr = (void *)((uintptr_t)POINTER_OFFSET(ptr, alignment + sizeof(MemHead)) &
                              ~((uintptr_t)alignment - 1));
    uint offset = (uintptr_t)used_ptr - (uintptr_t)ptr;
    BLI_assert(offset >= sizeof(MemHead));
    ((MemHead *)used_ptr - 1)->offset = offset;
    return used_ptr;
  }

  void deallocate(void *ptr)
  {
    MemHead *head = (MemHead *)ptr - 1;
    int offset = -head->offset;
    void *actual_pointer = POINTER_OFFSET(ptr, offset);
    free(actual_pointer);
  }
};

/**
 * Use this only under specific circumstances as described in BLI_temporary_allocator.h.
 */
class TemporaryAllocator {
 public:
  void *allocate(uint size, const char *UNUSED(name))
  {
    return BLI_temporary_allocate(size);
  }

  void *allocate_aligned(uint size, uint alignment, const char *UNUSED(name))
  {
    BLI_assert(alignment <= 64);
    UNUSED_VARS_NDEBUG(alignment);
    return BLI_temporary_allocate(size);
  }

  void deallocate(void *ptr)
  {
    BLI_temporary_deallocate(ptr);
  }
};

}  // namespace BLI

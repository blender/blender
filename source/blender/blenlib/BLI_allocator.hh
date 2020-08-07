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
#pragma once

/** \file
 * \ingroup bli
 *
 * An `Allocator` can allocate and deallocate memory. It is used by containers such as
 * blender::Vector. The allocators defined in this file do not work with standard library
 * containers such as std::vector.
 *
 * Every allocator has to implement two methods:
 *   void *allocate(size_t size, size_t alignment, const char *name);
 *   void deallocate(void *ptr);
 *
 * We don't use the std::allocator interface, because it does more than is really necessary for an
 * allocator and has some other quirks. It mixes the concepts of allocation and construction. It is
 * essentially forced to be a template, even though the allocator should not care about the type.
 * Also see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2271.html#std_allocator. Some
 * of these aspects have been improved in new versions of C++, so we might have to reevaluate the
 * strategy later on.
 *
 * The allocator interface dictated by this file is very simplistic, but for now that is all we
 * need. More complexity can be added when it seems necessary.
 */

#include <algorithm>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

namespace blender {

/**
 * Use Blender's guarded allocator (aka MEM_*). This should always be used except there is a
 * good reason not to use it.
 */
class GuardedAllocator {
 public:
  void *allocate(size_t size, size_t alignment, const char *name)
  {
    /* Should we use MEM_mallocN, when alignment is small? If yes, how small must alignment be? */
    return MEM_mallocN_aligned(size, alignment, name);
  }

  void deallocate(void *ptr)
  {
    MEM_freeN(ptr);
  }
};

/**
 * This is a wrapper around malloc/free. Only use this when the GuardedAllocator cannot be
 * used. This can be the case when the allocated memory might live longer than Blender's
 * allocator. For example, when the memory is owned by a static variable.
 */
class RawAllocator {
 private:
  struct MemHead {
    int offset;
  };

 public:
  void *allocate(size_t size, size_t alignment, const char *UNUSED(name))
  {
    BLI_assert(is_power_of_2_i(static_cast<int>(alignment)));
    void *ptr = malloc(size + alignment + sizeof(MemHead));
    void *used_ptr = reinterpret_cast<void *>(
        reinterpret_cast<uintptr_t>(POINTER_OFFSET(ptr, alignment + sizeof(MemHead))) &
        ~(static_cast<uintptr_t>(alignment) - 1));
    int offset = static_cast<int>((intptr_t)used_ptr - (intptr_t)ptr);
    BLI_assert(offset >= static_cast<int>(sizeof(MemHead)));
    (static_cast<MemHead *>(used_ptr) - 1)->offset = offset;
    return used_ptr;
  }

  void deallocate(void *ptr)
  {
    MemHead *head = static_cast<MemHead *>(ptr) - 1;
    int offset = -head->offset;
    void *actual_pointer = POINTER_OFFSET(ptr, offset);
    free(actual_pointer);
  }
};

}  // namespace blender

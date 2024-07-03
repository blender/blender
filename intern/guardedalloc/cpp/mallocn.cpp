/* SPDX-FileCopyrightText: 2002-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 */

#include <cstddef>
#include <new>

#include "../intern/mallocn_intern_function_pointers.hh"

using namespace mem_guarded::internal;

void *operator new(size_t size, const char *str);
void *operator new[](size_t size, const char *str);

/* not default but can be used when needing to set a string */
void *operator new(size_t size, const char *str)
{
  return mem_mallocN_aligned_ex(size, 1, str, AllocationType::NEW_DELETE);
}
void *operator new[](size_t size, const char *str)
{
  return mem_mallocN_aligned_ex(size, 1, str, AllocationType::NEW_DELETE);
}

void *operator new(size_t size)
{
  return mem_mallocN_aligned_ex(size, 1, "C++/anonymous", AllocationType::NEW_DELETE);
}
void *operator new[](size_t size)
{
  return mem_mallocN_aligned_ex(size, 1, "C++/anonymous[]", AllocationType::NEW_DELETE);
}

void operator delete(void *p) throw()
{
  /* `delete nullptr` is valid in c++. */
  if (p) {
    mem_freeN_ex(p, AllocationType::NEW_DELETE);
  }
}
void operator delete[](void *p) throw()
{
  /* `delete nullptr` is valid in c++. */
  if (p) {
    mem_freeN_ex(p, AllocationType::NEW_DELETE);
  }
}

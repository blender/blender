/* SPDX-FileCopyrightText: 2002-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 */

#include "../MEM_guardedalloc.h"
#include <new>

void *operator new(size_t size, const char *str);
void *operator new[](size_t size, const char *str);

/* not default but can be used when needing to set a string */
void *operator new(size_t size, const char *str)
{
  return MEM_mallocN(size, str);
}
void *operator new[](size_t size, const char *str)
{
  return MEM_mallocN(size, str);
}

void *operator new(size_t size)
{
  return MEM_mallocN(size, "C++/anonymous");
}
void *operator new[](size_t size)
{
  return MEM_mallocN(size, "C++/anonymous[]");
}

void operator delete(void *p) throw()
{
  /* delete NULL is valid in c++ */
  if (p)
    MEM_freeN(p);
}
void operator delete[](void *p) throw()
{
  /* delete NULL is valid in c++ */
  if (p)
    MEM_freeN(p);
}

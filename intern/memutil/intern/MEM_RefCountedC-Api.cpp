/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 */

#include "MEM_RefCountedC-Api.h"
#include "MEM_RefCounted.h"

int MEM_RefCountedGetRef(MEM_TRefCountedObjectPtr shared)
{
  return shared ? ((MEM_RefCounted *)shared)->getRef() : 0;
}

int MEM_RefCountedIncRef(MEM_TRefCountedObjectPtr shared)
{
  return shared ? ((MEM_RefCounted *)shared)->incRef() : 0;
}

int MEM_RefCountedDecRef(MEM_TRefCountedObjectPtr shared)
{
  return shared ? ((MEM_RefCounted *)shared)->decRef() : 0;
}

/* SPDX-FileCopyrightText: 2009-2011 Jörg Hermann Müller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup audaspaceintern
 */

#include <set>

#include "AUD_Set.h"

void *AUD_createSet()
{
  return new std::set<void *>();
}

void AUD_destroySet(void *set)
{
  delete reinterpret_cast<std::set<void *> *>(set);
}

char AUD_removeSet(void *set, void *entry)
{
  if (set)
    return reinterpret_cast<std::set<void *> *>(set)->erase(entry);
  return 0;
}

void AUD_addSet(void *set, void *entry)
{
  if (entry)
    reinterpret_cast<std::set<void *> *>(set)->insert(entry);
}

void *AUD_getSet(void *set)
{
  if (set) {
    std::set<void *> *rset = reinterpret_cast<std::set<void *> *>(set);
    if (!rset->empty()) {
      std::set<void *>::iterator it = rset->begin();
      void *result = *it;
      rset->erase(it);
      return result;
    }
  }

  return (void *)0;
}

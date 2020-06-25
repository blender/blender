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

#ifndef __BLI_LISTBASE_WRAPPER_HH__
#define __BLI_LISTBASE_WRAPPER_HH__

/** \file
 * \ingroup bli
 *
 * `blender::ListBaseWrapper` is a typed wrapper for the #ListBase struct. That makes it safer and
 * more convenient to use in C++ in some cases. However, if you find yourself iterating over a
 * linked list a lot, consider to convert it into a vector for further processing. This improves
 * performance and debug-ability.
 */

#include "BLI_listbase.h"
#include "DNA_listBase.h"

namespace blender {

template<typename T> class ListBaseWrapper {
 private:
  ListBase *m_listbase;

 public:
  ListBaseWrapper(ListBase *listbase) : m_listbase(listbase)
  {
    BLI_assert(listbase);
  }

  ListBaseWrapper(ListBase &listbase) : ListBaseWrapper(&listbase)
  {
  }

  class Iterator {
   private:
    ListBase *m_listbase;
    T *m_current;

   public:
    Iterator(ListBase *listbase, T *current) : m_listbase(listbase), m_current(current)
    {
    }

    Iterator &operator++()
    {
      m_current = m_current->next;
      return *this;
    }

    Iterator operator++(int)
    {
      Iterator iterator = *this;
      ++*this;
      return iterator;
    }

    bool operator!=(const Iterator &iterator) const
    {
      return m_current != iterator.m_current;
    }

    T *operator*() const
    {
      return m_current;
    }
  };

  Iterator begin() const
  {
    return Iterator(m_listbase, (T *)m_listbase->first);
  }

  Iterator end() const
  {
    return Iterator(m_listbase, nullptr);
  }

  T get(uint index) const
  {
    void *ptr = BLI_findlink(m_listbase, index);
    BLI_assert(ptr);
    return (T *)ptr;
  }

  uint index_of(const T *value) const
  {
    uint index = 0;
    for (T *ptr : *this) {
      if (ptr == value) {
        return index;
      }
      index++;
    }
    BLI_assert(false);
    return 0;
  }
};

} /* namespace blender */

#endif /* __BLI_LISTBASE_WRAPPER_HH__ */

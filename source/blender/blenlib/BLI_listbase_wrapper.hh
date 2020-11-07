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
  ListBase *listbase_;

 public:
  ListBaseWrapper(ListBase *listbase) : listbase_(listbase)
  {
    BLI_assert(listbase);
  }

  ListBaseWrapper(ListBase &listbase) : ListBaseWrapper(&listbase)
  {
  }

  class Iterator {
   private:
    ListBase *listbase_;
    T *current_;

   public:
    Iterator(ListBase *listbase, T *current) : listbase_(listbase), current_(current)
    {
    }

    Iterator &operator++()
    {
      /* Some types store next/prev using `void *`, so cast is necessary. */
      current_ = static_cast<T *>(current_->next);
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
      return current_ != iterator.current_;
    }

    T *operator*() const
    {
      return current_;
    }
  };

  Iterator begin() const
  {
    return Iterator(listbase_, static_cast<T *>(listbase_->first));
  }

  Iterator end() const
  {
    return Iterator(listbase_, nullptr);
  }

  T get(uint index) const
  {
    void *ptr = BLI_findlink(listbase_, index);
    BLI_assert(ptr);
    return static_cast<T *>(ptr);
  }

  int64_t index_of(const T *value) const
  {
    int64_t index = 0;
    for (T *ptr : *this) {
      if (ptr == value) {
        return index;
      }
      index++;
    }
    BLI_assert(false);
    return -1;
  }
};

} /* namespace blender */

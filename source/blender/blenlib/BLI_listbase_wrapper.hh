/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

template<typename LB, typename T> class ListBaseWrapperTemplate {
 private:
  LB *listbase_;

 public:
  ListBaseWrapperTemplate(LB *listbase) : listbase_(listbase)
  {
    BLI_assert(listbase);
  }

  ListBaseWrapperTemplate(LB &listbase) : ListBaseWrapperTemplate(&listbase) {}

  class Iterator {
   private:
    LB *listbase_;
    T *current_;

   public:
    Iterator(LB *listbase, T *current) : listbase_(listbase), current_(current) {}

    Iterator &operator++()
    {
      /* Some types store `next/prev` using `void *`, so cast is necessary. */
      current_ = static_cast<T *>(current_->next);
      return *this;
    }

    Iterator operator++(int)
    {
      Iterator iterator = *this;
      ++(*this);
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

  T *get(uint index) const
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

template<typename T> using ListBaseWrapper = ListBaseWrapperTemplate<ListBase, T>;
template<typename T> using ConstListBaseWrapper = ListBaseWrapperTemplate<const ListBase, const T>;

} /* namespace blender */

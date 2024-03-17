/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A simple set class that's optimized for iteration.
 * Elements are stored in both a blender::Map and a flat array.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include <utility>

#include <cstdio>

namespace blender::bke::dyntopo {
template<typename T> class DyntopoSet {
 public:
  DyntopoSet(int64_t reserve)
  {
    elem_to_index_.reserve(reserve);
    index_to_elem_.reserve(reserve);
  }
  DyntopoSet() {}
  DyntopoSet(const DyntopoSet &) = delete;

  template<typename DS = DyntopoSet<T>, typename RT = T> struct iterator_base {
    iterator_base() : set_(nullptr), i_(-1) {}
    iterator_base(DS *set, int i) : set_(set), i_(i) {}
    iterator_base(const iterator_base &b) : set_(b.set_), i_(b.i_) {}

    iterator_base &operator=(const iterator_base &b)
    {
      set_ = b.set_;
      i_ = b.i_;

      return *this;
    }

    inline RT *operator*()
    {
      return set_->index_to_elem_[i_];
    }

    inline iterator_base &operator++()
    {
      i_++;

      while (i_ < set_->index_to_elem_.size() && set_->index_to_elem_[i_] == nullptr) {
        i_++;
      }

      return *this;
    }

    inline bool operator==(const iterator_base &b)
    {
      return b.i_ == i_;
    }

    inline bool operator!=(const iterator_base &b)
    {
      return b.i_ != i_;
    }

   private:
    DS *set_;
    int i_;
  };

  using iterator = iterator_base<DyntopoSet<T>, T>;
  using const_iterator = iterator_base<const DyntopoSet<T>, const T>;

  bool contains(T *key)
  {
    return elem_to_index_.contains(key);
  }

  void remove(T *key)
  {
    if (!elem_to_index_.contains(key)) {
      return;
    }

    int i = elem_to_index_.pop(key);
    index_to_elem_[i] = nullptr;
    freelist_.append(i);
  }

  /* Add key, returns true if key was already in set. */
  bool add(T *key)
  {
    int i;
    if (freelist_.size() > 0) {
      i = freelist_.last();
    }
    else {
      i = index_to_elem_.size();
    }

    bool was_added = elem_to_index_.add(key, i);
    if (was_added) {
      if (i == index_to_elem_.size()) {
        index_to_elem_.append(key);
      }
      else {
        freelist_.pop_last();
        index_to_elem_[i] = key;
      }
    }

    return was_added;
  }

  int size()
  {
    return elem_to_index_.size();
  }

  iterator begin()
  {
    int i = 0;
    while (i < index_to_elem_.size() && index_to_elem_[i] == nullptr) {
      i++;
    }

    return iterator(this, i);
  }

  iterator end()
  {
    return iterator(this, index_to_elem_.size());
  }

  const_iterator begin() const
  {
    int i = 0;
    while (i < index_to_elem_.size() && index_to_elem_[i] == nullptr) {
      i++;
    }

    return const_iterator(this, i);
  }

  const_iterator end() const
  {
    return const_iterator(this, index_to_elem_.size());
  }

 private:
  blender::Map<T *, int> elem_to_index_;
  blender::Vector<T *> index_to_elem_;
  blender::Vector<int> freelist_;
};
}  // namespace blender::bke::dyntopo

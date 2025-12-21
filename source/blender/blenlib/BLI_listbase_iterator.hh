/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <iterator>

/** An iterator for use with #ListBase.  */
template<typename T> struct ListBaseTIterator {
 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

 private:
  T *data_ = nullptr;

 public:
  ListBaseTIterator(T *data) : data_(data) {}

  ListBaseTIterator &operator++()
  {
    data_ = static_cast<T *>(data_->next);
    return *this;
  }

  ListBaseTIterator operator++(int)
  {
    ListBaseTIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  ListBaseTIterator &operator--()
  {
    data_ = static_cast<T *>(data_->prev);
    return *this;
  }

  ListBaseTIterator operator--(int)
  {
    ListBaseTIterator tmp = *this;
    --(*this);
    return tmp;
  }

  friend bool operator==(const ListBaseTIterator &a, const ListBaseTIterator &b)
  {
    return a.data_ == b.data_;
  }

  friend bool operator!=(const ListBaseTIterator &a, const ListBaseTIterator &b)
  {
    return a.data_ != b.data_;
  }

  T &operator*() const
  {
    return *data_;
  }
};

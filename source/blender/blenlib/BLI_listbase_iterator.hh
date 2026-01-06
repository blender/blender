/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_listBase.h"

#include <iterator>

namespace blender {

struct Link;

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
    data_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(data_))->next);
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
    data_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(data_))->prev);
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

/** An iterator for use with #ListBase that includes an index. */
template<typename T> struct ListBaseEnumerateIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::pair<int, T &>;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = value_type;

 private:
  T *data_ = nullptr;
  int index_ = 0;

 public:
  ListBaseEnumerateIterator(T *data, int index) : data_(data), index_(index) {}

  ListBaseEnumerateIterator &operator++()
  {
    data_ = reinterpret_cast<T *>(reinterpret_cast<const Link *>(data_)->next);
    index_++;
    return *this;
  }

  ListBaseEnumerateIterator operator++(int)
  {
    ListBaseEnumerateIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  friend bool operator==(const ListBaseEnumerateIterator &a, const ListBaseEnumerateIterator &b)
  {
    return a.data_ == b.data_;
  }

  friend bool operator!=(const ListBaseEnumerateIterator &a, const ListBaseEnumerateIterator &b)
  {
    return a.data_ != b.data_;
  }

  value_type operator*() const
  {
    return {index_, *data_};
  }
};

template<typename T> struct ListBaseEnumerateWrapper {
  void *first;

  ListBaseEnumerateIterator<T> begin() const
  {
    return ListBaseEnumerateIterator<T>(static_cast<T *>(first), 0);
  }

  ListBaseEnumerateIterator<T> end() const
  {
    return ListBaseEnumerateIterator<T>(nullptr, 0);
  }
};

/** An iterator for use with #ListBase that caches the next pointer to allow removal. */
template<typename T> struct ListBaseMutableIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

 private:
  T *current_ = nullptr;
  T *next_ = nullptr;

 public:
  ListBaseMutableIterator(T *data) : current_(data)
  {
    if (current_) {
      next_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(current_))->next);
    }
  }

  ListBaseMutableIterator &operator++()
  {
    current_ = next_;
    if (current_) {
      next_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(current_))->next);
    }
    return *this;
  }

  ListBaseMutableIterator operator++(int)
  {
    ListBaseMutableIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  friend bool operator==(const ListBaseMutableIterator &a, const ListBaseMutableIterator &b)
  {
    return a.current_ == b.current_;
  }

  friend bool operator!=(const ListBaseMutableIterator &a, const ListBaseMutableIterator &b)
  {
    return a.current_ != b.current_;
  }

  T &operator*() const
  {
    return *current_;
  }
};

template<typename T> struct ListBaseMutableWrapper {
  void *first;

  ListBaseMutableIterator<T> begin() const
  {
    return ListBaseMutableIterator<T>(static_cast<T *>(first));
  }

  ListBaseMutableIterator<T> end() const
  {
    return ListBaseMutableIterator<T>(nullptr);
  }
};

/** An iterator for use with #ListBase iterating backwards. */
template<typename T> struct ListBaseBackwardIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

 private:
  T *data_ = nullptr;

 public:
  ListBaseBackwardIterator(T *data) : data_(data) {}

  ListBaseBackwardIterator &operator++()
  {
    data_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(data_))->prev);
    return *this;
  }

  ListBaseBackwardIterator operator++(int)
  {
    ListBaseBackwardIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  friend bool operator==(const ListBaseBackwardIterator &a, const ListBaseBackwardIterator &b)
  {
    return a.data_ == b.data_;
  }

  friend bool operator!=(const ListBaseBackwardIterator &a, const ListBaseBackwardIterator &b)
  {
    return a.data_ != b.data_;
  }

  T &operator*() const
  {
    return *data_;
  }
};

template<typename T> struct ListBaseBackwardWrapper {
  void *last;

  ListBaseBackwardIterator<T> begin() const
  {
    return ListBaseBackwardIterator<T>(static_cast<T *>(last));
  }

  ListBaseBackwardIterator<T> end() const
  {
    return ListBaseBackwardIterator<T>(nullptr);
  }
};

/** An iterator for use with #ListBase iterating backwards and allowing removal. */
template<typename T> struct ListBaseMutableBackwardIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

 private:
  T *current_ = nullptr;
  T *prev_ = nullptr;

 public:
  ListBaseMutableBackwardIterator(T *data) : current_(data)
  {
    if (current_) {
      prev_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(current_))->prev);
    }
  }

  ListBaseMutableBackwardIterator &operator++()
  {
    current_ = prev_;
    if (current_) {
      prev_ = reinterpret_cast<T *>((reinterpret_cast<const Link *>(current_))->prev);
    }
    return *this;
  }

  ListBaseMutableBackwardIterator operator++(int)
  {
    ListBaseMutableBackwardIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  friend bool operator==(const ListBaseMutableBackwardIterator &a,
                         const ListBaseMutableBackwardIterator &b)
  {
    return a.current_ == b.current_;
  }

  friend bool operator!=(const ListBaseMutableBackwardIterator &a,
                         const ListBaseMutableBackwardIterator &b)
  {
    return a.current_ != b.current_;
  }

  T &operator*() const
  {
    return *current_;
  }
};

template<typename T> struct ListBaseMutableBackwardWrapper {
  void *last;

  ListBaseMutableBackwardIterator<T> begin() const
  {
    return ListBaseMutableBackwardIterator<T>(static_cast<T *>(last));
  }

  ListBaseMutableBackwardIterator<T> end() const
  {
    return ListBaseMutableBackwardIterator<T>(nullptr);
  }
};

}  // namespace blender

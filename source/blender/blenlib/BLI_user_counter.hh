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
 */

#include <atomic>

namespace blender {

/**
 * A simple automatic reference counter. It is similar to std::shared_ptr, but expects that the
 * reference count is inside the object.
 */
template<typename T> class UserCounter {
 private:
  T *data_ = nullptr;

 public:
  UserCounter() = default;

  UserCounter(T *data) : data_(data)
  {
  }

  UserCounter(const UserCounter &other) : data_(other.data_)
  {
    this->user_add(data_);
  }

  UserCounter(UserCounter &&other) : data_(other.data_)
  {
    other.data_ = nullptr;
  }

  ~UserCounter()
  {
    this->user_remove(data_);
  }

  UserCounter &operator=(const UserCounter &other)
  {
    if (this == &other) {
      return *this;
    }

    this->user_remove(data_);
    data_ = other.data_;
    this->user_add(data_);
    return *this;
  }

  UserCounter &operator=(UserCounter &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->user_remove(data_);
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
  }

  T *operator->()
  {
    BLI_assert(data_ != nullptr);
    return data_;
  }

  const T *operator->() const
  {
    BLI_assert(data_ != nullptr);
    return data_;
  }

  T &operator*()
  {
    BLI_assert(data_ != nullptr);
    return *data_;
  }

  const T &operator*() const
  {
    BLI_assert(data_ != nullptr);
    return *data_;
  }

  operator bool() const
  {
    return data_ != nullptr;
  }

  T *get()
  {
    return data_;
  }

  const T *get() const
  {
    return data_;
  }

  T *release()
  {
    T *data = data_;
    data_ = nullptr;
    return data;
  }

  void reset()
  {
    this->user_remove(data_);
    data_ = nullptr;
  }

  bool has_value() const
  {
    return data_ != nullptr;
  }

  uint64_t hash() const
  {
    return get_default_hash(data_);
  }

  friend bool operator==(const UserCounter &a, const UserCounter &b)
  {
    return a.data_ == b.data_;
  }

  friend std::ostream &operator<<(std::ostream &stream, const UserCounter &value)
  {
    stream << value.data_;
    return stream;
  }

 private:
  static void user_add(T *data)
  {
    if (data != nullptr) {
      data->user_add();
    }
  }

  static void user_remove(T *data)
  {
    if (data != nullptr) {
      data->user_remove();
    }
  }
};

}  // namespace blender

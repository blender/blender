/* SPDX-FileCopyrightText: 2026 Clement Foucault
 *
 * SPDX-License-Identifier: MIT */

#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#ifndef NDEBUG
#  include <span>
#endif

namespace lexit {

template<typename T> class AlignedArrayPtr {
 private:
  T *ptr = nullptr;

 public:
  AlignedArrayPtr() = default;

  AlignedArrayPtr(int size)
  {
    ptr = static_cast<T *>(operator new[](sizeof(T) * size, std::align_val_t{64}));
  }

  AlignedArrayPtr(const AlignedArrayPtr &other) = delete;

  AlignedArrayPtr(AlignedArrayPtr &&other) : ptr(other.ptr)
  {
    other.ptr = nullptr;
  }

  ~AlignedArrayPtr()
  {
    operator delete[](ptr, std::align_val_t{64});
  }

  AlignedArrayPtr &operator=(AlignedArrayPtr &&other)
  {
    if (this != &other) {
      operator delete[](ptr, std::align_val_t{64});
      ptr = other.ptr;
      other.ptr = nullptr;
    }
    return *this;
  }

  T *get()
  {
    return ptr;
  }
  const T *get() const
  {
    return ptr;
  }
  T &operator[](int i)
  {
    return ptr[i];
  }
  const T &operator[](int i) const
  {
    return ptr[i];
  }
};

template<typename T> struct Vector {
 private:
  std::unique_ptr<T[]> data_;
  int size_ = 0;
  int alloc_size_ = 0;
#ifndef NDEBUG
  std::span<T> debug_view_;
#endif

 public:
  void reserve(int new_size)
  {
    if ((alloc_size_ >= new_size) && data_) {
      return;
    }
    std::unique_ptr<T[]> new_ptr(new T[new_size]);
    if (data_) {
      std::memcpy(new_ptr.get(), data_.get(), alloc_size_ * sizeof(T));
    }
    data_ = std::move(new_ptr);
    alloc_size_ = new_size;
#ifndef NDEBUG
    debug_view_ = std::span<T>(data_.get(), size_);
#endif
  }

  void resize(int new_size)
  {
    reserve(new_size);
    size_ = new_size;
#ifndef NDEBUG
    debug_view_ = std::span<T>(data_.get(), size_);
#endif
  }

  T *data()
  {
    return data_.get();
  }
  const T *data() const
  {
    return data_.get();
  }

  T &operator[](int i)
  {
    return data_.get()[i];
  }
  const T &operator[](int i) const
  {
    return data_.get()[i];
  }

  T *end()
  {
    return data_.get() + size_;
  }
  const T *end() const
  {
    return data_.get() + size_;
  }

  int size() const
  {
    return size_;
  }

  void increase_size_by_unchecked(int elem_count)
  {
    assert(size_ + elem_count <= alloc_size_);
    size_ += elem_count;
#ifndef NDEBUG
    debug_view_ = std::span<T>(data_.get(), size_);
#endif
  }
};

}  // namespace lexit

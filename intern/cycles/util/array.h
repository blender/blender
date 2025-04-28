/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cassert>
#include <cstring>

#include "util/aligned_malloc.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Simplified version of vector, serving multiple purposes:
 * - somewhat faster in that it does not clear memory on resize/alloc,
 *   this was actually showing up in profiles quite significantly. it
 *   also does not run any constructors/destructors
 * - if this is used, we are not tempted to use inefficient operations
 * - aligned allocation for CPU native data types */

template<typename T, const size_t alignment = MIN_ALIGNMENT_CPU_DATA_TYPES> class array {
 public:
  array() : data_(nullptr), datasize_(0), capacity_(0) {}

  explicit array(const size_t newsize)
  {
    if (newsize == 0) {
      data_ = nullptr;
      datasize_ = 0;
      capacity_ = 0;
    }
    else {
      data_ = mem_allocate(newsize);
      datasize_ = newsize;
      capacity_ = datasize_;
    }
  }

  array(const array &from)
  {
    if (from.datasize_ == 0) {
      data_ = nullptr;
      datasize_ = 0;
      capacity_ = 0;
    }
    else {
      data_ = mem_allocate(from.datasize_);
      if (from.datasize_ > 0) {
        mem_copy(data_, from.data_, from.datasize_);
      }
      datasize_ = from.datasize_;
      capacity_ = datasize_;
    }
  }

  array(array &&from)
  {
    data_ = from.data_;
    datasize_ = from.datasize_;
    capacity_ = from.capacity_;

    from.data_ = nullptr;
    from.datasize_ = 0;
    from.capacity_ = 0;
  }

  array &operator=(const array &from)
  {
    if (this != &from) {
      resize(from.size());
      if (datasize_ > 0) {
        mem_copy(data_, from.data_, datasize_);
      }
    }

    return *this;
  }

  array &operator=(const vector<T> &from)
  {
    resize(from.size());

    if (from.size() > 0 && datasize_ > 0) {
      mem_copy(data_, from.data(), datasize_);
    }

    return *this;
  }

  ~array()
  {
    mem_free(data_, capacity_);
  }

  bool operator==(const array<T> &other) const
  {
    if (datasize_ != other.datasize_) {
      return false;
    }
    if (datasize_ == 0) {
      return true;
    }

    return memcmp(data_, other.data_, datasize_ * sizeof(T)) == 0;
  }

  bool operator!=(const array<T> &other) const
  {
    return !(*this == other);
  }

  void steal_data(array &from)
  {
    if (this != &from) {
      clear();

      data_ = from.data_;
      datasize_ = from.datasize_;
      capacity_ = from.capacity_;

      from.data_ = nullptr;
      from.datasize_ = 0;
      from.capacity_ = 0;
    }
  }

  void set_data(T *ptr_, size_t datasize)
  {
    clear();
    data_ = ptr_;
    datasize_ = datasize;
    capacity_ = datasize;
  }

  T *steal_pointer()
  {
    T *ptr = data_;
    data_ = nullptr;
    clear();
    return ptr;
  }

  T *resize(const size_t newsize)
  {
    if (newsize == 0) {
      clear();
    }
    else if (newsize != datasize_) {
      if (newsize > capacity_) {
        T *newdata = mem_allocate(newsize);
        if (newdata == nullptr) {
          /* Allocation failed, likely out of memory. */
          clear();
          return nullptr;
        }
        if (data_ != nullptr) {
          mem_copy(newdata, data_, ((datasize_ < newsize) ? datasize_ : newsize));
          mem_free(data_, capacity_);
        }
        data_ = newdata;
        capacity_ = newsize;
      }
      datasize_ = newsize;
    }
    return data_;
  }

  T *resize(const size_t newsize, const T &value)
  {
    size_t oldsize = size();
    resize(newsize);

    for (size_t i = oldsize; i < size(); i++) {
      data_[i] = value;
    }

    return data_;
  }

  void clear()
  {
    if (data_ != nullptr) {
      mem_free(data_, capacity_);
      data_ = nullptr;
    }
    datasize_ = 0;
    capacity_ = 0;
  }

  size_t empty() const
  {
    return datasize_ == 0;
  }

  size_t size() const
  {
    return datasize_;
  }

  T *data()
  {
    return data_;
  }

  const T *data() const
  {
    return data_;
  }

  T &operator[](size_t i) const
  {
    assert(i < datasize_);
    return data_[i];
  }

  T *begin()
  {
    return data_;
  }

  const T *begin() const
  {
    return data_;
  }

  T *end()
  {
    return data_ + datasize_;
  }

  const T *end() const
  {
    return data_ + datasize_;
  }

  void reserve(const size_t newcapacity)
  {
    if (newcapacity > capacity_) {
      T *newdata = mem_allocate(newcapacity);
      if (data_ != nullptr) {
        mem_copy(newdata, data_, ((datasize_ < newcapacity) ? datasize_ : newcapacity));
        mem_free(data_, capacity_);
      }
      data_ = newdata;
      capacity_ = newcapacity;
    }
  }

  size_t capacity() const
  {
    return capacity_;
  }

  // do not use this method unless you are sure the code is not performance critical
  void push_back_slow(const T &t)
  {
    if (capacity_ == datasize_) {
      reserve(datasize_ == 0 ? 1 : (size_t)((datasize_ + 1) * 1.2));
    }

    data_[datasize_++] = t;
  }

  void push_back_reserved(const T &t)
  {
    assert(datasize_ < capacity_);
    push_back_slow(t);
  }

  void append(const array<T> &from)
  {
    if (from.size()) {
      size_t old_size = size();
      resize(old_size + from.size());
      mem_copy(data_ + old_size, from.data(), from.size());
    }
  }

 protected:
  T *mem_allocate(const size_t N)
  {
    if (N == 0) {
      return nullptr;
    }
    T *mem = (T *)util_aligned_malloc(sizeof(T) * N, alignment);
    if (mem == nullptr) {
      throw std::bad_alloc();
    }
    return mem;
  }

  void mem_free(T *mem, const size_t N)
  {
    if (mem != nullptr) {
      util_aligned_free(mem, sizeof(T) * N);
    }
  }

  void mem_copy(T *mem_to, const T *mem_from, const size_t N)
  {
    memcpy((void *)mem_to, mem_from, sizeof(T) * N);
  }

  T *data_;
  size_t datasize_;
  size_t capacity_;
};

CCL_NAMESPACE_END

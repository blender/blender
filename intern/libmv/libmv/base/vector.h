// Copyright (c) 2007, 2008 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Get an aligned vector implementation. Must be included before <vector>. The
// Eigen guys went through some trouble to make a portable override for the
// fixed size vector types.

#ifndef LIBMV_BASE_VECTOR_H
#define LIBMV_BASE_VECTOR_H

#include <cstring>
#include <new>

#include <Eigen/Core>

namespace libmv {

// A simple container class, which guarantees 16 byte alignment needed for most
// vectorization. Don't use this container for classes that cannot be copied
// via memcpy.
// FIXME: this class has some issues:
// - doesn't support iterators.
// - impede compatibility with code using STL.
// - the STL already provide support for custom allocators
// it could be replaced with a simple
// template <T> class vector : std::vector<T, aligned_allocator> {} declaration
// provided it doesn't break code relying on libmv::vector specific behavior
template <typename T,
          typename Allocator = Eigen::aligned_allocator<T> >
class vector {
 public:
  ~vector()                        { clear();                 }

  vector()                         { init();                  }
  vector(int size)                 { init(); resize(size);    }
  vector(int size, const T & val)  {
    init();
    resize(size);
    std::fill(data_, data_+size_, val); }

  // Copy constructor and assignment.
  vector(const vector<T, Allocator> &rhs) {
    init();
    copy(rhs);
  }
  vector<T, Allocator> &operator=(const vector<T, Allocator> &rhs) {
    if (&rhs != this) {
      copy(rhs);
    }
    return *this;
  }

  /// Swaps the contents of two vectors in constant time.
  void swap(vector<T, Allocator> &other) {
    std::swap(allocator_, other.allocator_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
    std::swap(data_, other.data_);
  }

        T *data()            const { return data_;            }
  int      size()            const { return size_;            }
  int      capacity()        const { return capacity_;        }
  const T& back()            const { return data_[size_ - 1]; }
        T& back()                  { return data_[size_ - 1]; }
  const T& front()           const { return data_[0];         }
        T& front()                 { return data_[0];         }
  const T& operator[](int n) const { return data_[n];         }
        T& operator[](int n)       { return data_[n];         }
  const T& at(int n)         const { return data_[n];         }
        T& at(int n)               { return data_[n];         }
  const T * begin()          const { return data_;            }
  const T * end()            const { return data_+size_;      }
        T * begin()                { return data_;            }
        T * end()                  { return data_+size_;      }

  void resize(size_t size) {
    reserve(size);
    if (size > size_) {
      construct(size_, size);
    } else if (size < size_) {
      destruct(size, size_);
    }
    size_ = size;
  }

  void push_back(const T &value) {
    if (size_ == capacity_) {
      reserve(size_ ? 2 * size_ : 1);
    }
    new (&data_[size_++]) T(value);
  }

  void pop_back() {
    resize(size_ - 1);
  }

  void clear() {
    destruct(0, size_);
    deallocate();
    init();
  }

  void reserve(unsigned int size) {
    if (size > size_) {
      T *data = static_cast<T *>(allocate(size));
      memcpy(data, data_, sizeof(*data)*size_);
      allocator_.deallocate(data_, capacity_);
      data_ = data;
      capacity_ = size;
    }
  }

  bool empty() {
    return size_ == 0;
  }

 private:
  void construct(int start, int end) {
    for (int i = start; i < end; ++i) {
      new (&data_[i]) T;
    }
  }
  void destruct(int start, int end) {
    for (int i = start; i < end; ++i) {
      data_[i].~T();
    }
  }
  void init() {
    size_ = 0;
    data_ = 0;
    capacity_ = 0;
  }

  void *allocate(int size) {
    return size ? allocator_.allocate(size) : 0;
  }

  void deallocate() {
    allocator_.deallocate(data_, size_);
    data_ = 0;
  }

  void copy(const vector<T, Allocator> &rhs) {
    resize(rhs.size());
    for (int i = 0; i < rhs.size(); ++i) {
      (*this)[i] = rhs[i];
    }
  }

  Allocator allocator_;
  size_t size_;
  size_t capacity_;
  T *data_;
};

}  // namespace libmv

#endif  // LIBMV_BASE_VECTOR_H

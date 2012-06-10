// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: rennie@google.com (Jeffrey Rennie)
// Author: sanjay@google.com (Sanjay Ghemawat) -- renamed to FixedArray

#ifndef CERES_PUBLIC_INTERNAL_FIXED_ARRAY_H_
#define CERES_PUBLIC_INTERNAL_FIXED_ARRAY_H_

#include <cstddef>
#include <glog/logging.h>
#include "ceres/internal/manual_constructor.h"

namespace ceres {
namespace internal {

// A FixedArray<T> represents a non-resizable array of T where the
// length of the array does not need to be a compile time constant.
//
// FixedArray allocates small arrays inline, and large arrays on
// the heap.  It is a good replacement for non-standard and deprecated
// uses of alloca() and variable length arrays (a GCC extension).
//
// FixedArray keeps performance fast for small arrays, because it
// avoids heap operations.  It also helps reduce the chances of
// accidentally overflowing your stack if large input is passed to
// your function.
//
// Also, FixedArray is useful for writing portable code.  Not all
// compilers support arrays of dynamic size.

// Most users should not specify an inline_elements argument and let
// FixedArray<> automatically determine the number of elements
// to store inline based on sizeof(T).
//
// If inline_elements is specified, the FixedArray<> implementation
// will store arrays of length <= inline_elements inline.
//
// Finally note that unlike vector<T> FixedArray<T> will not zero-initialize
// simple types like int, double, bool, etc.
//
// Non-POD types will be default-initialized just like regular vectors or
// arrays.

#if defined(_WIN64)
   typedef __int64      ssize_t;
#elif defined(_WIN32)
   typedef __int32      ssize_t;
#endif

template <typename T, ssize_t inline_elements = -1>
class FixedArray {
 public:
  // For playing nicely with stl:
  typedef T value_type;
  typedef T* iterator;
  typedef T const* const_iterator;
  typedef T& reference;
  typedef T const& const_reference;
  typedef T* pointer;
  typedef std::ptrdiff_t difference_type;
  typedef size_t size_type;

  // REQUIRES: n >= 0
  // Creates an array object that can store "n" elements.
  //
  // FixedArray<T> will not zero-initialiaze POD (simple) types like int,
  // double, bool, etc.
  // Non-POD types will be default-initialized just like regular vectors or
  // arrays.
  explicit FixedArray(size_type n);

  // Releases any resources.
  ~FixedArray();

  // Returns the length of the array.
  inline size_type size() const { return size_; }

  // Returns the memory size of the array in bytes.
  inline size_t memsize() const { return size_ * sizeof(T); }

  // Returns a pointer to the underlying element array.
  inline const T* get() const { return &array_[0].element; }
  inline T* get() { return &array_[0].element; }

  // REQUIRES: 0 <= i < size()
  // Returns a reference to the "i"th element.
  inline T& operator[](size_type i) {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, size_);
    return array_[i].element;
  }

  // REQUIRES: 0 <= i < size()
  // Returns a reference to the "i"th element.
  inline const T& operator[](size_type i) const {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, size_);
    return array_[i].element;
  }

  inline iterator begin() { return &array_[0].element; }
  inline iterator end() { return &array_[size_].element; }

  inline const_iterator begin() const { return &array_[0].element; }
  inline const_iterator end() const { return &array_[size_].element; }

 private:
  // Container to hold elements of type T.  This is necessary to handle
  // the case where T is a a (C-style) array.  The size of InnerContainer
  // and T must be the same, otherwise callers' assumptions about use
  // of this code will be broken.
  struct InnerContainer {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    T element;
  };

  // How many elements should we store inline?
  //   a. If not specified, use a default of 256 bytes (256 bytes
  //      seems small enough to not cause stack overflow or unnecessary
  //      stack pollution, while still allowing stack allocation for
  //      reasonably long character arrays.
  //   b. Never use 0 length arrays (not ISO C++)
  static const size_type S1 = ((inline_elements < 0)
                               ? (256/sizeof(T)) : inline_elements);
  static const size_type S2 = (S1 <= 0) ? 1 : S1;
  static const size_type kInlineElements = S2;

  size_type const       size_;
  InnerContainer* const array_;

  // Allocate some space, not an array of elements of type T, so that we can
  // skip calling the T constructors and destructors for space we never use.
  ManualConstructor<InnerContainer> inline_space_[kInlineElements];
};

// Implementation details follow

template <class T, ssize_t S>
inline FixedArray<T, S>::FixedArray(typename FixedArray<T, S>::size_type n)
    : size_(n),
      array_((n <= kInlineElements
              ? reinterpret_cast<InnerContainer*>(inline_space_)
              : new InnerContainer[n])) {
  DCHECK_GE(n, 0);

  // Construct only the elements actually used.
  if (array_ == reinterpret_cast<InnerContainer*>(inline_space_)) {
    for (int i = 0; i != size_; ++i) {
      inline_space_[i].Init();
    }
  }
}

template <class T, ssize_t S>
inline FixedArray<T, S>::~FixedArray() {
  if (array_ != reinterpret_cast<InnerContainer*>(inline_space_)) {
    delete[] array_;
  } else {
    for (int i = 0; i != size_; ++i) {
      inline_space_[i].Destroy();
    }
  }
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_FIXED_ARRAY_H_

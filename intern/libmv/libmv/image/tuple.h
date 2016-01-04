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

#ifndef LIBMV_IMAGE_TUPLE_H
#define LIBMV_IMAGE_TUPLE_H

#include <algorithm>

namespace libmv {

// A vector of elements with fixed lenght and deep copy semantics.
template <typename T, int N>
class Tuple {
 public:
  enum { SIZE = N };
  Tuple() {}
  Tuple(T initial_value) { Reset(initial_value); }

  template <typename D>
  Tuple(D *values) { Reset(values); }

  template <typename D>
  Tuple(const Tuple<D, N> &b) { Reset(b); }

  template <typename D>
  Tuple& operator=(const Tuple<D, N>& b) {
    Reset(b);
    return *this;
  }

  template <typename D>
  void Reset(const Tuple<D, N>& b) { Reset(b.Data()); }

  template <typename D>
  void Reset(D *values) {
    for (int i = 0;i < N; i++) {
      data_[i] = T(values[i]);
    }
  }

  // Set all tuple values to the same thing.
  void Reset(T value) {
    for (int i = 0;i < N; i++) {
      data_[i] = value;
    }
  }

  // Pointer to the first element.
  T *Data() { return &data_[0]; }
  const T *Data() const { return &data_[0]; }

  T &operator()(int i) { return data_[i]; }
  const T &operator()(int i) const { return data_[i]; }

  bool operator==(const Tuple<T, N> &other) const {
    for (int i = 0; i < N; ++i) {
      if ((*this)(i) != other(i)) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const Tuple<T, N> &other) const {
    return !(*this == other);
  }

 private:
  T data_[N];
};

}  // namespace libmv

#endif  // LIBMV_IMAGE_TUPLE_H

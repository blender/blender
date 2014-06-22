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

#ifndef LIBMV_IMAGE_ARRAY_ND_H
#define LIBMV_IMAGE_ARRAY_ND_H

#include <cassert>
#include <cstdio>
#include <cstring>

#include "libmv/image/tuple.h"

namespace libmv {

class BaseArray {};

/// A multidimensional array class.
template <typename T, int N>
class ArrayND : public BaseArray {
 public:
  typedef T Scalar;

  /// Type for the multidimensional indices.
  typedef Tuple<int, N> Index;

  /// Create an empty array.
  ArrayND() : data_(NULL), own_data_(true) { Resize(Index(0)); }

  /// Create an array with the specified shape.
  ArrayND(const Index &shape) : data_(NULL), own_data_(true) { Resize(shape); }

  /// Create an array with the specified shape.
  ArrayND(int *shape) : data_(NULL), own_data_(true) { Resize(shape); }

  /// Copy constructor.
  ArrayND(const ArrayND<T, N> &b) : data_(NULL), own_data_(true) {
    ResizeLike(b);
    std::memcpy(Data(), b.Data(), sizeof(T) * Size());
  }

  ArrayND(int s0) : data_(NULL), own_data_(true) { Resize(s0); }
  ArrayND(int s0, int s1) : data_(NULL), own_data_(true) { Resize(s0, s1); }
  ArrayND(int s0, int s1, int s2) : data_(NULL), own_data_(true) {
     Resize(s0, s1, s2);
  }

  ArrayND(T* data, int s0, int s1, int s2) : data_(data), own_data_(false) {
    Resize(s0, s1, s2);
  }

  /// Destructor deletes pixel data.
  ~ArrayND() {
    if (own_data_) {
      delete [] data_;
    }
  }

  /// Assignation copies pixel data.
  ArrayND &operator=(const ArrayND<T, N> &b) {
    assert(this != &b);
    ResizeLike(b);
    std::memcpy(Data(), b.Data(), sizeof(T) * Size());
    return *this;
  }

  const Index &Shapes() const {
    return shape_;
  }

  const Index &Strides() const {
    return strides_;
  }

  /// Create an array of shape s.
  void Resize(const Index &new_shape) {
    if (data_ != NULL && shape_ == new_shape) {
      // Don't bother realloacting if the shapes match.
      return;
    }
    shape_.Reset(new_shape);
    strides_(N - 1) = 1;
    for (int i = N - 1; i > 0; --i) {
      strides_(i - 1) = strides_(i) * shape_(i);
    }
    if (own_data_) {
      delete [] data_;
      data_ = NULL;
      if (Size() > 0) {
        data_ = new T[Size()];
      }
    }
  }

  template<typename D>
  void ResizeLike(const ArrayND<D, N> &other) {
    Resize(other.Shape());
  }

  /// Resizes the array to shape s.  All data is lost.
  void Resize(const int *new_shape_array) {
    Resize(Index(new_shape_array));
  }

  /// Resize a 1D array to length s0.
  void Resize(int s0) {
    assert(N == 1);
    int shape[] = {s0};
    Resize(shape);
  }

  /// Resize a 2D array to shape (s0,s1).
  void Resize(int s0, int s1) {
    int shape[N] = {s0, s1};
    for (int i = 2; i < N; ++i) {
      shape[i] = 1;
    }
    Resize(shape);
  }

  // Match Eigen2's API.
  void resize(int rows, int cols) {
    Resize(rows, cols);
  }

  /// Resize a 3D array to shape (s0,s1,s2).
  void Resize(int s0, int s1, int s2) {
    assert(N == 3);
    int shape[] = {s0, s1, s2};
    Resize(shape);
  }

  template<typename D>
  void CopyFrom(const ArrayND<D, N> &other) {
    ResizeLike(other);
    T *data = Data();
    const D *other_data = other.Data();
    for (int i = 0; i < Size(); ++i) {
      data[i] = T(other_data[i]);
    }
  }

  void Fill(T value) {
    for (int i = 0; i < Size(); ++i) {
      Data()[i] = value;
    }
  }

  // Match Eigen's API.
  void fill(T value) {
    for (int i = 0; i < Size(); ++i) {
      Data()[i] = value;
    }
  }

  /// Return a tuple containing the length of each axis.
  const Index &Shape() const {
    return shape_;
  }

  /// Return the length of an axis.
  int Shape(int axis) const {
    return shape_(axis);
  }

  /// Return the distance between neighboring elements along axis.
  int Stride(int axis) const {
    return strides_(axis);
  }

  /// Return the number of elements of the array.
  int Size() const {
    int size = 1;
    for (int i = 0; i < N; ++i)
      size *= Shape(i);
    return size;
  }

  /// Return the total amount of memory used by the array.
  int MemorySizeInBytes() const {
    return sizeof(*this) + Size() * sizeof(T);
  }

  /// Pointer to the first element of the array.
  T *Data() { return data_; }

  /// Constant pointer to the first element of the array.
  const T *Data() const { return data_; }

  /// Distance between the first element and the element at position index.
  int Offset(const Index &index) const {
    int offset = 0;
    for (int i = 0; i < N; ++i)
      offset += index(i) * Stride(i);
    return offset;
  }

  /// 1D specialization.
  int Offset(int i0) const {
    assert(N == 1);
    return i0 * Stride(0);
  }

  /// 2D specialization.
  int Offset(int i0, int i1) const {
    assert(N == 2);
    return i0 * Stride(0) + i1 * Stride(1);
  }

  /// 3D specialization.
  int Offset(int i0, int i1, int i2) const {
    assert(N == 3);
    return i0 * Stride(0) + i1 * Stride(1) + i2 * Stride(2);
  }

  /// Return a reference to the element at position index.
  T &operator()(const Index &index) {
    // TODO(pau) Boundary checking in debug mode.
    return *( Data() + Offset(index) );
  }

  /// 1D specialization.
  T &operator()(int i0) {
    return *( Data() + Offset(i0) );
  }

  /// 2D specialization.
  T &operator()(int i0, int i1) {
    assert(0 <= i0 && i0 < Shape(0));
    assert(0 <= i1 && i1 < Shape(1));
    return *(Data() + Offset(i0, i1));
  }

  /// 3D specialization.
  T &operator()(int i0, int i1, int i2) {
    assert(0 <= i0 && i0 < Shape(0));
    assert(0 <= i1 && i1 < Shape(1));
    assert(0 <= i2 && i2 < Shape(2));
    return *(Data() + Offset(i0, i1, i2));
  }

  /// Return a constant reference to the element at position index.
  const T &operator()(const Index &index) const {
    return *(Data() + Offset(index));
  }

  /// 1D specialization.
  const T &operator()(int i0) const {
    return *(Data() + Offset(i0));
  }

  /// 2D specialization.
  const T &operator()(int i0, int i1) const {
    assert(0 <= i0 && i0 < Shape(0));
    assert(0 <= i1 && i1 < Shape(1));
    return *(Data() + Offset(i0, i1));
  }

  /// 3D specialization.
  const T &operator()(int i0, int i1, int i2) const {
    return *(Data() + Offset(i0, i1, i2));
  }

  /// True if index is inside array.
  bool Contains(const Index &index) const {
    for (int i = 0; i < N; ++i)
      if (index(i) < 0 || index(i) >= Shape(i))
        return false;
    return true;
  }

  /// 1D specialization.
  bool Contains(int i0) const {
    return 0 <= i0 && i0 < Shape(0);
  }

  /// 2D specialization.
  bool Contains(int i0, int i1) const {
    return 0 <= i0 && i0 < Shape(0)
        && 0 <= i1 && i1 < Shape(1);
  }

  /// 3D specialization.
  bool Contains(int i0, int i1, int i2) const {
    return 0 <= i0 && i0 < Shape(0)
        && 0 <= i1 && i1 < Shape(1)
        && 0 <= i2 && i2 < Shape(2);
  }

  bool operator==(const ArrayND<T, N> &other) const {
    if (shape_ != other.shape_) return false;
    if (strides_ != other.strides_) return false;
    for (int i = 0; i < Size(); ++i) {
      if (this->Data()[i] != other.Data()[i])
        return false;
    }
    return true;
  }

  bool operator!=(const ArrayND<T, N> &other) const {
    return !(*this == other);
  }

  ArrayND<T, N> operator*(const ArrayND<T, N> &other) const {
    assert(Shape() = other.Shape());
    ArrayND<T, N> res;
    res.ResizeLike(*this);
    for (int i = 0; i < res.Size(); ++i) {
      res.Data()[i] = Data()[i] * other.Data()[i];
    }
    return res;
  }

 protected:
  /// The number of element in each dimension.
  Index shape_;

  /// How to jump to neighbors in each dimension.
  Index strides_;

  /// Pointer to the first element of the array.
  T *data_;

  /// Flag if this Array either own or reference the data
  bool own_data_;
};

/// 3D array (row, column, channel).
template <typename T>
class Array3D : public ArrayND<T, 3> {
  typedef ArrayND<T, 3> Base;
 public:
  Array3D()
      : Base() {
  }
  Array3D(int height, int width, int depth = 1)
      : Base(height, width, depth) {
  }
  Array3D(T* data, int height, int width, int depth = 1)
      : Base(data, height, width, depth) {
  }

  void Resize(int height, int width, int depth = 1) {
    Base::Resize(height, width, depth);
  }

  int Height() const {
    return Base::Shape(0);
  }
  int Width() const {
    return Base::Shape(1);
  }
  int Depth() const {
    return Base::Shape(2);
  }

  // Match Eigen2's API so that Array3D's and Mat*'s can work together via
  // template magic.
  int rows() const { return Height(); }
  int cols() const { return Width(); }
  int depth() const { return Depth(); }

  int Get_Step() const { return Width()*Depth(); }

  /// Enable accessing with 2 indices for grayscale images.
  T &operator()(int i0, int i1, int i2 = 0) {
    assert(0 <= i0 && i0 < Height());
    assert(0 <= i1 && i1 < Width());
    return Base::operator()(i0, i1, i2);
  }
  const T &operator()(int i0, int i1, int i2 = 0) const {
    assert(0 <= i0 && i0 < Height());
    assert(0 <= i1 && i1 < Width());
    return Base::operator()(i0, i1, i2);
  }
};

typedef Array3D<unsigned char> Array3Du;
typedef Array3D<unsigned int> Array3Dui;
typedef Array3D<int> Array3Di;
typedef Array3D<float> Array3Df;
typedef Array3D<short> Array3Ds;

void SplitChannels(const Array3Df &input,
                   Array3Df *channel0,
                   Array3Df *channel1,
                   Array3Df *channel2);

void PrintArray(const Array3Df &array);

/** Convert a float array into a byte array by scaling values by 255* (max-min).
 *  where max and min are automatically detected 
 *  (if automatic_range_detection = true)
 * \note and TODO this automatic detection only works when the image contains
 *  at least one pixel of both bounds.
 **/
void FloatArrayToScaledByteArray(const Array3Df &float_array,
                                 Array3Du *byte_array,
                                 bool automatic_range_detection = false);

//! Convert a byte array into a float array by dividing values by 255.
void ByteArrayToScaledFloatArray(const Array3Du &byte_array,
                                 Array3Df *float_array);

template <typename AArrayType, typename BArrayType, typename CArrayType>
void MultiplyElements(const AArrayType &a,
           const BArrayType &b,
           CArrayType *c) {
  // This function does an element-wise multiply between
  // the two Arrays A and B, and stores the result in C.
  // A and B must have the same dimensions.
  assert(a.Shape() == b.Shape());
  c->ResizeLike(a);

  // To perform the multiplcation, a "current" index into the N-dimensions of
  // the A and B matrix specifies which elements are being multiplied.
  typename CArrayType::Index index;

  // The index starts at the maximum value for each dimension
  const typename CArrayType::Index& cShape = c->Shape();
  for ( int i = 0; i < CArrayType::Index::SIZE; ++i )
    index(i) = cShape(i) - 1;

  // After each multiplication, the highest-dimensional index is reduced.
  // if this reduces it less than zero, it resets to its maximum value
  // and decrements the index of the next lower dimension.
  // This ripple-action continues until the entire new array has been
  // calculated, indicated by dimension zero having a negative index.
  while ( index(0) >= 0 ) {
    (*c)(index) = a(index) * b(index);

    int dimension = CArrayType::Index::SIZE - 1;
    index(dimension) = index(dimension) - 1;
    while ( dimension > 0 && index(dimension) < 0 ) {
      index(dimension) = cShape(dimension) - 1;
      index(dimension - 1) = index(dimension - 1) - 1;
      --dimension;
    }
  }
}

template <typename TA, typename TB, typename TC>
void MultiplyElements(const ArrayND<TA, 3> &a,
                      const ArrayND<TB, 3> &b,
                      ArrayND<TC, 3> *c) {
  // Specialization for N==3
  c->ResizeLike(a);
  assert(a.Shape(0) == b.Shape(0));
  assert(a.Shape(1) == b.Shape(1));
  assert(a.Shape(2) == b.Shape(2));
  for (int i = 0; i < a.Shape(0); ++i) {
    for (int j = 0; j < a.Shape(1); ++j) {
      for (int k = 0; k < a.Shape(2); ++k) {
        (*c)(i, j, k) = TC(a(i, j, k) * b(i, j, k));
      }
    }
  }
}

template <typename TA, typename TB, typename TC>
void MultiplyElements(const Array3D<TA> &a,
                      const Array3D<TB> &b,
                      Array3D<TC> *c) {
  // Specialization for N==3
  c->ResizeLike(a);
  assert(a.Shape(0) == b.Shape(0));
  assert(a.Shape(1) == b.Shape(1));
  assert(a.Shape(2) == b.Shape(2));
  for (int i = 0; i < a.Shape(0); ++i) {
    for (int j = 0; j < a.Shape(1); ++j) {
      for (int k = 0; k < a.Shape(2); ++k) {
        (*c)(i, j, k) = TC(a(i, j, k) * b(i, j, k));
      }
    }
  }
}

}  // namespace libmv

#endif  // LIBMV_IMAGE_ARRAY_ND_H

/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Template for matrix types.
 *
 * The `blender::MatBase<T, NumCol, NumRow>` is a Row x Col matrix (in mathematical notation) laid
 * out as column major in memory.
 *
 * This class overloads `+, -, *` and `+=, -=, *=` mathematical operators.
 * They are all using per component operation, except for a few:
 * `MatBase<C,R> * Vector<C>` the vector product with the matrix.
 * `Vector<R> * MatBase<C,R>` the vector product with the **transposed** matrix.
 * `MatBase<C,R> * MatBase<R,C>` and `MatBase<C,R> *= MatBase<R,C>` the matrix multiplication.
 *
 * The `blender::MatView` allows working on a subset of a matrix without having to move the data
 * around. It can be obtained using the `MatBase.view<NumCol, NumRow>()`. It is const by default if
 * the matrix type is. Otherwise, a `blender::MutableMatView` is returned.
 *
 * A `blender::MutableMatView`. It is mostly the same as `blender::MatView`, but can to be
 * modified.
 *
 * This allow working with any number type `T` (`float, double, mpq, ...`)
 * and to use these types in shared shader files (code compiled in both C++ and Shader language).
 * To this end, only low level constructors are defined inside the class itself and every
 * function working on matrices are defined outside of the class in the `blender::math` namespace.
 */

#include <ostream>
#include <type_traits>

#include "BLI_unroll.hh"

#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"

namespace blender {

template<typename T,
         int NumCol,
         int NumRow,
         int SrcNumCol,
         int SrcNumRow,
         int SrcStartCol,
         int SrcStartRow,
         int Alignment>
struct MatView;

template<typename T,
         int NumCol,
         int NumRow,
         int SrcNumCol,
         int SrcNumRow,
         int SrcStartCol,
         int SrcStartRow,
         int Alignment>
struct MutableMatView;

template<
    /* Number type. */
    typename T,
    /* Number of column in the matrix. */
    int NumCol,
    /* Number of row in the matrix. */
    int NumRow,
    /* Alignment in bytes. Do not align matrices whose size is not a multiple of 4 component.
     * This is in order to avoid padding when using arrays of matrices. */
    int Alignment = (((NumCol * NumRow) % 4 == 0) ? 4 : 1) * sizeof(T)>
struct alignas(Alignment) MatBase : public vec_struct_base<VecBase<T, NumRow>, NumCol, false> {

  using base_type = T;
  using vec3_type = VecBase<T, 3>;
  using col_type = VecBase<T, NumRow>;
  using row_type = VecBase<T, NumCol>;
  using loc_type = VecBase<T, (NumRow < NumCol) ? NumRow : (NumRow - 1)>;
  static constexpr int min_dim = (NumRow < NumCol) ? NumRow : NumCol;
  static constexpr int col_len = NumCol;
  static constexpr int row_len = NumRow;

  MatBase() = default;

/* Workaround issue with template BLI_ENABLE_IF((Size == 2)) not working. */
#define BLI_ENABLE_IF_MAT(_size, _test) int S = _size, BLI_ENABLE_IF((S _test))

  template<BLI_ENABLE_IF_MAT(NumCol, == 2)> MatBase(col_type _x, col_type _y)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
  }

  template<BLI_ENABLE_IF_MAT(NumCol, == 3)> MatBase(col_type _x, col_type _y, col_type _z)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
    (*this)[2] = _z;
  }

  template<BLI_ENABLE_IF_MAT(NumCol, == 4)>
  MatBase(col_type _x, col_type _y, col_type _z, col_type _w)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
    (*this)[2] = _z;
    (*this)[3] = _w;
  }

  /** Masking. */

  template<typename U, int OtherNumCol, int OtherNumRow>
  explicit MatBase(const MatBase<U, OtherNumCol, OtherNumRow> &other)
  {
    if constexpr ((OtherNumRow >= NumRow) && (OtherNumCol >= NumCol)) {
      unroll<NumCol>([&](auto i) { (*this)[i] = col_type(other[i]); });
    }
    else {
      /* Allow enlarging following GLSL standard (i.e: mat4x4(mat3x3())). */
      unroll<NumCol>([&](auto i) {
        unroll<NumRow>([&](auto j) {
          if (i < OtherNumCol && j < OtherNumRow) {
            (*this)[i][j] = other[i][j];
          }
          else if (i == j) {
            (*this)[i][j] = T(1);
          }
          else {
            (*this)[i][j] = T(0);
          }
        });
      });
    }
  }

#undef BLI_ENABLE_IF_MAT

  /** Conversion from pointers (from C-style vectors). */

  explicit MatBase(const T *ptr)
  {
    unroll<NumCol>([&](auto i) { (*this)[i] = reinterpret_cast<const col_type *>(ptr)[i]; });
  }

  template<typename U, BLI_ENABLE_IF((std::is_convertible_v<U, T>))> explicit MatBase(const U *ptr)
  {
    unroll<NumCol>([&](auto i) { (*this)[i] = ptr[i]; });
  }

  explicit MatBase(const T (*ptr)[NumCol]) : MatBase(static_cast<const T *>(ptr[0])) {}

  /** Conversion from other matrix types. */

  template<typename U> explicit MatBase(const MatBase<U, NumRow, NumCol> &vec)
  {
    unroll<NumCol>([&](auto i) { (*this)[i] = col_type(vec[i]); });
  }

  /** C-style pointer dereference. */

  using c_style_mat = T[NumCol][NumRow];

  /** \note Prevent implicit cast to types that could fit other pointer constructor. */
  const c_style_mat &ptr() const
  {
    return *reinterpret_cast<const c_style_mat *>(this);
  }

  /** \note Prevent implicit cast to types that could fit other pointer constructor. */
  c_style_mat &ptr()
  {
    return *reinterpret_cast<c_style_mat *>(this);
  }

  /** \note Prevent implicit cast to types that could fit other pointer constructor. */
  const T *base_ptr() const
  {
    return reinterpret_cast<const T *>(this);
  }

  /** \note Prevent implicit cast to types that could fit other pointer constructor. */
  T *base_ptr()
  {
    return reinterpret_cast<T *>(this);
  }

  /** View creation. */

  template<int ViewNumCol = NumCol,
           int ViewNumRow = NumRow,
           int SrcStartCol = 0,
           int SrcStartRow = 0>
  const MatView<T, ViewNumCol, ViewNumRow, NumCol, NumRow, SrcStartCol, SrcStartRow, Alignment>
  view() const
  {
    return MatView<T, ViewNumCol, ViewNumRow, NumCol, NumRow, SrcStartCol, SrcStartRow, Alignment>(
        const_cast<MatBase &>(*this));
  }

  template<int ViewNumCol = NumCol,
           int ViewNumRow = NumRow,
           int SrcStartCol = 0,
           int SrcStartRow = 0>
  MutableMatView<T, ViewNumCol, ViewNumRow, NumCol, NumRow, SrcStartCol, SrcStartRow, Alignment>
  view()
  {
    return MutableMatView<T,
                          ViewNumCol,
                          ViewNumRow,
                          NumCol,
                          NumRow,
                          SrcStartCol,
                          SrcStartRow,
                          Alignment>(*this);
  }

  /** Array access. */

  const col_type &operator[](int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < NumCol);
    return reinterpret_cast<const col_type *>(this)[index];
  }

  col_type &operator[](int index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < NumCol);
    return reinterpret_cast<col_type *>(this)[index];
  }

  /** Access helpers. Using Blender coordinate system. */

  vec3_type &x_axis()
  {
    BLI_STATIC_ASSERT(NumCol >= 1, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 3, "Wrong Matrix dimension");
    return *reinterpret_cast<vec3_type *>(&(*this)[0]);
  }

  vec3_type &y_axis()
  {
    BLI_STATIC_ASSERT(NumCol >= 2, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 3, "Wrong Matrix dimension");
    return *reinterpret_cast<vec3_type *>(&(*this)[1]);
  }

  vec3_type &z_axis()
  {
    BLI_STATIC_ASSERT(NumCol >= 3, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 3, "Wrong Matrix dimension");
    return *reinterpret_cast<vec3_type *>(&(*this)[2]);
  }

  loc_type &location()
  {
    BLI_STATIC_ASSERT(NumCol >= 3, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 2, "Wrong Matrix dimension");
    return *reinterpret_cast<loc_type *>(&(*this)[NumCol - 1]);
  }

  const vec3_type &x_axis() const
  {
    BLI_STATIC_ASSERT(NumCol >= 1, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 3, "Wrong Matrix dimension");
    return *reinterpret_cast<const vec3_type *>(&(*this)[0]);
  }

  const vec3_type &y_axis() const
  {
    BLI_STATIC_ASSERT(NumCol >= 2, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 3, "Wrong Matrix dimension");
    return *reinterpret_cast<const vec3_type *>(&(*this)[1]);
  }

  const vec3_type &z_axis() const
  {
    BLI_STATIC_ASSERT(NumCol >= 3, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 3, "Wrong Matrix dimension");
    return *reinterpret_cast<const vec3_type *>(&(*this)[2]);
  }

  const loc_type &location() const
  {
    BLI_STATIC_ASSERT(NumCol >= 3, "Wrong Matrix dimension");
    BLI_STATIC_ASSERT(NumRow >= 2, "Wrong Matrix dimension");
    return *reinterpret_cast<const loc_type *>(&(*this)[NumCol - 1]);
  }

  /** Matrix operators. */

  friend MatBase operator+(const MatBase &a, const MatBase &b)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] + b[i]; });
    return result;
  }

  friend MatBase operator+(const MatBase &a, T b)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] + b; });
    return result;
  }

  friend MatBase operator+(T a, const MatBase &b)
  {
    return b + a;
  }

  MatBase &operator+=(const MatBase &b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] += b[i]; });
    return *this;
  }

  MatBase &operator+=(T b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] += b; });
    return *this;
  }

  friend MatBase operator-(const MatBase &a)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = -a[i]; });
    return result;
  }

  friend MatBase operator-(const MatBase &a, const MatBase &b)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] - b[i]; });
    return result;
  }

  friend MatBase operator-(const MatBase &a, T b)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] - b; });
    return result;
  }

  friend MatBase operator-(T a, const MatBase &b)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = a - b[i]; });
    return result;
  }

  MatBase &operator-=(const MatBase &b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] -= b[i]; });
    return *this;
  }

  MatBase &operator-=(T b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] -= b; });
    return *this;
  }

  /** Multiply each component by a scalar. */
  friend MatBase operator*(const MatBase &a, T b)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] * b; });
    return result;
  }

  /** Multiply each component by a scalar. */
  friend MatBase operator*(T a, const MatBase &b)
  {
    return b * a;
  }

  /** Multiply two matrices using matrix multiplication. */
  MatBase &operator*=(const MatBase &b) &
  {
    const MatBase &a = *this;
    *this = a * b;
    return *this;
  }

  /** Multiply each component by a scalar. */
  MatBase &operator*=(T b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] *= b; });
    return *this;
  }

  /** Vector operators. */

  friend col_type operator*(const MatBase &a, const row_type &b)
  {
    /* This is the reference implementation.
     * Might be overloaded with vectorized / optimized code. */
    col_type result(0);
    unroll<NumCol>([&](auto c) { result += b[c] * a[c]; });
    return result;
  }

  /** Multiply by the transposed. */
  friend row_type operator*(const col_type &a, const MatBase &b)
  {
    /* This is the reference implementation.
     * Might be overloaded with vectorized / optimized code. */
    row_type result(0);
    unroll<NumCol>([&](auto c) { unroll<NumRow>([&](auto r) { result[c] += b[c][r] * a[r]; }); });
    return result;
  }

  /** Compare. */

  friend bool operator==(const MatBase &a, const MatBase &b)
  {
    for (int i = 0; i < NumCol; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(const MatBase &a, const MatBase &b)
  {
    return !(a == b);
  }

  /** Miscellaneous. */

  static MatBase diagonal(T value)
  {
    MatBase result{};
    unroll<min_dim>([&](auto i) { result[i][i] = value; });
    return result;
  }

  static MatBase all(T value)
  {
    MatBase result;
    unroll<NumCol>([&](auto i) { result[i] = col_type(value); });
    return result;
  }

  static MatBase identity()
  {
    return diagonal(1);
  }

  static MatBase zero()
  {
    return all(0);
  }

  uint64_t hash() const
  {
    uint64_t h = 435109;
    unroll<NumCol * NumRow>([&](auto i) {
      T value = (reinterpret_cast<const T *>(this))[i];
      h = h * 33 + *reinterpret_cast<const as_uint_type<T> *>(&value);
    });
    return h;
  }

  friend std::ostream &operator<<(std::ostream &stream, const MatBase &mat)
  {
    stream << "(\n";
    for (int i = 0; i < NumRow; i++) {
      stream << "(";
      for (int j = 0; j < NumCol; j++) {
        /** NOTE: j and i are swapped to follow mathematical convention. */
        stream << mat[j][i];
        if (j < NumCol - 1) {
          stream << ", ";
        }
      }
      stream << ")";
      if (i < NumRow - 1) {
        stream << ",";
      }
      stream << "\n";
    }

    stream << ")\n";
    return stream;
  }
};

template<typename T,
         /** The view dimensions. */
         int NumCol,
         int NumRow,
         /** The source matrix dimensions. */
         int SrcNumCol,
         int SrcNumRow,
         /** The base offset inside the source matrix. */
         int SrcStartCol,
         int SrcStartRow,
         /** The source matrix alignment. */
         int SrcAlignment>
struct MatView : NonCopyable, NonMovable {
  using MatT = MatBase<T, NumCol, NumRow>;
  using SrcMatT = MatBase<T, SrcNumCol, SrcNumRow, SrcAlignment>;
  using col_type = VecBase<T, NumRow>;
  using row_type = VecBase<T, NumCol>;

  const SrcMatT &mat;

  MatView() = delete;

  MatView(const SrcMatT &src) : mat(src)
  {
    BLI_STATIC_ASSERT(SrcStartCol >= 0, "View does not fit source matrix dimensions");
    BLI_STATIC_ASSERT(SrcStartRow >= 0, "View does not fit source matrix dimensions");
    BLI_STATIC_ASSERT(SrcStartCol + NumCol <= SrcNumCol,
                      "View does not fit source matrix dimensions");
    BLI_STATIC_ASSERT(SrcStartRow + NumRow <= SrcNumRow,
                      "View does not fit source matrix dimensions");
  }

  /** Allow wrapping C-style matrices using view. IMPORTANT: Alignment of src needs to match. */
  explicit MatView(const float (*src)[SrcNumRow])
      : MatView(*reinterpret_cast<const SrcMatT *>(&src[0][0])) {};

  /** Array access. */

  const col_type &operator[](int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < NumCol);
    return *reinterpret_cast<const col_type *>(&mat[index + SrcStartCol][SrcStartRow]);
  }

  /** Conversion back to matrix. */

  operator MatT() const
  {
    MatT mat;
    unroll<NumCol>([&](auto c) { mat[c] = (*this)[c]; });
    return mat;
  }

  /** Matrix operators. */

  friend MatT operator+(const MatView &a, T b)
  {
    MatT result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] + b; });
    return result;
  }

  friend MatT operator+(T a, const MatView &b)
  {
    return b + a;
  }

  friend MatT operator-(const MatView &a)
  {
    MatT result;
    unroll<NumCol>([&](auto i) { result[i] = -a[i]; });
    return result;
  }

  template<int OtherSrcNumCol,
           int OtherSrcNumRow,
           int OtherSrcStartCol,
           int OtherSrcStartRow,
           int OtherSrcAlignment>
  friend MatT operator-(const MatView &a,
                        const MatView<T,
                                      NumCol,
                                      NumRow,
                                      OtherSrcNumCol,
                                      OtherSrcNumRow,
                                      OtherSrcStartCol,
                                      OtherSrcStartRow,
                                      OtherSrcAlignment> &b)
  {
    MatT result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] - b[i]; });
    return result;
  }

  friend MatT operator-(const MatView &a, const MatT &b)
  {
    return a - b.view();
  }

  template<int OtherSrcNumCol,
           int OtherSrcNumRow,
           int OtherSrcStartCol,
           int OtherSrcStartRow,
           int OtherSrcAlignment>
  friend MatT operator-(const MatView<T,
                                      NumCol,
                                      NumRow,
                                      OtherSrcNumCol,
                                      OtherSrcNumRow,
                                      OtherSrcStartCol,
                                      OtherSrcStartRow,
                                      OtherSrcAlignment> &a,
                        const MatView &b)
  {
    MatT result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] - b[i]; });
    return result;
  }

  friend MatT operator-(const MatT &a, const MatView &b)
  {
    return a.view() - b;
  }

  friend MatT operator-(const MatView &a, T b)
  {
    MatT result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] - b; });
    return result;
  }

  friend MatView operator-(T a, const MatView &b)
  {
    MatView result;
    unroll<NumCol>([&](auto i) { result[i] = a - b[i]; });
    return result;
  }

  MatT operator*(const MatT &b) const
  {
    return *this * b.view();
  }

  /** Multiply each component by a scalar. */
  friend MatT operator*(const MatView &a, T b)
  {
    MatT result;
    unroll<NumCol>([&](auto i) { result[i] = a[i] * b; });
    return result;
  }

  /** Multiply each component by a scalar. */
  friend MatT operator*(T a, const MatView &b)
  {
    return b * a;
  }

  /** Vector operators. */

  friend col_type operator*(const MatView &a, const row_type &b)
  {
    /* This is the reference implementation.
     * Might be overloaded with vectorized / optimized code. */
    col_type result(0);
    unroll<NumCol>([&](auto c) { result += b[c] * a[c]; });
    return result;
  }

  /** Multiply by the transposed. */
  friend row_type operator*(const col_type &a, const MatView &b)
  {
    /* This is the reference implementation.
     * Might be overloaded with vectorized / optimized code. */
    row_type result(0);
    unroll<NumCol>([&](auto c) { unroll<NumRow>([&](auto r) { result[c] += b[c][r] * a[r]; }); });
    return result;
  }

  /** Compare. */

  friend bool operator==(const MatView &a, const MatView &b)
  {
    for (int i = 0; i < NumCol; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(const MatView &a, const MatView &b)
  {
    return !(a == b);
  }

  /** Miscellaneous. */

  friend std::ostream &operator<<(std::ostream &stream, const MatView &mat)
  {
    return stream << mat.mat;
  }
};

template<typename T,
         /** The view dimensions. */
         int NumCol,
         int NumRow,
         /** The source matrix dimensions. */
         int SrcNumCol,
         int SrcNumRow,
         /** The base offset inside the source matrix. */
         int SrcStartCol,
         int SrcStartRow,
         /** The source matrix alignment. */
         int SrcAlignment>
struct MutableMatView
    : MatView<T, NumCol, NumRow, SrcNumCol, SrcNumRow, SrcStartCol, SrcStartRow, SrcAlignment> {

  using MatT = MatBase<T, NumCol, NumRow>;
  using MatViewT =
      MatView<T, NumCol, NumRow, SrcNumCol, SrcNumRow, SrcStartCol, SrcStartRow, SrcAlignment>;
  using SrcMatT = MatBase<T, SrcNumCol, SrcNumRow, SrcAlignment>;
  using col_type = VecBase<T, NumRow>;
  using row_type = VecBase<T, NumCol>;

 public:
  MutableMatView() = delete;

  MutableMatView(SrcMatT &src) : MatViewT(const_cast<const SrcMatT &>(src)) {};

  /** Allow wrapping C-style matrices using view. IMPORTANT: Alignment of src needs to match. */
  explicit MutableMatView(float src[SrcNumCol][SrcNumRow])
      : MutableMatView(*reinterpret_cast<SrcMatT *>(&src[0][0])) {};

  /** Array access. */

  col_type &operator[](int index)
  {
    return const_cast<col_type &>(static_cast<MatViewT &>(*this)[index]);
  }

  /** Conversion to immutable view. */

  operator MatViewT() const
  {
    return MatViewT(this->mat);
  }

  /** Copy Assignment. */

  template<int OtherSrcNumCol,
           int OtherSrcNumRow,
           int OtherSrcStartCol,
           int OtherSrcStartRow,
           int OtherSrcAlignment>
  MutableMatView &operator=(const MatView<T,
                                          NumCol,
                                          NumRow,
                                          OtherSrcNumCol,
                                          OtherSrcNumRow,
                                          OtherSrcStartCol,
                                          OtherSrcStartRow,
                                          OtherSrcAlignment> &other)
  {
    BLI_assert_msg(
        (reinterpret_cast<const void *>(&other.mat[0][0]) !=
         reinterpret_cast<const void *>(&this->mat[0][0])) ||
            /* Make sure assignment won't overwrite the source. OtherSrc* is the source. */
            ((OtherSrcStartCol > SrcStartCol) || (OtherSrcStartCol + NumCol <= SrcStartCol) ||
             (OtherSrcStartRow > SrcStartRow + NumRow) ||
             (OtherSrcStartRow + NumRow <= SrcStartRow)),
        "Operation is undefined if views overlap.");
    unroll<NumCol>([&](auto i) { (*this)[i] = other[i]; });
    return *this;
  }

  MutableMatView &operator=(const MatT &other)
  {
    *this = other.view();
    return *this;
  }

  /** Matrix operators. */

  template<int OtherSrcNumCol,
           int OtherSrcNumRow,
           int OtherSrcStartCol,
           int OtherSrcStartRow,
           int OtherSrcAlignment>
  MutableMatView &operator+=(const MatView<T,
                                           NumCol,
                                           NumRow,
                                           OtherSrcNumCol,
                                           OtherSrcNumRow,
                                           OtherSrcStartCol,
                                           OtherSrcStartRow,
                                           OtherSrcAlignment> &b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] += b[i]; });
    return *this;
  }

  MutableMatView &operator+=(const MatT &b) &
  {
    return *this += b.view();
  }

  MutableMatView &operator+=(T b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] += b; });
    return *this;
  }

  template<int OtherSrcNumCol,
           int OtherSrcNumRow,
           int OtherSrcStartCol,
           int OtherSrcStartRow,
           int OtherSrcAlignment>
  MutableMatView &operator-=(const MatView<T,
                                           NumCol,
                                           NumRow,
                                           OtherSrcNumCol,
                                           OtherSrcNumRow,
                                           OtherSrcStartCol,
                                           OtherSrcStartRow,
                                           OtherSrcAlignment> &b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] -= b[i]; });
    return *this;
  }

  MutableMatView &operator-=(const MatT &b) &
  {
    return *this -= b.view();
  }

  MutableMatView &operator-=(T b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] -= b; });
    return *this;
  }

  /** Multiply two matrices using matrix multiplication. */
  template<int OtherSrcNumCol,
           int OtherSrcNumRow,
           int OtherSrcStartCol,
           int OtherSrcStartRow,
           int OtherSrcAlignment>
  MutableMatView &operator*=(const MatView<T,
                                           NumCol,
                                           NumRow,
                                           OtherSrcNumCol,
                                           OtherSrcNumRow,
                                           OtherSrcStartCol,
                                           OtherSrcStartRow,
                                           OtherSrcAlignment> &b) &
  {
    *this = *static_cast<MatViewT *>(this) * b;
    return *this;
  }

  MutableMatView &operator*=(const MatT &b) &
  {
    return *this *= b.view();
  }

  /** Multiply each component by a scalar. */
  MutableMatView &operator*=(T b) &
  {
    unroll<NumCol>([&](auto i) { (*this)[i] *= b; });
    return *this;
  }

  /** Vector operators. Need to be redefined to avoid operator priority issue. */

  friend col_type operator*(MutableMatView &a, const row_type &b)
  {
    /* This is the reference implementation.
     * Might be overloaded with vectorized / optimized code. */
    col_type result(0);
    unroll<NumCol>([&](auto c) { result += b[c] * a[c]; });
    return result;
  }

  /** Multiply by the transposed. */
  friend row_type operator*(const col_type &a, MutableMatView &b)
  {
    /* This is the reference implementation.
     * Might be overloaded with vectorized / optimized code. */
    row_type result(0);
    unroll<NumCol>([&](auto c) { unroll<NumRow>([&](auto r) { result[c] += b[c][r] * a[r]; }); });
    return result;
  }
};

namespace detail {

/** Multiply two matrices using matrix multiplication. */
template<typename T,
         int A_NumCol,
         int A_NumRow,
         int B_NumCol,
         int B_NumRow,
         typename MatA,
         typename MatB>
MatBase<T, B_NumCol, A_NumRow> matrix_mul_impl(const MatA &a, const MatB &b)
{
  static_assert(A_NumCol == B_NumRow);
  /* This is the reference implementation.
   * Might be overloaded with vectorized / optimized code. */
  MatBase<T, B_NumCol, A_NumRow> result{};
  unroll<B_NumCol>([&](auto j) {
    unroll<A_NumRow>([&](auto i) {
      /* Same as dot product, but avoid dependency on vector math. */
      unroll<A_NumCol>([&](auto k) { result[j][i] += a[k][i] * b[j][k]; });
    });
  });
  return result;
}

}  // namespace detail

template<typename T, int A_NumCol, int A_NumRow, int B_NumCol, int B_NumRow>
MatBase<T, B_NumCol, A_NumRow> operator*(const MatBase<T, A_NumCol, A_NumRow> &a,
                                         const MatBase<T, B_NumCol, B_NumRow> &b)
{
  return detail::matrix_mul_impl<T, A_NumCol, A_NumRow, B_NumCol, B_NumRow>(a, b);
}

template<typename T,
         int A_NumCol,
         int A_NumRow,
         int A_SrcNumCol,
         int A_SrcNumRow,
         int A_SrcStartCol,
         int A_SrcStartRow,
         int A_SrcAlignment,
         int B_NumCol,
         int B_NumRow,
         int B_SrcNumCol,
         int B_SrcNumRow,
         int B_SrcStartCol,
         int B_SrcStartRow,
         int B_SrcAlignment>
MatBase<T, B_NumCol, A_NumRow> operator*(const MatView<T,
                                                       A_NumCol,
                                                       A_NumRow,
                                                       A_SrcNumCol,
                                                       A_SrcNumRow,
                                                       A_SrcStartCol,
                                                       A_SrcStartRow,
                                                       A_SrcAlignment> &a,
                                         const MatView<T,
                                                       B_NumCol,
                                                       B_NumRow,
                                                       B_SrcNumCol,
                                                       B_SrcNumRow,
                                                       B_SrcStartCol,
                                                       B_SrcStartRow,
                                                       B_SrcAlignment> &b)
{
  return detail::matrix_mul_impl<T, A_NumCol, A_NumRow, B_NumCol, B_NumRow>(a, b);
}
template<typename T,
         int A_NumCol,
         int A_NumRow,
         int A_SrcNumCol,
         int A_SrcNumRow,
         int A_SrcStartCol,
         int A_SrcStartRow,
         int A_SrcAlignment,
         int B_NumCol,
         int B_NumRow>
MatBase<T, B_NumCol, A_NumRow> operator*(const MatView<T,
                                                       A_NumCol,
                                                       A_NumRow,
                                                       A_SrcNumCol,
                                                       A_SrcNumRow,
                                                       A_SrcStartCol,
                                                       A_SrcStartRow,
                                                       A_SrcAlignment> &a,
                                         const MatBase<T, B_NumCol, B_NumRow> &b)
{
  return detail::matrix_mul_impl<T, A_NumCol, A_NumRow, B_NumCol, B_NumRow>(a, b);
}

template<typename T,
         int A_NumCol,
         int A_NumRow,
         int B_NumCol,
         int B_NumRow,
         int B_SrcNumCol,
         int B_SrcNumRow,
         int B_SrcStartCol,
         int B_SrcStartRow,
         int B_SrcAlignment>
MatBase<T, B_NumCol, A_NumRow> operator*(const MatBase<T, A_NumCol, A_NumRow> &a,
                                         const MatView<T,
                                                       B_NumCol,
                                                       B_NumRow,
                                                       B_SrcNumCol,
                                                       B_SrcNumRow,
                                                       B_SrcStartCol,
                                                       B_SrcStartRow,
                                                       B_SrcAlignment> &b)
{
  return detail::matrix_mul_impl<T, A_NumCol, A_NumRow, B_NumCol, B_NumRow>(a, b);
}

using float2x2 = MatBase<float, 2, 2>;
using float2x3 = MatBase<float, 2, 3>;
using float2x4 = MatBase<float, 2, 4>;
using float3x2 = MatBase<float, 3, 2>;
using float3x3 = MatBase<float, 3, 3>;
using float3x4 = MatBase<float, 3, 4>;
using float4x2 = MatBase<float, 4, 2>;
using float4x3 = MatBase<float, 4, 3>;
using float4x4 = MatBase<float, 4, 4>;

/* These types are reserved to wrap C matrices without copy. Note the un-alignment. */
/* TODO: It would be preferable to align all C matrices inside DNA structs. */
using float4x4_view = MatView<float, 4, 4, 4, 4, 0, 0, alignof(float)>;
using float4x4_mutableview = MutableMatView<float, 4, 4, 4, 4, 0, 0, alignof(float)>;

using double2x2 = MatBase<double, 2, 2>;
using double2x3 = MatBase<double, 2, 3>;
using double2x4 = MatBase<double, 2, 4>;
using double3x2 = MatBase<double, 3, 2>;
using double3x3 = MatBase<double, 3, 3>;
using double3x4 = MatBase<double, 3, 4>;
using double4x2 = MatBase<double, 4, 2>;
using double4x3 = MatBase<double, 4, 3>;
using double4x4 = MatBase<double, 4, 4>;

/* Specialization for SSE optimization. */
template<> float4x4 operator*(const float4x4 &a, const float4x4 &b);
template<> float3x3 operator*(const float3x3 &a, const float3x3 &b);

extern template float2x2 operator*(const float2x2 &a, const float2x2 &b);
extern template double2x2 operator*(const double2x2 &a, const double2x2 &b);
extern template double3x3 operator*(const double3x3 &a, const double3x3 &b);
extern template double4x4 operator*(const double4x4 &a, const double4x4 &b);

}  // namespace blender

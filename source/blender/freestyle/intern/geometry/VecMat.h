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

#ifndef __VECMAT_H__
#define __VECMAT_H__

/** \file
 * \ingroup freestyle
 * \brief Vectors and Matrices definition and manipulation
 */

#include <iostream>
#include <math.h>
#include <vector>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

namespace VecMat {

namespace Internal {
template<bool B> struct is_false {
};

template<> struct is_false<false> {
  static inline void ensure()
  {
  }
};
}  // end of namespace Internal

//
//  Vector class
//    - T: value type
//    - N: dimension
//
/////////////////////////////////////////////////////////////////////////////

template<class T, unsigned N> class Vec {
 public:
  typedef T value_type;

  // constructors
  inline Vec()
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] = 0;
    }
  }

  ~Vec()
  {
    Internal::is_false<(N == 0)>::ensure();
  }

  template<class U> explicit inline Vec(const U tab[N])
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] = (T)tab[i];
    }
  }

  template<class U> explicit inline Vec(const std::vector<U> &tab)
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] = (T)tab[i];
    }
  }

  template<class U> explicit inline Vec(const Vec<U, N> &v)
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] = (T)v[i];
    }
  }

  // accessors
  inline value_type operator[](const unsigned i) const
  {
    return this->_coord[i];
  }

  inline value_type &operator[](const unsigned i)
  {
    return this->_coord[i];
  }

  static inline unsigned dim()
  {
    return N;
  }

  // various useful methods
  inline value_type norm() const
  {
    return (T)sqrt((float)squareNorm());
  }

  inline value_type squareNorm() const
  {
    return (*this) * (*this);
  }

  inline Vec<T, N> &normalize()
  {
    value_type n = norm();
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] /= n;
    }
    return *this;
  }

  inline Vec<T, N> &normalizeSafe()
  {
    value_type n = norm();
    if (n) {
      for (unsigned int i = 0; i < N; i++) {
        this->_coord[i] /= n;
      }
    }
    return *this;
  }

  // classical operators
  inline Vec<T, N> operator+(const Vec<T, N> &v) const
  {
    Vec<T, N> res(v);
    res += *this;
    return res;
  }

  inline Vec<T, N> operator-(const Vec<T, N> &v) const
  {
    Vec<T, N> res(*this);
    res -= v;
    return res;
  }

  inline Vec<T, N> operator*(const typename Vec<T, N>::value_type r) const
  {
    Vec<T, N> res(*this);
    res *= r;
    return res;
  }

  inline Vec<T, N> operator/(const typename Vec<T, N>::value_type r) const
  {
    Vec<T, N> res(*this);
    if (r) {
      res /= r;
    }
    return res;
  }

  // dot product
  inline value_type operator*(const Vec<T, N> &v) const
  {
    value_type sum = 0;
    for (unsigned int i = 0; i < N; i++) {
      sum += (*this)[i] * v[i];
    }
    return sum;
  }

  template<class U> inline Vec<T, N> &operator=(const Vec<U, N> &v)
  {
    if (this != &v) {
      for (unsigned int i = 0; i < N; i++) {
        this->_coord[i] = (T)v[i];
      }
    }
    return *this;
  }

  template<class U> inline Vec<T, N> &operator+=(const Vec<U, N> &v)
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] += (T)v[i];
    }
    return *this;
  }

  template<class U> inline Vec<T, N> &operator-=(const Vec<U, N> &v)
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] -= (T)v[i];
    }
    return *this;
  }

  template<class U> inline Vec<T, N> &operator*=(const U r)
  {
    for (unsigned int i = 0; i < N; i++) {
      this->_coord[i] *= r;
    }
    return *this;
  }

  template<class U> inline Vec<T, N> &operator/=(const U r)
  {
    if (r) {
      for (unsigned int i = 0; i < N; i++) {
        this->_coord[i] /= r;
      }
    }
    return *this;
  }

  inline bool operator==(const Vec<T, N> &v) const
  {
    for (unsigned int i = 0; i < N; i++) {
      if (this->_coord[i] != v[i]) {
        return false;
      }
    }
    return true;
  }

  inline bool operator!=(const Vec<T, N> &v) const
  {
    for (unsigned int i = 0; i < N; i++) {
      if (this->_coord[i] != v[i]) {
        return true;
      }
    }
    return false;
  }

  inline bool operator<(const Vec<T, N> &v) const
  {
    for (unsigned int i = 0; i < N; i++) {
      if (this->_coord[i] < v[i]) {
        return true;
      }
      if (this->_coord[i] > v[i]) {
        return false;
      }
      if (this->_coord[i] == v[i]) {
        continue;
      }
    }
    return false;
  }

  inline bool operator>(const Vec<T, N> &v) const
  {
    for (unsigned int i = 0; i < N; i++) {
      if (this->_coord[i] > v[i]) {
        return true;
      }
      if (this->_coord[i] < v[i]) {
        return false;
      }
      if (this->_coord[i] == v[i]) {
        continue;
      }
    }
    return false;
  }

 protected:
  value_type _coord[N];
  enum {
    _dim = N,
  };

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:VecMat:Vec")
#endif
};

//
//  Vec2 class (2D Vector)
//    - T: value type
//
/////////////////////////////////////////////////////////////////////////////

template<class T> class Vec2 : public Vec<T, 2> {
 public:
  typedef typename Vec<T, 2>::value_type value_type;

  inline Vec2() : Vec<T, 2>()
  {
  }

  template<class U> explicit inline Vec2(const U tab[2]) : Vec<T, 2>(tab)
  {
  }

  template<class U> explicit inline Vec2(const std::vector<U> &tab) : Vec<T, 2>(tab)
  {
  }

  template<class U> inline Vec2(const Vec<U, 2> &v) : Vec<T, 2>(v)
  {
  }

  inline Vec2(const value_type x, const value_type y = 0) : Vec<T, 2>()
  {
    this->_coord[0] = (T)x;
    this->_coord[1] = (T)y;
  }

  inline value_type x() const
  {
    return this->_coord[0];
  }

  inline value_type &x()
  {
    return this->_coord[0];
  }

  inline value_type y() const
  {
    return this->_coord[1];
  }

  inline value_type &y()
  {
    return this->_coord[1];
  }

  inline void setX(const value_type v)
  {
    this->_coord[0] = v;
  }

  inline void setY(const value_type v)
  {
    this->_coord[1] = v;
  }

  // FIXME: hack swig -- no choice
  inline Vec2<T> operator+(const Vec2<T> &v) const
  {
    Vec2<T> res(v);
    res += *this;
    return res;
  }

  inline Vec2<T> operator-(const Vec2<T> &v) const
  {
    Vec2<T> res(*this);
    res -= v;
    return res;
  }

  inline Vec2<T> operator*(const value_type r) const
  {
    Vec2<T> res(*this);
    res *= r;
    return res;
  }

  inline Vec2<T> operator/(const value_type r) const
  {
    Vec2<T> res(*this);
    if (r) {
      res /= r;
    }
    return res;
  }

  // dot product
  inline value_type operator*(const Vec2<T> &v) const
  {
    value_type sum = 0;
    for (unsigned int i = 0; i < 2; i++) {
      sum += (*this)[i] * v[i];
    }
    return sum;
  }
};

//
//  HVec3 class (3D Vector in homogeneous coordinates)
//    - T: value type
//
/////////////////////////////////////////////////////////////////////////////

template<class T> class HVec3 : public Vec<T, 4> {
 public:
  typedef typename Vec<T, 4>::value_type value_type;

  inline HVec3() : Vec<T, 4>()
  {
  }

  template<class U> explicit inline HVec3(const U tab[4]) : Vec<T, 4>(tab)
  {
  }

  template<class U> explicit inline HVec3(const std::vector<U> &tab) : Vec<T, 4>(tab)
  {
  }

  template<class U> inline HVec3(const Vec<U, 4> &v) : Vec<T, 4>(v)
  {
  }

  inline HVec3(const value_type sx,
               const value_type sy = 0,
               const value_type sz = 0,
               const value_type s = 1)
  {
    this->_coord[0] = sx;
    this->_coord[1] = sy;
    this->_coord[2] = sz;
    this->_coord[3] = s;
  }

  template<class U> inline HVec3(const Vec<U, 3> &sv, const U s = 1)
  {
    this->_coord[0] = (T)sv[0];
    this->_coord[1] = (T)sv[1];
    this->_coord[2] = (T)sv[2];
    this->_coord[3] = (T)s;
  }

  inline value_type sx() const
  {
    return this->_coord[0];
  }

  inline value_type &sx()
  {
    return this->_coord[0];
  }

  inline value_type sy() const
  {
    return this->_coord[1];
  }

  inline value_type &sy()
  {
    return this->_coord[1];
  }

  inline value_type sz() const
  {
    return this->_coord[2];
  }

  inline value_type &sz()
  {
    return this->_coord[2];
  }

  inline value_type s() const
  {
    return this->_coord[3];
  }

  inline value_type &s()
  {
    return this->_coord[3];
  }

  // Access to non-homogeneous coordinates in 3D
  inline value_type x() const
  {
    return this->_coord[0] / this->_coord[3];
  }

  inline value_type y() const
  {
    return this->_coord[1] / this->_coord[3];
  }

  inline value_type z() const
  {
    return this->_coord[2] / this->_coord[3];
  }
};

//
//  Vec3 class (3D Vec)
//    - T: value type
//
/////////////////////////////////////////////////////////////////////////////
template<class T> class Vec3 : public Vec<T, 3> {
 public:
  typedef typename Vec<T, 3>::value_type value_type;

  inline Vec3() : Vec<T, 3>()
  {
  }

  template<class U> explicit inline Vec3(const U tab[3]) : Vec<T, 3>(tab)
  {
  }

  template<class U> explicit inline Vec3(const std::vector<U> &tab) : Vec<T, 3>(tab)
  {
  }

  template<class U> inline Vec3(const Vec<U, 3> &v) : Vec<T, 3>(v)
  {
  }

  template<class U> inline Vec3(const HVec3<U> &v)
  {
    this->_coord[0] = (T)v.x();
    this->_coord[1] = (T)v.y();
    this->_coord[2] = (T)v.z();
  }

  inline Vec3(const value_type x, const value_type y = 0, const value_type z = 0) : Vec<T, 3>()
  {
    this->_coord[0] = x;
    this->_coord[1] = y;
    this->_coord[2] = z;
  }

  inline value_type x() const
  {
    return this->_coord[0];
  }

  inline value_type &x()
  {
    return this->_coord[0];
  }

  inline value_type y() const
  {
    return this->_coord[1];
  }

  inline value_type &y()
  {
    return this->_coord[1];
  }

  inline value_type z() const
  {
    return this->_coord[2];
  }

  inline value_type &z()
  {
    return this->_coord[2];
  }

  inline void setX(const value_type v)
  {
    this->_coord[0] = v;
  }

  inline void setY(const value_type v)
  {
    this->_coord[1] = v;
  }

  inline void setZ(const value_type v)
  {
    this->_coord[2] = v;
  }

  // classical operators
  // FIXME: hack swig -- no choice
  inline Vec3<T> operator+(const Vec3<T> &v) const
  {
    Vec3<T> res(v);
    res += *this;
    return res;
  }

  inline Vec3<T> operator-(const Vec3<T> &v) const
  {
    Vec3<T> res(*this);
    res -= v;
    return res;
  }

  inline Vec3<T> operator*(const value_type r) const
  {
    Vec3<T> res(*this);
    res *= r;
    return res;
  }

  inline Vec3<T> operator/(const value_type r) const
  {
    Vec3<T> res(*this);
    if (r) {
      res /= r;
    }
    return res;
  }

  // dot product
  inline value_type operator*(const Vec3<T> &v) const
  {
    value_type sum = 0;
    for (unsigned int i = 0; i < 3; i++) {
      sum += (*this)[i] * v[i];
    }
    return sum;
  }

  // cross product for 3D Vectors
  // FIXME: hack swig -- no choice
  inline Vec3<T> operator^(const Vec3<T> &v) const
  {
    Vec3<T> res((*this)[1] * v[2] - (*this)[2] * v[1],
                (*this)[2] * v[0] - (*this)[0] * v[2],
                (*this)[0] * v[1] - (*this)[1] * v[0]);
    return res;
  }

  // cross product for 3D Vectors
  template<typename U> inline Vec3<T> operator^(const Vec<U, 3> &v) const
  {
    Vec3<T> res((*this)[1] * v[2] - (*this)[2] * v[1],
                (*this)[2] * v[0] - (*this)[0] * v[2],
                (*this)[0] * v[1] - (*this)[1] * v[0]);
    return res;
  }
};

//
//  Matrix class
//    - T: value type
//    - M: rows
//    - N: cols
//
/////////////////////////////////////////////////////////////////////////////

// Dirty, but icc under Windows needs this
#define _SIZE (M * N)

template<class T, unsigned M, unsigned N> class Matrix {
 public:
  typedef T value_type;

  inline Matrix()
  {
    for (unsigned int i = 0; i < _SIZE; i++) {
      this->_coord[i] = 0;
    }
  }

  ~Matrix()
  {
    Internal::is_false<(M == 0)>::ensure();
    Internal::is_false<(N == 0)>::ensure();
  }

  template<class U> explicit inline Matrix(const U tab[_SIZE])
  {
    for (unsigned int i = 0; i < _SIZE; i++) {
      this->_coord[i] = tab[i];
    }
  }

  template<class U> explicit inline Matrix(const std::vector<U> &tab)
  {
    for (unsigned int i = 0; i < _SIZE; i++) {
      this->_coord[i] = tab[i];
    }
  }

  template<class U> inline Matrix(const Matrix<U, M, N> &m)
  {
    for (unsigned int i = 0; i < M; i++) {
      for (unsigned int j = 0; j < N; j++) {
        this->_coord[i * N + j] = (T)m(i, j);
      }
    }
  }

  inline value_type operator()(const unsigned i, const unsigned j) const
  {
    return this->_coord[i * N + j];
  }

  inline value_type &operator()(const unsigned i, const unsigned j)
  {
    return this->_coord[i * N + j];
  }

  static inline unsigned rows()
  {
    return M;
  }

  static inline unsigned cols()
  {
    return N;
  }

  inline Matrix<T, M, N> &transpose() const
  {
    Matrix<T, N, M> res;
    for (unsigned int i = 0; i < M; i++) {
      for (unsigned int j = 0; j < N; j++) {
        res(j, i) = this->_coord[i * N + j];
      }
    }
    *this = res;
    return *this;
  }

  template<class U> inline Matrix<T, M, N> &operator=(const Matrix<U, M, N> &m)
  {
    if (this != &m) {
      for (unsigned int i = 0; i < M; i++) {
        for (unsigned int j = 0; j < N; j++) {
          this->_coord[i * N + j] = (T)m(i, j);
        }
      }
    }
    return *this;
  }

  template<class U> inline Matrix<T, M, N> &operator+=(const Matrix<U, M, N> &m)
  {
    for (unsigned int i = 0; i < M; i++) {
      for (unsigned int j = 0; j < N; j++) {
        this->_coord[i * N + j] += (T)m(i, j);
      }
    }
    return *this;
  }

  template<class U> inline Matrix<T, M, N> &operator-=(const Matrix<U, M, N> &m)
  {
    for (unsigned int i = 0; i < M; i++) {
      for (unsigned int j = 0; j < N; j++) {
        this->_coord[i * N + j] -= (T)m(i, j);
      }
    }
    return *this;
  }

  template<class U> inline Matrix<T, M, N> &operator*=(const U lambda)
  {
    for (unsigned int i = 0; i < M; i++) {
      for (unsigned int j = 0; j < N; j++) {
        this->_coord[i * N + j] *= lambda;
      }
    }
    return *this;
  }

  template<class U> inline Matrix<T, M, N> &operator/=(const U lambda)
  {
    if (lambda) {
      for (unsigned int i = 0; i < M; i++) {
        for (unsigned int j = 0; j < N; j++) {
          this->_coord[i * N + j] /= lambda;
        }
      }
    }
    return *this;
  }

 protected:
  value_type _coord[_SIZE];

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:VecMat:Matrix")
#endif
};

#undef _SIZE

//
//  SquareMatrix class
//    - T: value type
//    - N: rows & cols
//
/////////////////////////////////////////////////////////////////////////////

// Dirty, but icc under Windows needs this
#define _SIZE (N * N)

template<class T, unsigned N> class SquareMatrix : public Matrix<T, N, N> {
 public:
  typedef T value_type;

  inline SquareMatrix() : Matrix<T, N, N>()
  {
  }

  template<class U> explicit inline SquareMatrix(const U tab[_SIZE]) : Matrix<T, N, N>(tab)
  {
  }

  template<class U> explicit inline SquareMatrix(const std::vector<U> &tab) : Matrix<T, N, N>(tab)
  {
  }

  template<class U> inline SquareMatrix(const Matrix<U, N, N> &m) : Matrix<T, N, N>(m)
  {
  }

  static inline SquareMatrix<T, N> identity()
  {
    SquareMatrix<T, N> res;
    for (unsigned int i = 0; i < N; i++) {
      res(i, i) = 1;
    }
    return res;
  }
};

#undef _SIZE

//
// Vector external functions
//
/////////////////////////////////////////////////////////////////////////////

#if 0
template<class T, unsigned N> inline Vec<T, N> operator+(const Vec<T, N> &v1, const Vec<T, N> &v2)
{
  Vec<T, N> res(v1);
  res += v2;
  return res;
}

template<class T, unsigned N> inline Vec<T, N> operator-(const Vec<T, N> &v1, const Vec<T, N> &v2)
{
  Vec<T, N> res(v1);
  res -= v2;
  return res;
}

template<class T, unsigned N>
inline Vec<T, N> operator*(const Vec<T, N> &v, const typename Vec<T, N>::value_type r)
{
  Vec<T, N> res(v);
  res *= r;
  return res;
}
#endif

template<class T, unsigned N>
inline Vec<T, N> operator*(const typename Vec<T, N>::value_type r, const Vec<T, N> &v)
{
  Vec<T, N> res(v);
  res *= r;
  return res;
}

#if 0
template<class T, unsigned N>
inline Vec<T, N> operator/(const Vec<T, N> &v, const typename Vec<T, N>::value_type r)
{
  Vec<T, N> res(v);
  if (r) {
    res /= r;
  }
  return res;
}

// dot product
template<class T, unsigned N>
inline typename Vec<T, N>::value_type operator*(const Vec<T, N> &v1, const Vec<T, N> &v2)
{
  typename Vec<T, N>::value_type sum = 0;
  for (unsigned int i = 0; i < N; i++) {
    sum += v1[i] * v2[i];
  }
  return sum;
}

// cross product for 3D Vectors
template<typename T> inline Vec3<T> operator^(const Vec<T, 3> &v1, const Vec<T, 3> &v2)
{
  Vec3<T> res(
      v1[1] * v2[2] - v1[2] * v2[1], v1[2] * v2[0] - v1[0] * v2[2], v1[0] * v2[1] - v1[1] * v2[0]);
  return res;
}
#endif

// stream operator
template<class T, unsigned N> inline std::ostream &operator<<(std::ostream &s, const Vec<T, N> &v)
{
  unsigned int i;
  s << "[";
  for (i = 0; i < N - 1; i++) {
    s << v[i] << ", ";
  }
  s << v[i] << "]";
  return s;
}

//
// Matrix external functions
//
/////////////////////////////////////////////////////////////////////////////

template<class T, unsigned M, unsigned N>
inline Matrix<T, M, N> operator+(const Matrix<T, M, N> &m1, const Matrix<T, M, N> &m2)
{
  Matrix<T, M, N> res(m1);
  res += m2;
  return res;
}

template<class T, unsigned M, unsigned N>
inline Matrix<T, M, N> operator-(const Matrix<T, M, N> &m1, const Matrix<T, M, N> &m2)
{
  Matrix<T, M, N> res(m1);
  res -= m2;
  return res;
}

template<class T, unsigned M, unsigned N>
inline Matrix<T, M, N> operator*(const Matrix<T, M, N> &m1,
                                 const typename Matrix<T, M, N>::value_type lambda)
{
  Matrix<T, M, N> res(m1);
  res *= lambda;
  return res;
}

template<class T, unsigned M, unsigned N>
inline Matrix<T, M, N> operator*(const typename Matrix<T, M, N>::value_type lambda,
                                 const Matrix<T, M, N> &m1)
{
  Matrix<T, M, N> res(m1);
  res *= lambda;
  return res;
}

template<class T, unsigned M, unsigned N>
inline Matrix<T, M, N> operator/(const Matrix<T, M, N> &m1,
                                 const typename Matrix<T, M, N>::value_type lambda)
{
  Matrix<T, M, N> res(m1);
  res /= lambda;
  return res;
}

template<class T, unsigned M, unsigned N, unsigned P>
inline Matrix<T, M, P> operator*(const Matrix<T, M, N> &m1, const Matrix<T, N, P> &m2)
{
  unsigned int i, j, k;
  Matrix<T, M, P> res;
  typename Matrix<T, N, P>::value_type scale;

  for (j = 0; j < P; j++) {
    for (k = 0; k < N; k++) {
      scale = m2(k, j);
      for (i = 0; i < N; i++) {
        res(i, j) += m1(i, k) * scale;
      }
    }
  }
  return res;
}

template<class T, unsigned M, unsigned N>
inline Vec<T, M> operator*(const Matrix<T, M, N> &m, const Vec<T, N> &v)
{
  Vec<T, M> res;
  typename Matrix<T, M, N>::value_type scale;

  for (unsigned int j = 0; j < M; j++) {
    scale = v[j];
    for (unsigned int i = 0; i < N; i++) {
      res[i] += m(i, j) * scale;
    }
  }
  return res;
}

// stream operator
template<class T, unsigned M, unsigned N>
inline std::ostream &operator<<(std::ostream &s, const Matrix<T, M, N> &m)
{
  unsigned int i, j;
  for (i = 0; i < M; i++) {
    s << "[";
    for (j = 0; j < N - 1; j++) {
      s << m(i, j) << ", ";
    }
    s << m(i, j) << "]" << std::endl;
  }
  return s;
}

}  // end of namespace VecMat

} /* namespace Freestyle */

#endif  // __VECMAT_H__

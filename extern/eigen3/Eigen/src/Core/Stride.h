// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_STRIDE_H
#define EIGEN_STRIDE_H

/** \class Stride
  * \ingroup Core_Module
  *
  * \brief Holds strides information for Map
  *
  * This class holds the strides information for mapping arrays with strides with class Map.
  *
  * It holds two values: the inner stride and the outer stride.
  *
  * The inner stride is the pointer increment between two consecutive entries within a given row of a
  * row-major matrix or within a given column of a column-major matrix.
  *
  * The outer stride is the pointer increment between two consecutive rows of a row-major matrix or
  * between two consecutive columns of a column-major matrix.
  *
  * These two values can be passed either at compile-time as template parameters, or at runtime as
  * arguments to the constructor.
  *
  * Indeed, this class takes two template parameters:
  *  \param _OuterStrideAtCompileTime the outer stride, or Dynamic if you want to specify it at runtime.
  *  \param _InnerStrideAtCompileTime the inner stride, or Dynamic if you want to specify it at runtime.
  *
  * Here is an example:
  * \include Map_general_stride.cpp
  * Output: \verbinclude Map_general_stride.out
  *
  * \sa class InnerStride, class OuterStride, \ref TopicStorageOrders
  */
template<int _OuterStrideAtCompileTime, int _InnerStrideAtCompileTime>
class Stride
{
  public:
    typedef DenseIndex Index;
    enum {
      InnerStrideAtCompileTime = _InnerStrideAtCompileTime,
      OuterStrideAtCompileTime = _OuterStrideAtCompileTime
    };

    /** Default constructor, for use when strides are fixed at compile time */
    Stride()
      : m_outer(OuterStrideAtCompileTime), m_inner(InnerStrideAtCompileTime)
    {
      eigen_assert(InnerStrideAtCompileTime != Dynamic && OuterStrideAtCompileTime != Dynamic);
    }

    /** Constructor allowing to pass the strides at runtime */
    Stride(Index outerStride, Index innerStride)
      : m_outer(outerStride), m_inner(innerStride)
    {
      eigen_assert(innerStride>=0 && outerStride>=0);
    }

    /** Copy constructor */
    Stride(const Stride& other)
      : m_outer(other.outer()), m_inner(other.inner())
    {}

    /** \returns the outer stride */
    inline Index outer() const { return m_outer.value(); }
    /** \returns the inner stride */
    inline Index inner() const { return m_inner.value(); }

  protected:
    internal::variable_if_dynamic<Index, OuterStrideAtCompileTime> m_outer;
    internal::variable_if_dynamic<Index, InnerStrideAtCompileTime> m_inner;
};

/** \brief Convenience specialization of Stride to specify only an inner stride
  * See class Map for some examples */
template<int Value = Dynamic>
class InnerStride : public Stride<0, Value>
{
    typedef Stride<0, Value> Base;
  public:
    typedef DenseIndex Index;
    InnerStride() : Base() {}
    InnerStride(Index v) : Base(0, v) {}
};

/** \brief Convenience specialization of Stride to specify only an outer stride
  * See class Map for some examples */
template<int Value = Dynamic>
class OuterStride : public Stride<Value, 0>
{
    typedef Stride<Value, 0> Base;
  public:
    typedef DenseIndex Index;
    OuterStride() : Base() {}
    OuterStride(Index v) : Base(v,0) {}
};

#endif // EIGEN_STRIDE_H

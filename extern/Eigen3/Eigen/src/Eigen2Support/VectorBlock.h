// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN2_VECTORBLOCK_H
#define EIGEN2_VECTORBLOCK_H

/** \deprecated use DenseMase::head(Index) */
template<typename Derived>
inline VectorBlock<Derived>
MatrixBase<Derived>::start(Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived>(derived(), 0, size);
}

/** \deprecated use DenseMase::head(Index) */
template<typename Derived>
inline const VectorBlock<const Derived>
MatrixBase<Derived>::start(Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived>(derived(), 0, size);
}

/** \deprecated use DenseMase::tail(Index) */
template<typename Derived>
inline VectorBlock<Derived>
MatrixBase<Derived>::end(Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived>(derived(), this->size() - size, size);
}

/** \deprecated use DenseMase::tail(Index) */
template<typename Derived>
inline const VectorBlock<const Derived>
MatrixBase<Derived>::end(Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived>(derived(), this->size() - size, size);
}

/** \deprecated use DenseMase::head() */
template<typename Derived>
template<int Size>
inline VectorBlock<Derived,Size>
MatrixBase<Derived>::start()
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived,Size>(derived(), 0);
}

/** \deprecated use DenseMase::head() */
template<typename Derived>
template<int Size>
inline const VectorBlock<const Derived,Size>
MatrixBase<Derived>::start() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived,Size>(derived(), 0);
}

/** \deprecated use DenseMase::tail() */
template<typename Derived>
template<int Size>
inline VectorBlock<Derived,Size>
MatrixBase<Derived>::end()
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived, Size>(derived(), size() - Size);
}

/** \deprecated use DenseMase::tail() */
template<typename Derived>
template<int Size>
inline const VectorBlock<const Derived,Size>
MatrixBase<Derived>::end() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived, Size>(derived(), size() - Size);
}

#endif // EIGEN2_VECTORBLOCK_H

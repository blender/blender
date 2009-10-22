// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
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

#ifndef EIGEN_COEFFS_H
#define EIGEN_COEFFS_H

/** Short version: don't use this function, use
  * \link operator()(int,int) const \endlink instead.
  *
  * Long version: this function is similar to
  * \link operator()(int,int) const \endlink, but without the assertion.
  * Use this for limiting the performance cost of debugging code when doing
  * repeated coefficient access. Only use this when it is guaranteed that the
  * parameters \a row and \a col are in range.
  *
  * If EIGEN_INTERNAL_DEBUGGING is defined, an assertion will be made, making this
  * function equivalent to \link operator()(int,int) const \endlink.
  *
  * \sa operator()(int,int) const, coeffRef(int,int), coeff(int) const
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::coeff(int row, int col) const
{
  ei_internal_assert(row >= 0 && row < rows()
                     && col >= 0 && col < cols());
  return derived().coeff(row, col);
}

/** \returns the coefficient at given the given row and column.
  *
  * \sa operator()(int,int), operator[](int) const
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::operator()(int row, int col) const
{
  ei_assert(row >= 0 && row < rows()
      && col >= 0 && col < cols());
  return derived().coeff(row, col);
}

/** Short version: don't use this function, use
  * \link operator()(int,int) \endlink instead.
  *
  * Long version: this function is similar to
  * \link operator()(int,int) \endlink, but without the assertion.
  * Use this for limiting the performance cost of debugging code when doing
  * repeated coefficient access. Only use this when it is guaranteed that the
  * parameters \a row and \a col are in range.
  *
  * If EIGEN_INTERNAL_DEBUGGING is defined, an assertion will be made, making this
  * function equivalent to \link operator()(int,int) \endlink.
  *
  * \sa operator()(int,int), coeff(int, int) const, coeffRef(int)
  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::coeffRef(int row, int col)
{
  ei_internal_assert(row >= 0 && row < rows()
                     && col >= 0 && col < cols());
  return derived().coeffRef(row, col);
}

/** \returns a reference to the coefficient at given the given row and column.
  *
  * \sa operator()(int,int) const, operator[](int)
  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::operator()(int row, int col)
{
  ei_assert(row >= 0 && row < rows()
      && col >= 0 && col < cols());
  return derived().coeffRef(row, col);
}

/** Short version: don't use this function, use
  * \link operator[](int) const \endlink instead.
  *
  * Long version: this function is similar to
  * \link operator[](int) const \endlink, but without the assertion.
  * Use this for limiting the performance cost of debugging code when doing
  * repeated coefficient access. Only use this when it is guaranteed that the
  * parameter \a index is in range.
  *
  * If EIGEN_INTERNAL_DEBUGGING is defined, an assertion will be made, making this
  * function equivalent to \link operator[](int) const \endlink.
  *
  * \sa operator[](int) const, coeffRef(int), coeff(int,int) const
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::coeff(int index) const
{
  ei_internal_assert(index >= 0 && index < size());
  return derived().coeff(index);
}

/** \returns the coefficient at given index.
  *
  * This method is allowed only for vector expressions, and for matrix expressions having the LinearAccessBit.
  *
  * \sa operator[](int), operator()(int,int) const, x() const, y() const,
  * z() const, w() const
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::operator[](int index) const
{
  ei_assert(index >= 0 && index < size());
  return derived().coeff(index);
}

/** \returns the coefficient at given index.
  *
  * This is synonymous to operator[](int) const.
  *
  * This method is allowed only for vector expressions, and for matrix expressions having the LinearAccessBit.
  *
  * \sa operator[](int), operator()(int,int) const, x() const, y() const,
  * z() const, w() const
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::operator()(int index) const
{
  ei_assert(index >= 0 && index < size());
  return derived().coeff(index);
}

/** Short version: don't use this function, use
  * \link operator[](int) \endlink instead.
  *
  * Long version: this function is similar to
  * \link operator[](int) \endlink, but without the assertion.
  * Use this for limiting the performance cost of debugging code when doing
  * repeated coefficient access. Only use this when it is guaranteed that the
  * parameters \a row and \a col are in range.
  *
  * If EIGEN_INTERNAL_DEBUGGING is defined, an assertion will be made, making this
  * function equivalent to \link operator[](int) \endlink.
  *
  * \sa operator[](int), coeff(int) const, coeffRef(int,int)
  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::coeffRef(int index)
{
  ei_internal_assert(index >= 0 && index < size());
  return derived().coeffRef(index);
}

/** \returns a reference to the coefficient at given index.
  *
  * This method is allowed only for vector expressions, and for matrix expressions having the LinearAccessBit.
  *
  * \sa operator[](int) const, operator()(int,int), x(), y(), z(), w()
  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::operator[](int index)
{
  ei_assert(index >= 0 && index < size());
  return derived().coeffRef(index);
}

/** \returns a reference to the coefficient at given index.
  *
  * This is synonymous to operator[](int).
  *
  * This method is allowed only for vector expressions, and for matrix expressions having the LinearAccessBit.
  *
  * \sa operator[](int) const, operator()(int,int), x(), y(), z(), w()
  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::operator()(int index)
{
  ei_assert(index >= 0 && index < size());
  return derived().coeffRef(index);
}

/** equivalent to operator[](0).  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::x() const { return (*this)[0]; }

/** equivalent to operator[](1).  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::y() const { return (*this)[1]; }

/** equivalent to operator[](2).  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::z() const { return (*this)[2]; }

/** equivalent to operator[](3).  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename ei_traits<Derived>::Scalar MatrixBase<Derived>
  ::w() const { return (*this)[3]; }

/** equivalent to operator[](0).  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::x() { return (*this)[0]; }

/** equivalent to operator[](1).  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::y() { return (*this)[1]; }

/** equivalent to operator[](2).  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::z() { return (*this)[2]; }

/** equivalent to operator[](3).  */
template<typename Derived>
EIGEN_STRONG_INLINE typename ei_traits<Derived>::Scalar& MatrixBase<Derived>
  ::w() { return (*this)[3]; }

/** \returns the packet of coefficients starting at the given row and column. It is your responsibility
  * to ensure that a packet really starts there. This method is only available on expressions having the
  * PacketAccessBit.
  *
  * The \a LoadMode parameter may have the value \a Aligned or \a Unaligned. Its effect is to select
  * the appropriate vectorization instruction. Aligned access is faster, but is only possible for packets
  * starting at an address which is a multiple of the packet size.
  */
template<typename Derived>
template<int LoadMode>
EIGEN_STRONG_INLINE typename ei_packet_traits<typename ei_traits<Derived>::Scalar>::type
MatrixBase<Derived>::packet(int row, int col) const
{
  ei_internal_assert(row >= 0 && row < rows()
                     && col >= 0 && col < cols());
  return derived().template packet<LoadMode>(row,col);
}

/** Stores the given packet of coefficients, at the given row and column of this expression. It is your responsibility
  * to ensure that a packet really starts there. This method is only available on expressions having the
  * PacketAccessBit.
  *
  * The \a LoadMode parameter may have the value \a Aligned or \a Unaligned. Its effect is to select
  * the appropriate vectorization instruction. Aligned access is faster, but is only possible for packets
  * starting at an address which is a multiple of the packet size.
  */
template<typename Derived>
template<int StoreMode>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::writePacket
(int row, int col, const typename ei_packet_traits<typename ei_traits<Derived>::Scalar>::type& x)
{
  ei_internal_assert(row >= 0 && row < rows()
                     && col >= 0 && col < cols());
  derived().template writePacket<StoreMode>(row,col,x);
}

/** \returns the packet of coefficients starting at the given index. It is your responsibility
  * to ensure that a packet really starts there. This method is only available on expressions having the
  * PacketAccessBit and the LinearAccessBit.
  *
  * The \a LoadMode parameter may have the value \a Aligned or \a Unaligned. Its effect is to select
  * the appropriate vectorization instruction. Aligned access is faster, but is only possible for packets
  * starting at an address which is a multiple of the packet size.
  */
template<typename Derived>
template<int LoadMode>
EIGEN_STRONG_INLINE typename ei_packet_traits<typename ei_traits<Derived>::Scalar>::type
MatrixBase<Derived>::packet(int index) const
{
  ei_internal_assert(index >= 0 && index < size());
  return derived().template packet<LoadMode>(index);
}

/** Stores the given packet of coefficients, at the given index in this expression. It is your responsibility
  * to ensure that a packet really starts there. This method is only available on expressions having the
  * PacketAccessBit and the LinearAccessBit.
  *
  * The \a LoadMode parameter may have the value \a Aligned or \a Unaligned. Its effect is to select
  * the appropriate vectorization instruction. Aligned access is faster, but is only possible for packets
  * starting at an address which is a multiple of the packet size.
  */
template<typename Derived>
template<int StoreMode>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::writePacket
(int index, const typename ei_packet_traits<typename ei_traits<Derived>::Scalar>::type& x)
{
  ei_internal_assert(index >= 0 && index < size());
  derived().template writePacket<StoreMode>(index,x);
}

#ifndef EIGEN_PARSED_BY_DOXYGEN

/** \internal Copies the coefficient at position (row,col) of other into *this.
  *
  * This method is overridden in SwapWrapper, allowing swap() assignments to share 99% of their code
  * with usual assignments.
  *
  * Outside of this internal usage, this method has probably no usefulness. It is hidden in the public API dox.
  */
template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::copyCoeff(int row, int col, const MatrixBase<OtherDerived>& other)
{
  ei_internal_assert(row >= 0 && row < rows()
                     && col >= 0 && col < cols());
  derived().coeffRef(row, col) = other.derived().coeff(row, col);
}

/** \internal Copies the coefficient at the given index of other into *this.
  *
  * This method is overridden in SwapWrapper, allowing swap() assignments to share 99% of their code
  * with usual assignments.
  *
  * Outside of this internal usage, this method has probably no usefulness. It is hidden in the public API dox.
  */
template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::copyCoeff(int index, const MatrixBase<OtherDerived>& other)
{
  ei_internal_assert(index >= 0 && index < size());
  derived().coeffRef(index) = other.derived().coeff(index);
}

/** \internal Copies the packet at position (row,col) of other into *this.
  *
  * This method is overridden in SwapWrapper, allowing swap() assignments to share 99% of their code
  * with usual assignments.
  *
  * Outside of this internal usage, this method has probably no usefulness. It is hidden in the public API dox.
  */
template<typename Derived>
template<typename OtherDerived, int StoreMode, int LoadMode>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::copyPacket(int row, int col, const MatrixBase<OtherDerived>& other)
{
  ei_internal_assert(row >= 0 && row < rows()
                     && col >= 0 && col < cols());
  derived().template writePacket<StoreMode>(row, col,
    other.derived().template packet<LoadMode>(row, col));
}

/** \internal Copies the packet at the given index of other into *this.
  *
  * This method is overridden in SwapWrapper, allowing swap() assignments to share 99% of their code
  * with usual assignments.
  *
  * Outside of this internal usage, this method has probably no usefulness. It is hidden in the public API dox.
  */
template<typename Derived>
template<typename OtherDerived, int StoreMode, int LoadMode>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::copyPacket(int index, const MatrixBase<OtherDerived>& other)
{
  ei_internal_assert(index >= 0 && index < size());
  derived().template writePacket<StoreMode>(index,
    other.derived().template packet<LoadMode>(index));
}

#endif

#endif // EIGEN_COEFFS_H

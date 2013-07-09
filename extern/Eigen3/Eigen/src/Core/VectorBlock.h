// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_VECTORBLOCK_H
#define EIGEN_VECTORBLOCK_H

namespace Eigen { 

/** \class VectorBlock
  * \ingroup Core_Module
  *
  * \brief Expression of a fixed-size or dynamic-size sub-vector
  *
  * \param VectorType the type of the object in which we are taking a sub-vector
  * \param Size size of the sub-vector we are taking at compile time (optional)
  *
  * This class represents an expression of either a fixed-size or dynamic-size sub-vector.
  * It is the return type of DenseBase::segment(Index,Index) and DenseBase::segment<int>(Index) and
  * most of the time this is the only way it is used.
  *
  * However, if you want to directly maniputate sub-vector expressions,
  * for instance if you want to write a function returning such an expression, you
  * will need to use this class.
  *
  * Here is an example illustrating the dynamic case:
  * \include class_VectorBlock.cpp
  * Output: \verbinclude class_VectorBlock.out
  *
  * \note Even though this expression has dynamic size, in the case where \a VectorType
  * has fixed size, this expression inherits a fixed maximal size which means that evaluating
  * it does not cause a dynamic memory allocation.
  *
  * Here is an example illustrating the fixed-size case:
  * \include class_FixedVectorBlock.cpp
  * Output: \verbinclude class_FixedVectorBlock.out
  *
  * \sa class Block, DenseBase::segment(Index,Index,Index,Index), DenseBase::segment(Index,Index)
  */

namespace internal {
template<typename VectorType, int Size>
struct traits<VectorBlock<VectorType, Size> >
  : public traits<Block<VectorType,
                     traits<VectorType>::Flags & RowMajorBit ? 1 : Size,
                     traits<VectorType>::Flags & RowMajorBit ? Size : 1> >
{
};
}

template<typename VectorType, int Size> class VectorBlock
  : public Block<VectorType,
                     internal::traits<VectorType>::Flags & RowMajorBit ? 1 : Size,
                     internal::traits<VectorType>::Flags & RowMajorBit ? Size : 1>
{
    typedef Block<VectorType,
                     internal::traits<VectorType>::Flags & RowMajorBit ? 1 : Size,
                     internal::traits<VectorType>::Flags & RowMajorBit ? Size : 1> Base;
    enum {
      IsColVector = !(internal::traits<VectorType>::Flags & RowMajorBit)
    };
  public:
    EIGEN_DENSE_PUBLIC_INTERFACE(VectorBlock)

    using Base::operator=;

    /** Dynamic-size constructor
      */
    inline VectorBlock(VectorType& vector, Index start, Index size)
      : Base(vector,
             IsColVector ? start : 0, IsColVector ? 0 : start,
             IsColVector ? size  : 1, IsColVector ? 1 : size)
    {
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorBlock);
    }

    /** Fixed-size constructor
      */
    inline VectorBlock(VectorType& vector, Index start)
      : Base(vector, IsColVector ? start : 0, IsColVector ? 0 : start)
    {
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorBlock);
    }
};


/** \returns a dynamic-size expression of a segment (i.e. a vector block) in *this.
  *
  * \only_for_vectors
  *
  * \param start the first coefficient in the segment
  * \param size the number of coefficients in the segment
  *
  * Example: \include MatrixBase_segment_int_int.cpp
  * Output: \verbinclude MatrixBase_segment_int_int.out
  *
  * \note Even though the returned expression has dynamic size, in the case
  * when it is applied to a fixed-size vector, it inherits a fixed maximal size,
  * which means that evaluating it does not cause a dynamic memory allocation.
  *
  * \sa class Block, segment(Index)
  */
template<typename Derived>
inline typename DenseBase<Derived>::SegmentReturnType
DenseBase<Derived>::segment(Index start, Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return SegmentReturnType(derived(), start, size);
}

/** This is the const version of segment(Index,Index).*/
template<typename Derived>
inline typename DenseBase<Derived>::ConstSegmentReturnType
DenseBase<Derived>::segment(Index start, Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ConstSegmentReturnType(derived(), start, size);
}

/** \returns a dynamic-size expression of the first coefficients of *this.
  *
  * \only_for_vectors
  *
  * \param size the number of coefficients in the block
  *
  * Example: \include MatrixBase_start_int.cpp
  * Output: \verbinclude MatrixBase_start_int.out
  *
  * \note Even though the returned expression has dynamic size, in the case
  * when it is applied to a fixed-size vector, it inherits a fixed maximal size,
  * which means that evaluating it does not cause a dynamic memory allocation.
  *
  * \sa class Block, block(Index,Index)
  */
template<typename Derived>
inline typename DenseBase<Derived>::SegmentReturnType
DenseBase<Derived>::head(Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return SegmentReturnType(derived(), 0, size);
}

/** This is the const version of head(Index).*/
template<typename Derived>
inline typename DenseBase<Derived>::ConstSegmentReturnType
DenseBase<Derived>::head(Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ConstSegmentReturnType(derived(), 0, size);
}

/** \returns a dynamic-size expression of the last coefficients of *this.
  *
  * \only_for_vectors
  *
  * \param size the number of coefficients in the block
  *
  * Example: \include MatrixBase_end_int.cpp
  * Output: \verbinclude MatrixBase_end_int.out
  *
  * \note Even though the returned expression has dynamic size, in the case
  * when it is applied to a fixed-size vector, it inherits a fixed maximal size,
  * which means that evaluating it does not cause a dynamic memory allocation.
  *
  * \sa class Block, block(Index,Index)
  */
template<typename Derived>
inline typename DenseBase<Derived>::SegmentReturnType
DenseBase<Derived>::tail(Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return SegmentReturnType(derived(), this->size() - size, size);
}

/** This is the const version of tail(Index).*/
template<typename Derived>
inline typename DenseBase<Derived>::ConstSegmentReturnType
DenseBase<Derived>::tail(Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ConstSegmentReturnType(derived(), this->size() - size, size);
}

/** \returns a fixed-size expression of a segment (i.e. a vector block) in \c *this
  *
  * \only_for_vectors
  *
  * The template parameter \a Size is the number of coefficients in the block
  *
  * \param start the index of the first element of the sub-vector
  *
  * Example: \include MatrixBase_template_int_segment.cpp
  * Output: \verbinclude MatrixBase_template_int_segment.out
  *
  * \sa class Block
  */
template<typename Derived>
template<int Size>
inline typename DenseBase<Derived>::template FixedSegmentReturnType<Size>::Type
DenseBase<Derived>::segment(Index start)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename FixedSegmentReturnType<Size>::Type(derived(), start);
}

/** This is the const version of segment<int>(Index).*/
template<typename Derived>
template<int Size>
inline typename DenseBase<Derived>::template ConstFixedSegmentReturnType<Size>::Type
DenseBase<Derived>::segment(Index start) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename ConstFixedSegmentReturnType<Size>::Type(derived(), start);
}

/** \returns a fixed-size expression of the first coefficients of *this.
  *
  * \only_for_vectors
  *
  * The template parameter \a Size is the number of coefficients in the block
  *
  * Example: \include MatrixBase_template_int_start.cpp
  * Output: \verbinclude MatrixBase_template_int_start.out
  *
  * \sa class Block
  */
template<typename Derived>
template<int Size>
inline typename DenseBase<Derived>::template FixedSegmentReturnType<Size>::Type
DenseBase<Derived>::head()
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename FixedSegmentReturnType<Size>::Type(derived(), 0);
}

/** This is the const version of head<int>().*/
template<typename Derived>
template<int Size>
inline typename DenseBase<Derived>::template ConstFixedSegmentReturnType<Size>::Type
DenseBase<Derived>::head() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename ConstFixedSegmentReturnType<Size>::Type(derived(), 0);
}

/** \returns a fixed-size expression of the last coefficients of *this.
  *
  * \only_for_vectors
  *
  * The template parameter \a Size is the number of coefficients in the block
  *
  * Example: \include MatrixBase_template_int_end.cpp
  * Output: \verbinclude MatrixBase_template_int_end.out
  *
  * \sa class Block
  */
template<typename Derived>
template<int Size>
inline typename DenseBase<Derived>::template FixedSegmentReturnType<Size>::Type
DenseBase<Derived>::tail()
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename FixedSegmentReturnType<Size>::Type(derived(), size() - Size);
}

/** This is the const version of tail<int>.*/
template<typename Derived>
template<int Size>
inline typename DenseBase<Derived>::template ConstFixedSegmentReturnType<Size>::Type
DenseBase<Derived>::tail() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename ConstFixedSegmentReturnType<Size>::Type(derived(), size() - Size);
}

} // end namespace Eigen

#endif // EIGEN_VECTORBLOCK_H

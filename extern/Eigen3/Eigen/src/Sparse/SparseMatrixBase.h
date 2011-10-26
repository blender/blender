// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSEMATRIXBASE_H
#define EIGEN_SPARSEMATRIXBASE_H

/** \ingroup Sparse_Module
  *
  * \class SparseMatrixBase
  *
  * \brief Base class of any sparse matrices or sparse expressions
  *
  * \tparam Derived
  *
  * This class can be extended with the help of the plugin mechanism described on the page
  * \ref TopicCustomizingEigen by defining the preprocessor symbol \c EIGEN_SPARSEMATRIXBASE_PLUGIN.
  */
template<typename Derived> class SparseMatrixBase : public EigenBase<Derived>
{
  public:

    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;
    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;

    typedef SparseMatrixBase StorageBaseType;
    typedef EigenBase<Derived> Base;
    
    template<typename OtherDerived>
    Derived& operator=(const EigenBase<OtherDerived> &other)
    {
      other.derived().evalTo(derived());
      return derived();
    }
    
//     using Base::operator=;

    enum {

      RowsAtCompileTime = internal::traits<Derived>::RowsAtCompileTime,
        /**< The number of rows at compile-time. This is just a copy of the value provided
          * by the \a Derived type. If a value is not known at compile-time,
          * it is set to the \a Dynamic constant.
          * \sa MatrixBase::rows(), MatrixBase::cols(), ColsAtCompileTime, SizeAtCompileTime */

      ColsAtCompileTime = internal::traits<Derived>::ColsAtCompileTime,
        /**< The number of columns at compile-time. This is just a copy of the value provided
          * by the \a Derived type. If a value is not known at compile-time,
          * it is set to the \a Dynamic constant.
          * \sa MatrixBase::rows(), MatrixBase::cols(), RowsAtCompileTime, SizeAtCompileTime */


      SizeAtCompileTime = (internal::size_at_compile_time<internal::traits<Derived>::RowsAtCompileTime,
                                                   internal::traits<Derived>::ColsAtCompileTime>::ret),
        /**< This is equal to the number of coefficients, i.e. the number of
          * rows times the number of columns, or to \a Dynamic if this is not
          * known at compile-time. \sa RowsAtCompileTime, ColsAtCompileTime */

      MaxRowsAtCompileTime = RowsAtCompileTime,
      MaxColsAtCompileTime = ColsAtCompileTime,

      MaxSizeAtCompileTime = (internal::size_at_compile_time<MaxRowsAtCompileTime,
                                                      MaxColsAtCompileTime>::ret),

      IsVectorAtCompileTime = RowsAtCompileTime == 1 || ColsAtCompileTime == 1,
        /**< This is set to true if either the number of rows or the number of
          * columns is known at compile-time to be equal to 1. Indeed, in that case,
          * we are dealing with a column-vector (if there is only one column) or with
          * a row-vector (if there is only one row). */

      Flags = internal::traits<Derived>::Flags,
        /**< This stores expression \ref flags flags which may or may not be inherited by new expressions
          * constructed from this one. See the \ref flags "list of flags".
          */

      CoeffReadCost = internal::traits<Derived>::CoeffReadCost,
        /**< This is a rough measure of how expensive it is to read one coefficient from
          * this expression.
          */

      IsRowMajor = Flags&RowMajorBit ? 1 : 0,

      #ifndef EIGEN_PARSED_BY_DOXYGEN
      _HasDirectAccess = (int(Flags)&DirectAccessBit) ? 1 : 0 // workaround sunCC
      #endif
    };

    /* \internal the return type of MatrixBase::conjugate() */
//     typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
//                         const SparseCwiseUnaryOp<internal::scalar_conjugate_op<Scalar>, Derived>,
//                         const Derived&
//                      >::type ConjugateReturnType;
    /* \internal the return type of MatrixBase::real() */
//     typedef SparseCwiseUnaryOp<internal::scalar_real_op<Scalar>, Derived> RealReturnType;
    /* \internal the return type of MatrixBase::imag() */
//     typedef SparseCwiseUnaryOp<internal::scalar_imag_op<Scalar>, Derived> ImagReturnType;
    /** \internal the return type of MatrixBase::adjoint() */
    typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
                        CwiseUnaryOp<internal::scalar_conjugate_op<Scalar>, Eigen::Transpose<const Derived> >,
                        Transpose<const Derived>
                     >::type AdjointReturnType;


    typedef SparseMatrix<Scalar, Flags&RowMajorBit ? RowMajor : ColMajor> PlainObject;

#define EIGEN_CURRENT_STORAGE_BASE_CLASS Eigen::SparseMatrixBase
#   include "../plugins/CommonCwiseUnaryOps.h"
#   include "../plugins/CommonCwiseBinaryOps.h"
#   include "../plugins/MatrixCwiseUnaryOps.h"
#   include "../plugins/MatrixCwiseBinaryOps.h"
#   ifdef EIGEN_SPARSEMATRIXBASE_PLUGIN
#     include EIGEN_SPARSEMATRIXBASE_PLUGIN
#   endif
#   undef EIGEN_CURRENT_STORAGE_BASE_CLASS
#undef EIGEN_CURRENT_STORAGE_BASE_CLASS

#ifndef EIGEN_PARSED_BY_DOXYGEN
    /** This is the "real scalar" type; if the \a Scalar type is already real numbers
      * (e.g. int, float or double) then \a RealScalar is just the same as \a Scalar. If
      * \a Scalar is \a std::complex<T> then RealScalar is \a T.
      *
      * \sa class NumTraits
      */
    typedef typename NumTraits<Scalar>::Real RealScalar;

    /** \internal the return type of coeff()
      */
    typedef typename internal::conditional<_HasDirectAccess, const Scalar&, Scalar>::type CoeffReturnType;

    /** \internal Represents a matrix with all coefficients equal to one another*/
    typedef CwiseNullaryOp<internal::scalar_constant_op<Scalar>,Matrix<Scalar,Dynamic,Dynamic> > ConstantReturnType;

    /** type of the equivalent square matrix */
    typedef Matrix<Scalar,EIGEN_SIZE_MAX(RowsAtCompileTime,ColsAtCompileTime),
                          EIGEN_SIZE_MAX(RowsAtCompileTime,ColsAtCompileTime)> SquareMatrixType;

    inline const Derived& derived() const { return *static_cast<const Derived*>(this); }
    inline Derived& derived() { return *static_cast<Derived*>(this); }
    inline Derived& const_cast_derived() const
    { return *static_cast<Derived*>(const_cast<SparseMatrixBase*>(this)); }
#endif // not EIGEN_PARSED_BY_DOXYGEN

    /** \returns the number of rows. \sa cols(), RowsAtCompileTime */
    inline Index rows() const { return derived().rows(); }
    /** \returns the number of columns. \sa rows(), ColsAtCompileTime*/
    inline Index cols() const { return derived().cols(); }
    /** \returns the number of coefficients, which is \a rows()*cols().
      * \sa rows(), cols(), SizeAtCompileTime. */
    inline Index size() const { return rows() * cols(); }
    /** \returns the number of nonzero coefficients which is in practice the number
      * of stored coefficients. */
    inline Index nonZeros() const { return derived().nonZeros(); }
    /** \returns true if either the number of rows or the number of columns is equal to 1.
      * In other words, this function returns
      * \code rows()==1 || cols()==1 \endcode
      * \sa rows(), cols(), IsVectorAtCompileTime. */
    inline bool isVector() const { return rows()==1 || cols()==1; }
    /** \returns the size of the storage major dimension,
      * i.e., the number of columns for a columns major matrix, and the number of rows otherwise */
    Index outerSize() const { return (int(Flags)&RowMajorBit) ? this->rows() : this->cols(); }
    /** \returns the size of the inner dimension according to the storage order,
      * i.e., the number of rows for a columns major matrix, and the number of cols otherwise */
    Index innerSize() const { return (int(Flags)&RowMajorBit) ? this->cols() : this->rows(); }

    bool isRValue() const { return m_isRValue; }
    Derived& markAsRValue() { m_isRValue = true; return derived(); }

    SparseMatrixBase() : m_isRValue(false) { /* TODO check flags */ }
    
    inline Derived& operator=(const Derived& other)
    {
//       std::cout << "Derived& operator=(const Derived& other)\n";
//       if (other.isRValue())
//         derived().swap(other.const_cast_derived());
//       else
        this->operator=<Derived>(other);
      return derived();
    }
    
    template<typename OtherDerived>
    Derived& operator=(const ReturnByValue<OtherDerived>& other)
    {
      other.evalTo(derived());
      return derived();
    }


    template<typename OtherDerived>
    inline void assignGeneric(const OtherDerived& other)
    {
//       std::cout << "Derived& operator=(const MatrixBase<OtherDerived>& other)\n";
      //const bool transpose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
      eigen_assert(( ((internal::traits<Derived>::SupportedAccessPatterns&OuterRandomAccessPattern)==OuterRandomAccessPattern) ||
                  (!((Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit)))) &&
                  "the transpose operation is supposed to be handled in SparseMatrix::operator=");

      enum { Flip = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit) };

      const Index outerSize = other.outerSize();
      //typedef typename internal::conditional<transpose, LinkedVectorMatrix<Scalar,Flags&RowMajorBit>, Derived>::type TempType;
      // thanks to shallow copies, we always eval to a tempary
      Derived temp(other.rows(), other.cols());

      temp.reserve((std::max)(this->rows(),this->cols())*2);
      for (Index j=0; j<outerSize; ++j)
      {
        temp.startVec(j);
        for (typename OtherDerived::InnerIterator it(other.derived(), j); it; ++it)
        {
          Scalar v = it.value();
          if (v!=Scalar(0))
            temp.insertBackByOuterInner(Flip?it.index():j,Flip?j:it.index()) = v;
        }
      }
      temp.finalize();

      derived() = temp.markAsRValue();
    }


    template<typename OtherDerived>
    inline Derived& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
//       std::cout << typeid(OtherDerived).name() << "\n";
//       std::cout << Flags << " " << OtherDerived::Flags << "\n";
      const bool transpose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
//       std::cout << "eval transpose = " << transpose << "\n";
      const Index outerSize = (int(OtherDerived::Flags) & RowMajorBit) ? other.rows() : other.cols();
      if ((!transpose) && other.isRValue())
      {
        // eval without temporary
        derived().resize(other.rows(), other.cols());
        derived().setZero();
        derived().reserve((std::max)(this->rows(),this->cols())*2);
        for (Index j=0; j<outerSize; ++j)
        {
          derived().startVec(j);
          for (typename OtherDerived::InnerIterator it(other.derived(), j); it; ++it)
          {
            Scalar v = it.value();
            if (v!=Scalar(0))
              derived().insertBackByOuterInner(j,it.index()) = v;
          }
        }
        derived().finalize();
      }
      else
      {
        assignGeneric(other.derived());
      }
      return derived();
    }

    template<typename Lhs, typename Rhs>
    inline Derived& operator=(const SparseSparseProduct<Lhs,Rhs>& product);

    template<typename Lhs, typename Rhs>
    inline void _experimentalNewProduct(const Lhs& lhs, const Rhs& rhs);

    friend std::ostream & operator << (std::ostream & s, const SparseMatrixBase& m)
    {
      if (Flags&RowMajorBit)
      {
        for (Index row=0; row<m.outerSize(); ++row)
        {
          Index col = 0;
          for (typename Derived::InnerIterator it(m.derived(), row); it; ++it)
          {
            for ( ; col<it.index(); ++col)
              s << "0 ";
            s << it.value() << " ";
            ++col;
          }
          for ( ; col<m.cols(); ++col)
            s << "0 ";
          s << std::endl;
        }
      }
      else
      {
        if (m.cols() == 1) {
          Index row = 0;
          for (typename Derived::InnerIterator it(m.derived(), 0); it; ++it)
          {
            for ( ; row<it.index(); ++row)
              s << "0" << std::endl;
            s << it.value() << std::endl;
            ++row;
          }
          for ( ; row<m.rows(); ++row)
            s << "0" << std::endl;
        }
        else
        {
          SparseMatrix<Scalar, RowMajorBit> trans = m.derived();
          s << trans;
        }
      }
      return s;
    }

//     const SparseCwiseUnaryOp<internal::scalar_opposite_op<typename internal::traits<Derived>::Scalar>,Derived> operator-() const;

//     template<typename OtherDerived>
//     const CwiseBinaryOp<internal::scalar_sum_op<typename internal::traits<Derived>::Scalar>, Derived, OtherDerived>
//     operator+(const SparseMatrixBase<OtherDerived> &other) const;

//     template<typename OtherDerived>
//     const CwiseBinaryOp<internal::scalar_difference_op<typename internal::traits<Derived>::Scalar>, Derived, OtherDerived>
//     operator-(const SparseMatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    Derived& operator+=(const SparseMatrixBase<OtherDerived>& other);
    template<typename OtherDerived>
    Derived& operator-=(const SparseMatrixBase<OtherDerived>& other);

//     template<typename Lhs,typename Rhs>
//     Derived& operator+=(const Flagged<Product<Lhs,Rhs,CacheFriendlyProduct>, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit>& other);

    Derived& operator*=(const Scalar& other);
    Derived& operator/=(const Scalar& other);

    #define EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE \
      CwiseBinaryOp< \
        internal::scalar_product_op< \
          typename internal::scalar_product_traits< \
            typename internal::traits<Derived>::Scalar, \
            typename internal::traits<OtherDerived>::Scalar \
          >::ReturnType \
        >, \
        Derived, \
        OtherDerived \
      >

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
    cwiseProduct(const MatrixBase<OtherDerived> &other) const;

//     const SparseCwiseUnaryOp<internal::scalar_multiple_op<typename internal::traits<Derived>::Scalar>, Derived>
//     operator*(const Scalar& scalar) const;
//     const SparseCwiseUnaryOp<internal::scalar_quotient1_op<typename internal::traits<Derived>::Scalar>, Derived>
//     operator/(const Scalar& scalar) const;

//     inline friend const SparseCwiseUnaryOp<internal::scalar_multiple_op<typename internal::traits<Derived>::Scalar>, Derived>
//     operator*(const Scalar& scalar, const SparseMatrixBase& matrix)
//     { return matrix*scalar; }


    // sparse * sparse
    template<typename OtherDerived>
    const typename SparseSparseProductReturnType<Derived,OtherDerived>::Type
    operator*(const SparseMatrixBase<OtherDerived> &other) const;

    // sparse * diagonal
    template<typename OtherDerived>
    const SparseDiagonalProduct<Derived,OtherDerived>
    operator*(const DiagonalBase<OtherDerived> &other) const;

    // diagonal * sparse
    template<typename OtherDerived> friend
    const SparseDiagonalProduct<OtherDerived,Derived>
    operator*(const DiagonalBase<OtherDerived> &lhs, const SparseMatrixBase& rhs)
    { return SparseDiagonalProduct<OtherDerived,Derived>(lhs.derived(), rhs.derived()); }

    /** dense * sparse (return a dense object unless it is an outer product) */
    template<typename OtherDerived> friend
    const typename DenseSparseProductReturnType<OtherDerived,Derived>::Type
    operator*(const MatrixBase<OtherDerived>& lhs, const Derived& rhs)
    { return typename DenseSparseProductReturnType<OtherDerived,Derived>::Type(lhs.derived(),rhs); }

    /** sparse * dense (returns a dense object unless it is an outer product) */
    template<typename OtherDerived>
    const typename SparseDenseProductReturnType<Derived,OtherDerived>::Type
    operator*(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    Derived& operator*=(const SparseMatrixBase<OtherDerived>& other);

    #ifdef EIGEN2_SUPPORT
    // deprecated
    template<typename OtherDerived>
    typename internal::plain_matrix_type_column_major<OtherDerived>::type
    solveTriangular(const MatrixBase<OtherDerived>& other) const;

    // deprecated
    template<typename OtherDerived>
    void solveTriangularInPlace(MatrixBase<OtherDerived>& other) const;
//     template<typename OtherDerived>
//     void solveTriangularInPlace(SparseMatrixBase<OtherDerived>& other) const;
    #endif // EIGEN2_SUPPORT

    template<int Mode>
    inline const SparseTriangularView<Derived, Mode> triangularView() const;

    template<unsigned int UpLo> inline const SparseSelfAdjointView<Derived, UpLo> selfadjointView() const;
    template<unsigned int UpLo> inline SparseSelfAdjointView<Derived, UpLo> selfadjointView();

    template<typename OtherDerived> Scalar dot(const MatrixBase<OtherDerived>& other) const;
    template<typename OtherDerived> Scalar dot(const SparseMatrixBase<OtherDerived>& other) const;
    RealScalar squaredNorm() const;
    RealScalar norm()  const;
//     const PlainObject normalized() const;
//     void normalize();

    Transpose<Derived> transpose() { return derived(); }
    const Transpose<const Derived> transpose() const { return derived(); }
    // void transposeInPlace();
    const AdjointReturnType adjoint() const { return transpose(); }

    // sub-vector
    SparseInnerVectorSet<Derived,1> row(Index i);
    const SparseInnerVectorSet<Derived,1> row(Index i) const;
    SparseInnerVectorSet<Derived,1> col(Index j);
    const SparseInnerVectorSet<Derived,1> col(Index j) const;
    SparseInnerVectorSet<Derived,1> innerVector(Index outer);
    const SparseInnerVectorSet<Derived,1> innerVector(Index outer) const;

    // set of sub-vectors
    SparseInnerVectorSet<Derived,Dynamic> subrows(Index start, Index size);
    const SparseInnerVectorSet<Derived,Dynamic> subrows(Index start, Index size) const;
    SparseInnerVectorSet<Derived,Dynamic> subcols(Index start, Index size);
    const SparseInnerVectorSet<Derived,Dynamic> subcols(Index start, Index size) const;
    SparseInnerVectorSet<Derived,Dynamic> innerVectors(Index outerStart, Index outerSize);
    const SparseInnerVectorSet<Derived,Dynamic> innerVectors(Index outerStart, Index outerSize) const;

//     typename BlockReturnType<Derived>::Type block(int startRow, int startCol, int blockRows, int blockCols);
//     const typename BlockReturnType<Derived>::Type
//     block(int startRow, int startCol, int blockRows, int blockCols) const;
//
//     typename BlockReturnType<Derived>::SubVectorType segment(int start, int size);
//     const typename BlockReturnType<Derived>::SubVectorType segment(int start, int size) const;
//
//     typename BlockReturnType<Derived,Dynamic>::SubVectorType start(int size);
//     const typename BlockReturnType<Derived,Dynamic>::SubVectorType start(int size) const;
//
//     typename BlockReturnType<Derived,Dynamic>::SubVectorType end(int size);
//     const typename BlockReturnType<Derived,Dynamic>::SubVectorType end(int size) const;
//
//     template<int BlockRows, int BlockCols>
//     typename BlockReturnType<Derived, BlockRows, BlockCols>::Type block(int startRow, int startCol);
//     template<int BlockRows, int BlockCols>
//     const typename BlockReturnType<Derived, BlockRows, BlockCols>::Type block(int startRow, int startCol) const;

//     template<int Size> typename BlockReturnType<Derived,Size>::SubVectorType start(void);
//     template<int Size> const typename BlockReturnType<Derived,Size>::SubVectorType start() const;

//     template<int Size> typename BlockReturnType<Derived,Size>::SubVectorType end();
//     template<int Size> const typename BlockReturnType<Derived,Size>::SubVectorType end() const;

//     template<int Size> typename BlockReturnType<Derived,Size>::SubVectorType segment(int start);
//     template<int Size> const typename BlockReturnType<Derived,Size>::SubVectorType segment(int start) const;

//     Diagonal<Derived> diagonal();
//     const Diagonal<Derived> diagonal() const;

//     template<unsigned int Mode> Part<Derived, Mode> part();
//     template<unsigned int Mode> const Part<Derived, Mode> part() const;


//     static const ConstantReturnType Constant(int rows, int cols, const Scalar& value);
//     static const ConstantReturnType Constant(int size, const Scalar& value);
//     static const ConstantReturnType Constant(const Scalar& value);

//     template<typename CustomNullaryOp>
//     static const CwiseNullaryOp<CustomNullaryOp, Derived> NullaryExpr(int rows, int cols, const CustomNullaryOp& func);
//     template<typename CustomNullaryOp>
//     static const CwiseNullaryOp<CustomNullaryOp, Derived> NullaryExpr(int size, const CustomNullaryOp& func);
//     template<typename CustomNullaryOp>
//     static const CwiseNullaryOp<CustomNullaryOp, Derived> NullaryExpr(const CustomNullaryOp& func);

//     static const ConstantReturnType Zero(int rows, int cols);
//     static const ConstantReturnType Zero(int size);
//     static const ConstantReturnType Zero();
//     static const ConstantReturnType Ones(int rows, int cols);
//     static const ConstantReturnType Ones(int size);
//     static const ConstantReturnType Ones();
//     static const IdentityReturnType Identity();
//     static const IdentityReturnType Identity(int rows, int cols);
//     static const BasisReturnType Unit(int size, int i);
//     static const BasisReturnType Unit(int i);
//     static const BasisReturnType UnitX();
//     static const BasisReturnType UnitY();
//     static const BasisReturnType UnitZ();
//     static const BasisReturnType UnitW();

//     const DiagonalMatrix<Derived> asDiagonal() const;

//     Derived& setConstant(const Scalar& value);
//     Derived& setZero();
//     Derived& setOnes();
//     Derived& setRandom();
//     Derived& setIdentity();

      /** \internal use operator= */
      template<typename DenseDerived>
      void evalTo(MatrixBase<DenseDerived>& dst) const
      {
        dst.setZero();
        for (Index j=0; j<outerSize(); ++j)
          for (typename Derived::InnerIterator i(derived(),j); i; ++i)
            dst.coeffRef(i.row(),i.col()) = i.value();
      }

      Matrix<Scalar,RowsAtCompileTime,ColsAtCompileTime> toDense() const
      {
        return derived();
      }

    template<typename OtherDerived>
    bool isApprox(const SparseMatrixBase<OtherDerived>& other,
                  RealScalar prec = NumTraits<Scalar>::dummy_precision()) const
    { return toDense().isApprox(other.toDense(),prec); }

    template<typename OtherDerived>
    bool isApprox(const MatrixBase<OtherDerived>& other,
                  RealScalar prec = NumTraits<Scalar>::dummy_precision()) const
    { return toDense().isApprox(other,prec); }
//     bool isMuchSmallerThan(const RealScalar& other,
//                            RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     template<typename OtherDerived>
//     bool isMuchSmallerThan(const MatrixBase<OtherDerived>& other,
//                            RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;

//     bool isApproxToConstant(const Scalar& value, RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     bool isZero(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     bool isOnes(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     bool isIdentity(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     bool isDiagonal(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;

//     bool isUpper(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     bool isLower(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;

//     template<typename OtherDerived>
//     bool isOrthogonal(const MatrixBase<OtherDerived>& other,
//                       RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;
//     bool isUnitary(RealScalar prec = NumTraits<Scalar>::dummy_precision()) const;

//     template<typename OtherDerived>
//     inline bool operator==(const MatrixBase<OtherDerived>& other) const
//     { return (cwise() == other).all(); }

//     template<typename OtherDerived>
//     inline bool operator!=(const MatrixBase<OtherDerived>& other) const
//     { return (cwise() != other).any(); }


//     template<typename NewType>
//     const SparseCwiseUnaryOp<internal::scalar_cast_op<typename internal::traits<Derived>::Scalar, NewType>, Derived> cast() const;

    /** \returns the matrix or vector obtained by evaluating this expression.
      *
      * Notice that in the case of a plain matrix or vector (not an expression) this function just returns
      * a const reference, in order to avoid a useless copy.
      */
    inline const typename internal::eval<Derived>::type eval() const
    { return typename internal::eval<Derived>::type(derived()); }

//     template<typename OtherDerived>
//     void swap(MatrixBase<OtherDerived> const & other);

//     template<unsigned int Added>
//     const SparseFlagged<Derived, Added, 0> marked() const;
//     const Flagged<Derived, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit> lazy() const;

    /** \returns number of elements to skip to pass from one row (resp. column) to another
      * for a row-major (resp. column-major) matrix.
      * Combined with coeffRef() and the \ref flags flags, it allows a direct access to the data
      * of the underlying matrix.
      */
//     inline int stride(void) const { return derived().stride(); }

// FIXME
//     ConjugateReturnType conjugate() const;
//     const RealReturnType real() const;
//     const ImagReturnType imag() const;

//     template<typename CustomUnaryOp>
//     const SparseCwiseUnaryOp<CustomUnaryOp, Derived> unaryExpr(const CustomUnaryOp& func = CustomUnaryOp()) const;

//     template<typename CustomBinaryOp, typename OtherDerived>
//     const CwiseBinaryOp<CustomBinaryOp, Derived, OtherDerived>
//     binaryExpr(const MatrixBase<OtherDerived> &other, const CustomBinaryOp& func = CustomBinaryOp()) const;


    Scalar sum() const;
//     Scalar trace() const;

//     typename internal::traits<Derived>::Scalar minCoeff() const;
//     typename internal::traits<Derived>::Scalar maxCoeff() const;

//     typename internal::traits<Derived>::Scalar minCoeff(int* row, int* col = 0) const;
//     typename internal::traits<Derived>::Scalar maxCoeff(int* row, int* col = 0) const;

//     template<typename BinaryOp>
//     typename internal::result_of<BinaryOp(typename internal::traits<Derived>::Scalar)>::type
//     redux(const BinaryOp& func) const;

//     template<typename Visitor>
//     void visit(Visitor& func) const;


//     const SparseCwise<Derived> cwise() const;
//     SparseCwise<Derived> cwise();

//     inline const WithFormat<Derived> format(const IOFormat& fmt) const;

/////////// Array module ///////////
    /*
    bool all(void) const;
    bool any(void) const;

    const VectorwiseOp<Derived,Horizontal> rowwise() const;
    const VectorwiseOp<Derived,Vertical> colwise() const;

    static const CwiseNullaryOp<internal::scalar_random_op<Scalar>,Derived> Random(int rows, int cols);
    static const CwiseNullaryOp<internal::scalar_random_op<Scalar>,Derived> Random(int size);
    static const CwiseNullaryOp<internal::scalar_random_op<Scalar>,Derived> Random();

    template<typename ThenDerived,typename ElseDerived>
    const Select<Derived,ThenDerived,ElseDerived>
    select(const MatrixBase<ThenDerived>& thenMatrix,
           const MatrixBase<ElseDerived>& elseMatrix) const;

    template<typename ThenDerived>
    inline const Select<Derived,ThenDerived, typename ThenDerived::ConstantReturnType>
    select(const MatrixBase<ThenDerived>& thenMatrix, typename ThenDerived::Scalar elseScalar) const;

    template<typename ElseDerived>
    inline const Select<Derived, typename ElseDerived::ConstantReturnType, ElseDerived >
    select(typename ElseDerived::Scalar thenScalar, const MatrixBase<ElseDerived>& elseMatrix) const;

    template<int p> RealScalar lpNorm() const;
    */


//     template<typename OtherDerived>
//     Scalar dot(const MatrixBase<OtherDerived>& other) const
//     {
//       EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
//       EIGEN_STATIC_ASSERT_VECTOR_ONLY(OtherDerived)
//       EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
//         YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)
//
//       eigen_assert(derived().size() == other.size());
//       // short version, but the assembly looks more complicated because
//       // of the CwiseBinaryOp iterator complexity
//       // return res = (derived().cwise() * other.derived().conjugate()).sum();
//
//       // optimized, generic version
//       typename Derived::InnerIterator i(derived(),0);
//       typename OtherDerived::InnerIterator j(other.derived(),0);
//       Scalar res = 0;
//       while (i && j)
//       {
//         if (i.index()==j.index())
//         {
// //           std::cerr << i.value() << " * " << j.value() << "\n";
//           res += i.value() * internal::conj(j.value());
//           ++i; ++j;
//         }
//         else if (i.index()<j.index())
//           ++i;
//         else
//           ++j;
//       }
//       return res;
//     }
//
//     Scalar sum() const
//     {
//       Scalar res = 0;
//       for (typename Derived::InnerIterator iter(*this,0); iter; ++iter)
//       {
//         res += iter.value();
//       }
//       return res;
//     }

  protected:

    bool m_isRValue;
};

#endif // EIGEN_SPARSEMATRIXBASE_H

// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_MATRIXBASE_H
#define EIGEN_MATRIXBASE_H

/** \class MatrixBase
  *
  * \brief Base class for all matrices, vectors, and expressions
  *
  * This class is the base that is inherited by all matrix, vector, and expression
  * types. Most of the Eigen API is contained in this class. Other important classes for
  * the Eigen API are Matrix, Cwise, and PartialRedux.
  *
  * Note that some methods are defined in the \ref Array module.
  *
  * \param Derived is the derived type, e.g. a matrix type, or an expression, etc.
  *
  * When writing a function taking Eigen objects as argument, if you want your function
  * to take as argument any matrix, vector, or expression, just let it take a
  * MatrixBase argument. As an example, here is a function printFirstRow which, given
  * a matrix, vector, or expression \a x, prints the first row of \a x.
  *
  * \code
    template<typename Derived>
    void printFirstRow(const Eigen::MatrixBase<Derived>& x)
    {
      cout << x.row(0) << endl;
    }
  * \endcode
  *
  */
template<typename Derived> class MatrixBase
{
  public:

#ifndef EIGEN_PARSED_BY_DOXYGEN
    class InnerIterator;

    typedef typename ei_traits<Derived>::Scalar Scalar;
    typedef typename ei_packet_traits<Scalar>::type PacketScalar;
#endif // not EIGEN_PARSED_BY_DOXYGEN

    enum {

      RowsAtCompileTime = ei_traits<Derived>::RowsAtCompileTime,
        /**< The number of rows at compile-time. This is just a copy of the value provided
          * by the \a Derived type. If a value is not known at compile-time,
          * it is set to the \a Dynamic constant.
          * \sa MatrixBase::rows(), MatrixBase::cols(), ColsAtCompileTime, SizeAtCompileTime */

      ColsAtCompileTime = ei_traits<Derived>::ColsAtCompileTime,
        /**< The number of columns at compile-time. This is just a copy of the value provided
          * by the \a Derived type. If a value is not known at compile-time,
          * it is set to the \a Dynamic constant.
          * \sa MatrixBase::rows(), MatrixBase::cols(), RowsAtCompileTime, SizeAtCompileTime */


      SizeAtCompileTime = (ei_size_at_compile_time<ei_traits<Derived>::RowsAtCompileTime,
                                                   ei_traits<Derived>::ColsAtCompileTime>::ret),
        /**< This is equal to the number of coefficients, i.e. the number of
          * rows times the number of columns, or to \a Dynamic if this is not
          * known at compile-time. \sa RowsAtCompileTime, ColsAtCompileTime */

      MaxRowsAtCompileTime = ei_traits<Derived>::MaxRowsAtCompileTime,
        /**< This value is equal to the maximum possible number of rows that this expression
          * might have. If this expression might have an arbitrarily high number of rows,
          * this value is set to \a Dynamic.
          *
          * This value is useful to know when evaluating an expression, in order to determine
          * whether it is possible to avoid doing a dynamic memory allocation.
          *
          * \sa RowsAtCompileTime, MaxColsAtCompileTime, MaxSizeAtCompileTime
          */

      MaxColsAtCompileTime = ei_traits<Derived>::MaxColsAtCompileTime,
        /**< This value is equal to the maximum possible number of columns that this expression
          * might have. If this expression might have an arbitrarily high number of columns,
          * this value is set to \a Dynamic.
          *
          * This value is useful to know when evaluating an expression, in order to determine
          * whether it is possible to avoid doing a dynamic memory allocation.
          *
          * \sa ColsAtCompileTime, MaxRowsAtCompileTime, MaxSizeAtCompileTime
          */

      MaxSizeAtCompileTime = (ei_size_at_compile_time<ei_traits<Derived>::MaxRowsAtCompileTime,
                                                      ei_traits<Derived>::MaxColsAtCompileTime>::ret),
        /**< This value is equal to the maximum possible number of coefficients that this expression
          * might have. If this expression might have an arbitrarily high number of coefficients,
          * this value is set to \a Dynamic.
          *
          * This value is useful to know when evaluating an expression, in order to determine
          * whether it is possible to avoid doing a dynamic memory allocation.
          *
          * \sa SizeAtCompileTime, MaxRowsAtCompileTime, MaxColsAtCompileTime
          */

      IsVectorAtCompileTime = ei_traits<Derived>::RowsAtCompileTime == 1
                           || ei_traits<Derived>::ColsAtCompileTime == 1,
        /**< This is set to true if either the number of rows or the number of
          * columns is known at compile-time to be equal to 1. Indeed, in that case,
          * we are dealing with a column-vector (if there is only one column) or with
          * a row-vector (if there is only one row). */

      Flags = ei_traits<Derived>::Flags,
        /**< This stores expression \ref flags flags which may or may not be inherited by new expressions
          * constructed from this one. See the \ref flags "list of flags".
          */

      CoeffReadCost = ei_traits<Derived>::CoeffReadCost
        /**< This is a rough measure of how expensive it is to read one coefficient from
          * this expression.
          */
    };

    /** Default constructor. Just checks at compile-time for self-consistency of the flags. */
    MatrixBase()
    {
      ei_assert(ei_are_flags_consistent<Flags>::ret);
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    /** This is the "real scalar" type; if the \a Scalar type is already real numbers
      * (e.g. int, float or double) then \a RealScalar is just the same as \a Scalar. If
      * \a Scalar is \a std::complex<T> then RealScalar is \a T.
      *
      * \sa class NumTraits
      */
    typedef typename NumTraits<Scalar>::Real RealScalar;

    /** type of the equivalent square matrix */
    typedef Matrix<Scalar,EIGEN_ENUM_MAX(RowsAtCompileTime,ColsAtCompileTime),
                          EIGEN_ENUM_MAX(RowsAtCompileTime,ColsAtCompileTime)> SquareMatrixType;
#endif // not EIGEN_PARSED_BY_DOXYGEN

    /** \returns the number of rows. \sa cols(), RowsAtCompileTime */
    inline int rows() const { return derived().rows(); }
    /** \returns the number of columns. \sa rows(), ColsAtCompileTime*/
    inline int cols() const { return derived().cols(); }
    /** \returns the number of coefficients, which is \a rows()*cols().
      * \sa rows(), cols(), SizeAtCompileTime. */
    inline int size() const { return rows() * cols(); }
    /** \returns the number of nonzero coefficients which is in practice the number
      * of stored coefficients. */
    inline int nonZeros() const { return derived.nonZeros(); }
    /** \returns true if either the number of rows or the number of columns is equal to 1.
      * In other words, this function returns
      * \code rows()==1 || cols()==1 \endcode
      * \sa rows(), cols(), IsVectorAtCompileTime. */
    inline bool isVector() const { return rows()==1 || cols()==1; }
    /** \returns the size of the storage major dimension,
      * i.e., the number of columns for a columns major matrix, and the number of rows otherwise */
    int outerSize() const { return (int(Flags)&RowMajorBit) ? this->rows() : this->cols(); }
    /** \returns the size of the inner dimension according to the storage order,
      * i.e., the number of rows for a columns major matrix, and the number of cols otherwise */
    int innerSize() const { return (int(Flags)&RowMajorBit) ? this->cols() : this->rows(); }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    /** \internal the plain matrix type corresponding to this expression. Note that is not necessarily
      * exactly the return type of eval(): in the case of plain matrices, the return type of eval() is a const
      * reference to a matrix, not a matrix! It guaranteed however, that the return type of eval() is either
      * PlainMatrixType or const PlainMatrixType&.
      */
    typedef typename ei_plain_matrix_type<Derived>::type PlainMatrixType;
    /** \internal the column-major plain matrix type corresponding to this expression. Note that is not necessarily
      * exactly the return type of eval(): in the case of plain matrices, the return type of eval() is a const
      * reference to a matrix, not a matrix!
      * The only difference from PlainMatrixType is that PlainMatrixType_ColMajor is guaranteed to be column-major.
      */
    typedef typename ei_plain_matrix_type<Derived>::type PlainMatrixType_ColMajor;

    /** \internal Represents a matrix with all coefficients equal to one another*/
    typedef CwiseNullaryOp<ei_scalar_constant_op<Scalar>,Derived> ConstantReturnType;
    /** \internal Represents a scalar multiple of a matrix */
    typedef CwiseUnaryOp<ei_scalar_multiple_op<Scalar>, Derived> ScalarMultipleReturnType;
    /** \internal Represents a quotient of a matrix by a scalar*/
    typedef CwiseUnaryOp<ei_scalar_quotient1_op<Scalar>, Derived> ScalarQuotient1ReturnType;
    /** \internal the return type of MatrixBase::conjugate() */
    typedef typename ei_meta_if<NumTraits<Scalar>::IsComplex,
                        const CwiseUnaryOp<ei_scalar_conjugate_op<Scalar>, Derived>,
                        const Derived&
                     >::ret ConjugateReturnType;
    /** \internal the return type of MatrixBase::real() */
    typedef CwiseUnaryOp<ei_scalar_real_op<Scalar>, Derived> RealReturnType;
    /** \internal the return type of MatrixBase::imag() */
    typedef CwiseUnaryOp<ei_scalar_imag_op<Scalar>, Derived> ImagReturnType;
    /** \internal the return type of MatrixBase::adjoint() */
    typedef Eigen::Transpose<NestByValue<typename ei_cleantype<ConjugateReturnType>::type> >
            AdjointReturnType;
    /** \internal the return type of MatrixBase::eigenvalues() */
    typedef Matrix<typename NumTraits<typename ei_traits<Derived>::Scalar>::Real, ei_traits<Derived>::ColsAtCompileTime, 1> EigenvaluesReturnType;
    /** \internal expression tyepe of a column */
    typedef Block<Derived, ei_traits<Derived>::RowsAtCompileTime, 1> ColXpr;
    /** \internal expression tyepe of a column */
    typedef Block<Derived, 1, ei_traits<Derived>::ColsAtCompileTime> RowXpr;
    /** \internal the return type of identity */
    typedef CwiseNullaryOp<ei_scalar_identity_op<Scalar>,Derived> IdentityReturnType;
    /** \internal the return type of unit vectors */
    typedef Block<CwiseNullaryOp<ei_scalar_identity_op<Scalar>, SquareMatrixType>,
                  ei_traits<Derived>::RowsAtCompileTime,
                  ei_traits<Derived>::ColsAtCompileTime> BasisReturnType;
#endif // not EIGEN_PARSED_BY_DOXYGEN


    /** Copies \a other into *this. \returns a reference to *this. */
    template<typename OtherDerived>
    Derived& operator=(const MatrixBase<OtherDerived>& other);

    /** Special case of the template operator=, in order to prevent the compiler
      * from generating a default operator= (issue hit with g++ 4.1)
      */
    inline Derived& operator=(const MatrixBase& other)
    {
      return this->operator=<Derived>(other);
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    /** Copies \a other into *this without evaluating other. \returns a reference to *this. */
    template<typename OtherDerived>
    Derived& lazyAssign(const MatrixBase<OtherDerived>& other);

    /** Overloaded for cache friendly product evaluation */
    template<typename Lhs, typename Rhs>
    Derived& lazyAssign(const Product<Lhs,Rhs,CacheFriendlyProduct>& product);

    /** Overloaded for cache friendly product evaluation */
    template<typename OtherDerived>
    Derived& lazyAssign(const Flagged<OtherDerived, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit>& other)
    { return lazyAssign(other._expression()); }
#endif // not EIGEN_PARSED_BY_DOXYGEN

    CommaInitializer<Derived> operator<< (const Scalar& s);

    template<typename OtherDerived>
    CommaInitializer<Derived> operator<< (const MatrixBase<OtherDerived>& other);

    const Scalar coeff(int row, int col) const;
    const Scalar operator()(int row, int col) const;

    Scalar& coeffRef(int row, int col);
    Scalar& operator()(int row, int col);

    const Scalar coeff(int index) const;
    const Scalar operator[](int index) const;
    const Scalar operator()(int index) const;

    Scalar& coeffRef(int index);
    Scalar& operator[](int index);
    Scalar& operator()(int index);

#ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename OtherDerived>
    void copyCoeff(int row, int col, const MatrixBase<OtherDerived>& other);
    template<typename OtherDerived>
    void copyCoeff(int index, const MatrixBase<OtherDerived>& other);
    template<typename OtherDerived, int StoreMode, int LoadMode>
    void copyPacket(int row, int col, const MatrixBase<OtherDerived>& other);
    template<typename OtherDerived, int StoreMode, int LoadMode>
    void copyPacket(int index, const MatrixBase<OtherDerived>& other);
#endif // not EIGEN_PARSED_BY_DOXYGEN

    template<int LoadMode>
    PacketScalar packet(int row, int col) const;
    template<int StoreMode>
    void writePacket(int row, int col, const PacketScalar& x);

    template<int LoadMode>
    PacketScalar packet(int index) const;
    template<int StoreMode>
    void writePacket(int index, const PacketScalar& x);

    const Scalar x() const;
    const Scalar y() const;
    const Scalar z() const;
    const Scalar w() const;
    Scalar& x();
    Scalar& y();
    Scalar& z();
    Scalar& w();


    const CwiseUnaryOp<ei_scalar_opposite_op<typename ei_traits<Derived>::Scalar>,Derived> operator-() const;

    template<typename OtherDerived>
    const CwiseBinaryOp<ei_scalar_sum_op<typename ei_traits<Derived>::Scalar>, Derived, OtherDerived>
    operator+(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const CwiseBinaryOp<ei_scalar_difference_op<typename ei_traits<Derived>::Scalar>, Derived, OtherDerived>
    operator-(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    Derived& operator+=(const MatrixBase<OtherDerived>& other);
    template<typename OtherDerived>
    Derived& operator-=(const MatrixBase<OtherDerived>& other);

    template<typename Lhs,typename Rhs>
    Derived& operator+=(const Flagged<Product<Lhs,Rhs,CacheFriendlyProduct>, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit>& other);

    Derived& operator*=(const Scalar& other);
    Derived& operator/=(const Scalar& other);

    const ScalarMultipleReturnType operator*(const Scalar& scalar) const;
    const CwiseUnaryOp<ei_scalar_quotient1_op<typename ei_traits<Derived>::Scalar>, Derived>
    operator/(const Scalar& scalar) const;

    inline friend const CwiseUnaryOp<ei_scalar_multiple_op<typename ei_traits<Derived>::Scalar>, Derived>
    operator*(const Scalar& scalar, const MatrixBase& matrix)
    { return matrix*scalar; }


    template<typename OtherDerived>
    const typename ProductReturnType<Derived,OtherDerived>::Type
    operator*(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    Derived& operator*=(const MatrixBase<OtherDerived>& other);

    template<typename OtherDerived>
    typename ei_plain_matrix_type_column_major<OtherDerived>::type
		solveTriangular(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived>
    void solveTriangularInPlace(const MatrixBase<OtherDerived>& other) const;


    template<typename OtherDerived>
    Scalar dot(const MatrixBase<OtherDerived>& other) const;
    RealScalar squaredNorm() const;
    RealScalar norm()  const;
    const PlainMatrixType normalized() const;
    void normalize();

    Eigen::Transpose<Derived> transpose();
    const Eigen::Transpose<Derived> transpose() const;
    void transposeInPlace();
    const AdjointReturnType adjoint() const;


    RowXpr row(int i);
    const RowXpr row(int i) const;

    ColXpr col(int i);
    const ColXpr col(int i) const;

    Minor<Derived> minor(int row, int col);
    const Minor<Derived> minor(int row, int col) const;

    typename BlockReturnType<Derived>::Type block(int startRow, int startCol, int blockRows, int blockCols);
    const typename BlockReturnType<Derived>::Type
    block(int startRow, int startCol, int blockRows, int blockCols) const;

    typename BlockReturnType<Derived>::SubVectorType segment(int start, int size);
    const typename BlockReturnType<Derived>::SubVectorType segment(int start, int size) const;

    typename BlockReturnType<Derived,Dynamic>::SubVectorType start(int size);
    const typename BlockReturnType<Derived,Dynamic>::SubVectorType start(int size) const;

    typename BlockReturnType<Derived,Dynamic>::SubVectorType end(int size);
    const typename BlockReturnType<Derived,Dynamic>::SubVectorType end(int size) const;

    typename BlockReturnType<Derived>::Type corner(CornerType type, int cRows, int cCols);
    const typename BlockReturnType<Derived>::Type corner(CornerType type, int cRows, int cCols) const;

    template<int BlockRows, int BlockCols>
    typename BlockReturnType<Derived, BlockRows, BlockCols>::Type block(int startRow, int startCol);
    template<int BlockRows, int BlockCols>
    const typename BlockReturnType<Derived, BlockRows, BlockCols>::Type block(int startRow, int startCol) const;

    template<int CRows, int CCols>
    typename BlockReturnType<Derived, CRows, CCols>::Type corner(CornerType type);
    template<int CRows, int CCols>
    const typename BlockReturnType<Derived, CRows, CCols>::Type corner(CornerType type) const;

    template<int Size> typename BlockReturnType<Derived,Size>::SubVectorType start(void);
    template<int Size> const typename BlockReturnType<Derived,Size>::SubVectorType start() const;

    template<int Size> typename BlockReturnType<Derived,Size>::SubVectorType end();
    template<int Size> const typename BlockReturnType<Derived,Size>::SubVectorType end() const;

    template<int Size> typename BlockReturnType<Derived,Size>::SubVectorType segment(int start);
    template<int Size> const typename BlockReturnType<Derived,Size>::SubVectorType segment(int start) const;

    DiagonalCoeffs<Derived> diagonal();
    const DiagonalCoeffs<Derived> diagonal() const;

    template<unsigned int Mode> Part<Derived, Mode> part();
    template<unsigned int Mode> const Part<Derived, Mode> part() const;


    static const ConstantReturnType
    Constant(int rows, int cols, const Scalar& value);
    static const ConstantReturnType
    Constant(int size, const Scalar& value);
    static const ConstantReturnType
    Constant(const Scalar& value);

    template<typename CustomNullaryOp>
    static const CwiseNullaryOp<CustomNullaryOp, Derived>
    NullaryExpr(int rows, int cols, const CustomNullaryOp& func);
    template<typename CustomNullaryOp>
    static const CwiseNullaryOp<CustomNullaryOp, Derived>
    NullaryExpr(int size, const CustomNullaryOp& func);
    template<typename CustomNullaryOp>
    static const CwiseNullaryOp<CustomNullaryOp, Derived>
    NullaryExpr(const CustomNullaryOp& func);

    static const ConstantReturnType Zero(int rows, int cols);
    static const ConstantReturnType Zero(int size);
    static const ConstantReturnType Zero();
    static const ConstantReturnType Ones(int rows, int cols);
    static const ConstantReturnType Ones(int size);
    static const ConstantReturnType Ones();
    static const IdentityReturnType Identity();
    static const IdentityReturnType Identity(int rows, int cols);
    static const BasisReturnType Unit(int size, int i);
    static const BasisReturnType Unit(int i);
    static const BasisReturnType UnitX();
    static const BasisReturnType UnitY();
    static const BasisReturnType UnitZ();
    static const BasisReturnType UnitW();

    const DiagonalMatrix<Derived> asDiagonal() const;

    void fill(const Scalar& value);
    Derived& setConstant(const Scalar& value);
    Derived& setZero();
    Derived& setOnes();
    Derived& setRandom();
    Derived& setIdentity();


    template<typename OtherDerived>
    bool isApprox(const MatrixBase<OtherDerived>& other,
                  RealScalar prec = precision<Scalar>()) const;
    bool isMuchSmallerThan(const RealScalar& other,
                           RealScalar prec = precision<Scalar>()) const;
    template<typename OtherDerived>
    bool isMuchSmallerThan(const MatrixBase<OtherDerived>& other,
                           RealScalar prec = precision<Scalar>()) const;

    bool isApproxToConstant(const Scalar& value, RealScalar prec = precision<Scalar>()) const;
    bool isZero(RealScalar prec = precision<Scalar>()) const;
    bool isOnes(RealScalar prec = precision<Scalar>()) const;
    bool isIdentity(RealScalar prec = precision<Scalar>()) const;
    bool isDiagonal(RealScalar prec = precision<Scalar>()) const;

    bool isUpperTriangular(RealScalar prec = precision<Scalar>()) const;
    bool isLowerTriangular(RealScalar prec = precision<Scalar>()) const;

    template<typename OtherDerived>
    bool isOrthogonal(const MatrixBase<OtherDerived>& other,
                      RealScalar prec = precision<Scalar>()) const;
    bool isUnitary(RealScalar prec = precision<Scalar>()) const;

    template<typename OtherDerived>
    inline bool operator==(const MatrixBase<OtherDerived>& other) const
    { return (cwise() == other).all(); }

    template<typename OtherDerived>
    inline bool operator!=(const MatrixBase<OtherDerived>& other) const
    { return (cwise() != other).any(); }


    template<typename NewType>
    const CwiseUnaryOp<ei_scalar_cast_op<typename ei_traits<Derived>::Scalar, NewType>, Derived> cast() const;

    /** \returns the matrix or vector obtained by evaluating this expression.
      *
      * Notice that in the case of a plain matrix or vector (not an expression) this function just returns
      * a const reference, in order to avoid a useless copy.
      */
    EIGEN_STRONG_INLINE const typename ei_eval<Derived>::type eval() const
    { return typename ei_eval<Derived>::type(derived()); }

    template<typename OtherDerived>
    void swap(const MatrixBase<OtherDerived>& other);

    template<unsigned int Added>
    const Flagged<Derived, Added, 0> marked() const;
    const Flagged<Derived, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit> lazy() const;

    /** \returns number of elements to skip to pass from one row (resp. column) to another
      * for a row-major (resp. column-major) matrix.
      * Combined with coeffRef() and the \ref flags flags, it allows a direct access to the data
      * of the underlying matrix.
      */
    inline int stride(void) const { return derived().stride(); }

    inline const NestByValue<Derived> nestByValue() const;


    ConjugateReturnType conjugate() const;
    const RealReturnType real() const;
    const ImagReturnType imag() const;

    template<typename CustomUnaryOp>
    const CwiseUnaryOp<CustomUnaryOp, Derived> unaryExpr(const CustomUnaryOp& func = CustomUnaryOp()) const;

    template<typename CustomBinaryOp, typename OtherDerived>
    const CwiseBinaryOp<CustomBinaryOp, Derived, OtherDerived>
    binaryExpr(const MatrixBase<OtherDerived> &other, const CustomBinaryOp& func = CustomBinaryOp()) const;


    Scalar sum() const;
    Scalar trace() const;

    typename ei_traits<Derived>::Scalar minCoeff() const;
    typename ei_traits<Derived>::Scalar maxCoeff() const;

    typename ei_traits<Derived>::Scalar minCoeff(int* row, int* col = 0) const;
    typename ei_traits<Derived>::Scalar maxCoeff(int* row, int* col = 0) const;

    template<typename BinaryOp>
    typename ei_result_of<BinaryOp(typename ei_traits<Derived>::Scalar)>::type
    redux(const BinaryOp& func) const;

    template<typename Visitor>
    void visit(Visitor& func) const;

#ifndef EIGEN_PARSED_BY_DOXYGEN
    inline const Derived& derived() const { return *static_cast<const Derived*>(this); }
    inline Derived& derived() { return *static_cast<Derived*>(this); }
    inline Derived& const_cast_derived() const
    { return *static_cast<Derived*>(const_cast<MatrixBase*>(this)); }
#endif // not EIGEN_PARSED_BY_DOXYGEN

    const Cwise<Derived> cwise() const;
    Cwise<Derived> cwise();

    inline const WithFormat<Derived> format(const IOFormat& fmt) const;

/////////// Array module ///////////

    bool all(void) const;
    bool any(void) const;
    int count() const;

    const PartialRedux<Derived,Horizontal> rowwise() const;
    const PartialRedux<Derived,Vertical> colwise() const;

    static const CwiseNullaryOp<ei_scalar_random_op<Scalar>,Derived> Random(int rows, int cols);
    static const CwiseNullaryOp<ei_scalar_random_op<Scalar>,Derived> Random(int size);
    static const CwiseNullaryOp<ei_scalar_random_op<Scalar>,Derived> Random();

    template<typename ThenDerived,typename ElseDerived>
    const Select<Derived,ThenDerived,ElseDerived>
    select(const MatrixBase<ThenDerived>& thenMatrix,
           const MatrixBase<ElseDerived>& elseMatrix) const;

    template<typename ThenDerived>
    inline const Select<Derived,ThenDerived, NestByValue<typename ThenDerived::ConstantReturnType> >
    select(const MatrixBase<ThenDerived>& thenMatrix, typename ThenDerived::Scalar elseScalar) const;

    template<typename ElseDerived>
    inline const Select<Derived, NestByValue<typename ElseDerived::ConstantReturnType>, ElseDerived >
    select(typename ElseDerived::Scalar thenScalar, const MatrixBase<ElseDerived>& elseMatrix) const;

    template<int p> RealScalar lpNorm() const;

/////////// LU module ///////////

    const LU<PlainMatrixType> lu() const;
    const PlainMatrixType inverse() const;
    void computeInverse(PlainMatrixType *result) const;
    Scalar determinant() const;

/////////// Cholesky module ///////////

    const LLT<PlainMatrixType>  llt() const;
    const LDLT<PlainMatrixType> ldlt() const;

/////////// QR module ///////////

    const QR<PlainMatrixType> qr() const;

    EigenvaluesReturnType eigenvalues() const;
    RealScalar operatorNorm() const;

/////////// SVD module ///////////

    SVD<PlainMatrixType> svd() const;

/////////// Geometry module ///////////

    template<typename OtherDerived>
    PlainMatrixType cross(const MatrixBase<OtherDerived>& other) const;
    PlainMatrixType unitOrthogonal(void) const;
    Matrix<Scalar,3,1> eulerAngles(int a0, int a1, int a2) const;

/////////// Sparse module ///////////

    // dense = spasre * dense
    template<typename Derived1, typename Derived2>
    Derived& lazyAssign(const SparseProduct<Derived1,Derived2,SparseTimeDenseProduct>& product);
    // dense = dense * spasre
    template<typename Derived1, typename Derived2>
    Derived& lazyAssign(const SparseProduct<Derived1,Derived2,DenseTimeSparseProduct>& product);

    #ifdef EIGEN_MATRIXBASE_PLUGIN
    #include EIGEN_MATRIXBASE_PLUGIN
    #endif
};

#endif // EIGEN_MATRIXBASE_H

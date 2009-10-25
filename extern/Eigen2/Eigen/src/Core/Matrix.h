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

#ifndef EIGEN_MATRIX_H
#define EIGEN_MATRIX_H


/** \class Matrix
  *
  * \brief The matrix class, also used for vectors and row-vectors
  *
  * The %Matrix class is the work-horse for all \em dense (\ref dense "note") matrices and vectors within Eigen.
  * Vectors are matrices with one column, and row-vectors are matrices with one row.
  *
  * The %Matrix class encompasses \em both fixed-size and dynamic-size objects (\ref fixedsize "note").
  *
  * The first three template parameters are required:
  * \param _Scalar Numeric type, i.e. float, double, int
  * \param _Rows Number of rows, or \b Dynamic
  * \param _Cols Number of columns, or \b Dynamic
  *
  * The remaining template parameters are optional -- in most cases you don't have to worry about them.
  * \param _Options A combination of either \b RowMajor or \b ColMajor, and of either
  *                 \b AutoAlign or \b DontAlign.
  *                 The former controls storage order, and defaults to column-major. The latter controls alignment, which is required
  *                 for vectorization. It defaults to aligning matrices except for fixed sizes that aren't a multiple of the packet size.
  * \param _MaxRows Maximum number of rows. Defaults to \a _Rows (\ref maxrows "note").
  * \param _MaxCols Maximum number of columns. Defaults to \a _Cols (\ref maxrows "note").
  *
  * Eigen provides a number of typedefs covering the usual cases. Here are some examples:
  *
  * \li \c Matrix2d is a 2x2 square matrix of doubles (\c Matrix<double, 2, 2>)
  * \li \c Vector4f is a vector of 4 floats (\c Matrix<float, 4, 1>)
  * \li \c RowVector3i is a row-vector of 3 ints (\c Matrix<int, 1, 3>)
  *
  * \li \c MatrixXf is a dynamic-size matrix of floats (\c Matrix<float, Dynamic, Dynamic>)
  * \li \c VectorXf is a dynamic-size vector of floats (\c Matrix<float, Dynamic, 1>)
  *
  * See \link matrixtypedefs this page \endlink for a complete list of predefined \em %Matrix and \em Vector typedefs.
  *
  * You can access elements of vectors and matrices using normal subscripting:
  *
  * \code
  * Eigen::VectorXd v(10);
  * v[0] = 0.1;
  * v[1] = 0.2;
  * v(0) = 0.3;
  * v(1) = 0.4;
  *
  * Eigen::MatrixXi m(10, 10);
  * m(0, 1) = 1;
  * m(0, 2) = 2;
  * m(0, 3) = 3;
  * \endcode
  *
  * <i><b>Some notes:</b></i>
  *
  * <dl>
  * <dt><b>\anchor dense Dense versus sparse:</b></dt>
  * <dd>This %Matrix class handles dense, not sparse matrices and vectors. For sparse matrices and vectors, see the Sparse module.
  *
  * Dense matrices and vectors are plain usual arrays of coefficients. All the coefficients are stored, in an ordinary contiguous array.
  * This is unlike Sparse matrices and vectors where the coefficients are stored as a list of nonzero coefficients.</dd>
  *
  * <dt><b>\anchor fixedsize Fixed-size versus dynamic-size:</b></dt>
  * <dd>Fixed-size means that the numbers of rows and columns are known are compile-time. In this case, Eigen allocates the array
  * of coefficients as a fixed-size array, as a class member. This makes sense for very small matrices, typically up to 4x4, sometimes up
  * to 16x16. Larger matrices should be declared as dynamic-size even if one happens to know their size at compile-time.
  *
  * Dynamic-size means that the numbers of rows or columns are not necessarily known at compile-time. In this case they are runtime
  * variables, and the array of coefficients is allocated dynamically on the heap.
  *
  * Note that \em dense matrices, be they Fixed-size or Dynamic-size, <em>do not</em> expand dynamically in the sense of a std::map.
  * If you want this behavior, see the Sparse module.</dd>
  *
  * <dt><b>\anchor maxrows _MaxRows and _MaxCols:</b></dt>
  * <dd>In most cases, one just leaves these parameters to the default values.
  * These parameters mean the maximum size of rows and columns that the matrix may have. They are useful in cases
  * when the exact numbers of rows and columns are not known are compile-time, but it is known at compile-time that they cannot
  * exceed a certain value. This happens when taking dynamic-size blocks inside fixed-size matrices: in this case _MaxRows and _MaxCols
  * are the dimensions of the original matrix, while _Rows and _Cols are Dynamic.</dd>
  * </dl>
  *
  * \see MatrixBase for the majority of the API methods for matrices
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
struct ei_traits<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
{
  typedef _Scalar Scalar;
  enum {
    RowsAtCompileTime = _Rows,
    ColsAtCompileTime = _Cols,
    MaxRowsAtCompileTime = _MaxRows,
    MaxColsAtCompileTime = _MaxCols,
    Flags = ei_compute_matrix_flags<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::ret,
    CoeffReadCost = NumTraits<Scalar>::ReadCost
  };
};

template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
class Matrix
  : public MatrixBase<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
{
  public:
    EIGEN_GENERIC_PUBLIC_INTERFACE(Matrix)
    enum { Options = _Options };
    friend class Eigen::Map<Matrix, Unaligned>;
    typedef class Eigen::Map<Matrix, Unaligned> UnalignedMapType;
    friend class Eigen::Map<Matrix, Aligned>;
    typedef class Eigen::Map<Matrix, Aligned> AlignedMapType;

  protected:
    ei_matrix_storage<Scalar, MaxSizeAtCompileTime, RowsAtCompileTime, ColsAtCompileTime, Options> m_storage;

  public:
    enum { NeedsToAlign = (Options&AutoAlign) == AutoAlign
                          && SizeAtCompileTime!=Dynamic && ((sizeof(Scalar)*SizeAtCompileTime)%16)==0 };
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign)
    
    Base& base() { return *static_cast<Base*>(this); }
    const Base& base() const { return *static_cast<const Base*>(this); }

    EIGEN_STRONG_INLINE int rows() const { return m_storage.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return m_storage.cols(); }

    EIGEN_STRONG_INLINE int stride(void) const
    {
      if(Flags & RowMajorBit)
        return m_storage.cols();
      else
        return m_storage.rows();
    }

    EIGEN_STRONG_INLINE const Scalar& coeff(int row, int col) const
    {
      if(Flags & RowMajorBit)
        return m_storage.data()[col + row * m_storage.cols()];
      else // column-major
        return m_storage.data()[row + col * m_storage.rows()];
    }

    EIGEN_STRONG_INLINE const Scalar& coeff(int index) const
    {
      return m_storage.data()[index];
    }

    EIGEN_STRONG_INLINE Scalar& coeffRef(int row, int col)
    {
      if(Flags & RowMajorBit)
        return m_storage.data()[col + row * m_storage.cols()];
      else // column-major
        return m_storage.data()[row + col * m_storage.rows()];
    }

    EIGEN_STRONG_INLINE Scalar& coeffRef(int index)
    {
      return m_storage.data()[index];
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(int row, int col) const
    {
      return ei_ploadt<Scalar, LoadMode>
               (m_storage.data() + (Flags & RowMajorBit
                                   ? col + row * m_storage.cols()
                                   : row + col * m_storage.rows()));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(int index) const
    {
      return ei_ploadt<Scalar, LoadMode>(m_storage.data() + index);
    }

    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacket(int row, int col, const PacketScalar& x)
    {
      ei_pstoret<Scalar, PacketScalar, StoreMode>
              (m_storage.data() + (Flags & RowMajorBit
                                   ? col + row * m_storage.cols()
                                   : row + col * m_storage.rows()), x);
    }

    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacket(int index, const PacketScalar& x)
    {
      ei_pstoret<Scalar, PacketScalar, StoreMode>(m_storage.data() + index, x);
    }

    /** \returns a const pointer to the data array of this matrix */
    EIGEN_STRONG_INLINE const Scalar *data() const
    { return m_storage.data(); }

    /** \returns a pointer to the data array of this matrix */
    EIGEN_STRONG_INLINE Scalar *data()
    { return m_storage.data(); }

    /** Resizes \c *this to a \a rows x \a cols matrix.
      *
      * Makes sense for dynamic-size matrices only.
      *
      * If the current number of coefficients of \c *this exactly matches the
      * product \a rows * \a cols, then no memory allocation is performed and
      * the current values are left unchanged. In all other cases, including
      * shrinking, the data is reallocated and all previous values are lost.
      *
      * \sa resize(int) for vectors.
      */
    inline void resize(int rows, int cols)
    {
      ei_assert((MaxRowsAtCompileTime == Dynamic || MaxRowsAtCompileTime >= rows)
             && (RowsAtCompileTime == Dynamic || RowsAtCompileTime == rows)
             && (MaxColsAtCompileTime == Dynamic || MaxColsAtCompileTime >= cols)
             && (ColsAtCompileTime == Dynamic || ColsAtCompileTime == cols));
      m_storage.resize(rows * cols, rows, cols);
    }

    /** Resizes \c *this to a vector of length \a size
      *
      * \sa resize(int,int) for the details.
      */
    inline void resize(int size)
    {
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(Matrix)
      if(RowsAtCompileTime == 1)
        m_storage.resize(size, 1, size);
      else
        m_storage.resize(size, size, 1);
    }

    /** Copies the value of the expression \a other into \c *this with automatic resizing.
      *
      * *this might be resized to match the dimensions of \a other. If *this was a null matrix (not already initialized),
      * it will be initialized.
      *
      * Note that copying a row-vector into a vector (and conversely) is allowed.
      * The resizing, if any, is then done in the appropriate way so that row-vectors
      * remain row-vectors and vectors remain vectors.
      */
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix& operator=(const MatrixBase<OtherDerived>& other)
    {
      return _set(other);
    }

    /** This is a special case of the templated operator=. Its purpose is to
      * prevent a default operator= from hiding the templated operator=.
      */
    EIGEN_STRONG_INLINE Matrix& operator=(const Matrix& other)
    {
      return _set(other);
    }

    EIGEN_INHERIT_ASSIGNMENT_OPERATOR(Matrix, +=)
    EIGEN_INHERIT_ASSIGNMENT_OPERATOR(Matrix, -=)
    EIGEN_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Matrix, *=)
    EIGEN_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Matrix, /=)

    /** Default constructor.
      *
      * For fixed-size matrices, does nothing.
      *
      * For dynamic-size matrices, creates an empty matrix of size 0. Does not allocate any array. Such a matrix
      * is called a null matrix. This constructor is the unique way to create null matrices: resizing
      * a matrix to 0 is not supported.
      *
      * \sa resize(int,int)
      */
    EIGEN_STRONG_INLINE explicit Matrix() : m_storage()
    {
      _check_template_params();
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    /** \internal */
    Matrix(ei_constructor_without_unaligned_array_assert)
      : m_storage(ei_constructor_without_unaligned_array_assert())
    {}
#endif

    /** Constructs a vector or row-vector with given dimension. \only_for_vectors
      *
      * Note that this is only useful for dynamic-size vectors. For fixed-size vectors,
      * it is redundant to pass the dimension here, so it makes more sense to use the default
      * constructor Matrix() instead.
      */
    EIGEN_STRONG_INLINE explicit Matrix(int dim)
      : m_storage(dim, RowsAtCompileTime == 1 ? 1 : dim, ColsAtCompileTime == 1 ? 1 : dim)
    {
      _check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(Matrix)
      ei_assert(dim > 0);
      ei_assert(SizeAtCompileTime == Dynamic || SizeAtCompileTime == dim);
    }

    /** This constructor has two very different behaviors, depending on the type of *this.
      *
      * \li When Matrix is a fixed-size vector type of size 2, this constructor constructs
      *     an initialized vector. The parameters \a x, \a y are copied into the first and second
      *     coords of the vector respectively.
      * \li Otherwise, this constructor constructs an uninitialized matrix with \a x rows and
      *     \a y columns. This is useful for dynamic-size matrices. For fixed-size matrices,
      *     it is redundant to pass these parameters, so one should use the default constructor
      *     Matrix() instead.
      */
    EIGEN_STRONG_INLINE Matrix(int x, int y) : m_storage(x*y, x, y)
    {
      _check_template_params();
      if((RowsAtCompileTime == 1 && ColsAtCompileTime == 2)
      || (RowsAtCompileTime == 2 && ColsAtCompileTime == 1))
      {
        m_storage.data()[0] = Scalar(x);
        m_storage.data()[1] = Scalar(y);
      }
      else
      {
        ei_assert(x > 0 && (RowsAtCompileTime == Dynamic || RowsAtCompileTime == x)
               && y > 0 && (ColsAtCompileTime == Dynamic || ColsAtCompileTime == y));
      }
    }
    /** constructs an initialized 2D vector with given coefficients */
    EIGEN_STRONG_INLINE Matrix(const float& x, const float& y)
    {
      _check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 2)
      m_storage.data()[0] = x;
      m_storage.data()[1] = y;
    }
    /** constructs an initialized 2D vector with given coefficients */
    EIGEN_STRONG_INLINE Matrix(const double& x, const double& y)
    {
      _check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 2)
      m_storage.data()[0] = x;
      m_storage.data()[1] = y;
    }
    /** constructs an initialized 3D vector with given coefficients */
    EIGEN_STRONG_INLINE Matrix(const Scalar& x, const Scalar& y, const Scalar& z)
    {
      _check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 3)
      m_storage.data()[0] = x;
      m_storage.data()[1] = y;
      m_storage.data()[2] = z;
    }
    /** constructs an initialized 4D vector with given coefficients */
    EIGEN_STRONG_INLINE Matrix(const Scalar& x, const Scalar& y, const Scalar& z, const Scalar& w)
    {
      _check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 4)
      m_storage.data()[0] = x;
      m_storage.data()[1] = y;
      m_storage.data()[2] = z;
      m_storage.data()[3] = w;
    }

    explicit Matrix(const Scalar *data);

    /** Constructor copying the value of the expression \a other */
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix(const MatrixBase<OtherDerived>& other)
             : m_storage(other.rows() * other.cols(), other.rows(), other.cols())
    {
      _check_template_params();
      _set_noalias(other);
    }
    /** Copy constructor */
    EIGEN_STRONG_INLINE Matrix(const Matrix& other)
            : Base(), m_storage(other.rows() * other.cols(), other.rows(), other.cols())
    {
      _check_template_params();
      _set_noalias(other);
    }
    /** Destructor */
    inline ~Matrix() {}

    /** Override MatrixBase::swap() since for dynamic-sized matrices of same type it is enough to swap the
      * data pointers.
      */
    template<typename OtherDerived>
    void swap(const MatrixBase<OtherDerived>& other);

    /** \name Map
      * These are convenience functions returning Map objects. The Map() static functions return unaligned Map objects,
      * while the AlignedMap() functions return aligned Map objects and thus should be called only with 16-byte-aligned
      * \a data pointers.
      *
      * \see class Map
      */
    //@{
    inline static const UnalignedMapType Map(const Scalar* data)
    { return UnalignedMapType(data); }
    inline static UnalignedMapType Map(Scalar* data)
    { return UnalignedMapType(data); }
    inline static const UnalignedMapType Map(const Scalar* data, int size)
    { return UnalignedMapType(data, size); }
    inline static UnalignedMapType Map(Scalar* data, int size)
    { return UnalignedMapType(data, size); }
    inline static const UnalignedMapType Map(const Scalar* data, int rows, int cols)
    { return UnalignedMapType(data, rows, cols); }
    inline static UnalignedMapType Map(Scalar* data, int rows, int cols)
    { return UnalignedMapType(data, rows, cols); }

    inline static const AlignedMapType MapAligned(const Scalar* data)
    { return AlignedMapType(data); }
    inline static AlignedMapType MapAligned(Scalar* data)
    { return AlignedMapType(data); }
    inline static const AlignedMapType MapAligned(const Scalar* data, int size)
    { return AlignedMapType(data, size); }
    inline static AlignedMapType MapAligned(Scalar* data, int size)
    { return AlignedMapType(data, size); }
    inline static const AlignedMapType MapAligned(const Scalar* data, int rows, int cols)
    { return AlignedMapType(data, rows, cols); }
    inline static AlignedMapType MapAligned(Scalar* data, int rows, int cols)
    { return AlignedMapType(data, rows, cols); }
    //@}

    using Base::setConstant;
    Matrix& setConstant(int size, const Scalar& value);
    Matrix& setConstant(int rows, int cols, const Scalar& value);

    using Base::setZero;
    Matrix& setZero(int size);
    Matrix& setZero(int rows, int cols);

    using Base::setOnes;
    Matrix& setOnes(int size);
    Matrix& setOnes(int rows, int cols);

    using Base::setRandom;
    Matrix& setRandom(int size);
    Matrix& setRandom(int rows, int cols);

    using Base::setIdentity;
    Matrix& setIdentity(int rows, int cols);

/////////// Geometry module ///////////

    template<typename OtherDerived>
    explicit Matrix(const RotationBase<OtherDerived,ColsAtCompileTime>& r);
    template<typename OtherDerived>
    Matrix& operator=(const RotationBase<OtherDerived,ColsAtCompileTime>& r);

    // allow to extend Matrix outside Eigen
    #ifdef EIGEN_MATRIX_PLUGIN
    #include EIGEN_MATRIX_PLUGIN
    #endif

  private:
    /** \internal Resizes *this in preparation for assigning \a other to it.
      * Takes care of doing all the checking that's needed.
      *
      * Note that copying a row-vector into a vector (and conversely) is allowed.
      * The resizing, if any, is then done in the appropriate way so that row-vectors
      * remain row-vectors and vectors remain vectors.
      */
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void _resize_to_match(const MatrixBase<OtherDerived>& other)
    {
      if(RowsAtCompileTime == 1)
      {
        ei_assert(other.isVector());
        resize(1, other.size());
      }
      else if(ColsAtCompileTime == 1)
      {
        ei_assert(other.isVector());
        resize(other.size(), 1);
      }
      else resize(other.rows(), other.cols());
    }

    /** \internal Copies the value of the expression \a other into \c *this with automatic resizing.
      *
      * *this might be resized to match the dimensions of \a other. If *this was a null matrix (not already initialized),
      * it will be initialized.
      *
      * Note that copying a row-vector into a vector (and conversely) is allowed.
      * The resizing, if any, is then done in the appropriate way so that row-vectors
      * remain row-vectors and vectors remain vectors.
      *
      * \sa operator=(const MatrixBase<OtherDerived>&), _set_noalias()
      */
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix& _set(const MatrixBase<OtherDerived>& other)
    {
      // this enum introduced to fix compilation with gcc 3.3
      enum { cond = int(OtherDerived::Flags) & EvalBeforeAssigningBit };
      _set_selector(other.derived(), typename ei_meta_if<bool(cond), ei_meta_true, ei_meta_false>::ret());
      return *this;
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void _set_selector(const OtherDerived& other, const ei_meta_true&) { _set_noalias(other.eval()); }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void _set_selector(const OtherDerived& other, const ei_meta_false&) { _set_noalias(other); }

    /** \internal Like _set() but additionally makes the assumption that no aliasing effect can happen (which
      * is the case when creating a new matrix) so one can enforce lazy evaluation.
      *
      * \sa operator=(const MatrixBase<OtherDerived>&), _set()
      */
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix& _set_noalias(const MatrixBase<OtherDerived>& other)
    {
      _resize_to_match(other);
      // the 'false' below means to enforce lazy evaluation. We don't use lazyAssign() because
      // it wouldn't allow to copy a row-vector into a column-vector.
      return ei_assign_selector<Matrix,OtherDerived,false>::run(*this, other.derived());
    }

    static EIGEN_STRONG_INLINE void _check_template_params()
    {
        EIGEN_STATIC_ASSERT((_Rows > 0
                        && _Cols > 0
                        && _MaxRows <= _Rows
                        && _MaxCols <= _Cols
                        && (_Options & (AutoAlign|RowMajor)) == _Options),
          INVALID_MATRIX_TEMPLATE_PARAMETERS)
    }
    
    template<typename MatrixType, typename OtherDerived, bool IsSameType, bool IsDynamicSize>
    friend struct ei_matrix_swap_impl;
};

template<typename MatrixType, typename OtherDerived,
         bool IsSameType = ei_is_same_type<MatrixType, OtherDerived>::ret,
         bool IsDynamicSize = MatrixType::SizeAtCompileTime==Dynamic>
struct ei_matrix_swap_impl
{
  static inline void run(MatrixType& matrix, MatrixBase<OtherDerived>& other)
  {
    matrix.base().swap(other);
  }
};

template<typename MatrixType, typename OtherDerived>
struct ei_matrix_swap_impl<MatrixType, OtherDerived, true, true>
{
  static inline void run(MatrixType& matrix, MatrixBase<OtherDerived>& other)
  {
    matrix.m_storage.swap(other.derived().m_storage);
  }
};

template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
template<typename OtherDerived>
inline void Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::swap(const MatrixBase<OtherDerived>& other)
{
  ei_matrix_swap_impl<Matrix, OtherDerived>::run(*this, *const_cast<MatrixBase<OtherDerived>*>(&other));
}


/** \defgroup matrixtypedefs Global matrix typedefs
  *
  * \ingroup Core_Module
  *
  * Eigen defines several typedef shortcuts for most common matrix and vector types.
  *
  * The general patterns are the following:
  *
  * \c MatrixSizeType where \c Size can be \c 2,\c 3,\c 4 for fixed size square matrices or \c X for dynamic size,
  * and where \c Type can be \c i for integer, \c f for float, \c d for double, \c cf for complex float, \c cd
  * for complex double.
  *
  * For example, \c Matrix3d is a fixed-size 3x3 matrix type of doubles, and \c MatrixXf is a dynamic-size matrix of floats.
  *
  * There are also \c VectorSizeType and \c RowVectorSizeType which are self-explanatory. For example, \c Vector4cf is
  * a fixed-size vector of 4 complex floats.
  *
  * \sa class Matrix
  */

#define EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, Size, SizeSuffix)   \
/** \ingroup matrixtypedefs */                                    \
typedef Matrix<Type, Size, Size> Matrix##SizeSuffix##TypeSuffix;  \
/** \ingroup matrixtypedefs */                                    \
typedef Matrix<Type, Size, 1>    Vector##SizeSuffix##TypeSuffix;  \
/** \ingroup matrixtypedefs */                                    \
typedef Matrix<Type, 1, Size>    RowVector##SizeSuffix##TypeSuffix;

#define EIGEN_MAKE_TYPEDEFS_ALL_SIZES(Type, TypeSuffix) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 2, 2) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 3, 3) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 4, 4) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, Dynamic, X)

EIGEN_MAKE_TYPEDEFS_ALL_SIZES(int,                  i)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(float,                f)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(double,               d)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(std::complex<float>,  cf)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(std::complex<double>, cd)

#undef EIGEN_MAKE_TYPEDEFS_ALL_SIZES
#undef EIGEN_MAKE_TYPEDEFS

#undef EIGEN_MAKE_TYPEDEFS_LARGE

#define EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, SizeSuffix) \
using Eigen::Matrix##SizeSuffix##TypeSuffix; \
using Eigen::Vector##SizeSuffix##TypeSuffix; \
using Eigen::RowVector##SizeSuffix##TypeSuffix;

#define EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE(TypeSuffix) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, 2) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, 3) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, 4) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, X) \

#define EIGEN_USING_MATRIX_TYPEDEFS \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE(i) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE(f) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE(d) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE(cf) \
EIGEN_USING_MATRIX_TYPEDEFS_FOR_TYPE(cd)

#endif // EIGEN_MATRIX_H

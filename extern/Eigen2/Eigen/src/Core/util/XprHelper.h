// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_XPRHELPER_H
#define EIGEN_XPRHELPER_H

// just a workaround because GCC seems to not really like empty structs
#ifdef __GNUG__
  struct ei_empty_struct{char _ei_dummy_;};
  #define EIGEN_EMPTY_STRUCT : Eigen::ei_empty_struct
#else
  #define EIGEN_EMPTY_STRUCT
#endif

//classes inheriting ei_no_assignment_operator don't generate a default operator=.
class ei_no_assignment_operator
{
  private:
    ei_no_assignment_operator& operator=(const ei_no_assignment_operator&);
};

/** \internal If the template parameter Value is Dynamic, this class is just a wrapper around an int variable that
  * can be accessed using value() and setValue().
  * Otherwise, this class is an empty structure and value() just returns the template parameter Value.
  */
template<int Value> class ei_int_if_dynamic EIGEN_EMPTY_STRUCT
{
  public:
    ei_int_if_dynamic() {}
    explicit ei_int_if_dynamic(int) {}
    static int value() { return Value; }
    void setValue(int) {}
};

template<> class ei_int_if_dynamic<Dynamic>
{
    int m_value;
    ei_int_if_dynamic() {}
  public:
    explicit ei_int_if_dynamic(int value) : m_value(value) {}
    int value() const { return m_value; }
    void setValue(int value) { m_value = value; }
};

template<typename T> struct ei_functor_traits
{
  enum
  {
    Cost = 10,
    PacketAccess = false
  };
};

template<typename T> struct ei_packet_traits
{
  typedef T type;
  enum {size=1};
};

template<typename T> struct ei_unpacket_traits
{
  typedef T type;
  enum {size=1};
};

template<typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
class ei_compute_matrix_flags
{
    enum {
      row_major_bit = Options&RowMajor ? RowMajorBit : 0,
      inner_max_size = row_major_bit ? MaxCols : MaxRows,
      is_big = inner_max_size == Dynamic,
      is_packet_size_multiple = (Cols*Rows) % ei_packet_traits<Scalar>::size == 0,
      aligned_bit = ((Options&AutoAlign) && (is_big || is_packet_size_multiple)) ? AlignedBit : 0,
      packet_access_bit = ei_packet_traits<Scalar>::size > 1 && aligned_bit ? PacketAccessBit : 0
    };

  public:
    enum { ret = LinearAccessBit | DirectAccessBit | packet_access_bit | row_major_bit | aligned_bit };
};

template<int _Rows, int _Cols> struct ei_size_at_compile_time
{
  enum { ret = (_Rows==Dynamic || _Cols==Dynamic) ? Dynamic : _Rows * _Cols };
};

/* ei_eval : the return type of eval(). For matrices, this is just a const reference
 * in order to avoid a useless copy
 */

template<typename T, int Sparseness = ei_traits<T>::Flags&SparseBit> class ei_eval;

template<typename T> struct ei_eval<T,IsDense>
{
  typedef Matrix<typename ei_traits<T>::Scalar,
                ei_traits<T>::RowsAtCompileTime,
                ei_traits<T>::ColsAtCompileTime,
                AutoAlign | (ei_traits<T>::Flags&RowMajorBit ? RowMajor : ColMajor),
                ei_traits<T>::MaxRowsAtCompileTime,
                ei_traits<T>::MaxColsAtCompileTime
          > type;
};

// for matrices, no need to evaluate, just use a const reference to avoid a useless copy
template<typename _Scalar, int _Rows, int _Cols, int _StorageOrder, int _MaxRows, int _MaxCols>
struct ei_eval<Matrix<_Scalar, _Rows, _Cols, _StorageOrder, _MaxRows, _MaxCols>, IsDense>
{
  typedef const Matrix<_Scalar, _Rows, _Cols, _StorageOrder, _MaxRows, _MaxCols>& type;
};

/* ei_plain_matrix_type : the difference from ei_eval is that ei_plain_matrix_type is always a plain matrix type,
 * whereas ei_eval is a const reference in the case of a matrix
 */
template<typename T> struct ei_plain_matrix_type
{
  typedef Matrix<typename ei_traits<T>::Scalar,
                ei_traits<T>::RowsAtCompileTime,
                ei_traits<T>::ColsAtCompileTime,
                AutoAlign | (ei_traits<T>::Flags&RowMajorBit ? RowMajor : ColMajor),
                ei_traits<T>::MaxRowsAtCompileTime,
                ei_traits<T>::MaxColsAtCompileTime
          > type;
};

/* ei_plain_matrix_type_column_major : same as ei_plain_matrix_type but guaranteed to be column-major
 */
template<typename T> struct ei_plain_matrix_type_column_major
{
  typedef Matrix<typename ei_traits<T>::Scalar,
                ei_traits<T>::RowsAtCompileTime,
                ei_traits<T>::ColsAtCompileTime,
                AutoAlign | ColMajor,
                ei_traits<T>::MaxRowsAtCompileTime,
                ei_traits<T>::MaxColsAtCompileTime
          > type;
};

template<typename T> struct ei_must_nest_by_value { enum { ret = false }; };
template<typename T> struct ei_must_nest_by_value<NestByValue<T> > { enum { ret = true }; };

/** \internal Determines how a given expression should be nested into another one.
  * For example, when you do a * (b+c), Eigen will determine how the expression b+c should be
  * nested into the bigger product expression. The choice is between nesting the expression b+c as-is, or
  * evaluating that expression b+c into a temporary variable d, and nest d so that the resulting expression is
  * a*d. Evaluating can be beneficial for example if every coefficient access in the resulting expression causes
  * many coefficient accesses in the nested expressions -- as is the case with matrix product for example.
  *
  * \param T the type of the expression being nested
  * \param n the number of coefficient accesses in the nested expression for each coefficient access in the bigger expression.
  *
  * Example. Suppose that a, b, and c are of type Matrix3d. The user forms the expression a*(b+c).
  * b+c is an expression "sum of matrices", which we will denote by S. In order to determine how to nest it,
  * the Product expression uses: ei_nested<S, 3>::ret, which turns out to be Matrix3d because the internal logic of
  * ei_nested determined that in this case it was better to evaluate the expression b+c into a temporary. On the other hand,
  * since a is of type Matrix3d, the Product expression nests it as ei_nested<Matrix3d, 3>::ret, which turns out to be
  * const Matrix3d&, because the internal logic of ei_nested determined that since a was already a matrix, there was no point
  * in copying it into another matrix.
  */
template<typename T, int n=1, typename PlainMatrixType = typename ei_eval<T>::type> struct ei_nested
{
  enum {
    CostEval   = (n+1) * int(NumTraits<typename ei_traits<T>::Scalar>::ReadCost),
    CostNoEval = (n-1) * int(ei_traits<T>::CoeffReadCost)
  };
  typedef typename ei_meta_if<
    ei_must_nest_by_value<T>::ret,
    T,
    typename ei_meta_if<
      (int(ei_traits<T>::Flags) & EvalBeforeNestingBit)
      || ( int(CostEval) <= int(CostNoEval) ),
      PlainMatrixType,
      const T&
    >::ret
  >::ret type;
};

template<unsigned int Flags> struct ei_are_flags_consistent
{
  enum { ret = !( (Flags&UnitDiagBit && Flags&ZeroDiagBit) )
  };
};

/** \internal Gives the type of a sub-matrix or sub-vector of a matrix of type \a ExpressionType and size \a Size
  * TODO: could be a good idea to define a big ReturnType struct ??
  */
template<typename ExpressionType, int RowsOrSize=Dynamic, int Cols=Dynamic> struct BlockReturnType {
  typedef Block<ExpressionType, (ei_traits<ExpressionType>::RowsAtCompileTime == 1 ? 1 : RowsOrSize),
                                (ei_traits<ExpressionType>::ColsAtCompileTime == 1 ? 1 : RowsOrSize)> SubVectorType;
  typedef Block<ExpressionType, RowsOrSize, Cols> Type;
};

template<typename CurrentType, typename NewType> struct ei_cast_return_type
{
  typedef typename ei_meta_if<ei_is_same_type<CurrentType,NewType>::ret,const CurrentType&,NewType>::ret type;
};

#endif // EIGEN_XPRHELPER_H

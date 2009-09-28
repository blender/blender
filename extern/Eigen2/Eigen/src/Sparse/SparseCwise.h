// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_SPARSE_CWISE_H
#define EIGEN_SPARSE_CWISE_H

/** \internal
  * convenient macro to defined the return type of a cwise binary operation */
#define EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(OP) \
    CwiseBinaryOp<OP<typename ei_traits<ExpressionType>::Scalar>, ExpressionType, OtherDerived>

#define EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE \
    SparseCwiseBinaryOp< \
      ei_scalar_product_op< \
        typename ei_scalar_product_traits< \
          typename ei_traits<ExpressionType>::Scalar, \
          typename ei_traits<OtherDerived>::Scalar \
        >::ReturnType \
      >, \
      ExpressionType, \
      OtherDerived \
    >

/** \internal
  * convenient macro to defined the return type of a cwise unary operation */
#define EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(OP) \
    SparseCwiseUnaryOp<OP<typename ei_traits<ExpressionType>::Scalar>, ExpressionType>

/** \internal
  * convenient macro to defined the return type of a cwise comparison to a scalar */
/*#define EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(OP) \
    CwiseBinaryOp<OP<typename ei_traits<ExpressionType>::Scalar>, ExpressionType, \
        NestByValue<typename ExpressionType::ConstantReturnType> >*/

template<typename ExpressionType> class SparseCwise
{
  public:

    typedef typename ei_traits<ExpressionType>::Scalar Scalar;
    typedef typename ei_meta_if<ei_must_nest_by_value<ExpressionType>::ret,
        ExpressionType, const ExpressionType&>::ret ExpressionTypeNested;
    typedef CwiseUnaryOp<ei_scalar_add_op<Scalar>, ExpressionType> ScalarAddReturnType;

    inline SparseCwise(const ExpressionType& matrix) : m_matrix(matrix) {}

    /** \internal */
    inline const ExpressionType& _expression() const { return m_matrix; }

    template<typename OtherDerived>
    const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
    operator*(const SparseMatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
    operator*(const MatrixBase<OtherDerived> &other) const;

//     template<typename OtherDerived>
//     const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)
//     operator/(const SparseMatrixBase<OtherDerived> &other) const;
//
//     template<typename OtherDerived>
//     const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)
//     operator/(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_min_op)
    min(const SparseMatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_max_op)
    max(const SparseMatrixBase<OtherDerived> &other) const;

    const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_abs_op)      abs() const;
    const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_abs2_op)     abs2() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_square_op)   square() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_cube_op)     cube() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_inverse_op)  inverse() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_sqrt_op)     sqrt() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_exp_op)      exp() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_log_op)      log() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_cos_op)      cos() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_sin_op)      sin() const;
//     const EIGEN_SPARSE_CWISE_UNOP_RETURN_TYPE(ei_scalar_pow_op)      pow(const Scalar& exponent) const;

    template<typename OtherDerived>
    inline ExpressionType& operator*=(const SparseMatrixBase<OtherDerived> &other);

//     template<typename OtherDerived>
//     inline ExpressionType& operator/=(const SparseMatrixBase<OtherDerived> &other);

    /*
    template<typename OtherDerived> const EIGEN_CWISE_BINOP_RETURN_TYPE(std::less)
    operator<(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived> const EIGEN_CWISE_BINOP_RETURN_TYPE(std::less_equal)
    operator<=(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived> const EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater)
    operator>(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived> const EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater_equal)
    operator>=(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived> const EIGEN_CWISE_BINOP_RETURN_TYPE(std::equal_to)
    operator==(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived> const EIGEN_CWISE_BINOP_RETURN_TYPE(std::not_equal_to)
    operator!=(const MatrixBase<OtherDerived>& other) const;

    // comparisons to a scalar value
    const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less)
    operator<(Scalar s) const;

    const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less_equal)
    operator<=(Scalar s) const;

    const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater)
    operator>(Scalar s) const;

    const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater_equal)
    operator>=(Scalar s) const;

    const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::equal_to)
    operator==(Scalar s) const;

    const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::not_equal_to)
    operator!=(Scalar s) const;
    */

    // allow to extend SparseCwise outside Eigen
    #ifdef EIGEN_SPARSE_CWISE_PLUGIN
    #include EIGEN_SPARSE_CWISE_PLUGIN
    #endif

  protected:
    ExpressionTypeNested m_matrix;
};

template<typename Derived>
inline const SparseCwise<Derived>
SparseMatrixBase<Derived>::cwise() const
{
  return derived();
}

template<typename Derived>
inline SparseCwise<Derived>
SparseMatrixBase<Derived>::cwise()
{
  return derived();
}

#endif // EIGEN_SPARSE_CWISE_H

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

#ifndef EIGEN_CWISE_H
#define EIGEN_CWISE_H

/** \internal
  * convenient macro to defined the return type of a cwise binary operation */
#define EIGEN_CWISE_BINOP_RETURN_TYPE(OP) \
    CwiseBinaryOp<OP<typename ei_traits<ExpressionType>::Scalar>, ExpressionType, OtherDerived>

#define EIGEN_CWISE_PRODUCT_RETURN_TYPE \
    CwiseBinaryOp< \
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
#define EIGEN_CWISE_UNOP_RETURN_TYPE(OP) \
    CwiseUnaryOp<OP<typename ei_traits<ExpressionType>::Scalar>, ExpressionType>

/** \internal
  * convenient macro to defined the return type of a cwise comparison to a scalar */
#define EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(OP) \
    CwiseBinaryOp<OP<typename ei_traits<ExpressionType>::Scalar>, ExpressionType, \
        NestByValue<typename ExpressionType::ConstantReturnType> >

/** \class Cwise
  *
  * \brief Pseudo expression providing additional coefficient-wise operations
  *
  * \param ExpressionType the type of the object on which to do coefficient-wise operations
  *
  * This class represents an expression with additional coefficient-wise features.
  * It is the return type of MatrixBase::cwise()
  * and most of the time this is the only way it is used.
  *
  * Note that some methods are defined in the \ref Array module.
  *
  * Example: \include MatrixBase_cwise_const.cpp
  * Output: \verbinclude MatrixBase_cwise_const.out
  *
  * \sa MatrixBase::cwise() const, MatrixBase::cwise()
  */
template<typename ExpressionType> class Cwise
{
  public:

    typedef typename ei_traits<ExpressionType>::Scalar Scalar;
    typedef typename ei_meta_if<ei_must_nest_by_value<ExpressionType>::ret,
        ExpressionType, const ExpressionType&>::ret ExpressionTypeNested;
    typedef CwiseUnaryOp<ei_scalar_add_op<Scalar>, ExpressionType> ScalarAddReturnType;

    inline Cwise(const ExpressionType& matrix) : m_matrix(matrix) {}

    /** \internal */
    inline const ExpressionType& _expression() const { return m_matrix; }

    template<typename OtherDerived>
    const EIGEN_CWISE_PRODUCT_RETURN_TYPE
    operator*(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const EIGEN_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)
    operator/(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const EIGEN_CWISE_BINOP_RETURN_TYPE(ei_scalar_min_op)
    min(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const EIGEN_CWISE_BINOP_RETURN_TYPE(ei_scalar_max_op)
    max(const MatrixBase<OtherDerived> &other) const;

    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_abs_op)      abs() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_abs2_op)     abs2() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_square_op)   square() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_cube_op)     cube() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_inverse_op)  inverse() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_sqrt_op)     sqrt() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_exp_op)      exp() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_log_op)      log() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_cos_op)      cos() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_sin_op)      sin() const;
    const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_pow_op)      pow(const Scalar& exponent) const;

    const ScalarAddReturnType
    operator+(const Scalar& scalar) const;

    /** \relates Cwise */
    friend const ScalarAddReturnType
    operator+(const Scalar& scalar, const Cwise& mat)
    { return mat + scalar; }

    ExpressionType& operator+=(const Scalar& scalar);

    const ScalarAddReturnType
    operator-(const Scalar& scalar) const;

    ExpressionType& operator-=(const Scalar& scalar);

    template<typename OtherDerived>
    inline ExpressionType& operator*=(const MatrixBase<OtherDerived> &other);

    template<typename OtherDerived>
    inline ExpressionType& operator/=(const MatrixBase<OtherDerived> &other);

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

    // allow to extend Cwise outside Eigen
    #ifdef EIGEN_CWISE_PLUGIN
    #include EIGEN_CWISE_PLUGIN
    #endif

  protected:
    ExpressionTypeNested m_matrix;

  private:
    Cwise& operator=(const Cwise&);
};

/** \returns a Cwise wrapper of *this providing additional coefficient-wise operations
  *
  * Example: \include MatrixBase_cwise_const.cpp
  * Output: \verbinclude MatrixBase_cwise_const.out
  *
  * \sa class Cwise, cwise()
  */
template<typename Derived>
inline const Cwise<Derived>
MatrixBase<Derived>::cwise() const
{
  return derived();
}

/** \returns a Cwise wrapper of *this providing additional coefficient-wise operations
  *
  * Example: \include MatrixBase_cwise.cpp
  * Output: \verbinclude MatrixBase_cwise.out
  *
  * \sa class Cwise, cwise() const
  */
template<typename Derived>
inline Cwise<Derived>
MatrixBase<Derived>::cwise()
{
  return derived();
}

#endif // EIGEN_CWISE_H

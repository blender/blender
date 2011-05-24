/** \returns an expression of the coefficient wise product of \c *this and \a other
  *
  * \sa MatrixBase::cwiseProduct
  */
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_PRODUCT_RETURN_TYPE(Derived,OtherDerived)
operator*(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return EIGEN_CWISE_PRODUCT_RETURN_TYPE(Derived,OtherDerived)(derived(), other.derived());
}

/** \returns an expression of the coefficient wise quotient of \c *this and \a other
  *
  * \sa MatrixBase::cwiseQuotient
  */
template<typename OtherDerived>
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_quotient_op<Scalar>, const Derived, const OtherDerived>
operator/(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<internal::scalar_quotient_op<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}

/** \returns an expression of the coefficient-wise min of \c *this and \a other
  *
  * Example: \include Cwise_min.cpp
  * Output: \verbinclude Cwise_min.out
  *
  * \sa max()
  */
EIGEN_MAKE_CWISE_BINARY_OP(min,internal::scalar_min_op)

/** \returns an expression of the coefficient-wise max of \c *this and \a other
  *
  * Example: \include Cwise_max.cpp
  * Output: \verbinclude Cwise_max.out
  *
  * \sa min()
  */
EIGEN_MAKE_CWISE_BINARY_OP(max,internal::scalar_max_op)

/** \returns an expression of the coefficient-wise \< operator of *this and \a other
  *
  * Example: \include Cwise_less.cpp
  * Output: \verbinclude Cwise_less.out
  *
  * \sa all(), any(), operator>(), operator<=()
  */
EIGEN_MAKE_CWISE_BINARY_OP(operator<,std::less)

/** \returns an expression of the coefficient-wise \<= operator of *this and \a other
  *
  * Example: \include Cwise_less_equal.cpp
  * Output: \verbinclude Cwise_less_equal.out
  *
  * \sa all(), any(), operator>=(), operator<()
  */
EIGEN_MAKE_CWISE_BINARY_OP(operator<=,std::less_equal)

/** \returns an expression of the coefficient-wise \> operator of *this and \a other
  *
  * Example: \include Cwise_greater.cpp
  * Output: \verbinclude Cwise_greater.out
  *
  * \sa all(), any(), operator>=(), operator<()
  */
EIGEN_MAKE_CWISE_BINARY_OP(operator>,std::greater)

/** \returns an expression of the coefficient-wise \>= operator of *this and \a other
  *
  * Example: \include Cwise_greater_equal.cpp
  * Output: \verbinclude Cwise_greater_equal.out
  *
  * \sa all(), any(), operator>(), operator<=()
  */
EIGEN_MAKE_CWISE_BINARY_OP(operator>=,std::greater_equal)

/** \returns an expression of the coefficient-wise == operator of *this and \a other
  *
  * \warning this performs an exact comparison, which is generally a bad idea with floating-point types.
  * In order to check for equality between two vectors or matrices with floating-point coefficients, it is
  * generally a far better idea to use a fuzzy comparison as provided by isApprox() and
  * isMuchSmallerThan().
  *
  * Example: \include Cwise_equal_equal.cpp
  * Output: \verbinclude Cwise_equal_equal.out
  *
  * \sa all(), any(), isApprox(), isMuchSmallerThan()
  */
EIGEN_MAKE_CWISE_BINARY_OP(operator==,std::equal_to)

/** \returns an expression of the coefficient-wise != operator of *this and \a other
  *
  * \warning this performs an exact comparison, which is generally a bad idea with floating-point types.
  * In order to check for equality between two vectors or matrices with floating-point coefficients, it is
  * generally a far better idea to use a fuzzy comparison as provided by isApprox() and
  * isMuchSmallerThan().
  *
  * Example: \include Cwise_not_equal.cpp
  * Output: \verbinclude Cwise_not_equal.out
  *
  * \sa all(), any(), isApprox(), isMuchSmallerThan()
  */
EIGEN_MAKE_CWISE_BINARY_OP(operator!=,std::not_equal_to)

// scalar addition

/** \returns an expression of \c *this with each coeff incremented by the constant \a scalar
  *
  * Example: \include Cwise_plus.cpp
  * Output: \verbinclude Cwise_plus.out
  *
  * \sa operator+=(), operator-()
  */
inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>
operator+(const Scalar& scalar) const
{
  return CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>(derived(), internal::scalar_add_op<Scalar>(scalar));
}

friend inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>
operator+(const Scalar& scalar,const EIGEN_CURRENT_STORAGE_BASE_CLASS<Derived>& other)
{
  return other + scalar;
}

/** \returns an expression of \c *this with each coeff decremented by the constant \a scalar
  *
  * Example: \include Cwise_minus.cpp
  * Output: \verbinclude Cwise_minus.out
  *
  * \sa operator+(), operator-=()
  */
inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>
operator-(const Scalar& scalar) const
{
  return *this + (-scalar);
}

friend inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const CwiseUnaryOp<internal::scalar_opposite_op<Scalar>, const Derived> >
operator-(const Scalar& scalar,const EIGEN_CURRENT_STORAGE_BASE_CLASS<Derived>& other)
{
  return (-other) + scalar;
}

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

/** \returns an expression of the coefficient-wise min of \c *this and scalar \a other
  *
  * \sa max()
  */
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_min_op<Scalar>, const Derived,
                                        const CwiseNullaryOp<internal::scalar_constant_op<Scalar>, PlainObject> >
#ifdef EIGEN_PARSED_BY_DOXYGEN
min
#else
(min)
#endif
(const Scalar &other) const
{
  return (min)(Derived::PlainObject::Constant(rows(), cols(), other));
}

/** \returns an expression of the coefficient-wise max of \c *this and \a other
  *
  * Example: \include Cwise_max.cpp
  * Output: \verbinclude Cwise_max.out
  *
  * \sa min()
  */
EIGEN_MAKE_CWISE_BINARY_OP(max,internal::scalar_max_op)

/** \returns an expression of the coefficient-wise max of \c *this and scalar \a other
  *
  * \sa min()
  */
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_max_op<Scalar>, const Derived,
                                        const CwiseNullaryOp<internal::scalar_constant_op<Scalar>, PlainObject> >
#ifdef EIGEN_PARSED_BY_DOXYGEN
max
#else
(max)
#endif
(const Scalar &other) const
{
  return (max)(Derived::PlainObject::Constant(rows(), cols(), other));
}


#define EIGEN_MAKE_CWISE_COMP_OP(OP, COMPARATOR) \
template<typename OtherDerived> \
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_cmp_op<Scalar, internal::cmp_ ## COMPARATOR>, const Derived, const OtherDerived> \
OP(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const \
{ \
  return CwiseBinaryOp<internal::scalar_cmp_op<Scalar, internal::cmp_ ## COMPARATOR>, const Derived, const OtherDerived>(derived(), other.derived()); \
}\
typedef CwiseBinaryOp<internal::scalar_cmp_op<Scalar, internal::cmp_ ## COMPARATOR>, const Derived, const CwiseNullaryOp<internal::scalar_constant_op<Scalar>, PlainObject> > Cmp ## COMPARATOR ## ReturnType; \
typedef CwiseBinaryOp<internal::scalar_cmp_op<Scalar, internal::cmp_ ## COMPARATOR>, const CwiseNullaryOp<internal::scalar_constant_op<Scalar>, PlainObject>, const Derived > RCmp ## COMPARATOR ## ReturnType; \
EIGEN_STRONG_INLINE const Cmp ## COMPARATOR ## ReturnType \
OP(const Scalar& s) const { \
  return this->OP(Derived::PlainObject::Constant(rows(), cols(), s)); \
} \
friend EIGEN_STRONG_INLINE const RCmp ## COMPARATOR ## ReturnType \
OP(const Scalar& s, const Derived& d) { \
  return Derived::PlainObject::Constant(d.rows(), d.cols(), s).OP(d); \
}

#define EIGEN_MAKE_CWISE_COMP_R_OP(OP, R_OP, RCOMPARATOR) \
template<typename OtherDerived> \
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_cmp_op<Scalar, internal::cmp_##RCOMPARATOR>, const OtherDerived, const Derived> \
OP(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const \
{ \
  return CwiseBinaryOp<internal::scalar_cmp_op<Scalar, internal::cmp_##RCOMPARATOR>, const OtherDerived, const Derived>(other.derived(), derived()); \
} \
\
inline const RCmp ## RCOMPARATOR ## ReturnType \
OP(const Scalar& s) const { \
  return Derived::PlainObject::Constant(rows(), cols(), s).R_OP(*this); \
} \
friend inline const Cmp ## RCOMPARATOR ## ReturnType \
OP(const Scalar& s, const Derived& d) { \
  return d.R_OP(Derived::PlainObject::Constant(d.rows(), d.cols(), s)); \
}


/** \returns an expression of the coefficient-wise \< operator of *this and \a other
  *
  * Example: \include Cwise_less.cpp
  * Output: \verbinclude Cwise_less.out
  *
  * \sa all(), any(), operator>(), operator<=()
  */
EIGEN_MAKE_CWISE_COMP_OP(operator<, LT)

/** \returns an expression of the coefficient-wise \<= operator of *this and \a other
  *
  * Example: \include Cwise_less_equal.cpp
  * Output: \verbinclude Cwise_less_equal.out
  *
  * \sa all(), any(), operator>=(), operator<()
  */
EIGEN_MAKE_CWISE_COMP_OP(operator<=, LE)

/** \returns an expression of the coefficient-wise \> operator of *this and \a other
  *
  * Example: \include Cwise_greater.cpp
  * Output: \verbinclude Cwise_greater.out
  *
  * \sa all(), any(), operator>=(), operator<()
  */
EIGEN_MAKE_CWISE_COMP_R_OP(operator>, operator<, LT)

/** \returns an expression of the coefficient-wise \>= operator of *this and \a other
  *
  * Example: \include Cwise_greater_equal.cpp
  * Output: \verbinclude Cwise_greater_equal.out
  *
  * \sa all(), any(), operator>(), operator<=()
  */
EIGEN_MAKE_CWISE_COMP_R_OP(operator>=, operator<=, LE)

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
EIGEN_MAKE_CWISE_COMP_OP(operator==, EQ)

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
EIGEN_MAKE_CWISE_COMP_OP(operator!=, NEQ)

#undef EIGEN_MAKE_CWISE_COMP_OP
#undef EIGEN_MAKE_CWISE_COMP_R_OP

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

/** \returns an expression of the coefficient-wise && operator of *this and \a other
  *
  * \warning this operator is for expression of bool only.
  *
  * Example: \include Cwise_boolean_and.cpp
  * Output: \verbinclude Cwise_boolean_and.out
  *
  * \sa operator||(), select()
  */
template<typename OtherDerived>
inline const CwiseBinaryOp<internal::scalar_boolean_and_op, const Derived, const OtherDerived>
operator&&(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  EIGEN_STATIC_ASSERT((internal::is_same<bool,Scalar>::value && internal::is_same<bool,typename OtherDerived::Scalar>::value),
                      THIS_METHOD_IS_ONLY_FOR_EXPRESSIONS_OF_BOOL);
  return CwiseBinaryOp<internal::scalar_boolean_and_op, const Derived, const OtherDerived>(derived(),other.derived());
}

/** \returns an expression of the coefficient-wise || operator of *this and \a other
  *
  * \warning this operator is for expression of bool only.
  *
  * Example: \include Cwise_boolean_or.cpp
  * Output: \verbinclude Cwise_boolean_or.out
  *
  * \sa operator&&(), select()
  */
template<typename OtherDerived>
inline const CwiseBinaryOp<internal::scalar_boolean_or_op, const Derived, const OtherDerived>
operator||(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  EIGEN_STATIC_ASSERT((internal::is_same<bool,Scalar>::value && internal::is_same<bool,typename OtherDerived::Scalar>::value),
                      THIS_METHOD_IS_ONLY_FOR_EXPRESSIONS_OF_BOOL);
  return CwiseBinaryOp<internal::scalar_boolean_or_op, const Derived, const OtherDerived>(derived(),other.derived());
}



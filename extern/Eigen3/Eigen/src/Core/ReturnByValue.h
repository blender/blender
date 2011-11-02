// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_RETURNBYVALUE_H
#define EIGEN_RETURNBYVALUE_H

/** \class ReturnByValue
  * \ingroup Core_Module
  *
  */

namespace internal {

template<typename Derived>
struct traits<ReturnByValue<Derived> >
  : public traits<typename traits<Derived>::ReturnType>
{
  enum {
    // We're disabling the DirectAccess because e.g. the constructor of
    // the Block-with-DirectAccess expression requires to have a coeffRef method.
    // Also, we don't want to have to implement the stride stuff.
    Flags = (traits<typename traits<Derived>::ReturnType>::Flags
             | EvalBeforeNestingBit) & ~DirectAccessBit
  };
};

/* The ReturnByValue object doesn't even have a coeff() method.
 * So the only way that nesting it in an expression can work, is by evaluating it into a plain matrix.
 * So internal::nested always gives the plain return matrix type.
 *
 * FIXME: I don't understand why we need this specialization: isn't this taken care of by the EvalBeforeNestingBit ??
 */
template<typename Derived,int n,typename PlainObject>
struct nested<ReturnByValue<Derived>, n, PlainObject>
{
  typedef typename traits<Derived>::ReturnType type;
};

} // end namespace internal

template<typename Derived> class ReturnByValue
  : public internal::dense_xpr_base< ReturnByValue<Derived> >::type
{
  public:
    typedef typename internal::traits<Derived>::ReturnType ReturnType;

    typedef typename internal::dense_xpr_base<ReturnByValue>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(ReturnByValue)

    template<typename Dest>
    inline void evalTo(Dest& dst) const
    { static_cast<const Derived*>(this)->evalTo(dst); }
    inline Index rows() const { return static_cast<const Derived*>(this)->rows(); }
    inline Index cols() const { return static_cast<const Derived*>(this)->cols(); }

#ifndef EIGEN_PARSED_BY_DOXYGEN
#define Unusable YOU_ARE_TRYING_TO_ACCESS_A_SINGLE_COEFFICIENT_IN_A_SPECIAL_EXPRESSION_WHERE_THAT_IS_NOT_ALLOWED_BECAUSE_THAT_WOULD_BE_INEFFICIENT
    class Unusable{
      Unusable(const Unusable&) {}
      Unusable& operator=(const Unusable&) {return *this;}
    };
    const Unusable& coeff(Index) const { return *reinterpret_cast<const Unusable*>(this); }
    const Unusable& coeff(Index,Index) const { return *reinterpret_cast<const Unusable*>(this); }
    Unusable& coeffRef(Index) { return *reinterpret_cast<Unusable*>(this); }
    Unusable& coeffRef(Index,Index) { return *reinterpret_cast<Unusable*>(this); }
#endif
};

template<typename Derived>
template<typename OtherDerived>
Derived& DenseBase<Derived>::operator=(const ReturnByValue<OtherDerived>& other)
{
  other.evalTo(derived());
  return derived();
}

#endif // EIGEN_RETURNBYVALUE_H

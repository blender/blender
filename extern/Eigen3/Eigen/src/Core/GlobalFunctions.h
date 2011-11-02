// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_GLOBAL_FUNCTIONS_H
#define EIGEN_GLOBAL_FUNCTIONS_H

#define EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(NAME,FUNCTOR) \
  template<typename Derived> \
  inline const Eigen::CwiseUnaryOp<Eigen::internal::FUNCTOR<typename Derived::Scalar>, const Derived> \
  NAME(const Eigen::ArrayBase<Derived>& x) { \
    return x.derived(); \
  }

#define EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(NAME,FUNCTOR) \
  \
  template<typename Derived> \
  struct NAME##_retval<ArrayBase<Derived> > \
  { \
    typedef const Eigen::CwiseUnaryOp<Eigen::internal::FUNCTOR<typename Derived::Scalar>, const Derived> type; \
  }; \
  template<typename Derived> \
  struct NAME##_impl<ArrayBase<Derived> > \
  { \
    static inline typename NAME##_retval<ArrayBase<Derived> >::type run(const Eigen::ArrayBase<Derived>& x) \
    { \
      return x.derived(); \
    } \
  };


namespace std
{
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(real,scalar_real_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(imag,scalar_imag_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(sin,scalar_sin_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(cos,scalar_cos_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(asin,scalar_asin_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(acos,scalar_acos_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(tan,scalar_tan_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(exp,scalar_exp_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(log,scalar_log_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(abs,scalar_abs_op)
  EIGEN_ARRAY_DECLARE_GLOBAL_STD_UNARY(sqrt,scalar_sqrt_op)

  template<typename Derived>
  inline const Eigen::CwiseUnaryOp<Eigen::internal::scalar_pow_op<typename Derived::Scalar>, const Derived>
  pow(const Eigen::ArrayBase<Derived>& x, const typename Derived::Scalar& exponent) { \
    return x.derived().pow(exponent); \
  }
}

namespace Eigen
{
  namespace internal
  {
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(real,scalar_real_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(imag,scalar_imag_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(sin,scalar_sin_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(cos,scalar_cos_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(asin,scalar_asin_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(acos,scalar_acos_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(tan,scalar_tan_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(exp,scalar_exp_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(log,scalar_log_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(abs,scalar_abs_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(abs2,scalar_abs2_op)
    EIGEN_ARRAY_DECLARE_GLOBAL_EIGEN_UNARY(sqrt,scalar_sqrt_op)
  }
}

// TODO: cleanly disable those functions that are not supported on Array (internal::real_ref, internal::random, internal::isApprox...)

#endif // EIGEN_GLOBAL_FUNCTIONS_H

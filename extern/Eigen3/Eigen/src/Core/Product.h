// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_PRODUCT_H
#define EIGEN_PRODUCT_H

template<typename Lhs, typename Rhs> class Product;
template<typename Lhs, typename Rhs, typename StorageKind> class ProductImpl;

/** \class Product
  * \ingroup Core_Module
  *
  * \brief Expression of the product of two arbitrary matrices or vectors
  *
  * \param Lhs the type of the left-hand side expression
  * \param Rhs the type of the right-hand side expression
  *
  * This class represents an expression of the product of two arbitrary matrices.
  *
  */

namespace internal {
template<typename Lhs, typename Rhs>
struct traits<Product<Lhs, Rhs> >
{
  typedef MatrixXpr XprKind;
  typedef typename remove_all<Lhs>::type LhsCleaned;
  typedef typename remove_all<Rhs>::type RhsCleaned;
  typedef typename scalar_product_traits<typename traits<LhsCleaned>::Scalar, typename traits<RhsCleaned>::Scalar>::ReturnType Scalar;
  typedef typename promote_storage_type<typename traits<LhsCleaned>::StorageKind,
                                        typename traits<RhsCleaned>::StorageKind>::ret StorageKind;
  typedef typename promote_index_type<typename traits<LhsCleaned>::Index,
                                      typename traits<RhsCleaned>::Index>::type Index;
  enum {
    RowsAtCompileTime = LhsCleaned::RowsAtCompileTime,
    ColsAtCompileTime = RhsCleaned::ColsAtCompileTime,
    MaxRowsAtCompileTime = LhsCleaned::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = RhsCleaned::MaxColsAtCompileTime,
    Flags = (MaxRowsAtCompileTime==1 ? RowMajorBit : 0), // TODO should be no storage order
    CoeffReadCost = 0 // TODO CoeffReadCost should not be part of the expression traits
  };
};
} // end namespace internal


template<typename Lhs, typename Rhs>
class Product : public ProductImpl<Lhs,Rhs,typename internal::promote_storage_type<typename internal::traits<Lhs>::StorageKind,
                                                                            typename internal::traits<Rhs>::StorageKind>::ret>
{
  public:
    
    typedef typename ProductImpl<
        Lhs, Rhs,
        typename internal::promote_storage_type<typename Lhs::StorageKind,
                                                typename Rhs::StorageKind>::ret>::Base Base;
    EIGEN_GENERIC_PUBLIC_INTERFACE(Product)

    typedef typename Lhs::Nested LhsNested;
    typedef typename Rhs::Nested RhsNested;
    typedef typename internal::remove_all<LhsNested>::type LhsNestedCleaned;
    typedef typename internal::remove_all<RhsNested>::type RhsNestedCleaned;

    Product(const Lhs& lhs, const Rhs& rhs) : m_lhs(lhs), m_rhs(rhs)
    {
      eigen_assert(lhs.cols() == rhs.rows()
        && "invalid matrix product"
        && "if you wanted a coeff-wise or a dot product use the respective explicit functions");
    }

    inline Index rows() const { return m_lhs.rows(); }
    inline Index cols() const { return m_rhs.cols(); }

    const LhsNestedCleaned& lhs() const { return m_lhs; }
    const RhsNestedCleaned& rhs() const { return m_rhs; }

  protected:

    const LhsNested m_lhs;
    const RhsNested m_rhs;
};

template<typename Lhs, typename Rhs>
class ProductImpl<Lhs,Rhs,Dense> : public internal::dense_xpr_base<Product<Lhs,Rhs> >::type
{
    typedef Product<Lhs, Rhs> Derived;
  public:

    typedef typename internal::dense_xpr_base<Product<Lhs, Rhs> >::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Derived)
};

#endif // EIGEN_PRODUCT_H

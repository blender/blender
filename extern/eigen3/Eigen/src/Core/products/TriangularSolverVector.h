// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_TRIANGULAR_SOLVER_VECTOR_H
#define EIGEN_TRIANGULAR_SOLVER_VECTOR_H

namespace internal {

template<typename LhsScalar, typename RhsScalar, typename Index, int Mode, bool Conjugate, int StorageOrder>
struct triangular_solve_vector<LhsScalar, RhsScalar, Index, OnTheRight, Mode, Conjugate, StorageOrder>
{
  static void run(Index size, const LhsScalar* _lhs, Index lhsStride, RhsScalar* rhs)
  {
    triangular_solve_vector<LhsScalar,RhsScalar,Index,OnTheLeft,
        ((Mode&Upper)==Upper ? Lower : Upper) | (Mode&UnitDiag),
        Conjugate,StorageOrder==RowMajor?ColMajor:RowMajor
      >::run(size, _lhs, lhsStride, rhs);
  }
};
    
// forward and backward substitution, row-major, rhs is a vector
template<typename LhsScalar, typename RhsScalar, typename Index, int Mode, bool Conjugate>
struct triangular_solve_vector<LhsScalar, RhsScalar, Index, OnTheLeft, Mode, Conjugate, RowMajor>
{
  enum {
    IsLower = ((Mode&Lower)==Lower)
  };
  static void run(Index size, const LhsScalar* _lhs, Index lhsStride, RhsScalar* rhs)
  {
    typedef Map<const Matrix<LhsScalar,Dynamic,Dynamic,RowMajor>, 0, OuterStride<> > LhsMap;
    const LhsMap lhs(_lhs,size,size,OuterStride<>(lhsStride));
    typename internal::conditional<
                          Conjugate,
                          const CwiseUnaryOp<typename internal::scalar_conjugate_op<LhsScalar>,LhsMap>,
                          const LhsMap&>
                        ::type cjLhs(lhs);
    static const Index PanelWidth = EIGEN_TUNE_TRIANGULAR_PANEL_WIDTH;
    for(Index pi=IsLower ? 0 : size;
        IsLower ? pi<size : pi>0;
        IsLower ? pi+=PanelWidth : pi-=PanelWidth)
    {
      Index actualPanelWidth = std::min(IsLower ? size - pi : pi, PanelWidth);

      Index r = IsLower ? pi : size - pi; // remaining size
      if (r > 0)
      {
        // let's directly call the low level product function because:
        // 1 - it is faster to compile
        // 2 - it is slighlty faster at runtime
        Index startRow = IsLower ? pi : pi-actualPanelWidth;
        Index startCol = IsLower ? 0 : pi;

        general_matrix_vector_product<Index,LhsScalar,RowMajor,Conjugate,RhsScalar,false>::run(
          actualPanelWidth, r,
          &lhs.coeffRef(startRow,startCol), lhsStride,
          rhs + startCol, 1,
          rhs + startRow, 1,
          RhsScalar(-1));
      }

      for(Index k=0; k<actualPanelWidth; ++k)
      {
        Index i = IsLower ? pi+k : pi-k-1;
        Index s = IsLower ? pi   : i+1;
        if (k>0)
          rhs[i] -= (cjLhs.row(i).segment(s,k).transpose().cwiseProduct(Map<const Matrix<RhsScalar,Dynamic,1> >(rhs+s,k))).sum();
        
        if(!(Mode & UnitDiag))
          rhs[i] /= cjLhs(i,i);
      }
    }
  }
};

// forward and backward substitution, column-major, rhs is a vector
template<typename LhsScalar, typename RhsScalar, typename Index, int Mode, bool Conjugate>
struct triangular_solve_vector<LhsScalar, RhsScalar, Index, OnTheLeft, Mode, Conjugate, ColMajor>
{
  enum {
    IsLower = ((Mode&Lower)==Lower)
  };
  static void run(Index size, const LhsScalar* _lhs, Index lhsStride, RhsScalar* rhs)
  {
    typedef Map<const Matrix<LhsScalar,Dynamic,Dynamic,ColMajor>, 0, OuterStride<> > LhsMap;
    const LhsMap lhs(_lhs,size,size,OuterStride<>(lhsStride));
    typename internal::conditional<Conjugate,
                                   const CwiseUnaryOp<typename internal::scalar_conjugate_op<LhsScalar>,LhsMap>,
                                   const LhsMap&
                                  >::type cjLhs(lhs);
    static const Index PanelWidth = EIGEN_TUNE_TRIANGULAR_PANEL_WIDTH;

    for(Index pi=IsLower ? 0 : size;
        IsLower ? pi<size : pi>0;
        IsLower ? pi+=PanelWidth : pi-=PanelWidth)
    {
      Index actualPanelWidth = std::min(IsLower ? size - pi : pi, PanelWidth);
      Index startBlock = IsLower ? pi : pi-actualPanelWidth;
      Index endBlock = IsLower ? pi + actualPanelWidth : 0;

      for(Index k=0; k<actualPanelWidth; ++k)
      {
        Index i = IsLower ? pi+k : pi-k-1;
        if(!(Mode & UnitDiag))
          rhs[i] /= cjLhs.coeff(i,i);

        Index r = actualPanelWidth - k - 1; // remaining size
        Index s = IsLower ? i+1 : i-r;
        if (r>0)
          Map<Matrix<RhsScalar,Dynamic,1> >(rhs+s,r) -= rhs[i] * cjLhs.col(i).segment(s,r);
      }
      Index r = IsLower ? size - endBlock : startBlock; // remaining size
      if (r > 0)
      {
        // let's directly call the low level product function because:
        // 1 - it is faster to compile
        // 2 - it is slighlty faster at runtime
        general_matrix_vector_product<Index,LhsScalar,ColMajor,Conjugate,RhsScalar,false>::run(
            r, actualPanelWidth,
            &lhs.coeffRef(endBlock,startBlock), lhsStride,
            rhs+startBlock, 1,
            rhs+endBlock, 1, RhsScalar(-1));
      }
    }
  }
};

} // end namespace internal

#endif // EIGEN_TRIANGULAR_SOLVER_VECTOR_H

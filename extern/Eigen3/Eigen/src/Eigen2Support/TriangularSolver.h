// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_TRIANGULAR_SOLVER2_H
#define EIGEN_TRIANGULAR_SOLVER2_H

const unsigned int UnitDiagBit = UnitDiag;
const unsigned int SelfAdjointBit = SelfAdjoint;
const unsigned int UpperTriangularBit = Upper;
const unsigned int LowerTriangularBit = Lower;

const unsigned int UpperTriangular = Upper;
const unsigned int LowerTriangular = Lower;
const unsigned int UnitUpperTriangular = UnitUpper;
const unsigned int UnitLowerTriangular = UnitLower;

template<typename ExpressionType, unsigned int Added, unsigned int Removed>
template<typename OtherDerived>
typename ExpressionType::PlainObject
Flagged<ExpressionType,Added,Removed>::solveTriangular(const MatrixBase<OtherDerived>& other) const
{
  return m_matrix.template triangularView<Added>().solve(other.derived());
}

template<typename ExpressionType, unsigned int Added, unsigned int Removed>
template<typename OtherDerived>
void Flagged<ExpressionType,Added,Removed>::solveTriangularInPlace(const MatrixBase<OtherDerived>& other) const
{
  m_matrix.template triangularView<Added>().solveInPlace(other.derived());
}
    
#endif // EIGEN_TRIANGULAR_SOLVER2_H

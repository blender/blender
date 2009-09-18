//////////////////////////////////////////////////////////////////////
// This file is part of Wavelet Turbulence.
// 
// Wavelet Turbulence is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Wavelet Turbulence is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Wavelet Turbulence.  If not, see <http://www.gnu.org/licenses/>.
// 
// Copyright 2008 Theodore Kim and Nils Thuerey
// 
//////////////////////////////////////////////////////////////////////

#ifndef LU_HELPER_H
#define LU_HELPER_H

//////////////////////////////////////////////////////////////////////
// Helper function, compute eigenvalues of 3x3 matrix
//////////////////////////////////////////////////////////////////////

#include "tnt/jama_lu.h"

//////////////////////////////////////////////////////////////////////
// LU decomposition of 3x3 non-symmetric matrix
//////////////////////////////////////////////////////////////////////
JAMA::LU<float> inline computeLU3x3(
		float a[3][3])
{
		TNT::Array2D<float> A = TNT::Array2D<float>(3,3, &a[0][0]);
		JAMA::LU<float> jLU= JAMA::LU<float>(A);
		return jLU;
}

//////////////////////////////////////////////////////////////////////
// LU decomposition of 3x3 non-symmetric matrix
//////////////////////////////////////////////////////////////////////
void inline solveLU3x3(
    JAMA::LU<float>& A,
    float x[3],
    float b[3])
{
  TNT::Array1D<float> jamaB = TNT::Array1D<float>(3, &b[0]);
  TNT::Array1D<float> jamaX = A.solve(jamaB);

  x[0] = jamaX[0];
  x[1] = jamaX[1];
  x[2] = jamaX[2];
}
#endif

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
// Helper function, compute eigenvalues of 3x3 matrix
//////////////////////////////////////////////////////////////////////

#include "tnt/jama_eig.h"

//////////////////////////////////////////////////////////////////////
// eigenvalues of 3x3 non-symmetric matrix
//////////////////////////////////////////////////////////////////////
int inline computeEigenvalues3x3(
		float dout[3], 
		float a[3][3])
{
  TNT::Array2D<float> A = TNT::Array2D<float>(3,3, &a[0][0]);
  TNT::Array1D<float> eig = TNT::Array1D<float>(3);
  TNT::Array1D<float> eigImag = TNT::Array1D<float>(3);
  JAMA::Eigenvalue<float> jeig = JAMA::Eigenvalue<float>(A);
  jeig.getRealEigenvalues(eig);

  // complex ones
  jeig.getImagEigenvalues(eigImag);
  dout[0]  = sqrt(eig[0]*eig[0] + eigImag[0]*eigImag[0]);
  dout[1]  = sqrt(eig[1]*eig[1] + eigImag[1]*eigImag[1]);
  dout[2]  = sqrt(eig[2]*eig[2] + eigImag[2]*eigImag[2]);
  return 0;
}

#undef rfabs 
#undef ROT

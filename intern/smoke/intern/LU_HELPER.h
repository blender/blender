/** \file smoke/intern/LU_HELPER.h
 *  \ingroup smoke
 */
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
// Modified to not require TNT matrix library anymore. It was very slow
// when being run in parallel. Required TNT JAMA:LU libraries were
// converted into independent functions.
//		- MiikaH
//////////////////////////////////////////////////////////////////////

#ifndef LU_HELPER_H
#define LU_HELPER_H

#include <cmath>
#include <algorithm>

using namespace std;

//////////////////////////////////////////////////////////////////////
// Helper function, compute eigenvalues of 3x3 matrix
//////////////////////////////////////////////////////////////////////

struct sLU
{
	float values[3][3];
	int pivsign;
	int piv[3];
};


int isNonsingular (sLU LU_);
sLU computeLU( float a[3][3]);
void solveLU3x3(sLU& A, float x[3], float b[3]);


#endif

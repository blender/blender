/** \file smoke/intern/EIGENVALUE_HELPER.h
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
// when being run in parallel. Required TNT JAMA::Eigenvalue libraries were
// converted into independent functions.
//		- MiikaH
//
//////////////////////////////////////////////////////////////////////
// Helper function, compute eigenvalues of 3x3 matrix
//////////////////////////////////////////////////////////////////////

#ifndef EIGENVAL_HELPER_H
#define EIGENVAL_HELPER_H

//#include "tnt/jama_eig.h"

#include <algorithm>
#include <cmath>

using namespace std;

//////////////////////////////////////////////////////////////////////
// eigenvalues of 3x3 non-symmetric matrix
//////////////////////////////////////////////////////////////////////


struct sEigenvalue
{
	int n;
	int issymmetric;
	float d[3];         /* real part */
	float e[3];         /* img part */
	float V[3][3];		/* Eigenvectors */

	float H[3][3];
   

    float ort[3];

	float cdivr;
	float cdivi;
};

void Eigentred2(sEigenvalue& eval);

void Eigencdiv(sEigenvalue& eval, float xr, float xi, float yr, float yi);

void Eigentql2 (sEigenvalue& eval);

void Eigenorthes (sEigenvalue& eval);

void Eigenhqr2 (sEigenvalue& eval);

int computeEigenvalues3x3(float dout[3], float a[3][3]);


#endif

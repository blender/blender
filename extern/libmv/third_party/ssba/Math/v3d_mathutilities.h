// -*- C++ -*-
/*
Copyright (c) 2008 University of North Carolina at Chapel Hill

This file is part of SSBA (Simple Sparse Bundle Adjustment).

SSBA is free software: you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

SSBA is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License along
with SSBA. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef V3D_MATH_UTILITIES_H
#define V3D_MATH_UTILITIES_H

#include "Math/v3d_linear.h"
#include "Math/v3d_linear_utils.h"

#include <vector>

namespace V3D
{

   template <typename Vec, typename Mat>
   inline void
   createRotationMatrixRodriguez(Vec const& omega, Mat& R)
   {
      assert(omega.size() == 3);
      assert(R.num_rows() == 3);
      assert(R.num_cols() == 3);

      double const theta = norm_L2(omega);
      makeIdentityMatrix(R);
      if (fabs(theta) > 1e-6)
      {
         Matrix3x3d J, J2;
         makeCrossProductMatrix(omega, J);
         multiply_A_B(J, J, J2);
         double const c1 = sin(theta)/theta;
         double const c2 = (1-cos(theta))/(theta*theta);
         for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
               R[i][j] += c1*J[i][j] + c2*J2[i][j];
      }
   } // end createRotationMatrixRodriguez()

   template <typename T> inline double sqr(T x) { return x*x; }

} // namespace V3D

#endif

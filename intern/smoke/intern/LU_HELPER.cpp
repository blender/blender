/** \file smoke/intern/LU_HELPER.cpp
 *  \ingroup smoke
 */

#include "LU_HELPER.h"

int isNonsingular (sLU LU_) {
      for (int j = 0; j < 3; j++) {
         if (LU_.values[j][j] == 0)
            return 0;
      }
      return 1;
}

sLU computeLU( float a[3][3])
{
	sLU result;
	int m=3;
	int n=3;

	//float LU_[3][3];
	for (int i = 0; i < m; i++) {
			result.piv[i] = i;
			for (int j = 0; j < n; j++) result.values[i][j]=a[i][j];
      }

      result.pivsign = 1;
      //Real *LUrowi = 0;;
      //Array1D<Real> LUcolj(m);
	  //float *LUrowi = 0;
	  float LUcolj[3];

      // Outer loop.

      for (int j = 0; j < n; j++) {

         // Make a copy of the j-th column to localize references.

         for (int i = 0; i < m; i++) {
            LUcolj[i] = result.values[i][j];
         }

         // Apply previous transformations.

         for (int i = 0; i < m; i++) {
			 //float LUrowi[3];
			 //LUrowi = result.values[i];

            // Most of the time is spent in the following dot product.

            int kmax = min(i,j);
            double s = 0.0;
            for (int k = 0; k < kmax; k++) {
               s += (double)(result.values[i][k]*LUcolj[k]);
            }

            result.values[i][j] = LUcolj[i] -= (float)s;
         }
   
         // Find pivot and exchange if necessary.

         int p = j;
         for (int i = j+1; i < m; i++) {
            if (abs(LUcolj[i]) > abs(LUcolj[p])) {
               p = i;
            }
         }
         if (p != j) {
		    int k=0;
            for (k = 0; k < n; k++) {
               double t = result.values[p][k]; 
			   result.values[p][k] = result.values[j][k]; 
			   result.values[j][k] = t;
            }
            k = result.piv[p]; 
			result.piv[p] = result.piv[j]; 
			result.piv[j] = k;
            result.pivsign = -result.pivsign;
         }

         // Compute multipliers.
         
         if ((j < m) && (result.values[j][j] != 0.0f)) {
            for (int i = j+1; i < m; i++) {
               result.values[i][j] /= result.values[j][j];
            }
         }
      }

	  return result;
}

void solveLU3x3(sLU& A, float x[3], float b[3])
{
  //TNT::Array1D<float> jamaB = TNT::Array1D<float>(3, &b[0]);
  //TNT::Array1D<float> jamaX = A.solve(jamaB);

	
  // Solve A, B

	{
      if (!isNonsingular(A)) {
        x[0]=0.0f;
		x[1]=0.0f;
		x[2]=0.0f;
		return;
      }


	  //Array1D<Real> Ax = permute_copy(b, piv);
	  float Ax[3];

    // permute copy: b , A.piv
	{
         for (int i = 0; i < 3; i++) 
               Ax[i] = b[A.piv[i]];
	}

      // Solve L*Y = B(piv)
      for (int k = 0; k < 3; k++) {
         for (int i = k+1; i < 3; i++) {
               Ax[i] -= Ax[k]*A.values[i][k];
            }
         }
      
	  // Solve U*X = Y;
      for (int k = 2; k >= 0; k--) {
            Ax[k] /= A.values[k][k];
      		for (int i = 0; i < k; i++) 
            	Ax[i] -= Ax[k]*A.values[i][k];
      }
     

		x[0] = Ax[0];
		x[1] = Ax[1];
		x[2] = Ax[2];
      return;
	}
}

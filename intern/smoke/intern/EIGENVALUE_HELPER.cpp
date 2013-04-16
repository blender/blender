/** \file smoke/intern/EIGENVALUE_HELPER.cpp
 *  \ingroup smoke
 */

#include "EIGENVALUE_HELPER.h"


void Eigentred2(sEigenvalue& eval) {

   //  This is derived from the Algol procedures tred2 by
   //  Bowdler, Martin, Reinsch, and Wilkinson, Handbook for
   //  Auto. Comp., Vol.ii-Linear Algebra, and the corresponding
   //  Fortran subroutine in EISPACK.

	  int n=eval.n;

      for (int j = 0; j < n; j++) {
         eval.d[j] = eval.V[n-1][j];
      }

      // Householder reduction to tridiagonal form.
   
      for (int i = n-1; i > 0; i--) {
   
         // Scale to avoid under/overflow.
   
         float scale = 0.0;
         float h = 0.0;
         for (int k = 0; k < i; k++) {
            scale = scale + fabs(eval.d[k]);
         }
         if (scale == 0.0f) {
            eval.e[i] = eval.d[i-1];
            for (int j = 0; j < i; j++) {
               eval.d[j] = eval.V[i-1][j];
               eval.V[i][j] = 0.0;
               eval.V[j][i] = 0.0;
            }
         } else {
   
            // Generate Householder vector.
   
            for (int k = 0; k < i; k++) {
               eval.d[k] /= scale;
               h += eval.d[k] * eval.d[k];
            }
            float f = eval.d[i-1];
            float g = sqrt(h);
            if (f > 0) {
               g = -g;
            }
            eval.e[i] = scale * g;
            h = h - f * g;
            eval.d[i-1] = f - g;
            for (int j = 0; j < i; j++) {
               eval.e[j] = 0.0;
            }
   
            // Apply similarity transformation to remaining columns.
   
            for (int j = 0; j < i; j++) {
               f = eval.d[j];
               eval.V[j][i] = f;
               g = eval.e[j] + eval.V[j][j] * f;
               for (int k = j+1; k <= i-1; k++) {
                  g += eval.V[k][j] * eval.d[k];
                  eval.e[k] += eval.V[k][j] * f;
               }
               eval.e[j] = g;
            }
            f = 0.0;
            for (int j = 0; j < i; j++) {
               eval.e[j] /= h;
               f += eval.e[j] * eval.d[j];
            }
            float hh = f / (h + h);
            for (int j = 0; j < i; j++) {
               eval.e[j] -= hh * eval.d[j];
            }
            for (int j = 0; j < i; j++) {
               f = eval.d[j];
               g = eval.e[j];
               for (int k = j; k <= i-1; k++) {
                  eval.V[k][j] -= (f * eval.e[k] + g * eval.d[k]);
               }
               eval.d[j] = eval.V[i-1][j];
               eval.V[i][j] = 0.0;
            }
         }
         eval.d[i] = h;
      }
   
      // Accumulate transformations.
   
      for (int i = 0; i < n-1; i++) {
         eval.V[n-1][i] = eval.V[i][i];
         eval.V[i][i] = 1.0;
         float h = eval.d[i+1];
         if (h != 0.0f) {
            for (int k = 0; k <= i; k++) {
               eval.d[k] = eval.V[k][i+1] / h;
            }
            for (int j = 0; j <= i; j++) {
               float g = 0.0;
               for (int k = 0; k <= i; k++) {
                  g += eval.V[k][i+1] * eval.V[k][j];
               }
               for (int k = 0; k <= i; k++) {
                  eval.V[k][j] -= g * eval.d[k];
               }
            }
         }
         for (int k = 0; k <= i; k++) {
            eval.V[k][i+1] = 0.0;
         }
      }
      for (int j = 0; j < n; j++) {
         eval.d[j] = eval.V[n-1][j];
         eval.V[n-1][j] = 0.0;
      }
      eval.V[n-1][n-1] = 1.0;
      eval.e[0] = 0.0;
}

void Eigencdiv(sEigenvalue& eval, float xr, float xi, float yr, float yi) {
      float r,d;
      if (fabs(yr) > fabs(yi)) {
         r = yi/yr;
         d = yr + r*yi;
         eval.cdivr = (xr + r*xi)/d;
         eval.cdivi = (xi - r*xr)/d;
      } else {
         r = yr/yi;
         d = yi + r*yr;
         eval.cdivr = (r*xr + xi)/d;
         eval.cdivi = (r*xi - xr)/d;
      }
   }

void Eigentql2 (sEigenvalue& eval) {

   //  This is derived from the Algol procedures tql2, by
   //  Bowdler, Martin, Reinsch, and Wilkinson, Handbook for
   //  Auto. Comp., Vol.ii-Linear Algebra, and the corresponding
   //  Fortran subroutine in EISPACK.
   
	  int n=eval.n;

      for (int i = 1; i < n; i++) {
         eval.e[i-1] = eval.e[i];
      }
      eval.e[n-1] = 0.0;
   
      float f = 0.0;
      float tst1 = 0.0;
      float eps = pow(2.0,-52.0);
      for (int l = 0; l < n; l++) {

         // Find small subdiagonal element
   
         tst1 = max(tst1,fabs(eval.d[l]) + fabs(eval.e[l]));
         int m = l;

        // Original while-loop from Java code
         while (m < n) {
            if (fabs(eval.e[m]) <= eps*tst1) {
               break;
            }
            m++;
         }

   
         // If m == l, d[l] is an eigenvalue,
         // otherwise, iterate.
   
         if (m > l) {
            int iter = 0;
            do {
               iter = iter + 1;  // (Could check iteration count here.)
   
               // Compute implicit shift

               float g = eval.d[l];
               float p = (eval.d[l+1] - g) / (2.0f * eval.e[l]);
               float r = hypot(p,1.0);
               if (p < 0) {
                  r = -r;
               }
               eval.d[l] = eval.e[l] / (p + r);
               eval.d[l+1] = eval.e[l] * (p + r);
               float dl1 = eval.d[l+1];
               float h = g - eval.d[l];
               for (int i = l+2; i < n; i++) {
                  eval.d[i] -= h;
               }
               f = f + h;
   
               // Implicit QL transformation.
   
               p = eval.d[m];
               float c = 1.0;
               float c2 = c;
               float c3 = c;
               float el1 = eval.e[l+1];
               float s = 0.0;
               float s2 = 0.0;
               for (int i = m-1; i >= l; i--) {
                  c3 = c2;
                  c2 = c;
                  s2 = s;
                  g = c * eval.e[i];
                  h = c * p;
                  r = hypot(p,eval.e[i]);
                  eval.e[i+1] = s * r;
                  s = eval.e[i] / r;
                  c = p / r;
                  p = c * eval.d[i] - s * g;
                  eval.d[i+1] = h + s * (c * g + s * eval.d[i]);
   
                  // Accumulate transformation.
   
                  for (int k = 0; k < n; k++) {
                     h = eval.V[k][i+1];
                     eval.V[k][i+1] = s * eval.V[k][i] + c * h;
                     eval.V[k][i] = c * eval.V[k][i] - s * h;
                  }
               }
               p = -s * s2 * c3 * el1 * eval.e[l] / dl1;
               eval.e[l] = s * p;
               eval.d[l] = c * p;
   
               // Check for convergence.
   
            } while (fabs(eval.e[l]) > eps*tst1);
         }
         eval.d[l] = eval.d[l] + f;
         eval.e[l] = 0.0;
      }
     
      // Sort eigenvalues and corresponding vectors.
   
      for (int i = 0; i < n-1; i++) {
         int k = i;
         float p = eval.d[i];
         for (int j = i+1; j < n; j++) {
            if (eval.d[j] < p) {
               k = j;
               p = eval.d[j];
            }
         }
         if (k != i) {
            eval.d[k] = eval.d[i];
            eval.d[i] = p;
            for (int j = 0; j < n; j++) {
               p = eval.V[j][i];
               eval.V[j][i] = eval.V[j][k];
               eval.V[j][k] = p;
            }
         }
      }
}

void Eigenorthes (sEigenvalue& eval) {
   
      //  This is derived from the Algol procedures orthes and ortran,
      //  by Martin and Wilkinson, Handbook for Auto. Comp.,
      //  Vol.ii-Linear Algebra, and the corresponding
      //  Fortran subroutines in EISPACK.
   
	  int n=eval.n;

      int low = 0;
      int high = n-1;
   
      for (int m = low+1; m <= high-1; m++) {
   
         // Scale column.
   
         float scale = 0.0;
         for (int i = m; i <= high; i++) {
            scale = scale + fabs(eval.H[i][m-1]);
         }
         if (scale != 0.0f) {
   
            // Compute Householder transformation.
   
            float h = 0.0;
            for (int i = high; i >= m; i--) {
               eval.ort[i] = eval.H[i][m-1]/scale;
               h += eval.ort[i] * eval.ort[i];
            }
            float g = sqrt(h);
            if (eval.ort[m] > 0) {
               g = -g;
            }
            h = h - eval.ort[m] * g;
            eval.ort[m] = eval.ort[m] - g;
   
            // Apply Householder similarity transformation
            // H = (I-u*u'/h)*H*(I-u*u')/h)
   
            for (int j = m; j < n; j++) {
               float f = 0.0;
               for (int i = high; i >= m; i--) {
                  f += eval.ort[i]*eval.H[i][j];
               }
               f = f/h;
               for (int i = m; i <= high; i++) {
                  eval.H[i][j] -= f*eval.ort[i];
               }
           }
   
           for (int i = 0; i <= high; i++) {
               float f = 0.0;
               for (int j = high; j >= m; j--) {
                  f += eval.ort[j]*eval.H[i][j];
               }
               f = f/h;
               for (int j = m; j <= high; j++) {
                  eval.H[i][j] -= f*eval.ort[j];
               }
            }
            eval.ort[m] = scale*eval.ort[m];
            eval.H[m][m-1] = scale*g;
         }
      }
   
      // Accumulate transformations (Algol's ortran).

      for (int i = 0; i < n; i++) {
         for (int j = 0; j < n; j++) {
            eval.V[i][j] = (i == j ? 1.0 : 0.0);
         }
      }

      for (int m = high-1; m >= low+1; m--) {
         if (eval.H[m][m-1] != 0.0f) {
            for (int i = m+1; i <= high; i++) {
               eval.ort[i] = eval.H[i][m-1];
            }
            for (int j = m; j <= high; j++) {
               float g = 0.0;
               for (int i = m; i <= high; i++) {
                  g += eval.ort[i] * eval.V[i][j];
               }
               // Double division avoids possible underflow
               g = (g / eval.ort[m]) / eval.H[m][m-1];
               for (int i = m; i <= high; i++) {
                  eval.V[i][j] += g * eval.ort[i];
               }
            }
         }
      }
   }

void Eigenhqr2 (sEigenvalue& eval) {
   
      //  This is derived from the Algol procedure hqr2,
      //  by Martin and Wilkinson, Handbook for Auto. Comp.,
      //  Vol.ii-Linear Algebra, and the corresponding
      //  Fortran subroutine in EISPACK.
   
      // Initialize
   
      int nn = eval.n;
      int n = nn-1;
      int low = 0;
      int high = nn-1;
      float eps = pow(2.0,-52.0);
      float exshift = 0.0;
      float p=0,q=0,r=0,s=0,z=0,t,w,x,y;
   
      // Store roots isolated by balanc and compute matrix norm
   
      float norm = 0.0;
      for (int i = 0; i < nn; i++) {
         if ((i < low) || (i > high)) {
            eval.d[i] = eval.H[i][i];
            eval.e[i] = 0.0;
         }
         for (int j = max(i-1,0); j < nn; j++) {
            norm = norm + fabs(eval.H[i][j]);
         }
      }
   
      // Outer loop over eigenvalue index
   
      int iter = 0;
		int totIter = 0;
      while (n >= low) {

			// NT limit no. of iterations
			totIter++;
			if(totIter>100) {
				//if(totIter>15) std::cout<<"!!!!iter ABORT !!!!!!! "<<totIter<<"\n"; 
				// NT hack/fix, return large eigenvalues
				for (int i = 0; i < nn; i++) {
					eval.d[i] = 10000.;
					eval.e[i] = 10000.;
				}
				return;
			}
   
         // Look for single small sub-diagonal element
   
         int l = n;
         while (l > low) {
            s = fabs(eval.H[l-1][l-1]) + fabs(eval.H[l][l]);
            if (s == 0.0f) {
               s = norm;
            }
            if (fabs(eval.H[l][l-1]) < eps * s) {
               break;
            }
            l--;
         }
       
         // Check for convergence
         // One root found
   
         if (l == n) {
            eval.H[n][n] = eval.H[n][n] + exshift;
            eval.d[n] = eval.H[n][n];
            eval.e[n] = 0.0;
            n--;
            iter = 0;
   
         // Two roots found
   
         } else if (l == n-1) {
            w = eval.H[n][n-1] * eval.H[n-1][n];
            p = (eval.H[n-1][n-1] - eval.H[n][n]) / 2.0f;
            q = p * p + w;
            z = sqrt(fabs(q));
            eval.H[n][n] = eval.H[n][n] + exshift;
            eval.H[n-1][n-1] = eval.H[n-1][n-1] + exshift;
            x = eval.H[n][n];
   
            // float pair
   
            if (q >= 0) {
               if (p >= 0) {
                  z = p + z;
               } else {
                  z = p - z;
               }
               eval.d[n-1] = x + z;
               eval.d[n] = eval.d[n-1];
               if (z != 0.0f) {
                  eval.d[n] = x - w / z;
               }
               eval.e[n-1] = 0.0;
               eval.e[n] = 0.0;
               x = eval.H[n][n-1];
               s = fabs(x) + fabs(z);
               p = x / s;
               q = z / s;
               r = sqrt(p * p+q * q);
               p = p / r;
               q = q / r;
   
               // Row modification
   
               for (int j = n-1; j < nn; j++) {
                  z = eval.H[n-1][j];
                  eval.H[n-1][j] = q * z + p * eval.H[n][j];
                  eval.H[n][j] = q * eval.H[n][j] - p * z;
               }
   
               // Column modification
   
               for (int i = 0; i <= n; i++) {
                  z = eval.H[i][n-1];
                  eval.H[i][n-1] = q * z + p * eval.H[i][n];
                  eval.H[i][n] = q * eval.H[i][n] - p * z;
               }
   
               // Accumulate transformations
   
               for (int i = low; i <= high; i++) {
                  z = eval.V[i][n-1];
                  eval.V[i][n-1] = q * z + p * eval.V[i][n];
                  eval.V[i][n] = q * eval.V[i][n] - p * z;
               }
   
            // Complex pair
   
            } else {
               eval.d[n-1] = x + p;
               eval.d[n] = x + p;
               eval.e[n-1] = z;
               eval.e[n] = -z;
            }
            n = n - 2;
            iter = 0;
   
         // No convergence yet
   
         } else {
   
            // Form shift
   
            x = eval.H[n][n];
            y = 0.0;
            w = 0.0;
            if (l < n) {
               y = eval.H[n-1][n-1];
               w = eval.H[n][n-1] * eval.H[n-1][n];
            }
   
            // Wilkinson's original ad hoc shift
   
            if (iter == 10) {
               exshift += x;
               for (int i = low; i <= n; i++) {
                  eval.H[i][i] -= x;
               }
               s = fabs(eval.H[n][n-1]) + fabs(eval.H[n-1][n-2]);
               x = y = 0.75f * s;
               w = -0.4375f * s * s;
            }

            // MATLAB's new ad hoc shift

            if (iter == 30) {
                s = (y - x) / 2.0f;
                s = s * s + w;
                if (s > 0) {
                    s = sqrt(s);
                    if (y < x) {
                       s = -s;
                    }
                    s = x - w / ((y - x) / 2.0f + s);
                    for (int i = low; i <= n; i++) {
                       eval.H[i][i] -= s;
                    }
                    exshift += s;
                    x = y = w = 0.964;
                }
            }
   
            iter = iter + 1;   // (Could check iteration count here.)
   
            // Look for two consecutive small sub-diagonal elements
   
            int m = n-2;
            while (m >= l) {
               z = eval.H[m][m];
               r = x - z;
               s = y - z;
               p = (r * s - w) / eval.H[m+1][m] + eval.H[m][m+1];
               q = eval.H[m+1][m+1] - z - r - s;
               r = eval.H[m+2][m+1];
               s = fabs(p) + fabs(q) + fabs(r);
               p = p / s;
               q = q / s;
               r = r / s;
               if (m == l) {
                  break;
               }
               if (fabs(eval.H[m][m-1]) * (fabs(q) + fabs(r)) <
                  eps * (fabs(p) * (fabs(eval.H[m-1][m-1]) + fabs(z) +
                  fabs(eval.H[m+1][m+1])))) {
                     break;
               }
               m--;
            }
   
            for (int i = m+2; i <= n; i++) {
               eval.H[i][i-2] = 0.0;
               if (i > m+2) {
                  eval.H[i][i-3] = 0.0;
               }
            }
   
            // Double QR step involving rows l:n and columns m:n
   
            for (int k = m; k <= n-1; k++) {
               int notlast = (k != n-1);
               if (k != m) {
                  p = eval.H[k][k-1];
                  q = eval.H[k+1][k-1];
                  r = (notlast ? eval.H[k+2][k-1] : 0.0f);
                  x = fabs(p) + fabs(q) + fabs(r);
                  if (x != 0.0f) {
                     p = p / x;
                     q = q / x;
                     r = r / x;
                  }
               }
               if (x == 0.0f) {
                  break;
               }
               s = sqrt(p * p + q * q + r * r);
               if (p < 0) {
                  s = -s;
               }
               if (s != 0) {
                  if (k != m) {
                     eval.H[k][k-1] = -s * x;
                  } else if (l != m) {
                     eval.H[k][k-1] = -eval.H[k][k-1];
                  }
                  p = p + s;
                  x = p / s;
                  y = q / s;
                  z = r / s;
                  q = q / p;
                  r = r / p;
   
                  // Row modification
   
                  for (int j = k; j < nn; j++) {
                     p = eval.H[k][j] + q * eval.H[k+1][j];
                     if (notlast) {
                        p = p + r * eval.H[k+2][j];
                        eval.H[k+2][j] = eval.H[k+2][j] - p * z;
                     }
                     eval.H[k][j] = eval.H[k][j] - p * x;
                     eval.H[k+1][j] = eval.H[k+1][j] - p * y;
                  }
   
                  // Column modification
   
                  for (int i = 0; i <= min(n,k+3); i++) {
                     p = x * eval.H[i][k] + y * eval.H[i][k+1];
                     if (notlast) {
                        p = p + z * eval.H[i][k+2];
                        eval.H[i][k+2] = eval.H[i][k+2] - p * r;
                     }
                     eval.H[i][k] = eval.H[i][k] - p;
                     eval.H[i][k+1] = eval.H[i][k+1] - p * q;
                  }
   
                  // Accumulate transformations
   
                  for (int i = low; i <= high; i++) {
                     p = x * eval.V[i][k] + y * eval.V[i][k+1];
                     if (notlast) {
                        p = p + z * eval.V[i][k+2];
                        eval.V[i][k+2] = eval.V[i][k+2] - p * r;
                     }
                     eval.V[i][k] = eval.V[i][k] - p;
                     eval.V[i][k+1] = eval.V[i][k+1] - p * q;
                  }
               }  // (s != 0)
            }  // k loop
         }  // check convergence
      }  // while (n >= low)
		//if(totIter>15) std::cout<<"!!!!iter "<<totIter<<"\n";
      
      // Backsubstitute to find vectors of upper triangular form

      if (norm == 0.0f) {
         return;
      }
   
      for (n = nn-1; n >= 0; n--) {
         p = eval.d[n];
         q = eval.e[n];
   
         // float vector
   
         if (q == 0) {
            int l = n;
            eval.H[n][n] = 1.0;
            for (int i = n-1; i >= 0; i--) {
               w = eval.H[i][i] - p;
               r = 0.0;
               for (int j = l; j <= n; j++) {
                  r = r + eval.H[i][j] * eval.H[j][n];
               }
               if (eval.e[i] < 0.0f) {
                  z = w;
                  s = r;
               } else {
                  l = i;
                  if (eval.e[i] == 0.0f) {
                     if (w != 0.0f) {
                        eval.H[i][n] = -r / w;
                     } else {
                        eval.H[i][n] = -r / (eps * norm);
                     }
   
                  // Solve real equations
   
                  } else {
                     x = eval.H[i][i+1];
                     y = eval.H[i+1][i];
                     q = (eval.d[i] - p) * (eval.d[i] - p) + eval.e[i] * eval.e[i];
                     t = (x * s - z * r) / q;
                     eval.H[i][n] = t;
                     if (fabs(x) > fabs(z)) {
                        eval.H[i+1][n] = (-r - w * t) / x;
                     } else {
                        eval.H[i+1][n] = (-s - y * t) / z;
                     }
                  }
   
                  // Overflow control
   
                  t = fabs(eval.H[i][n]);
                  if ((eps * t) * t > 1) {
                     for (int j = i; j <= n; j++) {
                        eval.H[j][n] = eval.H[j][n] / t;
                     }
                  }
               }
            }
   
         // Complex vector
   
         } else if (q < 0) {
            int l = n-1;

            // Last vector component imaginary so matrix is triangular
   
            if (fabs(eval.H[n][n-1]) > fabs(eval.H[n-1][n])) {
               eval.H[n-1][n-1] = q / eval.H[n][n-1];
               eval.H[n-1][n] = -(eval.H[n][n] - p) / eval.H[n][n-1];
            } else {
               Eigencdiv(eval, 0.0,-eval.H[n-1][n],eval.H[n-1][n-1]-p,q);
               eval.H[n-1][n-1] = eval.cdivr;
               eval.H[n-1][n] = eval.cdivi;
            }
            eval.H[n][n-1] = 0.0;
            eval.H[n][n] = 1.0;
            for (int i = n-2; i >= 0; i--) {
               float ra,sa,vr,vi;
               ra = 0.0;
               sa = 0.0;
               for (int j = l; j <= n; j++) {
                  ra = ra + eval.H[i][j] * eval.H[j][n-1];
                  sa = sa + eval.H[i][j] * eval.H[j][n];
               }
               w = eval.H[i][i] - p;
   
               if (eval.e[i] < 0.0f) {
                  z = w;
                  r = ra;
                  s = sa;
               } else {
                  l = i;
                  if (eval.e[i] == 0) {
                     Eigencdiv(eval,-ra,-sa,w,q);
                     eval.H[i][n-1] = eval.cdivr;
                     eval.H[i][n] = eval.cdivi;
                  } else {
   
                     // Solve complex equations
   
                     x = eval.H[i][i+1];
                     y = eval.H[i+1][i];
                     vr = (eval.d[i] - p) * (eval.d[i] - p) + eval.e[i] * eval.e[i] - q * q;
                     vi = (eval.d[i] - p) * 2.0f * q;
                     if ((vr == 0.0f) && (vi == 0.0f)) {
                        vr = eps * norm * (fabs(w) + fabs(q) +
                        fabs(x) + fabs(y) + fabs(z));
                     }
                     Eigencdiv(eval, x*r-z*ra+q*sa,x*s-z*sa-q*ra,vr,vi);
                     eval.H[i][n-1] = eval.cdivr;
                     eval.H[i][n] = eval.cdivi;
                     if (fabs(x) > (fabs(z) + fabs(q))) {
                        eval.H[i+1][n-1] = (-ra - w * eval.H[i][n-1] + q * eval.H[i][n]) / x;
                        eval.H[i+1][n] = (-sa - w * eval.H[i][n] - q * eval.H[i][n-1]) / x;
                     } else {
                        Eigencdiv(eval, -r-y*eval.H[i][n-1],-s-y*eval.H[i][n],z,q);
                        eval.H[i+1][n-1] = eval.cdivr;
                        eval.H[i+1][n] = eval.cdivi;
                     }
                  }
   
                  // Overflow control

                  t = max(fabs(eval.H[i][n-1]),fabs(eval.H[i][n]));
                  if ((eps * t) * t > 1) {
                     for (int j = i; j <= n; j++) {
                        eval.H[j][n-1] = eval.H[j][n-1] / t;
                        eval.H[j][n] = eval.H[j][n] / t;
                     }
                  }
               }
            }
         }
      }
   
      // Vectors of isolated roots
   
      for (int i = 0; i < nn; i++) {
         if (i < low || i > high) {
            for (int j = i; j < nn; j++) {
               eval.V[i][j] = eval.H[i][j];
            }
         }
      }
   
      // Back transformation to get eigenvectors of original matrix
   
      for (int j = nn-1; j >= low; j--) {
         for (int i = low; i <= high; i++) {
            z = 0.0;
            for (int k = low; k <= min(j,high); k++) {
               z = z + eval.V[i][k] * eval.H[k][j];
            }
            eval.V[i][j] = z;
         }
      }
}



int computeEigenvalues3x3(
		float dout[3], 
		float a[3][3])
{
  /*TNT::Array2D<float> A = TNT::Array2D<float>(3,3, &a[0][0]);
  TNT::Array1D<float> eig = TNT::Array1D<float>(3);
  TNT::Array1D<float> eigImag = TNT::Array1D<float>(3);
  JAMA::Eigenvalue<float> jeig = JAMA::Eigenvalue<float>(A);*/

	sEigenvalue jeig;

	// Compute the values
	{
		jeig.n = 3;
		int n=3;
      //V = Array2D<float>(n,n);
      //d = Array1D<float>(n);
      //e = Array1D<float>(n);
		for (int y=0; y<3; y++)
		 {
			 jeig.d[y]=0.0f;
			 jeig.e[y]=0.0f;
			 for (int t=0; t<3; t++) jeig.V[y][t]=0.0f;
		 }

      jeig.issymmetric = 1;
      for (int j = 0; (j < 3) && jeig.issymmetric; j++) {
         for (int i = 0; (i < 3) && jeig.issymmetric; i++) {
            jeig.issymmetric = (a[i][j] == a[j][i]);
         }
      }

      if (jeig.issymmetric) {
         for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
               jeig.V[i][j] = a[i][j];
            }
         }
   
         // Tridiagonalize.
         Eigentred2(jeig);
   
         // Diagonalize.
         Eigentql2(jeig);

      } else {
         //H = TNT::Array2D<float>(n,n);
	     for (int y=0; y<3; y++)
		 {
			 jeig.ort[y]=0.0f;
			 for (int t=0; t<3; t++) jeig.H[y][t]=0.0f;
		 }
         //ort = TNT::Array1D<float>(n);
         
         for (int j = 0; j < n; j++) {
            for (int i = 0; i < n; i++) {
               jeig.H[i][j] = a[i][j];
            }
         }
   
         // Reduce to Hessenberg form.
         Eigenorthes(jeig);
   
         // Reduce Hessenberg to real Schur form.
         Eigenhqr2(jeig);
      }
   }

  //jeig.getfloatEigenvalues(eig);

  // complex ones
  //jeig.getImagEigenvalues(eigImag);
  dout[0]  = sqrt(jeig.d[0]*jeig.d[0] + jeig.e[0]*jeig.e[0]);
  dout[1]  = sqrt(jeig.d[1]*jeig.d[1] + jeig.e[1]*jeig.e[1]);
  dout[2]  = sqrt(jeig.d[2]*jeig.d[2] + jeig.e[2]*jeig.e[2]);
  return 0;
}

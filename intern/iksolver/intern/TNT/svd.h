/**
 * $Id$
 */

#ifndef SVD_H

#define SVD_H

// Compute the Single Value Decomposition of an arbitrary matrix A
// That is compute the 3 matrices U,W,V with U column orthogonal (m,n) 
// ,W a diagonal matrix and V an orthogonal square matrix s.t. 
// A = U.W.Vt. From this decomposition it is trivial to compute the 
// inverse of A as Ainv = V.Winv.tranpose(U).
//
// s = diagonal elements of W
// work1 = workspace, length must be A.num_rows
// work2 = workspace, length must be A.num_cols

#include "tntmath.h"

#define SVD_MAX_ITER 200

namespace TNT
{

template <class MaTRiX, class VecToR >
void SVD(MaTRiX &A, MaTRiX &U, VecToR &s, MaTRiX &V, VecToR &work1, VecToR &work2, int maxiter=SVD_MAX_ITER) {

	int m = A.num_rows();
	int n = A.num_cols();
	int nu = min(m,n);

	VecToR& work = work1;
	VecToR& e = work2;

	U = 0;
	s = 0;

	int i=0, j=0, k=0;

	// Reduce A to bidiagonal form, storing the diagonal elements
	// in s and the super-diagonal elements in e.

	int nct = min(m-1,n);
	int nrt = max(0,min(n-2,m));
	for (k = 0; k < max(nct,nrt); k++) {
		if (k < nct) {

			// Compute the transformation for the k-th column and
			// place the k-th diagonal in s[k].
			// Compute 2-norm of k-th column without under/overflow.
			s[k] = 0;
			for (i = k; i < m; i++) {
				s[k] = hypot(s[k],A[i][k]);
			}
			if (s[k] != 0.0) {
				if (A[k][k] < 0.0) {
					s[k] = -s[k];
				}
				for (i = k; i < m; i++) {
					A[i][k] /= s[k];
				}
				A[k][k] += 1.0;
			}
			s[k] = -s[k];
		}
		for (j = k+1; j < n; j++) {
			if ((k < nct) && (s[k] != 0.0))  {

			// Apply the transformation.

				typename MaTRiX::value_type t = 0;
				for (i = k; i < m; i++) {
					t += A[i][k]*A[i][j];
				}
				t = -t/A[k][k];
				for (i = k; i < m; i++) {
					A[i][j] += t*A[i][k];
				}
			}

			// Place the k-th row of A into e for the
			// subsequent calculation of the row transformation.

			e[j] = A[k][j];
		}
		if (k < nct) {

			// Place the transformation in U for subsequent back
			// multiplication.

			for (i = k; i < m; i++)
				U[i][k] = A[i][k];
		}
		if (k < nrt) {

			// Compute the k-th row transformation and place the
			// k-th super-diagonal in e[k].
			// Compute 2-norm without under/overflow.
			e[k] = 0;
			for (i = k+1; i < n; i++) {
				e[k] = hypot(e[k],e[i]);
			}
			if (e[k] != 0.0) {
				if (e[k+1] < 0.0) {
					e[k] = -e[k];
				}
				for (i = k+1; i < n; i++) {
					e[i] /= e[k];
				}
				e[k+1] += 1.0;
			}
			e[k] = -e[k];
			if ((k+1 < m) & (e[k] != 0.0)) {

			// Apply the transformation.

				for (i = k+1; i < m; i++) {
					work[i] = 0.0;
				}
				for (j = k+1; j < n; j++) {
					for (i = k+1; i < m; i++) {
						work[i] += e[j]*A[i][j];
					}
				}
				for (j = k+1; j < n; j++) {
					typename MaTRiX::value_type t = -e[j]/e[k+1];
					for (i = k+1; i < m; i++) {
						A[i][j] += t*work[i];
					}
				}
			}

			// Place the transformation in V for subsequent
			// back multiplication.

			for (i = k+1; i < n; i++)
				V[i][k] = e[i];
		}
	}

	// Set up the final bidiagonal matrix or order p.

	int p = min(n,m+1);
	if (nct < n) {
		s[nct] = A[nct][nct];
	}
	if (m < p) {
		s[p-1] = 0.0;
	}
	if (nrt+1 < p) {
		e[nrt] = A[nrt][p-1];
	}
	e[p-1] = 0.0;

	// If required, generate U.

	for (j = nct; j < nu; j++) {
		for (i = 0; i < m; i++) {
			U[i][j] = 0.0;
		}
		U[j][j] = 1.0;
	}
	for (k = nct-1; k >= 0; k--) {
		if (s[k] != 0.0) {
			for (j = k+1; j < nu; j++) {
				typename MaTRiX::value_type t = 0;
				for (i = k; i < m; i++) {
					t += U[i][k]*U[i][j];
				}
				t = -t/U[k][k];
				for (i = k; i < m; i++) {
					U[i][j] += t*U[i][k];
				}
			}
			for (i = k; i < m; i++ ) {
				U[i][k] = -U[i][k];
			}
			U[k][k] = 1.0 + U[k][k];
			for (i = 0; i < k-1; i++) {
				U[i][k] = 0.0;
			}
		} else {
			for (i = 0; i < m; i++) {
				U[i][k] = 0.0;
			}
			U[k][k] = 1.0;
		}
	}

	// If required, generate V.

	for (k = n-1; k >= 0; k--) {
		if ((k < nrt) & (e[k] != 0.0)) {
			for (j = k+1; j < nu; j++) {
				typename MaTRiX::value_type t = 0;
				for (i = k+1; i < n; i++) {
					t += V[i][k]*V[i][j];
				}
				t = -t/V[k+1][k];
				for (i = k+1; i < n; i++) {
					V[i][j] += t*V[i][k];
				}
			}
		}
		for (i = 0; i < n; i++) {
			V[i][k] = 0.0;
		}
		V[k][k] = 1.0;
	}

	// Main iteration loop for the singular values.

	int pp = p-1;
	int iter = 0;
	typename MaTRiX::value_type eps = pow(2.0,-52.0);
	while (p > 0) {
		int kase=0;
		k=0;

		// Test for maximum iterations to avoid infinite loop
		if(maxiter == 0)
			break;
		maxiter--;

		// This section of the program inspects for
		// negligible elements in the s and e arrays.  On
		// completion the variables kase and k are set as follows.

		// kase = 1	  if s(p) and e[k-1] are negligible and k<p
		// kase = 2	  if s(k) is negligible and k<p
		// kase = 3	  if e[k-1] is negligible, k<p, and
		//				  s(k), ..., s(p) are not negligible (qr step).
		// kase = 4	  if e(p-1) is negligible (convergence).

		for (k = p-2; k >= -1; k--) {
			if (k == -1) {
				break;
			}
			if (TNT::abs(e[k]) <= eps*(TNT::abs(s[k]) + TNT::abs(s[k+1]))) {
				e[k] = 0.0;
				break;
			}
		}
		if (k == p-2) {
			kase = 4;
		} else {
			int ks;
			for (ks = p-1; ks >= k; ks--) {
				if (ks == k) {
					break;
				}
				typename MaTRiX::value_type t = (ks != p ? TNT::abs(e[ks]) : 0.) + 
							  (ks != k+1 ? TNT::abs(e[ks-1]) : 0.);
				if (TNT::abs(s[ks]) <= eps*t)  {
					s[ks] = 0.0;
					break;
				}
			}
			if (ks == k) {
				kase = 3;
			} else if (ks == p-1) {
				kase = 1;
			} else {
				kase = 2;
				k = ks;
			}
		}
		k++;

		// Perform the task indicated by kase.

		switch (kase) {

			// Deflate negligible s(p).

			case 1: {
				typename MaTRiX::value_type f = e[p-2];
				e[p-2] = 0.0;
				for (j = p-2; j >= k; j--) {
					typename MaTRiX::value_type t = hypot(s[j],f);
					typename MaTRiX::value_type cs = s[j]/t;
					typename MaTRiX::value_type sn = f/t;
					s[j] = t;
					if (j != k) {
						f = -sn*e[j-1];
						e[j-1] = cs*e[j-1];
					}

					for (i = 0; i < n; i++) {
						t = cs*V[i][j] + sn*V[i][p-1];
						V[i][p-1] = -sn*V[i][j] + cs*V[i][p-1];
						V[i][j] = t;
					}
				}
			}
			break;

			// Split at negligible s(k).

			case 2: {
				typename MaTRiX::value_type f = e[k-1];
				e[k-1] = 0.0;
				for (j = k; j < p; j++) {
					typename MaTRiX::value_type t = hypot(s[j],f);
					typename MaTRiX::value_type cs = s[j]/t;
					typename MaTRiX::value_type sn = f/t;
					s[j] = t;
					f = -sn*e[j];
					e[j] = cs*e[j];

					for (i = 0; i < m; i++) {
						t = cs*U[i][j] + sn*U[i][k-1];
						U[i][k-1] = -sn*U[i][j] + cs*U[i][k-1];
						U[i][j] = t;
					}
				}
			}
			break;

			// Perform one qr step.

			case 3: {

				// Calculate the shift.

				typename MaTRiX::value_type scale = max(max(max(max(
						  TNT::abs(s[p-1]),TNT::abs(s[p-2])),TNT::abs(e[p-2])), 
						  TNT::abs(s[k])),TNT::abs(e[k]));
				typename MaTRiX::value_type sp = s[p-1]/scale;
				typename MaTRiX::value_type spm1 = s[p-2]/scale;
				typename MaTRiX::value_type epm1 = e[p-2]/scale;
				typename MaTRiX::value_type sk = s[k]/scale;
				typename MaTRiX::value_type ek = e[k]/scale;
				typename MaTRiX::value_type b = ((spm1 + sp)*(spm1 - sp) + epm1*epm1)/2.0;
				typename MaTRiX::value_type c = (sp*epm1)*(sp*epm1);
				typename MaTRiX::value_type shift = 0.0;
				if ((b != 0.0) || (c != 0.0)) {
					shift = sqrt(b*b + c);
					if (b < 0.0) {
						shift = -shift;
					}
					shift = c/(b + shift);
				}
				typename MaTRiX::value_type f = (sk + sp)*(sk - sp) + shift;
				typename MaTRiX::value_type g = sk*ek;

				// Chase zeros.

				for (j = k; j < p-1; j++) {
					typename MaTRiX::value_type t = hypot(f,g);
					typename MaTRiX::value_type cs = f/t;
					typename MaTRiX::value_type sn = g/t;
					if (j != k) {
						e[j-1] = t;
					}
					f = cs*s[j] + sn*e[j];
					e[j] = cs*e[j] - sn*s[j];
					g = sn*s[j+1];
					s[j+1] = cs*s[j+1];

					for (i = 0; i < n; i++) {
						t = cs*V[i][j] + sn*V[i][j+1];
						V[i][j+1] = -sn*V[i][j] + cs*V[i][j+1];
						V[i][j] = t;
					}

					t = hypot(f,g);
					cs = f/t;
					sn = g/t;
					s[j] = t;
					f = cs*e[j] + sn*s[j+1];
					s[j+1] = -sn*e[j] + cs*s[j+1];
					g = sn*e[j+1];
					e[j+1] = cs*e[j+1];
					if (j < m-1) {
						for (i = 0; i < m; i++) {
							t = cs*U[i][j] + sn*U[i][j+1];
							U[i][j+1] = -sn*U[i][j] + cs*U[i][j+1];
							U[i][j] = t;
						}
					}
				}
				e[p-2] = f;
				iter = iter + 1;
			}
			break;

			// Convergence.

			case 4: {

				// Make the singular values positive.

				if (s[k] <= 0.0) {
					s[k] = (s[k] < 0.0 ? -s[k] : 0.0);

					for (i = 0; i <= pp; i++)
						V[i][k] = -V[i][k];
				}

				// Order the singular values.

				while (k < pp) {
					if (s[k] >= s[k+1]) {
						break;
					}
					typename MaTRiX::value_type t = s[k];
					s[k] = s[k+1];
					s[k+1] = t;
					if (k < n-1) {
						for (i = 0; i < n; i++) {
							t = V[i][k+1]; V[i][k+1] = V[i][k]; V[i][k] = t;
						}
					}
					if (k < m-1) {
						for (i = 0; i < m; i++) {
							t = U[i][k+1]; U[i][k+1] = U[i][k]; U[i][k] = t;
						}
					}
					k++;
				}
				iter = 0;
				p--;
			}
			break;
		}
	}
}

}

#endif


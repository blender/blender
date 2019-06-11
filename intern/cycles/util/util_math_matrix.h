/*
 * Copyright 2011-2017 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_MATH_MATRIX_H__
#define __UTIL_MATH_MATRIX_H__

CCL_NAMESPACE_BEGIN

#define MAT(A, size, row, col) A[(row) * (size) + (col)]

/* Variants that use a constant stride on GPUS. */
#ifdef __KERNEL_GPU__
#  define MATS(A, n, r, c, s) A[((r) * (n) + (c)) * (s)]
/* Element access when only the lower-triangular elements are stored. */
#  define MATHS(A, r, c, s) A[((r) * ((r) + 1) / 2 + (c)) * (s)]
#  define VECS(V, i, s) V[(i) * (s)]
#else
#  define MATS(A, n, r, c, s) MAT(A, n, r, c)
#  define MATHS(A, r, c, s) A[(r) * ((r) + 1) / 2 + (c)]
#  define VECS(V, i, s) V[i]
#endif

/* Zeroing helpers. */

ccl_device_inline void math_vector_zero(float *v, int n)
{
  for (int i = 0; i < n; i++) {
    v[i] = 0.0f;
  }
}

ccl_device_inline void math_matrix_zero(float *A, int n)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      MAT(A, n, row, col) = 0.0f;
    }
  }
}

/* Elementary vector operations. */

ccl_device_inline void math_vector_add(float *a, const float *ccl_restrict b, int n)
{
  for (int i = 0; i < n; i++) {
    a[i] += b[i];
  }
}

ccl_device_inline void math_vector_mul(float *a, const float *ccl_restrict b, int n)
{
  for (int i = 0; i < n; i++) {
    a[i] *= b[i];
  }
}

ccl_device_inline void math_vector_mul_strided(ccl_global float *a,
                                               const float *ccl_restrict b,
                                               int astride,
                                               int n)
{
  for (int i = 0; i < n; i++) {
    a[i * astride] *= b[i];
  }
}

ccl_device_inline void math_vector_scale(float *a, float b, int n)
{
  for (int i = 0; i < n; i++) {
    a[i] *= b;
  }
}

ccl_device_inline void math_vector_max(float *a, const float *ccl_restrict b, int n)
{
  for (int i = 0; i < n; i++) {
    a[i] = max(a[i], b[i]);
  }
}

ccl_device_inline void math_vec3_add(float3 *v, int n, float *x, float3 w)
{
  for (int i = 0; i < n; i++) {
    v[i] += w * x[i];
  }
}

ccl_device_inline void math_vec3_add_strided(
    ccl_global float3 *v, int n, float *x, float3 w, int stride)
{
  for (int i = 0; i < n; i++) {
    ccl_global float *elem = (ccl_global float *)(v + i * stride);
    atomic_add_and_fetch_float(elem + 0, w.x * x[i]);
    atomic_add_and_fetch_float(elem + 1, w.y * x[i]);
    atomic_add_and_fetch_float(elem + 2, w.z * x[i]);
  }
}

/* Elementary matrix operations.
 * Note: TriMatrix refers to a square matrix that is symmetric,
 * and therefore its upper-triangular part isn't stored. */

ccl_device_inline void math_trimatrix_add_diagonal(ccl_global float *A,
                                                   int n,
                                                   float val,
                                                   int stride)
{
  for (int row = 0; row < n; row++) {
    MATHS(A, row, row, stride) += val;
  }
}

/* Add Gramian matrix of v to A.
 * The Gramian matrix of v is vt*v, so element (i,j) is v[i]*v[j]. */
ccl_device_inline void math_matrix_add_gramian(float *A,
                                               int n,
                                               const float *ccl_restrict v,
                                               float weight)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      MAT(A, n, row, col) += v[row] * v[col] * weight;
    }
  }
}

/* Add Gramian matrix of v to A.
 * The Gramian matrix of v is vt*v, so element (i,j) is v[i]*v[j]. */
ccl_device_inline void math_trimatrix_add_gramian_strided(
    ccl_global float *A, int n, const float *ccl_restrict v, float weight, int stride)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      atomic_add_and_fetch_float(&MATHS(A, row, col, stride), v[row] * v[col] * weight);
    }
  }
}

ccl_device_inline void math_trimatrix_add_gramian(ccl_global float *A,
                                                  int n,
                                                  const float *ccl_restrict v,
                                                  float weight)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      MATHS(A, row, col, 1) += v[row] * v[col] * weight;
    }
  }
}

/* Transpose matrix A inplace. */
ccl_device_inline void math_matrix_transpose(ccl_global float *A, int n, int stride)
{
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < i; j++) {
      float temp = MATS(A, n, i, j, stride);
      MATS(A, n, i, j, stride) = MATS(A, n, j, i, stride);
      MATS(A, n, j, i, stride) = temp;
    }
  }
}

/* Solvers for matrix problems */

/* In-place Cholesky-Banachiewicz decomposition of the square, positive-definite matrix A
 * into a lower triangular matrix L so that A = L*L^T. A is being overwritten by L.
 * Also, only the lower triangular part of A is ever accessed. */
ccl_device void math_trimatrix_cholesky(ccl_global float *A, int n, int stride)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      float sum_col = MATHS(A, row, col, stride);
      for (int k = 0; k < col; k++) {
        sum_col -= MATHS(A, row, k, stride) * MATHS(A, col, k, stride);
      }
      if (row == col) {
        sum_col = sqrtf(max(sum_col, 0.0f));
      }
      else {
        sum_col /= MATHS(A, col, col, stride);
      }
      MATHS(A, row, col, stride) = sum_col;
    }
  }
}

/* Solve A*S=y for S given A and y,
 * where A is symmetrical positive-semi-definite and both inputs are destroyed in the process.
 *
 * We can apply Cholesky decomposition to find a lower triangular L so that L*Lt = A.
 * With that we get (L*Lt)*S = L*(Lt*S) = L*b = y, defining b as Lt*S.
 * Since L is lower triangular, finding b is relatively easy since y is known.
 * Then, the remaining problem is Lt*S = b, which again can be solved easily.
 *
 * This is useful for solving the normal equation S=inv(Xt*W*X)*Xt*W*y, since Xt*W*X is
 * symmetrical positive-semidefinite by construction,
 * so we can just use this function with A=Xt*W*X and y=Xt*W*y. */
ccl_device_inline void math_trimatrix_vec3_solve(ccl_global float *A,
                                                 ccl_global float3 *y,
                                                 int n,
                                                 int stride)
{
  /* Since the first entry of the design row is always 1, the upper-left element of XtWX is a good
   * heuristic for the amount of pixels considered (with weighting),
   * therefore the amount of correction is scaled based on it. */
  math_trimatrix_add_diagonal(A, n, 3e-7f * A[0], stride); /* Improve the numerical stability. */
  math_trimatrix_cholesky(A, n, stride);                   /* Replace A with L so that L*Lt = A. */

  /* Use forward substitution to solve L*b = y, replacing y by b. */
  for (int row = 0; row < n; row++) {
    float3 sum = VECS(y, row, stride);
    for (int col = 0; col < row; col++)
      sum -= MATHS(A, row, col, stride) * VECS(y, col, stride);
    VECS(y, row, stride) = sum / MATHS(A, row, row, stride);
  }

  /* Use backward substitution to solve Lt*S = b, replacing b by S. */
  for (int row = n - 1; row >= 0; row--) {
    float3 sum = VECS(y, row, stride);
    for (int col = row + 1; col < n; col++)
      sum -= MATHS(A, col, row, stride) * VECS(y, col, stride);
    VECS(y, row, stride) = sum / MATHS(A, row, row, stride);
  }
}

/* Perform the Jacobi Eigenvalue Methon on matrix A.
 * A is assumed to be a symmetrical matrix, therefore only the lower-triangular part is ever
 * accessed. The algorithm overwrites the contents of A.
 *
 * After returning, A will be overwritten with D, which is (almost) diagonal,
 * and V will contain the eigenvectors of the original A in its rows (!),
 * so that A = V^T*D*V. Therefore, the diagonal elements of D are the (sorted) eigenvalues of A.
 */
ccl_device void math_matrix_jacobi_eigendecomposition(float *A,
                                                      ccl_global float *V,
                                                      int n,
                                                      int v_stride)
{
  const float singular_epsilon = 1e-9f;

  for (int row = 0; row < n; row++) {
    for (int col = 0; col < n; col++) {
      MATS(V, n, row, col, v_stride) = (col == row) ? 1.0f : 0.0f;
    }
  }

  for (int sweep = 0; sweep < 8; sweep++) {
    float off_diagonal = 0.0f;
    for (int row = 1; row < n; row++) {
      for (int col = 0; col < row; col++) {
        off_diagonal += fabsf(MAT(A, n, row, col));
      }
    }
    if (off_diagonal < 1e-7f) {
      /* The matrix has nearly reached diagonal form.
       * Since the eigenvalues are only used to determine truncation, their exact values aren't
       * required - a relative error of a few ULPs won't matter at all. */
      break;
    }

    /* Set the threshold for the small element rotation skip in the first sweep:
     * Skip all elements that are less than a tenth of the average off-diagonal element. */
    float threshold = 0.2f * off_diagonal / (n * n);

    for (int row = 1; row < n; row++) {
      for (int col = 0; col < row; col++) {
        /* Perform a Jacobi rotation on this element that reduces it to zero. */
        float element = MAT(A, n, row, col);
        float abs_element = fabsf(element);

        /* If we're in a later sweep and the element already is very small,
         * just set it to zero and skip the rotation. */
        if (sweep > 3 && abs_element <= singular_epsilon * fabsf(MAT(A, n, row, row)) &&
            abs_element <= singular_epsilon * fabsf(MAT(A, n, col, col))) {
          MAT(A, n, row, col) = 0.0f;
          continue;
        }

        if (element == 0.0f) {
          continue;
        }

        /* If we're in one of the first sweeps and the element is smaller than the threshold,
         * skip it. */
        if (sweep < 3 && (abs_element < threshold)) {
          continue;
        }

        /* Determine rotation: The rotation is characterized by its angle phi - or,
         * in the actual implementation, sin(phi) and cos(phi).
         * To find those, we first compute their ratio - that might be unstable if the angle
         * approaches 90°, so there's a fallback for that case.
         * Then, we compute sin(phi) and cos(phi) themselves. */
        float singular_diff = MAT(A, n, row, row) - MAT(A, n, col, col);
        float ratio;
        if (abs_element > singular_epsilon * fabsf(singular_diff)) {
          float cot_2phi = 0.5f * singular_diff / element;
          ratio = 1.0f / (fabsf(cot_2phi) + sqrtf(1.0f + cot_2phi * cot_2phi));
          if (cot_2phi < 0.0f)
            ratio = -ratio; /* Copy sign. */
        }
        else {
          ratio = element / singular_diff;
        }

        float c = 1.0f / sqrtf(1.0f + ratio * ratio);
        float s = ratio * c;
        /* To improve numerical stability by avoiding cancellation, the update equations are
         * reformulized to use sin(phi) and tan(phi/2) instead. */
        float tan_phi_2 = s / (1.0f + c);

        /* Update the singular values in the diagonal. */
        float singular_delta = ratio * element;
        MAT(A, n, row, row) += singular_delta;
        MAT(A, n, col, col) -= singular_delta;

        /* Set the element itself to zero. */
        MAT(A, n, row, col) = 0.0f;

        /* Perform the actual rotations on the matrices. */
#define ROT(M, r1, c1, r2, c2, stride) \
  { \
    float M1 = MATS(M, n, r1, c1, stride); \
    float M2 = MATS(M, n, r2, c2, stride); \
    MATS(M, n, r1, c1, stride) -= s * (M2 + tan_phi_2 * M1); \
    MATS(M, n, r2, c2, stride) += s * (M1 - tan_phi_2 * M2); \
  }

        /* Split into three parts to ensure correct accesses since we only store the
         * lower-triangular part of A. */
        for (int i = 0; i < col; i++)
          ROT(A, col, i, row, i, 1);
        for (int i = col + 1; i < row; i++)
          ROT(A, i, col, row, i, 1);
        for (int i = row + 1; i < n; i++)
          ROT(A, i, col, i, row, 1);

        for (int i = 0; i < n; i++)
          ROT(V, col, i, row, i, v_stride);
#undef ROT
      }
    }
  }

  /* Sort eigenvalues and the associated eigenvectors. */
  for (int i = 0; i < n - 1; i++) {
    float v = MAT(A, n, i, i);
    int k = i;
    for (int j = i; j < n; j++) {
      if (MAT(A, n, j, j) >= v) {
        v = MAT(A, n, j, j);
        k = j;
      }
    }
    if (k != i) {
      /* Swap eigenvalues. */
      MAT(A, n, k, k) = MAT(A, n, i, i);
      MAT(A, n, i, i) = v;
      /* Swap eigenvectors. */
      for (int j = 0; j < n; j++) {
        float v = MATS(V, n, i, j, v_stride);
        MATS(V, n, i, j, v_stride) = MATS(V, n, k, j, v_stride);
        MATS(V, n, k, j, v_stride) = v;
      }
    }
  }
}

#ifdef __KERNEL_SSE3__
ccl_device_inline void math_vector_zero_sse(float4 *A, int n)
{
  for (int i = 0; i < n; i++) {
    A[i] = make_float4(0.0f);
  }
}

ccl_device_inline void math_matrix_zero_sse(float4 *A, int n)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      MAT(A, n, row, col) = make_float4(0.0f);
    }
  }
}

/* Add Gramian matrix of v to A.
 * The Gramian matrix of v is v^T*v, so element (i,j) is v[i]*v[j]. */
ccl_device_inline void math_matrix_add_gramian_sse(float4 *A,
                                                   int n,
                                                   const float4 *ccl_restrict v,
                                                   float4 weight)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      MAT(A, n, row, col) = MAT(A, n, row, col) + v[row] * v[col] * weight;
    }
  }
}

ccl_device_inline void math_vector_add_sse(float4 *V, int n, const float4 *ccl_restrict a)
{
  for (int i = 0; i < n; i++) {
    V[i] += a[i];
  }
}

ccl_device_inline void math_vector_mul_sse(float4 *V, int n, const float4 *ccl_restrict a)
{
  for (int i = 0; i < n; i++) {
    V[i] *= a[i];
  }
}

ccl_device_inline void math_vector_max_sse(float4 *a, const float4 *ccl_restrict b, int n)
{
  for (int i = 0; i < n; i++) {
    a[i] = max(a[i], b[i]);
  }
}

ccl_device_inline void math_matrix_hsum(float *A, int n, const float4 *ccl_restrict B)
{
  for (int row = 0; row < n; row++) {
    for (int col = 0; col <= row; col++) {
      MAT(A, n, row, col) = reduce_add(MAT(B, n, row, col))[0];
    }
  }
}
#endif

#undef MAT

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_MATRIX_H__ */

// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: yangfan34@lenovo.com (Lenovo Research Device+ Lab - Shanghai)
//
// Optimization for simple blas functions used in the Schur Eliminator.
// These are fairly basic implementations which already yield a significant
// speedup in the eliminator performance.

#ifndef CERES_INTERNAL_SMALL_BLAS_GENERIC_H_
#define CERES_INTERNAL_SMALL_BLAS_GENERIC_H_

namespace ceres {
namespace internal {

// The following macros are used to share code
#define CERES_GEMM_OPT_NAIVE_HEADER              \
  double c0 = 0.0;                               \
  double c1 = 0.0;                               \
  double c2 = 0.0;                               \
  double c3 = 0.0;                               \
  const double* pa = a;                          \
  const double* pb = b;                          \
  const int span = 4;                            \
  int col_r = col_a & (span - 1);                \
  int col_m = col_a - col_r;

#define CERES_GEMM_OPT_STORE_MAT1X4              \
  if (kOperation > 0) {                          \
    *c++ += c0;                                  \
    *c++ += c1;                                  \
    *c++ += c2;                                  \
    *c++ += c3;                                  \
  } else if (kOperation < 0) {                   \
    *c++ -= c0;                                  \
    *c++ -= c1;                                  \
    *c++ -= c2;                                  \
    *c++ -= c3;                                  \
  } else {                                       \
    *c++ = c0;                                   \
    *c++ = c1;                                   \
    *c++ = c2;                                   \
    *c++ = c3;                                   \
  }

// Matrix-Matrix Multiplication
// Figure out 1x4 of Matrix C in one batch
//
// c op a * B;
// where op can be +=, -=, or =, indicated by kOperation.
//
//  Matrix C              Matrix A                   Matrix B
//
//  C0, C1, C2, C3   op   A0, A1, A2, A3, ...    *   B0, B1, B2, B3
//                                                   B4, B5, B6, B7
//                                                   B8, B9, Ba, Bb
//                                                   Bc, Bd, Be, Bf
//                                                   . , . , . , .
//                                                   . , . , . , .
//                                                   . , . , . , .
//
// unroll for loops
// utilize the data resided in cache
// NOTE: col_a means the columns of A
static inline void MMM_mat1x4(const int col_a,
                              const double* a,
                              const double* b,
                              const int col_stride_b,
                              double* c,
                              const int kOperation) {
  CERES_GEMM_OPT_NAIVE_HEADER
  double av = 0.0;
  int bi = 0;

#define CERES_GEMM_OPT_MMM_MAT1X4_MUL  \
  av = pa[k];                          \
  pb = b + bi;                         \
  c0 += av * *pb++;                    \
  c1 += av * *pb++;                    \
  c2 += av * *pb++;                    \
  c3 += av * *pb++;                    \
  bi += col_stride_b;                  \
  k++;

  for (int k = 0; k < col_m;) {
    CERES_GEMM_OPT_MMM_MAT1X4_MUL
    CERES_GEMM_OPT_MMM_MAT1X4_MUL
    CERES_GEMM_OPT_MMM_MAT1X4_MUL
    CERES_GEMM_OPT_MMM_MAT1X4_MUL
  }

  for (int k = col_m; k < col_a;) {
    CERES_GEMM_OPT_MMM_MAT1X4_MUL
  }

  CERES_GEMM_OPT_STORE_MAT1X4

#undef CERES_GEMM_OPT_MMM_MAT1X4_MUL
}

// Matrix Transpose-Matrix multiplication
// Figure out 1x4 of Matrix C in one batch
//
// c op a' * B;
// where op can be +=, -=, or = indicated by kOperation.
//
//                        Matrix A
//
//                        A0
//                        A1
//                        A2
//                        A3
//                        .
//                        .
//                        .
//
//  Matrix C              Matrix A'                  Matrix B
//
//  C0, C1, C2, C3   op   A0, A1, A2, A3, ...    *   B0, B1, B2, B3
//                                                   B4, B5, B6, B7
//                                                   B8, B9, Ba, Bb
//                                                   Bc, Bd, Be, Bf
//                                                   . , . , . , .
//                                                   . , . , . , .
//                                                   . , . , . , .
//
// unroll for loops
// utilize the data resided in cache
// NOTE: col_a means the columns of A'
static inline void MTM_mat1x4(const int col_a,
                              const double* a,
                              const int col_stride_a,
                              const double* b,
                              const int col_stride_b,
                              double* c,
                              const int kOperation) {
  CERES_GEMM_OPT_NAIVE_HEADER
  double av = 0.0;
  int ai = 0;
  int bi = 0;

#define CERES_GEMM_OPT_MTM_MAT1X4_MUL  \
  av = pa[ai];                         \
  pb = b + bi;                         \
  c0 += av * *pb++;                    \
  c1 += av * *pb++;                    \
  c2 += av * *pb++;                    \
  c3 += av * *pb++;                    \
  ai += col_stride_a;                  \
  bi += col_stride_b;

  for (int k = 0; k < col_m; k += span) {
    CERES_GEMM_OPT_MTM_MAT1X4_MUL
    CERES_GEMM_OPT_MTM_MAT1X4_MUL
    CERES_GEMM_OPT_MTM_MAT1X4_MUL
    CERES_GEMM_OPT_MTM_MAT1X4_MUL
  }

  for (int k = col_m; k < col_a; k++) {
    CERES_GEMM_OPT_MTM_MAT1X4_MUL
  }

  CERES_GEMM_OPT_STORE_MAT1X4

#undef CERES_GEMM_OPT_MTM_MAT1X4_MUL
}

// Matrix-Vector Multiplication
// Figure out 4x1 of vector c in one batch
//
// c op A * b;
// where op can be +=, -=, or =, indicated by kOperation.
//
//  Vector c              Matrix A                   Vector b
//
//  C0               op   A0, A1, A2, A3, ...    *   B0
//  C1                    A4, A5, A6, A7, ...        B1
//  C2                    A8, A9, Aa, Ab, ...        B2
//  C3                    Ac, Ad, Ae, Af, ...        B3
//                                                   .
//                                                   .
//                                                   .
//
// unroll for loops
// utilize the data resided in cache
// NOTE: col_a means the columns of A
static inline void MVM_mat4x1(const int col_a,
                              const double* a,
                              const int col_stride_a,
                              const double* b,
                              double* c,
                              const int kOperation) {
  CERES_GEMM_OPT_NAIVE_HEADER
  double bv = 0.0;

#define CERES_GEMM_OPT_MVM_MAT4X1_MUL              \
  bv = *pb;                                        \
  c0 += *(pa                   ) * bv;             \
  c1 += *(pa + col_stride_a    ) * bv;             \
  c2 += *(pa + col_stride_a * 2) * bv;             \
  c3 += *(pa + col_stride_a * 3) * bv;             \
  pa++;                                            \
  pb++;

  for (int k = 0; k < col_m; k += span) {
    CERES_GEMM_OPT_MVM_MAT4X1_MUL
    CERES_GEMM_OPT_MVM_MAT4X1_MUL
    CERES_GEMM_OPT_MVM_MAT4X1_MUL
    CERES_GEMM_OPT_MVM_MAT4X1_MUL
  }

  for (int k = col_m; k < col_a; k++) {
    CERES_GEMM_OPT_MVM_MAT4X1_MUL
  }

  CERES_GEMM_OPT_STORE_MAT1X4

#undef CERES_GEMM_OPT_MVM_MAT4X1_MUL
}

// Matrix Transpose-Vector multiplication
// Figure out 4x1 of vector c in one batch
//
// c op A' * b;
// where op can be +=, -=, or =, indicated by kOperation.
//
//                        Matrix A
//
//                        A0, A4, A8, Ac
//                        A1, A5, A9, Ad
//                        A2, A6, Aa, Ae
//                        A3, A7, Ab, Af
//                        . , . , . , .
//                        . , . , . , .
//                        . , . , . , .
//
//  Vector c              Matrix A'                  Vector b
//
//  C0               op   A0, A1, A2, A3, ...    *   B0
//  C1                    A4, A5, A6, A7, ...        B1
//  C2                    A8, A9, Aa, Ab, ...        B2
//  C3                    Ac, Ad, Ae, Af, ...        B3
//                                                   .
//                                                   .
//                                                   .
//
// unroll for loops
// utilize the data resided in cache
// NOTE: col_a means the columns of A'
static inline void MTV_mat4x1(const int col_a,
                              const double* a,
                              const int col_stride_a,
                              const double* b,
                              double* c,
                              const int kOperation) {
  CERES_GEMM_OPT_NAIVE_HEADER
  double bv = 0.0;

#define CERES_GEMM_OPT_MTV_MAT4X1_MUL  \
  bv = *pb;                            \
  c0 += *(pa    ) * bv;                \
  c1 += *(pa + 1) * bv;                \
  c2 += *(pa + 2) * bv;                \
  c3 += *(pa + 3) * bv;                \
  pa += col_stride_a;                  \
  pb++;

  for (int k = 0; k < col_m; k += span) {
    CERES_GEMM_OPT_MTV_MAT4X1_MUL
    CERES_GEMM_OPT_MTV_MAT4X1_MUL
    CERES_GEMM_OPT_MTV_MAT4X1_MUL
    CERES_GEMM_OPT_MTV_MAT4X1_MUL
  }

  for (int k = col_m; k < col_a; k++) {
    CERES_GEMM_OPT_MTV_MAT4X1_MUL
  }

  CERES_GEMM_OPT_STORE_MAT1X4

#undef CERES_GEMM_OPT_MTV_MAT4X1_MUL
}

#undef CERES_GEMM_OPT_NAIVE_HEADER
#undef CERES_GEMM_OPT_STORE_MAT1X4

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SMALL_BLAS_GENERIC_H_

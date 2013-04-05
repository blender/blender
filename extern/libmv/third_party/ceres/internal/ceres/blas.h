// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//
// Simple blas functions for use in the Schur Eliminator. These are
// fairly basic implementations which already yield a significant
// speedup in the eliminator performance.

#ifndef CERES_INTERNAL_BLAS_H_
#define CERES_INTERNAL_BLAS_H_

#include "ceres/internal/eigen.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Remove the ".noalias()" annotation from the matrix matrix
// mutliplies to produce a correct build with the Android NDK,
// including versions 6, 7, 8, and 8b, when built with STLPort and the
// non-standalone toolchain (i.e. ndk-build). This appears to be a
// compiler bug; if the workaround is not in place, the line
//
//   block.noalias() -= A * B;
//
// gets compiled to
//
//   block.noalias() += A * B;
//
// which breaks schur elimination. Introducing a temporary by removing the
// .noalias() annotation causes the issue to disappear. Tracking this
// issue down was tricky, since the test suite doesn't run when built with
// the non-standalone toolchain.
//
// TODO(keir): Make a reproduction case for this and send it upstream.
#ifdef CERES_WORK_AROUND_ANDROID_NDK_COMPILER_BUG
#define CERES_MAYBE_NOALIAS
#else
#define CERES_MAYBE_NOALIAS .noalias()
#endif

// For the matrix-matrix functions below, there are three functions
// for each functionality. Foo, FooNaive and FooEigen. Foo is the one
// to be called by the user. FooNaive is a basic loop based
// implementation and FooEigen uses Eigen's implementation. Foo
// chooses between FooNaive and FooEigen depending on how many of the
// template arguments are fixed at compile time. Currently, FooEigen
// is called if all matrix dimenions are compile time
// constants. FooNaive is called otherwise. This leads to the best
// performance currently.
//
// TODO(sameeragarwal): Benchmark and simplify the matrix-vector
// functions.

// C op A * B;
//
// where op can be +=, -=, or =.
//
// The template parameters (kRowA, kColA, kRowB, kColB) allow
// specialization of the loop at compile time. If this information is
// not available, then Eigen::Dynamic should be used as the template
// argument.
//
//   kOperation =  1  -> C += A * B
//   kOperation = -1  -> C -= A * B
//   kOperation =  0  -> C  = A * B
//
// The function can write into matrices C which are larger than the
// matrix A * B. This is done by specifying the true size of C via
// row_stride_c and col_stride_c, and then indicating where A * B
// should be written into by start_row_c and start_col_c.
//
// Graphically if row_stride_c = 10, col_stride_c = 12, start_row_c =
// 4 and start_col_c = 5, then if A = 3x2 and B = 2x4, we get
//
//   ------------
//   ------------
//   ------------
//   ------------
//   -----xxxx---
//   -----xxxx---
//   -----xxxx---
//   ------------
//   ------------
//   ------------
//
template<int kRowA, int kColA, int kRowB, int kColB, int kOperation>
inline void MatrixMatrixMultiplyEigen(const double* A,
                                      const int num_row_a,
                                      const int num_col_a,
                                      const double* B,
                                      const int num_row_b,
                                      const int num_col_b,
                                      double* C,
                                      const int start_row_c,
                                      const int start_col_c,
                                      const int row_stride_c,
                                      const int col_stride_c) {
  const typename EigenTypes<kRowA, kColA>::ConstMatrixRef Aref(A, num_row_a, num_col_a);
  const typename EigenTypes<kRowB, kColB>::ConstMatrixRef Bref(B, num_row_b, num_col_b);
  MatrixRef Cref(C, row_stride_c, col_stride_c);
  Eigen::Block<MatrixRef, kRowA, kColB> block(Cref,
                                              start_row_c, start_col_c,
                                              num_row_a, num_col_b);
  if (kOperation > 0) {
    block CERES_MAYBE_NOALIAS += Aref * Bref;
  } else if (kOperation < 0) {
    block CERES_MAYBE_NOALIAS -= Aref * Bref;
  } else {
    block CERES_MAYBE_NOALIAS = Aref * Bref;
  }
}

template<int kRowA, int kColA, int kRowB, int kColB, int kOperation>
inline void MatrixMatrixMultiplyNaive(const double* A,
                                      const int num_row_a,
                                      const int num_col_a,
                                      const double* B,
                                      const int num_row_b,
                                      const int num_col_b,
                                      double* C,
                                      const int start_row_c,
                                      const int start_col_c,
                                      const int row_stride_c,
                                      const int col_stride_c) {
  DCHECK_GT(num_row_a, 0);
  DCHECK_GT(num_col_a, 0);
  DCHECK_GT(num_row_b, 0);
  DCHECK_GT(num_col_b, 0);
  DCHECK_GE(start_row_c, 0);
  DCHECK_GE(start_col_c, 0);
  DCHECK_GT(row_stride_c, 0);
  DCHECK_GT(col_stride_c, 0);

  DCHECK((kRowA == Eigen::Dynamic) || (kRowA == num_row_a));
  DCHECK((kColA == Eigen::Dynamic) || (kColA == num_col_a));
  DCHECK((kRowB == Eigen::Dynamic) || (kRowB == num_row_b));
  DCHECK((kColB == Eigen::Dynamic) || (kColB == num_col_b));

  const int NUM_ROW_A = (kRowA != Eigen::Dynamic ? kRowA : num_row_a);
  const int NUM_COL_A = (kColA != Eigen::Dynamic ? kColA : num_col_a);
  const int NUM_ROW_B = (kColB != Eigen::Dynamic ? kRowB : num_row_b);
  const int NUM_COL_B = (kColB != Eigen::Dynamic ? kColB : num_col_b);
  DCHECK_EQ(NUM_COL_A, NUM_ROW_B);

  const int NUM_ROW_C = NUM_ROW_A;
  const int NUM_COL_C = NUM_COL_B;
  DCHECK_LT(start_row_c + NUM_ROW_C, row_stride_c);
  DCHECK_LT(start_col_c + NUM_COL_C, col_stride_c);

  for (int row = 0; row < NUM_ROW_C; ++row) {
    for (int col = 0; col < NUM_COL_C; ++col) {
      double tmp = 0.0;
      for (int k = 0; k < NUM_COL_A; ++k) {
        tmp += A[row * NUM_COL_A + k] * B[k * NUM_COL_B + col];
      }

      const int index = (row + start_row_c) * col_stride_c + start_col_c + col;
      if (kOperation > 0) {
        C[index] += tmp;
      } else if (kOperation < 0) {
        C[index] -= tmp;
      } else {
        C[index] = tmp;
      }
    }
  }
}

template<int kRowA, int kColA, int kRowB, int kColB, int kOperation>
inline void MatrixMatrixMultiply(const double* A,
                                 const int num_row_a,
                                 const int num_col_a,
                                 const double* B,
                                 const int num_row_b,
                                 const int num_col_b,
                                 double* C,
                                 const int start_row_c,
                                 const int start_col_c,
                                 const int row_stride_c,
                                 const int col_stride_c) {
#ifdef CERES_NO_CUSTOM_BLAS
  MatrixMatrixMultiplyEigen<kRowA, kColA, kRowB, kColB, kOperation>(
      A, num_row_a, num_col_a,
      B, num_row_b, num_col_b,
      C, start_row_c, start_col_c, row_stride_c, col_stride_c);
  return;

#else

  if (kRowA != Eigen::Dynamic && kColA != Eigen::Dynamic &&
      kRowB != Eigen::Dynamic && kColB != Eigen::Dynamic) {
    MatrixMatrixMultiplyEigen<kRowA, kColA, kRowB, kColB, kOperation>(
        A, num_row_a, num_col_a,
        B, num_row_b, num_col_b,
        C, start_row_c, start_col_c, row_stride_c, col_stride_c);
  } else {
    MatrixMatrixMultiplyNaive<kRowA, kColA, kRowB, kColB, kOperation>(
        A, num_row_a, num_col_a,
        B, num_row_b, num_col_b,
        C, start_row_c, start_col_c, row_stride_c, col_stride_c);
  }

#endif
}


// C op A' * B;
//
// where op can be +=, -=, or =.
//
// The template parameters (kRowA, kColA, kRowB, kColB) allow
// specialization of the loop at compile time. If this information is
// not available, then Eigen::Dynamic should be used as the template
// argument.
//
//   kOperation =  1  -> C += A' * B
//   kOperation = -1  -> C -= A' * B
//   kOperation =  0  -> C  = A' * B
//
// The function can write into matrices C which are larger than the
// matrix A' * B. This is done by specifying the true size of C via
// row_stride_c and col_stride_c, and then indicating where A * B
// should be written into by start_row_c and start_col_c.
//
// Graphically if row_stride_c = 10, col_stride_c = 12, start_row_c =
// 4 and start_col_c = 5, then if A = 2x3 and B = 2x4, we get
//
//   ------------
//   ------------
//   ------------
//   ------------
//   -----xxxx---
//   -----xxxx---
//   -----xxxx---
//   ------------
//   ------------
//   ------------
//
template<int kRowA, int kColA, int kRowB, int kColB, int kOperation>
inline void MatrixTransposeMatrixMultiplyEigen(const double* A,
                                               const int num_row_a,
                                               const int num_col_a,
                                               const double* B,
                                               const int num_row_b,
                                               const int num_col_b,
                                               double* C,
                                               const int start_row_c,
                                               const int start_col_c,
                                               const int row_stride_c,
                                               const int col_stride_c) {
  const typename EigenTypes<kRowA, kColA>::ConstMatrixRef Aref(A, num_row_a, num_col_a);
  const typename EigenTypes<kRowB, kColB>::ConstMatrixRef Bref(B, num_row_b, num_col_b);
  MatrixRef Cref(C, row_stride_c, col_stride_c);
  Eigen::Block<MatrixRef, kColA, kColB> block(Cref,
                                              start_row_c, start_col_c,
                                              num_col_a, num_col_b);
  if (kOperation > 0) {
    block CERES_MAYBE_NOALIAS += Aref.transpose() * Bref;
  } else if (kOperation < 0) {
    block CERES_MAYBE_NOALIAS -= Aref.transpose() * Bref;
  } else {
    block CERES_MAYBE_NOALIAS = Aref.transpose() * Bref;
  }
}

template<int kRowA, int kColA, int kRowB, int kColB, int kOperation>
inline void MatrixTransposeMatrixMultiplyNaive(const double* A,
                                               const int num_row_a,
                                               const int num_col_a,
                                               const double* B,
                                               const int num_row_b,
                                               const int num_col_b,
                                               double* C,
                                               const int start_row_c,
                                               const int start_col_c,
                                               const int row_stride_c,
                                               const int col_stride_c) {
  DCHECK_GT(num_row_a, 0);
  DCHECK_GT(num_col_a, 0);
  DCHECK_GT(num_row_b, 0);
  DCHECK_GT(num_col_b, 0);
  DCHECK_GE(start_row_c, 0);
  DCHECK_GE(start_col_c, 0);
  DCHECK_GT(row_stride_c, 0);
  DCHECK_GT(col_stride_c, 0);

  DCHECK((kRowA == Eigen::Dynamic) || (kRowA == num_row_a));
  DCHECK((kColA == Eigen::Dynamic) || (kColA == num_col_a));
  DCHECK((kRowB == Eigen::Dynamic) || (kRowB == num_row_b));
  DCHECK((kColB == Eigen::Dynamic) || (kColB == num_col_b));

  const int NUM_ROW_A = (kRowA != Eigen::Dynamic ? kRowA : num_row_a);
  const int NUM_COL_A = (kColA != Eigen::Dynamic ? kColA : num_col_a);
  const int NUM_ROW_B = (kColB != Eigen::Dynamic ? kRowB : num_row_b);
  const int NUM_COL_B = (kColB != Eigen::Dynamic ? kColB : num_col_b);
  DCHECK_EQ(NUM_ROW_A, NUM_ROW_B);

  const int NUM_ROW_C = NUM_COL_A;
  const int NUM_COL_C = NUM_COL_B;
  DCHECK_LT(start_row_c + NUM_ROW_C, row_stride_c);
  DCHECK_LT(start_col_c + NUM_COL_C, col_stride_c);

  for (int row = 0; row < NUM_ROW_C; ++row) {
    for (int col = 0; col < NUM_COL_C; ++col) {
      double tmp = 0.0;
      for (int k = 0; k < NUM_ROW_A; ++k) {
        tmp += A[k * NUM_COL_A + row] * B[k * NUM_COL_B + col];
      }

      const int index = (row + start_row_c) * col_stride_c + start_col_c + col;
      if (kOperation > 0) {
        C[index]+= tmp;
      } else if (kOperation < 0) {
        C[index]-= tmp;
      } else {
        C[index]= tmp;
      }
    }
  }
}

template<int kRowA, int kColA, int kRowB, int kColB, int kOperation>
inline void MatrixTransposeMatrixMultiply(const double* A,
                                          const int num_row_a,
                                          const int num_col_a,
                                          const double* B,
                                          const int num_row_b,
                                          const int num_col_b,
                                          double* C,
                                          const int start_row_c,
                                          const int start_col_c,
                                          const int row_stride_c,
                                          const int col_stride_c) {
#ifdef CERES_NO_CUSTOM_BLAS
  MatrixTransposeMatrixMultiplyEigen<kRowA, kColA, kRowB, kColB, kOperation>(
      A, num_row_a, num_col_a,
      B, num_row_b, num_col_b,
      C, start_row_c, start_col_c, row_stride_c, col_stride_c);
  return;

#else

  if (kRowA != Eigen::Dynamic && kColA != Eigen::Dynamic &&
      kRowB != Eigen::Dynamic && kColB != Eigen::Dynamic) {
    MatrixTransposeMatrixMultiplyEigen<kRowA, kColA, kRowB, kColB, kOperation>(
        A, num_row_a, num_col_a,
        B, num_row_b, num_col_b,
        C, start_row_c, start_col_c, row_stride_c, col_stride_c);
  } else {
    MatrixTransposeMatrixMultiplyNaive<kRowA, kColA, kRowB, kColB, kOperation>(
        A, num_row_a, num_col_a,
        B, num_row_b, num_col_b,
        C, start_row_c, start_col_c, row_stride_c, col_stride_c);
  }

#endif
}

template<int kRowA, int kColA, int kOperation>
inline void MatrixVectorMultiply(const double* A,
                                 const int num_row_a,
                                 const int num_col_a,
                                 const double* b,
                                 double* c) {
#ifdef CERES_NO_CUSTOM_BLAS
  const typename EigenTypes<kRowA, kColA>::ConstMatrixRef Aref(A, num_row_a, num_col_a);
  const typename EigenTypes<kColA>::ConstVectorRef bref(b, num_col_a);
  typename EigenTypes<kRowA>::VectorRef cref(c, num_row_a);

  // lazyProduct works better than .noalias() for matrix-vector
  // products.
  if (kOperation > 0) {
    cref += Aref.lazyProduct(bref);
  } else if (kOperation < 0) {
    cref -= Aref.lazyProduct(bref);
  } else {
    cref -= Aref.lazyProduct(bref);
  }
#else

  DCHECK_GT(num_row_a, 0);
  DCHECK_GT(num_col_a, 0);
  DCHECK((kRowA == Eigen::Dynamic) || (kRowA == num_row_a));
  DCHECK((kColA == Eigen::Dynamic) || (kColA == num_col_a));

  const int NUM_ROW_A = (kRowA != Eigen::Dynamic ? kRowA : num_row_a);
  const int NUM_COL_A = (kColA != Eigen::Dynamic ? kColA : num_col_a);

  for (int row = 0; row < NUM_ROW_A; ++row) {
    double tmp = 0.0;
    for (int col = 0; col < NUM_COL_A; ++col) {
      tmp += A[row * NUM_COL_A + col] * b[col];
    }

    if (kOperation > 0) {
      c[row] += tmp;
    } else if (kOperation < 0) {
      c[row] -= tmp;
    } else {
      c[row] = tmp;
    }
  }
#endif  // CERES_NO_CUSTOM_BLAS
}

// c op A' * b;
//
// where op can be +=, -=, or =.
//
// The template parameters (kRowA, kColA) allow specialization of the
// loop at compile time. If this information is not available, then
// Eigen::Dynamic should be used as the template argument.
//
// kOperation =  1  -> c += A' * b
// kOperation = -1  -> c -= A' * b
// kOperation =  0  -> c  = A' * b
template<int kRowA, int kColA, int kOperation>
inline void MatrixTransposeVectorMultiply(const double* A,
                                          const int num_row_a,
                                          const int num_col_a,
                                          const double* b,
                                          double* c) {
#ifdef CERES_NO_CUSTOM_BLAS
  const typename EigenTypes<kRowA, kColA>::ConstMatrixRef Aref(A, num_row_a, num_col_a);
  const typename EigenTypes<kRowA>::ConstVectorRef bref(b, num_row_a);
  typename EigenTypes<kColA>::VectorRef cref(c, num_col_a);

  // lazyProduct works better than .noalias() for matrix-vector
  // products.
  if (kOperation > 0) {
    cref += Aref.transpose().lazyProduct(bref);
  } else if (kOperation < 0) {
    cref -= Aref.transpose().lazyProduct(bref);
  } else {
    cref -= Aref.transpose().lazyProduct(bref);
  }
#else

  DCHECK_GT(num_row_a, 0);
  DCHECK_GT(num_col_a, 0);
  DCHECK((kRowA == Eigen::Dynamic) || (kRowA == num_row_a));
  DCHECK((kColA == Eigen::Dynamic) || (kColA == num_col_a));

  const int NUM_ROW_A = (kRowA != Eigen::Dynamic ? kRowA : num_row_a);
  const int NUM_COL_A = (kColA != Eigen::Dynamic ? kColA : num_col_a);

  for (int row = 0; row < NUM_COL_A; ++row) {
    double tmp = 0.0;
    for (int col = 0; col < NUM_ROW_A; ++col) {
      tmp += A[col * NUM_COL_A + row] * b[col];
    }

    if (kOperation > 0) {
      c[row] += tmp;
    } else if (kOperation < 0) {
      c[row] -= tmp;
    } else {
      c[row] = tmp;
    }
  }
#endif  // CERES_NO_CUSTOM_BLAS
}

#undef CERES_MAYBE_NOALIAS

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLAS_H_

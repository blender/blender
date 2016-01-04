// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_EIGEN_H_
#define CERES_INTERNAL_EIGEN_H_

#include "Eigen/Core"

namespace ceres {

typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vector;
typedef Eigen::Matrix<double,
                      Eigen::Dynamic,
                      Eigen::Dynamic,
                      Eigen::RowMajor> Matrix;
typedef Eigen::Map<Vector> VectorRef;
typedef Eigen::Map<Matrix> MatrixRef;
typedef Eigen::Map<const Vector> ConstVectorRef;
typedef Eigen::Map<const Matrix> ConstMatrixRef;

// Column major matrices for DenseSparseMatrix/DenseQRSolver
typedef Eigen::Matrix<double,
                      Eigen::Dynamic,
                      Eigen::Dynamic,
                      Eigen::ColMajor> ColMajorMatrix;

typedef Eigen::Map<ColMajorMatrix, 0,
                   Eigen::Stride<Eigen::Dynamic, 1> > ColMajorMatrixRef;

typedef Eigen::Map<const ColMajorMatrix,
                   0,
                   Eigen::Stride<Eigen::Dynamic, 1> > ConstColMajorMatrixRef;



// C++ does not support templated typdefs, thus the need for this
// struct so that we can support statically sized Matrix and Maps.
template <int num_rows = Eigen::Dynamic, int num_cols = Eigen::Dynamic>
struct EigenTypes {
  typedef Eigen::Matrix <double, num_rows, num_cols, Eigen::RowMajor>
  Matrix;

  typedef Eigen::Map<
    Eigen::Matrix<double, num_rows, num_cols, Eigen::RowMajor> >
  MatrixRef;

  typedef Eigen::Matrix <double, num_rows, 1>
  Vector;

  typedef Eigen::Map <
    Eigen::Matrix<double, num_rows, 1> >
  VectorRef;


  typedef Eigen::Map<
    const Eigen::Matrix<double, num_rows, num_cols, Eigen::RowMajor> >
  ConstMatrixRef;

  typedef Eigen::Map <
    const Eigen::Matrix<double, num_rows, 1> >
  ConstVectorRef;
};

}  // namespace ceres

#endif  // CERES_INTERNAL_EIGEN_H_

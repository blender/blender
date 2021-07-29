// Copyright (c) 2009 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_MULTIVIEW_TWO_VIEW_KERNEL_H_
#define LIBMV_MULTIVIEW_TWO_VIEW_KERNEL_H_

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/conditioning.h"
#include "libmv/numeric/numeric.h"

namespace libmv {
namespace two_view {
namespace kernel {

template<typename Solver, typename Unnormalizer>
struct NormalizedSolver {
  enum { MINIMUM_SAMPLES = Solver::MINIMUM_SAMPLES };
  static void Solve(const Mat &x1, const Mat &x2, vector<Mat3> *models) {
    assert(2 == x1.rows());
    assert(MINIMUM_SAMPLES <= x1.cols());
    assert(x1.rows() == x2.rows());
    assert(x1.cols() == x2.cols());

    // Normalize the data.
    Mat3 T1, T2;
    Mat x1_normalized, x2_normalized;
    NormalizePoints(x1, &x1_normalized, &T1);
    NormalizePoints(x2, &x2_normalized, &T2);

    Solver::Solve(x1_normalized, x2_normalized, models);

    for (int i = 0; i < models->size(); ++i) {
      Unnormalizer::Unnormalize(T1, T2, &(*models)[i]);
    }
  }
};

template<typename Solver, typename Unnormalizer>
struct IsotropicNormalizedSolver {
  enum { MINIMUM_SAMPLES = Solver::MINIMUM_SAMPLES };
  static void Solve(const Mat &x1, const Mat &x2, vector<Mat3> *models) {
    assert(2 == x1.rows());
    assert(MINIMUM_SAMPLES <= x1.cols());
    assert(x1.rows() == x2.rows());
    assert(x1.cols() == x2.cols());

    // Normalize the data.
    Mat3 T1, T2;
    Mat x1_normalized, x2_normalized;
    NormalizeIsotropicPoints(x1, &x1_normalized, &T1);
    NormalizeIsotropicPoints(x2, &x2_normalized, &T2);

    Solver::Solve(x1_normalized, x2_normalized, models);

    for (int i = 0; i < models->size(); ++i) {
      Unnormalizer::Unnormalize(T1, T2, &(*models)[i]);
    }
  }
};
// This is one example (targeted at solvers that operate on correspondences
// between two views) that shows the "kernel" part of a robust fitting
// problem:
//
//   1. The model; Mat3 in the case of the F or H matrix.
//   2. The minimum number of samples needed to fit; 7 or 8 (or 4).
//   3. A way to convert samples to a model.
//   4. A way to convert a sample and a model to an error.
//
// Of particular note is that the kernel does not expose what the samples are.
// All the robust fitting algorithm sees is that there is some number of
// samples; it is able to fit subsets of them (via the kernel) and check their
// error, but can never access the samples themselves.
//
// The Kernel objects must follow the following concept so that the robust
// fitting alogrithm can fit this type of relation:
//
//   1. Kernel::Model
//   2. Kernel::MINIMUM_SAMPLES
//   3. Kernel::Fit(vector<int>, vector<Kernel::Model> *)
//   4. Kernel::Error(int, Model) -> error
//
// The fit routine must not clear existing entries in the vector of models; it
// should append new solutions to the end.
template<typename SolverArg,
         typename ErrorArg,
         typename ModelArg = Mat3>
class Kernel {
 public:
  Kernel(const Mat &x1, const Mat &x2) : x1_(x1), x2_(x2) {}
  typedef SolverArg Solver;
  typedef ModelArg  Model;
  enum { MINIMUM_SAMPLES = Solver::MINIMUM_SAMPLES };
  void Fit(const vector<int> &samples, vector<Model> *models) const {
    Mat x1 = ExtractColumns(x1_, samples);
    Mat x2 = ExtractColumns(x2_, samples);
    Solver::Solve(x1, x2, models);
  }
  double Error(int sample, const Model &model) const {
    return ErrorArg::Error(model,
                           static_cast<Vec>(x1_.col(sample)),
                           static_cast<Vec>(x2_.col(sample)));
  }
  int NumSamples() const {
    return x1_.cols();
  }
  static void Solve(const Mat &x1, const Mat &x2, vector<Model> *models) {
    // By offering this, Kernel types can be passed to templates.
    Solver::Solve(x1, x2, models);
  }
 protected:
  const Mat &x1_;
  const Mat &x2_;
};

}  // namespace kernel
}  // namespace two_view
}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_TWO_VIEW_KERNEL_H_

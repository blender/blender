// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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

#ifndef CERES_PUBLIC_CUBIC_INTERPOLATION_H_
#define CERES_PUBLIC_CUBIC_INTERPOLATION_H_

#include "Eigen/Core"
#include "ceres/internal/export.h"
#include "glog/logging.h"

namespace ceres {

// Given samples from a function sampled at four equally spaced points,
//
//   p0 = f(-1)
//   p1 = f(0)
//   p2 = f(1)
//   p3 = f(2)
//
// Evaluate the cubic Hermite spline (also known as the Catmull-Rom
// spline) at a point x that lies in the interval [0, 1].
//
// This is also the interpolation kernel (for the case of a = 0.5) as
// proposed by R. Keys, in:
//
// "Cubic convolution interpolation for digital image processing".
// IEEE Transactions on Acoustics, Speech, and Signal Processing
// 29 (6): 1153-1160.
//
// For more details see
//
// http://en.wikipedia.org/wiki/Cubic_Hermite_spline
// http://en.wikipedia.org/wiki/Bicubic_interpolation
//
// f if not nullptr will contain the interpolated function values.
// dfdx if not nullptr will contain the interpolated derivative values.
template <int kDataDimension>
void CubicHermiteSpline(const Eigen::Matrix<double, kDataDimension, 1>& p0,
                        const Eigen::Matrix<double, kDataDimension, 1>& p1,
                        const Eigen::Matrix<double, kDataDimension, 1>& p2,
                        const Eigen::Matrix<double, kDataDimension, 1>& p3,
                        const double x,
                        double* f,
                        double* dfdx) {
  using VType = Eigen::Matrix<double, kDataDimension, 1>;
  const VType a = 0.5 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3);
  const VType b = 0.5 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3);
  const VType c = 0.5 * (-p0 + p2);
  const VType d = p1;

  // Use Horner's rule to evaluate the function value and its
  // derivative.

  // f = ax^3 + bx^2 + cx + d
  if (f != nullptr) {
    Eigen::Map<VType>(f, kDataDimension) = d + x * (c + x * (b + x * a));
  }

  // dfdx = 3ax^2 + 2bx + c
  if (dfdx != nullptr) {
    Eigen::Map<VType>(dfdx, kDataDimension) = c + x * (2.0 * b + 3.0 * a * x);
  }
}

// Given as input an infinite one dimensional grid, which provides the
// following interface.
//
//   class Grid {
//    public:
//     enum { DATA_DIMENSION = 2; };
//     void GetValue(int n, double* f) const;
//   };
//
// Here, GetValue gives the value of a function f (possibly vector
// valued) for any integer n.
//
// The enum DATA_DIMENSION indicates the dimensionality of the
// function being interpolated. For example if you are interpolating
// rotations in axis-angle format over time, then DATA_DIMENSION = 3.
//
// CubicInterpolator uses cubic Hermite splines to produce a smooth
// approximation to it that can be used to evaluate the f(x) and f'(x)
// at any point on the real number line.
//
// For more details on cubic interpolation see
//
// http://en.wikipedia.org/wiki/Cubic_Hermite_spline
//
// Example usage:
//
//  const double data[] = {1.0, 2.0, 5.0, 6.0};
//  Grid1D<double, 1> grid(data, 0, 4);
//  CubicInterpolator<Grid1D<double, 1>> interpolator(grid);
//  double f, dfdx;
//  interpolator.Evaluator(1.5, &f, &dfdx);
template <typename Grid>
class CubicInterpolator {
 public:
  explicit CubicInterpolator(const Grid& grid) : grid_(grid) {
    // The + casts the enum into an int before doing the
    // comparison. It is needed to prevent
    // "-Wunnamed-type-template-args" related errors.
    CHECK_GE(+Grid::DATA_DIMENSION, 1);
  }

  void Evaluate(double x, double* f, double* dfdx) const {
    const int n = std::floor(x);
    Eigen::Matrix<double, Grid::DATA_DIMENSION, 1> p0, p1, p2, p3;
    grid_.GetValue(n - 1, p0.data());
    grid_.GetValue(n, p1.data());
    grid_.GetValue(n + 1, p2.data());
    grid_.GetValue(n + 2, p3.data());
    CubicHermiteSpline<Grid::DATA_DIMENSION>(p0, p1, p2, p3, x - n, f, dfdx);
  }

  // The following two Evaluate overloads are needed for interfacing
  // with automatic differentiation. The first is for when a scalar
  // evaluation is done, and the second one is for when Jets are used.
  void Evaluate(const double& x, double* f) const { Evaluate(x, f, nullptr); }

  template <typename JetT>
  void Evaluate(const JetT& x, JetT* f) const {
    double fx[Grid::DATA_DIMENSION], dfdx[Grid::DATA_DIMENSION];
    Evaluate(x.a, fx, dfdx);
    for (int i = 0; i < Grid::DATA_DIMENSION; ++i) {
      f[i].a = fx[i];
      f[i].v = dfdx[i] * x.v;
    }
  }

 private:
  const Grid& grid_;
};

// An object that implements an infinite one dimensional grid needed
// by the CubicInterpolator where the source of the function values is
// an array of type T on the interval
//
//   [begin, ..., end - 1]
//
// Since the input array is finite and the grid is infinite, values
// outside this interval needs to be computed. Grid1D uses the value
// from the nearest edge.
//
// The function being provided can be vector valued, in which case
// kDataDimension > 1. The dimensional slices of the function maybe
// interleaved, or they maybe stacked, i.e, if the function has
// kDataDimension = 2, if kInterleaved = true, then it is stored as
//
//   f01, f02, f11, f12 ....
//
// and if kInterleaved = false, then it is stored as
//
//  f01, f11, .. fn1, f02, f12, .. , fn2
//
template <typename T, int kDataDimension = 1, bool kInterleaved = true>
struct Grid1D {
 public:
  enum { DATA_DIMENSION = kDataDimension };

  Grid1D(const T* data, const int begin, const int end)
      : data_(data), begin_(begin), end_(end), num_values_(end - begin) {
    CHECK_LT(begin, end);
  }

  EIGEN_STRONG_INLINE void GetValue(const int n, double* f) const {
    const int idx = (std::min)((std::max)(begin_, n), end_ - 1) - begin_;
    if (kInterleaved) {
      for (int i = 0; i < kDataDimension; ++i) {
        f[i] = static_cast<double>(data_[kDataDimension * idx + i]);
      }
    } else {
      for (int i = 0; i < kDataDimension; ++i) {
        f[i] = static_cast<double>(data_[i * num_values_ + idx]);
      }
    }
  }

 private:
  const T* data_;
  const int begin_;
  const int end_;
  const int num_values_;
};

// Given as input an infinite two dimensional grid like object, which
// provides the following interface:
//
//   struct Grid {
//     enum { DATA_DIMENSION = 1 };
//     void GetValue(int row, int col, double* f) const;
//   };
//
// Where, GetValue gives us the value of a function f (possibly vector
// valued) for any pairs of integers (row, col), and the enum
// DATA_DIMENSION indicates the dimensionality of the function being
// interpolated. For example if you are interpolating a color image
// with three channels (Red, Green & Blue), then DATA_DIMENSION = 3.
//
// BiCubicInterpolator uses the cubic convolution interpolation
// algorithm of R. Keys, to produce a smooth approximation to it that
// can be used to evaluate the f(r,c), df(r, c)/dr and df(r,c)/dc at
// any point in the real plane.
//
// For more details on the algorithm used here see:
//
// "Cubic convolution interpolation for digital image processing".
// Robert G. Keys, IEEE Trans. on Acoustics, Speech, and Signal
// Processing 29 (6): 1153-1160, 1981.
//
// http://en.wikipedia.org/wiki/Cubic_Hermite_spline
// http://en.wikipedia.org/wiki/Bicubic_interpolation
//
// Example usage:
//
// const double data[] = {1.0, 3.0, -1.0, 4.0,
//                         3.6, 2.1,  4.2, 2.0,
//                        2.0, 1.0,  3.1, 5.2};
//  Grid2D<double, 1>  grid(data, 3, 4);
//  BiCubicInterpolator<Grid2D<double, 1>> interpolator(grid);
//  double f, dfdr, dfdc;
//  interpolator.Evaluate(1.2, 2.5, &f, &dfdr, &dfdc);

template <typename Grid>
class BiCubicInterpolator {
 public:
  explicit BiCubicInterpolator(const Grid& grid) : grid_(grid) {
    // The + casts the enum into an int before doing the
    // comparison. It is needed to prevent
    // "-Wunnamed-type-template-args" related errors.
    CHECK_GE(+Grid::DATA_DIMENSION, 1);
  }

  // Evaluate the interpolated function value and/or its
  // derivative. Uses the nearest point on the grid boundary if r or
  // c is out of bounds.
  void Evaluate(
      double r, double c, double* f, double* dfdr, double* dfdc) const {
    // BiCubic interpolation requires 16 values around the point being
    // evaluated.  We will use pij, to indicate the elements of the
    // 4x4 grid of values.
    //
    //          col
    //      p00 p01 p02 p03
    // row  p10 p11 p12 p13
    //      p20 p21 p22 p23
    //      p30 p31 p32 p33
    //
    // The point (r,c) being evaluated is assumed to lie in the square
    // defined by p11, p12, p22 and p21.

    const int row = std::floor(r);
    const int col = std::floor(c);

    Eigen::Matrix<double, Grid::DATA_DIMENSION, 1> p0, p1, p2, p3;

    // Interpolate along each of the four rows, evaluating the function
    // value and the horizontal derivative in each row.
    Eigen::Matrix<double, Grid::DATA_DIMENSION, 1> f0, f1, f2, f3;
    Eigen::Matrix<double, Grid::DATA_DIMENSION, 1> df0dc, df1dc, df2dc, df3dc;

    grid_.GetValue(row - 1, col - 1, p0.data());
    grid_.GetValue(row - 1, col, p1.data());
    grid_.GetValue(row - 1, col + 1, p2.data());
    grid_.GetValue(row - 1, col + 2, p3.data());
    CubicHermiteSpline<Grid::DATA_DIMENSION>(
        p0, p1, p2, p3, c - col, f0.data(), df0dc.data());

    grid_.GetValue(row, col - 1, p0.data());
    grid_.GetValue(row, col, p1.data());
    grid_.GetValue(row, col + 1, p2.data());
    grid_.GetValue(row, col + 2, p3.data());
    CubicHermiteSpline<Grid::DATA_DIMENSION>(
        p0, p1, p2, p3, c - col, f1.data(), df1dc.data());

    grid_.GetValue(row + 1, col - 1, p0.data());
    grid_.GetValue(row + 1, col, p1.data());
    grid_.GetValue(row + 1, col + 1, p2.data());
    grid_.GetValue(row + 1, col + 2, p3.data());
    CubicHermiteSpline<Grid::DATA_DIMENSION>(
        p0, p1, p2, p3, c - col, f2.data(), df2dc.data());

    grid_.GetValue(row + 2, col - 1, p0.data());
    grid_.GetValue(row + 2, col, p1.data());
    grid_.GetValue(row + 2, col + 1, p2.data());
    grid_.GetValue(row + 2, col + 2, p3.data());
    CubicHermiteSpline<Grid::DATA_DIMENSION>(
        p0, p1, p2, p3, c - col, f3.data(), df3dc.data());

    // Interpolate vertically the interpolated value from each row and
    // compute the derivative along the columns.
    CubicHermiteSpline<Grid::DATA_DIMENSION>(f0, f1, f2, f3, r - row, f, dfdr);
    if (dfdc != nullptr) {
      // Interpolate vertically the derivative along the columns.
      CubicHermiteSpline<Grid::DATA_DIMENSION>(
          df0dc, df1dc, df2dc, df3dc, r - row, dfdc, nullptr);
    }
  }

  // The following two Evaluate overloads are needed for interfacing
  // with automatic differentiation. The first is for when a scalar
  // evaluation is done, and the second one is for when Jets are used.
  void Evaluate(const double& r, const double& c, double* f) const {
    Evaluate(r, c, f, nullptr, nullptr);
  }

  template <typename JetT>
  void Evaluate(const JetT& r, const JetT& c, JetT* f) const {
    double frc[Grid::DATA_DIMENSION];
    double dfdr[Grid::DATA_DIMENSION];
    double dfdc[Grid::DATA_DIMENSION];
    Evaluate(r.a, c.a, frc, dfdr, dfdc);
    for (int i = 0; i < Grid::DATA_DIMENSION; ++i) {
      f[i].a = frc[i];
      f[i].v = dfdr[i] * r.v + dfdc[i] * c.v;
    }
  }

 private:
  const Grid& grid_;
};

// An object that implements an infinite two dimensional grid needed
// by the BiCubicInterpolator where the source of the function values
// is an grid of type T on the grid
//
//   [(row_start,   col_start), ..., (row_start,   col_end - 1)]
//   [                          ...                            ]
//   [(row_end - 1, col_start), ..., (row_end - 1, col_end - 1)]
//
// Since the input grid is finite and the grid is infinite, values
// outside this interval needs to be computed. Grid2D uses the value
// from the nearest edge.
//
// The function being provided can be vector valued, in which case
// kDataDimension > 1. The data maybe stored in row or column major
// format and the various dimensional slices of the function maybe
// interleaved, or they maybe stacked, i.e, if the function has
// kDataDimension = 2, is stored in row-major format and if
// kInterleaved = true, then it is stored as
//
//   f001, f002, f011, f012, ...
//
// A commonly occuring example are color images (RGB) where the three
// channels are stored interleaved.
//
// If kInterleaved = false, then it is stored as
//
//  f001, f011, ..., fnm1, f002, f012, ...
template <typename T,
          int kDataDimension = 1,
          bool kRowMajor = true,
          bool kInterleaved = true>
struct Grid2D {
 public:
  enum { DATA_DIMENSION = kDataDimension };

  Grid2D(const T* data,
         const int row_begin,
         const int row_end,
         const int col_begin,
         const int col_end)
      : data_(data),
        row_begin_(row_begin),
        row_end_(row_end),
        col_begin_(col_begin),
        col_end_(col_end),
        num_rows_(row_end - row_begin),
        num_cols_(col_end - col_begin),
        num_values_(num_rows_ * num_cols_) {
    CHECK_GE(kDataDimension, 1);
    CHECK_LT(row_begin, row_end);
    CHECK_LT(col_begin, col_end);
  }

  EIGEN_STRONG_INLINE void GetValue(const int r, const int c, double* f) const {
    const int row_idx =
        (std::min)((std::max)(row_begin_, r), row_end_ - 1) - row_begin_;
    const int col_idx =
        (std::min)((std::max)(col_begin_, c), col_end_ - 1) - col_begin_;

    const int n = (kRowMajor) ? num_cols_ * row_idx + col_idx
                              : num_rows_ * col_idx + row_idx;

    if (kInterleaved) {
      for (int i = 0; i < kDataDimension; ++i) {
        f[i] = static_cast<double>(data_[kDataDimension * n + i]);
      }
    } else {
      for (int i = 0; i < kDataDimension; ++i) {
        f[i] = static_cast<double>(data_[i * num_values_ + n]);
      }
    }
  }

 private:
  const T* data_;
  const int row_begin_;
  const int row_end_;
  const int col_begin_;
  const int col_end_;
  const int num_rows_;
  const int num_cols_;
  const int num_values_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_CUBIC_INTERPOLATOR_H_

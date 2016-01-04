// Copyright (c) 2010 libmv authors.
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

#include "libmv/multiview/conditioning.h"
#include "libmv/multiview/projection.h"

namespace libmv {

// HZ 4.4.4 pag.109: Point conditioning (non isotropic)
void PreconditionerFromPoints(const Mat &points, Mat3 *T) {
  Vec mean, variance;
  MeanAndVarianceAlongRows(points, &mean, &variance);

  double xfactor = sqrt(2.0 / variance(0));
  double yfactor = sqrt(2.0 / variance(1));

  // If variance is equal to 0.0 set scaling factor to identity.
  // -> Else it will provide nan value (because division by 0).
  if (variance(0) < 1e-8)
    xfactor = mean(0) = 1.0;
  if (variance(1) < 1e-8)
    yfactor = mean(1) = 1.0;

  *T << xfactor, 0,       -xfactor * mean(0),
        0,       yfactor, -yfactor * mean(1),
        0,       0,        1;
}
// HZ 4.4.4 pag.107: Point conditioning (isotropic)
void IsotropicPreconditionerFromPoints(const Mat &points, Mat3 *T) {
  Vec mean, variance;
  MeanAndVarianceAlongRows(points, &mean, &variance);

  double var_norm = variance.norm();
  double factor = sqrt(2.0 / var_norm);

  // If variance is equal to 0.0 set scaling factor to identity.
  // -> Else it will provide nan value (because division by 0).
  if (var_norm < 1e-8) {
    factor = 1.0;
    mean.setOnes();
  }

  *T << factor, 0,       -factor * mean(0),
        0,       factor, -factor * mean(1),
        0,       0,        1;
}

void ApplyTransformationToPoints(const Mat &points,
                                 const Mat3 &T,
                                 Mat *transformed_points) {
  int n = points.cols();
  transformed_points->resize(2, n);
  Mat3X p(3, n);
  EuclideanToHomogeneous(points, &p);
  p = T * p;
  HomogeneousToEuclidean(p, transformed_points);
}

void NormalizePoints(const Mat &points,
                     Mat *normalized_points,
                     Mat3 *T) {
  PreconditionerFromPoints(points, T);
  ApplyTransformationToPoints(points, *T, normalized_points);
}

void NormalizeIsotropicPoints(const Mat &points,
                              Mat *normalized_points,
                              Mat3 *T) {
  IsotropicPreconditionerFromPoints(points, T);
  ApplyTransformationToPoints(points, *T, normalized_points);
}

// Denormalize the results. See HZ page 109.
void UnnormalizerT::Unnormalize(const Mat3 &T1, const Mat3 &T2, Mat3 *H)  {
  *H = T2.transpose() * (*H) * T1;
}

void UnnormalizerI::Unnormalize(const Mat3 &T1, const Mat3 &T2, Mat3 *H)  {
  *H = T2.inverse() * (*H) * T1;
}

}  // namespace libmv

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

#ifndef LIBMV_MULTIVIEW_CONDITIONNING_H_
#define LIBMV_MULTIVIEW_CONDITIONNING_H_

#include "libmv/numeric/numeric.h"

namespace libmv {

// Point conditioning (non isotropic)
void PreconditionerFromPoints(const Mat &points, Mat3 *T);
// Point conditioning (isotropic)
void IsotropicPreconditionerFromPoints(const Mat &points, Mat3 *T);

void ApplyTransformationToPoints(const Mat &points,
                                 const Mat3 &T,
                                 Mat *transformed_points);

void NormalizePoints(const Mat &points,
                     Mat *normalized_points,
                     Mat3 *T);

void NormalizeIsotropicPoints(const Mat &points,
                              Mat *normalized_points,
                              Mat3 *T);

/// Use inverse for unnormalize
struct UnnormalizerI {
  // Denormalize the results. See HZ page 109.
  static void Unnormalize(const Mat3 &T1, const Mat3 &T2, Mat3 *H);
};

/// Use transpose for unnormalize
struct UnnormalizerT {
  // Denormalize the results. See HZ page 109.
  static void Unnormalize(const Mat3 &T1, const Mat3 &T2, Mat3 *H);
};

}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_CONDITIONNING_H_

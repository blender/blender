// Copyright (c) 2012 libmv authors.
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

#ifndef LIBMV_IMAGE_CORRELATION_H
#define LIBMV_IMAGE_CORRELATION_H

#include "libmv/logging/logging.h"
#include "libmv/image/image.h"

namespace libmv {

inline double PearsonProductMomentCorrelation(
        const FloatImage &image_and_gradient1_sampled,
        const FloatImage &image_and_gradient2_sampled) {
  assert(image_and_gradient1_sampled.Width() ==
         image_and_gradient2_sampled.Width());
  assert(image_and_gradient1_sampled.Height() ==
         image_and_gradient2_sampled.Height());

  const int width = image_and_gradient1_sampled.Width(),
            height = image_and_gradient1_sampled.Height();
  double sX = 0, sY = 0, sXX = 0, sYY = 0, sXY = 0;

  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      double x = image_and_gradient1_sampled(r, c, 0);
      double y = image_and_gradient2_sampled(r, c, 0);
      sX += x;
      sY += y;
      sXX += x * x;
      sYY += y * y;
      sXY += x * y;
    }
  }

  // Normalize.
  double N = width * height;
  sX /= N;
  sY /= N;
  sXX /= N;
  sYY /= N;
  sXY /= N;

  double var_x = sXX - sX * sX;
  double var_y = sYY - sY * sY;
  double covariance_xy = sXY - sX * sY;

  double correlation = covariance_xy / sqrt(var_x * var_y);
  LG << "Covariance xy: " << covariance_xy
     << ", var 1: " << var_x << ", var 2: " << var_y
     << ", correlation: " << correlation;
  return correlation;
}

}  // namespace libmv

#endif  // LIBMV_IMAGE_IMAGE_CORRELATION_H

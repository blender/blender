// Copyright (c) 2011 libmv authors.
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

#include "libmv/tracking/trklt_region_tracker.h"

#include "libmv/logging/logging.h"
#include "libmv/numeric/numeric.h"
#include "libmv/image/image.h"
#include "libmv/image/convolve.h"
#include "libmv/image/sample.h"

namespace libmv {

// TODO(keir): Switch this to use the smarter LM loop like in ESM.

// Computes U and e from the Ud = e equation (number 14) from the paper.
static void ComputeTrackingEquation(const Array3Df &image_and_gradient1,
                                    const Array3Df &image_and_gradient2,
                                    double x1, double y1,
                                    double x2, double y2,
                                    int half_width,
                                    double lambda,
                                    Mat2f *U,
                                    Vec2f *e) {
  Mat2f A, B, C, D;
  A = B = C = D  = Mat2f::Zero();

  Vec2f R, S, V, W;
  R = S = V = W = Vec2f::Zero();

  for (int r = -half_width; r <= half_width; ++r) {
    for (int c = -half_width; c <= half_width; ++c) {
      float xx1 = x1 + c;
      float yy1 = y1 + r;
      float xx2 = x2 + c;
      float yy2 = y2 + r;

      float I = SampleLinear(image_and_gradient1, yy1, xx1, 0);
      float J = SampleLinear(image_and_gradient2, yy2, xx2, 0);

      Vec2f gI, gJ;
      gI << SampleLinear(image_and_gradient1, yy1, xx1, 1),
            SampleLinear(image_and_gradient1, yy1, xx1, 2);
      gJ << SampleLinear(image_and_gradient2, yy2, xx2, 1),
            SampleLinear(image_and_gradient2, yy2, xx2, 2);

      // Equation 15 from the paper.
      A += gI * gI.transpose();
      B += gI * gJ.transpose();
      C += gJ * gJ.transpose();
      R += I * gI;
      S += J * gI;
      V += I * gJ;
      W += J * gJ;
    }
  }

  // In the paper they show a D matrix, but it is just B transpose, so use that
  // instead of explicitly computing D.
  Mat2f Di = B.transpose().inverse();

  // Equation 14 from the paper.
  *U = A*Di*C + lambda*Di*C - 0.5*B;
  *e = (A + lambda*Mat2f::Identity())*Di*(V - W) + 0.5*(S - R);
}

static bool RegionIsInBounds(const FloatImage &image1,
                      double x, double y,
                      int half_window_size) {
  // Check the minimum coordinates.
  int min_x = floor(x) - half_window_size - 1;
  int min_y = floor(y) - half_window_size - 1;
  if (min_x < 0.0 ||
      min_y < 0.0) {
    return false;
  }

  // Check the maximum coordinates.
  int max_x = ceil(x) + half_window_size + 1;
  int max_y = ceil(y) + half_window_size + 1;
  if (max_x > image1.cols() ||
      max_y > image1.rows()) {
    return false;
  }

  // Ok, we're good.
  return true;
}

bool TrkltRegionTracker::Track(const FloatImage &image1,
                               const FloatImage &image2,
                               double  x1, double  y1,
                               double *x2, double *y2) const {
  if (!RegionIsInBounds(image1, x1, y1, half_window_size)) {
    LG << "Fell out of image1's window with x1=" << x1 << ", y1=" << y1
       << ", hw=" << half_window_size << ".";
    return false;
  }

  Array3Df image_and_gradient1;
  Array3Df image_and_gradient2;
  BlurredImageAndDerivativesChannels(image1, sigma, &image_and_gradient1);
  BlurredImageAndDerivativesChannels(image2, sigma, &image_and_gradient2);

  int i;
  Vec2f d = Vec2f::Zero();
  for (i = 0; i < max_iterations; ++i) {
    // Check that the entire image patch is within the bounds of the images.
    if (!RegionIsInBounds(image2, *x2, *y2, half_window_size)) {
      LG << "Fell out of image2's window with x2=" << *x2 << ", y2=" << *y2
         << ", hw=" << half_window_size << ".";
      return false;
    }

    // Compute gradient matrix and error vector.
    Mat2f U;
    Vec2f e;
    ComputeTrackingEquation(image_and_gradient1,
                            image_and_gradient2,
                            x1, y1,
                            *x2, *y2,
                            half_window_size,
                            lambda,
                            &U, &e);

    // Solve the linear system for the best update to x2 and y2.
    d = U.lu().solve(e);

    // Update the position with the solved displacement.
    *x2 += d[0];
    *y2 += d[1];

    // Check for the quality of the solution, but not until having already
    // updated the position with our best estimate. The reason to do the update
    // anyway is that the user already knows the position is bad, so we may as
    // well try our best.
    float determinant = U.determinant();
    if (fabs(determinant) < min_determinant) {
      // The determinant, which indicates the trackiness of the point, is too
      // small, so fail out.
      LG << "Determinant " << determinant << " is too small; failing tracking.";
      return false;
    }
    LG << "x=" << *x2 << ", y=" << *y2 << ", dx=" << d[0] << ", dy=" << d[1] << ", det=" << determinant;


    // If the update is small, then we probably found the target.
    if (d.squaredNorm() < min_update_squared_distance) {
      LG << "Successful track in " << i << " iterations.";
      return true;
    }
  }
  // Getting here means we hit max iterations, so tracking failed.
  LG << "Too many iterations; max is set to " << max_iterations << ".";
  return false;
}

}  // namespace libmv

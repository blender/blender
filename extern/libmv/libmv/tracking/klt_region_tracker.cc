// Copyright (c) 2007, 2008, 2009, 2011 libmv authors.
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

#include "libmv/logging/logging.h"
#include "libmv/tracking/klt_region_tracker.h"
#include "libmv/image/image.h"
#include "libmv/image/convolve.h"
#include "libmv/image/sample.h"

namespace libmv {

// Compute the gradient matrix noted by Z and the error vector e. See Good
// Features to Track.
//
// TODO(keir): The calls to SampleLinear() do boundary checking that should
// instead happen outside the loop. Since this is the innermost loop, the extra
// bounds checking hurts performance.
static void ComputeTrackingEquation(const Array3Df &image_and_gradient1,
                                    const Array3Df &image_and_gradient2,
                                    double x1, double y1,
                                    double x2, double y2,
                                    int half_width,
                                    float *gxx,
                                    float *gxy,
                                    float *gyy,
                                    float *ex,
                                    float *ey) {
  *gxx = *gxy = *gyy = 0;
  *ex = *ey = 0;
  for (int r = -half_width; r <= half_width; ++r) {
    for (int c = -half_width; c <= half_width; ++c) {
      float xx1 = x1 + c;
      float yy1 = y1 + r;
      float xx2 = x2 + c;
      float yy2 = y2 + r;
      float I =  SampleLinear(image_and_gradient1, yy1, xx1, 0);
      float J =  SampleLinear(image_and_gradient2, yy2, xx2, 0);
      float gx = SampleLinear(image_and_gradient2, yy2, xx2, 1);
      float gy = SampleLinear(image_and_gradient2, yy2, xx2, 2);
      *gxx += gx * gx;
      *gxy += gx * gy;
      *gyy += gy * gy;
      *ex += (I - J) * gx;
      *ey += (I - J) * gy;
    }
  }
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

bool KltRegionTracker::Track(const FloatImage &image1,
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
  float dx = 0, dy = 0;
  for (i = 0; i < max_iterations; ++i) {
    // Check that the entire image patch is within the bounds of the images.
    if (!RegionIsInBounds(image2, *x2, *y2, half_window_size)) {
      LG << "Fell out of image2's window with x2=" << *x2 << ", y2=" << *y2
         << ", hw=" << half_window_size << ".";
      return false;
    }

    // Compute gradient matrix and error vector.
    float gxx, gxy, gyy, ex, ey;
    ComputeTrackingEquation(image_and_gradient1,
                            image_and_gradient2,
                            x1, y1,
                            *x2, *y2,
                            half_window_size,
                            &gxx, &gxy, &gyy, &ex, &ey);

    // Solve the tracking equation
    //
    //   [gxx gxy] [dx] = [ex]
    //   [gxy gyy] [dy] = [ey]
    //
    // for dx and dy.  Borrowed from Stan Birchfield's KLT implementation.
    float determinant = gxx * gyy - gxy * gxy;
    dx = (gyy * ex - gxy * ey) / determinant;
    dy = (gxx * ey - gxy * ex) / determinant;

    // Update the position with the solved displacement.
    *x2 += dx;
    *y2 += dy;

    // Check for the quality of the solution, but not until having already
    // updated the position with our best estimate. The reason to do the update
    // anyway is that the user already knows the position is bad, so we may as
    // well try our best.
    if (determinant < min_determinant) {
      // The determinant, which indicates the trackiness of the point, is too
      // small, so fail out.
      LG << "Determinant " << determinant << " is too small; failing tracking.";
      return false;
    }
    LG << "x=" << *x2 << ", y=" << *y2 << ", dx=" << dx << ", dy=" << dy << ", det=" << determinant;

    // If the update is small, then we probably found the target.
    if (dx * dx + dy * dy < min_update_squared_distance) {
      LG << "Successful track in " << i << " iterations.";
      return true;
    }
  }
  // Getting here means we hit max iterations, so tracking failed.
  LG << "Too many iterations; max is set to " << max_iterations << ".";
  return false;
}

}  // namespace libmv

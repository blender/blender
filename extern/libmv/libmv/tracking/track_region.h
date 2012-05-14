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

#ifndef LIBMV_TRACKING_TRACK_REGION_H_

// Necessary for M_E when building with MSVC.
#define _USE_MATH_DEFINES

#include "libmv/tracking/esm_region_tracker.h"

#include "libmv/image/image.h"
#include "libmv/image/sample.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

struct TrackRegionOptions {
  enum Mode {
    TRANSLATION,
    TRANSLATION_ROTATION,
    TRANSLATION_SCALE,
    TRANSLATION_ROTATION_SCALE,
    AFFINE,
    HOMOGRAPHY,
  };
  Mode mode;

  int num_samples_x;
  int num_samples_y;

  double minimum_correlation;
  int max_iterations;

  // Use the "Efficient Second-order Minimization" scheme. This increases
  // convergence speed at the cost of more per-iteration work.
  bool use_esm;

  double sigma;
};

struct TrackRegionResult {
  enum Termination {
    // Ceres termination types, duplicated.
    PARAMETER_TOLERANCE = 0,
    FUNCTION_TOLERANCE,
    GRADIENT_TOLERANCE,
    NO_CONVERGENCE,
    DID_NOT_RUN,
    NUMERICAL_FAILURE,

    // Libmv specific errors.
    SOURCE_OUT_OF_BOUNDS,
    DESTINATION_OUT_OF_BOUNDS,
    FELL_OUT_OF_BOUNDS,
    INSUFFICIENT_CORRELATION,
  };
  Termination termination;

  int num_iterations;
  double correlation;

  // Final parameters?
};

// Always needs 4 correspondences.
void TrackRegion(const FloatImage &image1,
                 const FloatImage &image2,
                 const double *x1, const double *y1,
                 const TrackRegionOptions &options,
                 double *x2, double *y2,
                 TrackRegionResult *result);

// TODO(keir): May need a "samplewarp" function.

}  // namespace libmv

#endif  // LIBMV_TRACKING_TRACK_REGION_H_

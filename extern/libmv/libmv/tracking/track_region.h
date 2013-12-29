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

#include "libmv/image/image.h"
#include "libmv/image/sample.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

struct TrackRegionOptions {
  TrackRegionOptions();

  enum Mode {
    TRANSLATION,
    TRANSLATION_ROTATION,
    TRANSLATION_SCALE,
    TRANSLATION_ROTATION_SCALE,
    AFFINE,
    HOMOGRAPHY,
  };
  Mode mode;

  double minimum_correlation;
  int max_iterations;

  // Use the "Efficient Second-order Minimization" scheme. This increases
  // convergence speed at the cost of more per-iteration work.
  bool use_esm;

  // If true, apply a brute-force translation-only search before attempting the
  // full search. This is not enabled if the destination image ("image2") is
  // too small; in that case eithen the basin of attraction is close enough
  // that the nearby minima is correct, or the search area is too small.
  bool use_brute_initialization;

  // If true and brute initialization is enabled, first try refining with the
  // initial guess instead of starting with the brute initialization. If the
  // initial refinement fails, then a normal brute search followed by
  // refinement is attempted. If the initial refinement succeeds, then the
  // result is returned as is (skipping a costly brute search).
  bool attempt_refine_before_brute;

  // If true, normalize the image patches by their mean before doing the sum of
  // squared error calculation. This is reasonable since the effect of
  // increasing light intensity is multiplicative on the pixel intensities.
  //
  // Note: This does nearly double the solving time, so it is not advised to
  // turn this on all the time.
  bool use_normalized_intensities;

  // The size in pixels of the blur kernel used to both smooth the image and
  // take the image derivative.
  double sigma;

  // Extra points that should get transformed by the warp. These points are
  // appended to the x and y arrays. This is useful because the actual warp
  // parameters are not exposed.
  int num_extra_points;

  // For motion models other than translation, the optimizer sometimes has
  // trouble deciding what to do around flat areas in the cost function. This
  // leads to the optimizer picking poor solutions near the minimum. Visually,
  // the effect is that the quad corners jiggle around, even though the center
  // of the patch is well estimated. regularization_coefficient controls a term
  // in the sum of squared error cost that makes it expensive for the optimizer
  // to pick a warp that changes the shape of the patch dramatically (e.g.
  // rotating, scaling, skewing, etc).
  //
  // In particular it adds an 8-residual cost function to the optimization,
  // where each corner induces 2 residuals: the difference between the warped
  // and the initial guess. However, the patch centroids are subtracted so that
  // only patch distortions are penalized.
  //
  // If zero, no regularization is used.
  double regularization_coefficient;

  // If the maximum shift of any patch corner between successful iterations of
  // the solver is less than this amount, then the tracking is declared
  // successful. The solver termination becomes PARAMETER_TOLERANCE.
  double minimum_corner_shift_tolerance_pixels;

  // If non-null, this is used as the pattern mask. It should match the size of
  // image1, even though only values inside the image1 quad are examined. The
  // values must be in the range 0.0 to 0.1.
  FloatImage *image1_mask;
};

struct TrackRegionResult {
  enum Termination {
    // Ceres termination types, duplicated; though, not the int values.
    CONVERGENCE,
    NO_CONVERGENCE,
    FAILURE,

    // Libmv specific errors.
    SOURCE_OUT_OF_BOUNDS,
    DESTINATION_OUT_OF_BOUNDS,
    FELL_OUT_OF_BOUNDS,
    INSUFFICIENT_CORRELATION,
    INSUFFICIENT_PATTERN_AREA,
    CONFIGURATION_ERROR,
  };
  Termination termination;

  int num_iterations;
  double correlation;

  // Final parameters?
  bool used_brute_translation_initialization;
};

// Always needs 4 correspondences.
void TrackRegion(const FloatImage &image1,
                 const FloatImage &image2,
                 const double *x1, const double *y1,
                 const TrackRegionOptions &options,
                 double *x2, double *y2,
                 TrackRegionResult *result);

// Sample a "canonical" version of the passed planar patch, using bilinear
// sampling. The passed corners must be within the image, and have at least two
// pixels of border around them. (so e.g. a corner of the patch cannot lie
// directly on the edge of the image). Four corners are always required. All
// channels are interpolated.
// When mask is not null it'll be used as a pattern mask. Ot should match
// the size of image.
// Warped coordinates of marker's position would be returned in
// warped_position_x and warped_position_y
bool SamplePlanarPatch(const FloatImage &image,
                       const double *xs, const double *ys,
                       int num_samples_x, int num_samples_y,
                       FloatImage *mask, FloatImage *patch,
                       double *warped_position_x, double *warped_position_y);

}  // namespace libmv

#endif  // LIBMV_TRACKING_TRACK_REGION_H_

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

#include "libmv/tracking/hybrid_region_tracker.h"

#include "libmv/image/image.h"
#include "libmv/image/convolve.h"
#include "libmv/image/sample.h"
#include "libmv/logging/logging.h"

namespace libmv {

bool HybridRegionTracker::Track(const FloatImage &image1,
                                const FloatImage &image2,
                                double  x1, double  y1,
                                double *x2, double *y2) const {
  double x2_coarse = *x2;
  double y2_coarse = *y2;
  if (!coarse_tracker_->Track(image1, image2, x1, y1, &x2_coarse, &y2_coarse)) {
    LG << "Coarse tracker failed.";
    return false;
  }

  double x2_fine = x2_coarse;
  double y2_fine = y2_coarse;
  if (!fine_tracker_->Track(image1, image2, x1, y1, &x2_fine, &y2_fine)) {
    LG << "Fine tracker failed.";
    return false;
  }

  // Calculate the shift done by the fine tracker.
  double dx2 = x2_coarse - x2_fine;
  double dy2 = y2_coarse - y2_fine;
  double fine_shift = sqrt(dx2 * dx2 + dy2 * dy2);

  LG << "Refinement: dx=" << dx2 << " dy=" << dy2 << ", d=" << fine_shift;

  // If the fine tracker shifted the window by more than a pixel, then
  // something bad probably happened and we should give up tracking.
  if (fine_shift < 2.0) {
    LG << "Refinement small enough; success.";
    *x2 = x2_fine;
    *y2 = y2_fine;
    return true;
  }
  LG << "Refinement was too big; failing.";
  return false;
}

}  // namespace libmv

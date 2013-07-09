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

#include "libmv/tracking/retrack_region_tracker.h"

#include <cmath>
#include <vector>

namespace libmv {

bool RetrackRegionTracker::Track(const FloatImage &image1,
                                 const FloatImage &image2,
                                 double  x1, double  y1,
                                 double *x2, double *y2) const {
  // Track forward, getting x2 and y2.
  if (!tracker_->Track(image1, image2, x1, y1, x2, y2)) {
    return false;
  }
  // Now track x2 and y2 backward, to get xx1 and yy1 which, if the track is
  // good, should match x1 and y1 (but may not if the track is bad).
  double xx1 = *x2, yy1 = *x2;
  if (!tracker_->Track(image2, image1, *x2, *y2, &xx1, &yy1)) {
    return false;
  }
  double dx = xx1 - x1;
  double dy = yy1 - y1;
  return sqrt(dx * dx + dy * dy) < tolerance_;
}

}  // namespace libmv

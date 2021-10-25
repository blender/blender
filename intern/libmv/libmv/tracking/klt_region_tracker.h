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

#ifndef LIBMV_REGION_TRACKING_KLT_REGION_TRACKER_H_
#define LIBMV_REGION_TRACKING_KLT_REGION_TRACKER_H_

#include "libmv/image/image.h"
#include "libmv/tracking/region_tracker.h"

namespace libmv {

struct KltRegionTracker : public RegionTracker {
  KltRegionTracker()
      : half_window_size(4),
        max_iterations(16),
        min_determinant(1e-6),
        min_update_squared_distance(1e-6),
        sigma(0.9) {}

  virtual ~KltRegionTracker() {}

  // Tracker interface.
  virtual bool Track(const FloatImage &image1,
                     const FloatImage &image2,
                     double  x1, double  y1,
                     double *x2, double *y2) const;

  // No point in creating getters or setters.
  int half_window_size;
  int max_iterations;
  double min_determinant;
  double min_update_squared_distance;
  double sigma;
};

}  // namespace libmv

#endif  // LIBMV_REGION_TRACKING_KLT_REGION_TRACKER_H_

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

#ifndef LIBMV_TRACKING_TRACKER_H_
#define LIBMV_TRACKING_TRACKER_H_

#include "libmv/image/image.h"

namespace libmv {

class RegionTracker {
 public:
  RegionTracker() {}
  virtual ~RegionTracker() {}

  /*!
      Track a point from \a image1 to \a image2.

      \a x2, \a y2 should start out as a best guess for the position in \a
      image2. If no guess is available, (\a x1, \a y1) is a good start. Returns
      true on success, false otherwise
  */
  virtual bool Track(const FloatImage &image1,
                     const FloatImage &image2,
                     double  x1, double  y1,
                     double *x2, double *y2) const = 0;
};

}  // namespace libmv

#endif  // LIBMV_CORRESPONDENCE_TRACKER_H_

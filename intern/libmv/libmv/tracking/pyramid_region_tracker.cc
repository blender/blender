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

#include "libmv/tracking/pyramid_region_tracker.h"

#include <vector>

#include "libmv/image/convolve.h"
#include "libmv/image/image.h"
#include "libmv/image/sample.h"
#include "libmv/logging/logging.h"

namespace libmv {

static void MakePyramid(const FloatImage &image, int num_levels,
                        std::vector<FloatImage> *pyramid) {
  pyramid->resize(num_levels);
  (*pyramid)[0] = image;
  for (int i = 1; i < num_levels; ++i) {
    DownsampleChannelsBy2((*pyramid)[i - 1], &(*pyramid)[i]);
  }
}

bool PyramidRegionTracker::Track(const FloatImage &image1,
                                 const FloatImage &image2,
                                 double  x1, double  y1,
                                 double *x2, double *y2) const {
  // Shrink the guessed x and y location to match the coarsest level + 1 (which
  // when gets corrected in the loop).
  *x2 /= pow(2., num_levels_);
  *y2 /= pow(2., num_levels_);

  // Create all the levels of the pyramid, since tracking has to happen from
  // the coarsest to finest levels, which means holding on to all levels of the
  // pyraid at once.
  std::vector<FloatImage> pyramid1(num_levels_);
  std::vector<FloatImage> pyramid2(num_levels_);
  MakePyramid(image1, num_levels_, &pyramid1);
  MakePyramid(image2, num_levels_, &pyramid2);

  for (int i = num_levels_ - 1; i >= 0; --i) {
    // Position in the first image at pyramid level i.
    double xx = x1 / pow(2., i);
    double yy = y1 / pow(2., i);

    // Guess the new tracked position is where the last level tracked to.
    *x2 *= 2;
    *y2 *= 2;

    // Save the previous best guess for this level, since tracking within this
    // level might fail.
    double x2_new = *x2;
    double y2_new = *y2;

    // Track the point on this level with the base tracker.
    LG << "Tracking on level " << i;
    bool succeeded = tracker_->Track(pyramid1[i], pyramid2[i], xx, yy,
                                     &x2_new, &y2_new);

    if (!succeeded) {
      if (i == 0) {
        // Only fail on the highest-resolution level, because a failure on a
        // coarse level does not mean failure at a lower level (consider
        // out-of-bounds conditions).
        LG << "Finest level of pyramid tracking failed; failing.";
        return false;
      }

      LG << "Failed to track at level " << i << "; restoring guess.";
    } else {
      // Only save the update if the track for this level succeeded. This is a
      // bit of a hack; the jury remains out on whether this is better than
      // re-using the previous failed-attempt.
      *x2 = x2_new;
      *y2 = y2_new;
    }
  }
  return true;
}

}  // namespace libmv

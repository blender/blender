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

#include "libmv/tracking/pyramid_region_tracker.h"
#include "libmv/tracking/klt_region_tracker.h"
#include "libmv/image/image.h"
#include "testing/testing.h"

namespace libmv {
namespace {

TEST(PyramidKltRegionTracker, Track) {
  Array3Df image1(100, 100);
  image1.Fill(0);

  Array3Df image2(image1);

  int x1 = 25, y1 = 25;
  image1(y1 + 0, x1 + 0) = 1.0f;
  image1(y1 + 0, x1 + 1) = 1.0f;
  image1(y1 + 1, x1 + 0) = 1.0f;
  image1(y1 + 1, x1 + 1) = 1.0f;

  // Make the displacement too large for a single-level KLT.
  int x2 = x1 + 6, y2 = y1 + 5;
  image2(y2 + 0, x2 + 0) = 1.0f;
  image2(y2 + 0, x2 + 1) = 1.0f;
  image2(y2 + 1, x2 + 0) = 1.0f;
  image2(y2 + 1, x2 + 1) = 1.0f;

  // Use a small 5x5 tracking region.
  int half_window_size = 3;

  // Ensure that the track doesn't work with one level of KLT.
  {
    double x2_actual = x1;
    double y2_actual = y1;

    KltRegionTracker tracker;
    tracker.half_window_size = half_window_size;
    EXPECT_FALSE(tracker.Track(image1, image2, x1, y1,
                               &x2_actual, &y2_actual));
  }

  // Verify that it works with the pyramid tracker.
  {
    double x2_actual = x1;
    double y2_actual = y1;

    KltRegionTracker *klt_tracker = new KltRegionTracker;
    klt_tracker->half_window_size = half_window_size;

    PyramidRegionTracker tracker(klt_tracker, 3);
    EXPECT_TRUE(tracker.Track(image1, image2, x1, y1,
                              &x2_actual, &y2_actual));

    EXPECT_NEAR(x2_actual, x2, 0.001);
    EXPECT_NEAR(y2_actual, y2, 0.001);
  }
}

}  // namespace
}  // namespace libmv

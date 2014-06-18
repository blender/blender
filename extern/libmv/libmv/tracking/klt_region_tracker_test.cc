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

#include "libmv/tracking/klt_region_tracker.h"
#include "libmv/image/image.h"
#include "testing/testing.h"

namespace libmv {
namespace {

TEST(KltRegionTracker, Track) {
  Array3Df image1(51, 51);
  image1.Fill(0);

  Array3Df image2(image1);

  int x0 = 25, y0 = 25;
  int dx = 3, dy = 2;
  image1(y0, x0) = 1.0f;
  image2(y0 + dy, x0 + dx) = 1.0f;

  double x1 = x0;
  double y1 = y0;

  KltRegionTracker tracker;
  tracker.half_window_size = 6;
  EXPECT_TRUE(tracker.Track(image1, image2, x0, y0, &x1, &y1));

  EXPECT_NEAR(x1, x0 + dx, 0.001);
  EXPECT_NEAR(y1, y0 + dy, 0.001);
}

}  // namespace
}  // namespace libmv

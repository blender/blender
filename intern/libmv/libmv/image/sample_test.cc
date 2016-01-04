// Copyright (c) 2007, 2008 libmv authors.
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

#include "libmv/image/sample.h"
#include "testing/testing.h"

using namespace libmv;

namespace {

TEST(Image, Nearest) {
  Array3Du image(2, 2);
  image(0, 0) = 0;
  image(0, 1) = 1;
  image(1, 0) = 2;
  image(1, 1) = 3;
  EXPECT_EQ(0, SampleNearest(image, -0.4f, -0.4f));
  EXPECT_EQ(0, SampleNearest(image,  0.4f,  0.4f));
  EXPECT_EQ(3, SampleNearest(image,  0.6f,  0.6f));
  EXPECT_EQ(3, SampleNearest(image,  1.4f,  1.4f));
}

TEST(Image, Linear) {
  Array3Df image(2, 2);
  image(0, 0) = 0;
  image(0, 1) = 1;
  image(1, 0) = 2;
  image(1, 1) = 3;
  EXPECT_EQ(1.5, SampleLinear(image, 0.5, 0.5));
}

TEST(Image, DownsampleBy2) {
  Array3Df image(2, 2);
  image(0, 0) = 0;
  image(0, 1) = 1;
  image(1, 0) = 2;
  image(1, 1) = 3;
  Array3Df resampled_image;
  DownsampleChannelsBy2(image, &resampled_image);
  ASSERT_EQ(1, resampled_image.Height());
  ASSERT_EQ(1, resampled_image.Width());
  ASSERT_EQ(1, resampled_image.Depth());
  EXPECT_FLOAT_EQ(6./4., resampled_image(0, 0));
}

TEST(Image, DownsampleBy2MultiChannel) {
  Array3Df image(2, 2, 3);
  image(0, 0, 0) = 0;
  image(0, 1, 0) = 1;
  image(1, 0, 0) = 2;
  image(1, 1, 0) = 3;

  image(0, 0, 1) = 5;
  image(0, 1, 1) = 6;
  image(1, 0, 1) = 7;
  image(1, 1, 1) = 8;

  image(0, 0, 2) = 9;
  image(0, 1, 2) = 10;
  image(1, 0, 2) = 11;
  image(1, 1, 2) = 12;

  Array3Df resampled_image;
  DownsampleChannelsBy2(image, &resampled_image);
  ASSERT_EQ(1, resampled_image.Height());
  ASSERT_EQ(1, resampled_image.Width());
  ASSERT_EQ(3, resampled_image.Depth());
  EXPECT_FLOAT_EQ((0+1+2+3)/4.,    resampled_image(0, 0, 0));
  EXPECT_FLOAT_EQ((5+6+7+8)/4.,    resampled_image(0, 0, 1));
  EXPECT_FLOAT_EQ((9+10+11+12)/4., resampled_image(0, 0, 2));
}
}  // namespace

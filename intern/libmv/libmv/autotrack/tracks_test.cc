// Copyright (c) 2014 libmv authors.
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
//
// Author: mierle@gmail.com (Keir Mierle)

#include "libmv/autotrack/tracks.h"

#include "testing/testing.h"
#include "libmv/logging/logging.h"

namespace mv {

TEST(Tracks, MaxFrame) {
  Marker marker;
  Tracks tracks;

  // Add some markers to clip 0.
  marker.clip = 0;
  marker.frame = 1;
  tracks.AddMarker(marker);

  // Add some markers to clip 1.
  marker.clip = 1;
  marker.frame = 1;
  tracks.AddMarker(marker);

  marker.clip = 1;
  marker.frame = 12;
  tracks.AddMarker(marker);

  EXPECT_EQ(1, tracks.MaxFrame(0));
  EXPECT_EQ(12, tracks.MaxFrame(1));
}

}  // namespace mv

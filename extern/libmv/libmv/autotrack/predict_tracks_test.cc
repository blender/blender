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

#include "libmv/autotrack/predict_tracks.h"

#include "libmv/autotrack/marker.h"
#include "libmv/autotrack/tracks.h"
#include "libmv/logging/logging.h"
#include "testing/testing.h"

namespace mv {

void AddMarker(int frame, float x, float y, Tracks* tracks) {
  Marker marker;
  marker.clip = marker.track = 0;
  marker.frame = frame;
  marker.center.x() = x;
  marker.center.y() = y;
  marker.patch.coordinates << x - 1, y - 1,
                              x + 1, y - 1,
                              x + 1, y + 1,
                              x - 1, y + 1;
  tracks->AddMarker(marker);
}

TEST(PredictMarkerPosition, EasyLinearMotion) {
  Tracks tracks;
  AddMarker(0, 1.0,  0.0, &tracks);
  AddMarker(1, 2.0,  5.0, &tracks);
  AddMarker(2, 3.0, 10.0, &tracks);
  AddMarker(3, 4.0, 15.0, &tracks);
  AddMarker(4, 5.0, 20.0, &tracks);
  AddMarker(5, 6.0, 25.0, &tracks);
  AddMarker(6, 7.0, 30.0, &tracks);
  AddMarker(7, 8.0, 35.0, &tracks);

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 8;

  PredictMarkerPosition(tracks, &predicted);
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 0.1);

  // Check the patch coordinates as well.
  double x = 9, y = 40.0;
  Quad2Df expected_patch;
  expected_patch.coordinates << x - 1, y - 1,
                                x + 1, y - 1,
                                x + 1, y + 1,
                                x - 1, y + 1;

  error = (expected_patch.coordinates - predicted.patch.coordinates).norm();
  LG << "Patch error: " << error;
  EXPECT_LT(error, 0.1);
}

TEST(PredictMarkerPosition, EasyBackwardLinearMotion) {
  Tracks tracks;
  AddMarker(8, 1.0,  0.0, &tracks);
  AddMarker(7, 2.0,  5.0, &tracks);
  AddMarker(6, 3.0, 10.0, &tracks);
  AddMarker(5, 4.0, 15.0, &tracks);
  AddMarker(4, 5.0, 20.0, &tracks);
  AddMarker(3, 6.0, 25.0, &tracks);
  AddMarker(2, 7.0, 30.0, &tracks);
  AddMarker(1, 8.0, 35.0, &tracks);

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 0;

  PredictMarkerPosition(tracks, &predicted);
  LG << predicted;
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 0.1);

  // Check the patch coordinates as well.
  double x = 9.0, y = 40.0;
  Quad2Df expected_patch;
  expected_patch.coordinates << x - 1, y - 1,
                                x + 1, y - 1,
                                x + 1, y + 1,
                                x - 1, y + 1;

  error = (expected_patch.coordinates - predicted.patch.coordinates).norm();
  LG << "Patch error: " << error;
  EXPECT_LT(error, 0.1);
}

TEST(PredictMarkerPosition, TwoFrameGap) {
  Tracks tracks;
  AddMarker(0, 1.0,  0.0, &tracks);
  AddMarker(1, 2.0,  5.0, &tracks);
  AddMarker(2, 3.0, 10.0, &tracks);
  AddMarker(3, 4.0, 15.0, &tracks);
  AddMarker(4, 5.0, 20.0, &tracks);
  AddMarker(5, 6.0, 25.0, &tracks);
  AddMarker(6, 7.0, 30.0, &tracks);
  // Missing frame 7!

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 8;

  PredictMarkerPosition(tracks, &predicted);
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 0.1);
}

TEST(PredictMarkerPosition, FourFrameGap) {
  Tracks tracks;
  AddMarker(0, 1.0,  0.0, &tracks);
  AddMarker(1, 2.0,  5.0, &tracks);
  AddMarker(2, 3.0, 10.0, &tracks);
  AddMarker(3, 4.0, 15.0, &tracks);
  // Missing frames 4, 5, 6, 7.

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 8;

  PredictMarkerPosition(tracks, &predicted);
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 2.0);  // Generous error due to larger prediction window.
}

TEST(PredictMarkerPosition, MultipleGaps) {
  Tracks tracks;
  AddMarker(0, 1.0,  0.0, &tracks);
  AddMarker(1, 2.0,  5.0, &tracks);
  AddMarker(2, 3.0, 10.0, &tracks);
  // AddMarker(3, 4.0, 15.0, &tracks);   // Note the 3-frame gap.
  // AddMarker(4, 5.0, 20.0, &tracks);
  // AddMarker(5, 6.0, 25.0, &tracks);
  AddMarker(6, 7.0, 30.0, &tracks);      // Intermediate measurement.
  // AddMarker(7, 8.0, 35.0, &tracks);

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 8;

  PredictMarkerPosition(tracks, &predicted);
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 1.0);  // Generous error due to larger prediction window.
}

TEST(PredictMarkerPosition, MarkersInRandomOrder) {
  Tracks tracks;

  // This is the same as the easy, except that the tracks are randomly ordered.
  AddMarker(0, 1.0,  0.0, &tracks);
  AddMarker(2, 3.0, 10.0, &tracks);
  AddMarker(7, 8.0, 35.0, &tracks);
  AddMarker(5, 6.0, 25.0, &tracks);
  AddMarker(4, 5.0, 20.0, &tracks);
  AddMarker(3, 4.0, 15.0, &tracks);
  AddMarker(6, 7.0, 30.0, &tracks);
  AddMarker(1, 2.0,  5.0, &tracks);

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 8;

  PredictMarkerPosition(tracks, &predicted);
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 0.1);
}

}  // namespace mv

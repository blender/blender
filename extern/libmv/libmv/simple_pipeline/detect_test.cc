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

#include "libmv/simple_pipeline/detect.h"

#include "testing/testing.h"
#include "libmv/logging/logging.h"

namespace libmv {

namespace {

void PreformSinglePointTest(const DetectOptions &options) {
  // Prepare the image.
  FloatImage image(15, 15);
  image.fill(1.0);
  image(7, 7) = 0.0;

  // Run the detector.
  vector<Feature> detected_features;
  Detect(image, options, &detected_features);

  // Check detected features matches our expectations.
  EXPECT_EQ(1, detected_features.size());
  if (detected_features.size() == 1) {
    Feature &feature = detected_features[0];
    EXPECT_EQ(7, feature.x);
    EXPECT_EQ(7, feature.y);
  }
}

void PreformCheckerBoardTest(const DetectOptions &options) {
  // Prepare the image.
  FloatImage image(30, 30);
  for (int y = 0; y < image.Height(); ++y) {
    for (int x = 0; x < image.Width(); ++x) {
      image(y, x) = (x / 10 + y / 10) % 2 ? 1.0 : 0.0;
    }
  }

  // Run the detector.
  vector<Feature> detected_features;
  Detect(image, options, &detected_features);

  // Check detected features matches our expectations.

  // We expect here only corners of a center square to be
  // considered a feature points.
  EXPECT_EQ(4, detected_features.size());

  // We don't know which side of the corner detector will choose,
  // so what we're checking here is that detected feature is from
  // any side of the corner.
  //
  // This doesn't check whether there're multiple features which
  // are placed on different sides of the same corner. The way we
  // deal with this is requiring min_distance to be greater than 2px.
  for (int i = 0; i < detected_features.size(); ++i) {
    Feature &feature = detected_features[i];
    int rounded_x = ((feature.x + 1) / 10) * 10,
        rounded_y = ((feature.y + 1) / 10) * 10;
    EXPECT_LE(1, std::abs(feature.x - rounded_x));
    EXPECT_LE(1, std::abs(feature.y - rounded_y));
  }
}

void CheckExpectedFeatures(const vector<Feature> &detected_features,
                           const vector<Feature> &expected_features) {
  EXPECT_EQ(expected_features.size(), detected_features.size());

  // That's unsafe to iterate over vectors when their lengths
  // doesn't match. And it doesn't make any sense actually since
  // the test will already be considered failed here.
  if (expected_features.size() != detected_features.size()) {
    return;
  }

  for (int i = 0; i < expected_features.size(); ++i) {
    const Feature &extected_feature = expected_features[i];
    bool found = false;
    for (int j = 0; j < detected_features.size(); ++j) {
      const Feature &detected_feature = detected_features[j];
      if (extected_feature.x == detected_feature.x &&
          extected_feature.y == detected_feature.y) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

void PreformSingleTriangleTest(const DetectOptions &options) {
  // Prepare the image.
  FloatImage image(15, 21);
  image.fill(1.0);

  int vertex_x = 10, vertex_y = 5;
  for (int i = 0; i < 6; ++i) {
    int current_x = vertex_x - i,
        current_y = vertex_y + i;
    for (int j = 0; j < i * 2 + 1; ++j, ++current_x) {
      image(current_y, current_x) = 0.0;
    }
  }

  // Run the detector.
  vector<Feature> detected_features;
  Detect(image, options, &detected_features);

  // Check detected features matches our expectations.
  vector<Feature> expected_features;
  expected_features.push_back(Feature(6, 10));
  expected_features.push_back(Feature(14, 10));
  expected_features.push_back(Feature(10, 6));

  CheckExpectedFeatures(detected_features, expected_features);
}

}  // namespace

#ifndef LIBMV_NO_FAST_DETECTOR
TEST(Detect, FASTSinglePointTest) {
  DetectOptions options;
  options.type = DetectOptions::FAST;
  options.min_distance = 0;
  options.fast_min_trackness = 1;

  PreformSinglePointTest(options);
}
#endif  // LIBMV_NO_FAST_DETECTOR

#if 0
// TODO(sergey): FAST doesn't detect checker board corners, but should it?
TEST(Detect, FASTCheckerBoardTest) {
  DetectOptions options;
  options.type = DetectOptions::FAST;
  options.min_distance = 0;
  options.fast_min_trackness = 1;

  PreformCheckerBoardTest(options);
}
#endif

#if 0
// TODO(sergey): FAST doesn't detect triangle corners!
TEST(Detect, FASTSingleTriangleTest) {
  DetectOptions options;
  options.type = DetectOptions::FAST;
  options.margin = 3;
  options.min_distance = 0;
  options.fast_min_trackness = 2;

  PreformSingleTriangleTest(options);
}
#endif

#if 0
// TODO(sergey): This doesn't actually detect single point,
// but should it or it's expected that Moravec wouldn't consider
// single point as  feature?
//
// Uncomment this or remove as soon as we know answer for the
// question.
TEST(Detect, MoravecSinglePointTest) {
  DetectOptions options;
  options.type = DetectOptions::MORAVEC;
  options.min_distance = 0;
  options.moravec_max_count = 10;

  PreformSinglePointTest(options);
}

// TODO(sergey): Moravec doesn't detect checker board corners, but should it?
TEST(Detect, MoravecCheckerBoardTest) {
  DetectOptions options;
  options.type = DetectOptions::MORAVEC;
  options.min_distance = 0;
  options.moravec_max_count = 10;

  PreformCheckerBoardTest(options);
}
#endif

TEST(Detect, HarrisSinglePointTest) {
  DetectOptions options;
  options.type = DetectOptions::HARRIS;

  // Set this to non-zero so image corners are not considered
  // a feature points and avoid center point neighbors to be
  // considered a features as well.
  options.margin = 3;
  options.min_distance = 3;

  PreformSinglePointTest(options);
}

TEST(Detect, HarrisSingleTriangleTest) {
  DetectOptions options;
  options.type = DetectOptions::HARRIS;

  options.margin = 3;
  options.min_distance = 2;
  options.harris_threshold = 1e-3;

  PreformSingleTriangleTest(options);
}

// TODO(sergey): Add tests for margin option.

// TODO(sergey): Add tests for min_distance option.

}  // namespace libmv

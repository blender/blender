/****************************************************************************
**
** Copyright (c) 2011 libmv authors.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
**
****************************************************************************/

#ifndef LIBMV_SIMPLE_PIPELINE_DETECT_H_
#define LIBMV_SIMPLE_PIPELINE_DETECT_H_

#include <vector>

#include "libmv/base/vector.h"
#include "libmv/image/image.h"

namespace libmv {

typedef unsigned char ubyte;

/*!
    A Feature is the 2D location of a detected feature in an image.

    \a x, \a y is the position of the feature in pixels from the top left corner.
    \a score is an estimate of how well the feature will be tracked.
    \a size can be used as an initial pattern size to track the feature.

    \sa Detect
*/
struct Feature {
  /// Position in pixels (from top-left corner)
  /// \note libmv might eventually support subpixel precision.
  float x, y;
  /// Trackness of the feature
  float score;
  /// Size of the feature in pixels
  float size;
};

struct DetectOptions {
  DetectOptions();

  // TODO(sergey): Write descriptions to each of algorithms.
  enum DetectorType {
    FAST,
    MORAVEC,
    HARRIS,
  };
  DetectorType type;

  // Margin in pixels from the image boundary.
  // No features will be detected within the margin.
  int margin;

  // Minimal distance between detected features.
  int min_distance;

  // Minimum score to add a feature. Only used by FAST detector.
  //
  // TODO(sergey): Write more detailed documentation about which
  // units this value is measured in and so on.
  int fast_min_trackness;

  // Maximum count to detect. Only used by MORAVEC detector.
  int moravec_max_count;

  // Find only features similar to this pattern. Only used by MORAVEC detector.
  //
  // This is an image patch denoted in byte array with dimensions of 16px by 16px
  // used to filter features by similarity to this patch.
  unsigned char *moravec_pattern;

  // Threshold value of the Harris function to add new featrue
  // to the result.
  double harris_threshold;
};

// Detect features on a given image using given detector options.
//
// Image could have 1-4 channels, it'll be converted to a grayscale
// by the detector function if needed.
void Detect(const FloatImage &image,
            const DetectOptions &options,
            vector<Feature> *detected_features);

}  // namespace libmv

#endif

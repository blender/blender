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

#include <stdlib.h>
#include <memory.h>
#include <queue>

#include "libmv/base/scoped_ptr.h"
#include "libmv/image/array_nd.h"
#include "libmv/image/image_converter.h"
#include "libmv/image/convolve.h"
#include "libmv/logging/logging.h"
#include "libmv/simple_pipeline/detect.h"

#ifndef LIBMV_NO_FAST_DETECTOR
#  include <third_party/fast/fast.h>
#endif

#ifdef __SSE2__
#  include <emmintrin.h>
#endif

namespace libmv {

namespace {

class FeatureComparison {
 public:
  bool operator() (const Feature &left, const Feature &right) const {
    return right.score > left.score;
  }
};

// Filter the features so there are no features closer than
// minimal distance to each other.
// This is a naive implementation with O(n^2) asymptotic.
void FilterFeaturesByDistance(const vector<Feature> &all_features,
                              int min_distance,
                              vector<Feature> *detected_features) {
  const int min_distance_squared = min_distance * min_distance;

  // Use priority queue to sort the features by their score.
  //
  // Do this on copy of the input features to prevent possible
  // distortion in callee function behavior.
  std::priority_queue<Feature,
                      std::vector<Feature>,
                      FeatureComparison> priority_features;

  for (int i = 0; i < all_features.size(); i++) {
    priority_features.push(all_features.at(i));
  }

  while (!priority_features.empty()) {
    bool ok = true;
    Feature a = priority_features.top();

    for (int i = 0; i < detected_features->size(); i++) {
      Feature &b = detected_features->at(i);
      if (Square(a.x - b.x) + Square(a.y - b.y) < min_distance_squared) {
        ok = false;
        break;
      }
    }

    if (ok) {
      detected_features->push_back(a);
    }

    priority_features.pop();
  }
}

void DetectFAST(const FloatImage &grayscale_image,
                const DetectOptions &options,
                vector<Feature> *detected_features) {
#ifndef LIBMV_NO_FAST_DETECTOR
  const int min_distance = options.min_distance;
  const int min_trackness = options.fast_min_trackness;
  const int margin = options.margin;
  const int width = grayscale_image.Width() - 2 * margin;
  const int height = grayscale_image.Width() - 2 * margin;
  const int stride = grayscale_image.Width();

  scoped_array<unsigned char> byte_image(FloatImageToUCharArray(grayscale_image));
  const int byte_image_offset = margin * stride + margin;

  // TODO(MatthiasF): Support targetting a feature count (binary search trackness)
  int num_features;
  xy *all = fast9_detect(byte_image.get() + byte_image_offset,
                         width,
                         height,
                         stride,
                         min_trackness,
                         &num_features);
  if (num_features == 0) {
    free(all);
    return;
  }
  int *scores = fast9_score(byte_image.get() + byte_image_offset,
                            stride,
                            all,
                            num_features,
                            min_trackness);
  // TODO(MatthiasF): merge with close feature suppression
  xy *nonmax = nonmax_suppression(all, scores, num_features, &num_features);
  free(all);
  // Remove too close features
  // TODO(MatthiasF): A resolution independent parameter would be better than
  // distance e.g. a coefficient going from 0 (no minimal distance) to 1
  // (optimal circle packing)
  // FIXME(MatthiasF): this method will not necessarily give all maximum markers
  if (num_features) {
    vector<Feature> all_features;
    for (int i = 0; i < num_features; ++i) {
      Feature new_feature = {(float)nonmax[i].x + margin,
                             (float)nonmax[i].y + margin,
                             (float)scores[i],
                             0};
      all_features.push_back(new_feature);
    }
    FilterFeaturesByDistance(all_features, min_distance, detected_features);
  }
  free(scores);
  free(nonmax);
#else
  (void) grayscale_image;  // Ignored.
  (void) options;  // Ignored.
  (void) detected_features;  // Ignored.
  LOG(FATAL) << "FAST detector is disabled in this build.";
#endif
}

#ifdef __SSE2__
static unsigned int SAD(const ubyte* imageA, const ubyte* imageB,
                        int strideA, int strideB) {
  __m128i a = _mm_setzero_si128();
  for (int i = 0; i < 16; i++) {
    a = _mm_adds_epu16(a,
            _mm_sad_epu8(_mm_loadu_si128((__m128i*)(imageA+i*strideA)),
                         _mm_loadu_si128((__m128i*)(imageB+i*strideB))));
  }
  return _mm_extract_epi16(a, 0) + _mm_extract_epi16(a, 4);
}
#else
static unsigned int SAD(const ubyte* imageA, const ubyte* imageB,
                        int strideA, int strideB) {
  unsigned int sad = 0;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      sad += abs((int)imageA[i*strideA+j] - imageB[i*strideB+j]);
    }
  }
  return sad;
}
#endif

void DetectMORAVEC(const FloatImage &grayscale_image,
                   const DetectOptions &options,
                   vector<Feature> *detected_features) {
  const int distance = options.min_distance;
  const int margin = options.margin;
  const unsigned char *pattern = options.moravec_pattern;
  const int count = options.moravec_max_count;
  const int width = grayscale_image.Width() - 2 * margin;
  const int height = grayscale_image.Width() - 2 * margin;
  const int stride = grayscale_image.Width();

  scoped_array<unsigned char> byte_image(FloatImageToUCharArray(grayscale_image));

  unsigned short histogram[256];
  memset(histogram, 0, sizeof(histogram));
  scoped_array<ubyte> scores(new ubyte[width*height]);
  memset(scores.get(), 0, width*height);
  const int r = 1;  // radius for self similarity comparison
  for (int y = distance; y < height-distance; y++) {
    for (int x = distance; x < width-distance; x++) {
      const ubyte* s = &byte_image[y*stride+x];
      int score =  // low self-similarity with overlapping patterns
                   // OPTI: load pattern once
          SAD(s, s-r*stride-r, stride, stride)+SAD(s, s-r*stride, stride, stride)+SAD(s, s-r*stride+r, stride, stride)+
          SAD(s, s         -r, stride, stride)+                                   SAD(s, s         +r, stride, stride)+
          SAD(s, s+r*stride-r, stride, stride)+SAD(s, s+r*stride, stride, stride)+SAD(s, s+r*stride+r, stride, stride);
      score /= 256;  // normalize
      if (pattern)  // find only features similar to pattern
        score -= SAD(s, pattern, stride, 16);
      if (score <= 16) continue;  // filter very self-similar features
      score -= 16;  // translate to score/histogram values
      if (score>255) score=255;  // clip
      ubyte* c = &scores[y*width+x];
      for (int i = -distance; i < 0; i++) {
        for (int j = -distance; j < distance; j++) {
          int s = c[i*width+j];
          if (s == 0) continue;
          if (s >= score) goto nonmax;
          c[i*width+j] = 0;
          histogram[s]--;
        }
      }
      for (int i = 0, j = -distance; j < 0; j++) {
        int s = c[i*width+j];
        if (s == 0) continue;
        if (s >= score) goto nonmax;
        c[i*width+j] = 0;
        histogram[s]--;
      }
      c[0] = score, histogram[score]++;
      nonmax:
      { }  // Do nothing.
    }
  }
  int min = 255, total = 0;
  for (; min > 0; min--) {
    int h = histogram[min];
    if (total+h > count) break;
    total += h;
  }
  for (int y = 16; y < height-16; y++) {
    for (int x = 16; x < width-16; x++) {
      int s = scores[y*width+x];
      Feature f = { (float)x+8.0f, (float)y+8.0f, (float)s, 16 };
      if (s > min) {
        detected_features->push_back(f);
      }
    }
  }
}

void DetectHarris(const FloatImage &grayscale_image,
                  const DetectOptions &options,
                  vector<Feature> *detected_features) {
  const double alpha = 0.06;
  const double sigma = 0.9;

  const int min_distance = options.min_distance;
  const int margin = options.margin;
  const double threshold = options.harris_threshold;

  FloatImage gradient_x, gradient_y;
  ImageDerivatives(grayscale_image, sigma, &gradient_x, &gradient_y);

  FloatImage gradient_xx, gradient_yy, gradient_xy;
  MultiplyElements(gradient_x, gradient_x, &gradient_xx);
  MultiplyElements(gradient_y, gradient_y, &gradient_yy);
  MultiplyElements(gradient_x, gradient_y, &gradient_xy);

  FloatImage gradient_xx_blurred,
             gradient_yy_blurred,
             gradient_xy_blurred;
  ConvolveGaussian(gradient_xx, sigma, &gradient_xx_blurred);
  ConvolveGaussian(gradient_yy, sigma, &gradient_yy_blurred);
  ConvolveGaussian(gradient_xy, sigma, &gradient_xy_blurred);

  vector<Feature> all_features;
  for (int y = margin; y < gradient_xx_blurred.Height() - margin; ++y) {
    for (int x = margin; x < gradient_xx_blurred.Width() - margin; ++x) {
      // Construct matrix
      //
      //  A = [ Ix^2  Ix*Iy ]
      //      [ Ix*Iy Iy^2  ]
      Mat2 A;
      A(0, 0) = gradient_xx_blurred(y, x);
      A(1, 1) = gradient_yy_blurred(y, x);
      A(0, 1) = A(1, 0) = gradient_xy_blurred(y, x);

      double detA = A.determinant();
      double traceA = A.trace();
      double harris_function = detA - alpha * traceA * traceA;
      if (harris_function > threshold) {
        Feature new_feature = {(float)x, (float)y, (float)harris_function, 0.0f};
        all_features.push_back(new_feature);
      }
    }
  }

  FilterFeaturesByDistance(all_features, min_distance, detected_features);
}

}  // namespace

DetectOptions::DetectOptions()
  : type(DetectOptions::HARRIS),
    margin(0),
    min_distance(120),
    fast_min_trackness(128),
    moravec_max_count(0),
    moravec_pattern(NULL),
    harris_threshold(0.0) {}

void Detect(const FloatImage &image,
            const DetectOptions &options,
            vector<Feature> *detected_features) {
  // Currently all the detectors requires image to be grayscale.
  // Do it here to avoid code duplication.
  FloatImage grayscale_image;
  if (image.Depth() != 1) {
    Rgb2Gray(image, &grayscale_image);
  } else {
    // TODO(sergey): Find a way to avoid such image duplication/
    grayscale_image = image;
  }

  if (options.type == DetectOptions::FAST) {
    DetectFAST(grayscale_image, options, detected_features);
  } else if (options.type == DetectOptions::MORAVEC) {
    DetectMORAVEC(grayscale_image, options, detected_features);
  } else if (options.type == DetectOptions::HARRIS) {
    DetectHarris(grayscale_image, options, detected_features);
  } else {
    LOG(FATAL) << "Unknown detector has been passed to featur detection";
  }
}

}  // namespace libmv

/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "intern/detector.h"
#include "intern/image.h"
#include "intern/utildefines.h"
#include "libmv/simple_pipeline/detect.h"

using libmv::Detect;
using libmv::DetectOptions;
using libmv::Feature;
using libmv::FloatImage;

struct libmv_Features {
  int count;
  Feature* features;
};

namespace {

libmv_Features *libmv_featuresFromVector(
    const libmv::vector<Feature>& features) {
  libmv_Features* libmv_features = LIBMV_STRUCT_NEW(libmv_Features, 1);
  int count = features.size();
  if (count) {
    libmv_features->features = LIBMV_STRUCT_NEW(Feature, count);
    for (int i = 0; i < count; i++) {
      libmv_features->features[i] = features.at(i);
    }
  } else {
    libmv_features->features = NULL;
  }
  libmv_features->count = count;
  return libmv_features;
}

void libmv_convertDetectorOptions(libmv_DetectOptions *options,
                                  DetectOptions *detector_options) {
  switch (options->detector) {
#define LIBMV_CONVERT(the_detector) \
  case LIBMV_DETECTOR_ ## the_detector:                 \
    detector_options->type = DetectOptions::the_detector; \
    break;
    LIBMV_CONVERT(FAST)
    LIBMV_CONVERT(MORAVEC)
    LIBMV_CONVERT(HARRIS)
#undef LIBMV_CONVERT
  }
  detector_options->margin = options->margin;
  detector_options->min_distance = options->min_distance;
  detector_options->fast_min_trackness = options->fast_min_trackness;
  detector_options->moravec_max_count = options->moravec_max_count;
  detector_options->moravec_pattern = options->moravec_pattern;
  detector_options->harris_threshold = options->harris_threshold;
}

}  // namespace

libmv_Features *libmv_detectFeaturesByte(const unsigned char* image_buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         libmv_DetectOptions* options) {
  // Prepare the image.
  FloatImage image;
  libmv_byteBufferToFloatImage(image_buffer, width, height, channels, &image);

  // Configure detector.
  DetectOptions detector_options;
  libmv_convertDetectorOptions(options, &detector_options);

  // Run the detector.
  libmv::vector<Feature> detected_features;
  Detect(image, detector_options, &detected_features);

  // Convert result to C-API.
  libmv_Features* result = libmv_featuresFromVector(detected_features);
  return result;
}

libmv_Features* libmv_detectFeaturesFloat(const float* image_buffer,
                                          int width,
                                          int height,
                                          int channels,
                                          libmv_DetectOptions* options) {
  // Prepare the image.
  FloatImage image;
  libmv_floatBufferToFloatImage(image_buffer, width, height, channels, &image);

  // Configure detector.
  DetectOptions detector_options;
  libmv_convertDetectorOptions(options, &detector_options);

  // Run the detector.
  libmv::vector<Feature> detected_features;
  Detect(image, detector_options, &detected_features);

  // Convert result to C-API.
  libmv_Features* result = libmv_featuresFromVector(detected_features);
  return result;
}

void libmv_featuresDestroy(libmv_Features* libmv_features) {
  if (libmv_features->features) {
    LIBMV_STRUCT_DELETE(libmv_features->features);
  }
  LIBMV_STRUCT_DELETE(libmv_features);
}

int libmv_countFeatures(const libmv_Features* libmv_features) {
  return libmv_features->count;
}

void libmv_getFeature(const libmv_Features* libmv_features,
                      int number,
                      double* x,
                      double* y,
                      double* score,
                      double* size) {
  Feature &feature = libmv_features->features[number];
  *x = feature.x;
  *y = feature.y;
  *score = feature.score;
  *size = feature.size;
}

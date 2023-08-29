/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_DETECTOR_H_
#define LIBMV_C_API_DETECTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_Features libmv_Features;

enum {
  LIBMV_DETECTOR_FAST,
  LIBMV_DETECTOR_MORAVEC,
  LIBMV_DETECTOR_HARRIS,
};

typedef struct libmv_DetectOptions {
  int detector;
  int margin;
  int min_distance;
  int fast_min_trackness;
  int moravec_max_count;
  unsigned char* moravec_pattern;
  double harris_threshold;
} libmv_DetectOptions;

libmv_Features* libmv_detectFeaturesByte(const unsigned char* image_buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         libmv_DetectOptions* options);

libmv_Features* libmv_detectFeaturesFloat(const float* image_buffer,
                                          int width,
                                          int height,
                                          int channels,
                                          libmv_DetectOptions* options);

void libmv_featuresDestroy(libmv_Features* libmv_features);
int libmv_countFeatures(const libmv_Features* libmv_features);
void libmv_getFeature(const libmv_Features* libmv_features,
                      int number,
                      double* x,
                      double* y,
                      double* score,
                      double* size);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_DETECTOR_H_

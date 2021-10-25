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
  unsigned char *moravec_pattern;
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

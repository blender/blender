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

#ifndef LIBMV_C_API_TRACK_REGION_H_
#define LIBMV_C_API_TRACK_REGION_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_TrackRegionOptions {
  int motion_model;
  int num_iterations;
  int use_brute;
  int use_normalization;
  double minimum_correlation;
  double sigma;
  float *image1_mask;
} libmv_TrackRegionOptions;

typedef struct libmv_TrackRegionResult {
  int termination;
  const char* termination_reason;
  double correlation;
} libmv_TrackRegionResult;

#ifdef __cplusplus
namespace libmv {
  struct TrackRegionOptions;
  struct TrackRegionResult;
}
void libmv_configureTrackRegionOptions(
    const libmv_TrackRegionOptions& options,
    libmv::TrackRegionOptions* track_region_options);

void libmv_regionTrackergetResult(
    const libmv::TrackRegionResult& track_region_result,
    libmv_TrackRegionResult* result);
#endif

int libmv_trackRegion(const libmv_TrackRegionOptions* options,
                      const float* image1,
                      int image1_width,
                      int image1_height,
                      const float* image2,
                      int image2_width,
                      int image2_height,
                      const double* x1,
                      const double* y1,
                      libmv_TrackRegionResult* result,
                      double* x2,
                      double* y2);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_PLANAR_TRACKER_H_

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_TRACK_REGION_H_
#define LIBMV_C_API_TRACK_REGION_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum libmv_TrackRegionDirection {
  LIBMV_TRACK_REGION_FORWARD,
  LIBMV_TRACK_REGION_BACKWARD,
} libmv_TrackRegionDirection;

typedef struct libmv_TrackRegionOptions {
  libmv_TrackRegionDirection direction;
  int motion_model;
  int num_iterations;
  int use_brute;
  int use_normalization;
  double minimum_correlation;
  double sigma;
  float* image1_mask;
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
}  // namespace libmv
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

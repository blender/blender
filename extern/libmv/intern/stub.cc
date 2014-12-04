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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "libmv-capi.h"

#include <cstdlib>
#include <cstring>

/* ************ Logging ************ */

void libmv_initLogging(const char * /*argv0*/) {
}

void libmv_startDebugLogging(void) {
}

void libmv_setLoggingVerbosity(int /*verbosity*/) {
}

/* ************ Planar tracker ************ */

/* TrackRegion (new planar tracker) */
int libmv_trackRegion(const libmv_TrackRegionOptions * /*options*/,
                      const float * /*image1*/,
                      int /*image1_width*/,
                      int /*image1_height*/,
                      const float * /*image2*/,
                      int /*image2_width*/,
                      int /*image2_height*/,
                      const double *x1,
                      const double *y1,
                      libmv_TrackRegionResult *result,
                      double *x2,
                      double *y2) {
  /* Convert to doubles for the libmv api. The four corners and the center. */
  for (int i = 0; i < 5; ++i) {
    x2[i] = x1[i];
    y2[i] = y1[i];
  }

  result->termination = -1;
  result->termination_reason = "Built without libmv support";
  result->correlation = 0.0;

  return false;
}

void libmv_samplePlanarPatchFloat(const float * /*image*/,
                                  int /*width*/,
                                  int /*height*/,
                                  int /*channels*/,
                                  const double * /*xs*/,
                                  const double * /*ys*/,
                                  int /*num_samples_x*/,
                                  int /*num_samples_y*/,
                                  const float * /*mask*/,
                                  float * /*patch*/,
                                  double * /*warped_position_x*/,
                                  double * /*warped_position_y*/) {
  /* TODO(sergey): implement */
}

void libmv_samplePlanarPatchByte(const unsigned char * /*image*/,
                                 int /*width*/,
                                 int /*height*/,
                                 int /*channels*/,
                                 const double * /*xs*/,
                                 const double * /*ys*/,
                                 int /*num_samples_x*/, int /*num_samples_y*/,
                                 const float * /*mask*/,
                                 unsigned char * /*patch*/,
                                 double * /*warped_position_x*/,
                                 double * /*warped_position_y*/) {
  /* TODO(sergey): implement */
}

void libmv_floatImageDestroy(libmv_FloatImage* /*image*/)
{
}

/* ************ Tracks ************ */

libmv_Tracks *libmv_tracksNew(void) {
  return NULL;
}

void libmv_tracksInsert(libmv_Tracks * /*libmv_tracks*/,
                        int /*image*/,
                        int /*track*/,
                        double /*x*/,
                        double /*y*/,
                        double /*weight*/) {
}

void libmv_tracksDestroy(libmv_Tracks * /*libmv_tracks*/) {
}

/* ************ Reconstruction solver ************ */

libmv_Reconstruction *libmv_solveReconstruction(
    const libmv_Tracks * /*libmv_tracks*/,
    const libmv_CameraIntrinsicsOptions * /*libmv_camera_intrinsics_options*/,
    libmv_ReconstructionOptions * /*libmv_reconstruction_options*/,
    reconstruct_progress_update_cb /*progress_update_callback*/,
    void * /*callback_customdata*/) {
  return NULL;
}

libmv_Reconstruction *libmv_solveModal(
    const libmv_Tracks * /*libmv_tracks*/,
    const libmv_CameraIntrinsicsOptions * /*libmv_camera_intrinsics_options*/,
    const libmv_ReconstructionOptions * /*libmv_reconstruction_options*/,
    reconstruct_progress_update_cb /*progress_update_callback*/,
    void * /*callback_customdata*/) {
  return NULL;
}

int libmv_reconstructionIsValid(libmv_Reconstruction * /*libmv_reconstruction*/) {
  return 0;
}

int libmv_reprojectionPointForTrack(
    const libmv_Reconstruction * /*libmv_reconstruction*/,
    int /*track*/,
    double /*pos*/[3]) {
  return 0;
}

double libmv_reprojectionErrorForTrack(
    const libmv_Reconstruction * /*libmv_reconstruction*/,
    int /*track*/) {
  return 0.0;
}

double libmv_reprojectionErrorForImage(
    const libmv_Reconstruction * /*libmv_reconstruction*/,
    int /*image*/) {
  return 0.0;
}

int libmv_reprojectionCameraForImage(
    const libmv_Reconstruction * /*libmv_reconstruction*/,
    int /*image*/,
    double /*mat*/[4][4]) {
  return 0;
}

double libmv_reprojectionError(
    const libmv_Reconstruction * /*libmv_reconstruction*/) {
  return 0.0;
}

void libmv_reconstructionDestroy(
    struct libmv_Reconstruction * /*libmv_reconstruction*/) {
}

/* ************ Feature detector ************ */

libmv_Features *libmv_detectFeaturesByte(const unsigned char */*image_buffer*/,
                                         int /*width*/,
                                         int /*height*/,
                                         int /*channels*/,
                                         libmv_DetectOptions */*options*/) {
  return NULL;
}

struct libmv_Features *libmv_detectFeaturesFloat(
    const float */*image_buffer*/,
    int /*width*/,
    int /*height*/,
    int /*channels*/,
    libmv_DetectOptions */*options*/) {
  return NULL;
}

int libmv_countFeatures(const libmv_Features * /*libmv_features*/) {
  return 0;
}

void libmv_getFeature(const libmv_Features * /*libmv_features*/,
                      int /*number*/,
                      double *x,
                      double *y,
                      double *score,
                      double *size) {
  *x = 0.0;
  *y = 0.0;
  *score = 0.0;
  *size = 0.0;
}

void libmv_featuresDestroy(struct libmv_Features * /*libmv_features*/) {
}

/* ************ Camera intrinsics ************ */

libmv_CameraIntrinsics *libmv_reconstructionExtractIntrinsics(
    libmv_Reconstruction * /*libmv_reconstruction*/) {
  return NULL;
}

libmv_CameraIntrinsics *libmv_cameraIntrinsicsNew(
    const libmv_CameraIntrinsicsOptions * /*libmv_camera_intrinsics_options*/) {
  return NULL;
}

libmv_CameraIntrinsics *libmv_cameraIntrinsicsCopy(
    const libmv_CameraIntrinsics * /*libmvIntrinsics*/) {
  return NULL;
}

void libmv_cameraIntrinsicsDestroy(
    libmv_CameraIntrinsics * /*libmvIntrinsics*/) {
}

void libmv_cameraIntrinsicsUpdate(
    const libmv_CameraIntrinsicsOptions * /*libmv_camera_intrinsics_options*/,
    libmv_CameraIntrinsics * /*libmv_intrinsics*/) {
}

void libmv_cameraIntrinsicsSetThreads(
    libmv_CameraIntrinsics * /*libmv_intrinsics*/,
    int /*threads*/) {
}

void libmv_cameraIntrinsicsExtractOptions(
    const libmv_CameraIntrinsics */*libmv_intrinsics*/,
    libmv_CameraIntrinsicsOptions *camera_intrinsics_options) {
  memset(camera_intrinsics_options, 0, sizeof(libmv_CameraIntrinsicsOptions));
  camera_intrinsics_options->focal_length = 1.0;
}

void libmv_cameraIntrinsicsUndistortByte(
    const libmv_CameraIntrinsics * /*libmv_intrinsics*/,
    const unsigned char *source_image,
    int width, int height,
    float overscan, int channels,
    unsigned char *destination_image) {
  memcpy(destination_image, source_image,
         channels * width * height * sizeof(unsigned char));
}

void libmv_cameraIntrinsicsUndistortFloat(
    const libmv_CameraIntrinsics* /*libmv_intrinsics*/,
    const float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image) {
  memcpy(destination_image, source_image,
         channels * width * height * sizeof(float));
}

void libmv_cameraIntrinsicsDistortByte(
    const struct libmv_CameraIntrinsics* /*libmv_intrinsics*/,
    const unsigned char *source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char *destination_image) {
  memcpy(destination_image, source_image,
         channels * width * height * sizeof(unsigned char));
}

void libmv_cameraIntrinsicsDistortFloat(
    const libmv_CameraIntrinsics* /*libmv_intrinsics*/,
    float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image) {
  memcpy(destination_image, source_image,
         channels * width * height * sizeof(float));
}

/* ************ utils ************ */

void libmv_cameraIntrinsicsApply(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    double x,
    double y,
    double* x1,
    double* y1) {
  double focal_length = libmv_camera_intrinsics_options->focal_length;
  double principal_x = libmv_camera_intrinsics_options->principal_point_x;
  double principal_y = libmv_camera_intrinsics_options->principal_point_y;
  *x1 = x * focal_length + principal_x;
  *y1 = y * focal_length + principal_y;
}

void libmv_cameraIntrinsicsInvert(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    double x,
    double y,
    double* x1,
    double* y1) {
  double focal_length = libmv_camera_intrinsics_options->focal_length;
  double principal_x = libmv_camera_intrinsics_options->principal_point_x;
  double principal_y = libmv_camera_intrinsics_options->principal_point_y;
  *x1 = (x - principal_x) / focal_length;
  *y1 = (y - principal_y) / focal_length;
}

void libmv_homography2DFromCorrespondencesEuc(/* const */ double (*x1)[2],
                                              /* const */ double (*x2)[2],
                                              int num_points,
                                              double H[3][3]) {
  memset(H, 0, sizeof(double[3][3]));
  H[0][0] = 1.0f;
  H[1][1] = 1.0f;
  H[2][2] = 1.0f;
}

/* ************ autotrack ************ */

libmv_AutoTrack* libmv_autoTrackNew(libmv_FrameAccessor* /*frame_accessor*/)
{
  return NULL;
}

void libmv_autoTrackDestroy(libmv_AutoTrack* /*libmv_autotrack*/)
{
}

void libmv_autoTrackSetOptions(libmv_AutoTrack* /*libmv_autotrack*/,
                               const libmv_AutoTrackOptions* /*options*/)
{
}

int libmv_autoTrackMarker(libmv_AutoTrack* /*libmv_autotrack*/,
                          const libmv_TrackRegionOptions* /*libmv_options*/,
                          libmv_Marker */*libmv_tracker_marker*/,
                          libmv_TrackRegionResult* /*libmv_result*/)
{
  return 0;
}

void libmv_autoTrackAddMarker(libmv_AutoTrack* /*libmv_autotrack*/,
                              const libmv_Marker* /*libmv_marker*/)
{
}

int libmv_autoTrackGetMarker(libmv_AutoTrack* /*libmv_autotrack*/,
                             int /*clip*/,
                             int /*frame*/,
                             int /*track*/,
                             libmv_Marker* /*libmv_marker*/)
{
  return 0;
}

/* ************ frame accessor ************ */

libmv_FrameAccessor* libmv_FrameAccessorNew(
    libmv_FrameAccessorUserData* /*user_data**/,
    libmv_GetImageCallback /*get_image_callback*/,
    libmv_ReleaseImageCallback /*release_image_callback*/)
{
  return NULL;
}

void libmv_FrameAccessorDestroy(libmv_FrameAccessor* /*frame_accessor*/)
{
}

int64_t libmv_frameAccessorgetTransformKey(
    const libmv_FrameTransform */*transform*/)
{
  return 0;
}

void libmv_frameAccessorgetTransformRun(const libmv_FrameTransform* /*transform*/,
                                        const libmv_FloatImage* /*input_image*/,
                                        libmv_FloatImage* /*output_image*/)
{
}


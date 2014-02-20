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

#ifndef LIBMV_C_API_H
#define LIBMV_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct libmv_Tracks;
struct libmv_Reconstruction;
struct libmv_Features;
struct libmv_CameraIntrinsics;

/* Logging */
void libmv_initLogging(const char *argv0);
void libmv_startDebugLogging(void);
void libmv_setLoggingVerbosity(int verbosity);

/* Planar tracker */
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
	const char *termination_reason;
	double correlation;
} libmv_TrackRegionResult;

int libmv_trackRegion(const libmv_TrackRegionOptions *options,
                      const float *image1, int image1_width, int image1_height,
                      const float *image2, int image2_width, int image2_height,
                      const double *x1, const double *y1,
                      libmv_TrackRegionResult *result,
                      double *x2, double *y2);
void libmv_samplePlanarPatch(const float *image,
                             int width, int height,
                             int channels,
                             const double *xs, const double *ys,
                             int num_samples_x, int num_samples_y,
                             const float *mask,
                             float *patch,
                             double *warped_position_x, double *warped_position_y);
void libmv_samplePlanarPatchByte(const unsigned char *image,
                                 int width, int height,
                                 int channels,
                                 const double *xs, const double *ys,
                                 int num_samples_x, int num_samples_y,
                                 const float *mask,
                                 unsigned char *patch,
                                 double *warped_position_x, double *warped_position_y);

/* Tracks */
struct libmv_Tracks *libmv_tracksNew(void);
void libmv_tracksDestroy(struct libmv_Tracks *libmv_tracks);
void libmv_tracksInsert(struct libmv_Tracks *libmv_tracks, int image, int track, double x, double y, double weight);

/* Reconstruction */
#define LIBMV_REFINE_FOCAL_LENGTH          (1 << 0)
#define LIBMV_REFINE_PRINCIPAL_POINT       (1 << 1)
#define LIBMV_REFINE_RADIAL_DISTORTION_K1  (1 << 2)
#define LIBMV_REFINE_RADIAL_DISTORTION_K2  (1 << 4)

enum {
	LIBMV_DISTORTION_MODEL_POLYNOMIAL = 0,
	LIBMV_DISTORTION_MODEL_DIVISION = 1,
};

typedef struct libmv_CameraIntrinsicsOptions {
	/* Common settings of all distortion models. */
	int distortion_model;
	int image_width, image_height;
	double focal_length;
	double principal_point_x, principal_point_y;

	/* Radial distortion model. */
	double polynomial_k1, polynomial_k2, polynomial_k3;
	double polynomial_p1, polynomial_p2;

	/* Division distortion model. */
	double division_k1, division_k2;
} libmv_CameraIntrinsicsOptions;

typedef struct libmv_ReconstructionOptions {
	int select_keyframes;
	int keyframe1, keyframe2;

	int refine_intrinsics;
} libmv_ReconstructionOptions;

typedef void (*reconstruct_progress_update_cb) (void *customdata, double progress, const char *message);

struct libmv_Reconstruction *libmv_solveReconstruction(const struct libmv_Tracks *libmv_tracks,
		const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
		libmv_ReconstructionOptions *libmv_reconstruction_options,
		reconstruct_progress_update_cb progress_update_callback,
		void *callback_customdata);
struct libmv_Reconstruction *libmv_solveModal(const struct libmv_Tracks *libmv_tracks,
		const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
		const libmv_ReconstructionOptions *libmv_reconstruction_options,
		reconstruct_progress_update_cb progress_update_callback,
		void *callback_customdata);
void libmv_reconstructionDestroy(struct libmv_Reconstruction *libmv_reconstruction);
int libmv_reprojectionPointForTrack(const struct libmv_Reconstruction *libmv_reconstruction, int track, double pos[3]);
double libmv_reprojectionErrorForTrack(const struct libmv_Reconstruction *libmv_reconstruction, int track);
double libmv_reprojectionErrorForImage(const struct libmv_Reconstruction *libmv_reconstruction, int image);
int libmv_reprojectionCameraForImage(const struct libmv_Reconstruction *libmv_reconstruction,
                                     int image, double mat[4][4]);
double libmv_reprojectionError(const struct libmv_Reconstruction *libmv_reconstruction);
struct libmv_CameraIntrinsics *libmv_reconstructionExtractIntrinsics(struct libmv_Reconstruction *libmv_Reconstruction);

/* Feature detector */
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

struct libmv_Features *libmv_detectFeaturesByte(const unsigned char *image_buffer,
                                                int width, int height, int channels,
                                                libmv_DetectOptions *options);
struct libmv_Features *libmv_detectFeaturesFloat(const float *image_buffer,
                                                 int width, int height, int channels,
                                                 libmv_DetectOptions *options);

void libmv_featuresDestroy(struct libmv_Features *libmv_features);
int libmv_countFeatures(const struct libmv_Features *libmv_features);
void libmv_getFeature(const struct libmv_Features *libmv_features, int number, double *x, double *y, double *score,
                      double *size);

/* Camera intrinsics */
struct libmv_CameraIntrinsics *libmv_cameraIntrinsicsNew(
		const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options);
struct libmv_CameraIntrinsics *libmv_cameraIntrinsicsCopy(const struct libmv_CameraIntrinsics *libmv_intrinsics);
void libmv_cameraIntrinsicsDestroy(struct libmv_CameraIntrinsics *libmv_intrinsics);
void libmv_cameraIntrinsicsUpdate(const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
                                  struct libmv_CameraIntrinsics *libmv_intrinsics);
void libmv_cameraIntrinsicsSetThreads(struct libmv_CameraIntrinsics *libmv_intrinsics, int threads);
void libmv_cameraIntrinsicsExtractOptions(
	const struct libmv_CameraIntrinsics *libmv_intrinsics,
	struct libmv_CameraIntrinsicsOptions *camera_intrinsics_options);
void libmv_cameraIntrinsicsUndistortByte(const struct libmv_CameraIntrinsics *libmv_intrinsics,
                                         unsigned char *src, unsigned char *dst, int width, int height,
                                         float overscan, int channels);
void libmv_cameraIntrinsicsUndistortFloat(const struct libmv_CameraIntrinsics *libmv_intrinsics,
                                          float *src, float *dst, int width, int height,
                                          float overscan, int channels);
void libmv_cameraIntrinsicsDistortByte(const struct libmv_CameraIntrinsics *libmv_intrinsics,
                                       unsigned char *src, unsigned char *dst, int width, int height,
                                       float overscan, int channels);
void libmv_cameraIntrinsicsDistortFloat(const struct libmv_CameraIntrinsics *libmv_intrinsics,
                                        float *src, float *dst, int width, int height,
                                        float overscan, int channels);
void libmv_cameraIntrinsicsApply(const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
                                 double x, double y, double *x1, double *y1);
void libmv_cameraIntrinsicsInvert(const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
                                  double x, double y, double *x1, double *y1);

void libmv_homography2DFromCorrespondencesEuc(double (*x1)[2], double (*x2)[2], int num_points, double H[3][3]);

#ifdef __cplusplus
}
#endif

#endif // LIBMV_C_API_H

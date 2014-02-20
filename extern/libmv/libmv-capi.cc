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

#ifdef WITH_LIBMV

/* define this to generate PNG images with content of search areas
   tracking between which failed */
#undef DUMP_FAILURE

/* define this to generate PNG images with content of search areas
   on every itteration of tracking */
#undef DUMP_ALWAYS

#include "libmv-capi.h"
#include "libmv-util.h"

#include <cassert>

#include "libmv-capi_intern.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/homography.h"
#include "libmv/tracking/track_region.h"
#include "libmv/simple_pipeline/callbacks.h"
#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/initialize_reconstruction.h"
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/detect.h"
#include "libmv/simple_pipeline/pipeline.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"
#include "libmv/simple_pipeline/modal_solver.h"
#include "libmv/simple_pipeline/reconstruction_scale.h"
#include "libmv/simple_pipeline/keyframe_selection.h"

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

using libmv::CameraIntrinsics;
using libmv::DetectOptions;
using libmv::DivisionCameraIntrinsics;
using libmv::EuclideanCamera;
using libmv::EuclideanPoint;
using libmv::EuclideanReconstruction;
using libmv::EuclideanScaleToUnity;
using libmv::Feature;
using libmv::FloatImage;
using libmv::Marker;
using libmv::PolynomialCameraIntrinsics;
using libmv::ProgressUpdateCallback;
using libmv::Tracks;
using libmv::TrackRegionOptions;
using libmv::TrackRegionResult;

using libmv::Detect;
using libmv::EuclideanBundle;
using libmv::EuclideanCompleteReconstruction;
using libmv::EuclideanReconstructTwoFrames;
using libmv::EuclideanReprojectionError;
using libmv::TrackRegion;
using libmv::SamplePlanarPatch;

typedef struct libmv_Tracks libmv_Tracks;
typedef struct libmv_Reconstruction libmv_Reconstruction;
typedef struct libmv_Features libmv_Features;
typedef struct libmv_CameraIntrinsics libmv_CameraIntrinsics;

struct libmv_Reconstruction {
	EuclideanReconstruction reconstruction;

	/* used for per-track average error calculation after reconstruction */
	Tracks tracks;
	CameraIntrinsics *intrinsics;

	double error;
};

struct libmv_Features {
	int count;
	Feature *features;
};

/* ************ Logging ************ */

void libmv_initLogging(const char *argv0)
{
	/* Make it so FATAL messages are always print into console */
	char severity_fatal[32];
	snprintf(severity_fatal, sizeof(severity_fatal), "%d",
	         google::GLOG_FATAL);

	google::InitGoogleLogging(argv0);
	google::SetCommandLineOption("logtostderr", "1");
	google::SetCommandLineOption("v", "0");
	google::SetCommandLineOption("stderrthreshold", severity_fatal);
	google::SetCommandLineOption("minloglevel", severity_fatal);
}

void libmv_startDebugLogging(void)
{
	google::SetCommandLineOption("logtostderr", "1");
	google::SetCommandLineOption("v", "2");
	google::SetCommandLineOption("stderrthreshold", "1");
	google::SetCommandLineOption("minloglevel", "0");
}

void libmv_setLoggingVerbosity(int verbosity)
{
	char val[10];
	snprintf(val, sizeof(val), "%d", verbosity);

	google::SetCommandLineOption("v", val);
}

/* ************ Planar tracker ************ */

/* TrackRegion */
int libmv_trackRegion(const libmv_TrackRegionOptions *options,
                      const float *image1, int image1_width, int image1_height,
                      const float *image2, int image2_width, int image2_height,
                      const double *x1, const double *y1,
                      libmv_TrackRegionResult *result,
                      double *x2, double *y2)
{
	double xx1[5], yy1[5];
	double xx2[5], yy2[5];
	bool tracking_result = false;

	/* Convert to doubles for the libmv api. The four corners and the center. */
	for (int i = 0; i < 5; ++i) {
		xx1[i] = x1[i];
		yy1[i] = y1[i];
		xx2[i] = x2[i];
		yy2[i] = y2[i];
	}

	TrackRegionOptions track_region_options;
	FloatImage image1_mask;

	switch (options->motion_model) {
#define LIBMV_CONVERT(the_model) \
	case TrackRegionOptions::the_model: \
		track_region_options.mode = TrackRegionOptions::the_model; \
		break;
		LIBMV_CONVERT(TRANSLATION)
		LIBMV_CONVERT(TRANSLATION_ROTATION)
		LIBMV_CONVERT(TRANSLATION_SCALE)
		LIBMV_CONVERT(TRANSLATION_ROTATION_SCALE)
		LIBMV_CONVERT(AFFINE)
		LIBMV_CONVERT(HOMOGRAPHY)
#undef LIBMV_CONVERT
	}

	track_region_options.minimum_correlation = options->minimum_correlation;
	track_region_options.max_iterations = options->num_iterations;
	track_region_options.sigma = options->sigma;
	track_region_options.num_extra_points = 1;
	track_region_options.image1_mask = NULL;
	track_region_options.use_brute_initialization = options->use_brute;
	/* TODO(keir): This will make some cases better, but may be a regression until
	 * the motion model is in. Since this is on trunk, enable it for now.
	 *
	 * TODO(sergey): This gives much worse results on mango footage (see 04_2e)
	 * so disabling for now for until proper prediction model is landed.
	 *
	 * The thing is, currently blender sends input coordinates as the guess to
	 * region tracker and in case of fast motion such an early out ruins the track.
	 */
	track_region_options.attempt_refine_before_brute = false;
	track_region_options.use_normalized_intensities = options->use_normalization;

	if (options->image1_mask) {
		libmv_floatBufferToImage(options->image1_mask,
		                         image1_width, image1_height, 1,
		                         &image1_mask);

		track_region_options.image1_mask = &image1_mask;
	}

	/* Convert from raw float buffers to libmv's FloatImage. */
	FloatImage old_patch, new_patch;
	libmv_floatBufferToImage(image1,
	                         image1_width, image1_height, 1,
	                         &old_patch);
	libmv_floatBufferToImage(image2,
	                         image2_width, image2_height, 1,
	                         &new_patch);

	TrackRegionResult track_region_result;
	TrackRegion(old_patch, new_patch,
	            xx1, yy1,
	            track_region_options,
	            xx2, yy2,
	            &track_region_result);

	/* Convert to floats for the blender api. */
	for (int i = 0; i < 5; ++i) {
		x2[i] = xx2[i];
		y2[i] = yy2[i];
	}

	/* TODO(keir): Update the termination string with failure details. */
	if (track_region_result.termination == TrackRegionResult::CONVERGENCE ||
	    track_region_result.termination == TrackRegionResult::NO_CONVERGENCE)
	{
		tracking_result = true;
	}

	/* Debug dump of patches. */
#if defined(DUMP_FAILURE) || defined(DUMP_ALWAYS)
	{
		bool need_dump = !tracking_result;

#  ifdef DUMP_ALWAYS
		need_dump = true;
#  endif

		if (need_dump) {
			libmv_saveImage(old_patch, "old_patch", x1[4], y1[4]);
			libmv_saveImage(new_patch, "new_patch", x2[4], y2[4]);

			if (options->image1_mask) {
				libmv_saveImage(image1_mask, "mask", x2[4], y2[4]);
			}
		}
	}
#endif

	return tracking_result;
}

void libmv_samplePlanarPatch(const float *image,
                             int width, int height, int channels,
                             const double *xs, const double *ys,
                             int num_samples_x, int num_samples_y,
                             const float *mask,
                             float *patch,
                             double *warped_position_x,
                             double *warped_position_y)
{
	FloatImage libmv_image, libmv_patch, libmv_mask;
	FloatImage *libmv_mask_for_sample = NULL;

	libmv_floatBufferToImage(image, width, height, channels, &libmv_image);

	if (mask) {
		libmv_floatBufferToImage(mask, width, height, 1, &libmv_mask);

		libmv_mask_for_sample = &libmv_mask;
	}

	SamplePlanarPatch(libmv_image,
	                  xs, ys,
	                  num_samples_x, num_samples_y,
	                  libmv_mask_for_sample,
	                  &libmv_patch,
	                  warped_position_x,
	                  warped_position_y);

	libmv_imageToFloatBuffer(libmv_patch, patch);
}

 void libmv_samplePlanarPatchByte(const unsigned char *image,
                                  int width, int height, int channels,
                                  const double *xs, const double *ys,
                                  int num_samples_x, int num_samples_y,
                                  const float *mask,
                                  unsigned char *patch,
                                  double *warped_position_x, double *warped_position_y)
{
	libmv::FloatImage libmv_image, libmv_patch, libmv_mask;
	libmv::FloatImage *libmv_mask_for_sample = NULL;

	libmv_byteBufferToImage(image, width, height, channels, &libmv_image);

	if (mask) {
		libmv_floatBufferToImage(mask, width, height, 1, &libmv_mask);

		libmv_mask_for_sample = &libmv_mask;
	}

	libmv::SamplePlanarPatch(libmv_image, xs, ys,
	                         num_samples_x, num_samples_y,
	                         libmv_mask_for_sample,
	                         &libmv_patch,
	                         warped_position_x,
	                         warped_position_y);

	libmv_imageToByteBuffer(libmv_patch, patch);
}

/* ************ Tracks ************ */

libmv_Tracks *libmv_tracksNew(void)
{
	Tracks *libmv_tracks = LIBMV_OBJECT_NEW(Tracks);

	return (libmv_Tracks *) libmv_tracks;
}

void libmv_tracksDestroy(libmv_Tracks *libmv_tracks)
{
	LIBMV_OBJECT_DELETE(libmv_tracks, Tracks);
}

void libmv_tracksInsert(libmv_Tracks *libmv_tracks,
                        int image, int track,
                        double x, double y,
                        double weight)
{
	((Tracks *) libmv_tracks)->Insert(image, track, x, y, weight);
}

/* ************ Reconstruction ************ */

namespace {

class ReconstructUpdateCallback : public ProgressUpdateCallback {
public:
	ReconstructUpdateCallback(
		reconstruct_progress_update_cb progress_update_callback,
		void *callback_customdata)
	{
		progress_update_callback_ = progress_update_callback;
		callback_customdata_ = callback_customdata;
	}

	void invoke(double progress, const char *message)
	{
		if (progress_update_callback_) {
			progress_update_callback_(callback_customdata_, progress, message);
		}
	}
protected:
	reconstruct_progress_update_cb progress_update_callback_;
	void *callback_customdata_;
};

void libmv_solveRefineIntrinsics(
	const Tracks &tracks,
	const int refine_intrinsics,
	const int bundle_constraints,
	reconstruct_progress_update_cb progress_update_callback,
	void *callback_customdata,
	EuclideanReconstruction *reconstruction,
	CameraIntrinsics *intrinsics)
{
	/* only a few combinations are supported but trust the caller */
	int bundle_intrinsics = 0;

	if (refine_intrinsics & LIBMV_REFINE_FOCAL_LENGTH) {
		bundle_intrinsics |= libmv::BUNDLE_FOCAL_LENGTH;
	}
	if (refine_intrinsics & LIBMV_REFINE_PRINCIPAL_POINT) {
		bundle_intrinsics |= libmv::BUNDLE_PRINCIPAL_POINT;
	}
	if (refine_intrinsics & LIBMV_REFINE_RADIAL_DISTORTION_K1) {
		bundle_intrinsics |= libmv::BUNDLE_RADIAL_K1;
	}
	if (refine_intrinsics & LIBMV_REFINE_RADIAL_DISTORTION_K2) {
		bundle_intrinsics |= libmv::BUNDLE_RADIAL_K2;
	}

	progress_update_callback(callback_customdata, 1.0, "Refining solution");

	EuclideanBundleCommonIntrinsics(tracks,
	                                bundle_intrinsics,
	                                bundle_constraints,
	                                reconstruction,
	                                intrinsics);
}

void finishReconstruction(
	const Tracks &tracks,
	const CameraIntrinsics &camera_intrinsics,
	libmv_Reconstruction *libmv_reconstruction,
	reconstruct_progress_update_cb progress_update_callback,
	void *callback_customdata)
{
	EuclideanReconstruction &reconstruction =
		libmv_reconstruction->reconstruction;

	/* reprojection error calculation */
	progress_update_callback(callback_customdata, 1.0, "Finishing solution");
	libmv_reconstruction->tracks = tracks;
	libmv_reconstruction->error = EuclideanReprojectionError(tracks,
		                                                     reconstruction,
		                                                     camera_intrinsics);
}

bool selectTwoKeyframesBasedOnGRICAndVariance(
	Tracks &tracks,
	Tracks &normalized_tracks,
	CameraIntrinsics &camera_intrinsics,
	int &keyframe1,
	int &keyframe2)
{
	libmv::vector<int> keyframes;

	/* Get list of all keyframe candidates first. */
	SelectKeyframesBasedOnGRICAndVariance(normalized_tracks,
	                                      camera_intrinsics,
	                                      keyframes);

	if (keyframes.size() < 2) {
		LG << "Not enough keyframes detected by GRIC";
		return false;
	}
	else if (keyframes.size() == 2) {
		keyframe1 = keyframes[0];
		keyframe2 = keyframes[1];
		return true;
	}

	/* Now choose two keyframes with minimal reprojection error after initial
	 * reconstruction choose keyframes with the least reprojection error after
	 * solving from two candidate keyframes.
	 *
	 * In fact, currently libmv returns single pair only, so this code will
	 * not actually run. But in the future this could change, so let's stay
	 * prepared.
	 */
	int previous_keyframe = keyframes[0];
	double best_error = std::numeric_limits<double>::max();
	for (int i = 1; i < keyframes.size(); i++) {
		EuclideanReconstruction reconstruction;
		int current_keyframe = keyframes[i];

		libmv::vector<Marker> keyframe_markers =
			normalized_tracks.MarkersForTracksInBothImages(previous_keyframe,
			                                               current_keyframe);

		Tracks keyframe_tracks(keyframe_markers);

		/* get a solution from two keyframes only */
		EuclideanReconstructTwoFrames(keyframe_markers, &reconstruction);
		EuclideanBundle(keyframe_tracks, &reconstruction);
		EuclideanCompleteReconstruction(keyframe_tracks,
		                                &reconstruction,
		                                NULL);

		double current_error = EuclideanReprojectionError(tracks,
			                                              reconstruction,
			                                              camera_intrinsics);

		LG << "Error between " << previous_keyframe
		   << " and " << current_keyframe
		   << ": " << current_error;

		if (current_error < best_error) {
			best_error = current_error;
			keyframe1 = previous_keyframe;
			keyframe2 = current_keyframe;
		}

		previous_keyframe = current_keyframe;
	}

	return true;
}

}  // namespace

libmv_Reconstruction *libmv_solveReconstruction(
	const libmv_Tracks *libmv_tracks,
	const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
	libmv_ReconstructionOptions *libmv_reconstruction_options,
	reconstruct_progress_update_cb progress_update_callback,
	void *callback_customdata)
{
	libmv_Reconstruction *libmv_reconstruction =
		LIBMV_OBJECT_NEW(libmv_Reconstruction);

	Tracks &tracks = *((Tracks *) libmv_tracks);
	EuclideanReconstruction &reconstruction =
		libmv_reconstruction->reconstruction;

	ReconstructUpdateCallback update_callback =
		ReconstructUpdateCallback(progress_update_callback,
		                          callback_customdata);

	/* Retrieve reconstruction options from C-API to libmv API */
	CameraIntrinsics *camera_intrinsics;
	camera_intrinsics = libmv_reconstruction->intrinsics =
		libmv_cameraIntrinsicsCreateFromOptions(
			libmv_camera_intrinsics_options);

	/* Invert the camera intrinsics */
	Tracks normalized_tracks;
	libmv_getNormalizedTracks(tracks, *camera_intrinsics, &normalized_tracks);

	/* keyframe selection */
	int keyframe1 = libmv_reconstruction_options->keyframe1,
	    keyframe2 = libmv_reconstruction_options->keyframe2;

	if (libmv_reconstruction_options->select_keyframes) {
		LG << "Using automatic keyframe selection";

		update_callback.invoke(0, "Selecting keyframes");

		selectTwoKeyframesBasedOnGRICAndVariance(tracks,
		                                         normalized_tracks,
		                                         *camera_intrinsics,
		                                         keyframe1,
		                                         keyframe2);

		/* so keyframes in the interface would be updated */
		libmv_reconstruction_options->keyframe1 = keyframe1;
		libmv_reconstruction_options->keyframe2 = keyframe2;
	}

	/* actual reconstruction */
	LG << "frames to init from: " << keyframe1 << " " << keyframe2;

	libmv::vector<Marker> keyframe_markers =
		normalized_tracks.MarkersForTracksInBothImages(keyframe1, keyframe2);

	LG << "number of markers for init: " << keyframe_markers.size();

	update_callback.invoke(0, "Initial reconstruction");

	EuclideanReconstructTwoFrames(keyframe_markers, &reconstruction);
	EuclideanBundle(normalized_tracks, &reconstruction);
	EuclideanCompleteReconstruction(normalized_tracks,
	                                &reconstruction,
	                                &update_callback);

	/* refinement */
	if (libmv_reconstruction_options->refine_intrinsics) {
		libmv_solveRefineIntrinsics(
			tracks,
			libmv_reconstruction_options->refine_intrinsics,
			libmv::BUNDLE_NO_CONSTRAINTS,
			progress_update_callback,
			callback_customdata,
			&reconstruction,
			camera_intrinsics);
	}

	/* set reconstruction scale to unity */
	EuclideanScaleToUnity(&reconstruction);

	/* finish reconstruction */
	finishReconstruction(tracks,
	                     *camera_intrinsics,
	                     libmv_reconstruction,
	                     progress_update_callback,
	                     callback_customdata);

	return (libmv_Reconstruction *) libmv_reconstruction;
}

libmv_Reconstruction *libmv_solveModal(
	const libmv_Tracks *libmv_tracks,
	const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
	const libmv_ReconstructionOptions *libmv_reconstruction_options,
	reconstruct_progress_update_cb progress_update_callback,
	void *callback_customdata)
{
	libmv_Reconstruction *libmv_reconstruction =
		LIBMV_OBJECT_NEW(libmv_Reconstruction);

	Tracks &tracks = *((Tracks *) libmv_tracks);
	EuclideanReconstruction &reconstruction =
		libmv_reconstruction->reconstruction;

	ReconstructUpdateCallback update_callback =
		ReconstructUpdateCallback(progress_update_callback,
		                          callback_customdata);

	/* Retrieve reconstruction options from C-API to libmv API */
	CameraIntrinsics *camera_intrinsics;
	camera_intrinsics = libmv_reconstruction->intrinsics =
		libmv_cameraIntrinsicsCreateFromOptions(
			libmv_camera_intrinsics_options);

	/* Invert the camera intrinsics. */
	Tracks normalized_tracks;
	libmv_getNormalizedTracks(tracks, *camera_intrinsics, &normalized_tracks);

	/* Actual reconstruction. */
	ModalSolver(normalized_tracks, &reconstruction, &update_callback);

	PolynomialCameraIntrinsics empty_intrinsics;
	EuclideanBundleCommonIntrinsics(normalized_tracks,
	                                libmv::BUNDLE_NO_INTRINSICS,
	                                libmv::BUNDLE_NO_TRANSLATION,
	                                &reconstruction,
	                                &empty_intrinsics);

	/* Refinement. */
	if (libmv_reconstruction_options->refine_intrinsics) {
		libmv_solveRefineIntrinsics(
			tracks,
			libmv_reconstruction_options->refine_intrinsics,
			libmv::BUNDLE_NO_TRANSLATION,
			progress_update_callback, callback_customdata,
			&reconstruction,
			camera_intrinsics);
	}

	/* Finish reconstruction. */
	finishReconstruction(tracks,
	                     *camera_intrinsics,
	                     libmv_reconstruction,
	                     progress_update_callback,
	                     callback_customdata);

	return (libmv_Reconstruction *) libmv_reconstruction;
}

void libmv_reconstructionDestroy(libmv_Reconstruction *libmv_reconstruction)
{
	LIBMV_OBJECT_DELETE(libmv_reconstruction->intrinsics, CameraIntrinsics);
	LIBMV_OBJECT_DELETE(libmv_reconstruction, libmv_Reconstruction);
}

int libmv_reprojectionPointForTrack(
	const libmv_Reconstruction *libmv_reconstruction,
	int track,
	double pos[3])
{
	const EuclideanReconstruction *reconstruction =
		&libmv_reconstruction->reconstruction;
	const EuclideanPoint *point =
		reconstruction->PointForTrack(track);

	if (point) {
		pos[0] = point->X[0];
		pos[1] = point->X[2];
		pos[2] = point->X[1];

		return 1;
	}

	return 0;
}

double libmv_reprojectionErrorForTrack(
	const libmv_Reconstruction *libmv_reconstruction,
	int track)
{
	const EuclideanReconstruction *reconstruction =
		&libmv_reconstruction->reconstruction;
	const CameraIntrinsics *intrinsics = libmv_reconstruction->intrinsics;
	libmv::vector<Marker> markers =
		libmv_reconstruction->tracks.MarkersForTrack(track);

	int num_reprojected = 0;
	double total_error = 0.0;

	for (int i = 0; i < markers.size(); ++i) {
		double weight = markers[i].weight;
		const EuclideanCamera *camera =
			reconstruction->CameraForImage(markers[i].image);
		const EuclideanPoint *point =
			reconstruction->PointForTrack(markers[i].track);

		if (!camera || !point || weight == 0.0) {
			continue;
		}

		num_reprojected++;

		Marker reprojected_marker =
			libmv_projectMarker(*point, *camera, *intrinsics);
		double ex = (reprojected_marker.x - markers[i].x) * weight;
		double ey = (reprojected_marker.y - markers[i].y) * weight;

		total_error += sqrt(ex * ex + ey * ey);
	}

	return total_error / num_reprojected;
}

double libmv_reprojectionErrorForImage(
	const libmv_Reconstruction *libmv_reconstruction,
	int image)
{
	const EuclideanReconstruction *reconstruction =
		&libmv_reconstruction->reconstruction;
	const CameraIntrinsics *intrinsics = libmv_reconstruction->intrinsics;
	libmv::vector<Marker> markers =
		libmv_reconstruction->tracks.MarkersInImage(image);
	const EuclideanCamera *camera = reconstruction->CameraForImage(image);
	int num_reprojected = 0;
	double total_error = 0.0;

	if (!camera) {
		return 0.0;
	}

	for (int i = 0; i < markers.size(); ++i) {
		const EuclideanPoint *point =
			reconstruction->PointForTrack(markers[i].track);

		if (!point) {
			continue;
		}

		num_reprojected++;

		Marker reprojected_marker =
			libmv_projectMarker(*point, *camera, *intrinsics);
		double ex = (reprojected_marker.x - markers[i].x) * markers[i].weight;
		double ey = (reprojected_marker.y - markers[i].y) * markers[i].weight;

		total_error += sqrt(ex * ex + ey * ey);
	}

	return total_error / num_reprojected;
}

int libmv_reprojectionCameraForImage(
	const libmv_Reconstruction *libmv_reconstruction,
	int image, double mat[4][4])
{
	const EuclideanReconstruction *reconstruction =
		&libmv_reconstruction->reconstruction;
	const EuclideanCamera *camera =
		reconstruction->CameraForImage(image);

	if (camera) {
		for (int j = 0; j < 3; ++j) {
			for (int k = 0; k < 3; ++k) {
				int l = k;

				if (k == 1) l = 2;
				else if (k == 2) l = 1;

				if (j == 2) mat[j][l] = -camera->R(j,k);
				else mat[j][l] = camera->R(j,k);
			}
			mat[j][3] = 0.0;
		}

		libmv::Vec3 optical_center = -camera->R.transpose() * camera->t;

		mat[3][0] = optical_center(0);
		mat[3][1] = optical_center(2);
		mat[3][2] = optical_center(1);

		mat[3][3] = 1.0;

		return 1;
	}

	return 0;
}

double libmv_reprojectionError(
	const libmv_Reconstruction *libmv_reconstruction)
{
	return libmv_reconstruction->error;
}

libmv_CameraIntrinsics *libmv_reconstructionExtractIntrinsics(
	libmv_Reconstruction *libmv_reconstruction)
{
	return (libmv_CameraIntrinsics *) libmv_reconstruction->intrinsics;
}

/* ************ Feature detector ************ */

static libmv_Features *libmv_featuresFromVector(
	const libmv::vector<Feature> &features)
{
	libmv_Features *libmv_features = LIBMV_STRUCT_NEW(libmv_Features, 1);
	int count = features.size();
	if (count) {
		libmv_features->features = LIBMV_STRUCT_NEW(Feature, count);

		for (int i = 0; i < count; i++) {
			libmv_features->features[i] = features.at(i);
		}
	}
	else {
		libmv_features->features = NULL;
	}

	libmv_features->count = count;

	return libmv_features;
}

static void libmv_convertDetectorOptions(libmv_DetectOptions *options,
                                         DetectOptions *detector_options)
{
	switch (options->detector) {
#define LIBMV_CONVERT(the_detector) \
	case LIBMV_DETECTOR_ ## the_detector: \
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

libmv_Features *libmv_detectFeaturesByte(
	const unsigned char *image_buffer,
	int width, int height,
	int channels,
	libmv_DetectOptions *options)
{
	// Prepare the image.
	FloatImage image;
	libmv_byteBufferToImage(image_buffer, width, height, channels, &image);

	// Configure detector.
	DetectOptions detector_options;
	libmv_convertDetectorOptions(options, &detector_options);

	// Run the detector.
	libmv::vector<Feature> detected_features;
	Detect(image, detector_options, &detected_features);

	// Convert result to C-API.
	libmv_Features *result = libmv_featuresFromVector(detected_features);
	return result;
}

libmv_Features *libmv_detectFeaturesFloat(const float *image_buffer,
                                          int width, int height,
                                          int channels,
                                          libmv_DetectOptions *options)
{
	// Prepare the image.
	FloatImage image;
	libmv_floatBufferToImage(image_buffer, width, height, channels, &image);

	// Configure detector.
	DetectOptions detector_options;
	libmv_convertDetectorOptions(options, &detector_options);

	// Run the detector.
	libmv::vector<Feature> detected_features;
	Detect(image, detector_options, &detected_features);

	// Convert result to C-API.
	libmv_Features *result = libmv_featuresFromVector(detected_features);
	return result;
}

void libmv_featuresDestroy(libmv_Features *libmv_features)
{
	if (libmv_features->features) {
		LIBMV_STRUCT_DELETE(libmv_features->features);
	}

	LIBMV_STRUCT_DELETE(libmv_features);
}

int libmv_countFeatures(const libmv_Features *libmv_features)
{
	return libmv_features->count;
}

void libmv_getFeature(const libmv_Features *libmv_features,
                      int number,
                      double *x, double *y, double *score, double *size)
{
	Feature &feature = libmv_features->features[number];

	*x = feature.x;
	*y = feature.y;
	*score = feature.score;
	*size = feature.size;
}

/* ************ Camera intrinsics ************ */

libmv_CameraIntrinsics *libmv_cameraIntrinsicsNew(
	const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options)
{
	CameraIntrinsics *camera_intrinsics =
		libmv_cameraIntrinsicsCreateFromOptions(libmv_camera_intrinsics_options);

	return (libmv_CameraIntrinsics *) camera_intrinsics;
}

libmv_CameraIntrinsics *libmv_cameraIntrinsicsCopy(
	const libmv_CameraIntrinsics *libmvIntrinsics)
{
	const CameraIntrinsics *orig_intrinsics =
		(const CameraIntrinsics *) libmvIntrinsics;

	CameraIntrinsics *new_intrinsics = NULL;

	switch (orig_intrinsics->GetDistortionModelType()) {
		case libmv::DISTORTION_MODEL_POLYNOMIAL:
		{
			const PolynomialCameraIntrinsics *polynomial_intrinsics =
				static_cast<const PolynomialCameraIntrinsics*>(orig_intrinsics);
			new_intrinsics = LIBMV_OBJECT_NEW(PolynomialCameraIntrinsics,
								 *polynomial_intrinsics);
			break;
		}
		case libmv::DISTORTION_MODEL_DIVISION:
		{
			const DivisionCameraIntrinsics *division_intrinsics =
				static_cast<const DivisionCameraIntrinsics*>(orig_intrinsics);
			new_intrinsics = LIBMV_OBJECT_NEW(DivisionCameraIntrinsics,
								 *division_intrinsics);
			break;
		}
		default:
			assert(!"Unknown distortion model");
	}

	return (libmv_CameraIntrinsics *) new_intrinsics;
}

void libmv_cameraIntrinsicsDestroy(libmv_CameraIntrinsics *libmvIntrinsics)
{
	LIBMV_OBJECT_DELETE(libmvIntrinsics, CameraIntrinsics);
}

void libmv_cameraIntrinsicsUpdate(
	const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
	libmv_CameraIntrinsics *libmv_intrinsics)
{
	CameraIntrinsics *camera_intrinsics = (CameraIntrinsics *) libmv_intrinsics;

	double focal_length = libmv_camera_intrinsics_options->focal_length;
	double principal_x = libmv_camera_intrinsics_options->principal_point_x;
	double principal_y = libmv_camera_intrinsics_options->principal_point_y;
	int image_width = libmv_camera_intrinsics_options->image_width;
	int image_height = libmv_camera_intrinsics_options->image_height;

	/* Try avoid unnecessary updates,
	 * so pre-computed distortion grids are not freed.
	 */

	if (camera_intrinsics->focal_length() != focal_length)
		camera_intrinsics->SetFocalLength(focal_length, focal_length);

	if (camera_intrinsics->principal_point_x() != principal_x ||
	    camera_intrinsics->principal_point_y() != principal_y)
	{
		camera_intrinsics->SetPrincipalPoint(principal_x, principal_y);
	}

	if (camera_intrinsics->image_width() != image_width ||
	    camera_intrinsics->image_height() != image_height)
	{
		camera_intrinsics->SetImageSize(image_width, image_height);
	}

	switch (libmv_camera_intrinsics_options->distortion_model) {
		case LIBMV_DISTORTION_MODEL_POLYNOMIAL:
		{
			assert(camera_intrinsics->GetDistortionModelType() ==
				   libmv::DISTORTION_MODEL_POLYNOMIAL);

			PolynomialCameraIntrinsics *polynomial_intrinsics =
				(PolynomialCameraIntrinsics *) camera_intrinsics;

			double k1 = libmv_camera_intrinsics_options->polynomial_k1;
			double k2 = libmv_camera_intrinsics_options->polynomial_k2;
			double k3 = libmv_camera_intrinsics_options->polynomial_k3;

			if (polynomial_intrinsics->k1() != k1 ||
			    polynomial_intrinsics->k2() != k2 ||
			    polynomial_intrinsics->k3() != k3)
			{
				polynomial_intrinsics->SetRadialDistortion(k1, k2, k3);
			}

			break;
		}

		case LIBMV_DISTORTION_MODEL_DIVISION:
		{
			assert(camera_intrinsics->GetDistortionModelType() ==
				   libmv::DISTORTION_MODEL_DIVISION);

			DivisionCameraIntrinsics *division_intrinsics =
				(DivisionCameraIntrinsics *) camera_intrinsics;

			double k1 = libmv_camera_intrinsics_options->division_k1;
			double k2 = libmv_camera_intrinsics_options->division_k2;

			if (division_intrinsics->k1() != k1 ||
			    division_intrinsics->k2() != k2)
			{
				division_intrinsics->SetDistortion(k1, k2);
			}

			break;
		}

		default:
			assert(!"Unknown distortion model");
	}
}

void libmv_cameraIntrinsicsSetThreads(
	libmv_CameraIntrinsics *libmv_intrinsics, int threads)
{
	CameraIntrinsics *camera_intrinsics = (CameraIntrinsics *) libmv_intrinsics;

	camera_intrinsics->SetThreads(threads);
}

void libmv_cameraIntrinsicsExtractOptions(
	const libmv_CameraIntrinsics *libmv_intrinsics,
	libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
	const CameraIntrinsics *camera_intrinsics =
		(const CameraIntrinsics *) libmv_intrinsics;

	// Fill in options which are common for all distortion models.
	camera_intrinsics_options->focal_length = camera_intrinsics->focal_length();
	camera_intrinsics_options->principal_point_x =
		camera_intrinsics->principal_point_x();
	camera_intrinsics_options->principal_point_y =
		camera_intrinsics->principal_point_y();

	camera_intrinsics_options->image_width = camera_intrinsics->image_width();
	camera_intrinsics_options->image_height = camera_intrinsics->image_height();

	switch (camera_intrinsics->GetDistortionModelType()) {
		case libmv::DISTORTION_MODEL_POLYNOMIAL:
		{
			const PolynomialCameraIntrinsics *polynomial_intrinsics =
				static_cast<const PolynomialCameraIntrinsics *>(camera_intrinsics);
			camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_POLYNOMIAL;
			camera_intrinsics_options->polynomial_k1 = polynomial_intrinsics->k1();
			camera_intrinsics_options->polynomial_k2 = polynomial_intrinsics->k2();
			camera_intrinsics_options->polynomial_k3 = polynomial_intrinsics->k3();
			camera_intrinsics_options->polynomial_p1 = polynomial_intrinsics->p1();
			camera_intrinsics_options->polynomial_p1 = polynomial_intrinsics->p2();
			break;
		}

		case libmv::DISTORTION_MODEL_DIVISION:
		{
			const DivisionCameraIntrinsics *division_intrinsics =
				static_cast<const DivisionCameraIntrinsics *>(camera_intrinsics);
			camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_DIVISION;
			camera_intrinsics_options->division_k1 = division_intrinsics->k1();
			camera_intrinsics_options->division_k2 = division_intrinsics->k2();
			break;
		}

		default:
			assert(!"Uknown distortion model");
	}
}

void libmv_cameraIntrinsicsUndistortByte(
	const libmv_CameraIntrinsics *libmv_intrinsics,
	unsigned char *src, unsigned char *dst, int width, int height,
	float overscan, int channels)
{
	CameraIntrinsics *camera_intrinsics = (CameraIntrinsics *) libmv_intrinsics;
	camera_intrinsics->UndistortBuffer(src,
	                                   width, height, overscan, channels,
	                                   dst);
}

void libmv_cameraIntrinsicsUndistortFloat(
	const libmv_CameraIntrinsics *libmvIntrinsics,
	float *src, float *dst, int width, int height,
	float overscan, int channels)
{
	CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmvIntrinsics;
	intrinsics->UndistortBuffer(src,
	                            width, height, overscan, channels,
	                            dst);
}

void libmv_cameraIntrinsicsDistortByte(
	const libmv_CameraIntrinsics *libmvIntrinsics,
	unsigned char *src, unsigned char *dst, int width, int height,
	float overscan, int channels)
{
	CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmvIntrinsics;
	intrinsics->DistortBuffer(src,
	                          width, height, overscan, channels,
	                          dst);
}

void libmv_cameraIntrinsicsDistortFloat(
	const libmv_CameraIntrinsics *libmvIntrinsics,
	float *src, float *dst, int width, int height,
	float overscan, int channels)
{
	CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmvIntrinsics;
	intrinsics->DistortBuffer(src,
	                          width, height, overscan, channels,
	                          dst);
}

void libmv_cameraIntrinsicsApply(
	const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
	double x, double y, double *x1, double *y1)
{
	/* do a lens undistortion if focal length is non-zero only */
	if (libmv_camera_intrinsics_options->focal_length) {
		CameraIntrinsics *camera_intrinsics =
			libmv_cameraIntrinsicsCreateFromOptions(libmv_camera_intrinsics_options);

		camera_intrinsics->ApplyIntrinsics(x, y, x1, y1);

		LIBMV_OBJECT_DELETE(camera_intrinsics, CameraIntrinsics);
	}
}

void libmv_cameraIntrinsicsInvert(
	const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
	double x, double y, double *x1, double *y1)
{
	/* do a lens distortion if focal length is non-zero only */
	if (libmv_camera_intrinsics_options->focal_length) {
		CameraIntrinsics *camera_intrinsics =
			libmv_cameraIntrinsicsCreateFromOptions(libmv_camera_intrinsics_options);

		camera_intrinsics->InvertIntrinsics(x, y, x1, y1);

		LIBMV_OBJECT_DELETE(camera_intrinsics, CameraIntrinsics);
	}
}

void libmv_homography2DFromCorrespondencesEuc(double (*x1)[2],
                                              double (*x2)[2],
                                              int num_points,
                                              double H[3][3])
{
	libmv::Mat x1_mat, x2_mat;
	libmv::Mat3 H_mat;

	x1_mat.resize(2, num_points);
	x2_mat.resize(2, num_points);

	for (int i = 0; i < num_points; i++) {
		x1_mat.col(i) = libmv::Vec2(x1[i][0], x1[i][1]);
		x2_mat.col(i) = libmv::Vec2(x2[i][0], x2[i][1]);
	}

	LG << "x1: " << x1_mat;
	LG << "x2: " << x2_mat;

	libmv::EstimateHomographyOptions options;
	libmv::EstimateHomography2DFromCorrespondences(x1_mat,
	                                               x2_mat,
	                                               options,
	                                               &H_mat);

	LG << "H: " << H_mat;

	memcpy(H, H_mat.data(), 9 * sizeof(double));
}

#endif

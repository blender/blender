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

/* define this to generate PNG images with content of search areas
   tracking between which failed */
#undef DUMP_FAILURE

#include "libmv-capi.h"

#include "glog/logging.h"
#include "Math/v3d_optimization.h"

#include "libmv/tracking/klt_region_tracker.h"
#include "libmv/tracking/trklt_region_tracker.h"
#include "libmv/tracking/pyramid_region_tracker.h"
#include "libmv/tracking/retrack_region_tracker.h"

#include "libmv/tracking/sad.h"

#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/initialize_reconstruction.h"
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/detect.h"
#include "libmv/simple_pipeline/pipeline.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"

#include <stdlib.h>

#ifdef DUMP_FAILURE
#  include <png.h>
#endif

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

#define DEFAULT_WINDOW_HALFSIZE	5

typedef struct libmv_RegionTracker {
	libmv::TrkltRegionTracker *trklt_region_tracker;
	libmv::PyramidRegionTracker *pyramid_region_tracker;
	libmv::RegionTracker *region_tracker;
} libmv_RegionTracker;

typedef struct libmv_Reconstruction {
	libmv::EuclideanReconstruction reconstruction;

	/* used for per-track average error calculation after reconstruction */
	libmv::Tracks tracks;
	libmv::CameraIntrinsics intrinsics;

	double error;
} libmv_Reconstruction;

typedef struct libmv_Features {
	int count, margin;
	libmv::Feature *features;
} libmv_Features;

/* ************ Logging ************ */

void libmv_initLogging(const char *argv0)
{
	google::InitGoogleLogging(argv0);
	google::SetCommandLineOption("logtostderr", "1");
	google::SetCommandLineOption("v", "0");
	google::SetCommandLineOption("stderrthreshold", "7");
	google::SetCommandLineOption("minloglevel", "7");
	V3D::optimizerVerbosenessLevel = 0;
}

void libmv_startDebugLogging(void)
{
	google::SetCommandLineOption("logtostderr", "1");
	google::SetCommandLineOption("v", "0");
	google::SetCommandLineOption("stderrthreshold", "1");
	google::SetCommandLineOption("minloglevel", "0");
	V3D::optimizerVerbosenessLevel = 1;
}

void libmv_setLoggingVerbosity(int verbosity)
{
	char val[10];
	snprintf(val, sizeof(val), "%d", verbosity);

	google::SetCommandLineOption("v", val);
	V3D::optimizerVerbosenessLevel = verbosity;
}

/* ************ RegionTracker ************ */

libmv_RegionTracker *libmv_regionTrackerNew(int max_iterations, int pyramid_level, double tolerance)
{
	libmv::RegionTracker *region_tracker;
	libmv::TrkltRegionTracker *trklt_region_tracker = new libmv::TrkltRegionTracker;

	trklt_region_tracker->half_window_size = DEFAULT_WINDOW_HALFSIZE;
	trklt_region_tracker->max_iterations = max_iterations;

	libmv::PyramidRegionTracker *pyramid_region_tracker =
		new libmv::PyramidRegionTracker(trklt_region_tracker, pyramid_level);

	region_tracker = new libmv::RetrackRegionTracker(pyramid_region_tracker, tolerance);

	libmv_RegionTracker *configured_region_tracker = new libmv_RegionTracker;
	configured_region_tracker->trklt_region_tracker = trklt_region_tracker;
	configured_region_tracker->pyramid_region_tracker = pyramid_region_tracker;
	configured_region_tracker->region_tracker = region_tracker;

	return configured_region_tracker;
}

static void floatBufToImage(const float *buf, int width, int height, libmv::FloatImage *image)
{
	int x, y, a = 0;

	image->resize(height, width);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			(*image)(y, x, 0) = buf[a++];
		}
	}
}

#ifdef DUMP_FAILURE
void savePNGImage(png_bytep *row_pointers, int width, int height, int depth, int color_type, char *file_name)
{
	png_infop info_ptr;
	png_structp png_ptr;
	FILE *fp = fopen(file_name, "wb");

	if (!fp)
		return;

	/* Initialize stuff */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);

	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height,
		depth, color_type, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_write_image(png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_write_end(png_ptr, NULL);

	fclose(fp);
}

static void saveImage(char *prefix, libmv::FloatImage image, int x0, int y0)
{
	int x, y;
	png_bytep *row_pointers;

	row_pointers= (png_bytep*)malloc(sizeof(png_bytep)*image.Height());

	for (y = 0; y < image.Height(); y++) {
		row_pointers[y]= (png_bytep)malloc(sizeof(png_byte)*4*image.Width());

		for (x = 0; x < image.Width(); x++) {
			if (x0 == x && y0 == y) {
				row_pointers[y][x*4+0]= 255;
				row_pointers[y][x*4+1]= 0;
				row_pointers[y][x*4+2]= 0;
				row_pointers[y][x*4+3]= 255;
			}
			else {
				float pixel = image(y, x, 0);
				row_pointers[y][x*4+0]= pixel*255;
				row_pointers[y][x*4+1]= pixel*255;
				row_pointers[y][x*4+2]= pixel*255;
				row_pointers[y][x*4+3]= 255;
			}
		}
	}

	{
		static int a= 0;
		char buf[128];
		snprintf(buf, sizeof(buf), "%s_%02d.png", prefix, ++a);
		savePNGImage(row_pointers, image.Width(), image.Height(), 8, PNG_COLOR_TYPE_RGBA, buf);
	}

	for (y = 0; y < image.Height(); y++) {
		free(row_pointers[y]);
	}
	free(row_pointers);
}

static void saveBytesImage(char *prefix, unsigned char *data, int width, int height)
{
	int x, y;
	png_bytep *row_pointers;

	row_pointers= (png_bytep*)malloc(sizeof(png_bytep)*height);

	for (y = 0; y < height; y++) {
		row_pointers[y]= (png_bytep)malloc(sizeof(png_byte)*4*width);

		for (x = 0; x < width; x++) {
			char pixel = data[width*y+x];
			row_pointers[y][x*4+0]= pixel;
			row_pointers[y][x*4+1]= pixel;
			row_pointers[y][x*4+2]= pixel;
			row_pointers[y][x*4+3]= 255;
		}
	}

	{
		static int a= 0;
		char buf[128];
		snprintf(buf, sizeof(buf), "%s_%02d.png", prefix, ++a);
		savePNGImage(row_pointers, width, height, 8, PNG_COLOR_TYPE_RGBA, buf);
	}

	for (y = 0; y < height; y++) {
		free(row_pointers[y]);
	}
	free(row_pointers);
}
#endif

int libmv_regionTrackerTrack(libmv_RegionTracker *libmv_tracker, const float *ima1, const float *ima2,
			 int width, int height, int half_window_size,
			 double x1, double y1, double *x2, double *y2)
{
	libmv::RegionTracker *region_tracker;
	libmv::TrkltRegionTracker *trklt_region_tracker;
	libmv::FloatImage old_patch, new_patch;

	trklt_region_tracker = libmv_tracker->trklt_region_tracker;
	region_tracker = libmv_tracker->region_tracker;

	trklt_region_tracker->half_window_size = half_window_size;

	floatBufToImage(ima1, width, height, &old_patch);
	floatBufToImage(ima2, width, height, &new_patch);

#ifndef DUMP_FAILURE
	return region_tracker->Track(old_patch, new_patch, x1, y1, x2, y2);
#else
	{
		double sx2 = *x2, sy2 = *y2;
		int result = region_tracker->Track(old_patch, new_patch, x1, y1, x2, y2);

		if (!result) {
			saveImage("old_patch", old_patch, x1, y1);
			saveImage("new_patch", new_patch, sx2, sy2);
		}

		return result;
	}
#endif
}

void libmv_regionTrackerDestroy(libmv_RegionTracker *libmv_tracker)
{
	delete libmv_tracker->region_tracker;
	delete libmv_tracker;
}

/* ************ Tracks ************ */

void libmv_SADSamplePattern(unsigned char *image, int stride,
			float warp[3][2], unsigned char *pattern)
{
	libmv::mat32 mat32;

	memcpy(mat32.data, warp, sizeof(float)*3*2);

	libmv::SamplePattern(image, stride, mat32, pattern, 16);
}

float libmv_SADTrackerTrack(unsigned char *pattern, unsigned char *warped, unsigned char *image, int stride,
			int width, int height, float warp[3][2])
{
	float result;
	libmv::mat32 mat32;

	memcpy(mat32.data, warp, sizeof(float)*3*2);

	result = libmv::Track(pattern, warped, 16, image, stride, width, height, &mat32, 16, 16);

	memcpy(warp, mat32.data, sizeof(float)*3*2);

	return result;
}

/* ************ Tracks ************ */

libmv_Tracks *libmv_tracksNew(void)
{
	libmv::Tracks *libmv_tracks = new libmv::Tracks();

	return (libmv_Tracks *)libmv_tracks;
}

void libmv_tracksInsert(struct libmv_Tracks *libmv_tracks, int image, int track, double x, double y)
{
	((libmv::Tracks*)libmv_tracks)->Insert(image, track, x, y);
}

void libmv_tracksDestroy(libmv_Tracks *libmv_tracks)
{
	delete (libmv::Tracks*)libmv_tracks;
}

/* ************ Reconstruction solver ************ */

libmv_Reconstruction *libmv_solveReconstruction(libmv_Tracks *tracks, int keyframe1, int keyframe2,
			double focal_length, double principal_x, double principal_y, double k1, double k2, double k3)
{
	/* Invert the camera intrinsics. */
	libmv::vector<libmv::Marker> markers = ((libmv::Tracks*)tracks)->AllMarkers();
	libmv_Reconstruction *libmv_reconstruction = new libmv_Reconstruction();
	libmv::EuclideanReconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::CameraIntrinsics *intrinsics = &libmv_reconstruction->intrinsics;

	intrinsics->SetFocalLength(focal_length, focal_length);
	intrinsics->SetPrincipalPoint(principal_x, principal_y);
	intrinsics->SetRadialDistortion(k1, k2, k3);

	if(focal_length) {
		/* do a lens undistortion if focal length is non-zero only */
		for (int i = 0; i < markers.size(); ++i) {
			intrinsics->InvertIntrinsics(markers[i].x,
				markers[i].y,
				&(markers[i].x),
				&(markers[i].y));
		}
	}

	libmv::Tracks normalized_tracks(markers);

	libmv::vector<libmv::Marker> keyframe_markers =
		normalized_tracks.MarkersForTracksInBothImages(keyframe1, keyframe2);

	libmv::EuclideanReconstructTwoFrames(keyframe_markers, reconstruction);
	libmv::EuclideanBundle(normalized_tracks, reconstruction);
	libmv::EuclideanCompleteReconstruction(normalized_tracks, reconstruction);

	libmv_reconstruction->tracks = *(libmv::Tracks *)tracks;
	libmv_reconstruction->error = libmv::EuclideanReprojectionError(*(libmv::Tracks *)tracks, *reconstruction, *intrinsics);

	return (libmv_Reconstruction *)libmv_reconstruction;
}

int libmv_reporojectionPointForTrack(libmv_Reconstruction *libmv_reconstruction, int track, double pos[3])
{
	libmv::EuclideanReconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::EuclideanPoint *point = reconstruction->PointForTrack(track);

	if(point) {
		pos[0] = point->X[0];
		pos[1] = point->X[2];
		pos[2] = point->X[1];

		return 1;
	}

	return 0;
}

static libmv::Marker ProjectMarker(const libmv::EuclideanPoint &point, const libmv::EuclideanCamera &camera,
			const libmv::CameraIntrinsics &intrinsics) {
	libmv::Vec3 projected = camera.R * point.X + camera.t;
	projected /= projected(2);

	libmv::Marker reprojected_marker;
	intrinsics.ApplyIntrinsics(projected(0), projected(1), &reprojected_marker.x, &reprojected_marker.y);

	reprojected_marker.image = camera.image;
	reprojected_marker.track = point.track;

	return reprojected_marker;
}

double libmv_reporojectionErrorForTrack(libmv_Reconstruction *libmv_reconstruction, int track)
{
	libmv::EuclideanReconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::CameraIntrinsics *intrinsics = &libmv_reconstruction->intrinsics;
	libmv::vector<libmv::Marker> markers =  libmv_reconstruction->tracks.MarkersForTrack(track);

	int num_reprojected = 0;
	double total_error = 0.0;

	for (int i = 0; i < markers.size(); ++i) {
		const libmv::EuclideanCamera *camera = reconstruction->CameraForImage(markers[i].image);
		const libmv::EuclideanPoint *point = reconstruction->PointForTrack(markers[i].track);

		if (!camera || !point) {
			continue;
		}

		num_reprojected++;

		libmv::Marker reprojected_marker = ProjectMarker(*point, *camera, *intrinsics);
		double ex = reprojected_marker.x - markers[i].x;
		double ey = reprojected_marker.y - markers[i].y;

		total_error += sqrt(ex*ex + ey*ey);
	}

	return total_error / num_reprojected;
}

double libmv_reporojectionErrorForImage(libmv_Reconstruction *libmv_reconstruction, int image)
{
	libmv::EuclideanReconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::CameraIntrinsics *intrinsics = &libmv_reconstruction->intrinsics;
	libmv::vector<libmv::Marker> markers = libmv_reconstruction->tracks.MarkersInImage(image);
	const libmv::EuclideanCamera *camera = reconstruction->CameraForImage(image);
	int num_reprojected = 0;
	double total_error = 0.0;

	if (!camera)
		return 0;

	for (int i = 0; i < markers.size(); ++i) {
		const libmv::EuclideanPoint *point = reconstruction->PointForTrack(markers[i].track);

		if (!point) {
			continue;
		}

		num_reprojected++;

		libmv::Marker reprojected_marker = ProjectMarker(*point, *camera, *intrinsics);
		double ex = reprojected_marker.x - markers[i].x;
		double ey = reprojected_marker.y - markers[i].y;

		total_error += sqrt(ex*ex + ey*ey);
	}

	return total_error / num_reprojected;
}

int libmv_reporojectionCameraForImage(libmv_Reconstruction *libmv_reconstruction, int image, double mat[4][4])
{
	libmv::EuclideanReconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::EuclideanCamera *camera = reconstruction->CameraForImage(image);

	if(camera) {
		for (int j = 0; j < 3; ++j) {
			for (int k = 0; k < 3; ++k) {
				int l = k;

				if (k == 1) l = 2;
				else if (k == 2) l = 1;

				if (j == 2) mat[j][l] = -camera->R(j,k);
				else mat[j][l] = camera->R(j,k);
			}
			mat[j][3]= 0.0;
		}

		libmv::Vec3 optical_center = -camera->R.transpose() * camera->t;

		mat[3][0] = optical_center(0);
		mat[3][1] = optical_center(2);
		mat[3][2] = optical_center(1);

		mat[3][3]= 1.0;

		return 1;
	}

	return 0;
}

double libmv_reprojectionError(libmv_Reconstruction *libmv_reconstruction)
{
	return libmv_reconstruction->error;
}

void libmv_destroyReconstruction(libmv_Reconstruction *libmv_reconstruction)
{
	delete libmv_reconstruction;
}

/* ************ feature detector ************ */

struct libmv_Features *libmv_detectFeaturesFAST(unsigned char *data, int width, int height, int stride,
			int margin, int min_trackness, int min_distance)
{
	libmv::Feature *features = NULL;
	std::vector<libmv::Feature> v;
	libmv_Features *libmv_features = new libmv_Features();
	int i= 0, count;

	if(margin) {
		data += margin*stride+margin;
		width -= 2*margin;
		height -= 2*margin;
	}

	v = libmv::DetectFAST(data, width, height, stride, min_trackness, min_distance);

	count = v.size();

	if(count) {
		features= new libmv::Feature[count];

		for(std::vector<libmv::Feature>::iterator it = v.begin(); it != v.end(); it++) {
			features[i++]= *it;
		}
	}

	libmv_features->features = features;
	libmv_features->count = count;
	libmv_features->margin = margin;

	return (libmv_Features *)libmv_features;
}

struct libmv_Features *libmv_detectFeaturesMORAVEC(unsigned char *data, int width, int height, int stride,
			int margin, int count, int min_distance)
{
	libmv::Feature *features = NULL;
	libmv_Features *libmv_features = new libmv_Features;

	if(count) {
		if(margin) {
			data += margin*stride+margin;
			width -= 2*margin;
			height -= 2*margin;
		}

		features = new libmv::Feature[count];
		libmv::DetectMORAVEC(data, stride, width, height, features, &count, min_distance, NULL);
	}

	libmv_features->count = count;
	libmv_features->margin = margin;
	libmv_features->features = features;

	return libmv_features;
}

int libmv_countFeatures(struct libmv_Features *libmv_features)
{
	return libmv_features->count;
}

void libmv_getFeature(struct libmv_Features *libmv_features, int number, double *x, double *y, double *score, double *size)
{
	libmv::Feature feature= libmv_features->features[number];

	*x = feature.x + libmv_features->margin;
	*y = feature.y + libmv_features->margin;
	*score = feature.score;
	*size = feature.size;
}

void libmv_destroyFeatures(struct libmv_Features *libmv_features)
{
	if(libmv_features->features)
		delete [] libmv_features->features;

	delete libmv_features;
}

/* ************ camera intrinsics ************ */

struct libmv_CameraIntrinsics *libmv_CameraIntrinsicsNew(double focal_length, double principal_x, double principal_y,
			double k1, double k2, double k3, int width, int height)
{
	libmv::CameraIntrinsics *intrinsics= new libmv::CameraIntrinsics();

	intrinsics->SetFocalLength(focal_length, focal_length);
	intrinsics->SetPrincipalPoint(principal_x, principal_y);
	intrinsics->SetRadialDistortion(k1, k2, k3);
	intrinsics->SetImageSize(width, height);

	return (struct libmv_CameraIntrinsics *) intrinsics;
}

struct libmv_CameraIntrinsics *libmv_CameraIntrinsicsCopy(struct libmv_CameraIntrinsics *libmvIntrinsics)
{
	libmv::CameraIntrinsics *orig_intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;
	libmv::CameraIntrinsics *new_intrinsics= new libmv::CameraIntrinsics(*orig_intrinsics);

	return (struct libmv_CameraIntrinsics *) new_intrinsics;
}

void libmv_CameraIntrinsicsDestroy(struct libmv_CameraIntrinsics *libmvIntrinsics)
{
	libmv::CameraIntrinsics *intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;

	delete intrinsics;
}

void libmv_CameraIntrinsicsUpdate(struct libmv_CameraIntrinsics *libmvIntrinsics, double focal_length,
			double principal_x, double principal_y, double k1, double k2, double k3, int width, int height)
{
	libmv::CameraIntrinsics *intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;

	if (intrinsics->focal_length() != focal_length)
		intrinsics->SetFocalLength(focal_length, focal_length);

	if (intrinsics->principal_point_x() != principal_x || intrinsics->principal_point_y() != principal_y)
		intrinsics->SetFocalLength(focal_length, focal_length);

	if (intrinsics->k1() != k1 || intrinsics->k2() != k2 || intrinsics->k3() != k3)
		intrinsics->SetRadialDistortion(k1, k2, k3);

	if (intrinsics->image_width() != width || intrinsics->image_height() != height)
		intrinsics->SetImageSize(width, height);
}

void libmv_CameraIntrinsicsUndistortByte(struct libmv_CameraIntrinsics *libmvIntrinsics,
			unsigned char *src, unsigned char *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics *intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;

	intrinsics->Undistort(src, dst, width, height, overscan, channels);
}

void libmv_CameraIntrinsicsUndistortFloat(struct libmv_CameraIntrinsics *libmvIntrinsics,
			float *src, float *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics *intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;

	intrinsics->Undistort(src, dst, width, height, overscan, channels);
}

void libmv_CameraIntrinsicsDistortByte(struct libmv_CameraIntrinsics *libmvIntrinsics,
			unsigned char *src, unsigned char *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics *intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;
	intrinsics->Distort(src, dst, width, height, overscan, channels);
}

void libmv_CameraIntrinsicsDistortFloat(struct libmv_CameraIntrinsics *libmvIntrinsics,
			float *src, float *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics *intrinsics = (libmv::CameraIntrinsics *) libmvIntrinsics;

	intrinsics->Distort(src, dst, width, height, overscan, channels);
}

/* ************ distortion ************ */

void libmv_undistortByte(double focal_length, double principal_x, double principal_y, double k1, double k2, double k3,
			unsigned char *src, unsigned char *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics intrinsics;

	intrinsics.SetFocalLength(focal_length, focal_length);
	intrinsics.SetPrincipalPoint(principal_x, principal_y);
	intrinsics.SetRadialDistortion(k1, k2, k3);

	intrinsics.Undistort(src, dst, width, height, overscan, channels);
}

void libmv_undistortFloat(double focal_length, double principal_x, double principal_y, double k1, double k2, double k3,
			float *src, float *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics intrinsics;

	intrinsics.SetFocalLength(focal_length, focal_length);
	intrinsics.SetPrincipalPoint(principal_x, principal_y);
	intrinsics.SetRadialDistortion(k1, k2, k3);

	intrinsics.Undistort(src, dst, width, height, overscan, channels);
}

void libmv_distortByte(double focal_length, double principal_x, double principal_y, double k1, double k2, double k3,
			unsigned char *src, unsigned char *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics intrinsics;

	intrinsics.SetFocalLength(focal_length, focal_length);
	intrinsics.SetPrincipalPoint(principal_x, principal_y);
	intrinsics.SetRadialDistortion(k1, k2, k3);

	intrinsics.Distort(src, dst, width, height, overscan, channels);
}

void libmv_distortFloat(double focal_length, double principal_x, double principal_y, double k1, double k2, double k3,
			float *src, float *dst, int width, int height, float overscan, int channels)
{
	libmv::CameraIntrinsics intrinsics;

	intrinsics.SetFocalLength(focal_length, focal_length);
	intrinsics.SetPrincipalPoint(principal_x, principal_y);
	intrinsics.SetRadialDistortion(k1, k2, k3);

	intrinsics.Distort(src, dst, width, height, overscan, channels);
}

/* ************ utils ************ */

void libmv_applyCameraIntrinsics(double focal_length, double principal_x, double principal_y, double k1, double k2, double k3,
			double x, double y, double *x1, double *y1)
{
	libmv::CameraIntrinsics intrinsics;

	intrinsics.SetFocalLength(focal_length, focal_length);
	intrinsics.SetPrincipalPoint(principal_x, principal_y);
	intrinsics.SetRadialDistortion(k1, k2, k3);

	if(focal_length) {
		/* do a lens undistortion if focal length is non-zero only */

		intrinsics.ApplyIntrinsics(x, y, x1, y1);
	}
}

void libmv_InvertIntrinsics(double focal_length, double principal_x, double principal_y, double k1, double k2, double k3,
			double x, double y, double *x1, double *y1)
{
	libmv::CameraIntrinsics intrinsics;

	intrinsics.SetFocalLength(focal_length, focal_length);
	intrinsics.SetPrincipalPoint(principal_x, principal_y);
	intrinsics.SetRadialDistortion(k1, k2, k3);

	if(focal_length) {
		/* do a lens distortion if focal length is non-zero only */

		intrinsics.InvertIntrinsics(x, y, x1, y1);
	}
}

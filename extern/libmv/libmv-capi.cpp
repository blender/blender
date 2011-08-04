/*
 * $Id$
 *
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
	libmv::Reconstruction reconstruction;

	/* used for per-track average error calculation after reconstruction */
	libmv::Tracks tracks;
	libmv::CameraIntrinsics intrinsics;

	double error;
} libmv_Reconstruction;

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

static void saveImage(libmv::FloatImage image, int x0, int y0)
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
		snprintf(buf, sizeof(buf), "%02d.png", ++a);
		savePNGImage(row_pointers, image.Width(), image.Height(), 8, PNG_COLOR_TYPE_RGBA, buf);
	}

	for (y = 0; y < image.Height(); y++) {
		free(row_pointers[y]);
	}
	free(row_pointers);
}

static void saveBytesImage(unsigned char *data, int width, int height)
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
		snprintf(buf, sizeof(buf), "%02d.png", ++a);
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
			saveImage(old_patch, x1, y1);
			saveImage(new_patch, sx2, sy2);
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
	libmv::Reconstruction *reconstruction = &libmv_reconstruction->reconstruction;
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

	libmv::ReconstructTwoFrames(keyframe_markers, reconstruction);
	libmv::Bundle(normalized_tracks, reconstruction);
	libmv::CompleteReconstruction(normalized_tracks, reconstruction);

	libmv_reconstruction->tracks = *(libmv::Tracks *)tracks;
	libmv_reconstruction->error = libmv::ReprojectionError(*(libmv::Tracks *)tracks, *reconstruction, *intrinsics);

	return (libmv_Reconstruction *)libmv_reconstruction;
}

int libmv_reporojectionPointForTrack(libmv_Reconstruction *libmv_reconstruction, int track, double pos[3])
{
	libmv::Reconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::Point *point = reconstruction->PointForTrack(track);

	if(point) {
		pos[0] = point->X[0];
		pos[1] = point->X[2];
		pos[2] = point->X[1];

		return 1;
	}

	return 0;
}

static libmv::Marker ProjectMarker(const libmv::Point &point, const libmv::Camera &camera,
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
	libmv::Reconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::CameraIntrinsics *intrinsics = &libmv_reconstruction->intrinsics;
	libmv::vector<libmv::Marker> markers =  libmv_reconstruction->tracks.MarkersForTrack(track);

	int num_reprojected = 0;
	double total_error = 0.0;

	for (int i = 0; i < markers.size(); ++i) {
		const libmv::Camera *camera = reconstruction->CameraForImage(markers[i].image);
		const libmv::Point *point = reconstruction->PointForTrack(markers[i].track);

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

int libmv_reporojectionCameraForImage(libmv_Reconstruction *libmv_reconstruction, int image, double mat[4][4])
{
	libmv::Reconstruction *reconstruction = &libmv_reconstruction->reconstruction;
	libmv::Camera *camera = reconstruction->CameraForImage(image);

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

struct libmv_Corners *libmv_detectCorners(unsigned char *data, int width, int height, int stride)
{
	std::vector<libmv::Corner> detect= libmv::Detect(data, width, height, stride);
	std::vector<libmv::Corner> *libmv_corners= new std::vector<libmv::Corner>();

	libmv_corners->insert(libmv_corners->begin(), detect.begin(), detect.end());

	return (libmv_Corners *)libmv_corners;
}

int libmv_countCorners(struct libmv_Corners *libmv_corners)
{
	return ((std::vector<libmv::Corner> *)libmv_corners)->size();
}

void libmv_getCorner(struct libmv_Corners *libmv_corners, int number, double *x, double *y, double *score, double *size)
{
	libmv::Corner corner = ((std::vector<libmv::Corner> *)libmv_corners)->at(number);

	*x = corner.x;
	*y = corner.y;
	*score = corner.score;
	*size = corner.size;
}

void libmv_destroyCorners(struct libmv_Corners *libmv_corners)
{
	delete (std::vector<libmv::Corner> *)libmv_corners;
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

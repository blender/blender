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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "libmv-util.h"
#include "libmv-capi_intern.h"

#include <cassert>
#include <png.h>

using libmv::CameraIntrinsics;
using libmv::DivisionCameraIntrinsics;
using libmv::EuclideanCamera;
using libmv::EuclideanPoint;
using libmv::FloatImage;
using libmv::Marker;
using libmv::PolynomialCameraIntrinsics;
using libmv::Tracks;

/* Image <-> buffers conversion */

void libmv_byteBufferToImage(const unsigned char *buf,
                             int width, int height, int channels,
                             FloatImage *image)
{
	int x, y, k, a = 0;

	image->Resize(height, width, channels);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			for (k = 0; k < channels; k++) {
				(*image)(y, x, k) = (float)buf[a++] / 255.0f;
			}
		}
	}
}

void libmv_floatBufferToImage(const float *buf,
                              int width, int height, int channels,
                              FloatImage *image)
{
	image->Resize(height, width, channels);

	for (int y = 0, a = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			for (int k = 0; k < channels; k++) {
				(*image)(y, x, k) = buf[a++];
			}
		}
	}
}

void libmv_imageToFloatBuffer(const FloatImage &image,
                              float *buf)
{
	for (int y = 0, a = 0; y < image.Height(); y++) {
		for (int x = 0; x < image.Width(); x++) {
			for (int k = 0; k < image.Depth(); k++) {
				buf[a++] = image(y, x, k);
			}
		}
	}
}

void libmv_imageToByteBuffer(const libmv::FloatImage &image,
                             unsigned char *buf)
{
	for (int y = 0, a= 0; y < image.Height(); y++) {
		for (int x = 0; x < image.Width(); x++) {
			for (int k = 0; k < image.Depth(); k++) {
				buf[a++] = image(y, x, k) * 255.0f;
			}
		}
	}
}

/* Debugging */

static void savePNGImage(png_bytep *row_pointers,
                         int width, int height, int depth, int color_type,
                         const char *file_name)
{
	png_infop info_ptr;
	png_structp png_ptr;
	FILE *fp = fopen(file_name, "wb");

	if (!fp) {
		return;
    }

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

	png_set_IHDR(png_ptr, info_ptr,
	             width, height, depth, color_type,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_BASE,
	             PNG_FILTER_TYPE_BASE);

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

void libmv_saveImage(const FloatImage &image,
					 const char *prefix,
					 int x0, int y0)
{
	int x, y;
	png_bytep *row_pointers;

	assert(image.Depth() == 1);

	row_pointers = new png_bytep[image.Height()];

	for (y = 0; y < image.Height(); y++) {
		row_pointers[y] = new png_byte[4 * image.Width()];

		for (x = 0; x < image.Width(); x++) {
			if (x0 == x && image.Height() - y0 - 1 == y) {
				row_pointers[y][x * 4 + 0] = 255;
				row_pointers[y][x * 4 + 1] = 0;
				row_pointers[y][x * 4 + 2] = 0;
				row_pointers[y][x * 4 + 3] = 255;
			}
			else {
				float pixel = image(image.Height() - y - 1, x, 0);
				row_pointers[y][x * 4 + 0] = pixel * 255;
				row_pointers[y][x * 4 + 1] = pixel * 255;
				row_pointers[y][x * 4 + 2] = pixel * 255;
				row_pointers[y][x * 4 + 3] = 255;
			}
		}
	}

	{
		static int a = 0;
		char buf[128];
		snprintf(buf, sizeof(buf), "%s_%02d.png", prefix, ++a);
		savePNGImage(row_pointers,
		             image.Width(), image.Height(), 8,
		             PNG_COLOR_TYPE_RGBA,
		             buf);
	}

	for (y = 0; y < image.Height(); y++) {
		delete [] row_pointers[y];
	}
	delete [] row_pointers;
}

/* Camera intrinsics utility functions */

void libmv_cameraIntrinsicsFillFromOptions(
	const libmv_CameraIntrinsicsOptions *camera_intrinsics_options,
	CameraIntrinsics *camera_intrinsics)
{
	camera_intrinsics->SetFocalLength(camera_intrinsics_options->focal_length,
	                                  camera_intrinsics_options->focal_length);

	camera_intrinsics->SetPrincipalPoint(
		camera_intrinsics_options->principal_point_x,
		camera_intrinsics_options->principal_point_y);

	camera_intrinsics->SetImageSize(camera_intrinsics_options->image_width,
	                                camera_intrinsics_options->image_height);

	switch (camera_intrinsics_options->distortion_model) {
		case LIBMV_DISTORTION_MODEL_POLYNOMIAL:
		{
			PolynomialCameraIntrinsics *polynomial_intrinsics =
				static_cast<PolynomialCameraIntrinsics*>(camera_intrinsics);

			polynomial_intrinsics->SetRadialDistortion(
				camera_intrinsics_options->polynomial_k1,
				camera_intrinsics_options->polynomial_k2,
				camera_intrinsics_options->polynomial_k3);

			break;
		}

		case LIBMV_DISTORTION_MODEL_DIVISION:
		{
			DivisionCameraIntrinsics *division_intrinsics =
				static_cast<DivisionCameraIntrinsics*>(camera_intrinsics);

			division_intrinsics->SetDistortion(
				camera_intrinsics_options->division_k1,
				camera_intrinsics_options->division_k2);

			break;
		}

		default:
			assert(!"Unknown distortion model");
	}
}

CameraIntrinsics *libmv_cameraIntrinsicsCreateFromOptions(
	const libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
	CameraIntrinsics *camera_intrinsics = NULL;

	switch (camera_intrinsics_options->distortion_model) {
		case LIBMV_DISTORTION_MODEL_POLYNOMIAL:
			camera_intrinsics = LIBMV_OBJECT_NEW(PolynomialCameraIntrinsics);
			break;

		case LIBMV_DISTORTION_MODEL_DIVISION:
			camera_intrinsics = LIBMV_OBJECT_NEW(DivisionCameraIntrinsics);
			break;

		default:
			assert(!"Unknown distortion model");
	}

	libmv_cameraIntrinsicsFillFromOptions(camera_intrinsics_options, camera_intrinsics);

	return camera_intrinsics;
}

/* Reconstruction utilities */

void libmv_getNormalizedTracks(const Tracks &tracks,
                               const CameraIntrinsics &camera_intrinsics,
                               Tracks *normalized_tracks)
{
	libmv::vector<Marker> markers = tracks.AllMarkers();

	for (int i = 0; i < markers.size(); ++i) {
		Marker &marker = markers[i];
		camera_intrinsics.InvertIntrinsics(marker.x, marker.y,
		                                   &marker.x, &marker.y);
		normalized_tracks->Insert(marker.image, marker.track,
		                          marker.x, marker.y,
		                          marker.weight);
	}
}

Marker libmv_projectMarker(const EuclideanPoint &point,
                           const EuclideanCamera &camera,
                           const CameraIntrinsics &intrinsics)
{
	libmv::Vec3 projected = camera.R * point.X + camera.t;
	projected /= projected(2);

	libmv::Marker reprojected_marker;
	intrinsics.ApplyIntrinsics(projected(0), projected(1),
	                           &reprojected_marker.x,
	                           &reprojected_marker.y);

	reprojected_marker.image = camera.image;
	reprojected_marker.track = point.track;

	return reprojected_marker;
}

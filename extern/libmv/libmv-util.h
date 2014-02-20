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

#ifndef LIBMV_UTIL_H
#define LIBMV_UTIL_H

#include "libmv-capi.h"
#include "libmv/image/image.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"
#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/reconstruction.h"

void libmv_byteBufferToImage(const unsigned char *buf,
                             int width, int height, int channels,
                             libmv::FloatImage *image);

void libmv_floatBufferToImage(const float *buf,
                              int width, int height, int channels,
                              libmv::FloatImage *image);

void libmv_imageToFloatBuffer(const libmv::FloatImage &image,
                              float *buf);

void libmv_imageToByteBuffer(const libmv::FloatImage &image,
                             unsigned char *buf);

void libmv_saveImage(const libmv::FloatImage &image,
                     const char *prefix,
                     int x0, int y0);

void libmv_cameraIntrinsicsFillFromOptions(
	const libmv_CameraIntrinsicsOptions *camera_intrinsics_options,
	libmv::CameraIntrinsics *camera_intrinsics);

libmv::CameraIntrinsics *libmv_cameraIntrinsicsCreateFromOptions(
	const libmv_CameraIntrinsicsOptions *camera_intrinsics_options);

void libmv_getNormalizedTracks(const libmv::Tracks &tracks,
                               const libmv::CameraIntrinsics &camera_intrinsics,
                               libmv::Tracks *normalized_tracks);

libmv::Marker libmv_projectMarker(const libmv::EuclideanPoint &point,
                                  const libmv::EuclideanCamera &camera,
                                  const libmv::CameraIntrinsics &intrinsics);

#endif

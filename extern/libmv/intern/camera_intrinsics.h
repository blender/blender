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

#ifndef LIBMV_C_API_CAMERA_INTRINSICS_H_
#define LIBMV_C_API_CAMERA_INTRINSICS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_CameraIntrinsics libmv_CameraIntrinsics;

enum {
  LIBMV_DISTORTION_MODEL_POLYNOMIAL = 0,
  LIBMV_DISTORTION_MODEL_DIVISION = 1,
};

typedef struct libmv_CameraIntrinsicsOptions {
  // Common settings of all distortion models.
  int distortion_model;
  int image_width, image_height;
  double focal_length;
  double principal_point_x, principal_point_y;

  // Radial distortion model.
  double polynomial_k1, polynomial_k2, polynomial_k3;
  double polynomial_p1, polynomial_p2;

  // Division distortion model.
  double division_k1, division_k2;
} libmv_CameraIntrinsicsOptions;

libmv_CameraIntrinsics *libmv_cameraIntrinsicsNew(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options);

libmv_CameraIntrinsics *libmv_cameraIntrinsicsCopy(
    const libmv_CameraIntrinsics* libmv_intrinsics);

void libmv_cameraIntrinsicsDestroy(libmv_CameraIntrinsics* libmv_intrinsics);
void libmv_cameraIntrinsicsUpdate(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    libmv_CameraIntrinsics* libmv_intrinsics);

void libmv_cameraIntrinsicsSetThreads(libmv_CameraIntrinsics* libmv_intrinsics,
                                      int threads);

void libmv_cameraIntrinsicsExtractOptions(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    libmv_CameraIntrinsicsOptions* camera_intrinsics_options);

void libmv_cameraIntrinsicsUndistortByte(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char *source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char* destination_image);

void libmv_cameraIntrinsicsUndistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image);

void libmv_cameraIntrinsicsDistortByte(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char *source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char *destination_image);

void libmv_cameraIntrinsicsDistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image);

void libmv_cameraIntrinsicsApply(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    double x,
    double y,
    double* x1,
    double* y1);

void libmv_cameraIntrinsicsInvert(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    double x,
    double y,
    double* x1,
    double* y1);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace libmv {
  class CameraIntrinsics;
}

libmv::CameraIntrinsics* libmv_cameraIntrinsicsCreateFromOptions(
    const libmv_CameraIntrinsicsOptions* camera_intrinsics_options);
#endif

#endif  // LIBMV_C_API_CAMERA_INTRINSICS_H_

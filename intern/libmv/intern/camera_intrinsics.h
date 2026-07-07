/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_CAMERA_INTRINSICS_H_
#define LIBMV_C_API_CAMERA_INTRINSICS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_CameraIntrinsics libmv_CameraIntrinsics;

enum {
  LIBMV_DISTORTION_MODEL_POLYNOMIAL = 0,
  LIBMV_DISTORTION_MODEL_DIVISION = 1,
  LIBMV_DISTORTION_MODEL_NUKE = 2,
  LIBMV_DISTORTION_MODEL_BROWN = 3,
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

  // Nuke distortion model.
  double nuke_k1, nuke_k2;
  double nuke_p1, nuke_p2;

  // Brown-Conrady distortion model.
  double brown_k1, brown_k2, brown_k3, brown_k4;
  double brown_p1, brown_p2;
} libmv_CameraIntrinsicsOptions;

libmv_CameraIntrinsics* libmv_cameraIntrinsicsNew(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options);

libmv_CameraIntrinsics* libmv_cameraIntrinsicsCopy(
    const libmv_CameraIntrinsics* libmv_intrinsics);

void libmv_cameraIntrinsicsDestroy(libmv_CameraIntrinsics* libmv_intrinsics);
void libmv_cameraIntrinsicsUpdate(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    libmv_CameraIntrinsics* libmv_intrinsics);

void libmv_cameraIntrinsicsExtractOptions(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    libmv_CameraIntrinsicsOptions* camera_intrinsics_options);

void libmv_cameraIntrinsicsUndistortByte(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char* source_image,
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
    const unsigned char* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char* destination_image);

void libmv_cameraIntrinsicsDistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image);

void libmv_cameraIntrinsicsApply(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    double x,
    double y,
    double* x1,
    double* y1);

void libmv_cameraIntrinsicsInvert(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
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

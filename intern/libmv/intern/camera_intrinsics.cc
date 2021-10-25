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

#include "intern/camera_intrinsics.h"
#include "intern/utildefines.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"

using libmv::CameraIntrinsics;
using libmv::DivisionCameraIntrinsics;
using libmv::PolynomialCameraIntrinsics;

libmv_CameraIntrinsics *libmv_cameraIntrinsicsNew(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options) {
  CameraIntrinsics *camera_intrinsics =
    libmv_cameraIntrinsicsCreateFromOptions(libmv_camera_intrinsics_options);
  return (libmv_CameraIntrinsics *) camera_intrinsics;
}

libmv_CameraIntrinsics *libmv_cameraIntrinsicsCopy(
    const libmv_CameraIntrinsics* libmv_intrinsics) {
  const CameraIntrinsics *orig_intrinsics =
    (const CameraIntrinsics *) libmv_intrinsics;

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

void libmv_cameraIntrinsicsDestroy(libmv_CameraIntrinsics* libmv_intrinsics) {
  LIBMV_OBJECT_DELETE(libmv_intrinsics, CameraIntrinsics);
}

void libmv_cameraIntrinsicsUpdate(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    libmv_CameraIntrinsics* libmv_intrinsics) {
  CameraIntrinsics *camera_intrinsics = (CameraIntrinsics *) libmv_intrinsics;

  double focal_length = libmv_camera_intrinsics_options->focal_length;
  double principal_x = libmv_camera_intrinsics_options->principal_point_x;
  double principal_y = libmv_camera_intrinsics_options->principal_point_y;
  int image_width = libmv_camera_intrinsics_options->image_width;
  int image_height = libmv_camera_intrinsics_options->image_height;

  /* Try avoid unnecessary updates, so pre-computed distortion grids
   * are not freed.
   */

  if (camera_intrinsics->focal_length() != focal_length) {
    camera_intrinsics->SetFocalLength(focal_length, focal_length);
  }

  if (camera_intrinsics->principal_point_x() != principal_x ||
      camera_intrinsics->principal_point_y() != principal_y) {
    camera_intrinsics->SetPrincipalPoint(principal_x, principal_y);
  }

  if (camera_intrinsics->image_width() != image_width ||
      camera_intrinsics->image_height() != image_height) {
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
            polynomial_intrinsics->k3() != k3) {
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
            division_intrinsics->k2() != k2) {
          division_intrinsics->SetDistortion(k1, k2);
        }

        break;
      }

    default:
      assert(!"Unknown distortion model");
  }
}

void libmv_cameraIntrinsicsSetThreads(libmv_CameraIntrinsics* libmv_intrinsics,
                                      int threads) {
  CameraIntrinsics *camera_intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  camera_intrinsics->SetThreads(threads);
}

void libmv_cameraIntrinsicsExtractOptions(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    libmv_CameraIntrinsicsOptions* camera_intrinsics_options) {
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
        camera_intrinsics_options->distortion_model =
          LIBMV_DISTORTION_MODEL_POLYNOMIAL;
        camera_intrinsics_options->polynomial_k1 = polynomial_intrinsics->k1();
        camera_intrinsics_options->polynomial_k2 = polynomial_intrinsics->k2();
        camera_intrinsics_options->polynomial_k3 = polynomial_intrinsics->k3();
        camera_intrinsics_options->polynomial_p1 = polynomial_intrinsics->p1();
        camera_intrinsics_options->polynomial_p2 = polynomial_intrinsics->p2();
        break;
      }

    case libmv::DISTORTION_MODEL_DIVISION:
      {
        const DivisionCameraIntrinsics *division_intrinsics =
          static_cast<const DivisionCameraIntrinsics *>(camera_intrinsics);
        camera_intrinsics_options->distortion_model =
          LIBMV_DISTORTION_MODEL_DIVISION;
        camera_intrinsics_options->division_k1 = division_intrinsics->k1();
        camera_intrinsics_options->division_k2 = division_intrinsics->k2();
        break;
      }

    default:
      assert(!"Unknown distortion model");
  }
}

void libmv_cameraIntrinsicsUndistortByte(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char *source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char* destination_image) {
  CameraIntrinsics *camera_intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  camera_intrinsics->UndistortBuffer(source_image,
                                     width, height,
                                     overscan,
                                     channels,
                                     destination_image);
}

void libmv_cameraIntrinsicsUndistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image) {
  CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  intrinsics->UndistortBuffer(source_image,
                              width, height,
                              overscan,
                              channels,
                              destination_image);
}

void libmv_cameraIntrinsicsDistortByte(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char *source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char *destination_image) {
  CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  intrinsics->DistortBuffer(source_image,
                            width, height,
                            overscan,
                            channels,
                            destination_image);
}

void libmv_cameraIntrinsicsDistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image) {
  CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  intrinsics->DistortBuffer(source_image,
                            width, height,
                            overscan,
                            channels,
                            destination_image);
}

void libmv_cameraIntrinsicsApply(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    double x,
    double y,
    double* x1,
    double* y1) {
  CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  intrinsics->ApplyIntrinsics(x, y, x1, y1);
}

void libmv_cameraIntrinsicsInvert(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    double x,
    double y,
    double* x1,
    double* y1) {
  CameraIntrinsics *intrinsics = (CameraIntrinsics *) libmv_intrinsics;
  intrinsics->InvertIntrinsics(x, y, x1, y1);
}

static void libmv_cameraIntrinsicsFillFromOptions(
    const libmv_CameraIntrinsicsOptions* camera_intrinsics_options,
    CameraIntrinsics* camera_intrinsics) {
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

CameraIntrinsics* libmv_cameraIntrinsicsCreateFromOptions(
    const libmv_CameraIntrinsicsOptions* camera_intrinsics_options) {
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
  libmv_cameraIntrinsicsFillFromOptions(camera_intrinsics_options,
                                        camera_intrinsics);
  return camera_intrinsics;
}

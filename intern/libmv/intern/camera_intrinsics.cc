/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "intern/camera_intrinsics.h"
#include "intern/utildefines.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"

using libmv::BrownCameraIntrinsics;
using libmv::CameraIntrinsics;
using libmv::DivisionCameraIntrinsics;
using libmv::NukeCameraIntrinsics;
using libmv::PolynomialCameraIntrinsics;

libmv_CameraIntrinsics* libmv_cameraIntrinsicsNew(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options) {
  CameraIntrinsics* camera_intrinsics =
      libmv_cameraIntrinsicsCreateFromOptions(libmv_camera_intrinsics_options);
  return (libmv_CameraIntrinsics*)camera_intrinsics;
}

libmv_CameraIntrinsics* libmv_cameraIntrinsicsCopy(
    const libmv_CameraIntrinsics* libmv_intrinsics) {
  const CameraIntrinsics* orig_intrinsics =
      (const CameraIntrinsics*)libmv_intrinsics;

  CameraIntrinsics* new_intrinsics = NULL;
  switch (orig_intrinsics->GetDistortionModelType()) {
    case libmv::DISTORTION_MODEL_POLYNOMIAL: {
      const PolynomialCameraIntrinsics* polynomial_intrinsics =
          static_cast<const PolynomialCameraIntrinsics*>(orig_intrinsics);
      new_intrinsics =
          LIBMV_OBJECT_NEW(PolynomialCameraIntrinsics, *polynomial_intrinsics);
      break;
    }
    case libmv::DISTORTION_MODEL_DIVISION: {
      const DivisionCameraIntrinsics* division_intrinsics =
          static_cast<const DivisionCameraIntrinsics*>(orig_intrinsics);
      new_intrinsics =
          LIBMV_OBJECT_NEW(DivisionCameraIntrinsics, *division_intrinsics);
      break;
    }
    case libmv::DISTORTION_MODEL_NUKE: {
      const NukeCameraIntrinsics* nuke_intrinsics =
          static_cast<const NukeCameraIntrinsics*>(orig_intrinsics);
      new_intrinsics = LIBMV_OBJECT_NEW(NukeCameraIntrinsics, *nuke_intrinsics);
      break;
    }
    case libmv::DISTORTION_MODEL_BROWN: {
      const BrownCameraIntrinsics* brown_intrinsics =
          static_cast<const BrownCameraIntrinsics*>(orig_intrinsics);
      new_intrinsics =
          LIBMV_OBJECT_NEW(BrownCameraIntrinsics, *brown_intrinsics);
      break;
    }
    default: assert(!"Unknown distortion model");
  }
  return (libmv_CameraIntrinsics*)new_intrinsics;
}

void libmv_cameraIntrinsicsDestroy(libmv_CameraIntrinsics* libmv_intrinsics) {
  LIBMV_OBJECT_DELETE(libmv_intrinsics, CameraIntrinsics);
}

void libmv_cameraIntrinsicsUpdate(
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    libmv_CameraIntrinsics* libmv_intrinsics) {
  CameraIntrinsics* camera_intrinsics = (CameraIntrinsics*)libmv_intrinsics;

  double focal_length = libmv_camera_intrinsics_options->focal_length;
  double principal_x = libmv_camera_intrinsics_options->principal_point_x;
  double principal_y = libmv_camera_intrinsics_options->principal_point_y;
  int image_width = libmv_camera_intrinsics_options->image_width;
  int image_height = libmv_camera_intrinsics_options->image_height;

  /* Try avoid unnecessary updates, so pre-computed distortion grids
   * are not freed. */

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
    case LIBMV_DISTORTION_MODEL_POLYNOMIAL: {
      assert(camera_intrinsics->GetDistortionModelType() ==
             libmv::DISTORTION_MODEL_POLYNOMIAL);

      PolynomialCameraIntrinsics* polynomial_intrinsics =
          (PolynomialCameraIntrinsics*)camera_intrinsics;

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

    case LIBMV_DISTORTION_MODEL_DIVISION: {
      assert(camera_intrinsics->GetDistortionModelType() ==
             libmv::DISTORTION_MODEL_DIVISION);

      DivisionCameraIntrinsics* division_intrinsics =
          (DivisionCameraIntrinsics*)camera_intrinsics;

      double k1 = libmv_camera_intrinsics_options->division_k1;
      double k2 = libmv_camera_intrinsics_options->division_k2;

      if (division_intrinsics->k1() != k1 || division_intrinsics->k2() != k2) {
        division_intrinsics->SetDistortion(k1, k2);
      }

      break;
    }

    case LIBMV_DISTORTION_MODEL_NUKE: {
      assert(camera_intrinsics->GetDistortionModelType() ==
             libmv::DISTORTION_MODEL_NUKE);

      NukeCameraIntrinsics* nuke_intrinsics =
          (NukeCameraIntrinsics*)camera_intrinsics;

      double k1 = libmv_camera_intrinsics_options->nuke_k1;
      double k2 = libmv_camera_intrinsics_options->nuke_k2;

      if (nuke_intrinsics->k1() != k1 || nuke_intrinsics->k2() != k2) {
        nuke_intrinsics->SetRadialDistortion(k1, k2);
      }

      double p1 = libmv_camera_intrinsics_options->nuke_p1;
      double p2 = libmv_camera_intrinsics_options->nuke_p2;

      if (nuke_intrinsics->p1() != p1 || nuke_intrinsics->p2() != p2) {
        nuke_intrinsics->SetTangentialDistortion(p1, p2);
      }
      break;
    }

    case LIBMV_DISTORTION_MODEL_BROWN: {
      assert(camera_intrinsics->GetDistortionModelType() ==
             libmv::DISTORTION_MODEL_BROWN);

      BrownCameraIntrinsics* brown_intrinsics =
          (BrownCameraIntrinsics*)camera_intrinsics;

      double k1 = libmv_camera_intrinsics_options->brown_k1;
      double k2 = libmv_camera_intrinsics_options->brown_k2;
      double k3 = libmv_camera_intrinsics_options->brown_k3;
      double k4 = libmv_camera_intrinsics_options->brown_k4;

      if (brown_intrinsics->k1() != k1 || brown_intrinsics->k2() != k2 ||
          brown_intrinsics->k3() != k3 || brown_intrinsics->k4() != k4) {
        brown_intrinsics->SetRadialDistortion(k1, k2, k3, k4);
      }

      double p1 = libmv_camera_intrinsics_options->brown_p1;
      double p2 = libmv_camera_intrinsics_options->brown_p2;

      if (brown_intrinsics->p1() != p1 || brown_intrinsics->p2() != p2) {
        brown_intrinsics->SetTangentialDistortion(p1, p2);
      }
      break;
    }

    default: assert(!"Unknown distortion model");
  }
}

void libmv_cameraIntrinsicsExtractOptions(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    libmv_CameraIntrinsicsOptions* camera_intrinsics_options) {
  const CameraIntrinsics* camera_intrinsics =
      (const CameraIntrinsics*)libmv_intrinsics;

  // Fill in options which are common for all distortion models.
  camera_intrinsics_options->focal_length = camera_intrinsics->focal_length();
  camera_intrinsics_options->principal_point_x =
      camera_intrinsics->principal_point_x();
  camera_intrinsics_options->principal_point_y =
      camera_intrinsics->principal_point_y();

  camera_intrinsics_options->image_width = camera_intrinsics->image_width();
  camera_intrinsics_options->image_height = camera_intrinsics->image_height();

  switch (camera_intrinsics->GetDistortionModelType()) {
    case libmv::DISTORTION_MODEL_POLYNOMIAL: {
      const PolynomialCameraIntrinsics* polynomial_intrinsics =
          static_cast<const PolynomialCameraIntrinsics*>(camera_intrinsics);
      camera_intrinsics_options->distortion_model =
          LIBMV_DISTORTION_MODEL_POLYNOMIAL;
      camera_intrinsics_options->polynomial_k1 = polynomial_intrinsics->k1();
      camera_intrinsics_options->polynomial_k2 = polynomial_intrinsics->k2();
      camera_intrinsics_options->polynomial_k3 = polynomial_intrinsics->k3();
      camera_intrinsics_options->polynomial_p1 = polynomial_intrinsics->p1();
      camera_intrinsics_options->polynomial_p2 = polynomial_intrinsics->p2();
      break;
    }

    case libmv::DISTORTION_MODEL_DIVISION: {
      const DivisionCameraIntrinsics* division_intrinsics =
          static_cast<const DivisionCameraIntrinsics*>(camera_intrinsics);
      camera_intrinsics_options->distortion_model =
          LIBMV_DISTORTION_MODEL_DIVISION;
      camera_intrinsics_options->division_k1 = division_intrinsics->k1();
      camera_intrinsics_options->division_k2 = division_intrinsics->k2();
      break;
    }

    case libmv::DISTORTION_MODEL_NUKE: {
      const NukeCameraIntrinsics* nuke_intrinsics =
          static_cast<const NukeCameraIntrinsics*>(camera_intrinsics);
      camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_NUKE;
      camera_intrinsics_options->nuke_k1 = nuke_intrinsics->k1();
      camera_intrinsics_options->nuke_k2 = nuke_intrinsics->k2();
      camera_intrinsics_options->nuke_p1 = nuke_intrinsics->p1();
      camera_intrinsics_options->nuke_p2 = nuke_intrinsics->p2();
      break;
    }

    case libmv::DISTORTION_MODEL_BROWN: {
      const BrownCameraIntrinsics* brown_intrinsics =
          static_cast<const BrownCameraIntrinsics*>(camera_intrinsics);
      camera_intrinsics_options->distortion_model =
          LIBMV_DISTORTION_MODEL_BROWN;
      camera_intrinsics_options->brown_k1 = brown_intrinsics->k1();
      camera_intrinsics_options->brown_k2 = brown_intrinsics->k2();
      camera_intrinsics_options->brown_k3 = brown_intrinsics->k3();
      camera_intrinsics_options->brown_k4 = brown_intrinsics->k4();
      camera_intrinsics_options->brown_p1 = brown_intrinsics->p1();
      camera_intrinsics_options->brown_p2 = brown_intrinsics->p2();
      break;
    }

    default: assert(!"Unknown distortion model");
  }
}

void libmv_cameraIntrinsicsUndistortByte(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char* destination_image) {
  CameraIntrinsics* camera_intrinsics = (CameraIntrinsics*)libmv_intrinsics;
  camera_intrinsics->UndistortBuffer(
      source_image, width, height, overscan, channels, destination_image);
}

void libmv_cameraIntrinsicsUndistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    const float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image) {
  CameraIntrinsics* intrinsics = (CameraIntrinsics*)libmv_intrinsics;
  intrinsics->UndistortBuffer(
      source_image, width, height, overscan, channels, destination_image);
}

void libmv_cameraIntrinsicsDistortByte(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    const unsigned char* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    unsigned char* destination_image) {
  CameraIntrinsics* intrinsics = (CameraIntrinsics*)libmv_intrinsics;
  intrinsics->DistortBuffer(
      source_image, width, height, overscan, channels, destination_image);
}

void libmv_cameraIntrinsicsDistortFloat(
    const libmv_CameraIntrinsics* libmv_intrinsics,
    float* source_image,
    int width,
    int height,
    float overscan,
    int channels,
    float* destination_image) {
  CameraIntrinsics* intrinsics = (CameraIntrinsics*)libmv_intrinsics;
  intrinsics->DistortBuffer(
      source_image, width, height, overscan, channels, destination_image);
}

void libmv_cameraIntrinsicsApply(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    double x,
    double y,
    double* x1,
    double* y1) {
  CameraIntrinsics* intrinsics = (CameraIntrinsics*)libmv_intrinsics;
  intrinsics->ApplyIntrinsics(x, y, x1, y1);
}

void libmv_cameraIntrinsicsInvert(
    const struct libmv_CameraIntrinsics* libmv_intrinsics,
    double x,
    double y,
    double* x1,
    double* y1) {
  CameraIntrinsics* intrinsics = (CameraIntrinsics*)libmv_intrinsics;
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
    case LIBMV_DISTORTION_MODEL_POLYNOMIAL: {
      PolynomialCameraIntrinsics* polynomial_intrinsics =
          static_cast<PolynomialCameraIntrinsics*>(camera_intrinsics);

      polynomial_intrinsics->SetRadialDistortion(
          camera_intrinsics_options->polynomial_k1,
          camera_intrinsics_options->polynomial_k2,
          camera_intrinsics_options->polynomial_k3);

      break;
    }

    case LIBMV_DISTORTION_MODEL_DIVISION: {
      DivisionCameraIntrinsics* division_intrinsics =
          static_cast<DivisionCameraIntrinsics*>(camera_intrinsics);

      division_intrinsics->SetDistortion(
          camera_intrinsics_options->division_k1,
          camera_intrinsics_options->division_k2);
      break;
    }

    case LIBMV_DISTORTION_MODEL_NUKE: {
      NukeCameraIntrinsics* nuke_intrinsics =
          static_cast<NukeCameraIntrinsics*>(camera_intrinsics);

      nuke_intrinsics->SetRadialDistortion(camera_intrinsics_options->nuke_k1,
                                           camera_intrinsics_options->nuke_k2);
      nuke_intrinsics->SetTangentialDistortion(
          camera_intrinsics_options->nuke_p1,
          camera_intrinsics_options->nuke_p2);
      break;
    }

    case LIBMV_DISTORTION_MODEL_BROWN: {
      BrownCameraIntrinsics* brown_intrinsics =
          static_cast<BrownCameraIntrinsics*>(camera_intrinsics);

      brown_intrinsics->SetRadialDistortion(
          camera_intrinsics_options->brown_k1,
          camera_intrinsics_options->brown_k2,
          camera_intrinsics_options->brown_k3,
          camera_intrinsics_options->brown_k4);
      brown_intrinsics->SetTangentialDistortion(
          camera_intrinsics_options->brown_p1,
          camera_intrinsics_options->brown_p2);

      break;
    }

    default: assert(!"Unknown distortion model");
  }
}

CameraIntrinsics* libmv_cameraIntrinsicsCreateFromOptions(
    const libmv_CameraIntrinsicsOptions* camera_intrinsics_options) {
  CameraIntrinsics* camera_intrinsics = NULL;
  switch (camera_intrinsics_options->distortion_model) {
    case LIBMV_DISTORTION_MODEL_POLYNOMIAL:
      camera_intrinsics = LIBMV_OBJECT_NEW(PolynomialCameraIntrinsics);
      break;
    case LIBMV_DISTORTION_MODEL_DIVISION:
      camera_intrinsics = LIBMV_OBJECT_NEW(DivisionCameraIntrinsics);
      break;
    case LIBMV_DISTORTION_MODEL_NUKE:
      camera_intrinsics = LIBMV_OBJECT_NEW(NukeCameraIntrinsics);
      break;
    case LIBMV_DISTORTION_MODEL_BROWN:
      camera_intrinsics = LIBMV_OBJECT_NEW(BrownCameraIntrinsics);
      break;
    default: assert(!"Unknown distortion model");
  }
  libmv_cameraIntrinsicsFillFromOptions(camera_intrinsics_options,
                                        camera_intrinsics);
  return camera_intrinsics;
}

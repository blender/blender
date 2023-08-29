/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_RECONSTRUCTION_H_
#define LIBMV_C_API_RECONSTRUCTION_H_

#ifdef __cplusplus
extern "C" {
#endif

struct libmv_Tracks;
struct libmv_CameraIntrinsics;
struct libmv_CameraIntrinsicsOptions;

typedef struct libmv_Reconstruction libmv_Reconstruction;

enum {
  LIBMV_REFINE_FOCAL_LENGTH = (1 << 0),
  LIBMV_REFINE_PRINCIPAL_POINT = (1 << 1),

  LIBMV_REFINE_RADIAL_DISTORTION_K1 = (1 << 2),
  LIBMV_REFINE_RADIAL_DISTORTION_K2 = (1 << 3),
  LIBMV_REFINE_RADIAL_DISTORTION_K3 = (1 << 4),
  LIBMV_REFINE_RADIAL_DISTORTION_K4 = (1 << 5),
  LIBMV_REFINE_RADIAL_DISTORTION =
      (LIBMV_REFINE_RADIAL_DISTORTION_K1 | LIBMV_REFINE_RADIAL_DISTORTION_K2 |
       LIBMV_REFINE_RADIAL_DISTORTION_K3 | LIBMV_REFINE_RADIAL_DISTORTION_K4),

  LIBMV_REFINE_TANGENTIAL_DISTORTION_P1 = (1 << 6),
  LIBMV_REFINE_TANGENTIAL_DISTORTION_P2 = (1 << 7),
  LIBMV_REFINE_TANGENTIAL_DISTORTION = (LIBMV_REFINE_TANGENTIAL_DISTORTION_P1 |
                                        LIBMV_REFINE_TANGENTIAL_DISTORTION_P2),
};

typedef struct libmv_ReconstructionOptions {
  int select_keyframes;
  int keyframe1, keyframe2;
  int refine_intrinsics;
} libmv_ReconstructionOptions;

typedef void (*reconstruct_progress_update_cb)(void* customdata,
                                               double progress,
                                               const char* message);

libmv_Reconstruction* libmv_solveReconstruction(
    const struct libmv_Tracks* libmv_tracks,
    const struct libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    libmv_ReconstructionOptions* libmv_reconstruction_options,
    reconstruct_progress_update_cb progress_update_callback,
    void* callback_customdata);

libmv_Reconstruction* libmv_solveModal(
    const struct libmv_Tracks* libmv_tracks,
    const struct libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    const libmv_ReconstructionOptions* libmv_reconstruction_options,
    reconstruct_progress_update_cb progress_update_callback,
    void* callback_customdata);

int libmv_reconstructionIsValid(libmv_Reconstruction* libmv_reconstruction);

void libmv_reconstructionDestroy(libmv_Reconstruction* libmv_reconstruction);

int libmv_reprojectionPointForTrack(
    const libmv_Reconstruction* libmv_reconstruction, int track, double pos[3]);

double libmv_reprojectionErrorForTrack(
    const libmv_Reconstruction* libmv_reconstruction, int track);

double libmv_reprojectionErrorForImage(
    const libmv_Reconstruction* libmv_reconstruction, int image);

int libmv_reprojectionCameraForImage(
    const libmv_Reconstruction* libmv_reconstruction,
    int image,
    double mat[4][4]);

double libmv_reprojectionError(
    const libmv_Reconstruction* libmv_reconstruction);

struct libmv_CameraIntrinsics* libmv_reconstructionExtractIntrinsics(
    libmv_Reconstruction* libmv_Reconstruction);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_RECONSTRUCTION_H_

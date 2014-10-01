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
  LIBMV_REFINE_FOCAL_LENGTH         = (1 << 0),
  LIBMV_REFINE_PRINCIPAL_POINT      = (1 << 1),
  LIBMV_REFINE_RADIAL_DISTORTION_K1 = (1 << 2),
  LIBMV_REFINE_RADIAL_DISTORTION_K2 = (1 << 4),
};

typedef struct libmv_ReconstructionOptions {
  int select_keyframes;
  int keyframe1, keyframe2;
  int refine_intrinsics;
} libmv_ReconstructionOptions;

typedef void (*reconstruct_progress_update_cb) (void* customdata,
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

void libmv_reconstructionDestroy(libmv_Reconstruction* libmv_reconstruction);

int libmv_reprojectionPointForTrack(
    const libmv_Reconstruction* libmv_reconstruction,
    int track,
    double pos[3]);

double libmv_reprojectionErrorForTrack(
    const libmv_Reconstruction* libmv_reconstruction,
    int track);

double libmv_reprojectionErrorForImage(
    const libmv_Reconstruction* libmv_reconstruction,
    int image);

int libmv_reprojectionCameraForImage(
    const libmv_Reconstruction* libmv_reconstruction,
    int image,
    double mat[4][4]);

double libmv_reprojectionError(const libmv_Reconstruction* libmv_reconstruction);

struct libmv_CameraIntrinsics* libmv_reconstructionExtractIntrinsics(
    libmv_Reconstruction *libmv_Reconstruction);

#ifdef __cplusplus
}
#endif

#endif   // LIBMV_C_API_RECONSTRUCTION_H_

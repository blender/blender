/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct ProjCameraInfo;

/**
 * Create UV info from the camera, needs to be freed.
 *
 * \param rotmat: can be `obedit->object_to_world` when uv project is used.
 * \param winx, winy: can be from `scene->r.xsch / ysch`.
 */
struct ProjCameraInfo *BLI_uvproject_camera_info(struct Object *ob,
                                                 float rotmat[4][4],
                                                 float winx,
                                                 float winy);

/**
 * Apply UV from #ProjCameraInfo (camera).
 */
void BLI_uvproject_from_camera(float target[2], float source[3], struct ProjCameraInfo *uci);

/**
 * Apply uv from perspective matrix.
 * \param persmat: Can be `rv3d->persmat`.
 */
void BLI_uvproject_from_view(float target[2],
                             float source[3],
                             float persmat[4][4],
                             float rotmat[4][4],
                             float winx,
                             float winy);

/**
 * Apply orthographic UVs.
 */
void BLI_uvproject_from_view_ortho(float target[2], float source[3], const float rotmat[4][4]);

/**
 * So we can adjust scale with keeping the struct private.
 */
void BLI_uvproject_camera_info_scale(struct ProjCameraInfo *uci, float scale_x, float scale_y);

#ifdef __cplusplus
}
#endif

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
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BLI_UVPROJECT_H__
#define __BLI_UVPROJECT_H__

/** \file BLI_uvproject.h
 *  \ingroup bli
 */

struct ProjCameraInfo;
struct Object;

/* create uv info from the camera, needs to be freed */
struct ProjCameraInfo *BLI_uvproject_camera_info(struct Object *ob, float rotmat[4][4], float winx, float winy);

/* apply uv from uvinfo (camera) */
void BLI_uvproject_from_camera(float target[2], float source[3], struct ProjCameraInfo *uci);

/* apply uv from perspective matrix */
void BLI_uvproject_from_view(float target[2], float source[3], float persmat[4][4], float rotmat[4][4], float winx, float winy);

/* apply ortho uv's */
void BLI_uvproject_from_view_ortho(float target[2], float source[3], float rotmat[4][4]);

/* so we can adjust scale with keeping the struct private */
void BLI_uvproject_camera_info_scale(struct ProjCameraInfo *uci, float scale_x, float scale_y);

#endif

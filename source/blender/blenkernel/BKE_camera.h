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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_CAMERA_H
#define BKE_CAMERA_H

/** \file BKE_camera.h
 *  \ingroup bke
 *  \brief Camera datablock and utility functions.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct Camera;
struct Object;
struct RenderData;
struct Scene;
struct rctf;

void *add_camera(const char *name);
struct Camera *copy_camera(struct Camera *cam);
void make_local_camera(struct Camera *cam);
void free_camera(struct Camera *ca);

float dof_camera(struct Object *ob);

void object_camera_mode(struct RenderData *rd, struct Object *camera);
void object_camera_intrinsics(struct Object *camera, struct Camera **cam_r, short *is_ortho, float *shiftx, float *shifty,
			float *clipsta, float *clipend, float *lens, float *sensor_x, float *sensor_y, short *sensor_fit);
void object_camera_matrix(
		struct RenderData *rd, struct Object *camera, int winx, int winy, short field_second,
		float winmat[][4], struct rctf *viewplane, float *clipsta, float *clipend, float *lens,
		float *sensor_x, float *sensor_y, short *sensor_fit, float *ycor,
		float *viewdx, float *viewdy);

void camera_view_frame_ex(struct Scene *scene, struct Camera *camera, float drawsize, const short do_clip, const float scale[3],
                          float r_asp[2], float r_shift[2], float *r_drawsize, float r_vec[4][3]);

void camera_view_frame(struct Scene *scene, struct Camera *camera, float r_vec[4][3]);

#ifdef __cplusplus
}
#endif

#endif


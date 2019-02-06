/*
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup eduv
 */


#ifndef __UVEDIT_INTERN_H__
#define __UVEDIT_INTERN_H__

struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct Image;
struct Object;
struct Scene;
struct SpaceImage;
struct wmOperatorType;

/* geometric utilities */
void  uv_poly_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy, int len);
void  uv_poly_center(struct BMFace *f, float r_cent[2], const int cd_loop_uv_offset);

/* find nearest */

typedef struct UvNearestHit {
	/** Only for `*_multi(..)` versions of functions. */
	struct Object *ob;
	/** Always set if we have a hit. */
	struct BMFace *efa;
	struct BMLoop *l;
	struct MLoopUV *luv, *luv_next;
	/** Index of loop within face. */
	int lindex;
	/** Needs to be set before calling nearest functions. */
	float dist_sq;
} UvNearestHit;

#define UV_NEAREST_HIT_INIT { .dist_sq = FLT_MAX, }

bool uv_find_nearest_vert(
        struct Scene *scene, struct Image *ima, struct Object *obedit,
        const float co[2], const float penalty_dist, struct UvNearestHit *hit_final);
bool uv_find_nearest_vert_multi(
        struct Scene *scene, struct Image *ima, struct Object **objects, const uint objects_len,
        const float co[2], const float penalty_dist, struct UvNearestHit *hit_final);

bool uv_find_nearest_edge(
        struct Scene *scene, struct Image *ima, struct Object *obedit,
        const float co[2], struct UvNearestHit *hit_final);
bool uv_find_nearest_edge_multi(
        struct Scene *scene, struct Image *ima, struct Object **objects, const uint objects_len,
        const float co[2], struct UvNearestHit *hit_final);

bool uv_find_nearest_face(
        struct Scene *scene, struct Image *ima, struct Object *obedit,
        const float co[2], struct UvNearestHit *hit_final);
bool uv_find_nearest_face_multi(
        struct Scene *scene, struct Image *ima, struct Object **objects, const uint objects_len,
        const float co[2], struct UvNearestHit *hit_final);

/* utility tool functions */

void uvedit_live_unwrap_update(struct SpaceImage *sima, struct Scene *scene, struct Object *obedit);

/* operators */

void UV_OT_average_islands_scale(struct wmOperatorType *ot);
void UV_OT_cube_project(struct wmOperatorType *ot);
void UV_OT_cylinder_project(struct wmOperatorType *ot);
void UV_OT_project_from_view(struct wmOperatorType *ot);
void UV_OT_minimize_stretch(struct wmOperatorType *ot);
void UV_OT_pack_islands(struct wmOperatorType *ot);
void UV_OT_reset(struct wmOperatorType *ot);
void UV_OT_sphere_project(struct wmOperatorType *ot);
void UV_OT_unwrap(struct wmOperatorType *ot);
void UV_OT_stitch(struct wmOperatorType *ot);

#endif /* __UVEDIT_INTERN_H__ */

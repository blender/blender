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

/** \file ED_transform_snap_object_context.h
 *  \ingroup editors
 */

#ifndef __ED_TRANSFORM_SNAP_OBJECT_CONTEXT_H__
#define __ED_TRANSFORM_SNAP_OBJECT_CONTEXT_H__

struct BMVert;
struct BMEdge;
struct BMFace;

struct ListBase;
struct Scene;
struct Main;
struct Object;
struct ARegion;
struct View3D;

/* transform_snap_object.c */

/* ED_transform_snap_object_*** API */

/** used for storing multiple hits */
struct SnapObjectHitDepth {
	struct SnapObjectHitDepth *next, *prev;

	float depth;
	float co[3];
	float no[3];
	int index;

	struct Object *ob;
	float obmat[4][4];

	/* needed to tell which ray-cast this was part of,
	 * the same object may be part of many ray-casts when dupli's are used. */
	unsigned int ob_uuid;
};

struct SnapObjectParams {
	int snap_select;  /* SnapSelect */
	union {
		unsigned int snap_to : 4;
		/* snap_target_flag: Snap to vert/edge/face. */
		unsigned int snap_to_flag : 4;
	};
	/* use editmode cage */
	unsigned int use_object_edit : 1;
	/* special context sensitive handling for the active object */
	unsigned int use_object_active : 1;
};

enum {
	SNAP_OBJECT_USE_CACHE = (1 << 0),
};

typedef struct SnapObjectContext SnapObjectContext;
SnapObjectContext *ED_transform_snap_object_context_create(
        struct Main *bmain, struct Scene *scene, int flag);
SnapObjectContext *ED_transform_snap_object_context_create_view3d(
        struct Main *bmain, struct Scene *scene, int flag,
        /* extra args for view3d */
        struct ARegion *ar, struct View3D *v3d);
void ED_transform_snap_object_context_destroy(SnapObjectContext *sctx);

/* callbacks to filter how snap works */
void ED_transform_snap_object_context_set_editmesh_callbacks(
        SnapObjectContext *sctx,
        bool (*test_vert_fn)(struct BMVert *, void *user_data),
        bool (*test_edge_fn)(struct BMEdge *, void *user_data),
        bool (*test_face_fn)(struct BMFace *, void *user_data),
        void *user_data);

bool ED_transform_snap_object_project_ray_ex(
        struct SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        struct Object **r_ob, float r_obmat[4][4]);
bool ED_transform_snap_object_project_ray(
        SnapObjectContext *sctx,
        const float ray_origin[3], const float ray_direction[3], float *ray_depth,
        float r_co[3], float r_no[3]);

bool ED_transform_snap_object_project_ray_all(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3],
        float ray_depth, bool sort,
        struct ListBase *r_hit_list);

bool ED_transform_snap_object_project_view3d_ex(
        struct SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index);
bool ED_transform_snap_object_project_view3d(
        struct SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3]);
bool ED_transform_snap_object_project_view3d_mixed(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval_fl[2], float *dist_px,
        bool use_depth,
        float r_co[3], float r_no[3]);

bool ED_transform_snap_object_project_all_view3d_ex(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2],
        float ray_depth, bool sort,
        ListBase *r_hit_list);

#endif  /* __ED_TRANSFORM_SNAP_OBJECT_CONTEXT_H__ */

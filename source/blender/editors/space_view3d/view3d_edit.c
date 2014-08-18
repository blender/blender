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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_edit.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_font.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_action.h"
#include "BKE_depsgraph.h" /* for ED_view3d_camera_lock_sync */


#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_sculpt.h"

#include "UI_resources.h"

#include "PIL_time.h" /* smoothview */

#include "view3d_intern.h"  /* own include */

bool ED_view3d_offset_lock_check(struct View3D *v3d, struct RegionView3D *rv3d)
{
	return (rv3d->persp != RV3D_CAMOB) && (v3d->ob_centre_cursor || v3d->ob_centre);
}

static bool view3d_operator_offset_lock_check(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	if (ED_view3d_offset_lock_check(v3d, rv3d)) {
		BKE_report(op->reports, RPT_WARNING, "View offset is locked");
		return true;
	}
	else {
		return false;
	}
}

/* ********************** view3d_edit: view manipulations ********************* */

bool ED_view3d_camera_lock_check(View3D *v3d, RegionView3D *rv3d)
{
	return ((v3d->camera) &&
	        (v3d->camera->id.lib == NULL) &&
	        (v3d->flag2 & V3D_LOCK_CAMERA) &&
	        (rv3d->persp == RV3D_CAMOB));
}

void ED_view3d_camera_lock_init_ex(View3D *v3d, RegionView3D *rv3d, const bool calc_dist)
{
	if (ED_view3d_camera_lock_check(v3d, rv3d)) {
		if (calc_dist) {
			/* using a fallback dist is OK here since ED_view3d_from_object() compensates for it */
			rv3d->dist = ED_view3d_offset_distance(v3d->camera->obmat, rv3d->ofs, VIEW3D_DIST_FALLBACK);
		}
		ED_view3d_from_object(v3d->camera, rv3d->ofs, rv3d->viewquat, &rv3d->dist, NULL);
	}
}

void ED_view3d_camera_lock_init(View3D *v3d, RegionView3D *rv3d)
{
	ED_view3d_camera_lock_init_ex(v3d, rv3d, true);
}

/* return true if the camera is moved */
bool ED_view3d_camera_lock_sync(View3D *v3d, RegionView3D *rv3d)
{
	if (ED_view3d_camera_lock_check(v3d, rv3d)) {
		ObjectTfmProtectedChannels obtfm;
		Object *root_parent;

		if ((U.uiflag & USER_CAM_LOCK_NO_PARENT) == 0 && (root_parent = v3d->camera->parent)) {
			Object *ob_update;
			float tmat[4][4];
			float imat[4][4];
			float view_mat[4][4];
			float diff_mat[4][4];
			float parent_mat[4][4];

			while (root_parent->parent) {
				root_parent = root_parent->parent;
			}

			ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);

			normalize_m4_m4(tmat, v3d->camera->obmat);

			invert_m4_m4(imat, tmat);
			mul_m4_m4m4(diff_mat, view_mat, imat);

			mul_m4_m4m4(parent_mat, diff_mat, root_parent->obmat);

			BKE_object_tfm_protected_backup(root_parent, &obtfm);
			BKE_object_apply_mat4(root_parent, parent_mat, true, false);
			BKE_object_tfm_protected_restore(root_parent, &obtfm, root_parent->protectflag);

			ob_update = v3d->camera;
			while (ob_update) {
				DAG_id_tag_update(&ob_update->id, OB_RECALC_OB);
				WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, ob_update);
				ob_update = ob_update->parent;
			}
		}
		else {
			/* always maintain the same scale */
			const short protect_scale_all = (OB_LOCK_SCALEX | OB_LOCK_SCALEY | OB_LOCK_SCALEZ);
			BKE_object_tfm_protected_backup(v3d->camera, &obtfm);
			ED_view3d_to_object(v3d->camera, rv3d->ofs, rv3d->viewquat, rv3d->dist);
			BKE_object_tfm_protected_restore(v3d->camera, &obtfm, v3d->camera->protectflag | protect_scale_all);

			DAG_id_tag_update(&v3d->camera->id, OB_RECALC_OB);
			WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, v3d->camera);
		}

		return true;
	}
	else {
		return false;
	}
}

bool ED_view3d_camera_autokey(
        Scene *scene, ID *id_key,
        struct bContext *C, const bool do_rotate, const bool do_translate)
{
	if (autokeyframe_cfra_can_key(scene, id_key)) {
		ListBase dsources = {NULL, NULL};

		/* add data-source override for the camera object */
		ANIM_relative_keyingset_add_source(&dsources, id_key, NULL, NULL);

		/* insert keyframes
		 * 1) on the first frame
		 * 2) on each subsequent frame
		 *    TODO: need to check in future that frame changed before doing this
		 */
		if (do_rotate) {
			struct KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		}
		if (do_translate) {
			struct KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		}

		/* free temp data */
		BLI_freelistN(&dsources);

		return true;
	}
	else {
		return false;
	}
}

/**
 * Call after modifying a locked view.
 *
 * \note Not every view edit currently auto-keys (numpad for eg),
 * this is complicated because of smoothview.
 */
bool ED_view3d_camera_lock_autokey(
        View3D *v3d, RegionView3D *rv3d,
        struct bContext *C, const bool do_rotate, const bool do_translate)
{
	/* similar to ED_view3d_cameracontrol_update */
	if (ED_view3d_camera_lock_check(v3d, rv3d)) {
		Scene *scene = CTX_data_scene(C);
		ID *id_key;
		Object *root_parent;
		if ((U.uiflag & USER_CAM_LOCK_NO_PARENT) == 0 && (root_parent = v3d->camera->parent)) {
			while (root_parent->parent) {
				root_parent = root_parent->parent;
			}
			id_key = &root_parent->id;
		}
		else {
			id_key = &v3d->camera->id;
		}

		return ED_view3d_camera_autokey(scene, id_key, C, do_rotate, do_translate);
	}
	else {
		return false;
	}
}

/**
 * For viewport operators that exit camera persp.
 *
 * \note This differs from simply setting ``rv3d->persp = persp`` because it
 * sets the ``ofs`` and ``dist`` values of the viewport so it matches the camera,
 * otherwise switching out of camera view may jump to a different part of the scene.
 */
static void view3d_persp_switch_from_camera(View3D *v3d, RegionView3D *rv3d, const char persp)
{
	BLI_assert(rv3d->persp == RV3D_CAMOB);
	BLI_assert(persp != RV3D_CAMOB);

	if (v3d->camera) {
		rv3d->dist = ED_view3d_offset_distance(v3d->camera->obmat, rv3d->ofs, VIEW3D_DIST_FALLBACK);
		ED_view3d_from_object(v3d->camera, rv3d->ofs, rv3d->viewquat, &rv3d->dist, NULL);
	}

	if (!ED_view3d_camera_lock_check(v3d, rv3d)) {
		rv3d->persp = persp;
	}
}

/* ********************* box view support ***************** */

static void view3d_boxview_clip(ScrArea *sa)
{
	ARegion *ar;
	BoundBox *bb = MEM_callocN(sizeof(BoundBox), "clipbb");
	float clip[6][4];
	float x1 = 0.0f, y1 = 0.0f, z1 = 0.0f, ofs[3] = {0.0f, 0.0f, 0.0f};
	int val;

	/* create bounding box */
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3d = ar->regiondata;

			if (rv3d->viewlock & RV3D_BOXCLIP) {
				if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
					if (ar->winx > ar->winy) x1 = rv3d->dist;
					else x1 = ar->winx * rv3d->dist / ar->winy;

					if (ar->winx > ar->winy) y1 = ar->winy * rv3d->dist / ar->winx;
					else y1 = rv3d->dist;
					copy_v2_v2(ofs, rv3d->ofs);
				}
				else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
					ofs[2] = rv3d->ofs[2];

					if (ar->winx > ar->winy) z1 = ar->winy * rv3d->dist / ar->winx;
					else z1 = rv3d->dist;
				}
			}
		}
	}

	for (val = 0; val < 8; val++) {
		if (ELEM(val, 0, 3, 4, 7))
			bb->vec[val][0] = -x1 - ofs[0];
		else
			bb->vec[val][0] =  x1 - ofs[0];

		if (ELEM(val, 0, 1, 4, 5))
			bb->vec[val][1] = -y1 - ofs[1];
		else
			bb->vec[val][1] =  y1 - ofs[1];

		if (val > 3)
			bb->vec[val][2] = -z1 - ofs[2];
		else
			bb->vec[val][2] =  z1 - ofs[2];
	}

	/* normals for plane equations */
	normal_tri_v3(clip[0], bb->vec[0], bb->vec[1], bb->vec[4]);
	normal_tri_v3(clip[1], bb->vec[1], bb->vec[2], bb->vec[5]);
	normal_tri_v3(clip[2], bb->vec[2], bb->vec[3], bb->vec[6]);
	normal_tri_v3(clip[3], bb->vec[3], bb->vec[0], bb->vec[7]);
	normal_tri_v3(clip[4], bb->vec[4], bb->vec[5], bb->vec[6]);
	normal_tri_v3(clip[5], bb->vec[0], bb->vec[2], bb->vec[1]);

	/* then plane equations */
	for (val = 0; val < 6; val++) {
		clip[val][3] = -dot_v3v3(clip[val], bb->vec[val % 5]);
	}

	/* create bounding box */
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3d = ar->regiondata;

			if (rv3d->viewlock & RV3D_BOXCLIP) {
				rv3d->rflag |= RV3D_CLIPPING;
				memcpy(rv3d->clip, clip, sizeof(clip));
				if (rv3d->clipbb) MEM_freeN(rv3d->clipbb);
				rv3d->clipbb = MEM_dupallocN(bb);
			}
		}
	}
	MEM_freeN(bb);
}

/**
 * Find which axis values are shared between both views and copy to \a rv3d_dst
 * taking axis flipping into account.
 */
static void view3d_boxview_sync_axis(RegionView3D *rv3d_dst, RegionView3D *rv3d_src)
{
	/* absolute axis values above this are considered to be set (will be ~1.0f) */
	const float axis_eps = 0.5f;
	float viewinv[4];

	/* use the view rotation to identify which axis to sync on */
	float view_axis_all[4][3] = {
	    {1.0f, 0.0f, 0.0f},
	    {0.0f, 1.0f, 0.0f},
	    {1.0f, 0.0f, 0.0f},
	    {0.0f, 1.0f, 0.0f}};

	float *view_src_x = &view_axis_all[0][0];
	float *view_src_y = &view_axis_all[1][0];

	float *view_dst_x = &view_axis_all[2][0];
	float *view_dst_y = &view_axis_all[3][0];
	int i;


	/* we could use rv3d->viewinv, but better not depend on view matrix being updated */
	if (UNLIKELY(ED_view3d_quat_from_axis_view(rv3d_src->view, viewinv) == false)) {
		return;
	}
	invert_qt(viewinv);
	mul_qt_v3(viewinv, view_src_x);
	mul_qt_v3(viewinv, view_src_y);

	if (UNLIKELY(ED_view3d_quat_from_axis_view(rv3d_dst->view, viewinv) == false)) {
		return;
	}
	invert_qt(viewinv);
	mul_qt_v3(viewinv, view_dst_x);
	mul_qt_v3(viewinv, view_dst_y);

	/* check source and dest have a matching axis */
	for (i = 0; i < 3; i++) {
		if (((fabsf(view_src_x[i]) > axis_eps) || (fabsf(view_src_y[i]) > axis_eps)) &&
		    ((fabsf(view_dst_x[i]) > axis_eps) || (fabsf(view_dst_y[i]) > axis_eps)))
		{
			rv3d_dst->ofs[i] = rv3d_src->ofs[i];
		}
	}
}

/* sync center/zoom view of region to others, for view transforms */
static void view3d_boxview_sync(ScrArea *sa, ARegion *ar)
{
	ARegion *artest;
	RegionView3D *rv3d = ar->regiondata;
	short clip = 0;

	for (artest = sa->regionbase.first; artest; artest = artest->next) {
		if (artest != ar && artest->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3dtest = artest->regiondata;

			if (rv3dtest->viewlock & RV3D_LOCKED) {
				rv3dtest->dist = rv3d->dist;
				view3d_boxview_sync_axis(rv3dtest, rv3d);
				clip |= rv3dtest->viewlock & RV3D_BOXCLIP;

				ED_region_tag_redraw(artest);
			}
		}
	}

	if (clip) {
		view3d_boxview_clip(sa);
	}
}

/* for home, center etc */
void view3d_boxview_copy(ScrArea *sa, ARegion *ar)
{
	ARegion *artest;
	RegionView3D *rv3d = ar->regiondata;
	bool clip = false;

	for (artest = sa->regionbase.first; artest; artest = artest->next) {
		if (artest != ar && artest->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3dtest = artest->regiondata;

			if (rv3dtest->viewlock) {
				rv3dtest->dist = rv3d->dist;
				copy_v3_v3(rv3dtest->ofs, rv3d->ofs);
				ED_region_tag_redraw(artest);

				clip |= ((rv3dtest->viewlock & RV3D_BOXCLIP) != 0);
			}
		}
	}

	if (clip) {
		view3d_boxview_clip(sa);
	}
}

/* 'clip' is used to know if our clip setting has changed */
void ED_view3d_quadview_update(ScrArea *sa, ARegion *ar, bool do_clip)
{
	ARegion *ar_sync = NULL;
	RegionView3D *rv3d = ar->regiondata;
	short viewlock;
	/* this function copies flags from the first of the 3 other quadview
	 * regions to the 2 other, so it assumes this is the region whose
	 * properties are always being edited, weak */
	viewlock = rv3d->viewlock;

	if ((viewlock & RV3D_LOCKED) == 0) {
		do_clip = (viewlock & RV3D_BOXCLIP) != 0;
		viewlock = 0;
	}
	else if ((viewlock & RV3D_BOXVIEW) == 0 && (viewlock & RV3D_BOXCLIP) != 0) {
		do_clip = true;
		viewlock &= ~RV3D_BOXCLIP;
	}

	for (; ar; ar = ar->prev) {
		if (ar->alignment == RGN_ALIGN_QSPLIT) {
			rv3d = ar->regiondata;
			rv3d->viewlock = viewlock;

			if (do_clip && (viewlock & RV3D_BOXCLIP) == 0) {
				rv3d->rflag &= ~RV3D_BOXCLIP;
			}

			/* use ar_sync so we sync with one of the aligned views below
			 * else the view jumps on changing view settings like 'clip'
			 * since it copies from the perspective view */
			ar_sync = ar;
		}
	}

	if (rv3d->viewlock & RV3D_BOXVIEW) {
		view3d_boxview_sync(sa, ar_sync ? ar_sync : sa->regionbase.last);
	}

	/* ensure locked regions have an axis, locked user views don't make much sense */
	if (viewlock & RV3D_LOCKED) {
		int index_qsplit = 0;
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->alignment == RGN_ALIGN_QSPLIT) {
				rv3d = ar->regiondata;
				if (rv3d->viewlock) {
					if (!RV3D_VIEW_IS_AXIS(rv3d->view)) {
						rv3d->view = ED_view3d_lock_view_from_index(index_qsplit);
						rv3d->persp = RV3D_ORTHO;
						ED_view3d_lock(rv3d);
					}
				}
				index_qsplit++;
			}
		}
	}

	ED_area_tag_redraw(sa);
}

/* ************************** init for view ops **********************************/

typedef struct ViewOpsData {
	/* context pointers (assigned by viewops_data_alloc) */
	ScrArea *sa;
	ARegion *ar;
	View3D *v3d;
	RegionView3D *rv3d;

	/* needed for continuous zoom */
	wmTimer *timer;
	double timer_lastdraw;

	float oldquat[4];
	float viewquat[4]; /* working copy of rv3d->viewquat */
	float trackvec[3];
	float mousevec[3]; /* dolly only */
	float reverse;
	float dist_prev, camzoom_prev;
	float grid, far;
	bool axis_snap;  /* view rotate only */
	float zfac;

	/* use for orbit selection and auto-dist */
	float ofs[3], dyn_ofs[3];
	bool use_dyn_ofs;

	int origx, origy, oldx, oldy;
	int origkey; /* the key that triggered the operator */

} ViewOpsData;

#define TRACKBALLSIZE  (1.1)

static void calctrackballvec(const rcti *rect, int mx, int my, float vec[3])
{
	float x, y, radius, d, z, t;

	radius = TRACKBALLSIZE;

	/* normalize x and y */
	x = BLI_rcti_cent_x(rect) - mx;
	x /= (float)(BLI_rcti_size_x(rect) / 4);
	y = BLI_rcti_cent_y(rect) - my;
	y /= (float)(BLI_rcti_size_y(rect) / 2);

	d = sqrtf(x * x + y * y);
	if (d < radius * (float)M_SQRT1_2) { /* Inside sphere */
		z = sqrtf(radius * radius - d * d);
	}
	else { /* On hyperbola */
		t = radius / (float)M_SQRT2;
		z = t * t / d;
	}

	vec[0] = x;
	vec[1] = y;
	vec[2] = -z;     /* yah yah! */
}


/* -------------------------------------------------------------------- */
/* ViewOpsData */

/** \name Generic View Operator Custom-Data.
 * \{ */

/**
 * Allocate and fill in context pointers for #ViewOpsData
 */
static void viewops_data_alloc(bContext *C, wmOperator *op)
{
	ViewOpsData *vod = MEM_callocN(sizeof(ViewOpsData), "viewops data");

	/* store data */
	op->customdata = vod;
	vod->sa = CTX_wm_area(C);
	vod->ar = CTX_wm_region(C);
	vod->v3d = vod->sa->spacedata.first;
	vod->rv3d = vod->ar->regiondata;
}

static void view3d_orbit_apply_dyn_ofs(
        float r_ofs[3], const float dyn_ofs[3],
        const float oldquat[4], const float viewquat[4])
{
	float q1[4];
	conjugate_qt_qt(q1, oldquat);
	mul_qt_qtqt(q1, q1, viewquat);

	conjugate_qt(q1);  /* conj == inv for unit quat */

	sub_v3_v3(r_ofs, dyn_ofs);
	mul_qt_v3(q1, r_ofs);
	add_v3_v3(r_ofs, dyn_ofs);
}

static bool view3d_orbit_calc_center(bContext *C, float r_dyn_ofs[3])
{
	static float lastofs[3] = {0, 0, 0};
	bool is_set = false;

	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;

	if (ob && (ob->mode & OB_MODE_ALL_PAINT) && (BKE_object_pose_armature_get(ob) == NULL)) {
		/* in case of sculpting use last average stroke position as a rotation
		 * center, in other cases it's not clear what rotation center shall be
		 * so just rotate around object origin
		 */
		if (ob->mode & OB_MODE_SCULPT) {
			float stroke[3];
			ED_sculpt_stroke_get_average(ob, stroke);
			copy_v3_v3(lastofs, stroke);
		}
		else {
			copy_v3_v3(lastofs, ob->obmat[3]);
		}
		is_set = true;
	}
	else if (ob && (ob->mode & OB_MODE_EDIT) && (ob->type == OB_FONT)) {
		Curve *cu = ob->data;
		EditFont *ef = cu->editfont;
		int i;

		zero_v3(lastofs);
		for (i = 0; i < 4; i++) {
			add_v2_v2(lastofs, ef->textcurs[i]);
		}
		mul_v2_fl(lastofs, 1.0f / 4.0f);

		mul_m4_v3(ob->obmat, lastofs);

		is_set = true;
	}
	else if (ob == NULL || ob->mode == OB_MODE_OBJECT) {
		/* object mode use boundbox centers */
		View3D *v3d = CTX_wm_view3d(C);
		Base *base;
		unsigned int tot = 0;
		float select_center[3];

		zero_v3(select_center);
		for (base = FIRSTBASE; base; base = base->next) {
			if (TESTBASE(v3d, base)) {
				/* use the boundbox if we can */
				Object *ob = base->object;

				if (ob->bb && !(ob->bb->flag & BOUNDBOX_DIRTY)) {
					float cent[3];

					BKE_boundbox_calc_center_aabb(ob->bb, cent);

					mul_m4_v3(ob->obmat, cent);
					add_v3_v3(select_center, cent);
				}
				else {
					add_v3_v3(select_center, ob->obmat[3]);
				}
				tot++;
			}
		}
		if (tot) {
			mul_v3_fl(select_center, 1.0f / (float)tot);
			copy_v3_v3(lastofs, select_center);
			is_set = true;
		}
	}
	else {
		/* If there's no selection, lastofs is unmodified and last value since static */
		is_set = calculateTransformCenter(C, V3D_CENTROID, lastofs, NULL);
	}

	copy_v3_v3(r_dyn_ofs, lastofs);

	return is_set;
}

/**
 * Calculate the values for #ViewOpsData
 */
static void viewops_data_create_ex(bContext *C, wmOperator *op, const wmEvent *event,
                                   const bool use_orbit_select,
                                   const bool use_orbit_zbuf)
{
	ViewOpsData *vod = op->customdata;
	RegionView3D *rv3d = vod->rv3d;

	/* set the view from the camera, if view locking is enabled.
	 * we may want to make this optional but for now its needed always */
	ED_view3d_camera_lock_init(vod->v3d, vod->rv3d);

	vod->dist_prev = rv3d->dist;
	vod->camzoom_prev = rv3d->camzoom;
	copy_qt_qt(vod->viewquat, rv3d->viewquat);
	copy_qt_qt(vod->oldquat, rv3d->viewquat);
	vod->origx = vod->oldx = event->x;
	vod->origy = vod->oldy = event->y;
	vod->origkey = event->type; /* the key that triggered the operator.  */
	vod->use_dyn_ofs = false;
	copy_v3_v3(vod->ofs, rv3d->ofs);

	if (use_orbit_select) {

		vod->use_dyn_ofs = true;

		view3d_orbit_calc_center(C, vod->dyn_ofs);

		negate_v3(vod->dyn_ofs);
	}
	else if (use_orbit_zbuf) {
		Scene *scene = CTX_data_scene(C);
		float fallback_depth_pt[3];

		view3d_operator_needs_opengl(C); /* needed for zbuf drawing */

		negate_v3_v3(fallback_depth_pt, rv3d->ofs);

		if ((vod->use_dyn_ofs = ED_view3d_autodist(scene, vod->ar, vod->v3d,
		                                           event->mval, vod->dyn_ofs, true, fallback_depth_pt)))
		{
			if (rv3d->is_persp) {
				float my_origin[3]; /* original G.vd->ofs */
				float my_pivot[3]; /* view */
				float dvec[3];

				/* locals for dist correction */
				float mat[3][3];
				float upvec[3];

				negate_v3_v3(my_origin, rv3d->ofs);             /* ofs is flipped */

				/* Set the dist value to be the distance from this 3d point
				 * this means youll always be able to zoom into it and panning wont go bad when dist was zero */

				/* remove dist value */
				upvec[0] = upvec[1] = 0;
				upvec[2] = rv3d->dist;
				copy_m3_m4(mat, rv3d->viewinv);

				mul_m3_v3(mat, upvec);
				sub_v3_v3v3(my_pivot, rv3d->ofs, upvec);
				negate_v3(my_pivot);                /* ofs is flipped */

				/* find a new ofs value that is along the view axis (rather than the mouse location) */
				closest_to_line_v3(dvec, vod->dyn_ofs, my_pivot, my_origin);
				vod->dist_prev = rv3d->dist = len_v3v3(my_pivot, dvec);

				negate_v3_v3(rv3d->ofs, dvec);
			}
			else {
				const float mval_ar_mid[2] = {
				    (float)vod->ar->winx / 2.0f,
				    (float)vod->ar->winy / 2.0f};

				ED_view3d_win_to_3d(vod->ar, vod->dyn_ofs, mval_ar_mid, rv3d->ofs);
				negate_v3(rv3d->ofs);
			}
			negate_v3(vod->dyn_ofs);
			copy_v3_v3(vod->ofs, rv3d->ofs);
		}
	}

	{
		/* for dolly */
		const float mval_f[2] = {(float)event->mval[0],
		                         (float)event->mval[1]};
		ED_view3d_win_to_vector(vod->ar, mval_f, vod->mousevec);
	}

	/* lookup, we don't pass on v3d to prevent confusement */
	vod->grid = vod->v3d->grid;
	vod->far = vod->v3d->far;

	calctrackballvec(&vod->ar->winrct, event->x, event->y, vod->trackvec);

	{
		float tvec[3];
		negate_v3_v3(tvec, rv3d->ofs);
		vod->zfac = ED_view3d_calc_zfac(rv3d, tvec, NULL);
	}

	vod->reverse = 1.0f;
	if (rv3d->persmat[2][1] < 0.0f)
		vod->reverse = -1.0f;

	rv3d->rflag |= RV3D_NAVIGATING;
}

static void viewops_data_create(bContext *C, wmOperator *op, const wmEvent *event)
{
	viewops_data_create_ex(
	        C, op, event,
	        (U.uiflag & USER_ORBIT_SELECTION) != 0,
	        (U.uiflag & USER_ZBUF_ORBIT) != 0);
}

static void viewops_data_free(bContext *C, wmOperator *op)
{
	ARegion *ar;
	Paint *p = BKE_paint_get_active_from_context(C);

	if (op->customdata) {
		ViewOpsData *vod = op->customdata;
		ar = vod->ar;
		vod->rv3d->rflag &= ~RV3D_NAVIGATING;

		if (vod->timer)
			WM_event_remove_timer(CTX_wm_manager(C), vod->timer->win, vod->timer);

		MEM_freeN(vod);
		op->customdata = NULL;
	}
	else {
		ar = CTX_wm_region(C);
	}

	if (p && (p->flags & PAINT_FAST_NAVIGATE))
		ED_region_tag_redraw(ar);
}
/** \} */


/* ************************** viewrotate **********************************/

enum {
	VIEW_PASS = 0,
	VIEW_APPLY,
	VIEW_CONFIRM
};

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
#define VIEW_MODAL_CONFIRM              1 /* used for all view operations */
#define VIEWROT_MODAL_AXIS_SNAP_ENABLE  2
#define VIEWROT_MODAL_AXIS_SNAP_DISABLE 3
#define VIEWROT_MODAL_SWITCH_ZOOM       4
#define VIEWROT_MODAL_SWITCH_MOVE       5
#define VIEWROT_MODAL_SWITCH_ROTATE     6

/* called in transform_ops.c, on each regeneration of keymaps  */
void viewrotate_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{VIEW_MODAL_CONFIRM,    "CONFIRM", 0, "Confirm", ""},

		{VIEWROT_MODAL_AXIS_SNAP_ENABLE,    "AXIS_SNAP_ENABLE", 0, "Enable Axis Snap", ""},
		{VIEWROT_MODAL_AXIS_SNAP_DISABLE,   "AXIS_SNAP_DISABLE", 0, "Disable Axis Snap", ""},
		
		{VIEWROT_MODAL_SWITCH_ZOOM, "SWITCH_TO_ZOOM", 0, "Switch to Zoom"},
		{VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Rotate Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "View3D Rotate Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, VIEW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, VIEW_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, LEFTALTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_AXIS_SNAP_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTALTKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_AXIS_SNAP_DISABLE);

	/* disabled mode switching for now, can re-implement better, later on */
#if 0
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_MOVE);
#endif
	
	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_rotate");

}

static void viewrotate_apply_dyn_ofs(ViewOpsData *vod, const float viewquat[4])
{
	if (vod->use_dyn_ofs) {
		RegionView3D *rv3d = vod->rv3d;
		copy_v3_v3(rv3d->ofs, vod->ofs);
		view3d_orbit_apply_dyn_ofs(rv3d->ofs, vod->dyn_ofs, vod->oldquat, viewquat);
	}
}

static void viewrotate_apply_snap(ViewOpsData *vod)
{
	const float axis_limit = DEG2RADF(45 / 3);

	RegionView3D *rv3d = vod->rv3d;

	float viewquat_inv[4];
	float zaxis[3] = {0, 0, 1};
	float zaxis_best[3];
	int x, y, z;
	bool found = false;

	invert_qt_qt(viewquat_inv, vod->viewquat);

	mul_qt_v3(viewquat_inv, zaxis);
	normalize_v3(zaxis);


	for (x = -1; x < 2; x++) {
		for (y = -1; y < 2; y++) {
			for (z = -1; z < 2; z++) {
				if (x || y || z) {
					float zaxis_test[3] = {x, y, z};

					normalize_v3(zaxis_test);

					if (angle_normalized_v3v3(zaxis_test, zaxis) < axis_limit) {
						copy_v3_v3(zaxis_best, zaxis_test);
						found = true;
					}
				}
			}
		}
	}

	if (found) {

		/* find the best roll */
		float quat_roll[4], quat_final[4], quat_best[4], quat_snap[4];
		float viewquat_align[4]; /* viewquat aligned to zaxis_best */
		float viewquat_align_inv[4]; /* viewquat aligned to zaxis_best */
		float best_angle = axis_limit;
		int j;

		/* viewquat_align is the original viewquat aligned to the snapped axis
		 * for testing roll */
		rotation_between_vecs_to_quat(viewquat_align, zaxis_best, zaxis);
		normalize_qt(viewquat_align);
		mul_qt_qtqt(viewquat_align, vod->viewquat, viewquat_align);
		normalize_qt(viewquat_align);
		invert_qt_qt(viewquat_align_inv, viewquat_align);

		vec_to_quat(quat_snap, zaxis_best, OB_NEGZ, OB_POSY);
		invert_qt(quat_snap);
		normalize_qt(quat_snap);

		/* check if we can find the roll */
		found = false;

		/* find best roll */
		for (j = 0; j < 8; j++) {
			float angle;
			float xaxis1[3] = {1, 0, 0};
			float xaxis2[3] = {1, 0, 0};
			float quat_final_inv[4];

			axis_angle_to_quat(quat_roll, zaxis_best, (float)j * DEG2RADF(45.0f));
			normalize_qt(quat_roll);

			mul_qt_qtqt(quat_final, quat_snap, quat_roll);
			normalize_qt(quat_final);

			/* compare 2 vector angles to find the least roll */
			invert_qt_qt(quat_final_inv, quat_final);
			mul_qt_v3(viewquat_align_inv, xaxis1);
			mul_qt_v3(quat_final_inv, xaxis2);
			angle = angle_v3v3(xaxis1, xaxis2);

			if (angle <= best_angle) {
				found = true;
				best_angle = angle;
				copy_qt_qt(quat_best, quat_final);
			}
		}

		if (found) {
			/* lock 'quat_best' to an axis view if we can */
			rv3d->view = ED_view3d_quat_to_axis_view(quat_best, 0.01f);
			if (rv3d->view != RV3D_VIEW_USER) {
				ED_view3d_quat_from_axis_view(rv3d->view, quat_best);
			}
		}
		else {
			copy_qt_qt(quat_best, viewquat_align);
		}

		copy_qt_qt(rv3d->viewquat, quat_best);

		viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);
	}
}

static void viewrotate_apply(ViewOpsData *vod, int x, int y)
{
	RegionView3D *rv3d = vod->rv3d;

	rv3d->view = RV3D_VIEW_USER; /* need to reset every time because of view snapping */

	if (U.flag & USER_TRACKBALL) {
		float phi, si, q1[4], dvec[3], newvec[3];

		calctrackballvec(&vod->ar->winrct, x, y, newvec);

		sub_v3_v3v3(dvec, newvec, vod->trackvec);

		si = len_v3(dvec);
		si /= (float)(2.0 * TRACKBALLSIZE);

		cross_v3_v3v3(q1 + 1, vod->trackvec, newvec);
		normalize_v3(q1 + 1);

		/* Allow for rotation beyond the interval [-pi, pi] */
		while (si > 1.0f)
			si -= 2.0f;

		/* This relation is used instead of
		 * - phi = asin(si) so that the angle
		 * - of rotation is linearly proportional
		 * - to the distance that the mouse is
		 * - dragged. */
		phi = si * (float)(M_PI / 2.0);

		q1[0] = cos(phi);
		mul_v3_fl(q1 + 1, sin(phi));
		mul_qt_qtqt(vod->viewquat, q1, vod->oldquat);

		viewrotate_apply_dyn_ofs(vod, vod->viewquat);
	}
	else {
		/* New turntable view code by John Aughey */
		float quat_local_x[4], quat_global_z[4];
		float m[3][3];
		float m_inv[3][3];
		const float zvec_global[3] = {0.0f, 0.0f, 1.0f};
		float xaxis[3];

		/* Sensitivity will control how fast the viewport rotates.  0.007 was
		 * obtained experimentally by looking at viewport rotation sensitivities
		 * on other modeling programs. */
		/* Perhaps this should be a configurable user parameter. */
		const float sensitivity = 0.007f;

		/* Get the 3x3 matrix and its inverse from the quaternion */
		quat_to_mat3(m, vod->viewquat);
		invert_m3_m3(m_inv, m);

		/* avoid gimble lock */
#if 1
		if (len_squared_v3v3(zvec_global, m_inv[2]) > 0.001f) {
			float fac;
			cross_v3_v3v3(xaxis, zvec_global, m_inv[2]);
			if (dot_v3v3(xaxis, m_inv[0]) < 0) {
				negate_v3(xaxis);
			}
			fac = angle_normalized_v3v3(zvec_global, m_inv[2]) / (float)M_PI;
			fac = fabsf(fac - 0.5f) * 2;
			fac = fac * fac;
			interp_v3_v3v3(xaxis, xaxis, m_inv[0], fac);
		}
		else {
			copy_v3_v3(xaxis, m_inv[0]);
		}
#else
		copy_v3_v3(xaxis, m_inv[0]);
#endif

		/* Determine the direction of the x vector (for rotating up and down) */
		/* This can likely be computed directly from the quaternion. */

		/* Perform the up/down rotation */
		axis_angle_to_quat(quat_local_x, xaxis, sensitivity * -(y - vod->oldy));
		mul_qt_qtqt(quat_local_x, vod->viewquat, quat_local_x);

		/* Perform the orbital rotation */
		axis_angle_normalized_to_quat(quat_global_z, zvec_global, sensitivity * vod->reverse * (x - vod->oldx));
		mul_qt_qtqt(vod->viewquat, quat_local_x, quat_global_z);

		viewrotate_apply_dyn_ofs(vod, vod->viewquat);
	}

	/* avoid precision loss over time */
	normalize_qt(vod->viewquat);

	/* use a working copy so view rotation locking doesnt overwrite the locked
	 * rotation back into the view we calculate with */
	copy_qt_qt(rv3d->viewquat, vod->viewquat);

	/* check for view snap,
	 * note: don't apply snap to vod->viewquat so the view wont jam up */
	if (vod->axis_snap) {
		viewrotate_apply_snap(vod);
	}
	vod->oldx = x;
	vod->oldy = y;

	ED_view3d_camera_lock_sync(vod->v3d, rv3d);

	ED_region_tag_redraw(vod->ar);
}

static int viewrotate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod = op->customdata;
	short event_code = VIEW_PASS;

	/* execute the events */
	if (event->type == MOUSEMOVE) {
		event_code = VIEW_APPLY;
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case VIEW_MODAL_CONFIRM:
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_AXIS_SNAP_ENABLE:
				vod->axis_snap = true;
				event_code = VIEW_APPLY;
				break;
			case VIEWROT_MODAL_AXIS_SNAP_DISABLE:
				vod->axis_snap = false;
				event_code = VIEW_APPLY;
				break;
			case VIEWROT_MODAL_SWITCH_ZOOM:
				WM_operator_name_call(C, "VIEW3D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_MOVE:
				WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
		}
	}
	else if (event->type == vod->origkey && event->val == KM_RELEASE) {
		event_code = VIEW_CONFIRM;
	}

	if (event_code == VIEW_APPLY) {
		viewrotate_apply(vod, event->x, event->y);
	}
	else if (event_code == VIEW_CONFIRM) {
		ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, true, true);
		ED_view3d_depth_tag_update(vod->rv3d);

		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

/**
 * Action to take when rotating the view,
 * handle auto-persp and logic for switching out of views.
 *
 * shared with NDOF.
 */
static bool view3d_ensure_persp(struct View3D *v3d, ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	const bool autopersp = (U.uiflag & USER_AUTOPERSP) != 0;

	BLI_assert((rv3d->viewlock & RV3D_LOCKED) == 0);

	if (ED_view3d_camera_lock_check(v3d, rv3d))
		return false;

	if (rv3d->persp != RV3D_PERSP) {
		if (rv3d->persp == RV3D_CAMOB) {
			/* If autopersp and previous view was an axis one, switch back to PERSP mode, else reuse previous mode. */
			char persp = (autopersp && RV3D_VIEW_IS_AXIS(rv3d->lview)) ? RV3D_PERSP : rv3d->lpersp;
			view3d_persp_switch_from_camera(v3d, rv3d, persp);
		}
		else if (autopersp && RV3D_VIEW_IS_AXIS(rv3d->view)) {
			rv3d->persp = RV3D_PERSP;
		}
		return true;
	}

	return false;
}

static int viewrotate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod;

	/* makes op->customdata */
	viewops_data_alloc(C, op);
	viewops_data_create(C, op, event);
	vod = op->customdata;

	/* poll should check but in some cases fails, see poll func for details */
	if (vod->rv3d->viewlock & RV3D_LOCKED) {
		viewops_data_free(C, op);
		return OPERATOR_PASS_THROUGH;
	}

	/* switch from camera view when: */
	if (view3d_ensure_persp(vod->v3d, vod->ar)) {
		/* If we're switching from camera view to the perspective one,
		 * need to tag viewport update, so camera vuew and borders
		 * are properly updated.
		 */
		ED_region_tag_redraw(vod->ar);
	}

	if (event->type == MOUSEPAN) {
		/* Rotate direction we keep always same */
		if (U.uiflag2 & USER_TRACKPAD_NATURAL)
			viewrotate_apply(vod, 2 * event->x - event->prevx, 2 * event->y - event->prevy);
		else
			viewrotate_apply(vod, event->prevx, event->prevy);
			
		ED_view3d_depth_tag_update(vod->rv3d);
		
		viewops_data_free(C, op);
		
		return OPERATOR_FINISHED;
	}
	else if (event->type == MOUSEROTATE) {
		/* MOUSEROTATE performs orbital rotation, so y axis delta is set to 0 */
		viewrotate_apply(vod, event->prevx, event->y);
		ED_view3d_depth_tag_update(vod->rv3d);
		
		viewops_data_free(C, op);
		
		return OPERATOR_FINISHED;
	}
	else {
		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
}

/* test for unlocked camera view in quad view */
static int view3d_camera_user_poll(bContext *C)
{
	View3D *v3d;
	ARegion *ar;

	if (ED_view3d_context_user_region(C, &v3d, &ar)) {
		RegionView3D *rv3d = ar->regiondata;
		if (rv3d->persp == RV3D_CAMOB) {
			return 1;
		}
	}

	return 0;
}

static int view3d_lock_poll(bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		if (rv3d) {
			return ED_view3d_offset_lock_check(v3d, rv3d);
		}
	}
	return false;
}

static void viewrotate_cancel(bContext *C, wmOperator *op)
{
	viewops_data_free(C, op);
}

void VIEW3D_OT_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate View";
	ot->description = "Rotate the view";
	ot->idname = "VIEW3D_OT_rotate";

	/* api callbacks */
	ot->invoke = viewrotate_invoke;
	ot->modal = viewrotate_modal;
	ot->poll = ED_operator_region_view3d_active;
	ot->cancel = viewrotate_cancel;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_POINTER;
}

/** \name NDOF Utility Functions
 * \{ */

#define NDOF_HAS_TRANSLATE ((!ED_view3d_offset_lock_check(v3d, rv3d)) && !is_zero_v3(ndof->tvec))
#define NDOF_HAS_ROTATE    (((rv3d->viewlock & RV3D_LOCKED) == 0) && !is_zero_v3(ndof->rvec))

/**
 * \param depth_pt: A point to calculate the depth (in perspective mode)
 */
static float view3d_ndof_pan_speed_calc_ex(RegionView3D *rv3d, const float depth_pt[3])
{
	float speed = rv3d->pixsize * NDOF_PIXELS_PER_SECOND;

	if (rv3d->is_persp) {
		speed *= ED_view3d_calc_zfac(rv3d, depth_pt, NULL);
	}

	return speed;
}

static float view3d_ndof_pan_speed_calc_from_dist(RegionView3D *rv3d, const float dist)
{
	float viewinv[4];
	float tvec[3];

	BLI_assert(dist >= 0.0f);

	copy_v3_fl3(tvec, 0.0f, 0.0f, dist);
	/* rv3d->viewinv isn't always valid */
#if 0
	mul_mat3_m4_v3(rv3d->viewinv, tvec);
#else
	invert_qt_qt(viewinv, rv3d->viewquat);
	mul_qt_v3(viewinv, tvec);
#endif

	return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

static float view3d_ndof_pan_speed_calc(RegionView3D *rv3d)
{
	float tvec[3];
	negate_v3_v3(tvec, rv3d->ofs);

	return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

/**
 * Zoom and pan in the same function since sometimes zoom is interpreted as dolly (pan forward).
 *
 * \param has_zoom zoom, otherwise dolly, often `!rv3d->is_persp` since it doesnt make sense to dolly in ortho.
 */
static void view3d_ndof_pan_zoom(const struct wmNDOFMotionData *ndof, ScrArea *sa, ARegion *ar,
                                 const bool has_translate, const bool has_zoom)
{
	RegionView3D *rv3d = ar->regiondata;
	float view_inv[4];
	float pan_vec[3];

	if (has_translate == false && has_zoom == false) {
		return;
	}

	WM_event_ndof_pan_get(ndof, pan_vec, false);

	if (has_zoom) {
		/* zoom with Z */

		/* Zoom!
		 * velocity should be proportional to the linear velocity attained by rotational motion of same strength
		 * [got that?]
		 * proportional to arclength = radius * angle
		 */

		pan_vec[2] = 0.0f;

		/* "zoom in" or "translate"? depends on zoom mode in user settings? */
		if (ndof->tvec[2]) {
			float zoom_distance = rv3d->dist * ndof->dt * ndof->tvec[2];

			if (U.ndof_flag & NDOF_ZOOM_INVERT)
				zoom_distance = -zoom_distance;

			rv3d->dist += zoom_distance;
		}
	}
	else {
		/* dolly with Z */

		/* all callers must check */
		if (has_translate) {
			BLI_assert(ED_view3d_offset_lock_check((View3D *)sa->spacedata.first, rv3d) == false);
		}
	}

	if (has_translate) {
		const float speed = view3d_ndof_pan_speed_calc(rv3d);

		mul_v3_fl(pan_vec, speed * ndof->dt);

		/* transform motion from view to world coordinates */
		invert_qt_qt(view_inv, rv3d->viewquat);
		mul_qt_v3(view_inv, pan_vec);

		/* move center of view opposite of hand motion (this is camera mode, not object mode) */
		sub_v3_v3(rv3d->ofs, pan_vec);

		if (rv3d->viewlock & RV3D_BOXVIEW) {
			view3d_boxview_sync(sa, ar);
		}
	}
}


static void view3d_ndof_orbit(const struct wmNDOFMotionData *ndof, ScrArea *sa, ARegion *ar,
                              /* optional, can be NULL*/
                              ViewOpsData *vod)
{
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;

	float view_inv[4];

	BLI_assert((rv3d->viewlock & RV3D_LOCKED) == 0);

	view3d_ensure_persp(v3d, ar);

	rv3d->view = RV3D_VIEW_USER;

	invert_qt_qt(view_inv, rv3d->viewquat);

	if (U.ndof_flag & NDOF_TURNTABLE) {
		float rot[3];

		/* turntable view code by John Aughey, adapted for 3D mouse by [mce] */
		float angle, quat[4];
		float xvec[3] = {1, 0, 0};

		/* only use XY, ignore Z */
		WM_event_ndof_rotate_get(ndof, rot);

		/* Determine the direction of the x vector (for rotating up and down) */
		mul_qt_v3(view_inv, xvec);

		/* Perform the up/down rotation */
		angle = ndof->dt * rot[0];
		quat[0] = cosf(angle);
		mul_v3_v3fl(quat + 1, xvec, sin(angle));
		mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);

		/* Perform the orbital rotation */
		angle = ndof->dt * rot[1];

		/* update the onscreen doo-dad */
		rv3d->rot_angle = angle;
		rv3d->rot_axis[0] = 0;
		rv3d->rot_axis[1] = 0;
		rv3d->rot_axis[2] = 1;

		quat[0] = cosf(angle);
		quat[1] = 0.0f;
		quat[2] = 0.0f;
		quat[3] = sinf(angle);
		mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);

	}
	else {
		float quat[4];
		float axis[3];
		float angle = WM_event_ndof_to_axis_angle(ndof, axis);

		/* transform rotation axis from view to world coordinates */
		mul_qt_v3(view_inv, axis);

		/* update the onscreen doo-dad */
		rv3d->rot_angle = angle;
		copy_v3_v3(rv3d->rot_axis, axis);

		axis_angle_to_quat(quat, axis, angle);

		/* apply rotation */
		mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
	}

	if (vod) {
		viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);
	}
}

/**
 * Called from both fly mode and walk mode,
 */
void view3d_ndof_fly(
        const wmNDOFMotionData *ndof,
        View3D *v3d, RegionView3D *rv3d,
        const bool use_precision, const short protectflag,
        bool *r_has_translate, bool *r_has_rotate)
{
	bool has_translate = NDOF_HAS_TRANSLATE;
	bool has_rotate = NDOF_HAS_ROTATE;

	float view_inv[4];
	invert_qt_qt(view_inv, rv3d->viewquat);

	rv3d->rot_angle = 0.0f;  /* disable onscreen rotation doo-dad */

	if (has_translate) {
		/* ignore real 'dist' since fly has its own speed settings,
		 * also its overwritten at this point. */
		float speed = view3d_ndof_pan_speed_calc_from_dist(rv3d, 1.0f);
		float trans[3], trans_orig_y;

		if (use_precision)
			speed *= 0.2f;

		WM_event_ndof_pan_get(ndof, trans, false);
		mul_v3_fl(trans, speed * ndof->dt);
		trans_orig_y = trans[1];

		if (U.ndof_flag & NDOF_FLY_HELICOPTER) {
			trans[1] = 0.0f;
		}

		/* transform motion from view to world coordinates */
		mul_qt_v3(view_inv, trans);

		if (U.ndof_flag & NDOF_FLY_HELICOPTER) {
			/* replace world z component with device y (yes it makes sense) */
			trans[2] = trans_orig_y;
		}

		if (rv3d->persp == RV3D_CAMOB) {
			/* respect camera position locks */
			if (protectflag & OB_LOCK_LOCX) trans[0] = 0.0f;
			if (protectflag & OB_LOCK_LOCY) trans[1] = 0.0f;
			if (protectflag & OB_LOCK_LOCZ) trans[2] = 0.0f;
		}

		if (!is_zero_v3(trans)) {
			/* move center of view opposite of hand motion (this is camera mode, not object mode) */
			sub_v3_v3(rv3d->ofs, trans);
			has_translate = true;
		}
		else {
			has_translate = false;
		}
	}

	if (has_rotate) {
		const float turn_sensitivity = 1.0f;

		float rotation[4];
		float axis[3];
		float angle = turn_sensitivity * WM_event_ndof_to_axis_angle(ndof, axis);

		if (fabsf(angle) > 0.0001f) {
			has_rotate = true;

			if (use_precision)
				angle *= 0.2f;

			/* transform rotation axis from view to world coordinates */
			mul_qt_v3(view_inv, axis);

			/* apply rotation to view */
			axis_angle_to_quat(rotation, axis, angle);
			mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);

			if (U.ndof_flag & NDOF_LOCK_HORIZON) {
				/* force an upright viewpoint
				 * TODO: make this less... sudden */
				float view_horizon[3] = {1.0f, 0.0f, 0.0f}; /* view +x */
				float view_direction[3] = {0.0f, 0.0f, -1.0f}; /* view -z (into screen) */

				/* find new inverse since viewquat has changed */
				invert_qt_qt(view_inv, rv3d->viewquat);
				/* could apply reverse rotation to existing view_inv to save a few cycles */

				/* transform view vectors to world coordinates */
				mul_qt_v3(view_inv, view_horizon);
				mul_qt_v3(view_inv, view_direction);


				/* find difference between view & world horizons
				 * true horizon lives in world xy plane, so look only at difference in z */
				angle = -asinf(view_horizon[2]);

				/* rotate view so view horizon = world horizon */
				axis_angle_to_quat(rotation, view_direction, angle);
				mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);
			}

			rv3d->view = RV3D_VIEW_USER;
		}
		else {
			has_rotate = false;
		}
	}

	*r_has_translate = has_translate;
	*r_has_rotate    = has_rotate;
}

/** \} */


/* -- "orbit" navigation (trackball/turntable)
 * -- zooming
 * -- panning in rotationally-locked views
 */
static int ndof_orbit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	
	if (event->type != NDOF_MOTION) {
		return OPERATOR_CANCELLED;
	}
	else {
		ViewOpsData *vod;
		View3D *v3d;
		RegionView3D *rv3d;

		const wmNDOFMotionData *ndof = event->customdata;

		viewops_data_alloc(C, op);
		viewops_data_create_ex(C, op, event,
		                       (U.uiflag & USER_ORBIT_SELECTION) != 0, false);

		vod = op->customdata;
		v3d = vod->v3d;
		rv3d = vod->rv3d;

		/* off by default, until changed later this function */
		rv3d->rot_angle = 0.0f;

		ED_view3d_camera_lock_init_ex(v3d, rv3d, false);

		if (ndof->progress != P_FINISHING) {
			const bool has_rotation = NDOF_HAS_ROTATE;
			/* if we can't rotate, fallback to translate (locked axis views) */
			const bool has_translate = NDOF_HAS_TRANSLATE && (rv3d->viewlock & RV3D_LOCKED);
			const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

			if (has_translate || has_zoom) {
				view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, has_translate, has_zoom);
			}

			if (has_rotation) {
				view3d_ndof_orbit(ndof, vod->sa, vod->ar, vod);
			}
		}

		ED_view3d_camera_lock_sync(v3d, rv3d);

		ED_region_tag_redraw(vod->ar);

		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}
}

void VIEW3D_OT_ndof_orbit(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "NDOF Orbit View";
	ot->description = "Orbit the view using the 3D mouse";
	ot->idname = "VIEW3D_OT_ndof_orbit";

	/* api callbacks */
	ot->invoke = ndof_orbit_invoke;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}

static int ndof_orbit_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	
	if (event->type != NDOF_MOTION) {
		return OPERATOR_CANCELLED;
	}
	else {
		ViewOpsData *vod;
		View3D *v3d;
		RegionView3D *rv3d;

		const wmNDOFMotionData *ndof = event->customdata;

		viewops_data_alloc(C, op);
		viewops_data_create_ex(C, op, event,
		                       (U.uiflag & USER_ORBIT_SELECTION) != 0, false);

		vod = op->customdata;
		v3d = vod->v3d;
		rv3d = vod->rv3d;

		/* off by default, until changed later this function */
		rv3d->rot_angle = 0.0f;

		ED_view3d_camera_lock_init_ex(v3d, rv3d, false);

		if (ndof->progress == P_FINISHING) {
			/* pass */
		}
		else if ((rv3d->persp == RV3D_ORTHO) && RV3D_VIEW_IS_AXIS(rv3d->view)) {
			/* if we can't rotate, fallback to translate (locked axis views) */
			const bool has_translate = NDOF_HAS_TRANSLATE;
			const bool has_zoom = (ndof->tvec[2] != 0.0f) && ED_view3d_offset_lock_check(v3d, rv3d);

			if (has_translate || has_zoom) {
				view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, has_translate, true);
			}
		}
		else if ((U.ndof_flag & NDOF_MODE_ORBIT) ||
		         ED_view3d_offset_lock_check(v3d, rv3d))
		{
			const bool has_rotation = NDOF_HAS_ROTATE;
			const bool has_zoom = (ndof->tvec[2] != 0.0f);

			if (has_zoom) {
				view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, false, has_zoom);
			}

			if (has_rotation) {
				view3d_ndof_orbit(ndof, vod->sa, vod->ar, vod);
			}
		}
		else {  /* free/explore (like fly mode) */
			const bool has_rotation = NDOF_HAS_ROTATE;
			const bool has_translate = NDOF_HAS_TRANSLATE;
			const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

			float dist_backup;

			if (has_translate || has_zoom) {
				view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, has_translate, has_zoom);
			}

			dist_backup = rv3d->dist;
			ED_view3d_distance_set(rv3d, 0.0f);

			if (has_rotation) {
				view3d_ndof_orbit(ndof, vod->sa, vod->ar, NULL);
			}

			ED_view3d_distance_set(rv3d, dist_backup);
		}

		ED_view3d_camera_lock_sync(v3d, rv3d);

		ED_region_tag_redraw(vod->ar);

		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}
}

void VIEW3D_OT_ndof_orbit_zoom(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "NDOF Orbit View with Zoom";
	ot->description = "Orbit and zoom the view using the 3D mouse";
	ot->idname = "VIEW3D_OT_ndof_orbit_zoom";

	/* api callbacks */
	ot->invoke = ndof_orbit_zoom_invoke;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}

/* -- "pan" navigation
 * -- zoom or dolly?
 */
static int ndof_pan_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	if (event->type != NDOF_MOTION) {
		return OPERATOR_CANCELLED;
	}
	else {
		View3D *v3d = CTX_wm_view3d(C);
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		const wmNDOFMotionData *ndof = event->customdata;

		const bool has_translate = NDOF_HAS_TRANSLATE;
		const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

		/* we're panning here! so erase any leftover rotation from other operators */
		rv3d->rot_angle = 0.0f;

		if (!(has_translate || has_zoom))
			return OPERATOR_CANCELLED;

		ED_view3d_camera_lock_init_ex(v3d, rv3d, false);

		if (ndof->progress != P_FINISHING) {
			ScrArea *sa = CTX_wm_area(C);
			ARegion *ar = CTX_wm_region(C);

			if (has_translate || has_zoom) {
				view3d_ndof_pan_zoom(ndof, sa, ar, has_translate, has_zoom);
			}
		}

		ED_view3d_camera_lock_sync(v3d, rv3d);

		ED_region_tag_redraw(CTX_wm_region(C));

		return OPERATOR_FINISHED;
	}
}

void VIEW3D_OT_ndof_pan(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "NDOF Pan View";
	ot->description = "Pan the view with the 3D mouse";
	ot->idname = "VIEW3D_OT_ndof_pan";

	/* api callbacks */
	ot->invoke = ndof_pan_invoke;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}


/**
 * wraps #ndof_orbit_zoom but never restrict to orbit.
 */
static int ndof_all_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	/* weak!, but it works */
	const int ndof_flag = U.ndof_flag;
	int ret;

	U.ndof_flag &= ~NDOF_MODE_ORBIT;

	ret = ndof_orbit_zoom_invoke(C, op, event);

	U.ndof_flag = ndof_flag;

	return ret;
}

void VIEW3D_OT_ndof_all(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "NDOF Move View";
	ot->description = "Pan and rotate the view with the 3D mouse";
	ot->idname = "VIEW3D_OT_ndof_all";

	/* api callbacks */
	ot->invoke = ndof_all_invoke;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}

/* ************************ viewmove ******************************** */


/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */

/* called in transform_ops.c, on each regeneration of keymaps  */
void viewmove_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{VIEW_MODAL_CONFIRM,    "CONFIRM", 0, "Confirm", ""},
		
		{VIEWROT_MODAL_SWITCH_ZOOM, "SWITCH_TO_ZOOM", 0, "Switch to Zoom"},
		{VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},

		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Move Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "View3D Move Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, VIEW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, VIEW_MODAL_CONFIRM);

	/* disabled mode switching for now, can re-implement better, later on */
#if 0
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
#endif
	
	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_move");
}


static void viewmove_apply(ViewOpsData *vod, int x, int y)
{
	if (ED_view3d_offset_lock_check(vod->v3d, vod->rv3d)) {
		vod->rv3d->ofs_lock[0] -= ((vod->oldx - x) * 2.0f) / (float)vod->ar->winx;
		vod->rv3d->ofs_lock[1] -= ((vod->oldy - y) * 2.0f) / (float)vod->ar->winy;
	}
	else if ((vod->rv3d->persp == RV3D_CAMOB) && !ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) {
		const float zoomfac = BKE_screen_view3d_zoom_to_fac((float)vod->rv3d->camzoom) * 2.0f;
		vod->rv3d->camdx += (vod->oldx - x) / (vod->ar->winx * zoomfac);
		vod->rv3d->camdy += (vod->oldy - y) / (vod->ar->winy * zoomfac);
		CLAMP(vod->rv3d->camdx, -1.0f, 1.0f);
		CLAMP(vod->rv3d->camdy, -1.0f, 1.0f);
	}
	else {
		float dvec[3];
		float mval_f[2];

		mval_f[0] = x - vod->oldx;
		mval_f[1] = y - vod->oldy;
		ED_view3d_win_to_delta(vod->ar, mval_f, dvec, vod->zfac);

		add_v3_v3(vod->rv3d->ofs, dvec);

		if (vod->rv3d->viewlock & RV3D_BOXVIEW)
			view3d_boxview_sync(vod->sa, vod->ar);
	}

	vod->oldx = x;
	vod->oldy = y;

	ED_view3d_camera_lock_sync(vod->v3d, vod->rv3d);

	ED_region_tag_redraw(vod->ar);
}


static int viewmove_modal(bContext *C, wmOperator *op, const wmEvent *event)
{

	ViewOpsData *vod = op->customdata;
	short event_code = VIEW_PASS;

	/* execute the events */
	if (event->type == MOUSEMOVE) {
		event_code = VIEW_APPLY;
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case VIEW_MODAL_CONFIRM:
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_ZOOM:
				WM_operator_name_call(C, "VIEW3D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_ROTATE:
				WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
		}
	}
	else if (event->type == vod->origkey && event->val == KM_RELEASE) {
		event_code = VIEW_CONFIRM;
	}

	if (event_code == VIEW_APPLY) {
		viewmove_apply(vod, event->x, event->y);
	}
	else if (event_code == VIEW_CONFIRM) {
		ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
		ED_view3d_depth_tag_update(vod->rv3d);

		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int viewmove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod;

	/* makes op->customdata */
	viewops_data_alloc(C, op);
	viewops_data_create(C, op, event);
	vod = op->customdata;

	if (event->type == MOUSEPAN) {
		/* invert it, trackpad scroll follows same principle as 2d windows this way */
		viewmove_apply(vod, 2 * event->x - event->prevx, 2 * event->y - event->prevy);
		ED_view3d_depth_tag_update(vod->rv3d);
		
		viewops_data_free(C, op);
		
		return OPERATOR_FINISHED;
	}
	else {
		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
}

static void viewmove_cancel(bContext *C, wmOperator *op)
{
	viewops_data_free(C, op);
}

void VIEW3D_OT_move(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Move View";
	ot->description = "Move the view";
	ot->idname = "VIEW3D_OT_move";

	/* api callbacks */
	ot->invoke = viewmove_invoke;
	ot->modal = viewmove_modal;
	ot->poll = ED_operator_view3d_active;
	ot->cancel = viewmove_cancel;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_POINTER;
}

/* ************************ viewzoom ******************************** */

/* viewdolly_modal_keymap has an exact copy of this, apply fixes to both */
/* called in transform_ops.c, on each regeneration of keymaps  */
void viewzoom_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{VIEW_MODAL_CONFIRM,    "CONFIRM", 0, "Confirm", ""},
		
		{VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
		{VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Zoom Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "View3D Zoom Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, VIEW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, VIEW_MODAL_CONFIRM);

	/* disabled mode switching for now, can re-implement better, later on */
#if 0
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_MOVE);
#endif
	
	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom");
}

static void view_zoom_mouseloc(ARegion *ar, float dfac, int mx, int my)
{
	RegionView3D *rv3d = ar->regiondata;

	if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
		float dvec[3];
		float tvec[3];
		float tpos[3];
		float mval_f[2];
		float new_dist;
		float zfac;

		negate_v3_v3(tpos, rv3d->ofs);

		mval_f[0] = (float)(((mx - ar->winrct.xmin) * 2) - ar->winx) / 2.0f;
		mval_f[1] = (float)(((my - ar->winrct.ymin) * 2) - ar->winy) / 2.0f;

		/* Project cursor position into 3D space */
		zfac = ED_view3d_calc_zfac(rv3d, tpos, NULL);
		ED_view3d_win_to_delta(ar, mval_f, dvec, zfac);

		/* Calculate view target position for dolly */
		add_v3_v3v3(tvec, tpos, dvec);
		negate_v3(tvec);

		/* Offset to target position and dolly */
		new_dist = rv3d->dist * dfac;

		copy_v3_v3(rv3d->ofs, tvec);
		rv3d->dist = new_dist;

		/* Calculate final offset */
		madd_v3_v3v3fl(rv3d->ofs, tvec, dvec, dfac);
	}
	else {
		rv3d->dist *= dfac;
	}
}


static void viewzoom_apply(ViewOpsData *vod, const int xy[2], const short viewzoom, const short zoom_invert)
{
	float zfac = 1.0;
	bool use_cam_zoom;

	use_cam_zoom = (vod->rv3d->persp == RV3D_CAMOB) && !(vod->rv3d->is_persp && ED_view3d_camera_lock_check(vod->v3d, vod->rv3d));

	if (use_cam_zoom) {
		float delta;
		delta = (xy[0] - vod->origx + xy[1] - vod->origy) / 10.0f;
		vod->rv3d->camzoom = vod->camzoom_prev + (zoom_invert ? -delta : delta);

		CLAMP(vod->rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
	}

	if (viewzoom == USER_ZOOM_CONT) {
		double time = PIL_check_seconds_timer();
		float time_step = (float)(time - vod->timer_lastdraw);
		float fac;

		if (U.uiflag & USER_ZOOM_HORIZ) {
			fac = (float)(vod->origx - xy[0]);
		}
		else {
			fac = (float)(vod->origy - xy[1]);
		}

		if (zoom_invert) {
			fac = -fac;
		}

		/* oldstyle zoom */
		zfac = 1.0f + ((fac / 20.0f) * time_step);
		vod->timer_lastdraw = time;
	}
	else if (viewzoom == USER_ZOOM_SCALE) {
		/* method which zooms based on how far you move the mouse */

		const int ctr[2] = {
		    BLI_rcti_cent_x(&vod->ar->winrct),
		    BLI_rcti_cent_y(&vod->ar->winrct),
		};
		const float len_new = 5 + len_v2v2_int(ctr, xy);
		const float len_old = 5 + len_v2v2_int(ctr, &vod->origx);
		zfac = vod->dist_prev * ((len_old + 5) / (len_new + 5)) / vod->rv3d->dist;
	}
	else {  /* USER_ZOOM_DOLLY */
		float len1, len2;
		
		if (U.uiflag & USER_ZOOM_HORIZ) {
			len1 = (vod->ar->winrct.xmax - xy[0]) + 5;
			len2 = (vod->ar->winrct.xmax - vod->origx) + 5;
		}
		else {
			len1 = (vod->ar->winrct.ymax - xy[1]) + 5;
			len2 = (vod->ar->winrct.ymax - vod->origy) + 5;
		}
		if (zoom_invert) {
			SWAP(float, len1, len2);
		}
		
		if (use_cam_zoom) {
			/* zfac is ignored in this case, see below */
#if 0
			zfac = vod->camzoom_prev * (2.0f * ((len1 / len2) - 1.0f) + 1.0f) / vod->rv3d->camzoom;
#endif
		}
		else {
			zfac = vod->dist_prev * (2.0f * ((len1 / len2) - 1.0f) + 1.0f) / vod->rv3d->dist;
		}
	}

	if (!use_cam_zoom) {
		if (zfac != 1.0f && zfac * vod->rv3d->dist > 0.001f * vod->grid &&
		    zfac * vod->rv3d->dist < 10.0f * vod->far)
		{
			view_zoom_mouseloc(vod->ar, zfac, vod->oldx, vod->oldy);
		}
	}

	/* these limits were in old code too */
	if (vod->rv3d->dist < 0.001f * vod->grid) vod->rv3d->dist = 0.001f * vod->grid;
	if (vod->rv3d->dist > 10.0f * vod->far) vod->rv3d->dist = 10.0f * vod->far;

	if (vod->rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(vod->sa, vod->ar);

	ED_view3d_camera_lock_sync(vod->v3d, vod->rv3d);

	ED_region_tag_redraw(vod->ar);
}


static int viewzoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod = op->customdata;
	short event_code = VIEW_PASS;

	/* execute the events */
	if (event->type == TIMER && event->customdata == vod->timer) {
		/* continuous zoom */
		event_code = VIEW_APPLY;
	}
	else if (event->type == MOUSEMOVE) {
		event_code = VIEW_APPLY;
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case VIEW_MODAL_CONFIRM:
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_MOVE:
				WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_ROTATE:
				WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
		}
	}
	else if (event->type == vod->origkey && event->val == KM_RELEASE) {
		event_code = VIEW_CONFIRM;
	}

	if (event_code == VIEW_APPLY) {
		viewzoom_apply(vod, &event->x, U.viewzoom, (U.uiflag & USER_ZOOM_INVERT) != 0);
	}
	else if (event_code == VIEW_CONFIRM) {
		ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
		ED_view3d_depth_tag_update(vod->rv3d);
		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int viewzoom_exec(bContext *C, wmOperator *op)
{
	View3D *v3d;
	RegionView3D *rv3d;
	ScrArea *sa;
	ARegion *ar;
	bool use_cam_zoom;

	const int delta = RNA_int_get(op->ptr, "delta");
	int mx, my;

	if (op->customdata) {
		ViewOpsData *vod = op->customdata;

		sa = vod->sa;
		ar = vod->ar;
	}
	else {
		sa = CTX_wm_area(C);
		ar = CTX_wm_region(C);
	}

	v3d = sa->spacedata.first;
	rv3d = ar->regiondata;

	mx = RNA_struct_property_is_set(op->ptr, "mx") ? RNA_int_get(op->ptr, "mx") : ar->winx / 2;
	my = RNA_struct_property_is_set(op->ptr, "my") ? RNA_int_get(op->ptr, "my") : ar->winy / 2;

	use_cam_zoom = (rv3d->persp == RV3D_CAMOB) && !(rv3d->is_persp && ED_view3d_camera_lock_check(v3d, rv3d));

	if (delta < 0) {
		/* this min and max is also in viewmove() */
		if (use_cam_zoom) {
			rv3d->camzoom -= 10.0f;
			if (rv3d->camzoom < RV3D_CAMZOOM_MIN) rv3d->camzoom = RV3D_CAMZOOM_MIN;
		}
		else if (rv3d->dist < 10.0f * v3d->far) {
			view_zoom_mouseloc(ar, 1.2f, mx, my);
		}
	}
	else {
		if (use_cam_zoom) {
			rv3d->camzoom += 10.0f;
			if (rv3d->camzoom > RV3D_CAMZOOM_MAX) rv3d->camzoom = RV3D_CAMZOOM_MAX;
		}
		else if (rv3d->dist > 0.001f * v3d->grid) {
			view_zoom_mouseloc(ar, 0.83333f, mx, my);
		}
	}

	if (rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(sa, ar);

	ED_view3d_depth_tag_update(rv3d);

	ED_view3d_camera_lock_sync(v3d, rv3d);

	ED_region_tag_redraw(ar);

	viewops_data_free(C, op);

	return OPERATOR_FINISHED;
}

/* this is an exact copy of viewzoom_modal_keymap */
/* called in transform_ops.c, on each regeneration of keymaps  */
void viewdolly_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{VIEW_MODAL_CONFIRM,    "CONFIRM", 0, "Confirm", ""},

		{VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
		{VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Dolly Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "View3D Dolly Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, VIEW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, VIEW_MODAL_CONFIRM);

	/* disabled mode switching for now, can re-implement better, later on */
#if 0
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_MOVE);
#endif
	
	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_dolly");
}

/* viewdolly_invoke() copied this function, changes here may apply there */
static int viewzoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod;

	/* makes op->customdata */
	viewops_data_alloc(C, op);
	viewops_data_create(C, op, event);
	vod = op->customdata;

	/* if one or the other zoom position aren't set, set from event */
	if (!RNA_struct_property_is_set(op->ptr, "mx") || !RNA_struct_property_is_set(op->ptr, "my")) {
		RNA_int_set(op->ptr, "mx", event->x);
		RNA_int_set(op->ptr, "my", event->y);
	}

	if (RNA_struct_property_is_set(op->ptr, "delta")) {
		viewzoom_exec(C, op);
	}
	else {
		if (event->type == MOUSEZOOM || event->type == MOUSEPAN) {

			if (U.uiflag & USER_ZOOM_HORIZ) {
				vod->origx = vod->oldx = event->x;
				viewzoom_apply(vod, &event->prevx, USER_ZOOM_DOLLY, (U.uiflag & USER_ZOOM_INVERT) != 0);
			}
			else {
				/* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
				vod->origy = vod->oldy = vod->origy + event->x - event->prevx;
				viewzoom_apply(vod, &event->prevx, USER_ZOOM_DOLLY, (U.uiflag & USER_ZOOM_INVERT) != 0);
			}
			ED_view3d_depth_tag_update(vod->rv3d);
			
			viewops_data_free(C, op);
			return OPERATOR_FINISHED;
		}
		else {
			if (U.viewzoom == USER_ZOOM_CONT) {
				/* needs a timer to continue redrawing */
				vod->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
				vod->timer_lastdraw = PIL_check_seconds_timer();
			}

			/* add temp handler */
			WM_event_add_modal_handler(C, op);

			return OPERATOR_RUNNING_MODAL;
		}
	}
	return OPERATOR_FINISHED;
}

static void viewzoom_cancel(bContext *C, wmOperator *op)
{
	viewops_data_free(C, op);
}

void VIEW3D_OT_zoom(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Zoom View";
	ot->description = "Zoom in/out in the view";
	ot->idname = "VIEW3D_OT_zoom";

	/* api callbacks */
	ot->invoke = viewzoom_invoke;
	ot->exec = viewzoom_exec;
	ot->modal = viewzoom_modal;
	ot->poll = ED_operator_region_view3d_active;
	ot->cancel = viewzoom_cancel;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_POINTER;

	RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mx", 0, 0, INT_MAX, "Zoom Position X", "", 0, INT_MAX);
	RNA_def_int(ot->srna, "my", 0, 0, INT_MAX, "Zoom Position Y", "", 0, INT_MAX);
}


/* ************************ viewdolly ******************************** */
static void view_dolly_mouseloc(ARegion *ar, float orig_ofs[3], float dvec[3], float dfac)
{
	RegionView3D *rv3d = ar->regiondata;
	madd_v3_v3v3fl(rv3d->ofs, orig_ofs, dvec, -(1.0f - dfac));
}

static void viewdolly_apply(ViewOpsData *vod, int x, int y, const short zoom_invert)
{
	float zfac = 1.0;

	{
		float len1, len2;

		if (U.uiflag & USER_ZOOM_HORIZ) {
			len1 = (vod->ar->winrct.xmax - x) + 5;
			len2 = (vod->ar->winrct.xmax - vod->origx) + 5;
		}
		else {
			len1 = (vod->ar->winrct.ymax - y) + 5;
			len2 = (vod->ar->winrct.ymax - vod->origy) + 5;
		}
		if (zoom_invert)
			SWAP(float, len1, len2);

		zfac =  1.0f + ((len1 - len2) * 0.01f * vod->rv3d->dist);
	}

	if (zfac != 1.0f)
		view_dolly_mouseloc(vod->ar, vod->ofs, vod->mousevec, zfac);

	if (vod->rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(vod->sa, vod->ar);

	ED_view3d_camera_lock_sync(vod->v3d, vod->rv3d);

	ED_region_tag_redraw(vod->ar);
}


static int viewdolly_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod = op->customdata;
	short event_code = VIEW_PASS;

	/* execute the events */
	if (event->type == MOUSEMOVE) {
		event_code = VIEW_APPLY;
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case VIEW_MODAL_CONFIRM:
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_MOVE:
				WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_ROTATE:
				WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
		}
	}
	else if (event->type == vod->origkey && event->val == KM_RELEASE) {
		event_code = VIEW_CONFIRM;
	}

	if (event_code == VIEW_APPLY) {
		viewdolly_apply(vod, event->x, event->y, (U.uiflag & USER_ZOOM_INVERT) != 0);
	}
	else if (event_code == VIEW_CONFIRM) {
		ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
		ED_view3d_depth_tag_update(vod->rv3d);
		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int viewdolly_exec(bContext *C, wmOperator *op)
{
	View3D *v3d;
	RegionView3D *rv3d;
	ScrArea *sa;
	ARegion *ar;
	float mousevec[3];

	const int delta = RNA_int_get(op->ptr, "delta");

	if (op->customdata) {
		ViewOpsData *vod = op->customdata;

		sa = vod->sa;
		ar = vod->ar;
		copy_v3_v3(mousevec, vod->mousevec);
	}
	else {
		sa = CTX_wm_area(C);
		ar = CTX_wm_region(C);
		negate_v3_v3(mousevec, ((RegionView3D *)ar->regiondata)->viewinv[2]);
		normalize_v3(mousevec);
	}

	v3d = sa->spacedata.first;
	rv3d = ar->regiondata;

	/* overwrite the mouse vector with the view direction (zoom into the center) */
	if ((U.uiflag & USER_ZOOM_TO_MOUSEPOS) == 0) {
		normalize_v3_v3(mousevec, rv3d->viewinv[2]);
	}

	if (delta < 0) {
		view_dolly_mouseloc(ar, rv3d->ofs, mousevec, 0.2f);
	}
	else {
		view_dolly_mouseloc(ar, rv3d->ofs, mousevec, 1.8f);
	}

	if (rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(sa, ar);

	ED_view3d_depth_tag_update(rv3d);

	ED_view3d_camera_lock_sync(v3d, rv3d);

	ED_region_tag_redraw(ar);

	viewops_data_free(C, op);

	return OPERATOR_FINISHED;
}

/* copied from viewzoom_invoke(), changes here may apply there */
static int viewdolly_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod;

	if (view3d_operator_offset_lock_check(C, op))
		return OPERATOR_CANCELLED;

	/* makes op->customdata */
	viewops_data_alloc(C, op);
	vod = op->customdata;

	/* poll should check but in some cases fails, see poll func for details */
	if (vod->rv3d->viewlock & RV3D_LOCKED) {
		viewops_data_free(C, op);
		return OPERATOR_PASS_THROUGH;
	}

	/* needs to run before 'viewops_data_create' so the backup 'rv3d->ofs' is correct */
	/* switch from camera view when: */
	if (vod->rv3d->persp != RV3D_PERSP) {
		if (vod->rv3d->persp == RV3D_CAMOB) {
			/* ignore rv3d->lpersp because dolly only makes sense in perspective mode */
			view3d_persp_switch_from_camera(vod->v3d, vod->rv3d, RV3D_PERSP);
		}
		else {
			vod->rv3d->persp = RV3D_PERSP;
		}
		ED_region_tag_redraw(vod->ar);
	}

	viewops_data_create(C, op, event);


	/* if one or the other zoom position aren't set, set from event */
	if (!RNA_struct_property_is_set(op->ptr, "mx") || !RNA_struct_property_is_set(op->ptr, "my")) {
		RNA_int_set(op->ptr, "mx", event->x);
		RNA_int_set(op->ptr, "my", event->y);
	}

	if (RNA_struct_property_is_set(op->ptr, "delta")) {
		viewdolly_exec(C, op);
	}
	else {
		/* overwrite the mouse vector with the view direction (zoom into the center) */
		if ((U.uiflag & USER_ZOOM_TO_MOUSEPOS) == 0) {
			negate_v3_v3(vod->mousevec, vod->rv3d->viewinv[2]);
			normalize_v3(vod->mousevec);
		}

		if (event->type == MOUSEZOOM) {
			/* Bypass Zoom invert flag for track pads (pass false always) */

			if (U.uiflag & USER_ZOOM_HORIZ) {
				vod->origx = vod->oldx = event->x;
				viewdolly_apply(vod, event->prevx, event->prevy, (U.uiflag & USER_ZOOM_INVERT) == 0);
			}
			else {

				/* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
				vod->origy = vod->oldy = vod->origy + event->x - event->prevx;
				viewdolly_apply(vod, event->prevx, event->prevy, (U.uiflag & USER_ZOOM_INVERT) == 0);
			}
			ED_view3d_depth_tag_update(vod->rv3d);

			viewops_data_free(C, op);
			return OPERATOR_FINISHED;
		}
		else {
			/* add temp handler */
			WM_event_add_modal_handler(C, op);

			return OPERATOR_RUNNING_MODAL;
		}
	}
	return OPERATOR_FINISHED;
}

static void viewdolly_cancel(bContext *C, wmOperator *op)
{
	viewops_data_free(C, op);
}

void VIEW3D_OT_dolly(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dolly View";
	ot->description = "Dolly in/out in the view";
	ot->idname = "VIEW3D_OT_dolly";

	/* api callbacks */
	ot->invoke = viewdolly_invoke;
	ot->exec = viewdolly_exec;
	ot->modal = viewdolly_modal;
	ot->poll = ED_operator_region_view3d_active;
	ot->cancel = viewdolly_cancel;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_POINTER;

	RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mx", 0, 0, INT_MAX, "Zoom Position X", "", 0, INT_MAX);
	RNA_def_int(ot->srna, "my", 0, 0, INT_MAX, "Zoom Position Y", "", 0, INT_MAX);
}

static void view3d_from_minmax(bContext *C, View3D *v3d, ARegion *ar,
                               const float min[3], const float max[3],
                               bool ok_dist, const int smooth_viewtx)
{
	RegionView3D *rv3d = ar->regiondata;
	float afm[3];
	float size;

	/* SMOOTHVIEW */
	float new_ofs[3];
	float new_dist;

	sub_v3_v3v3(afm, max, min);
	size = max_fff(afm[0], afm[1], afm[2]);

	if (ok_dist) {
		/* fix up zoom distance if needed */

		if (rv3d->is_persp) {
			float lens, sensor_size;
			/* offset the view based on the lens */
			if (rv3d->persp == RV3D_CAMOB && ED_view3d_camera_lock_check(v3d, rv3d)) {
				CameraParams params;
				BKE_camera_params_init(&params);
				params.clipsta = v3d->near;
				params.clipend = v3d->far;
				BKE_camera_params_from_object(&params, v3d->camera);

				lens = params.lens;
				sensor_size = BKE_camera_sensor_size(params.sensor_fit, params.sensor_x, params.sensor_y);
			}
			else {
				lens = v3d->lens;
				sensor_size = DEFAULT_SENSOR_WIDTH;
			}
			size = ED_view3d_radius_to_persp_dist(focallength_to_fov(lens, sensor_size), size / 2.0f) * VIEW3D_MARGIN;

			/* do not zoom closer than the near clipping plane */
			size = max_ff(size, v3d->near * 1.5f);
		}
		else { /* ortho */
			if (size < 0.0001f) {
				/* bounding box was a single point so do not zoom */
				ok_dist = false;
			}
			else {
				/* adjust zoom so it looks nicer */
				size = ED_view3d_radius_to_ortho_dist(v3d->lens, size / 2.0f) * VIEW3D_MARGIN;
			}
		}
	}

	mid_v3_v3v3(new_ofs, min, max);
	negate_v3(new_ofs);

	new_dist = size;

	/* correction for window aspect ratio */
	if (ar->winy > 2 && ar->winx > 2) {
		size = (float)ar->winx / (float)ar->winy;
		if (size < 1.0f) size = 1.0f / size;
		new_dist *= size;
	}

	if (rv3d->persp == RV3D_CAMOB && !ED_view3d_camera_lock_check(v3d, rv3d)) {
		rv3d->persp = RV3D_PERSP;
		ED_view3d_smooth_view(C, v3d, ar, v3d->camera, NULL,
		                      new_ofs, NULL, ok_dist ? &new_dist : NULL, NULL,
		                      smooth_viewtx);
	}
	else {
		ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
		                      new_ofs, NULL, ok_dist ? &new_dist : NULL, NULL,
		                      smooth_viewtx);
	}

	/* smooth view does viewlock RV3D_BOXVIEW copy */
}

/* same as view3d_from_minmax but for all regions (except cameras) */
static void view3d_from_minmax_multi(bContext *C, View3D *v3d,
                                     const float min[3], const float max[3],
                                     const bool ok_dist, const int smooth_viewtx)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar;
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3d = ar->regiondata;
			/* when using all regions, don't jump out of camera view,
			 * but _do_ allow locked cameras to be moved */
			if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
				view3d_from_minmax(C, v3d, ar, min, max, ok_dist, smooth_viewtx);
			}
		}
	}
}

static int view3d_all_exec(bContext *C, wmOperator *op) /* was view3d_home() in 2.4x */
{
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	Base *base;
	float *curs;
	const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
	const bool skip_camera = (ED_view3d_camera_lock_check(v3d, ar->regiondata) ||
	                          /* any one of the regions may be locked */
	                          (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));
	const bool center = RNA_boolean_get(op->ptr, "center");
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

	float min[3], max[3];
	bool changed = false;

	if (center) {
		/* in 2.4x this also move the cursor to (0, 0, 0) (with shift+c). */
		curs = ED_view3d_cursor3d_get(scene, v3d);
		zero_v3(min);
		zero_v3(max);
		zero_v3(curs);
	}
	else {
		INIT_MINMAX(min, max);
	}

	for (base = scene->base.first; base; base = base->next) {
		if (BASE_VISIBLE(v3d, base)) {
			changed = true;

			if (skip_camera && base->object == v3d->camera) {
				continue;
			}

			BKE_object_minmax(base->object, min, max, false);
		}
	}
	if (!changed) {
		ED_region_tag_redraw(ar);
		/* TODO - should this be cancel?
		 * I think no, because we always move the cursor, with or without
		 * object, but in this case there is no change in the scene,
		 * only the cursor so I choice a ED_region_tag like
		 * view3d_smooth_view do for the center_cursor.
		 * See bug #22640
		 */
		return OPERATOR_FINISHED;
	}

	if (use_all_regions) {
		view3d_from_minmax_multi(C, v3d, min, max, true, smooth_viewtx);
	}
	else {
		view3d_from_minmax(C, v3d, ar, min, max, true, smooth_viewtx);
	}

	return OPERATOR_FINISHED;
}


void VIEW3D_OT_view_all(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View All";
	ot->description = "View all objects in scene";
	ot->idname = "VIEW3D_OT_view_all";

	/* api callbacks */
	ot->exec = view3d_all_exec;
	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;

	prop = RNA_def_boolean(ot->srna, "use_all_regions", 0, "All Regions", "View selected for all regions");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	RNA_def_boolean(ot->srna, "center", 0, "Center", "");
}

/* like a localview without local!, was centerview() in 2.4x */
static int viewselected_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	Object *obedit = CTX_data_edit_object(C);
	float min[3], max[3];
	bool ok = false, ok_dist = true;
	const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
	const bool skip_camera = (ED_view3d_camera_lock_check(v3d, ar->regiondata) ||
	                          /* any one of the regions may be locked */
	                          (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

	INIT_MINMAX(min, max);

	if (ob && (ob->mode & OB_MODE_WEIGHT_PAINT)) {
		/* hard-coded exception, we look for the one selected armature */
		/* this is weak code this way, we should make a generic active/selection callback interface once... */
		Base *base;
		for (base = scene->base.first; base; base = base->next) {
			if (TESTBASELIB(v3d, base)) {
				if (base->object->type == OB_ARMATURE)
					if (base->object->mode & OB_MODE_POSE)
						break;
			}
		}
		if (base)
			ob = base->object;
	}


	if (obedit) {
		ok = ED_view3d_minmax_verts(obedit, min, max);    /* only selected */
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		if (ob->pose) {
			bArmature *arm = ob->data;
			bPoseChannel *pchan;
			float vec[3];

			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				if (pchan->bone->flag & BONE_SELECTED) {
					if (pchan->bone->layer & arm->layer) {
						bPoseChannel *pchan_tx = pchan->custom_tx ? pchan->custom_tx : pchan;
						ok = 1;
						mul_v3_m4v3(vec, ob->obmat, pchan_tx->pose_head);
						minmax_v3v3_v3(min, max, vec);
						mul_v3_m4v3(vec, ob->obmat, pchan_tx->pose_tail);
						minmax_v3v3_v3(min, max, vec);
					}
				}
			}
		}
	}
	else if (BKE_paint_select_face_test(ob)) {
		ok = paintface_minmax(ob, min, max);
	}
	else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT)) {
		ok = PE_minmax(scene, min, max);
	}
	else if (ob && (ob->mode & OB_MODE_SCULPT)) {
		ok = ED_sculpt_minmax(C, min, max);
		ok_dist = 0; /* don't zoom */
	}
	else {
		Base *base;
		for (base = FIRSTBASE; base; base = base->next) {
			if (TESTBASE(v3d, base)) {

				if (skip_camera && base->object == v3d->camera) {
					continue;
				}

				/* account for duplis */
				if (BKE_object_minmax_dupli(scene, base->object, min, max, false) == 0)
					BKE_object_minmax(base->object, min, max, false);  /* use if duplis not found */

				ok = 1;
			}
		}
	}

	if (ok == 0) {
		return OPERATOR_FINISHED;
	}

	if (use_all_regions) {
		view3d_from_minmax_multi(C, v3d, min, max, ok_dist, smooth_viewtx);
	}
	else {
		view3d_from_minmax(C, v3d, ar, min, max, ok_dist, smooth_viewtx);
	}

// XXX	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_selected(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Selected";
	ot->description = "Move the view to the selection center";
	ot->idname = "VIEW3D_OT_view_selected";

	/* api callbacks */
	ot->exec = viewselected_exec;
	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;

	/* rna later */
	prop = RNA_def_boolean(ot->srna, "use_all_regions", 0, "All Regions", "View selected for all regions");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int view_lock_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);

	if (v3d) {
		ED_view3D_lock_clear(v3d);

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void VIEW3D_OT_view_lock_clear(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "View Lock Clear";
	ot->description = "Clear all view locking";
	ot->idname = "VIEW3D_OT_view_lock_clear";

	/* api callbacks */
	ot->exec = view_lock_clear_exec;
	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;
}

static int view_lock_to_active_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	Object *obact = CTX_data_active_object(C);

	if (v3d) {

		ED_view3D_lock_clear(v3d);

		v3d->ob_centre = obact; /* can be NULL */

		if (obact && obact->type == OB_ARMATURE) {
			if (obact->mode & OB_MODE_POSE) {
				bPoseChannel *pcham_act = BKE_pose_channel_active(obact);
				if (pcham_act) {
					BLI_strncpy(v3d->ob_centre_bone, pcham_act->name, sizeof(v3d->ob_centre_bone));
				}
			}
			else {
				EditBone *ebone_act = ((bArmature *)obact->data)->act_edbone;
				if (ebone_act) {
					BLI_strncpy(v3d->ob_centre_bone, ebone_act->name, sizeof(v3d->ob_centre_bone));
				}
			}
		}

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void VIEW3D_OT_view_lock_to_active(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "View Lock to Active";
	ot->description = "Lock the view to the active object/bone";
	ot->idname = "VIEW3D_OT_view_lock_to_active";

	/* api callbacks */
	ot->exec = view_lock_to_active_exec;
	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;
}

static int viewcenter_cursor_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Scene *scene = CTX_data_scene(C);
	
	if (rv3d) {
		ARegion *ar = CTX_wm_region(C);
		const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

		/* non camera center */
		float new_ofs[3];
		negate_v3_v3(new_ofs, ED_view3d_cursor3d_get(scene, v3d));
		ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
		                      new_ofs, NULL, NULL, NULL,
		                      smooth_viewtx);
		
		/* smooth view does viewlock RV3D_BOXVIEW copy */
	}
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Center View to Cursor";
	ot->description = "Center the view so that the cursor is in the middle of the view";
	ot->idname = "VIEW3D_OT_view_center_cursor";
	
	/* api callbacks */
	ot->exec = viewcenter_cursor_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = 0;
}

static int viewcenter_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);

	if (rv3d) {
		float new_ofs[3];
		const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

		view3d_operator_needs_opengl(C);

		if (ED_view3d_autodist(scene, ar, v3d, event->mval, new_ofs, false, NULL)) {
			/* pass */
		}
		else {
			/* fallback to simple pan */
			negate_v3_v3(new_ofs, rv3d->ofs);
			ED_view3d_win_to_3d_int(ar, new_ofs, event->mval, new_ofs);
		}
		negate_v3(new_ofs);
		ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
		                      new_ofs, NULL, NULL, NULL,
		                      smooth_viewtx);
	}

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Center View to Mouse";
	ot->description = "Center the view to the Z-depth position under the mouse cursor";
	ot->idname = "VIEW3D_OT_view_center_pick";

	/* api callbacks */
	ot->invoke = viewcenter_pick_invoke;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}

static int view3d_center_camera_exec(bContext *C, wmOperator *UNUSED(op)) /* was view3d_home() in 2.4x */
{
	Scene *scene = CTX_data_scene(C);
	float xfac, yfac;
	float size[2];

	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	rv3d->camdx = rv3d->camdy = 0.0f;

	ED_view3d_calc_camera_border_size(scene, ar, v3d, rv3d, size);

	/* 4px is just a little room from the edge of the area */
	xfac = (float)ar->winx / (float)(size[0] + 4);
	yfac = (float)ar->winy / (float)(size[1] + 4);

	rv3d->camzoom = BKE_screen_view3d_zoom_from_fac(min_ff(xfac, yfac));
	CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_camera(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Camera Center";
	ot->description = "Center the camera view";
	ot->idname = "VIEW3D_OT_view_center_camera";

	/* api callbacks */
	ot->exec = view3d_center_camera_exec;
	ot->poll = view3d_camera_user_poll;

	/* flags */
	ot->flag = 0;
}

static int view3d_center_lock_exec(bContext *C, wmOperator *UNUSED(op)) /* was view3d_home() in 2.4x */
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);

	zero_v2(rv3d->ofs_lock);

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_lock(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Lock Center";
	ot->description = "Center the view lock offset";
	ot->idname = "VIEW3D_OT_view_center_lock";

	/* api callbacks */
	ot->exec = view3d_center_lock_exec;
	ot->poll = view3d_lock_poll;

	/* flags */
	ot->flag = 0;
}

/* ********************* Set render border operator ****************** */

static int render_border_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	Scene *scene = CTX_data_scene(C);

	rcti rect;
	rctf vb, border;

	const bool camera_only = RNA_boolean_get(op->ptr, "camera_only");

	if (camera_only && rv3d->persp != RV3D_CAMOB)
		return OPERATOR_PASS_THROUGH;

	/* get border select values using rna */
	WM_operator_properties_border_to_rcti(op, &rect);

	/* calculate range */

	if (rv3d->persp == RV3D_CAMOB) {
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &vb, false);
	}
	else {
		vb.xmin = 0;
		vb.ymin = 0;
		vb.xmax = ar->winx;
		vb.ymax = ar->winy;
	}

	border.xmin = ((float)rect.xmin - vb.xmin) / BLI_rctf_size_x(&vb);
	border.ymin = ((float)rect.ymin - vb.ymin) / BLI_rctf_size_y(&vb);
	border.xmax = ((float)rect.xmax - vb.xmin) / BLI_rctf_size_x(&vb);
	border.ymax = ((float)rect.ymax - vb.ymin) / BLI_rctf_size_y(&vb);

	/* actually set border */
	CLAMP(border.xmin, 0.0f, 1.0f);
	CLAMP(border.ymin, 0.0f, 1.0f);
	CLAMP(border.xmax, 0.0f, 1.0f);
	CLAMP(border.ymax, 0.0f, 1.0f);

	if (rv3d->persp == RV3D_CAMOB) {
		scene->r.border = border;

		WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	}
	else {
		v3d->render_border = border;

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
	}

	/* drawing a border surrounding the entire camera view switches off border rendering
	 * or the border covers no pixels */
	if ((border.xmin <= 0.0f && border.xmax >= 1.0f &&
	     border.ymin <= 0.0f && border.ymax >= 1.0f) ||
	    (border.xmin == border.xmax || border.ymin == border.ymax))
	{
		if (rv3d->persp == RV3D_CAMOB)
			scene->r.mode &= ~R_BORDER;
		else
			v3d->flag2 &= ~V3D_RENDER_BORDER;
	}
	else {
		if (rv3d->persp == RV3D_CAMOB)
			scene->r.mode |= R_BORDER;
		else
			v3d->flag2 |= V3D_RENDER_BORDER;
	}

	return OPERATOR_FINISHED;

}

void VIEW3D_OT_render_border(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Set Render Border";
	ot->description = "Set the boundaries of the border render and enable border render";
	ot->idname = "VIEW3D_OT_render_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = render_border_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;

	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* rna */
	WM_operator_properties_border(ot);

	prop = RNA_def_boolean(ot->srna, "camera_only", 0, "Camera Only", "Set render border for camera view and final render only");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* ********************* Clear render border operator ****************** */

static int clear_render_border_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	Scene *scene = CTX_data_scene(C);
	rctf *border = NULL;

	if (rv3d->persp == RV3D_CAMOB) {
		scene->r.mode &= ~R_BORDER;
		border = &scene->r.border;

		WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	}
	else {
		v3d->flag2 &= ~V3D_RENDER_BORDER;
		border = &v3d->render_border;

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
	}

	border->xmin = 0.0f;
	border->ymin = 0.0f;
	border->xmax = 1.0f;
	border->ymax = 1.0f;

	return OPERATOR_FINISHED;

}

void VIEW3D_OT_clear_render_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Render Border";
	ot->description = "Clear the boundaries of the border render and disable border render";
	ot->idname = "VIEW3D_OT_clear_render_border";

	/* api callbacks */
	ot->exec = clear_render_border_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************* Border Zoom operator ****************** */

static int view3d_zoom_border_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Scene *scene = CTX_data_scene(C);
	int gesture_mode;
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

	/* Zooms in on a border drawn by the user */
	rcti rect;
	float dvec[3], vb[2], xscale, yscale;
	float dist_range_min;

	/* SMOOTHVIEW */
	float new_dist;
	float new_ofs[3];

	/* ZBuffer depth vars */
	bglMats mats;
	float depth_close = FLT_MAX;
	double cent[2],  p[3];

	/* note; otherwise opengl won't work */
	view3d_operator_needs_opengl(C);

	/* get border select values using rna */
	WM_operator_properties_border_to_rcti(op, &rect);

	/* check if zooming in/out view */
	gesture_mode = RNA_int_get(op->ptr, "gesture_mode");

	/* Get Z Depths, needed for perspective, nice for ortho */
	bgl_get_mats(&mats);
	ED_view3d_draw_depth(scene, ar, v3d, true);
	
	{
		/* avoid allocating the whole depth buffer */
		ViewDepths depth_temp = {0};

		/* avoid view3d_update_depths() for speed. */
		view3d_update_depths_rect(ar, &depth_temp, &rect);
	
		/* find the closest Z pixel */
		depth_close = view3d_depth_near(&depth_temp);
	
		MEM_freeN(depth_temp.depths);
	}

	cent[0] = (((double)rect.xmin) + ((double)rect.xmax)) / 2;
	cent[1] = (((double)rect.ymin) + ((double)rect.ymax)) / 2;

	if (rv3d->is_persp) {
		double p_corner[3];

		/* no depths to use, we cant do anything! */
		if (depth_close == FLT_MAX) {
			BKE_report(op->reports, RPT_ERROR, "Depth too large");
			return OPERATOR_CANCELLED;
		}
		/* convert border to 3d coordinates */
		if ((!gluUnProject(cent[0], cent[1], depth_close,
		                   mats.modelview, mats.projection, (GLint *)mats.viewport,
		                   &p[0], &p[1], &p[2])) ||
		    (!gluUnProject((double)rect.xmin, (double)rect.ymin, depth_close,
		                   mats.modelview, mats.projection, (GLint *)mats.viewport,
		                   &p_corner[0], &p_corner[1], &p_corner[2])))
		{
			return OPERATOR_CANCELLED;
		}

		dvec[0] = p[0] - p_corner[0];
		dvec[1] = p[1] - p_corner[1];
		dvec[2] = p[2] - p_corner[2];

		new_ofs[0] = -p[0];
		new_ofs[1] = -p[1];
		new_ofs[2] = -p[2];

		new_dist = len_v3(dvec);
		dist_range_min = v3d->near * 1.5f;

	}
	else { /* othographic */
		   /* find the current window width and height */
		vb[0] = ar->winx;
		vb[1] = ar->winy;

		new_dist = rv3d->dist;

		/* convert the drawn rectangle into 3d space */
		if (depth_close != FLT_MAX && gluUnProject(cent[0], cent[1], depth_close,
		                                           mats.modelview, mats.projection, (GLint *)mats.viewport,
		                                           &p[0], &p[1], &p[2]))
		{
			new_ofs[0] = -p[0];
			new_ofs[1] = -p[1];
			new_ofs[2] = -p[2];
		}
		else {
			float mval_f[2];
			float zfac;

			/* We cant use the depth, fallback to the old way that dosnt set the center depth */
			copy_v3_v3(new_ofs, rv3d->ofs);

			{
				float tvec[3];
				negate_v3_v3(tvec, new_ofs);
				zfac = ED_view3d_calc_zfac(rv3d, tvec, NULL);
			}

			mval_f[0] = (rect.xmin + rect.xmax - vb[0]) / 2.0f;
			mval_f[1] = (rect.ymin + rect.ymax - vb[1]) / 2.0f;
			ED_view3d_win_to_delta(ar, mval_f, dvec, zfac);
			/* center the view to the center of the rectangle */
			sub_v3_v3(new_ofs, dvec);
		}

		/* work out the ratios, so that everything selected fits when we zoom */
		xscale = (BLI_rcti_size_x(&rect) / vb[0]);
		yscale = (BLI_rcti_size_y(&rect) / vb[1]);
		new_dist *= max_ff(xscale, yscale);

		/* zoom in as required, or as far as we can go */
		dist_range_min = 0.001f * v3d->grid;
	}

	if (gesture_mode == GESTURE_MODAL_OUT) {
		sub_v3_v3v3(dvec, new_ofs, rv3d->ofs);
		new_dist = rv3d->dist * (rv3d->dist / new_dist);
		add_v3_v3v3(new_ofs, rv3d->ofs, dvec);
	}

	/* clamp after because we may have been zooming out */
	if (new_dist < dist_range_min) {
		new_dist = dist_range_min;
	}

	ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
	                      new_ofs, NULL, &new_dist, NULL,
	                      smooth_viewtx);

	if (rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(CTX_wm_area(C), ar);

	return OPERATOR_FINISHED;
}

static int view3d_zoom_border_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);

	/* if in camera view do not exec the operator so we do not conflict with set render border*/
	if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d))
		return WM_border_select_invoke(C, op, event);
	else
		return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_zoom_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Zoom to Border";
	ot->description = "Zoom in the view to the nearest object contained in the border";
	ot->idname = "VIEW3D_OT_zoom_border";

	/* api callbacks */
	ot->invoke = view3d_zoom_border_invoke;
	ot->exec = view3d_zoom_border_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;

	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;

	/* rna */
	WM_operator_properties_gesture_border(ot, false);
}

/* sets the view to 1:1 camera/render-pixel */
static void view3d_set_1_to_1_viewborder(Scene *scene, ARegion *ar, View3D *v3d)
{
	RegionView3D *rv3d = ar->regiondata;
	float size[2];
	int im_width = (scene->r.size * scene->r.xsch) / 100;
	
	ED_view3d_calc_camera_border_size(scene, ar, v3d, rv3d, size);

	rv3d->camzoom = BKE_screen_view3d_zoom_from_fac((float)im_width / size[0]);
	CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
}

static int view3d_zoom_1_to_1_camera_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	View3D *v3d;
	ARegion *ar;

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d, &ar);

	view3d_set_1_to_1_viewborder(scene, ar, v3d);

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_zoom_camera_1_to_1(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Zoom Camera 1:1";
	ot->description = "Match the camera to 1:1 to the render output";
	ot->idname = "VIEW3D_OT_zoom_camera_1_to_1";

	/* api callbacks */
	ot->exec = view3d_zoom_1_to_1_camera_exec;
	ot->poll = view3d_camera_user_poll;

	/* flags */
	ot->flag = 0;
}

/* ********************* Changing view operator ****************** */

static EnumPropertyItem prop_view_items[] = {
	{RV3D_VIEW_LEFT, "LEFT", ICON_TRIA_LEFT, "Left", "View From the Left"},
	{RV3D_VIEW_RIGHT, "RIGHT", ICON_TRIA_RIGHT, "Right", "View From the Right"},
	{RV3D_VIEW_BOTTOM, "BOTTOM", ICON_TRIA_DOWN, "Bottom", "View From the Bottom"},
	{RV3D_VIEW_TOP, "TOP", ICON_TRIA_UP, "Top", "View From the Top"},
	{RV3D_VIEW_FRONT, "FRONT", 0, "Front", "View From the Front"},
	{RV3D_VIEW_BACK, "BACK", 0, "Back", "View From the Back"},
	{RV3D_VIEW_CAMERA, "CAMERA", ICON_CAMERA_DATA, "Camera", "View From the Active Camera"},
	{0, NULL, 0, NULL, NULL}
};


/* would like to make this a generic function - outside of transform */

static void axis_set_view(bContext *C, View3D *v3d, ARegion *ar,
                          const float quat_[4],
                          short view, int perspo, bool align_active,
                          const int smooth_viewtx)
{
	RegionView3D *rv3d = ar->regiondata; /* no NULL check is needed, poll checks */
	float quat[4];

	normalize_qt_qt(quat, quat_);

	if (align_active) {
		/* align to active object */
		Object *obact = CTX_data_active_object(C);
		if (obact == NULL) {
			/* no active object, ignore this option */
			align_active = false;
		}
		else {
			float obact_quat[4];
			float twmat[3][3];

			/* same as transform manipulator when normal is set */
			ED_getTransformOrientationMatrix(C, twmat, true);

			mat3_to_quat(obact_quat, twmat);
			invert_qt(obact_quat);
			mul_qt_qtqt(quat, quat, obact_quat);

			rv3d->view = view = RV3D_VIEW_USER;
		}
	}

	if (align_active == false) {
		/* normal operation */
		if (rv3d->viewlock & RV3D_LOCKED) {
			/* only pass on if */

			/* nice confusing if-block */
			if (!((rv3d->view == RV3D_VIEW_FRONT  && view == RV3D_VIEW_BACK)  ||
			      (rv3d->view == RV3D_VIEW_BACK   && view == RV3D_VIEW_FRONT) ||
			      (rv3d->view == RV3D_VIEW_RIGHT  && view == RV3D_VIEW_LEFT)  ||
			      (rv3d->view == RV3D_VIEW_LEFT   && view == RV3D_VIEW_RIGHT) ||
			      (rv3d->view == RV3D_VIEW_BOTTOM && view == RV3D_VIEW_TOP)   ||
			      (rv3d->view == RV3D_VIEW_TOP    && view == RV3D_VIEW_BOTTOM)))
			{
				return;
			}
		}

		rv3d->view = view;
	}

	if (rv3d->viewlock & RV3D_LOCKED) {
		ED_region_tag_redraw(ar);
		return;
	}

	if (U.uiflag & USER_AUTOPERSP) {
		rv3d->persp = RV3D_VIEW_IS_AXIS(view) ? RV3D_ORTHO : perspo;
	}
	else if (rv3d->persp == RV3D_CAMOB) {
		rv3d->persp = perspo;
	}

	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		ED_view3d_smooth_view(C, v3d, ar, v3d->camera, NULL,
		                      rv3d->ofs, quat, NULL, NULL,
		                      smooth_viewtx);
	}
	else {
		ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
		                      NULL, quat, NULL, NULL,
		                      smooth_viewtx);
	}

}

static int viewnumpad_exec(bContext *C, wmOperator *op)
{
	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;
	Scene *scene = CTX_data_scene(C);
	static int perspo = RV3D_PERSP;
	int viewnum, nextperspo;
	bool align_active;
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	viewnum = RNA_enum_get(op->ptr, "type");
	align_active = RNA_boolean_get(op->ptr, "align_active");

	/* set this to zero, gets handled in axis_set_view */
	if (rv3d->viewlock & RV3D_LOCKED)
		align_active = false;

	/* Use this to test if we started out with a camera */

	if (rv3d->persp == RV3D_CAMOB) {
		nextperspo = rv3d->lpersp;
	}
	else {
		nextperspo = perspo;
	}

	if (RV3D_VIEW_IS_AXIS(viewnum)) {
		float quat[4];

		ED_view3d_quat_from_axis_view(viewnum, quat);
		axis_set_view(C, v3d, ar, quat, viewnum, nextperspo, align_active, smooth_viewtx);
	}
	else if (viewnum == RV3D_VIEW_CAMERA) {
		if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
			/* lastview -  */

			if (rv3d->persp != RV3D_CAMOB) {
				Object *ob = OBACT;

				if (!rv3d->smooth_timer) {
					/* store settings of current view before allowing overwriting with camera view
					 * only if we're not currently in a view transition */

					ED_view3d_lastview_store(rv3d);
				}

#if 0
				if (G.qual == LR_ALTKEY) {
					if (oldcamera && is_an_active_object(oldcamera)) {
						v3d->camera = oldcamera;
					}
					handle_view3d_lock();
				}
#endif

				/* first get the default camera for the view lock type */
				if (v3d->scenelock) {
					/* sets the camera view if available */
					v3d->camera = scene->camera;
				}
				else {
					/* use scene camera if one is not set (even though we're unlocked) */
					if (v3d->camera == NULL) {
						v3d->camera = scene->camera;
					}
				}

				/* if the camera isn't found, check a number of options */
				if (v3d->camera == NULL && ob && ob->type == OB_CAMERA)
					v3d->camera = ob;

				if (v3d->camera == NULL)
					v3d->camera = BKE_scene_camera_find(scene);

				/* couldnt find any useful camera, bail out */
				if (v3d->camera == NULL)
					return OPERATOR_CANCELLED;

				/* important these don't get out of sync for locked scenes */
				if (v3d->scenelock)
					scene->camera = v3d->camera;

				/* finally do snazzy view zooming */
				rv3d->persp = RV3D_CAMOB;
				ED_view3d_smooth_view(C, v3d, ar, NULL, v3d->camera,
				                      rv3d->ofs, rv3d->viewquat, &rv3d->dist, &v3d->lens,
				                      smooth_viewtx);

			}
			else {
				/* return to settings of last view */
				/* does view3d_smooth_view too */
				axis_set_view(C, v3d, ar,
				              rv3d->lviewquat,
				              rv3d->lview, rv3d->lpersp, 0,
				              smooth_viewtx);
			}
		}
	}

	if (rv3d->persp != RV3D_CAMOB) perspo = rv3d->persp;

	return OPERATOR_FINISHED;
}


void VIEW3D_OT_viewnumpad(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Numpad";
	ot->description = "Use a preset viewpoint";
	ot->idname = "VIEW3D_OT_viewnumpad";

	/* api callbacks */
	ot->exec = viewnumpad_exec;
	ot->poll = ED_operator_rv3d_user_region_poll;

	/* flags */
	ot->flag = 0;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_view_items, 0, "View", "Preset viewpoint to use");
	RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "align_active", 0, "Align Active", "Align to the active object's axis");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static EnumPropertyItem prop_view_orbit_items[] = {
	{V3D_VIEW_STEPLEFT, "ORBITLEFT", 0, "Orbit Left", "Orbit the view around to the Left"},
	{V3D_VIEW_STEPRIGHT, "ORBITRIGHT", 0, "Orbit Right", "Orbit the view around to the Right"},
	{V3D_VIEW_STEPUP, "ORBITUP", 0, "Orbit Up", "Orbit the view Up"},
	{V3D_VIEW_STEPDOWN, "ORBITDOWN", 0, "Orbit Down", "Orbit the view Down"},
	{0, NULL, 0, NULL, NULL}
};

static int vieworbit_exec(bContext *C, wmOperator *op)
{
	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;
	int orbitdir;

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	orbitdir = RNA_enum_get(op->ptr, "type");

	if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
		if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
			int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
			float angle = DEG2RADF((float)U.pad_rot_angle);
			float quat_mul[4];
			float quat_new[4];
			float ofs_new[3];
			float *ofs_new_pt = NULL;

			view3d_ensure_persp(v3d, ar);

			if (ELEM(orbitdir, V3D_VIEW_STEPLEFT, V3D_VIEW_STEPRIGHT)) {
				const float zvec[3] = {0.0f, 0.0f, 1.0f};

				if (orbitdir == V3D_VIEW_STEPRIGHT) {
					angle = -angle;
				}

				/* z-axis */
				axis_angle_normalized_to_quat(quat_mul, zvec, angle);
			}
			else {

				if (orbitdir == V3D_VIEW_STEPDOWN) {
					angle = -angle;
				}

				/* horizontal axis */
				axis_angle_to_quat(quat_mul, rv3d->viewinv[0], angle);
			}

			mul_qt_qtqt(quat_new, rv3d->viewquat, quat_mul);
			rv3d->view = RV3D_VIEW_USER;

			if (U.uiflag & USER_ORBIT_SELECTION) {
				float dyn_ofs[3];

				view3d_orbit_calc_center(C, dyn_ofs);
				negate_v3(dyn_ofs);

				copy_v3_v3(ofs_new, rv3d->ofs);

				view3d_orbit_apply_dyn_ofs(ofs_new, dyn_ofs, rv3d->viewquat, quat_new);
				ofs_new_pt = ofs_new;

				/* disable smoothview in this case
				 * although it works OK, it looks a little odd. */
				smooth_viewtx = 0;
			}

			ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
			                      ofs_new_pt, quat_new, NULL, NULL,
			                      smooth_viewtx);

			return OPERATOR_FINISHED;
		}
	}

	return OPERATOR_CANCELLED;
}

void VIEW3D_OT_view_orbit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Orbit";
	ot->description = "Orbit the view";
	ot->idname = "VIEW3D_OT_view_orbit";

	/* api callbacks */
	ot->exec = vieworbit_exec;
	ot->poll = ED_operator_rv3d_user_region_poll;

	/* flags */
	ot->flag = 0;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_view_orbit_items, 0, "Orbit", "Direction of View Orbit");
}


/* ************************ viewroll ******************************** */

static void view_roll_angle(ARegion *ar, float quat[4], const float orig_quat[4], const float dvec[3], float angle)
{
	RegionView3D *rv3d = ar->regiondata;
	float quat_mul[4];

	/* camera axis */
	axis_angle_normalized_to_quat(quat_mul, dvec, angle);

	mul_qt_qtqt(quat, orig_quat, quat_mul);
	rv3d->view = RV3D_VIEW_USER;
}

static void viewroll_apply(ViewOpsData *vod, int x, int UNUSED(y))
{
	float angle = 0.0;

	{
		float len1, len2, tot;

		tot = vod->ar->winrct.xmax - vod->ar->winrct.xmin;
		len1 = (vod->ar->winrct.xmax - x) / tot;
		len2 = (vod->ar->winrct.xmax - vod->origx) / tot;
		angle = (len1 - len2) * (float)M_PI * 4.0f;
	}

	if (angle != 0.0f)
		view_roll_angle(vod->ar, vod->rv3d->viewquat, vod->oldquat, vod->mousevec, angle);

	if (vod->rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(vod->sa, vod->ar);

	ED_view3d_camera_lock_sync(vod->v3d, vod->rv3d);

	ED_region_tag_redraw(vod->ar);
}

static int viewroll_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod = op->customdata;
	short event_code = VIEW_PASS;

	/* execute the events */
	if (event->type == MOUSEMOVE) {
		event_code = VIEW_APPLY;
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case VIEW_MODAL_CONFIRM:
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_MOVE:
				WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
			case VIEWROT_MODAL_SWITCH_ROTATE:
				WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
				event_code = VIEW_CONFIRM;
				break;
		}
	}
	else if (event->type == vod->origkey && event->val == KM_RELEASE) {
		event_code = VIEW_CONFIRM;
	}

	if (event_code == VIEW_APPLY) {
		viewroll_apply(vod, event->x, event->y);
	}
	else if (event_code == VIEW_CONFIRM) {
		ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, true, false);
		ED_view3d_depth_tag_update(vod->rv3d);
		viewops_data_free(C, op);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int viewroll_exec(bContext *C, wmOperator *op)
{
	View3D *v3d;
	RegionView3D *rv3d;
	ARegion *ar;

	if (op->customdata) {
		ViewOpsData *vod = op->customdata;
		ar = vod->ar;
		v3d = vod->v3d;
	}
	else {
		ED_view3d_context_user_region(C, &v3d, &ar);
	}

	rv3d = ar->regiondata;
	if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
		const float angle = RNA_float_get(op->ptr, "angle");
		float mousevec[3];
		float quat_new[4];

		const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

		normalize_v3_v3(mousevec, rv3d->viewinv[2]);
		negate_v3(mousevec);
		view_roll_angle(ar, quat_new, rv3d->viewquat, mousevec, angle);

		ED_view3d_smooth_view(C, v3d, ar, NULL, NULL,
		                      NULL, quat_new, NULL, NULL,
		                      smooth_viewtx);

		viewops_data_free(C, op);
		return OPERATOR_FINISHED;
	}
	else {
		viewops_data_free(C, op);
		return OPERATOR_CANCELLED;
	}
}

static int viewroll_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewOpsData *vod;

	if (RNA_struct_property_is_set(op->ptr, "angle")) {
		viewroll_exec(C, op);
	}
	else {
		/* makes op->customdata */
		viewops_data_alloc(C, op);
		viewops_data_create(C, op, event);
		vod = op->customdata;

		/* overwrite the mouse vector with the view direction */
		normalize_v3_v3(vod->mousevec, vod->rv3d->viewinv[2]);
		negate_v3(vod->mousevec);

		if (event->type == MOUSEROTATE) {
			vod->origx = vod->oldx = event->x;
			viewroll_apply(vod, event->prevx, event->prevy);
			ED_view3d_depth_tag_update(vod->rv3d);

			viewops_data_free(C, op);
			return OPERATOR_FINISHED;
		}
		else {
			/* add temp handler */
			WM_event_add_modal_handler(C, op);

			return OPERATOR_RUNNING_MODAL;
		}
	}
	return OPERATOR_FINISHED;
}

static void viewroll_cancel(bContext *C, wmOperator *op)
{
	viewops_data_free(C, op);
}

void VIEW3D_OT_view_roll(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Roll";
	ot->description = "Roll the view";
	ot->idname = "VIEW3D_OT_view_roll";

	/* api callbacks */
	ot->invoke = viewroll_invoke;
	ot->exec = viewroll_exec;
	ot->modal = viewroll_modal;
	ot->poll = ED_operator_rv3d_user_region_poll;
	ot->cancel = viewroll_cancel;

	/* flags */
	ot->flag = 0;

	/* properties */
	ot->prop = RNA_def_float(ot->srna, "angle", 0, -FLT_MAX, FLT_MAX, "Roll", "", -FLT_MAX, FLT_MAX);
}

static EnumPropertyItem prop_view_pan_items[] = {
	{V3D_VIEW_PANLEFT, "PANLEFT", 0, "Pan Left", "Pan the view to the Left"},
	{V3D_VIEW_PANRIGHT, "PANRIGHT", 0, "Pan Right", "Pan the view to the Right"},
	{V3D_VIEW_PANUP, "PANUP", 0, "Pan Up", "Pan the view Up"},
	{V3D_VIEW_PANDOWN, "PANDOWN", 0, "Pan Down", "Pan the view Down"},
	{0, NULL, 0, NULL, NULL}
};

static int viewpan_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float vec[3];
	const float co_zero[3] = {0.0f};
	float mval_f[2] = {0.0f, 0.0f};
	float zfac;
	int pandir;

	if (view3d_operator_offset_lock_check(C, op))
		return OPERATOR_CANCELLED;

	pandir = RNA_enum_get(op->ptr, "type");

	ED_view3d_camera_lock_init(v3d, rv3d);

	zfac = ED_view3d_calc_zfac(rv3d, co_zero, NULL);
	if      (pandir == V3D_VIEW_PANRIGHT)  { mval_f[0] = -32.0f; }
	else if (pandir == V3D_VIEW_PANLEFT)   { mval_f[0] =  32.0f; }
	else if (pandir == V3D_VIEW_PANUP)     { mval_f[1] = -25.0f; }
	else if (pandir == V3D_VIEW_PANDOWN)   { mval_f[1] =  25.0f; }
	ED_view3d_win_to_delta(ar, mval_f, vec, zfac);
	add_v3_v3(rv3d->ofs, vec);

	if (rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_sync(sa, ar);

	ED_view3d_depth_tag_update(rv3d);

	ED_view3d_camera_lock_sync(v3d, rv3d);

	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Pan";
	ot->description = "Pan the view";
	ot->idname = "VIEW3D_OT_view_pan";

	/* api callbacks */
	ot->exec = viewpan_exec;
	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;
	
	/* Properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_view_pan_items, 0, "Pan", "Direction of View Pan");
}

static int viewpersportho_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d_dummy;
	ARegion *ar;
	RegionView3D *rv3d;

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d_dummy, &ar);
	rv3d = ar->regiondata;

	if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
		if (rv3d->persp != RV3D_ORTHO)
			rv3d->persp = RV3D_ORTHO;
		else rv3d->persp = RV3D_PERSP;
		ED_region_tag_redraw(ar);
	}

	return OPERATOR_FINISHED;

}

void VIEW3D_OT_view_persportho(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Persp/Ortho";
	ot->description = "Switch the current view from perspective/orthographic projection";
	ot->idname = "VIEW3D_OT_view_persportho";

	/* api callbacks */
	ot->exec = viewpersportho_exec;
	ot->poll = ED_operator_rv3d_user_region_poll;

	/* flags */
	ot->flag = 0;
}

static int view3d_navigate_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	eViewNavigation_Method mode = U.navigation_mode;

	switch (mode) {
		case VIEW_NAVIGATION_FLY:
			WM_operator_name_call(C, "VIEW3D_OT_fly", WM_OP_INVOKE_DEFAULT, NULL);
			break;
		case VIEW_NAVIGATION_WALK:
		default:
			WM_operator_name_call(C, "VIEW3D_OT_walk", WM_OP_INVOKE_DEFAULT, NULL);
			break;
	}

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_navigate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Navigation";
	ot->description = "Interactively navigate around the scene (uses the mode (walk/fly) preference)";
	ot->idname = "VIEW3D_OT_navigate";

	/* api callbacks */
	ot->invoke = view3d_navigate_invoke;
	ot->poll = ED_operator_view3d_active;
}


/* ******************** add background image operator **************** */

static BGpic *background_image_add(bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);

	return ED_view3D_background_image_new(v3d);
}

static int background_image_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	background_image_add(C);

	return OPERATOR_FINISHED;
}

static int background_image_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	View3D *v3d = CTX_wm_view3d(C);
	Image *ima = NULL;
	BGpic *bgpic;
	char name[MAX_ID_NAME - 2];
	
	/* check input variables */
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		char path[FILE_MAX];
		
		RNA_string_get(op->ptr, "filepath", path);
		ima = BKE_image_load_exists(path);
	}
	else if (RNA_struct_property_is_set(op->ptr, "name")) {
		RNA_string_get(op->ptr, "name", name);
		ima = (Image *)BKE_libblock_find_name(ID_IM, name);
	}
	
	bgpic = background_image_add(C);
	
	if (ima) {
		bgpic->ima = ima;
		
		id_us_plus(&ima->id);
		
		if (!(v3d->flag & V3D_DISPBGPICS))
			v3d->flag |= V3D_DISPBGPICS;
	}
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_background_image_add(wmOperatorType *ot)
{
	/* identifiers */
	/* note: having key shortcut here is bad practice,
	 * but for now keep because this displays when dragging an image over the 3D viewport */
	ot->name   = "Add Background Image (Ctrl for Empty Object)";
	ot->description = "Add a new background image";
	ot->idname = "VIEW3D_OT_background_image_add";

	/* api callbacks */
	ot->invoke = background_image_add_invoke;
	ot->exec   = background_image_add_exec;
	ot->poll   = ED_operator_view3d_active;

	/* flags */
	ot->flag   = 0;
	
	/* properties */
	RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME - 2, "Name", "Image name to assign");
	RNA_def_string(ot->srna, "filepath", "Path", FILE_MAX, "Filepath", "Path to image file");
}


/* ***** remove image operator ******* */
static int background_image_remove_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	const int index = RNA_int_get(op->ptr, "index");
	BGpic *bgpic_rem = BLI_findlink(&v3d->bgpicbase, index);

	if (bgpic_rem) {
		if (bgpic_rem->source == V3D_BGPIC_IMAGE) {
			id_us_min((ID *)bgpic_rem->ima);
		}
		else if (bgpic_rem->source == V3D_BGPIC_MOVIE) {
			id_us_min((ID *)bgpic_rem->clip);
		}

		ED_view3D_background_image_remove(v3d, bgpic_rem);

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}

}

void VIEW3D_OT_background_image_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Remove Background Image";
	ot->description = "Remove a background image from the 3D view";
	ot->idname = "VIEW3D_OT_background_image_remove";

	/* api callbacks */
	ot->exec   = background_image_remove_exec;
	ot->poll   = ED_operator_view3d_active;

	/* flags */
	ot->flag   = 0;
	
	/* properties */
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Background image index to remove", 0, INT_MAX);
}

/* ********************* set clipping operator ****************** */

static void calc_clipping_plane(float clip[6][4], const BoundBox *clipbb)
{
	int val;

	for (val = 0; val < 4; val++) {
		normal_tri_v3(clip[val], clipbb->vec[val], clipbb->vec[val == 3 ? 0 : val + 1], clipbb->vec[val + 4]);
		clip[val][3] = -dot_v3v3(clip[val], clipbb->vec[val]);
	}
}

static void calc_local_clipping(float clip_local[6][4], BoundBox *clipbb, float mat[4][4])
{
	BoundBox clipbb_local;
	float imat[4][4];
	int i;

	invert_m4_m4(imat, mat);

	for (i = 0; i < 8; i++) {
		mul_v3_m4v3(clipbb_local.vec[i], imat, clipbb->vec[i]);
	}

	calc_clipping_plane(clip_local, &clipbb_local);
}

void ED_view3d_clipping_local(RegionView3D *rv3d, float mat[4][4])
{
	if (rv3d->rflag & RV3D_CLIPPING)
		calc_local_clipping(rv3d->clip_local, rv3d->clipbb, mat);
}

static int view3d_clipping_exec(bContext *C, wmOperator *op)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	ViewContext vc;
	bglMats mats;
	rcti rect;

	WM_operator_properties_border_to_rcti(op, &rect);

	rv3d->rflag |= RV3D_CLIPPING;
	rv3d->clipbb = MEM_callocN(sizeof(BoundBox), "clipbb");

	/* note; otherwise opengl won't work */
	view3d_operator_needs_opengl(C);

	view3d_set_viewcontext(C, &vc);
	view3d_get_transformation(vc.ar, vc.rv3d, NULL, &mats); /* NULL because we don't want it in object space */
	ED_view3d_clipping_calc(rv3d->clipbb, rv3d->clip, &mats, &rect);

	return OPERATOR_FINISHED;
}

static int view3d_clipping_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	ARegion *ar = CTX_wm_region(C);

	if (rv3d->rflag & RV3D_CLIPPING) {
		rv3d->rflag &= ~RV3D_CLIPPING;
		ED_region_tag_redraw(ar);
		if (rv3d->clipbb) MEM_freeN(rv3d->clipbb);
		rv3d->clipbb = NULL;
		return OPERATOR_FINISHED;
	}
	else {
		return WM_border_select_invoke(C, op, event);
	}
}

/* toggles */
void VIEW3D_OT_clip_border(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Clipping Border";
	ot->description = "Set the view clipping border";
	ot->idname = "VIEW3D_OT_clip_border";

	/* api callbacks */
	ot->invoke = view3d_clipping_invoke;
	ot->exec = view3d_clipping_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;

	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = 0;

	/* rna */
	WM_operator_properties_border(ot);
}

/* ***************** 3d cursor cursor op ******************* */

/* cursor position in vec, result in vec, mval in region coords */
/* note: cannot use event->mval here (called by object_add() */
void ED_view3d_cursor3d_position(bContext *C, float fp[3], const int mval[2])
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	bool flip;
	bool depth_used = false;
	
	/* normally the caller should ensure this,
	 * but this is called from areas that aren't already dealing with the viewport */
	if (rv3d == NULL)
		return;

	ED_view3d_calc_zfac(rv3d, fp, &flip);
	
	/* reset the depth based on the view offset (we _know_ the offset is infront of us) */
	if (flip) {
		negate_v3_v3(fp, rv3d->ofs);
		/* re initialize, no need to check flip again */
		ED_view3d_calc_zfac(rv3d, fp, NULL /* &flip */ );
	}

	if (U.uiflag & USER_ZBUF_CURSOR) {  /* maybe this should be accessed some other way */
		view3d_operator_needs_opengl(C);
		if (ED_view3d_autodist(scene, ar, v3d, mval, fp, true, NULL))
			depth_used = true;
	}

	if (depth_used == false) {
		float depth_pt[3];
		copy_v3_v3(depth_pt, fp);
		ED_view3d_win_to_3d_int(ar, depth_pt, mval, fp);
	}
}

void ED_view3d_cursor3d_update(bContext *C, const int mval[2])
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	float *fp = ED_view3d_cursor3d_get(scene, v3d);

	ED_view3d_cursor3d_position(C, fp, mval);

	if (v3d && v3d->localvd)
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	else
		WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
}

static int view3d_cursor3d_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ED_view3d_cursor3d_update(C, event->mval);

	return OPERATOR_FINISHED;	
}

void VIEW3D_OT_cursor3d(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Set 3D Cursor";
	ot->description = "Set the location of the 3D cursor";
	ot->idname = "VIEW3D_OT_cursor3d";

	/* api callbacks */
	ot->invoke = view3d_cursor3d_invoke;

	ot->poll = ED_operator_view3d_active;

	/* flags */
//	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* rna later */

}

/* ***************** manipulator op ******************* */


static int manipulator_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);

	if (!(v3d->twflag & V3D_USE_MANIPULATOR)) return OPERATOR_PASS_THROUGH;
	if (!(v3d->twflag & V3D_DRAW_MANIPULATOR)) return OPERATOR_PASS_THROUGH;

	/* only no modifier or shift */
	if (event->keymodifier != 0 && event->keymodifier != KM_SHIFT) return OPERATOR_PASS_THROUGH;

	/* note; otherwise opengl won't work */
	view3d_operator_needs_opengl(C);

	if (0 == BIF_do_manipulator(C, event, op))
		return OPERATOR_PASS_THROUGH;

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_manipulator(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "3D Manipulator";
	ot->description = "Manipulate selected item by axis";
	ot->idname = "VIEW3D_OT_manipulator";

	/* api callbacks */
	ot->invoke = manipulator_invoke;

	ot->poll = ED_operator_view3d_active;

	/* properties to pass to transform */
	Transform_Properties(ot, P_CONSTRAINT);
}

static int enable_manipulator_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	View3D *v3d = CTX_wm_view3d(C);

	v3d->twtype = 0;
	
	if (RNA_boolean_get(op->ptr, "translate"))
		v3d->twtype |= V3D_MANIP_TRANSLATE;
	if (RNA_boolean_get(op->ptr, "rotate"))
		v3d->twtype |= V3D_MANIP_ROTATE;
	if (RNA_boolean_get(op->ptr, "scale"))
		v3d->twtype |= V3D_MANIP_SCALE;
		
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_enable_manipulator(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Enable 3D Manipulator";
	ot->description = "Enable the transform manipulator for use";
	ot->idname = "VIEW3D_OT_enable_manipulator";
	
	/* api callbacks */
	ot->invoke = enable_manipulator_invoke;
	ot->poll = ED_operator_view3d_active;
	
	/* rna later */
	prop = RNA_def_boolean(ot->srna, "translate", 0, "Translate", "Enable the translate manipulator");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "rotate", 0, "Rotate", "Enable the rotate manipulator");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "scale", 0, "Scale", "Enable the scale manipulator");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************************* below the line! *********************** */


static float view_autodist_depth_margin(ARegion *ar, const int mval[2], int margin)
{
	ViewDepths depth_temp = {0};
	rcti rect;
	float depth_close;

	if (margin == 0) {
		/* Get Z Depths, needed for perspective, nice for ortho */
		rect.xmin = mval[0];
		rect.ymin = mval[1];
		rect.xmax = mval[0] + 1;
		rect.ymax = mval[1] + 1;
	}
	else {
		rect.xmax = mval[0] + margin;
		rect.ymax = mval[1] + margin;

		rect.xmin = mval[0] - margin;
		rect.ymin = mval[1] - margin;
	}

	view3d_update_depths_rect(ar, &depth_temp, &rect);
	depth_close = view3d_depth_near(&depth_temp);
	if (depth_temp.depths) MEM_freeN(depth_temp.depths);
	return depth_close;
}

/* XXX todo Zooms in on a border drawn by the user */
bool ED_view3d_autodist(Scene *scene, ARegion *ar, View3D *v3d,
                        const int mval[2], float mouse_worldloc[3],
                        const bool alphaoverride, const float fallback_depth_pt[3])
{
	bglMats mats; /* ZBuffer depth vars */
	float depth_close;
	double cent[2],  p[3];

	/* Get Z Depths, needed for perspective, nice for ortho */
	bgl_get_mats(&mats);
	ED_view3d_draw_depth(scene, ar, v3d, alphaoverride);

	depth_close = view_autodist_depth_margin(ar, mval, 4);

	if (depth_close != FLT_MAX) {
		cent[0] = (double)mval[0];
		cent[1] = (double)mval[1];

		if (gluUnProject(cent[0], cent[1], depth_close,
		                 mats.modelview, mats.projection, (GLint *)mats.viewport, &p[0], &p[1], &p[2]))
		{
			mouse_worldloc[0] = (float)p[0];
			mouse_worldloc[1] = (float)p[1];
			mouse_worldloc[2] = (float)p[2];
			return true;
		}
	}

	if (fallback_depth_pt) {
		ED_view3d_win_to_3d_int(ar, fallback_depth_pt, mval, mouse_worldloc);
		return true;
	}
	else {
		return false;
	}
}

void ED_view3d_autodist_init(Scene *scene, ARegion *ar, View3D *v3d, int mode)
{
	/* Get Z Depths, needed for perspective, nice for ortho */
	switch (mode) {
		case 0:
			ED_view3d_draw_depth(scene, ar, v3d, true);
			break;
		case 1:
			ED_view3d_draw_depth_gpencil(scene, ar, v3d);
			break;
	}
}

/* no 4x4 sampling, run #ED_view3d_autodist_init first */
bool ED_view3d_autodist_simple(ARegion *ar, const int mval[2], float mouse_worldloc[3],
                               int margin, float *force_depth)
{
	bglMats mats; /* ZBuffer depth vars, could cache? */
	float depth;
	double cent[2],  p[3];

	/* Get Z Depths, needed for perspective, nice for ortho */
	if (force_depth)
		depth = *force_depth;
	else
		depth = view_autodist_depth_margin(ar, mval, margin);

	if (depth == FLT_MAX)
		return false;

	cent[0] = (double)mval[0];
	cent[1] = (double)mval[1];

	bgl_get_mats(&mats);

	if (!gluUnProject(cent[0], cent[1], depth,
	                  mats.modelview, mats.projection, (GLint *)mats.viewport, &p[0], &p[1], &p[2]))
	{
		return false;
	}

	mouse_worldloc[0] = (float)p[0];
	mouse_worldloc[1] = (float)p[1];
	mouse_worldloc[2] = (float)p[2];
	return true;
}

bool ED_view3d_autodist_depth(ARegion *ar, const int mval[2], int margin, float *depth)
{
	*depth = view_autodist_depth_margin(ar, mval, margin);

	return (*depth != FLT_MAX);
}

static bool depth_segment_cb(int x, int y, void *userData)
{
	struct { ARegion *ar; int margin; float depth; } *data = userData;
	int mval[2];
	float depth;

	mval[0] = x;
	mval[1] = y;

	depth = view_autodist_depth_margin(data->ar, mval, data->margin);

	if (depth != FLT_MAX) {
		data->depth = depth;
		return 0;
	}
	else {
		return 1;
	}
}

bool ED_view3d_autodist_depth_seg(ARegion *ar, const int mval_sta[2], const int mval_end[2],
                                  int margin, float *depth)
{
	struct { ARegion *ar; int margin; float depth; } data = {NULL};
	int p1[2];
	int p2[2];

	data.ar = ar;
	data.margin = margin;
	data.depth = FLT_MAX;

	copy_v2_v2_int(p1, mval_sta);
	copy_v2_v2_int(p2, mval_end);

	plot_line_v2v2i(p1, p2, depth_segment_cb, &data);

	*depth = data.depth;

	return (*depth != FLT_MAX);
}

/* problem - ofs[3] can be on same location as camera itself.
 * Blender needs proper dist value for zoom.
 * use fallback_dist to override small values
 */
float ED_view3d_offset_distance(float mat[4][4], const float ofs[3], const float fallback_dist)
{
	float pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	float dir[4] = {0.0f, 0.0f, 1.0f, 0.0f};
	float dist;
	
	mul_m4_v4(mat, pos);
	add_v3_v3(pos, ofs);
	mul_m4_v4(mat, dir);
	normalize_v3(dir);

	dist = dot_v3v3(pos, dir);

	if ((dist < FLT_EPSILON) && (fallback_dist != 0.0f)) {
		dist = fallback_dist;
	}

	return dist;
}

/**
 * Set the dist without moving the view (compensate with #RegionView3D.ofs)
 *
 * \note take care that viewinv is up to date, #ED_view3d_update_viewmat first.
 */
void ED_view3d_distance_set(RegionView3D *rv3d, const float dist)
{
	float viewinv[4];
	float tvec[3];

	BLI_assert(dist >= 0.0f);

	copy_v3_fl3(tvec, 0.0f, 0.0f, rv3d->dist - dist);
	/* rv3d->viewinv isn't always valid */
#if 0
	mul_mat3_m4_v3(rv3d->viewinv, tvec);
#else
	invert_qt_qt(viewinv, rv3d->viewquat);
	mul_qt_v3(viewinv, tvec);
#endif
	sub_v3_v3(rv3d->ofs, tvec);

	rv3d->dist = dist;
}

/**
 * Set the view transformation from a 4x4 matrix.
 *
 * \param mat The view 4x4 transformation matrix to assign.
 * \param ofs The view offset, normally from RegionView3D.ofs.
 * \param quat The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_from_m4(float mat[4][4], float ofs[3], float quat[4], float *dist)
{
	float nmat[3][3];

	/* dist depends on offset */
	BLI_assert(dist == NULL || ofs != NULL);

	copy_m3_m4(nmat, mat);
	normalize_m3(nmat);

	/* Offset */
	if (ofs)
		negate_v3_v3(ofs, mat[3]);

	/* Quat */
	if (quat) {
		float imat[3][3];
		invert_m3_m3(imat, nmat);
		mat3_to_quat(quat, imat);
	}

	if (ofs && dist) {
		float vec[3] = {0.0f, 0.0f, -(*dist)};

		mul_m3_v3(nmat, vec);
		sub_v3_v3(ofs, vec);
	}
}

/**
 * Calculate the view transformation matrix from RegionView3D input.
 * The resulting matrix is equivalent to RegionView3D.viewinv
 * \param mat The view 4x4 transformation matrix to calculate.
 * \param ofs The view offset, normally from RegionView3D.ofs.
 * \param quat The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_to_m4(float mat[4][4], const float ofs[3], const float quat[4], const float dist)
{
	float iviewquat[4] = {-quat[0], quat[1], quat[2], quat[3]};
	float dvec[3] = {0.0f, 0.0f, dist};

	quat_to_mat4(mat, iviewquat);
	mul_mat3_m4_v3(mat, dvec);
	sub_v3_v3v3(mat[3], dvec, ofs);
}

/**
 * Set the RegionView3D members from an objects transformation and optionally lens.
 * \param ob The object to set the view to.
 * \param ofs The view offset to be set, normally from RegionView3D.ofs.
 * \param quat The view rotation to be set, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs to be set, normally from RegionView3D.dist.
 * \param lens The view lens angle set for cameras and lamps, normally from View3D.lens.
 */
void ED_view3d_from_object(Object *ob, float ofs[3], float quat[4], float *dist, float *lens)
{
	ED_view3d_from_m4(ob->obmat, ofs, quat, dist);

	if (lens) {
		CameraParams params;

		BKE_camera_params_init(&params);
		BKE_camera_params_from_object(&params, ob);
		*lens = params.lens;
	}
}

/**
 * Set the object transformation from RegionView3D members.
 * \param ob The object which has the transformation assigned.
 * \param ofs The view offset, normally from RegionView3D.ofs.
 * \param quat The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_to_object(Object *ob, const float ofs[3], const float quat[4], const float dist)
{
	float mat[4][4];
	ED_view3d_to_m4(mat, ofs, quat, dist);
	BKE_object_apply_mat4(ob, mat, true, true);
}

/**
 * Use to store the last view, before entering camera view.
 */
void ED_view3d_lastview_store(RegionView3D *rv3d)
{
	copy_qt_qt(rv3d->lviewquat, rv3d->viewquat);
	rv3d->lview = rv3d->view;
	if (rv3d->persp != RV3D_CAMOB) {
		rv3d->lpersp = rv3d->persp;
	}
}

BGpic *ED_view3D_background_image_new(View3D *v3d)
{
	BGpic *bgpic = MEM_callocN(sizeof(BGpic), "Background Image");

	bgpic->size = 5.0;
	bgpic->blend = 0.5;
	bgpic->iuser.fie_ima = 2;
	bgpic->iuser.ok = 1;
	bgpic->view = 0; /* 0 for all */
	bgpic->flag |= V3D_BGPIC_EXPANDED;

	BLI_addtail(&v3d->bgpicbase, bgpic);

	return bgpic;
}

void ED_view3D_background_image_remove(View3D *v3d, BGpic *bgpic)
{
	BLI_remlink(&v3d->bgpicbase, bgpic);

	MEM_freeN(bgpic);
}

void ED_view3D_background_image_clear(View3D *v3d)
{
	BGpic *bgpic = v3d->bgpicbase.first;

	while (bgpic) {
		BGpic *next_bgpic = bgpic->next;

		ED_view3D_background_image_remove(v3d, bgpic);

		bgpic = next_bgpic;
	}
}

void ED_view3D_lock_clear(View3D *v3d)
{
	v3d->ob_centre = NULL;
	v3d->ob_centre_bone[0] = '\0';
	v3d->ob_centre_cursor = false;
	v3d->flag2 &= ~V3D_LOCK_CAMERA;
}

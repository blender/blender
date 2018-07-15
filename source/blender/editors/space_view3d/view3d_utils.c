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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_utils.c
 *  \ingroup spview3d
 *
 * 3D View checks and manipulation (no operators).
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_matrix.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name View Data Access Utilities
 *
 * \{ */

View3DCursor *ED_view3d_cursor3d_get(Scene *scene, View3D *v3d)
{
	if (v3d && v3d->localvd) {
		return &v3d->cursor;
	}
	else {
		return &scene->cursor;
	}
}

void ED_view3d_cursor3d_calc_mat3(const Scene *scene, const View3D *v3d, float mat[3][3])
{
	const View3DCursor *cursor = ED_view3d_cursor3d_get((Scene *)scene, (View3D *)v3d);
	quat_to_mat3(mat, cursor->rotation);
}

void ED_view3d_cursor3d_calc_mat4(const Scene *scene, const View3D *v3d, float mat[4][4])
{
	const View3DCursor *cursor = ED_view3d_cursor3d_get((Scene *)scene, (View3D *)v3d);
	quat_to_mat4(mat, cursor->rotation);
	copy_v3_v3(mat[3], cursor->location);
}

Camera *ED_view3d_camera_data_get(View3D *v3d, RegionView3D *rv3d)
{
	/* establish the camera object, so we can default to view mapping if anything is wrong with it */
	if ((rv3d->persp == RV3D_CAMOB) && v3d->camera && (v3d->camera->type == OB_CAMERA)) {
		return v3d->camera->data;
	}
	else {
		return NULL;
	}
}

void ED_view3d_dist_range_get(
        const View3D *v3d,
        float r_dist_range[2])
{
	r_dist_range[0] = v3d->grid * 0.001f;
	r_dist_range[1] = v3d->far * 10.0f;
}

/**
 * \note copies logic of #ED_view3d_viewplane_get(), keep in sync.
 */
bool ED_view3d_clip_range_get(
        Depsgraph *depsgraph,
        const View3D *v3d, const RegionView3D *rv3d,
        float *r_clipsta, float *r_clipend,
        const bool use_ortho_factor)
{
	CameraParams params;

	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);

	if (use_ortho_factor && params.is_ortho) {
		const float fac = 2.0f / (params.clipend - params.clipsta);
		params.clipsta *= fac;
		params.clipend *= fac;
	}

	if (r_clipsta) *r_clipsta = params.clipsta;
	if (r_clipend) *r_clipend = params.clipend;

	return params.is_ortho;
}

bool ED_view3d_viewplane_get(
        Depsgraph *depsgraph,
        const View3D *v3d, const RegionView3D *rv3d, int winx, int winy,
        rctf *r_viewplane, float *r_clipsta, float *r_clipend, float *r_pixsize)
{
	CameraParams params;

	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);
	BKE_camera_params_compute_viewplane(&params, winx, winy, 1.0f, 1.0f);

	if (r_viewplane) *r_viewplane = params.viewplane;
	if (r_clipsta) *r_clipsta = params.clipsta;
	if (r_clipend) *r_clipend = params.clipend;
	if (r_pixsize) *r_pixsize = params.viewdx;

	return params.is_ortho;
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name View State/Context Utilities
 *
 * \{ */

/**
 * Use this call when executing an operator,
 * event system doesn't set for each event the OpenGL drawing context.
 */
void view3d_operator_needs_opengl(const bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);

	view3d_region_operator_needs_opengl(win, ar);
}

void view3d_region_operator_needs_opengl(wmWindow *UNUSED(win), ARegion *ar)
{
	/* for debugging purpose, context should always be OK */
	if ((ar == NULL) || (ar->regiontype != RGN_TYPE_WINDOW)) {
		printf("view3d_region_operator_needs_opengl error, wrong region\n");
	}
	else {
		RegionView3D *rv3d = ar->regiondata;

		wmViewport(&ar->winrct); // TODO: bad
		GPU_matrix_projection_set(rv3d->winmat);
		GPU_matrix_set(rv3d->viewmat);
	}
}

/**
 * Use instead of: ``bglPolygonOffset(rv3d->dist, ...)`` see bug [#37727]
 */
void ED_view3d_polygon_offset(const RegionView3D *rv3d, const float dist)
{
	float viewdist;

	if (rv3d->rflag & RV3D_ZOFFSET_DISABLED) {
		return;
	}

	viewdist = rv3d->dist;

	/* special exception for ortho camera (viewdist isnt used for perspective cameras) */
	if (dist != 0.0f) {
		if (rv3d->persp == RV3D_CAMOB) {
			if (rv3d->is_persp == false) {
				viewdist = 1.0f / max_ff(fabsf(rv3d->winmat[0][0]), fabsf(rv3d->winmat[1][1]));
			}
		}
	}

	bglPolygonOffset(viewdist, dist);
}

bool ED_view3d_context_activate(bContext *C)
{
	bScreen *sc = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar;

	/* sa can be NULL when called from python */
	if (sa == NULL || sa->spacetype != SPACE_VIEW3D) {
		sa = BKE_screen_find_big_area(sc, SPACE_VIEW3D, 0);
	}

	if (sa == NULL) {
		return false;
	}

	ar = BKE_area_find_region_active_win(sa);
	if (ar == NULL) {
		return false;
	}

	/* bad context switch .. */
	CTX_wm_area_set(C, sa);
	CTX_wm_region_set(C, ar);

	return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Clipping Utilities
 *
 * \{ */

void ED_view3d_clipping_calc_from_boundbox(float clip[4][4], const BoundBox *bb, const bool is_flip)
{
	int val;

	for (val = 0; val < 4; val++) {
		normal_tri_v3(clip[val], bb->vec[val], bb->vec[val == 3 ? 0 : val + 1], bb->vec[val + 4]);
		if (UNLIKELY(is_flip)) {
			negate_v3(clip[val]);
		}

		clip[val][3] = -dot_v3v3(clip[val], bb->vec[val]);
	}
}

void ED_view3d_clipping_calc(BoundBox *bb, float planes[4][4], const ARegion *ar, const Object *ob, const rcti *rect)
{
	/* init in case unproject fails */
	memset(bb->vec, 0, sizeof(bb->vec));

	/* four clipping planes and bounding volume */
	/* first do the bounding volume */
	for (int val = 0; val < 4; val++) {
		float xs = (val == 0 || val == 3) ? rect->xmin : rect->xmax;
		float ys = (val == 0 || val == 1) ? rect->ymin : rect->ymax;

		ED_view3d_unproject(ar, xs, ys, 0.0, bb->vec[val]);
		ED_view3d_unproject(ar, xs, ys, 1.0, bb->vec[4 + val]);
	}

	/* optionally transform to object space */
	if (ob) {
		float imat[4][4];
		invert_m4_m4(imat, ob->obmat);

		for (int val = 0; val < 8; val++) {
			mul_m4_v3(imat, bb->vec[val]);
		}
	}

	/* verify if we have negative scale. doing the transform before cross
	 * product flips the sign of the vector compared to doing cross product
	 * before transform then, so we correct for that. */
	int flip_sign = (ob) ? is_negative_m4(ob->obmat) : false;

	ED_view3d_clipping_calc_from_boundbox(planes, bb, flip_sign);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Bound-Box Utilities
 *
 * \{ */

static bool view3d_boundbox_clip_m4(const BoundBox *bb, float persmatob[4][4])
{
	int a, flag = -1, fl;

	for (a = 0; a < 8; a++) {
		float vec[4], min, max;
		copy_v3_v3(vec, bb->vec[a]);
		vec[3] = 1.0;
		mul_m4_v4(persmatob, vec);
		max = vec[3];
		min = -vec[3];

		fl = 0;
		if (vec[0] < min) fl += 1;
		if (vec[0] > max) fl += 2;
		if (vec[1] < min) fl += 4;
		if (vec[1] > max) fl += 8;
		if (vec[2] < min) fl += 16;
		if (vec[2] > max) fl += 32;

		flag &= fl;
		if (flag == 0) return true;
	}

	return false;
}

bool ED_view3d_boundbox_clip_ex(const RegionView3D *rv3d, const BoundBox *bb, float obmat[4][4])
{
	/* return 1: draw */

	float persmatob[4][4];

	if (bb == NULL) return true;
	if (bb->flag & BOUNDBOX_DISABLED) return true;

	mul_m4_m4m4(persmatob, (float(*)[4])rv3d->persmat, obmat);

	return view3d_boundbox_clip_m4(bb, persmatob);
}

bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const BoundBox *bb)
{
	if (bb == NULL) return true;
	if (bb->flag & BOUNDBOX_DISABLED) return true;

	return view3d_boundbox_clip_m4(bb, rv3d->persmatob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Perspective & Mode Switching
 *
 * Misc view utility functions.
 * \{ */

bool ED_view3d_offset_lock_check(const  View3D *v3d, const  RegionView3D *rv3d)
{
	return (rv3d->persp != RV3D_CAMOB) && (v3d->ob_centre_cursor || v3d->ob_centre);
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

void ED_view3d_lock_clear(View3D *v3d)
{
	v3d->ob_centre = NULL;
	v3d->ob_centre_bone[0] = '\0';
	v3d->ob_centre_cursor = false;
	v3d->flag2 &= ~V3D_LOCK_CAMERA;
}

/**
 * For viewport operators that exit camera perspective.
 *
 * \note This differs from simply setting ``rv3d->persp = persp`` because it
 * sets the ``ofs`` and ``dist`` values of the viewport so it matches the camera,
 * otherwise switching out of camera view may jump to a different part of the scene.
 */
void ED_view3d_persp_switch_from_camera(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d, const char persp)
{
	BLI_assert(rv3d->persp == RV3D_CAMOB);
	BLI_assert(persp != RV3D_CAMOB);

	if (v3d->camera) {
		Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
		rv3d->dist = ED_view3d_offset_distance(ob_camera_eval->obmat, rv3d->ofs, VIEW3D_DIST_FALLBACK);
		ED_view3d_from_object(ob_camera_eval, rv3d->ofs, rv3d->viewquat, &rv3d->dist, NULL);
	}

	if (!ED_view3d_camera_lock_check(v3d, rv3d)) {
		rv3d->persp = persp;
	}
}
/**
 * Action to take when rotating the view,
 * handle auto-persp and logic for switching out of views.
 *
 * shared with NDOF.
 */
bool ED_view3d_persp_ensure(const Depsgraph *depsgraph, View3D *v3d, ARegion *ar)
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
			ED_view3d_persp_switch_from_camera(depsgraph, v3d, rv3d, persp);
		}
		else if (autopersp && RV3D_VIEW_IS_AXIS(rv3d->view)) {
			rv3d->persp = RV3D_PERSP;
		}
		return true;
	}

	return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Lock API
 *
 * Lock the camera to the view-port, allowing view manipulation to transform the camera.
 * \{ */

/**
 * \return true when the view-port is locked to its camera.
 */
bool ED_view3d_camera_lock_check(const View3D *v3d, const RegionView3D *rv3d)
{
	return ((v3d->camera) &&
	        (!ID_IS_LINKED(v3d->camera)) &&
	        (v3d->flag2 & V3D_LOCK_CAMERA) &&
	        (rv3d->persp == RV3D_CAMOB));
}

/**
 * Apply the camera object transformation to the view-port.
 * (needed so we can use regular view-port manipulation operators, that sync back to the camera).
 */
void ED_view3d_camera_lock_init_ex(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d, const bool calc_dist)
{
	if (ED_view3d_camera_lock_check(v3d, rv3d)) {
		Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
		if (calc_dist) {
			/* using a fallback dist is OK here since ED_view3d_from_object() compensates for it */
			rv3d->dist = ED_view3d_offset_distance(ob_camera_eval->obmat, rv3d->ofs, VIEW3D_DIST_FALLBACK);
		}
		ED_view3d_from_object(ob_camera_eval, rv3d->ofs, rv3d->viewquat, &rv3d->dist, NULL);
	}
}

void ED_view3d_camera_lock_init(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d)
{
	ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, true);
}

/**
 * Apply the view-port transformation back to the camera object.
 *
 * \return true if the camera is moved.
 */
bool ED_view3d_camera_lock_sync(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d)
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
			Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
			Object *root_parent_eval = DEG_get_evaluated_object(depsgraph, root_parent);

			ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);

			normalize_m4_m4(tmat, ob_camera_eval->obmat);

			invert_m4_m4(imat, tmat);
			mul_m4_m4m4(diff_mat, view_mat, imat);

			mul_m4_m4m4(parent_mat, diff_mat, root_parent_eval->obmat);

			BKE_object_tfm_protected_backup(root_parent, &obtfm);
			BKE_object_apply_mat4(root_parent, parent_mat, true, false);
			BKE_object_tfm_protected_restore(root_parent, &obtfm, root_parent->protectflag);

			ob_update = v3d->camera;
			while (ob_update) {
				DEG_id_tag_update(&ob_update->id, OB_RECALC_OB);
				WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, ob_update);
				ob_update = ob_update->parent;
			}
		}
		else {
			/* always maintain the same scale */
			const short protect_scale_all = (OB_LOCK_SCALEX | OB_LOCK_SCALEY | OB_LOCK_SCALEZ);
			BKE_object_tfm_protected_backup(v3d->camera, &obtfm);
			ED_view3d_to_object(depsgraph, v3d->camera, rv3d->ofs, rv3d->viewquat, rv3d->dist);
			BKE_object_tfm_protected_restore(v3d->camera, &obtfm, v3d->camera->protectflag | protect_scale_all);

			DEG_id_tag_update(&v3d->camera->id, OB_RECALC_OB);
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
		const float cfra = (float)CFRA;
		ListBase dsources = {NULL, NULL};

		/* add data-source override for the camera object */
		ANIM_relative_keyingset_add_source(&dsources, id_key, NULL, NULL);

		/* insert keyframes
		 * 1) on the first frame
		 * 2) on each subsequent frame
		 *    TODO: need to check in future that frame changed before doing this
		 */
		if (do_rotate) {
			struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_ROTATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
		}
		if (do_translate) {
			struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
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

/** \} */



/* -------------------------------------------------------------------- */
/** \name Box View Support
 *
 * Use with quad-split so each view is clipped by the bounds of each view axis.
 * \{ */

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
	invert_qt_normalized(viewinv);
	mul_qt_v3(viewinv, view_src_x);
	mul_qt_v3(viewinv, view_src_y);

	if (UNLIKELY(ED_view3d_quat_from_axis_view(rv3d_dst->view, viewinv) == false)) {
		return;
	}
	invert_qt_normalized(viewinv);
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
void view3d_boxview_sync(ScrArea *sa, ARegion *ar)
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Auto-Depth Utilities
 * \{ */

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
		BLI_rcti_init_pt_radius(&rect, mval, margin);
	}

	view3d_update_depths_rect(ar, &depth_temp, &rect);
	depth_close = view3d_depth_near(&depth_temp);
	MEM_SAFE_FREE(depth_temp.depths);
	return depth_close;
}

/**
 * Get the world-space 3d location from a screen-space 2d point.
 *
 * \param mval: Input screen-space pixel location.
 * \param mouse_worldloc: Output world-space location.
 * \param fallback_depth_pt: Use this points depth when no depth can be found.
 */
bool ED_view3d_autodist(
        Depsgraph *depsgraph, ARegion *ar, View3D *v3d,
        const int mval[2], float mouse_worldloc[3],
        const bool alphaoverride, const float fallback_depth_pt[3])
{
	float depth_close;
	int margin_arr[] = {0, 2, 4};
	int i;
	bool depth_ok = false;

	/* Get Z Depths, needed for perspective, nice for ortho */
	ED_view3d_draw_depth(depsgraph, ar, v3d, alphaoverride);

	/* Attempt with low margin's first */
	i = 0;
	do {
		depth_close = view_autodist_depth_margin(ar, mval, margin_arr[i++] * U.pixelsize);
		depth_ok = (depth_close != FLT_MAX);
	} while ((depth_ok == false) && (i < ARRAY_SIZE(margin_arr)));

	if (depth_ok) {
		float centx = (float)mval[0] + 0.5f;
		float centy = (float)mval[1] + 0.5f;

		if (ED_view3d_unproject(ar, centx, centy, depth_close, mouse_worldloc)) {
			return true;
		}
	}

	if (fallback_depth_pt) {
		ED_view3d_win_to_3d_int(v3d, ar, fallback_depth_pt, mval, mouse_worldloc);
		return true;
	}
	else {
		return false;
	}
}

void ED_view3d_autodist_init(Depsgraph *depsgraph,
        ARegion *ar, View3D *v3d, int mode)
{
	/* Get Z Depths, needed for perspective, nice for ortho */
	switch (mode) {
		case 0:
			ED_view3d_draw_depth(depsgraph, ar, v3d, true);
			break;
		case 1:
		{
			Scene *scene = DEG_get_evaluated_scene(depsgraph);
			ED_view3d_draw_depth_gpencil(depsgraph, scene, ar, v3d);
			break;
		}
	}
}

/* no 4x4 sampling, run #ED_view3d_autodist_init first */
bool ED_view3d_autodist_simple(ARegion *ar, const int mval[2], float mouse_worldloc[3],
                               int margin, float *force_depth)
{
	float depth;

	/* Get Z Depths, needed for perspective, nice for ortho */
	if (force_depth)
		depth = *force_depth;
	else
		depth = view_autodist_depth_margin(ar, mval, margin);

	if (depth == FLT_MAX)
		return false;

	float centx = (float)mval[0] + 0.5f;
	float centy = (float)mval[1] + 0.5f;
	return ED_view3d_unproject(ar, centx, centy, depth, mouse_worldloc);
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

bool ED_view3d_autodist_depth_seg(
        ARegion *ar, const int mval_sta[2], const int mval_end[2],
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

	BLI_bitmap_draw_2d_line_v2v2i(p1, p2, depth_segment_cb, &data);

	*depth = data.depth;

	return (*depth != FLT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Radius/Distance Utilities
 *
 * Use to calculate a distance to a point based on it's radius.
 * \{ */

float ED_view3d_radius_to_dist_persp(const float angle, const float radius)
{
	return radius * (1.0f / tanf(angle / 2.0f));
}

float ED_view3d_radius_to_dist_ortho(const float lens, const float radius)
{
	return radius / (DEFAULT_SENSOR_WIDTH / lens);
}

/**
 * Return a new RegionView3D.dist value to fit the \a radius.
 *
 * \note Depth isn't taken into account, this will fit a flat plane exactly,
 * but points towards the view (with a perspective projection),
 * may be within the radius but outside the view. eg:
 *
 * <pre>
 *           +
 * pt --> + /^ radius
 *         / |
 *        /  |
 * view  +   +
 *        \  |
 *         \ |
 *          \|
 *           +
 * </pre>
 *
 * \param ar  Can be NULL if \a use_aspect is false.
 * \param persp  Allow the caller to tell what kind of perspective to use (ortho/view/camera)
 * \param use_aspect  Increase the distance to account for non 1:1 view aspect.
 * \param radius  The radius will be fitted exactly, typically pre-scaled by a margin (#VIEW3D_MARGIN).
 */
float ED_view3d_radius_to_dist(
        const View3D *v3d, const ARegion *ar,
        const struct Depsgraph *depsgraph,
        const char persp, const bool use_aspect,
        const float radius)
{
	float dist;

	BLI_assert(ELEM(persp, RV3D_ORTHO, RV3D_PERSP, RV3D_CAMOB));
	BLI_assert((persp != RV3D_CAMOB) || v3d->camera);

	if (persp == RV3D_ORTHO) {
		dist = ED_view3d_radius_to_dist_ortho(v3d->lens, radius);
	}
	else {
		float lens, sensor_size, zoom;
		float angle;

		if (persp == RV3D_CAMOB) {
			CameraParams params;
			BKE_camera_params_init(&params);
			params.clipsta = v3d->near;
			params.clipend = v3d->far;
			Object *camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
			BKE_camera_params_from_object(&params, camera_eval);

			lens = params.lens;
			sensor_size = BKE_camera_sensor_size(params.sensor_fit, params.sensor_x, params.sensor_y);

			/* ignore 'rv3d->camzoom' because we want to fit to the cameras frame */
			zoom = CAMERA_PARAM_ZOOM_INIT_CAMOB;
		}
		else {
			lens = v3d->lens;
			sensor_size = DEFAULT_SENSOR_WIDTH;
			zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
		}

		angle = focallength_to_fov(lens, sensor_size);

		/* zoom influences lens, correct this by scaling the angle as a distance (by the zoom-level) */
		angle = atanf(tanf(angle / 2.0f) * zoom) * 2.0f;

		dist = ED_view3d_radius_to_dist_persp(angle, radius);
	}

	if (use_aspect) {
		const RegionView3D *rv3d = ar->regiondata;

		float winx, winy;

		if (persp == RV3D_CAMOB) {
			/* camera frame x/y in pixels */
			winx = ar->winx / rv3d->viewcamtexcofac[0];
			winy = ar->winy / rv3d->viewcamtexcofac[1];
		}
		else {
			winx = ar->winx;
			winy = ar->winy;
		}

		if (winx && winy) {
			float aspect = winx / winy;
			if (aspect < 1.0f) {
				aspect = 1.0f / aspect;
			}
			dist *= aspect;
		}
	}

	return dist;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Distance Utilities
 * \{ */

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
	invert_qt_qt_normalized(viewinv, rv3d->viewquat);
	mul_qt_v3(viewinv, tvec);
#endif
	sub_v3_v3(rv3d->ofs, tvec);

	rv3d->dist = dist;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Axis Utilities
 * \{ */
static float view3d_quat_axis[6][4] = {
	{M_SQRT1_2, -M_SQRT1_2, 0.0f, 0.0f},    /* RV3D_VIEW_FRONT */
	{0.0f, 0.0f, -M_SQRT1_2, -M_SQRT1_2},   /* RV3D_VIEW_BACK */
	{0.5f, -0.5f, 0.5f, 0.5f},              /* RV3D_VIEW_LEFT */
	{0.5f, -0.5f, -0.5f, -0.5f},            /* RV3D_VIEW_RIGHT */
	{1.0f, 0.0f, 0.0f, 0.0f},               /* RV3D_VIEW_TOP */
	{0.0f, -1.0f, 0.0f, 0.0f},              /* RV3D_VIEW_BOTTOM */
};


bool ED_view3d_quat_from_axis_view(const char view, float quat[4])
{
	if (RV3D_VIEW_IS_AXIS(view)) {
		copy_qt_qt(quat, view3d_quat_axis[view - RV3D_VIEW_FRONT]);
		return true;
	}
	else {
		return false;
	}
}

char ED_view3d_quat_to_axis_view(const float quat[4], const float epsilon)
{
	/* quat values are all unit length */

	char view;

	for (view = RV3D_VIEW_FRONT; view <= RV3D_VIEW_BOTTOM; view++) {
		if (fabsf(angle_signed_qtqt(quat, view3d_quat_axis[view - RV3D_VIEW_FRONT])) < epsilon) {
			return view;
		}
	}

	return RV3D_VIEW_USER;
}

char ED_view3d_lock_view_from_index(int index)
{
	switch (index) {
		case 0:  return RV3D_VIEW_FRONT;
		case 1:  return RV3D_VIEW_TOP;
		case 2:  return RV3D_VIEW_RIGHT;
		default: return RV3D_VIEW_USER;
	}

}

char ED_view3d_axis_view_opposite(char view)
{
	switch (view) {
		case RV3D_VIEW_FRONT:   return RV3D_VIEW_BACK;
		case RV3D_VIEW_BACK:    return RV3D_VIEW_FRONT;
		case RV3D_VIEW_LEFT:    return RV3D_VIEW_RIGHT;
		case RV3D_VIEW_RIGHT:   return RV3D_VIEW_LEFT;
		case RV3D_VIEW_TOP:     return RV3D_VIEW_BOTTOM;
		case RV3D_VIEW_BOTTOM:  return RV3D_VIEW_TOP;
	}

	return RV3D_VIEW_USER;
}


bool ED_view3d_lock(RegionView3D *rv3d)
{
	return ED_view3d_quat_from_axis_view(rv3d->view, rv3d->viewquat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Transform Utilities
 * \{ */

/**
 * Set the view transformation from a 4x4 matrix.
 *
 * \param mat The view 4x4 transformation matrix to assign.
 * \param ofs The view offset, normally from RegionView3D.ofs.
 * \param quat The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_from_m4(const float mat[4][4], float ofs[3], float quat[4], float *dist)
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
		mat3_normalized_to_quat(quat, nmat);
		invert_qt_normalized(quat);
	}

	if (ofs && dist) {
		madd_v3_v3fl(ofs, nmat[2], *dist);
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
 * \param depsgraph The depsgraph to get the evaluated object for the lens calculation.
 * \param ob The object to set the view to.
 * \param ofs The view offset to be set, normally from RegionView3D.ofs.
 * \param quat The view rotation to be set, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs to be set, normally from RegionView3D.dist.
 * \param lens The view lens angle set for cameras and lamps, normally from View3D.lens.
 */
void ED_view3d_from_object(const Object *ob, float ofs[3], float quat[4], float *dist, float *lens)
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
 * \param depsgraph The depsgraph to get the evaluated object parent for the transformation calculation.
 * \param ob The object which has the transformation assigned.
 * \param ofs The view offset, normally from RegionView3D.ofs.
 * \param quat The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_to_object(const Depsgraph *depsgraph, Object *ob, const float ofs[3], const float quat[4], const float dist)
{
	float mat[4][4];
	ED_view3d_to_m4(mat, ofs, quat, dist);

	Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	BKE_object_apply_mat4_ex(ob, mat, ob_eval->parent, ob_eval->parentinv, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth Buffer Utilities
 * \{ */

float ED_view3d_depth_read_cached(const ViewContext *vc, const int mval[2])
{
	ViewDepths *vd = vc->rv3d->depths;

	int x = mval[0];
	int y = mval[1];

	if (vd && vd->depths && x > 0 && y > 0 && x < vd->w && y < vd->h) {
		return vd->depths[y * vd->w + x];
	}
	else {
		BLI_assert(1.0 <= vd->depth_range[1]);
		return 1.0f;
	}
}

bool ED_view3d_depth_read_cached_normal(
        const ViewContext *vc, const int mval[2],
        float r_normal[3])
{
	/* Note: we could support passing in a radius.
	 * For now just read 9 pixels. */

	/* pixels surrounding */
	bool  depths_valid[9] = {false};
	float coords[9][3] = {{0}};

	ARegion *ar = vc->ar;
	const ViewDepths *depths = vc->rv3d->depths;

	for (int x = 0, i = 0; x < 2; x++) {
		for (int y = 0; y < 2; y++) {
			const int mval_ofs[2] = {mval[0] + (x - 1), mval[1] + (y - 1)};

			const double depth = (double)ED_view3d_depth_read_cached(vc, mval_ofs);
			if ((depth > depths->depth_range[0]) && (depth < depths->depth_range[1])) {
				if (ED_view3d_depth_unproject(ar, mval_ofs, depth, coords[i])) {
					depths_valid[i] = true;
				}
			}
			i++;
		}
	}

	const int edges[2][6][2] = {
	    /* x edges */
	    {{0, 1}, {1, 2},
	     {3, 4}, {4, 5},
	     {6, 7}, {7, 8}},
	    /* y edges */
	    {{0, 3}, {3, 6},
	     {1, 4}, {4, 7},
	     {2, 5}, {5, 8}},
	};

	float cross[2][3] = {{0.0f}};

	for (int i = 0; i < 6; i++) {
		for (int axis = 0; axis < 2; axis++) {
			if (depths_valid[edges[axis][i][0]] && depths_valid[edges[axis][i][1]]) {
				float delta[3];
				sub_v3_v3v3(delta, coords[edges[axis][i][0]], coords[edges[axis][i][1]]);
				add_v3_v3(cross[axis], delta);
			}
		}
	}

	cross_v3_v3v3(r_normal, cross[0], cross[1]);

	if (normalize_v3(r_normal) != 0.0f) {
		return true;
	}
	else {
		return false;
	}
}

bool ED_view3d_depth_unproject(
        const ARegion *ar,
        const int mval[2], const double depth,
        float r_location_world[3])
{
	float centx = (float)mval[0] + 0.5f;
	float centy = (float)mval[1] + 0.5f;
	return ED_view3d_unproject(ar, centx, centy, depth, r_location_world);
}

void ED_view3d_depth_tag_update(RegionView3D *rv3d)
{
	if (rv3d->depths)
		rv3d->depths->damaged = true;
}

/** \} */

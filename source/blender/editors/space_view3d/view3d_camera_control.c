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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_camera_control.c
 *  \ingroup spview3d
 *
 * The purpose of View3DCameraControl is to allow editing \a rv3d manipulation
 * (mainly \a ofs and \a viewquat) for the purpose of view navigation
 * without having to worry about positioning the camera, its parent...
 * or other details.
 *
 *
 * Typical view-control usage:
 *
 * - aquire a view-control (#ED_view3d_control_aquire).
 * - modify ``rv3d->ofs``, ``rv3d->viewquat``.
 * - update the view data (#ED_view3d_control_aquire) - within a loop which draws the viewport.
 * - finish and release the view-control (#ED_view3d_control_release),
 *   either keeping the current view or restoring the initial view.
 *
 * Notes:
 *
 * - when acquiring ``rv3d->dist`` is set to zero
 *   (so ``rv3d->ofs`` is always the view-point)
 * - updating can optionally keyframe the camera object.
 */

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_object.h"

#include "BKE_depsgraph.h" /* for object updating */

#include "ED_keyframing.h"
#include "ED_screen.h"

#include "view3d_intern.h"  /* own include */

#include "BLI_strict_flags.h"


typedef struct View3DCameraControl {

	/* -------------------------------------------------------------------- */
	/* Context (assign these to vars before use) */
	Scene        *ctx_scene;
	View3D       *ctx_v3d;
	RegionView3D *ctx_rv3d;


	/* -------------------------------------------------------------------- */
	/* internal vars */

	/* for parenting calculation */
	float view_mat_prev[4][4];


	/* -------------------------------------------------------------------- */
	/* optional capabilities */

	bool use_parent_root;


	/* -------------------------------------------------------------------- */
	/* intial values */

	/* root most parent */
	Object *root_parent;

	/* backup values */
	float dist_backup; /* backup the views distance since we use a zero dist for fly mode */
	float ofs_backup[3]; /* backup the views offset in case the user cancels flying in non camera mode */

	/* backup the views quat in case the user cancels flying in non camera mode.
	 * (quat for view, eul for camera) */
	float rot_backup[4];
	char persp_backup;  /* remember if were ortho or not, only used for restoring the view if it was a ortho view */

	/* are we flying an ortho camera in perspective view,
	 * which was originally in ortho view?
	 * could probably figure it out but better be explicit */
	bool is_ortho_cam;

	void *obtfm; /* backup the objects transform */
} View3DCameraControl;


BLI_INLINE Object *view3d_cameracontrol_object(View3DCameraControl *vctrl)
{
	return vctrl->root_parent ? vctrl->root_parent : vctrl->ctx_v3d->camera;
}


/**
 * Returns the object which is being manipulated or NULL.
 */
Object *ED_view3d_cameracontrol_object_get(View3DCameraControl *vctrl)
{
	RegionView3D *rv3d = vctrl->ctx_rv3d;

	if (rv3d->persp == RV3D_CAMOB) {
		return view3d_cameracontrol_object(vctrl);
	}
	else {
		return NULL;
	}
}


/**
 * Creates a #View3DControl handle and sets up
 * the view for first-person style navigation.
 */
struct View3DCameraControl *ED_view3d_cameracontrol_aquire(
        Scene *scene, View3D *v3d, RegionView3D *rv3d,
        const bool use_parent_root)
{
	View3DCameraControl *vctrl;

	vctrl = MEM_callocN(sizeof(View3DCameraControl), __func__);

	/* Store context */
	vctrl->ctx_scene = scene;
	vctrl->ctx_v3d = v3d;
	vctrl->ctx_rv3d = rv3d;

	vctrl->use_parent_root = use_parent_root;

	vctrl->persp_backup = rv3d->persp;
	vctrl->dist_backup = rv3d->dist;

	/* check for flying ortho camera - which we cant support well
	 * we _could_ also check for an ortho camera but this is easier */
	if ((rv3d->persp == RV3D_CAMOB) &&
	    (rv3d->is_persp == false))
	{
		((Camera *)v3d->camera->data)->type = CAM_PERSP;
		vctrl->is_ortho_cam = true;
	}

	if (rv3d->persp == RV3D_CAMOB) {
		Object *ob_back;
		if (use_parent_root && (vctrl->root_parent = v3d->camera->parent)) {
			while (vctrl->root_parent->parent)
				vctrl->root_parent = vctrl->root_parent->parent;
			ob_back = vctrl->root_parent;
		}
		else {
			ob_back = v3d->camera;
		}

		/* store the original camera loc and rot */
		vctrl->obtfm = BKE_object_tfm_backup(ob_back);

		BKE_object_where_is_calc(scene, v3d->camera);
		negate_v3_v3(rv3d->ofs, v3d->camera->obmat[3]);

		rv3d->dist = 0.0;
	}
	else {
		/* perspective or ortho */
		if (rv3d->persp == RV3D_ORTHO)
			rv3d->persp = RV3D_PERSP;  /* if ortho projection, make perspective */

		copy_qt_qt(vctrl->rot_backup, rv3d->viewquat);
		copy_v3_v3(vctrl->ofs_backup, rv3d->ofs);

		/* the dist defines a vector that is infront of the offset
		 * to rotate the view about.
		 * this is no good for fly mode because we
		 * want to rotate about the viewers center.
		 * but to correct the dist removal we must
		 * alter offset so the view doesn't jump. */

		ED_view3d_distance_set(rv3d, 0.0f);
		/* Done with correcting for the dist */
	}

	ED_view3d_to_m4(vctrl->view_mat_prev, rv3d->ofs, rv3d->viewquat, rv3d->dist);

	return vctrl;
}


/**
 * Updates cameras from the ``rv3d`` values, optionally auto-keyframing.
 */
void ED_view3d_cameracontrol_update(
        View3DCameraControl *vctrl,
        /* args for keyframing */
        const bool use_autokey,
        struct bContext *C, const bool do_rotate, const bool do_translate)
{
	/* we are in camera view so apply the view ofs and quat to the view matrix and set the camera to the view */

	Scene *scene       = vctrl->ctx_scene;
	View3D *v3d        = vctrl->ctx_v3d;
	RegionView3D *rv3d = vctrl->ctx_rv3d;

	ID *id_key;

	/* transform the parent or the camera? */
	if (vctrl->root_parent) {
		Object *ob_update;

		float view_mat[4][4];
		float prev_view_imat[4][4];
		float diff_mat[4][4];
		float parent_mat[4][4];

		invert_m4_m4(prev_view_imat, vctrl->view_mat_prev);
		ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
		mul_m4_m4m4(diff_mat, view_mat, prev_view_imat);
		mul_m4_m4m4(parent_mat, diff_mat, vctrl->root_parent->obmat);

		BKE_object_apply_mat4(vctrl->root_parent, parent_mat, true, false);

		ob_update = v3d->camera->parent;
		while (ob_update) {
			DAG_id_tag_update(&ob_update->id, OB_RECALC_OB);
			ob_update = ob_update->parent;
		}

		copy_m4_m4(vctrl->view_mat_prev, view_mat);

		id_key = &vctrl->root_parent->id;
	}
	else {
		float view_mat[4][4];
		float size_mat[4][4];
		float size_back[3];

		/* even though we handle the size matrix, this still changes over time */
		copy_v3_v3(size_back, v3d->camera->size);

		ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
		size_to_mat4(size_mat, v3d->camera->size);
		mul_m4_m4m4(view_mat, view_mat, size_mat);

		BKE_object_apply_mat4(v3d->camera, view_mat, true, true);

		copy_v3_v3(v3d->camera->size, size_back);

		id_key = &v3d->camera->id;
	}

	/* record the motion */
	if (use_autokey && autokeyframe_cfra_can_key(scene, id_key)) {
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
	}
}


/**
 * Release view control.
 *
 * \param restore  Sets the view state to the values that were set
 *                 before #ED_view3d_control_aquire was called.
 */
void ED_view3d_cameracontrol_release(
        View3DCameraControl *vctrl,
        const bool restore)
{
	View3D *v3d        = vctrl->ctx_v3d;
	RegionView3D *rv3d = vctrl->ctx_rv3d;

	if (restore) {
		/* Revert to original view? */
		if (vctrl->persp_backup == RV3D_CAMOB) { /* a camera view */
			Object *ob_back = view3d_cameracontrol_object(vctrl);

			/* store the original camera loc and rot */
			BKE_object_tfm_restore(ob_back, vctrl->obtfm);

			DAG_id_tag_update(&ob_back->id, OB_RECALC_OB);
		}
		else {
			/* Non Camera we need to reset the view back to the original location bacause the user canceled*/
			copy_qt_qt(rv3d->viewquat, vctrl->rot_backup);
			rv3d->persp = vctrl->persp_backup;
		}
		/* always, is set to zero otherwise */
		copy_v3_v3(rv3d->ofs, vctrl->ofs_backup);
		rv3d->dist = vctrl->dist_backup;
	}
	else if (vctrl->persp_backup == RV3D_CAMOB) { /* camera */
		DAG_id_tag_update((ID *)view3d_cameracontrol_object(vctrl), OB_RECALC_OB);

		/* always, is set to zero otherwise */
		copy_v3_v3(rv3d->ofs, vctrl->ofs_backup);
		rv3d->dist = vctrl->dist_backup;
	}
	else { /* not camera */
		/* Apply the fly mode view */
		/* restore the dist */
		ED_view3d_distance_set(rv3d, vctrl->dist_backup);
		/* Done with correcting for the dist */
	}

	if (vctrl->is_ortho_cam) {
		((Camera *)v3d->camera->data)->type = CAM_ORTHO;
	}

	if (vctrl->obtfm) {
		MEM_freeN(vctrl->obtfm);
	}

	MEM_freeN(vctrl);
}

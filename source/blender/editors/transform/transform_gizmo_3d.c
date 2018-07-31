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

/** \file blender/editors/transform/transform_gizmo_3d.c
 *  \ingroup edtransform
 *
 * \name 3D Transform Gizmo
 *
 * Used for 3D View
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_gpencil.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "BIF_gl.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_object.h"
#include "ED_particle.h"
#include "ED_view3d.h"
#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_resources.h"

/* local module include */
#include "transform.h"

#include "MEM_guardedalloc.h"

#include "GPU_select.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "DEG_depsgraph_query.h"

/* return codes for select, and drawing flags */

#define MAN_TRANS_X		(1 << 0)
#define MAN_TRANS_Y		(1 << 1)
#define MAN_TRANS_Z		(1 << 2)
#define MAN_TRANS_C		(MAN_TRANS_X | MAN_TRANS_Y | MAN_TRANS_Z)

#define MAN_ROT_X		(1 << 3)
#define MAN_ROT_Y		(1 << 4)
#define MAN_ROT_Z		(1 << 5)
#define MAN_ROT_C		(MAN_ROT_X | MAN_ROT_Y | MAN_ROT_Z)

#define MAN_SCALE_X		(1 << 8)
#define MAN_SCALE_Y		(1 << 9)
#define MAN_SCALE_Z		(1 << 10)
#define MAN_SCALE_C		(MAN_SCALE_X | MAN_SCALE_Y | MAN_SCALE_Z)

/* threshold for testing view aligned gizmo axis */
static struct {
	float min, max;
} g_tw_axis_range[2] = {
	/* Regular range */
	{0.02f, 0.1f},
	/* Use a different range because we flip the dot product,
	 * also the view aligned planes are harder to see so hiding early is preferred. */
	{0.175f,  0.25f},
};

/* axes as index */
enum {
	MAN_AXIS_TRANS_X = 0,
	MAN_AXIS_TRANS_Y,
	MAN_AXIS_TRANS_Z,
	MAN_AXIS_TRANS_C,

	MAN_AXIS_TRANS_XY,
	MAN_AXIS_TRANS_YZ,
	MAN_AXIS_TRANS_ZX,
#define MAN_AXIS_RANGE_TRANS_START MAN_AXIS_TRANS_X
#define MAN_AXIS_RANGE_TRANS_END (MAN_AXIS_TRANS_ZX + 1)

	MAN_AXIS_ROT_X,
	MAN_AXIS_ROT_Y,
	MAN_AXIS_ROT_Z,
	MAN_AXIS_ROT_C,
	MAN_AXIS_ROT_T, /* trackball rotation */
#define MAN_AXIS_RANGE_ROT_START MAN_AXIS_ROT_X
#define MAN_AXIS_RANGE_ROT_END (MAN_AXIS_ROT_T + 1)

	MAN_AXIS_SCALE_X,
	MAN_AXIS_SCALE_Y,
	MAN_AXIS_SCALE_Z,
	MAN_AXIS_SCALE_C,
	MAN_AXIS_SCALE_XY,
	MAN_AXIS_SCALE_YZ,
	MAN_AXIS_SCALE_ZX,
#define MAN_AXIS_RANGE_SCALE_START MAN_AXIS_SCALE_X
#define MAN_AXIS_RANGE_SCALE_END (MAN_AXIS_SCALE_ZX + 1)

	MAN_AXIS_LAST = MAN_AXIS_RANGE_SCALE_END,
};

/* axis types */
enum {
	MAN_AXES_ALL = 0,
	MAN_AXES_TRANSLATE,
	MAN_AXES_ROTATE,
	MAN_AXES_SCALE,
};

typedef struct GizmoGroup {
	bool all_hidden;
	int twtype;

	/* Users may change the twtype, detect changes to re-setup gizmo options. */
	int twtype_init;
	int twtype_prev;
	int use_twtype_refresh;

	struct wmGizmo *gizmos[MAN_AXIS_LAST];
} GizmoGroup;

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/* loop over axes */
#define MAN_ITER_AXES_BEGIN(axis, axis_idx) \
	{ \
		wmGizmo *axis; \
		int axis_idx; \
		for (axis_idx = 0; axis_idx < MAN_AXIS_LAST; axis_idx++) { \
			axis = gizmo_get_axis_from_index(man, axis_idx);

#define MAN_ITER_AXES_END \
		} \
	} ((void)0)

static wmGizmo *gizmo_get_axis_from_index(const GizmoGroup *man, const short axis_idx)
{
	BLI_assert(IN_RANGE_INCL(axis_idx, (float)MAN_AXIS_TRANS_X, (float)MAN_AXIS_LAST));
	return man->gizmos[axis_idx];
}

static short gizmo_get_axis_type(const int axis_idx)
{
	if (axis_idx >= MAN_AXIS_RANGE_TRANS_START && axis_idx < MAN_AXIS_RANGE_TRANS_END) {
		return MAN_AXES_TRANSLATE;
	}
	if (axis_idx >= MAN_AXIS_RANGE_ROT_START && axis_idx < MAN_AXIS_RANGE_ROT_END) {
		return MAN_AXES_ROTATE;
	}
	if (axis_idx >= MAN_AXIS_RANGE_SCALE_START && axis_idx < MAN_AXIS_RANGE_SCALE_END) {
		return MAN_AXES_SCALE;
	}
	BLI_assert(0);
	return -1;
}

static uint gizmo_orientation_axis(const int axis_idx, bool *r_is_plane)
{
	switch (axis_idx) {
		case MAN_AXIS_TRANS_YZ:
		case MAN_AXIS_SCALE_YZ:
			if (r_is_plane) {
				*r_is_plane = true;
			}
			ATTR_FALLTHROUGH;
		case MAN_AXIS_TRANS_X:
		case MAN_AXIS_ROT_X:
		case MAN_AXIS_SCALE_X:
			return 0;

		case MAN_AXIS_TRANS_ZX:
		case MAN_AXIS_SCALE_ZX:
			if (r_is_plane) {
				*r_is_plane = true;
			}
			ATTR_FALLTHROUGH;
		case MAN_AXIS_TRANS_Y:
		case MAN_AXIS_ROT_Y:
		case MAN_AXIS_SCALE_Y:
			return 1;

		case MAN_AXIS_TRANS_XY:
		case MAN_AXIS_SCALE_XY:
			if (r_is_plane) {
				*r_is_plane = true;
			}
			ATTR_FALLTHROUGH;
		case MAN_AXIS_TRANS_Z:
		case MAN_AXIS_ROT_Z:
		case MAN_AXIS_SCALE_Z:
			return 2;
	}
	return 3;
}

static bool gizmo_is_axis_visible(
        const RegionView3D *rv3d, const int twtype,
        const float idot[3], const int axis_type, const int axis_idx)
{
	if ((axis_idx >= MAN_AXIS_RANGE_ROT_START && axis_idx < MAN_AXIS_RANGE_ROT_END) == 0) {
		bool is_plane = false;
		const uint aidx_norm = gizmo_orientation_axis(axis_idx, &is_plane);
		/* don't draw axis perpendicular to the view */
		if (aidx_norm < 3) {
			float idot_axis = idot[aidx_norm];
			if (is_plane) {
				idot_axis = 1.0f - idot_axis;
			}
			if (idot_axis < g_tw_axis_range[is_plane].min) {
				return false;
			}
		}
	}

	if ((axis_type == MAN_AXES_TRANSLATE && !(twtype & SCE_MANIP_TRANSLATE)) ||
	    (axis_type == MAN_AXES_ROTATE && !(twtype & SCE_MANIP_ROTATE)) ||
	    (axis_type == MAN_AXES_SCALE && !(twtype & SCE_MANIP_SCALE)))
	{
		return false;
	}

	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
			return (rv3d->twdrawflag & MAN_TRANS_X);
		case MAN_AXIS_TRANS_Y:
			return (rv3d->twdrawflag & MAN_TRANS_Y);
		case MAN_AXIS_TRANS_Z:
			return (rv3d->twdrawflag & MAN_TRANS_Z);
		case MAN_AXIS_TRANS_C:
			return (rv3d->twdrawflag & MAN_TRANS_C);
		case MAN_AXIS_ROT_X:
			return (rv3d->twdrawflag & MAN_ROT_X);
		case MAN_AXIS_ROT_Y:
			return (rv3d->twdrawflag & MAN_ROT_Y);
		case MAN_AXIS_ROT_Z:
			return (rv3d->twdrawflag & MAN_ROT_Z);
		case MAN_AXIS_ROT_C:
		case MAN_AXIS_ROT_T:
			return (rv3d->twdrawflag & MAN_ROT_C);
		case MAN_AXIS_SCALE_X:
			return (rv3d->twdrawflag & MAN_SCALE_X);
		case MAN_AXIS_SCALE_Y:
			return (rv3d->twdrawflag & MAN_SCALE_Y);
		case MAN_AXIS_SCALE_Z:
			return (rv3d->twdrawflag & MAN_SCALE_Z);
		case MAN_AXIS_SCALE_C:
			return (rv3d->twdrawflag & MAN_SCALE_C && (twtype & SCE_MANIP_TRANSLATE) == 0);
		case MAN_AXIS_TRANS_XY:
			return (rv3d->twdrawflag & MAN_TRANS_X &&
			        rv3d->twdrawflag & MAN_TRANS_Y &&
			        (twtype & SCE_MANIP_ROTATE) == 0);
		case MAN_AXIS_TRANS_YZ:
			return (rv3d->twdrawflag & MAN_TRANS_Y &&
			        rv3d->twdrawflag & MAN_TRANS_Z &&
			        (twtype & SCE_MANIP_ROTATE) == 0);
		case MAN_AXIS_TRANS_ZX:
			return (rv3d->twdrawflag & MAN_TRANS_Z &&
			        rv3d->twdrawflag & MAN_TRANS_X &&
			        (twtype & SCE_MANIP_ROTATE) == 0);
		case MAN_AXIS_SCALE_XY:
			return (rv3d->twdrawflag & MAN_SCALE_X &&
			        rv3d->twdrawflag & MAN_SCALE_Y &&
			        (twtype & SCE_MANIP_TRANSLATE) == 0 &&
			        (twtype & SCE_MANIP_ROTATE) == 0);
		case MAN_AXIS_SCALE_YZ:
			return (rv3d->twdrawflag & MAN_SCALE_Y &&
			        rv3d->twdrawflag & MAN_SCALE_Z &&
			        (twtype & SCE_MANIP_TRANSLATE) == 0 &&
			        (twtype & SCE_MANIP_ROTATE) == 0);
		case MAN_AXIS_SCALE_ZX:
			return (rv3d->twdrawflag & MAN_SCALE_Z &&
			        rv3d->twdrawflag & MAN_SCALE_X &&
			        (twtype & SCE_MANIP_TRANSLATE) == 0 &&
			        (twtype & SCE_MANIP_ROTATE) == 0);
	}
	return false;
}

static void gizmo_get_axis_color(
        const int axis_idx, const float idot[3],
        float r_col[4], float r_col_hi[4])
{
	/* alpha values for normal/highlighted states */
	const float alpha = 0.6f;
	const float alpha_hi = 1.0f;
	float alpha_fac;

	if (axis_idx >= MAN_AXIS_RANGE_ROT_START && axis_idx < MAN_AXIS_RANGE_ROT_END) {
		/* Never fade rotation rings. */
		/* trackball rotation axis is a special case, we only draw a slight overlay */
		alpha_fac = (axis_idx == MAN_AXIS_ROT_T) ? 0.1f : 1.0f;
	}
	else {
		bool is_plane = false;
		const int axis_idx_norm = gizmo_orientation_axis(axis_idx, &is_plane);
		/* get alpha fac based on axis angle, to fade axis out when hiding it because it points towards view */
		if (axis_idx_norm < 3) {
			const float idot_min = g_tw_axis_range[is_plane].min;
			const float idot_max = g_tw_axis_range[is_plane].max;
			float idot_axis = idot[axis_idx_norm];
			if (is_plane) {
				idot_axis = 1.0f - idot_axis;
			}
			alpha_fac = (
			        (idot_axis > idot_max) ?
			        1.0f : (idot_axis < idot_min) ?
			        0.0f : ((idot_axis - idot_min) / (idot_max - idot_min)));
		}
		else {
			alpha_fac = 1.0f;
		}
	}

	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
		case MAN_AXIS_ROT_X:
		case MAN_AXIS_SCALE_X:
		case MAN_AXIS_TRANS_YZ:
		case MAN_AXIS_SCALE_YZ:
			UI_GetThemeColor4fv(TH_AXIS_X, r_col);
			break;
		case MAN_AXIS_TRANS_Y:
		case MAN_AXIS_ROT_Y:
		case MAN_AXIS_SCALE_Y:
		case MAN_AXIS_TRANS_ZX:
		case MAN_AXIS_SCALE_ZX:
			UI_GetThemeColor4fv(TH_AXIS_Y, r_col);
			break;
		case MAN_AXIS_TRANS_Z:
		case MAN_AXIS_ROT_Z:
		case MAN_AXIS_SCALE_Z:
		case MAN_AXIS_TRANS_XY:
		case MAN_AXIS_SCALE_XY:
			UI_GetThemeColor4fv(TH_AXIS_Z, r_col);
			break;
		case MAN_AXIS_TRANS_C:
		case MAN_AXIS_ROT_C:
		case MAN_AXIS_SCALE_C:
		case MAN_AXIS_ROT_T:
			copy_v4_fl(r_col, 1.0f);
			break;
	}

	copy_v4_v4(r_col_hi, r_col);

	r_col[3] = alpha * alpha_fac;
	r_col_hi[3] = alpha_hi * alpha_fac;
}

static void gizmo_get_axis_constraint(const int axis_idx, bool r_axis[3])
{
	ARRAY_SET_ITEMS(r_axis, 0, 0, 0);

	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
		case MAN_AXIS_ROT_X:
		case MAN_AXIS_SCALE_X:
			r_axis[0] = 1;
			break;
		case MAN_AXIS_TRANS_Y:
		case MAN_AXIS_ROT_Y:
		case MAN_AXIS_SCALE_Y:
			r_axis[1] = 1;
			break;
		case MAN_AXIS_TRANS_Z:
		case MAN_AXIS_ROT_Z:
		case MAN_AXIS_SCALE_Z:
			r_axis[2] = 1;
			break;
		case MAN_AXIS_TRANS_XY:
		case MAN_AXIS_SCALE_XY:
			r_axis[0] = r_axis[1] = 1;
			break;
		case MAN_AXIS_TRANS_YZ:
		case MAN_AXIS_SCALE_YZ:
			r_axis[1] = r_axis[2] = 1;
			break;
		case MAN_AXIS_TRANS_ZX:
		case MAN_AXIS_SCALE_ZX:
			r_axis[2] = r_axis[0] = 1;
			break;
		default:
			break;
	}
}


/* **************** Preparation Stuff **************** */

/* transform widget center calc helper for below */
static void calc_tw_center(struct TransformBounds *tbounds, const float co[3])
{
	minmax_v3v3_v3(tbounds->min, tbounds->max, co);
	add_v3_v3(tbounds->center, co);

	for (int i = 0; i < 3; i++) {
		const float d = dot_v3v3(tbounds->axis[i], co);
		tbounds->axis_min[i] = min_ff(d, tbounds->axis_min[i]);
		tbounds->axis_max[i] = max_ff(d, tbounds->axis_max[i]);
	}
}

static void protectflag_to_drawflags(short protectflag, short *drawflags)
{
	if (protectflag & OB_LOCK_LOCX)
		*drawflags &= ~MAN_TRANS_X;
	if (protectflag & OB_LOCK_LOCY)
		*drawflags &= ~MAN_TRANS_Y;
	if (protectflag & OB_LOCK_LOCZ)
		*drawflags &= ~MAN_TRANS_Z;

	if (protectflag & OB_LOCK_ROTX)
		*drawflags &= ~MAN_ROT_X;
	if (protectflag & OB_LOCK_ROTY)
		*drawflags &= ~MAN_ROT_Y;
	if (protectflag & OB_LOCK_ROTZ)
		*drawflags &= ~MAN_ROT_Z;

	if (protectflag & OB_LOCK_SCALEX)
		*drawflags &= ~MAN_SCALE_X;
	if (protectflag & OB_LOCK_SCALEY)
		*drawflags &= ~MAN_SCALE_Y;
	if (protectflag & OB_LOCK_SCALEZ)
		*drawflags &= ~MAN_SCALE_Z;
}

/* for pose mode */
static void protectflag_to_drawflags_pchan(RegionView3D *rv3d, const bPoseChannel *pchan)
{
	protectflag_to_drawflags(pchan->protectflag, &rv3d->twdrawflag);
}

/* for editmode*/
static void protectflag_to_drawflags_ebone(RegionView3D *rv3d, const EditBone *ebo)
{
	if (ebo->flag & BONE_EDITMODE_LOCKED) {
		protectflag_to_drawflags(OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE, &rv3d->twdrawflag);
	}
}

/* could move into BLI_math however this is only useful for display/editing purposes */
static void axis_angle_to_gimbal_axis(float gmat[3][3], const float axis[3], const float angle)
{
	/* X/Y are arbitrary axies, most importantly Z is the axis of rotation */

	float cross_vec[3];
	float quat[4];

	/* this is an un-scientific method to get a vector to cross with
	 * XYZ intentionally YZX */
	cross_vec[0] = axis[1];
	cross_vec[1] = axis[2];
	cross_vec[2] = axis[0];

	/* X-axis */
	cross_v3_v3v3(gmat[0], cross_vec, axis);
	normalize_v3(gmat[0]);
	axis_angle_to_quat(quat, axis, angle);
	mul_qt_v3(quat, gmat[0]);

	/* Y-axis */
	axis_angle_to_quat(quat, axis, M_PI_2);
	copy_v3_v3(gmat[1], gmat[0]);
	mul_qt_v3(quat, gmat[1]);

	/* Z-axis */
	copy_v3_v3(gmat[2], axis);

	normalize_m3(gmat);
}


static bool test_rotmode_euler(short rotmode)
{
	return (ELEM(rotmode, ROT_MODE_AXISANGLE, ROT_MODE_QUAT)) ? 0 : 1;
}

bool gimbal_axis(Object *ob, float gmat[3][3])
{
	if (ob->mode & OB_MODE_POSE) {
		bPoseChannel *pchan = BKE_pose_channel_active(ob);

		if (pchan) {
			float mat[3][3], tmat[3][3], obmat[3][3];
			if (test_rotmode_euler(pchan->rotmode)) {
				eulO_to_gimbal_axis(mat, pchan->eul, pchan->rotmode);
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				axis_angle_to_gimbal_axis(mat, pchan->rotAxis, pchan->rotAngle);
			}
			else { /* quat */
				return 0;
			}


			/* apply bone transformation */
			mul_m3_m3m3(tmat, pchan->bone->bone_mat, mat);

			if (pchan->parent) {
				float parent_mat[3][3];

				copy_m3_m4(parent_mat, pchan->parent->pose_mat);
				mul_m3_m3m3(mat, parent_mat, tmat);

				/* needed if object transformation isn't identity */
				copy_m3_m4(obmat, ob->obmat);
				mul_m3_m3m3(gmat, obmat, mat);
			}
			else {
				/* needed if object transformation isn't identity */
				copy_m3_m4(obmat, ob->obmat);
				mul_m3_m3m3(gmat, obmat, tmat);
			}

			normalize_m3(gmat);
			return 1;
		}
	}
	else {
		if (test_rotmode_euler(ob->rotmode)) {
			eulO_to_gimbal_axis(gmat, ob->rot, ob->rotmode);
		}
		else if (ob->rotmode == ROT_MODE_AXISANGLE) {
			axis_angle_to_gimbal_axis(gmat, ob->rotAxis, ob->rotAngle);
		}
		else { /* quat */
			return 0;
		}

		if (ob->parent) {
			float parent_mat[3][3];
			copy_m3_m4(parent_mat, ob->parent->obmat);
			normalize_m3(parent_mat);
			mul_m3_m3m3(gmat, parent_mat, gmat);
		}
		return 1;
	}

	return 0;
}


/* centroid, boundbox, of selection */
/* returns total items selected */
int ED_transform_calc_gizmo_stats(
        const bContext *C,
        const struct TransformCalcParams *params,
        struct TransformBounds *tbounds)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *obedit = CTX_data_edit_object(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	Base *base;
	Object *ob = OBACT(view_layer);
	bGPdata *gpd = CTX_data_gpencil_data(C);
	const bool is_gp_edit = GPENCIL_ANY_MODE(gpd);
	int a, totsel = 0;
	const int pivot_point = scene->toolsettings->transform_pivot_point;

	/* transform widget matrix */
	unit_m4(rv3d->twmat);

	unit_m3(rv3d->tw_axis_matrix);
	zero_v3(rv3d->tw_axis_min);
	zero_v3(rv3d->tw_axis_max);

	rv3d->twdrawflag = 0xFFFF;

	/* global, local or normal orientation?
	 * if we could check 'totsel' now, this should be skipped with no selection. */
	if (ob && !is_gp_edit) {
		const short orientation_type = params->orientation_type ? (params->orientation_type - 1) : scene->orientation_type;

		switch (orientation_type) {

			case V3D_MANIP_GLOBAL:
			{
				break; /* nothing to do */
			}
			case V3D_MANIP_GIMBAL:
			{
				float mat[3][3];
				if (gimbal_axis(ob, mat)) {
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* if not gimbal, fall through to normal */
				ATTR_FALLTHROUGH;
			}
			case V3D_MANIP_NORMAL:
			{
				if (obedit || ob->mode & OB_MODE_POSE) {
					float mat[3][3];
					ED_getTransformOrientationMatrix(C, mat, pivot_point);
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* no break we define 'normal' as 'local' in Object mode */
				ATTR_FALLTHROUGH;
			}
			case V3D_MANIP_LOCAL:
			{
				if (ob->mode & OB_MODE_POSE) {
					/* each bone moves on its own local axis, but  to avoid confusion,
					 * use the active pones axis for display [#33575], this works as expected on a single bone
					 * and users who select many bones will understand whats going on and what local means
					 * when they start transforming */
					float mat[3][3];
					ED_getTransformOrientationMatrix(C, mat, pivot_point);
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				copy_m4_m4(rv3d->twmat, ob->obmat);
				normalize_m4(rv3d->twmat);
				break;
			}
			case V3D_MANIP_VIEW:
			{
				float mat[3][3];
				copy_m3_m4(mat, rv3d->viewinv);
				normalize_m3(mat);
				copy_m4_m3(rv3d->twmat, mat);
				break;
			}
			case V3D_MANIP_CURSOR:
			{
				float mat[3][3];
				ED_view3d_cursor3d_calc_mat3(scene, v3d, mat);
				copy_m4_m3(rv3d->twmat, mat);
				break;
			}
			case V3D_MANIP_CUSTOM:
			{
				TransformOrientation *custom_orientation = BKE_scene_transform_orientation_find(
				        scene, scene->orientation_index_custom);
				float mat[3][3];

				if (applyTransformOrientation(custom_orientation, mat, NULL)) {
					copy_m4_m3(rv3d->twmat, mat);
				}
				break;
			}
		}
	}

	/* transform widget centroid/center */
	INIT_MINMAX(tbounds->min, tbounds->max);
	zero_v3(tbounds->center);

	copy_m3_m4(tbounds->axis, rv3d->twmat);
	if (params->use_local_axis && (ob && ob->mode & OB_MODE_EDIT)) {
		float diff_mat[3][3];
		copy_m3_m4(diff_mat, ob->obmat);
		normalize_m3(diff_mat);
		invert_m3(diff_mat);
		mul_m3_m3m3(tbounds->axis, tbounds->axis, diff_mat);
		normalize_m3(tbounds->axis);
	}

	for (int i = 0; i < 3; i++) {
		tbounds->axis_min[i] = +FLT_MAX;
		tbounds->axis_max[i] = -FLT_MAX;
	}

	if (is_gp_edit) {
		float diff_mat[4][4];
		float fpt[3];

		for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			/* only editable and visible layers are considered */
			if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {

				/* calculate difference matrix */
				ED_gpencil_parent_location(depsgraph, ob, gpd, gpl, diff_mat);

				for (bGPDstroke *gps = gpl->actframe->strokes.first; gps; gps = gps->next) {
					/* skip strokes that are invalid for current view */
					if (ED_gpencil_stroke_can_use(C, gps) == false) {
						continue;
					}

					/* we're only interested in selected points here... */
					if (gps->flag & GP_STROKE_SELECT) {
						bGPDspoint *pt;
						int i;

						/* Change selection status of all points, then make the stroke match */
						for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
							if (pt->flag & GP_SPOINT_SELECT) {
								if (gpl->parent == NULL) {
									calc_tw_center(tbounds, &pt->x);
									totsel++;
								}
								else {
									mul_v3_m4v3(fpt, diff_mat, &pt->x);
									calc_tw_center(tbounds, fpt);
									totsel++;
								}
							}
						}
					}
				}
			}
		}


		/* selection center */
		if (totsel) {
			mul_v3_fl(tbounds->center, 1.0f / (float)totsel);   /* centroid! */
		}
	}
	else if (obedit) {
		ob = obedit;
		if (obedit->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			BMEditSelection ese;
			float vec[3] = {0, 0, 0};

			/* USE LAST SELECTE WITH ACTIVE */
			if ((pivot_point == V3D_AROUND_ACTIVE) && BM_select_history_active_get(em->bm, &ese)) {
				BM_editselection_center(&ese, vec);
				calc_tw_center(tbounds, vec);
				totsel = 1;
			}
			else {
				BMesh *bm = em->bm;
				BMVert *eve;

				BMIter iter;

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							totsel++;
							calc_tw_center(tbounds, eve->co);
						}
					}
				}
			}
		} /* end editmesh */
		else if (obedit->type == OB_ARMATURE) {
			bArmature *arm = obedit->data;
			EditBone *ebo;

			if ((pivot_point == V3D_AROUND_ACTIVE) && (ebo = arm->act_edbone)) {
				/* doesn't check selection or visibility intentionally */
				if (ebo->flag & BONE_TIPSEL) {
					calc_tw_center(tbounds, ebo->tail);
					totsel++;
				}
				if ((ebo->flag & BONE_ROOTSEL) ||
				    ((ebo->flag & BONE_TIPSEL) == false))  /* ensure we get at least one point */
				{
					calc_tw_center(tbounds, ebo->head);
					totsel++;
				}
				protectflag_to_drawflags_ebone(rv3d, ebo);
			}
			else {
				for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
					if (EBONE_VISIBLE(arm, ebo)) {
						if (ebo->flag & BONE_TIPSEL) {
							calc_tw_center(tbounds, ebo->tail);
							totsel++;
						}
						if ((ebo->flag & BONE_ROOTSEL) &&
						    /* don't include same point multiple times */
						    ((ebo->flag & BONE_CONNECTED) &&
						     (ebo->parent != NULL) &&
						     (ebo->parent->flag & BONE_TIPSEL) &&
						     EBONE_VISIBLE(arm, ebo->parent)) == 0)
						{
							calc_tw_center(tbounds, ebo->head);
							totsel++;
						}
						if (ebo->flag & BONE_SELECTED) {
							protectflag_to_drawflags_ebone(rv3d, ebo);
						}
					}
				}
			}
		}
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu = obedit->data;
			float center[3];

			if ((pivot_point == V3D_AROUND_ACTIVE) && ED_curve_active_center(cu, center)) {
				calc_tw_center(tbounds, center);
				totsel++;
			}
			else {
				Nurb *nu;
				BezTriple *bezt;
				BPoint *bp;
				ListBase *nurbs = BKE_curve_editNurbs_get(cu);

				nu = nurbs->first;
				while (nu) {
					if (nu->type == CU_BEZIER) {
						bezt = nu->bezt;
						a = nu->pntsu;
						while (a--) {
							/* exceptions
							 * if handles are hidden then only check the center points.
							 * If the center knot is selected then only use this as the center point.
							 */
							if (cu->drawflag & CU_HIDE_HANDLES) {
								if (bezt->f2 & SELECT) {
									calc_tw_center(tbounds, bezt->vec[1]);
									totsel++;
								}
							}
							else if (bezt->f2 & SELECT) {
								calc_tw_center(tbounds, bezt->vec[1]);
								totsel++;
							}
							else {
								if (bezt->f1 & SELECT) {
									calc_tw_center(
									        tbounds, bezt->vec[(pivot_point == V3D_AROUND_LOCAL_ORIGINS) ? 1 : 0]);
									totsel++;
								}
								if (bezt->f3 & SELECT) {
									calc_tw_center(
									        tbounds, bezt->vec[(pivot_point == V3D_AROUND_LOCAL_ORIGINS) ? 1 : 2]);
									totsel++;
								}
							}
							bezt++;
						}
					}
					else {
						bp = nu->bp;
						a = nu->pntsu * nu->pntsv;
						while (a--) {
							if (bp->f1 & SELECT) {
								calc_tw_center(tbounds, bp->vec);
								totsel++;
							}
							bp++;
						}
					}
					nu = nu->next;
				}
			}
		}
		else if (obedit->type == OB_MBALL) {
			MetaBall *mb = (MetaBall *)obedit->data;
			MetaElem *ml;

			if ((pivot_point == V3D_AROUND_ACTIVE) && (ml = mb->lastelem)) {
				calc_tw_center(tbounds, &ml->x);
				totsel++;
			}
			else {
				for (ml = mb->editelems->first; ml; ml = ml->next) {
					if (ml->flag & SELECT) {
						calc_tw_center(tbounds, &ml->x);
						totsel++;
					}
				}
			}
		}
		else if (obedit->type == OB_LATTICE) {
			Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
			BPoint *bp;

			if ((pivot_point == V3D_AROUND_ACTIVE) && (bp = BKE_lattice_active_point_get(lt))) {
				calc_tw_center(tbounds, bp->vec);
				totsel++;
			}
			else {
				bp = lt->def;
				a = lt->pntsu * lt->pntsv * lt->pntsw;
				while (a--) {
					if (bp->f1 & SELECT) {
						calc_tw_center(tbounds, bp->vec);
						totsel++;
					}
					bp++;
				}
			}
		}

		/* selection center */
		if (totsel) {
			mul_v3_fl(tbounds->center, 1.0f / (float)totsel);   // centroid!
			mul_m4_v3(obedit->obmat, tbounds->center);
			mul_m4_v3(obedit->obmat, tbounds->min);
			mul_m4_v3(obedit->obmat, tbounds->max);
		}
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		bPoseChannel *pchan;
		int mode = TFM_ROTATION; // mislead counting bones... bah. We don't know the gizmo mode, could be mixed
		bool ok = false;

		if ((pivot_point == V3D_AROUND_ACTIVE) && (pchan = BKE_pose_channel_active(ob))) {
			/* doesn't check selection or visibility intentionally */
			Bone *bone = pchan->bone;
			if (bone) {
				calc_tw_center(tbounds, pchan->pose_head);
				protectflag_to_drawflags_pchan(rv3d, pchan);
				totsel = 1;
				ok = true;
			}
		}
		else {
			totsel = count_set_pose_transflags(ob, mode, V3D_AROUND_CENTER_BOUNDS, NULL);

			if (totsel) {
				/* use channels to get stats */
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					Bone *bone = pchan->bone;
					if (bone && (bone->flag & BONE_TRANSFORM)) {
						calc_tw_center(tbounds, pchan->pose_head);
						protectflag_to_drawflags_pchan(rv3d, pchan);
					}
				}
				ok = true;
			}
		}

		if (ok) {
			mul_v3_fl(tbounds->center, 1.0f / (float)totsel);   // centroid!
			mul_m4_v3(ob->obmat, tbounds->center);
			mul_m4_v3(ob->obmat, tbounds->min);
			mul_m4_v3(ob->obmat, tbounds->max);
		}
	}
	else if (ob && (ob->mode & OB_MODE_ALL_PAINT)) {
		/* pass */
	}
	else if (ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
		PTCacheEdit *edit = PE_get_current(scene, ob);
		PTCacheEditPoint *point;
		PTCacheEditKey *ek;
		int k;

		if (edit) {
			point = edit->points;
			for (a = 0; a < edit->totpoint; a++, point++) {
				if (point->flag & PEP_HIDE) continue;

				for (k = 0, ek = point->keys; k < point->totkey; k++, ek++) {
					if (ek->flag & PEK_SELECT) {
						calc_tw_center(tbounds, (ek->flag & PEK_USE_WCO) ? ek->world_co : ek->co);
						totsel++;
					}
				}
			}

			/* selection center */
			if (totsel)
				mul_v3_fl(tbounds->center, 1.0f / (float)totsel);  // centroid!
		}
	}
	else {

		/* we need the one selected object, if its not active */
		base = BASACT(view_layer);
		ob = OBACT(view_layer);
		if (base && ((base->flag & BASE_SELECTED) == 0)) ob = NULL;

		for (base = view_layer->object_bases.first; base; base = base->next) {
			if (!TESTBASELIB(base)) {
				continue;
			}
			if (ob == NULL) {
				ob = base->object;
			}
			if (params->use_only_center || base->object->bb == NULL) {
				calc_tw_center(tbounds, base->object->obmat[3]);
			}
			else {
				for (uint j = 0; j < 8; j++) {
					float co[3];
					mul_v3_m4v3(co, base->object->obmat, base->object->bb->vec[j]);
					calc_tw_center(tbounds, co);
				}
			}
			protectflag_to_drawflags(base->object->protectflag, &rv3d->twdrawflag);
			totsel++;
		}

		/* selection center */
		if (totsel) {
			mul_v3_fl(tbounds->center, 1.0f / (float)totsel);   // centroid!
		}
	}

	if (totsel == 0) {
		unit_m4(rv3d->twmat);
	}
	else {
		copy_v3_v3(rv3d->tw_axis_min, tbounds->axis_min);
		copy_v3_v3(rv3d->tw_axis_max, tbounds->axis_max);
		copy_m3_m3(rv3d->tw_axis_matrix, tbounds->axis);
	}

	return totsel;
}

static void gizmo_get_idot(RegionView3D *rv3d, float r_idot[3])
{
	float view_vec[3], axis_vec[3];
	ED_view3d_global_to_vector(rv3d, rv3d->twmat[3], view_vec);
	for (int i = 0; i < 3; i++) {
		normalize_v3_v3(axis_vec, rv3d->twmat[i]);
		r_idot[i] = 1.0f - fabsf(dot_v3v3(view_vec, axis_vec));
	}
}

static void gizmo_prepare_mat(
        const bContext *C, View3D *v3d, RegionView3D *rv3d, const struct TransformBounds *tbounds)
{
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);

	switch (scene->toolsettings->transform_pivot_point) {
		case V3D_AROUND_CENTER_BOUNDS:
		case V3D_AROUND_ACTIVE:
		{
			bGPdata *gpd = CTX_data_gpencil_data(C);
			Object *ob = OBACT(view_layer);

			if (((scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) &&
			     (OBEDIT_FROM_OBACT(ob) == NULL)) &&
			    ((gpd == NULL) || !(gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
			    (!(ob->mode & OB_MODE_POSE)))
			{
				copy_v3_v3(rv3d->twmat[3], ob->obmat[3]);
			}
			else {
				mid_v3_v3v3(rv3d->twmat[3], tbounds->min, tbounds->max);
			}
			break;
		}
		case V3D_AROUND_LOCAL_ORIGINS:
		case V3D_AROUND_CENTER_MEAN:
			copy_v3_v3(rv3d->twmat[3], tbounds->center);
			break;
		case V3D_AROUND_CURSOR:
			copy_v3_v3(rv3d->twmat[3], ED_view3d_cursor3d_get(scene, v3d)->location);
			break;
	}
}

/**
 * Sets up \a r_start and \a r_len to define arrow line range.
 * Needed to adjust line drawing for combined gizmo axis types.
 */
static void gizmo_line_range(const int twtype, const short axis_type, float *r_start, float *r_len)
{
	const float ofs = 0.2f;

	*r_start = 0.2f;
	*r_len = 1.0f;

	switch (axis_type) {
		case MAN_AXES_TRANSLATE:
			if (twtype & SCE_MANIP_SCALE) {
				*r_start = *r_len - ofs + 0.075f;
			}
			if (twtype & SCE_MANIP_ROTATE) {
				*r_len += ofs;
			}
			break;
		case MAN_AXES_SCALE:
			if (twtype & (SCE_MANIP_TRANSLATE | SCE_MANIP_ROTATE)) {
				*r_len -= ofs + 0.025f;
			}
			break;
	}

	*r_len -= *r_start;
}

static void gizmo_xform_message_subscribe(
        wmGizmoGroup *gzgroup, struct wmMsgBus *mbus,
        Scene *scene, bScreen *UNUSED(screen), ScrArea *UNUSED(sa), ARegion *ar, const void *type_fn)
{
	/* Subscribe to view properties */
	wmMsgSubscribeValue msg_sub_value_gz_tag_refresh = {
		.owner = ar,
		.user_data = gzgroup->parent_gzmap,
		.notify = WM_gizmo_do_msg_notify_tag_refresh,
	};

	PointerRNA scene_ptr;
	RNA_id_pointer_create(&scene->id, &scene_ptr);

	{
		extern PropertyRNA rna_Scene_transform_orientation;
		extern PropertyRNA rna_Scene_cursor_location;
		const PropertyRNA *props[] = {
			&rna_Scene_transform_orientation,
			(scene->toolsettings->transform_pivot_point == V3D_AROUND_CURSOR) ? &rna_Scene_cursor_location : NULL,
		};
		for (int i = 0; i < ARRAY_SIZE(props); i++) {
			if (props[i]) {
				WM_msg_subscribe_rna(mbus, &scene_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
			}
		}
	}

	PointerRNA toolsettings_ptr;
	RNA_pointer_create(&scene->id, &RNA_ToolSettings, scene->toolsettings, &toolsettings_ptr);

	if (type_fn == TRANSFORM_GGT_gizmo) {
		extern PropertyRNA rna_ToolSettings_transform_pivot_point;
		extern PropertyRNA rna_ToolSettings_use_gizmo_mode;
		const PropertyRNA *props[] = {
			&rna_ToolSettings_transform_pivot_point,
			&rna_ToolSettings_use_gizmo_mode,
		};
		for (int i = 0; i < ARRAY_SIZE(props); i++) {
			WM_msg_subscribe_rna(mbus, &toolsettings_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
		}
	}
	else if (type_fn == VIEW3D_GGT_xform_cage) {
		/* pass */
	}
	else {
		BLI_assert(0);
	}

	WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_gz_tag_refresh);
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name Transform Gizmo
 * \{ */

static GizmoGroup *gizmogroup_init(wmGizmoGroup *gzgroup)
{
	GizmoGroup *man;

	man = MEM_callocN(sizeof(GizmoGroup), "gizmo_data");

	const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
	const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);
	const wmGizmoType *gzt_prim = WM_gizmotype_find("GIZMO_GT_primitive_3d", true);

#define GIZMO_NEW_ARROW(v, draw_style) { \
	man->gizmos[v] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL); \
	RNA_enum_set(man->gizmos[v]->ptr, "draw_style", draw_style); \
} ((void)0)
#define GIZMO_NEW_DIAL(v, draw_options) { \
	man->gizmos[v] = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL); \
	RNA_enum_set(man->gizmos[v]->ptr, "draw_options", draw_options); \
} ((void)0)
#define GIZMO_NEW_PRIM(v, draw_style) { \
	man->gizmos[v] = WM_gizmo_new_ptr(gzt_prim, gzgroup, NULL); \
	RNA_enum_set(man->gizmos[v]->ptr, "draw_style", draw_style); \
} ((void)0)

	/* add/init widgets - order matters! */
	GIZMO_NEW_DIAL(MAN_AXIS_ROT_T, ED_GIZMO_DIAL_DRAW_FLAG_FILL);

	GIZMO_NEW_DIAL(MAN_AXIS_SCALE_C, ED_GIZMO_DIAL_DRAW_FLAG_NOP);

	GIZMO_NEW_ARROW(MAN_AXIS_SCALE_X, ED_GIZMO_ARROW_STYLE_BOX);
	GIZMO_NEW_ARROW(MAN_AXIS_SCALE_Y, ED_GIZMO_ARROW_STYLE_BOX);
	GIZMO_NEW_ARROW(MAN_AXIS_SCALE_Z, ED_GIZMO_ARROW_STYLE_BOX);

	GIZMO_NEW_PRIM(MAN_AXIS_SCALE_XY, ED_GIZMO_PRIMITIVE_STYLE_PLANE);
	GIZMO_NEW_PRIM(MAN_AXIS_SCALE_YZ, ED_GIZMO_PRIMITIVE_STYLE_PLANE);
	GIZMO_NEW_PRIM(MAN_AXIS_SCALE_ZX, ED_GIZMO_PRIMITIVE_STYLE_PLANE);

	GIZMO_NEW_DIAL(MAN_AXIS_ROT_X, ED_GIZMO_DIAL_DRAW_FLAG_CLIP);
	GIZMO_NEW_DIAL(MAN_AXIS_ROT_Y, ED_GIZMO_DIAL_DRAW_FLAG_CLIP);
	GIZMO_NEW_DIAL(MAN_AXIS_ROT_Z, ED_GIZMO_DIAL_DRAW_FLAG_CLIP);

	/* init screen aligned widget last here, looks better, behaves better */
	GIZMO_NEW_DIAL(MAN_AXIS_ROT_C, ED_GIZMO_DIAL_DRAW_FLAG_NOP);

	GIZMO_NEW_DIAL(MAN_AXIS_TRANS_C, ED_GIZMO_DIAL_DRAW_FLAG_NOP);

	GIZMO_NEW_ARROW(MAN_AXIS_TRANS_X, ED_GIZMO_ARROW_STYLE_NORMAL);
	GIZMO_NEW_ARROW(MAN_AXIS_TRANS_Y, ED_GIZMO_ARROW_STYLE_NORMAL);
	GIZMO_NEW_ARROW(MAN_AXIS_TRANS_Z, ED_GIZMO_ARROW_STYLE_NORMAL);

	GIZMO_NEW_PRIM(MAN_AXIS_TRANS_XY, ED_GIZMO_PRIMITIVE_STYLE_PLANE);
	GIZMO_NEW_PRIM(MAN_AXIS_TRANS_YZ, ED_GIZMO_PRIMITIVE_STYLE_PLANE);
	GIZMO_NEW_PRIM(MAN_AXIS_TRANS_ZX, ED_GIZMO_PRIMITIVE_STYLE_PLANE);

	man->gizmos[MAN_AXIS_ROT_T]->flag |= WM_GIZMO_SELECT_BACKGROUND;

	return man;
}

/**
 * Custom handler for gizmo widgets
 */
static int gizmo_modal(
        bContext *C, wmGizmo *widget, const wmEvent *event,
        eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
	/* Avoid unnecessary updates, partially address: T55458. */
	if (ELEM(event->type, TIMER, INBETWEEN_MOUSEMOVE)) {
		return OPERATOR_RUNNING_MODAL;
	}

	const ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	struct TransformBounds tbounds;


	if (ED_transform_calc_gizmo_stats(
	            C, &(struct TransformCalcParams){
	                .use_only_center = true,
	            }, &tbounds))
	{
		gizmo_prepare_mat(C, v3d, rv3d, &tbounds);
		WM_gizmo_set_matrix_location(widget, rv3d->twmat[3]);
	}

	ED_region_tag_redraw(ar);

	return OPERATOR_RUNNING_MODAL;
}

static void gizmogroup_init_properties_from_twtype(wmGizmoGroup *gzgroup)
{
	struct {
		wmOperatorType *translate, *rotate, *trackball, *resize;
	} ot_store = {NULL};
	GizmoGroup *man = gzgroup->customdata;
	MAN_ITER_AXES_BEGIN(axis, axis_idx)
	{
		const short axis_type = gizmo_get_axis_type(axis_idx);
		bool constraint_axis[3] = {1, 0, 0};
		PointerRNA *ptr;

		gizmo_get_axis_constraint(axis_idx, constraint_axis);

		/* custom handler! */
		WM_gizmo_set_fn_custom_modal(axis, gizmo_modal);

		switch (axis_idx) {
			case MAN_AXIS_TRANS_X:
			case MAN_AXIS_TRANS_Y:
			case MAN_AXIS_TRANS_Z:
			case MAN_AXIS_SCALE_X:
			case MAN_AXIS_SCALE_Y:
			case MAN_AXIS_SCALE_Z:
				if (axis_idx >= MAN_AXIS_RANGE_TRANS_START && axis_idx < MAN_AXIS_RANGE_TRANS_END) {
					int draw_options = 0;
					if ((man->twtype & (SCE_MANIP_ROTATE | SCE_MANIP_SCALE)) == 0) {
						draw_options |= ED_GIZMO_ARROW_DRAW_FLAG_STEM;
					}
					RNA_enum_set(axis->ptr, "draw_options", draw_options);
				}

				WM_gizmo_set_line_width(axis, GIZMO_AXIS_LINE_WIDTH);
				break;
			case MAN_AXIS_ROT_X:
			case MAN_AXIS_ROT_Y:
			case MAN_AXIS_ROT_Z:
				/* increased line width for better display */
				WM_gizmo_set_line_width(axis, GIZMO_AXIS_LINE_WIDTH + 1.0f);
				WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_VALUE, true);
				break;
			case MAN_AXIS_TRANS_XY:
			case MAN_AXIS_TRANS_YZ:
			case MAN_AXIS_TRANS_ZX:
			case MAN_AXIS_SCALE_XY:
			case MAN_AXIS_SCALE_YZ:
			case MAN_AXIS_SCALE_ZX:
			{
				const float ofs_ax = 7.0f;
				const float ofs[3] = {ofs_ax, ofs_ax, 0.0f};
				WM_gizmo_set_scale(axis, 0.07f);
				WM_gizmo_set_matrix_offset_location(axis, ofs);
				WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_OFFSET_SCALE, true);
				break;
			}
			case MAN_AXIS_TRANS_C:
			case MAN_AXIS_ROT_C:
			case MAN_AXIS_SCALE_C:
			case MAN_AXIS_ROT_T:
				WM_gizmo_set_line_width(axis, GIZMO_AXIS_LINE_WIDTH);
				if (axis_idx == MAN_AXIS_ROT_T) {
					WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_HOVER, true);
				}
				else if (axis_idx == MAN_AXIS_ROT_C) {
					WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_VALUE, true);
					WM_gizmo_set_scale(axis, 1.2f);
				}
				else {
					WM_gizmo_set_scale(axis, 0.2f);
				}
				break;
		}

		switch (axis_type) {
			case MAN_AXES_TRANSLATE:
				if (ot_store.translate == NULL) {
					ot_store.translate = WM_operatortype_find("TRANSFORM_OT_translate", true);
				}
				ptr = WM_gizmo_operator_set(axis, 0, ot_store.translate, NULL);
				break;
			case MAN_AXES_ROTATE:
			{
				wmOperatorType *ot_rotate;
				if (axis_idx == MAN_AXIS_ROT_T) {
					if (ot_store.trackball == NULL) {
						ot_store.trackball = WM_operatortype_find("TRANSFORM_OT_trackball", true);
					}
					ot_rotate = ot_store.trackball;
				}
				else {
					if (ot_store.rotate == NULL) {
						ot_store.rotate = WM_operatortype_find("TRANSFORM_OT_rotate", true);
					}
					ot_rotate = ot_store.rotate;
				}
				ptr = WM_gizmo_operator_set(axis, 0, ot_rotate, NULL);
				break;
			}
			case MAN_AXES_SCALE:
			{
				if (ot_store.resize == NULL) {
					ot_store.resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
				}
				ptr = WM_gizmo_operator_set(axis, 0, ot_store.resize, NULL);
				break;
			}
		}

		{
			PropertyRNA *prop;
			if ((prop = RNA_struct_find_property(ptr, "constraint_axis"))) {
				RNA_property_boolean_set_array(ptr, prop, constraint_axis);
			}
		}

		RNA_boolean_set(ptr, "release_confirm", 1);
	}
	MAN_ITER_AXES_END;
}

static void WIDGETGROUP_gizmo_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoGroup *man = gizmogroup_init(gzgroup);

	gzgroup->customdata = man;

	{
		man->twtype = 0;
		ScrArea *sa = CTX_wm_area(C);
		const bToolRef *tref = sa->runtime.tool;

		if (tref == NULL || STREQ(tref->idname, "Transform")) {
			/* Setup all gizmos, they can be toggled via 'ToolSettings.gizmo_flag' */
			man->twtype = SCE_MANIP_TRANSLATE | SCE_MANIP_ROTATE | SCE_MANIP_SCALE;
			man->use_twtype_refresh = true;
		}
		else if (STREQ(tref->idname, "Grab")) {
			man->twtype |= SCE_MANIP_TRANSLATE;
		}
		else if (STREQ(tref->idname, "Rotate")) {
			man->twtype |= SCE_MANIP_ROTATE;
		}
		else if (STREQ(tref->idname, "Scale")) {
			man->twtype |= SCE_MANIP_SCALE;
		}
		BLI_assert(man->twtype != 0);
		man->twtype_init = man->twtype;
	}

	/* *** set properties for axes *** */
	gizmogroup_init_properties_from_twtype(gzgroup);
}

static void WIDGETGROUP_gizmo_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoGroup *man = gzgroup->customdata;
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	struct TransformBounds tbounds;

	if (man->use_twtype_refresh) {
		Scene *scene = CTX_data_scene(C);
		man->twtype = scene->toolsettings->gizmo_flag & man->twtype_init;
		if (man->twtype != man->twtype_prev) {
			man->twtype_prev = man->twtype;
			gizmogroup_init_properties_from_twtype(gzgroup);
		}
	}

	/* skip, we don't draw anything anyway */
	if ((man->all_hidden =
	     (ED_transform_calc_gizmo_stats(
	             C, &(struct TransformCalcParams){
	                 .use_only_center = true,
	             }, &tbounds) == 0)))
	{
		return;
	}

	gizmo_prepare_mat(C, v3d, rv3d, &tbounds);

	/* *** set properties for axes *** */

	MAN_ITER_AXES_BEGIN(axis, axis_idx)
	{
		const short axis_type = gizmo_get_axis_type(axis_idx);
		const int aidx_norm = gizmo_orientation_axis(axis_idx, NULL);

		WM_gizmo_set_matrix_location(axis, rv3d->twmat[3]);

		switch (axis_idx) {
			case MAN_AXIS_TRANS_X:
			case MAN_AXIS_TRANS_Y:
			case MAN_AXIS_TRANS_Z:
			case MAN_AXIS_SCALE_X:
			case MAN_AXIS_SCALE_Y:
			case MAN_AXIS_SCALE_Z:
			{
				float start_co[3] = {0.0f, 0.0f, 0.0f};
				float len;

				gizmo_line_range(man->twtype, axis_type, &start_co[2], &len);

				WM_gizmo_set_matrix_rotation_from_z_axis(axis, rv3d->twmat[aidx_norm]);
				RNA_float_set(axis->ptr, "length", len);

				if (axis_idx >= MAN_AXIS_RANGE_TRANS_START && axis_idx < MAN_AXIS_RANGE_TRANS_END) {
					if (man->twtype & SCE_MANIP_ROTATE) {
						/* Avoid rotate and translate arrows overlap. */
						start_co[2] += 0.215f;
					}
				}
				WM_gizmo_set_matrix_offset_location(axis, start_co);
				WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_OFFSET_SCALE, true);
				break;
			}
			case MAN_AXIS_ROT_X:
			case MAN_AXIS_ROT_Y:
			case MAN_AXIS_ROT_Z:
				WM_gizmo_set_matrix_rotation_from_z_axis(axis, rv3d->twmat[aidx_norm]);
				break;
			case MAN_AXIS_TRANS_XY:
			case MAN_AXIS_TRANS_YZ:
			case MAN_AXIS_TRANS_ZX:
			case MAN_AXIS_SCALE_XY:
			case MAN_AXIS_SCALE_YZ:
			case MAN_AXIS_SCALE_ZX:
			{
				const float *y_axis = rv3d->twmat[aidx_norm - 1 < 0 ? 2 : aidx_norm - 1];
				const float *z_axis = rv3d->twmat[aidx_norm];
				WM_gizmo_set_matrix_rotation_from_yz_axis(axis, y_axis, z_axis);
				break;
			}
		}
	}
	MAN_ITER_AXES_END;
}

static void WIDGETGROUP_gizmo_message_subscribe(
        const bContext *C, wmGizmoGroup *gzgroup, struct wmMsgBus *mbus)
{
	Scene *scene = CTX_data_scene(C);
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	gizmo_xform_message_subscribe(gzgroup, mbus, scene, screen, sa, ar, TRANSFORM_GGT_gizmo);
}

static void WIDGETGROUP_gizmo_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoGroup *man = gzgroup->customdata;
	// ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	// View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	float idot[3];

	/* when looking through a selected camera, the gizmo can be at the
	 * exact same position as the view, skip so we don't break selection */
	if (man->all_hidden || fabsf(ED_view3d_pixel_size(rv3d, rv3d->twmat[3])) < 1e-6f) {
		MAN_ITER_AXES_BEGIN(axis, axis_idx)
		{
			WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, true);
		}
		MAN_ITER_AXES_END;
		return;
	}
	gizmo_get_idot(rv3d, idot);

	/* *** set properties for axes *** */

	MAN_ITER_AXES_BEGIN(axis, axis_idx)
	{
		const short axis_type = gizmo_get_axis_type(axis_idx);
		/* XXX maybe unset _HIDDEN flag on redraw? */
		if (gizmo_is_axis_visible(rv3d, man->twtype, idot, axis_type, axis_idx)) {
			WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, false);
		}
		else {
			WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, true);
			continue;
		}

		float color[4], color_hi[4];
		gizmo_get_axis_color(axis_idx, idot, color, color_hi);
		WM_gizmo_set_color(axis, color);
		WM_gizmo_set_color_highlight(axis, color_hi);

		switch (axis_idx) {
			case MAN_AXIS_TRANS_C:
			case MAN_AXIS_ROT_C:
			case MAN_AXIS_SCALE_C:
			case MAN_AXIS_ROT_T:
				WM_gizmo_set_matrix_rotation_from_z_axis(axis, rv3d->viewinv[2]);
				break;
		}
	}
	MAN_ITER_AXES_END;
}

static bool WIDGETGROUP_gizmo_poll(const struct bContext *C, struct wmGizmoGroupType *gzgt)
{
	/* it's a given we only use this in 3D view */
	bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
	if ((tref_rt == NULL) ||
	    !STREQ(gzgt->idname, tref_rt->gizmo_group))
	{
		WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
		return false;
	}

	View3D *v3d = CTX_wm_view3d(C);
	if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_TOOL)) {
		return false;
	}
	return true;
}

void TRANSFORM_GGT_gizmo(wmGizmoGroupType *gzgt)
{
	gzgt->name = "Transform Gizmo";
	gzgt->idname = "TRANSFORM_GGT_gizmo";

	gzgt->flag |= WM_GIZMOGROUPTYPE_3D;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = WIDGETGROUP_gizmo_poll;
	gzgt->setup = WIDGETGROUP_gizmo_setup;
	gzgt->refresh = WIDGETGROUP_gizmo_refresh;
	gzgt->message_subscribe = WIDGETGROUP_gizmo_message_subscribe;
	gzgt->draw_prepare = WIDGETGROUP_gizmo_draw_prepare;
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name Scale Cage Gizmo
 * \{ */

struct XFormCageWidgetGroup {
	wmGizmo *gizmo;
};

static bool WIDGETGROUP_xform_cage_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
	bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
	if (!STREQ(gzgt->idname, tref_rt->gizmo_group)) {
		WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
		return false;
	}
	return true;
}

static void WIDGETGROUP_xform_cage_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	struct XFormCageWidgetGroup *xgzgroup = MEM_mallocN(sizeof(struct XFormCageWidgetGroup), __func__);
	const wmGizmoType *gzt_cage = WM_gizmotype_find("GIZMO_GT_cage_3d", true);
	xgzgroup->gizmo = WM_gizmo_new_ptr(gzt_cage, gzgroup, NULL);
	wmGizmo *gz = xgzgroup->gizmo;

	RNA_enum_set(gz->ptr, "transform",
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE |
	             ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE);

	gz->color[0] = 1;
	gz->color_hi[0] = 1;

	gzgroup->customdata = xgzgroup;

	{
		wmOperatorType *ot_resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
		PointerRNA *ptr;

		/* assign operator */
		PropertyRNA *prop_release_confirm = NULL;
		PropertyRNA *prop_constraint_axis = NULL;

		int i = ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
		for (int x = 0; x < 3; x++) {
			for (int y = 0; y < 3; y++) {
				for (int z = 0; z < 3; z++) {
					bool constraint[3] = {x != 1, y != 1, z != 1};
					ptr = WM_gizmo_operator_set(gz, i, ot_resize, NULL);
					if (prop_release_confirm == NULL) {
						prop_release_confirm = RNA_struct_find_property(ptr, "release_confirm");
						prop_constraint_axis = RNA_struct_find_property(ptr, "constraint_axis");
					}
					RNA_property_boolean_set(ptr, prop_release_confirm, true);
					RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint);
					i++;
				}
			}
		}
	}
}

static void WIDGETGROUP_xform_cage_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	struct XFormCageWidgetGroup *xgzgroup = gzgroup->customdata;
	wmGizmo *gz = xgzgroup->gizmo;

	struct TransformBounds tbounds;

	if ((ED_transform_calc_gizmo_stats(
	             C, &(struct TransformCalcParams) {
	                 .use_local_axis = true,
	             }, &tbounds) == 0) ||
	    equals_v3v3(rv3d->tw_axis_min, rv3d->tw_axis_max))
	{
		WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
	}
	else {
		gizmo_prepare_mat(C, v3d, rv3d, &tbounds);

		WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
		WM_gizmo_set_flag(gz, WM_GIZMO_GRAB_CURSOR, true);

		float dims[3];
		sub_v3_v3v3(dims, rv3d->tw_axis_max, rv3d->tw_axis_min);
		RNA_float_set_array(gz->ptr, "dimensions", dims);
		mul_v3_fl(dims, 0.5f);

		copy_m4_m3(gz->matrix_offset, rv3d->tw_axis_matrix);
		mid_v3_v3v3(gz->matrix_offset[3], rv3d->tw_axis_max, rv3d->tw_axis_min);
		mul_m3_v3(rv3d->tw_axis_matrix, gz->matrix_offset[3]);

		PropertyRNA *prop_center_override = NULL;
		float center[3];
		float center_global[3];
		int i = ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
		for (int x = 0; x < 3; x++) {
			center[0] = (float)(1 - x) * dims[0];
			for (int y = 0; y < 3; y++) {
				center[1] = (float)(1 - y) * dims[1];
				for (int z = 0; z < 3; z++) {
					center[2] = (float)(1 - z) * dims[2];
					struct wmGizmoOpElem *mpop = WM_gizmo_operator_get(gz, i);
					if (prop_center_override == NULL) {
						prop_center_override = RNA_struct_find_property(&mpop->ptr, "center_override");
					}
					mul_v3_m4v3(center_global, gz->matrix_offset, center);
					RNA_property_float_set_array(&mpop->ptr, prop_center_override, center_global);
					i++;
				}
			}
		}
	}
}

static void WIDGETGROUP_xform_cage_message_subscribe(
        const bContext *C, wmGizmoGroup *gzgroup, struct wmMsgBus *mbus)
{
	Scene *scene = CTX_data_scene(C);
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	gizmo_xform_message_subscribe(gzgroup, mbus, scene, screen, sa, ar, VIEW3D_GGT_xform_cage);
}

static void WIDGETGROUP_xform_cage_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
	struct XFormCageWidgetGroup *xgzgroup = gzgroup->customdata;
	wmGizmo *gz = xgzgroup->gizmo;

	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob = OBACT(view_layer);
	if (ob && ob->mode & OB_MODE_EDIT) {
		copy_m4_m4(gz->matrix_space, ob->obmat);
	}
	else {
		unit_m4(gz->matrix_space);
	}
}

void VIEW3D_GGT_xform_cage(wmGizmoGroupType *gzgt)
{
	gzgt->name = "Transform Cage";
	gzgt->idname = "VIEW3D_GGT_xform_cage";

	gzgt->flag |= WM_GIZMOGROUPTYPE_3D;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = WIDGETGROUP_xform_cage_poll;
	gzgt->setup = WIDGETGROUP_xform_cage_setup;
	gzgt->refresh = WIDGETGROUP_xform_cage_refresh;
	gzgt->message_subscribe = WIDGETGROUP_xform_cage_message_subscribe;
	gzgt->draw_prepare = WIDGETGROUP_xform_cage_draw_prepare;
}

/** \} */

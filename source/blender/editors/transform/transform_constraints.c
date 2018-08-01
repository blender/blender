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

/** \file blender/editors/transform/transform_constraints.c
 *  \ingroup edtransform
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_rect.h"

#include "BKE_context.h"

#include "ED_image.h"
#include "ED_view3d.h"

#include "BLT_translation.h"

#include "UI_resources.h"

#include "transform.h"

static void drawObjectConstraint(TransInfo *t);

/* ************************** CONSTRAINTS ************************* */
static void constraintAutoValues(TransInfo *t, float vec[3])
{
	int mode = t->con.mode;
	if (mode & CON_APPLY) {
		float nval = (t->flag & T_NULL_ONE) ? 1.0f : 0.0f;

		if ((mode & CON_AXIS0) == 0) {
			vec[0] = nval;
		}
		if ((mode & CON_AXIS1) == 0) {
			vec[1] = nval;
		}
		if ((mode & CON_AXIS2) == 0) {
			vec[2] = nval;
		}
	}
}

void constraintNumInput(TransInfo *t, float vec[3])
{
	int mode = t->con.mode;
	if (mode & CON_APPLY) {
		float nval = (t->flag & T_NULL_ONE) ? 1.0f : 0.0f;

		const int dims = getConstraintSpaceDimension(t);
		if (dims == 2) {
			int axis = mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2);
			if (axis == (CON_AXIS0 | CON_AXIS1)) {
				/* vec[0] = vec[0]; */ /* same */
				/* vec[1] = vec[1]; */ /* same */
				vec[2] = nval;
			}
			else if (axis == (CON_AXIS1 | CON_AXIS2)) {
				vec[2] = vec[1];
				vec[1] = vec[0];
				vec[0] = nval;
			}
			else if (axis == (CON_AXIS0 | CON_AXIS2)) {
				/* vec[0] = vec[0]; */  /* same */
				vec[2] = vec[1];
				vec[1] = nval;
			}
		}
		else if (dims == 1) {
			if (mode & CON_AXIS0) {
				/* vec[0] = vec[0]; */ /* same */
				vec[1] = nval;
				vec[2] = nval;
			}
			else if (mode & CON_AXIS1) {
				vec[1] = vec[0];
				vec[0] = nval;
				vec[2] = nval;
			}
			else if (mode & CON_AXIS2) {
				vec[2] = vec[0];
				vec[0] = nval;
				vec[1] = nval;
			}
		}
	}
}

static void postConstraintChecks(TransInfo *t, float vec[3], float pvec[3])
{
	int i = 0;

	mul_m3_v3(t->con.imtx, vec);

	snapGridIncrement(t, vec);

	if (t->flag & T_NULL_ONE) {
		if (!(t->con.mode & CON_AXIS0))
			vec[0] = 1.0f;

		if (!(t->con.mode & CON_AXIS1))
			vec[1] = 1.0f;

		if (!(t->con.mode & CON_AXIS2))
			vec[2] = 1.0f;
	}

	if (applyNumInput(&t->num, vec)) {
		constraintNumInput(t, vec);
		removeAspectRatio(t, vec);
	}

	/* autovalues is operator param, use that directly but not if snapping is forced */
	if (t->flag & T_AUTOVALUES && (t->tsnap.status & SNAP_FORCED) == 0) {
		copy_v3_v3(vec, t->auto_values);
		constraintAutoValues(t, vec);
		/* inverse transformation at the end */
	}

	if (t->con.mode & CON_AXIS0) {
		pvec[i++] = vec[0];
	}
	if (t->con.mode & CON_AXIS1) {
		pvec[i++] = vec[1];
	}
	if (t->con.mode & CON_AXIS2) {
		pvec[i++] = vec[2];
	}

	mul_m3_v3(t->con.mtx, vec);
}

static void viewAxisCorrectCenter(const TransInfo *t, float t_con_center[3])
{
	if (t->spacetype == SPACE_VIEW3D) {
		// View3D *v3d = t->sa->spacedata.first;
		const float min_dist = 1.0f;  /* v3d->near; */
		float dir[3];
		float l;

		sub_v3_v3v3(dir, t_con_center, t->viewinv[3]);
		if (dot_v3v3(dir, t->viewinv[2]) < 0.0f) {
			negate_v3(dir);
		}
		project_v3_v3v3(dir, dir, t->viewinv[2]);

		l = len_v3(dir);

		if (l < min_dist) {
			float diff[3];
			normalize_v3_v3_length(diff, t->viewinv[2], min_dist - l);
			sub_v3_v3(t_con_center, diff);
		}
	}
}

/**
 * Axis calculation taking the view into account, correcting view-aligned axis.
 */
static void axisProjection(const TransInfo *t, const float axis[3], const float in[3], float out[3])
{
	float norm[3], vec[3], factor, angle;
	float t_con_center[3];

	if (is_zero_v3(in)) {
		return;
	}

	copy_v3_v3(t_con_center, t->center_global);

	/* checks for center being too close to the view center */
	viewAxisCorrectCenter(t, t_con_center);

	angle = fabsf(angle_v3v3(axis, t->viewinv[2]));
	if (angle > (float)M_PI_2) {
		angle = (float)M_PI - angle;
	}

	/* For when view is parallel to constraint... will cause NaNs otherwise
	 * So we take vertical motion in 3D space and apply it to the
	 * constraint axis. Nice for camera grab + MMB */
	if (angle < DEG2RADF(5.0f)) {
		project_v3_v3v3(vec, in, t->viewinv[1]);
		factor = dot_v3v3(t->viewinv[1], vec) * 2.0f;
		/* since camera distance is quite relative, use quadratic relationship. holding shift can compensate */
		if (factor < 0.0f) factor *= -factor;
		else factor *= factor;

		/* -factor makes move down going backwards */
		normalize_v3_v3_length(out, axis, -factor);
	}
	else {
		float v[3], i1[3], i2[3];
		float v2[3], v4[3];
		float norm_center[3];
		float plane[3];

		getViewVector(t, t_con_center, norm_center);
		cross_v3_v3v3(plane, norm_center, axis);

		project_v3_v3v3(vec, in, plane);
		sub_v3_v3v3(vec, in, vec);

		add_v3_v3v3(v, vec, t_con_center);
		getViewVector(t, v, norm);

		/* give arbitrary large value if projection is impossible */
		factor = dot_v3v3(axis, norm);
		if (1.0f - fabsf(factor) < 0.0002f) {
			copy_v3_v3(out, axis);
			if (factor > 0) {
				mul_v3_fl(out, 1000000000.0f);
			}
			else {
				mul_v3_fl(out, -1000000000.0f);
			}
		}
		else {
			add_v3_v3v3(v2, t_con_center, axis);
			add_v3_v3v3(v4, v, norm);

			isect_line_line_v3(t_con_center, v2, v, v4, i1, i2);

			sub_v3_v3v3(v, i2, v);

			sub_v3_v3v3(out, i1, t_con_center);

			/* possible some values become nan when
			 * viewpoint and object are both zero */
			if (!isfinite(out[0])) out[0] = 0.0f;
			if (!isfinite(out[1])) out[1] = 0.0f;
			if (!isfinite(out[2])) out[2] = 0.0f;
		}
	}
}

/**
 * Return true if the 2x axis are both aligned when projected into the view.
 * In this case, we can't usefully project the cursor onto the plane.
 */
static bool isPlaneProjectionViewAligned(const TransInfo *t)
{
	const float eps = 0.001f;
	const float *constraint_vector[2];
	int n = 0;
	for (int i = 0; i < 3; i++) {
		if (t->con.mode & (CON_AXIS0 << i)) {
			constraint_vector[n++] = t->con.mtx[i];
			if (n == 2) {
				break;
			}
		}
	}
	BLI_assert(n == 2);

	float view_to_plane[3], plane_normal[3];

	getViewVector(t, t->center_global, view_to_plane);

	cross_v3_v3v3(plane_normal, constraint_vector[0], constraint_vector[1]);
	normalize_v3(plane_normal);

	float factor = dot_v3v3(plane_normal, view_to_plane);
	return fabsf(factor) < eps;
}

static void planeProjection(const TransInfo *t, const float in[3], float out[3])
{
	float vec[3], factor, norm[3];

	add_v3_v3v3(vec, in, t->center_global);
	getViewVector(t, vec, norm);

	sub_v3_v3v3(vec, out, in);

	factor = dot_v3v3(vec, norm);
	if (fabsf(factor) <= 0.001f) {
		return; /* prevent divide by zero */
	}
	factor = dot_v3v3(vec, vec) / factor;

	copy_v3_v3(vec, norm);
	mul_v3_fl(vec, factor);

	add_v3_v3v3(out, in, vec);
}

/*
 * Generic callback for constant spatial constraints applied to linear motion
 *
 * The IN vector in projected into the constrained space and then further
 * projected along the view vector.
 * (in perspective mode, the view vector is relative to the position on screen)
 *
 */

static void applyAxisConstraintVec(
        TransInfo *t, TransDataContainer *UNUSED(tc), TransData *td, const float in[3], float out[3], float pvec[3])
{
	copy_v3_v3(out, in);
	if (!td && t->con.mode & CON_APPLY) {
		mul_m3_v3(t->con.pmtx, out);

		// With snap, a projection is alright, no need to correct for view alignment
		if (!validSnap(t)) {
			const int dims = getConstraintSpaceDimension(t);
			if (dims == 2) {
				if (!is_zero_v3(out)) {
					if (!isPlaneProjectionViewAligned(t)) {
						planeProjection(t, in, out);
					}
				}
			}
			else if (dims == 1) {
				float c[3];

				if (t->con.mode & CON_AXIS0) {
					copy_v3_v3(c, t->con.mtx[0]);
				}
				else if (t->con.mode & CON_AXIS1) {
					copy_v3_v3(c, t->con.mtx[1]);
				}
				else if (t->con.mode & CON_AXIS2) {
					copy_v3_v3(c, t->con.mtx[2]);
				}
				axisProjection(t, c, in, out);
			}
		}
		postConstraintChecks(t, out, pvec);
	}
}

/*
 * Generic callback for object based spatial constraints applied to linear motion
 *
 * At first, the following is applied to the first data in the array
 * The IN vector in projected into the constrained space and then further
 * projected along the view vector.
 * (in perspective mode, the view vector is relative to the position on screen)
 *
 * Further down, that vector is mapped to each data's space.
 */

static void applyObjectConstraintVec(
        TransInfo *t, TransDataContainer *tc, TransData *td, const float in[3], float out[3], float pvec[3])
{
	copy_v3_v3(out, in);
	if (t->con.mode & CON_APPLY) {
		if (!td) {
			mul_m3_v3(t->con.pmtx, out);

			const int dims = getConstraintSpaceDimension(t);
			if (dims == 2) {
				if (!is_zero_v3(out)) {
					if (!isPlaneProjectionViewAligned(t)) {
						planeProjection(t, in, out);
					}
				}
			}
			else if (dims == 1) {
				float c[3];

				if (t->con.mode & CON_AXIS0) {
					copy_v3_v3(c, t->con.mtx[0]);
				}
				else if (t->con.mode & CON_AXIS1) {
					copy_v3_v3(c, t->con.mtx[1]);
				}
				else if (t->con.mode & CON_AXIS2) {
					copy_v3_v3(c, t->con.mtx[2]);
				}
				axisProjection(t, c, in, out);
			}
			postConstraintChecks(t, out, pvec);
			copy_v3_v3(out, pvec);
		}
		else {
			int i = 0;

			out[0] = out[1] = out[2] = 0.0f;
			if (t->con.mode & CON_AXIS0) {
				out[0] = in[i++];
			}
			if (t->con.mode & CON_AXIS1) {
				out[1] = in[i++];
			}
			if (t->con.mode & CON_AXIS2) {
				out[2] = in[i++];
			}

			mul_m3_v3(td->axismtx, out);
			if (t->flag & T_EDIT) {
				mul_m3_v3(tc->mat3_unit, out);
			}
		}
	}
}

/*
 * Generic callback for constant spatial constraints applied to resize motion
 */

static void applyAxisConstraintSize(
        TransInfo *t, TransDataContainer *UNUSED(tc), TransData *td, float smat[3][3])
{
	if (!td && t->con.mode & CON_APPLY) {
		float tmat[3][3];

		if (!(t->con.mode & CON_AXIS0)) {
			smat[0][0] = 1.0f;
		}
		if (!(t->con.mode & CON_AXIS1)) {
			smat[1][1] = 1.0f;
		}
		if (!(t->con.mode & CON_AXIS2)) {
			smat[2][2] = 1.0f;
		}

		mul_m3_m3m3(tmat, smat, t->con.imtx);
		mul_m3_m3m3(smat, t->con.mtx, tmat);
	}
}

/*
 * Callback for object based spatial constraints applied to resize motion
 */

static void applyObjectConstraintSize(
        TransInfo *t, TransDataContainer *tc, TransData *td, float smat[3][3])
{
	if (td && t->con.mode & CON_APPLY) {
		float tmat[3][3];
		float imat[3][3];

		invert_m3_m3(imat, td->axismtx);

		if (!(t->con.mode & CON_AXIS0)) {
			smat[0][0] = 1.0f;
		}
		if (!(t->con.mode & CON_AXIS1)) {
			smat[1][1] = 1.0f;
		}
		if (!(t->con.mode & CON_AXIS2)) {
			smat[2][2] = 1.0f;
		}

		mul_m3_m3m3(tmat, smat, imat);
		if (t->flag & T_EDIT) {
			mul_m3_m3m3(smat, tc->mat3_unit, smat);
		}
		mul_m3_m3m3(smat, td->axismtx, tmat);
	}
}

/*
 * Generic callback for constant spatial constraints applied to rotations
 *
 * The rotation axis is copied into VEC.
 *
 * In the case of single axis constraints, the rotation axis is directly the one constrained to.
 * For planar constraints (2 axis), the rotation axis is the normal of the plane.
 *
 * The following only applies when CON_NOFLIP is not set.
 * The vector is then modified to always point away from the screen (in global space)
 * This insures that the rotation is always logically following the mouse.
 * (ie: not doing counterclockwise rotations when the mouse moves clockwise).
 */

static void applyAxisConstraintRot(TransInfo *t, TransDataContainer *UNUSED(tc), TransData *td, float vec[3], float *angle)
{
	if (!td && t->con.mode & CON_APPLY) {
		int mode = t->con.mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2);

		switch (mode) {
			case CON_AXIS0:
			case (CON_AXIS1 | CON_AXIS2):
				copy_v3_v3(vec, t->con.mtx[0]);
				break;
			case CON_AXIS1:
			case (CON_AXIS0 | CON_AXIS2):
				copy_v3_v3(vec, t->con.mtx[1]);
				break;
			case CON_AXIS2:
			case (CON_AXIS0 | CON_AXIS1):
				copy_v3_v3(vec, t->con.mtx[2]);
				break;
		}
		/* don't flip axis if asked to or if num input */
		if (angle && (mode & CON_NOFLIP) == 0 && hasNumInput(&t->num) == 0) {
			if (dot_v3v3(vec, t->viewinv[2]) > 0.0f) {
				*angle = -(*angle);
			}
		}
	}
}

/*
 * Callback for object based spatial constraints applied to rotations
 *
 * The rotation axis is copied into VEC.
 *
 * In the case of single axis constraints, the rotation axis is directly the one constrained to.
 * For planar constraints (2 axis), the rotation axis is the normal of the plane.
 *
 * The following only applies when CON_NOFLIP is not set.
 * The vector is then modified to always point away from the screen (in global space)
 * This insures that the rotation is always logically following the mouse.
 * (ie: not doing counterclockwise rotations when the mouse moves clockwise).
 */

static void applyObjectConstraintRot(
        TransInfo *t, TransDataContainer *tc, TransData *td, float vec[3], float *angle)
{
	if (t->con.mode & CON_APPLY) {
		int mode = t->con.mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2);
		float tmp_axismtx[3][3];
		float (*axismtx)[3];

		/* on setup call, use first object */
		if (td == NULL) {
			BLI_assert(tc == NULL);
			tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
			td = tc->data;
		}

		if (t->flag & T_EDIT) {
			mul_m3_m3m3(tmp_axismtx, tc->mat3_unit, td->axismtx);
			axismtx = tmp_axismtx;
		}
		else {
			axismtx = td->axismtx;
		}

		switch (mode) {
			case CON_AXIS0:
			case (CON_AXIS1 | CON_AXIS2):
				copy_v3_v3(vec, axismtx[0]);
				break;
			case CON_AXIS1:
			case (CON_AXIS0 | CON_AXIS2):
				copy_v3_v3(vec, axismtx[1]);
				break;
			case CON_AXIS2:
			case (CON_AXIS0 | CON_AXIS1):
				copy_v3_v3(vec, axismtx[2]);
				break;
		}
		if (angle && (mode & CON_NOFLIP) == 0 && hasNumInput(&t->num) == 0) {
			if (dot_v3v3(vec, t->viewinv[2]) > 0.0f) {
				*angle = -(*angle);
			}
		}
	}
}

/*--------------------- INTERNAL SETUP CALLS ------------------*/

void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[])
{
	BLI_strncpy(t->con.text + 1, text, sizeof(t->con.text) - 1);
	copy_m3_m3(t->con.mtx, space);
	t->con.mode = mode;
	getConstraintMatrix(t);

	startConstraint(t);

	t->con.drawExtra = NULL;
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = TREDRAW_HARD;
}

/* applies individual td->axismtx constraints */
void setAxisMatrixConstraint(TransInfo *t, int mode, const char text[])
{
	TransDataContainer *tc = t->data_container;
	if (t->data_len_all == 1) {
		float axismtx[3][3];
		if (t->flag & T_EDIT) {
			mul_m3_m3m3(axismtx, tc->mat3_unit, tc->data->axismtx);
		}
		else {
			copy_m3_m3(axismtx, tc->data->axismtx);
		}

		setConstraint(t, axismtx, mode, text);
	}
	else {
		BLI_strncpy(t->con.text + 1, text, sizeof(t->con.text) - 1);
		copy_m3_m3(t->con.mtx, tc->data->axismtx);
		t->con.mode = mode;
		getConstraintMatrix(t);

		startConstraint(t);

		t->con.drawExtra = drawObjectConstraint;
		t->con.applyVec = applyObjectConstraintVec;
		t->con.applySize = applyObjectConstraintSize;
		t->con.applyRot = applyObjectConstraintRot;
		t->redraw = TREDRAW_HARD;
	}
}

void setLocalConstraint(TransInfo *t, int mode, const char text[])
{
	/* edit-mode now allows local transforms too */
	if (t->flag & T_EDIT) {
		/* Use the active (first) edit object. */
		TransDataContainer *tc = t->data_container;
		setConstraint(t, tc->mat3_unit, mode, text);
	}
	else {
		setAxisMatrixConstraint(t, mode, text);
	}
}

/*
 * Set the constraint according to the user defined orientation
 *
 * ftext is a format string passed to BLI_snprintf. It will add the name of
 * the orientation where %s is (logically).
 */
void setUserConstraint(TransInfo *t, short orientation, int mode, const char ftext[])
{
	char text[256];

	switch (orientation) {
		case V3D_MANIP_GLOBAL:
		{
			float mtx[3][3];
			BLI_snprintf(text, sizeof(text), ftext, IFACE_("global"));
			unit_m3(mtx);
			setConstraint(t, mtx, mode, text);
			break;
		}
		case V3D_MANIP_LOCAL:
			BLI_snprintf(text, sizeof(text), ftext, IFACE_("local"));
			setLocalConstraint(t, mode, text);
			break;
		case V3D_MANIP_NORMAL:
			BLI_snprintf(text, sizeof(text), ftext, IFACE_("normal"));
			if (checkUseAxisMatrix(t)) {
				setAxisMatrixConstraint(t, mode, text);
			}
			else {
				setConstraint(t, t->spacemtx, mode, text);
			}
			break;
		case V3D_MANIP_VIEW:
			BLI_snprintf(text, sizeof(text), ftext, IFACE_("view"));
			setConstraint(t, t->spacemtx, mode, text);
			break;
		case V3D_MANIP_CURSOR:
			BLI_snprintf(text, sizeof(text), ftext, IFACE_("cursor"));
			setConstraint(t, t->spacemtx, mode, text);
			break;
		case V3D_MANIP_GIMBAL:
			BLI_snprintf(text, sizeof(text), ftext, IFACE_("gimbal"));
			setConstraint(t, t->spacemtx, mode, text);
			break;
		case V3D_MANIP_CUSTOM:
		{
			char orientation_str[128];
			BLI_snprintf(orientation_str, sizeof(orientation_str), "%s \"%s\"",
			             IFACE_("custom orientation"), t->custom_orientation->name);
			BLI_snprintf(text, sizeof(text), ftext, orientation_str);
			setConstraint(t, t->spacemtx, mode, text);
			break;
		}
	}

	t->con.orientation = orientation;

	t->con.mode |= CON_USER;
}

/*----------------- DRAWING CONSTRAINTS -------------------*/

void drawConstraint(TransInfo *t)
{
	TransCon *tc = &(t->con);

	if (!ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE, SPACE_NODE))
		return;
	if (!(tc->mode & CON_APPLY))
		return;
	if (t->flag & T_NO_CONSTRAINT)
		return;

	if (tc->drawExtra) {
		tc->drawExtra(t);
	}
	else {
		if (tc->mode & CON_SELECT) {
			float vec[3];
			int depth_test_enabled;

			convertViewVec(t, vec, (t->mval[0] - t->con.imval[0]), (t->mval[1] - t->con.imval[1]));
			add_v3_v3(vec, t->center_global);

			drawLine(t, t->center_global, tc->mtx[0], 'X', 0);
			drawLine(t, t->center_global, tc->mtx[1], 'Y', 0);
			drawLine(t, t->center_global, tc->mtx[2], 'Z', 0);

			depth_test_enabled = GPU_depth_test_enabled();
			if (depth_test_enabled)
				GPU_depth_test(false);

			const uint shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

			float viewport_size[4];
			GPU_viewport_size_get_f(viewport_size);
			immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

			immUniform1i("colors_len", 0);  /* "simple" mode */
			immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			immUniform1f("dash_width", 2.0f);
			immUniform1f("dash_factor", 0.5f);

			immBegin(GPU_PRIM_LINES, 2);
			immVertex3fv(shdr_pos, t->center_global);
			immVertex3fv(shdr_pos, vec);
			immEnd();

			immUnbindProgram();

			if (depth_test_enabled)
				GPU_depth_test(true);
		}

		if (tc->mode & CON_AXIS0) {
			drawLine(t, t->center_global, tc->mtx[0], 'X', DRAWLIGHT);
		}
		if (tc->mode & CON_AXIS1) {
			drawLine(t, t->center_global, tc->mtx[1], 'Y', DRAWLIGHT);
		}
		if (tc->mode & CON_AXIS2) {
			drawLine(t, t->center_global, tc->mtx[2], 'Z', DRAWLIGHT);
		}
	}
}

/* called from drawview.c, as an extra per-window draw option */
void drawPropCircle(const struct bContext *C, TransInfo *t)
{
	if (t->flag & T_PROP_EDIT) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		float tmat[4][4], imat[4][4];
		int depth_test_enabled;

		if (t->spacetype == SPACE_VIEW3D && rv3d != NULL) {
			copy_m4_m4(tmat, rv3d->viewmat);
			invert_m4_m4(imat, tmat);
		}
		else {
			unit_m4(tmat);
			unit_m4(imat);
		}

		GPU_matrix_push();

		if (t->spacetype == SPACE_VIEW3D) {
			/* pass */
		}
		else if (t->spacetype == SPACE_IMAGE) {
			GPU_matrix_scale_2f(1.0f / t->aspect[0], 1.0f / t->aspect[1]);
		}
		else if (ELEM(t->spacetype, SPACE_IPO, SPACE_ACTION)) {
			/* only scale y */
			rcti *mask = &t->ar->v2d.mask;
			rctf *datamask = &t->ar->v2d.cur;
			float xsize = BLI_rctf_size_x(datamask);
			float ysize = BLI_rctf_size_y(datamask);
			float xmask = BLI_rcti_size_x(mask);
			float ymask = BLI_rcti_size_y(mask);
			GPU_matrix_scale_2f(1.0f, (ysize / xsize) * (xmask / ymask));
		}

		depth_test_enabled = GPU_depth_test_enabled();
		if (depth_test_enabled)
			GPU_depth_test(false);

		uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformThemeColor(TH_GRID);

		set_inverted_drawing(1);
		imm_drawcircball(t->center_global, t->prop_size, imat, pos);
		set_inverted_drawing(0);

		immUnbindProgram();

		if (depth_test_enabled)
			GPU_depth_test(true);

		GPU_matrix_pop();
	}
}

static void drawObjectConstraint(TransInfo *t)
{
	/* Draw the first one lighter because that's the one who controls the others.
	 * Meaning the transformation is projected on that one and just copied on the others
	 * constraint space.
	 * In a nutshell, the object with light axis is controlled by the user and the others follow.
	 * Without drawing the first light, users have little clue what they are doing.
	 */
	short options = DRAWLIGHT;
	int i;
	float tmp_axismtx[3][3];

	FOREACH_TRANS_DATA_CONTAINER (t, tc) {
		TransData *td = tc->data;
		for (i = 0; i < tc->data_len; i++, td++) {
			float co[3];
			float (*axismtx)[3];

			if (t->flag & T_PROP_EDIT) {
				/* we're sorted, so skip the rest */
				if (td->factor == 0.0f) {
					break;
				}
			}

			if (t->options & CTX_GPENCIL_STROKES) {
				/* only draw a constraint line for one point, otherwise we can't see anything */
				if ((options & DRAWLIGHT) == 0) {
					break;
				}
			}

			if (t->flag & T_OBJECT) {
				copy_v3_v3(co, td->ob->obmat[3]);
				axismtx = td->axismtx;
			}
			else if (t->flag & T_EDIT) {
				mul_v3_m4v3(co, tc->mat, td->center);

				mul_m3_m3m3(tmp_axismtx, tc->mat3_unit, td->axismtx);
				axismtx = tmp_axismtx;
			}
			else if (t->flag & T_POSE) {
				mul_v3_m4v3(co, tc->mat, td->center);
				axismtx = td->axismtx;
			}
			else {
				copy_v3_v3(co, td->center);
				axismtx = td->axismtx;
			}

			if (t->con.mode & CON_AXIS0) {
				drawLine(t, co, axismtx[0], 'X', options);
			}
			if (t->con.mode & CON_AXIS1) {
				drawLine(t, co, axismtx[1], 'Y', options);
			}
			if (t->con.mode & CON_AXIS2) {
				drawLine(t, co, axismtx[2], 'Z', options);
			}
			options &= ~DRAWLIGHT;
		}
	}
}

/*--------------------- START / STOP CONSTRAINTS ---------------------- */

void startConstraint(TransInfo *t)
{
	t->con.mode |= CON_APPLY;
	*t->con.text = ' ';
	t->num.idx_max = min_ii(getConstraintSpaceDimension(t) - 1, t->idx_max);
}

void stopConstraint(TransInfo *t)
{
	t->con.mode &= ~(CON_APPLY | CON_SELECT);
	*t->con.text = '\0';
	t->num.idx_max = t->idx_max;
}

void getConstraintMatrix(TransInfo *t)
{
	float mat[3][3];
	invert_m3_m3(t->con.imtx, t->con.mtx);
	unit_m3(t->con.pmtx);

	if (!(t->con.mode & CON_AXIS0)) {
		zero_v3(t->con.pmtx[0]);
	}

	if (!(t->con.mode & CON_AXIS1)) {
		zero_v3(t->con.pmtx[1]);
	}

	if (!(t->con.mode & CON_AXIS2)) {
		zero_v3(t->con.pmtx[2]);
	}

	mul_m3_m3m3(mat, t->con.pmtx, t->con.imtx);
	mul_m3_m3m3(t->con.pmtx, t->con.mtx, mat);
}

/*------------------------- MMB Select -------------------------------*/

void initSelectConstraint(TransInfo *t, float mtx[3][3])
{
	copy_m3_m3(t->con.mtx, mtx);
	t->con.mode |= CON_APPLY;
	t->con.mode |= CON_SELECT;

	setNearestAxis(t);
	t->con.drawExtra = NULL;
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
}

void selectConstraint(TransInfo *t)
{
	if (t->con.mode & CON_SELECT) {
		setNearestAxis(t);
		startConstraint(t);
	}
}

void postSelectConstraint(TransInfo *t)
{
	if (!(t->con.mode & CON_SELECT))
		return;

	t->con.mode &= ~CON_AXIS0;
	t->con.mode &= ~CON_AXIS1;
	t->con.mode &= ~CON_AXIS2;
	t->con.mode &= ~CON_SELECT;

	setNearestAxis(t);

	startConstraint(t);
	t->redraw = TREDRAW_HARD;
}

static void setNearestAxis2d(TransInfo *t)
{
	/* no correction needed... just use whichever one is lower */
	if (abs(t->mval[0] - t->con.imval[0]) < abs(t->mval[1] - t->con.imval[1])) {
		t->con.mode |= CON_AXIS1;
		BLI_strncpy(t->con.text, IFACE_(" along Y axis"), sizeof(t->con.text));
	}
	else {
		t->con.mode |= CON_AXIS0;
		BLI_strncpy(t->con.text, IFACE_(" along X axis"), sizeof(t->con.text));
	}
}

static void setNearestAxis3d(TransInfo *t)
{
	float zfac;
	float mvec[3], proj[3];
	float len[3];
	int i;

	/* calculate mouse movement */
	mvec[0] = (float)(t->mval[0] - t->con.imval[0]);
	mvec[1] = (float)(t->mval[1] - t->con.imval[1]);
	mvec[2] = 0.0f;

	/* we need to correct axis length for the current zoomlevel of view,
	 * this to prevent projected values to be clipped behind the camera
	 * and to overflow the short integers.
	 * The formula used is a bit stupid, just a simplification of the subtraction
	 * of two 2D points 30 pixels apart (that's the last factor in the formula) after
	 * projecting them with ED_view3d_win_to_delta and then get the length of that vector.
	 */
	zfac = mul_project_m4_v3_zfac(t->persmat, t->center_global);
	zfac = len_v3(t->persinv[0]) * 2.0f / t->ar->winx * zfac * 30.0f;

	for (i = 0; i < 3; i++) {
		float axis[3], axis_2d[2];

		copy_v3_v3(axis, t->con.mtx[i]);

		mul_v3_fl(axis, zfac);
		/* now we can project to get window coordinate */
		add_v3_v3(axis, t->center_global);
		projectFloatView(t, axis, axis_2d);

		sub_v2_v2v2(axis, axis_2d, t->center2d);
		axis[2] = 0.0f;

		if (normalize_v3(axis) > 1e-3f) {
			project_v3_v3v3(proj, mvec, axis);
			sub_v3_v3v3(axis, mvec, proj);
			len[i] = normalize_v3(axis);
		}
		else {
			len[i] = 1e10f;
		}
	}

	if (len[0] <= len[1] && len[0] <= len[2]) {
		if (t->modifiers & MOD_CONSTRAINT_PLANE) {
			t->con.mode |= (CON_AXIS1 | CON_AXIS2);
			BLI_snprintf(t->con.text, sizeof(t->con.text), IFACE_(" locking %s X axis"), t->spacename);
		}
		else {
			t->con.mode |= CON_AXIS0;
			BLI_snprintf(t->con.text, sizeof(t->con.text), IFACE_(" along %s X axis"), t->spacename);
		}
	}
	else if (len[1] <= len[0] && len[1] <= len[2]) {
		if (t->modifiers & MOD_CONSTRAINT_PLANE) {
			t->con.mode |= (CON_AXIS0 | CON_AXIS2);
			BLI_snprintf(t->con.text, sizeof(t->con.text), IFACE_(" locking %s Y axis"), t->spacename);
		}
		else {
			t->con.mode |= CON_AXIS1;
			BLI_snprintf(t->con.text, sizeof(t->con.text), IFACE_(" along %s Y axis"), t->spacename);
		}
	}
	else if (len[2] <= len[1] && len[2] <= len[0]) {
		if (t->modifiers & MOD_CONSTRAINT_PLANE) {
			t->con.mode |= (CON_AXIS0 | CON_AXIS1);
			BLI_snprintf(t->con.text, sizeof(t->con.text), IFACE_(" locking %s Z axis"), t->spacename);
		}
		else {
			t->con.mode |= CON_AXIS2;
			BLI_snprintf(t->con.text, sizeof(t->con.text), IFACE_(" along %s Z axis"), t->spacename);
		}
	}
}

void setNearestAxis(TransInfo *t)
{
	/* clear any prior constraint flags */
	t->con.mode &= ~CON_AXIS0;
	t->con.mode &= ~CON_AXIS1;
	t->con.mode &= ~CON_AXIS2;

	/* constraint setting - depends on spacetype */
	if (t->spacetype == SPACE_VIEW3D) {
		/* 3d-view */
		setNearestAxis3d(t);
	}
	else {
		/* assume that this means a 2D-Editor */
		setNearestAxis2d(t);
	}

	getConstraintMatrix(t);
}

/*-------------- HELPER FUNCTIONS ----------------*/

char constraintModeToChar(TransInfo *t)
{
	if ((t->con.mode & CON_APPLY) == 0) {
		return '\0';
	}
	switch (t->con.mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2)) {
		case (CON_AXIS0):
		case (CON_AXIS1 | CON_AXIS2):
			return 'X';
		case (CON_AXIS1):
		case (CON_AXIS0 | CON_AXIS2):
			return 'Y';
		case (CON_AXIS2):
		case (CON_AXIS0 | CON_AXIS1):
			return 'Z';
		default:
			return '\0';
	}
}


bool isLockConstraint(TransInfo *t)
{
	int mode = t->con.mode;

	if ((mode & (CON_AXIS0 | CON_AXIS1)) == (CON_AXIS0 | CON_AXIS1))
		return true;

	if ((mode & (CON_AXIS1 | CON_AXIS2)) == (CON_AXIS1 | CON_AXIS2))
		return true;

	if ((mode & (CON_AXIS0 | CON_AXIS2)) == (CON_AXIS0 | CON_AXIS2))
		return true;

	return false;
}

/*
 * Returns the dimension of the constraint space.
 *
 * For that reason, the flags always needs to be set to properly evaluate here,
 * even if they aren't actually used in the callback function. (Which could happen
 * for weird constraints not yet designed. Along a path for example.)
 */

int getConstraintSpaceDimension(TransInfo *t)
{
	int n = 0;

	if (t->con.mode & CON_AXIS0)
		n++;

	if (t->con.mode & CON_AXIS1)
		n++;

	if (t->con.mode & CON_AXIS2)
		n++;

	return n;
/*
 * Someone willing to do it cryptically could do the following instead:
 *
 * return t->con & (CON_AXIS0|CON_AXIS1|CON_AXIS2);
 *
 * Based on the assumptions that the axis flags are one after the other and start at 1
 */
}

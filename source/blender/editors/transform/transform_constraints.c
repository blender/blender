/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "ED_image.h"
#include "ED_view3d.h"

#include "BLI_arithb.h"

//#include "blendef.h"
//
//#include "mydevice.h"

#include "WM_types.h"
#include "UI_resources.h"


#include "transform.h"

static void drawObjectConstraint(TransInfo *t);

/* ************************** CONSTRAINTS ************************* */
void constraintAutoValues(TransInfo *t, float vec[3])
{
	int mode = t->con.mode;
	if (mode & CON_APPLY)
	{
		float nval = (t->flag & T_NULL_ONE)?1.0f:0.0f;

		if ((mode & CON_AXIS0) == 0)
		{
			vec[0] = nval;
		}
		if ((mode & CON_AXIS1) == 0)
		{
			vec[1] = nval;
		}
		if ((mode & CON_AXIS2) == 0)
		{
			vec[2] = nval;
		}
	}
}

void constraintNumInput(TransInfo *t, float vec[3])
{
	int mode = t->con.mode;
	if (mode & CON_APPLY) {
		float nval = (t->flag & T_NULL_ONE)?1.0f:0.0f;

		if (getConstraintSpaceDimension(t) == 2) {
			int axis = mode & (CON_AXIS0|CON_AXIS1|CON_AXIS2);
			if (axis == (CON_AXIS0|CON_AXIS1)) {
				vec[0] = vec[0];
				vec[1] = vec[1];
				vec[2] = nval;
			}
			else if (axis == (CON_AXIS1|CON_AXIS2)) {
				vec[2] = vec[1];
				vec[1] = vec[0];
				vec[0] = nval;
			}
			else if (axis == (CON_AXIS0|CON_AXIS2)) {
				vec[0] = vec[0];
				vec[2] = vec[1];
				vec[1] = nval;
			}
		}
		else if (getConstraintSpaceDimension(t) == 1) {
			if (mode & CON_AXIS0) {
				vec[0] = vec[0];
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

static void postConstraintChecks(TransInfo *t, float vec[3], float pvec[3]) {
	int i = 0;

	Mat3MulVecfl(t->con.imtx, vec);

	snapGrid(t, vec);

	if (t->num.flag & T_NULL_ONE) {
		if (!(t->con.mode & CON_AXIS0))
			vec[0] = 1.0f;

		if (!(t->con.mode & CON_AXIS1))
			vec[1] = 1.0f;

		if (!(t->con.mode & CON_AXIS2))
			vec[2] = 1.0f;
	}

	if (hasNumInput(&t->num)) {
		applyNumInput(&t->num, vec);
		constraintNumInput(t, vec);
	}

	/* autovalues is operator param, use that directly but not if snapping is forced */
	if (t->flag & T_AUTOVALUES && (t->tsnap.status & SNAP_FORCED) == 0)
	{
		VECCOPY(vec, t->auto_values);
		constraintAutoValues(t, vec);
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

	Mat3MulVecfl(t->con.mtx, vec);
}

static void axisProjection(TransInfo *t, float axis[3], float in[3], float out[3]) {
	float norm[3], vec[3], factor;

	if(in[0]==0.0f && in[1]==0.0f && in[2]==0.0f)
		return;

	/* For when view is parallel to constraint... will cause NaNs otherwise
	   So we take vertical motion in 3D space and apply it to the
	   constraint axis. Nice for camera grab + MMB */
	if(1.0f - fabs(Inpf(axis, t->viewinv[2])) < 0.000001f) {
		Projf(vec, in, t->viewinv[1]);
		factor = Inpf(t->viewinv[1], vec) * 2.0f;
		/* since camera distance is quite relative, use quadratic relationship. holding shift can compensate */
		if(factor<0.0f) factor*= -factor;
		else factor*= factor;

		VECCOPY(out, axis);
		Normalize(out);
		VecMulf(out, -factor);	/* -factor makes move down going backwards */
	}
	else {
		float cb[3], ab[3];

		VECCOPY(out, axis);

		/* Get view vector on axis to define a plane */
		VecAddf(vec, t->con.center, in);
		getViewVector(t, vec, norm);

		Crossf(vec, norm, axis);

		/* Project input vector on the plane passing on axis */
		Projf(vec, in, vec);
		VecSubf(vec, in, vec);

		/* intersect the two lines: axis and norm */
		Crossf(cb, vec, norm);
		Crossf(ab, axis, norm);

		VecMulf(out, Inpf(cb, ab) / Inpf(ab, ab));
	}
}

static void planeProjection(TransInfo *t, float in[3], float out[3]) {
	float vec[3], factor, norm[3];

	VecAddf(vec, in, t->con.center);
	getViewVector(t, vec, norm);

	VecSubf(vec, out, in);

	factor = Inpf(vec, norm);
	if (fabs(factor) <= 0.001) {
		return; /* prevent divide by zero */
	}
	factor = Inpf(vec, vec) / factor;

	VECCOPY(vec, norm);
	VecMulf(vec, factor);

	VecAddf(out, in, vec);
}

/*
 * Generic callback for constant spacial constraints applied to linear motion
 *
 * The IN vector in projected into the constrained space and then further
 * projected along the view vector.
 * (in perspective mode, the view vector is relative to the position on screen)
 *
 */

static void applyAxisConstraintVec(TransInfo *t, TransData *td, float in[3], float out[3], float pvec[3])
{
	VECCOPY(out, in);
	if (!td && t->con.mode & CON_APPLY) {
		Mat3MulVecfl(t->con.pmtx, out);

		// With snap, a projection is alright, no need to correct for view alignment
		if ((t->tsnap.status & SNAP_ON) == 0) {
			if (getConstraintSpaceDimension(t) == 2) {
				if (out[0] != 0.0f || out[1] != 0.0f || out[2] != 0.0f) {
					planeProjection(t, in, out);
				}
			}
			else if (getConstraintSpaceDimension(t) == 1) {
				float c[3];

				if (t->con.mode & CON_AXIS0) {
					VECCOPY(c, t->con.mtx[0]);
				}
				else if (t->con.mode & CON_AXIS1) {
					VECCOPY(c, t->con.mtx[1]);
				}
				else if (t->con.mode & CON_AXIS2) {
					VECCOPY(c, t->con.mtx[2]);
				}
				axisProjection(t, c, in, out);
			}
		}
		postConstraintChecks(t, out, pvec);
	}
}

/*
 * Generic callback for object based spacial constraints applied to linear motion
 *
 * At first, the following is applied to the first data in the array
 * The IN vector in projected into the constrained space and then further
 * projected along the view vector.
 * (in perspective mode, the view vector is relative to the position on screen)
 *
 * Further down, that vector is mapped to each data's space.
 */

static void applyObjectConstraintVec(TransInfo *t, TransData *td, float in[3], float out[3], float pvec[3])
{
	VECCOPY(out, in);
	if (t->con.mode & CON_APPLY) {
		if (!td) {
			Mat3MulVecfl(t->con.pmtx, out);
			if (getConstraintSpaceDimension(t) == 2) {
				if (out[0] != 0.0f || out[1] != 0.0f || out[2] != 0.0f) {
					planeProjection(t, in, out);
				}
			}
			else if (getConstraintSpaceDimension(t) == 1) {
				float c[3];

				if (t->con.mode & CON_AXIS0) {
					VECCOPY(c, t->con.mtx[0]);
				}
				else if (t->con.mode & CON_AXIS1) {
					VECCOPY(c, t->con.mtx[1]);
				}
				else if (t->con.mode & CON_AXIS2) {
					VECCOPY(c, t->con.mtx[2]);
				}
				axisProjection(t, c, in, out);
			}
			postConstraintChecks(t, out, pvec);
			VECCOPY(out, pvec);
		}
		else {
			int i=0;

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
			Mat3MulVecfl(td->axismtx, out);
		}
	}
}

/*
 * Generic callback for constant spacial constraints applied to resize motion
 *
 *
 */

static void applyAxisConstraintSize(TransInfo *t, TransData *td, float smat[3][3])
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

		Mat3MulMat3(tmat, smat, t->con.imtx);
		Mat3MulMat3(smat, t->con.mtx, tmat);
	}
}

/*
 * Callback for object based spacial constraints applied to resize motion
 *
 *
 */

static void applyObjectConstraintSize(TransInfo *t, TransData *td, float smat[3][3])
{
	if (td && t->con.mode & CON_APPLY) {
		float tmat[3][3];
		float imat[3][3];

		Mat3Inv(imat, td->axismtx);

		if (!(t->con.mode & CON_AXIS0)) {
			smat[0][0] = 1.0f;
		}
		if (!(t->con.mode & CON_AXIS1)) {
			smat[1][1] = 1.0f;
		}
		if (!(t->con.mode & CON_AXIS2)) {
			smat[2][2] = 1.0f;
		}

		Mat3MulMat3(tmat, smat, imat);
		Mat3MulMat3(smat, td->axismtx, tmat);
	}
}

/*
 * Generic callback for constant spacial constraints applied to rotations
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

static void applyAxisConstraintRot(TransInfo *t, TransData *td, float vec[3], float *angle)
{
	if (!td && t->con.mode & CON_APPLY) {
		int mode = t->con.mode & (CON_AXIS0|CON_AXIS1|CON_AXIS2);

		switch(mode) {
		case CON_AXIS0:
		case (CON_AXIS1|CON_AXIS2):
			VECCOPY(vec, t->con.mtx[0]);
			break;
		case CON_AXIS1:
		case (CON_AXIS0|CON_AXIS2):
			VECCOPY(vec, t->con.mtx[1]);
			break;
		case CON_AXIS2:
		case (CON_AXIS0|CON_AXIS1):
			VECCOPY(vec, t->con.mtx[2]);
			break;
		}
		/* don't flip axis if asked to or if num input */
		if (angle && (mode & CON_NOFLIP) == 0 && hasNumInput(&t->num) == 0) {
			if (Inpf(vec, t->viewinv[2]) > 0.0f) {
				*angle = -(*angle);
			}
		}
	}
}

/*
 * Callback for object based spacial constraints applied to rotations
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

static void applyObjectConstraintRot(TransInfo *t, TransData *td, float vec[3], float *angle)
{
	if (t->con.mode & CON_APPLY) {
		int mode = t->con.mode & (CON_AXIS0|CON_AXIS1|CON_AXIS2);

		/* on setup call, use first object */
		if (td == NULL) {
			td= t->data;
		}

		switch(mode) {
		case CON_AXIS0:
		case (CON_AXIS1|CON_AXIS2):
			VECCOPY(vec, td->axismtx[0]);
			break;
		case CON_AXIS1:
		case (CON_AXIS0|CON_AXIS2):
			VECCOPY(vec, td->axismtx[1]);
			break;
		case CON_AXIS2:
		case (CON_AXIS0|CON_AXIS1):
			VECCOPY(vec, td->axismtx[2]);
			break;
		}
		if (angle && (mode & CON_NOFLIP) == 0 && hasNumInput(&t->num) == 0) {
			if (Inpf(vec, t->viewinv[2]) > 0.0f) {
				*angle = -(*angle);
			}
		}
	}
}

/*--------------------- INTERNAL SETUP CALLS ------------------*/

void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[]) {
	strncpy(t->con.text + 1, text, 48);
	Mat3CpyMat3(t->con.mtx, space);
	t->con.mode = mode;
	getConstraintMatrix(t);

	startConstraint(t);

	t->con.drawExtra = NULL;
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = 1;
}

void setLocalConstraint(TransInfo *t, int mode, const char text[]) {
	if (t->flag & T_EDIT) {
		float obmat[3][3];
		Mat3CpyMat4(obmat, t->scene->obedit->obmat);
		setConstraint(t, obmat, mode, text);
	}
	else {
		if (t->total == 1) {
			setConstraint(t, t->data->axismtx, mode, text);
		}
		else {
			strncpy(t->con.text + 1, text, 48);
			Mat3CpyMat3(t->con.mtx, t->data->axismtx);
			t->con.mode = mode;
			getConstraintMatrix(t);

			startConstraint(t);

			t->con.drawExtra = drawObjectConstraint;
			t->con.applyVec = applyObjectConstraintVec;
			t->con.applySize = applyObjectConstraintSize;
			t->con.applyRot = applyObjectConstraintRot;
			t->redraw = 1;
		}
	}
}

/*
	Set the constraint according to the user defined orientation

	ftext is a format string passed to sprintf. It will add the name of
	the orientation where %s is (logically).
*/
void setUserConstraint(TransInfo *t, short orientation, int mode, const char ftext[]) {
	char text[40];

	switch(orientation) {
	case V3D_MANIP_GLOBAL:
		{
			float mtx[3][3];
			sprintf(text, ftext, "global");
			Mat3One(mtx);
			setConstraint(t, mtx, mode, text);
		}
		break;
	case V3D_MANIP_LOCAL:
		sprintf(text, ftext, "local");
		setLocalConstraint(t, mode, text);
		break;
	case V3D_MANIP_NORMAL:
		sprintf(text, ftext, "normal");
		setConstraint(t, t->spacemtx, mode, text);
		break;
	case V3D_MANIP_VIEW:
		sprintf(text, ftext, "view");
		setConstraint(t, t->spacemtx, mode, text);
		break;
	default: /* V3D_MANIP_CUSTOM */
		sprintf(text, ftext, t->spacename);
		setConstraint(t, t->spacemtx, mode, text);
		break;
	}

	t->con.mode |= CON_USER;
}

/*----------------- DRAWING CONSTRAINTS -------------------*/

void drawConstraint(const struct bContext *C, TransInfo *t)
{
	TransCon *tc = &(t->con);

	if (!ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE))
		return;
	if (!(tc->mode & CON_APPLY))
		return;
	if (t->flag & T_USES_MANIPULATOR)
		return;
	if (t->flag & T_NO_CONSTRAINT)
		return;

	/* nasty exception for Z constraint in camera view */
	// TRANSFORM_FIX_ME
//	if((t->flag & T_OBJECT) && G.vd->camera==OBACT && G.vd->persp==V3D_CAMOB)
//		return;

	if (tc->drawExtra) {
		tc->drawExtra(t);
	}
	else {
		if (tc->mode & CON_SELECT) {
			float vec[3];
			char col2[3] = {255,255,255};
			convertViewVec(t, vec, (short)(t->mval[0] - t->con.imval[0]), (short)(t->mval[1] - t->con.imval[1]));
			VecAddf(vec, vec, tc->center);

			drawLine(t, tc->center, tc->mtx[0], 'x', 0);
			drawLine(t, tc->center, tc->mtx[1], 'y', 0);
			drawLine(t, tc->center, tc->mtx[2], 'z', 0);

			glColor3ubv((GLubyte *)col2);

			glDisable(GL_DEPTH_TEST);
			setlinestyle(1);
			glBegin(GL_LINE_STRIP);
				glVertex3fv(tc->center);
				glVertex3fv(vec);
			glEnd();
			setlinestyle(0);
			// TRANSFORM_FIX_ME
			//if(G.vd->zbuf)
				glEnable(GL_DEPTH_TEST);
		}

		if (tc->mode & CON_AXIS0) {
			drawLine(t, tc->center, tc->mtx[0], 'x', DRAWLIGHT);
		}
		if (tc->mode & CON_AXIS1) {
			drawLine(t, tc->center, tc->mtx[1], 'y', DRAWLIGHT);
		}
		if (tc->mode & CON_AXIS2) {
			drawLine(t, tc->center, tc->mtx[2], 'z', DRAWLIGHT);
		}
	}
}

/* called from drawview.c, as an extra per-window draw option */
void drawPropCircle(const struct bContext *C, TransInfo *t)
{
	if (t->flag & T_PROP_EDIT) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		float tmat[4][4], imat[4][4];

		UI_ThemeColor(TH_GRID);

		if(t->spacetype == SPACE_VIEW3D && rv3d != NULL)
		{
			Mat4CpyMat4(tmat, rv3d->viewmat);
			Mat4Invert(imat, tmat);
		}
		else
		{
			Mat4One(tmat);
			Mat4One(imat);
		}

		glPushMatrix();

		if((t->spacetype == SPACE_VIEW3D) && t->obedit)
		{
			glMultMatrixf(t->obedit->obmat); /* because t->center is in local space */
		}
		else if(t->spacetype == SPACE_IMAGE)
		{
			float aspx, aspy;

			ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
			glScalef(1.0f/aspx, 1.0f/aspy, 1.0);
		}

		set_inverted_drawing(1);
		drawcircball(GL_LINE_LOOP, t->center, t->prop_size, imat);
		set_inverted_drawing(0);

		glPopMatrix();
	}
}

static void drawObjectConstraint(TransInfo *t) {
	int i;
	TransData * td = t->data;

	/* Draw the first one lighter because that's the one who controls the others.
	   Meaning the transformation is projected on that one and just copied on the others
	   constraint space.
	   In a nutshell, the object with light axis is controlled by the user and the others follow.
	   Without drawing the first light, users have little clue what they are doing.
	 */
	if (t->con.mode & CON_AXIS0) {
		drawLine(t, td->ob->obmat[3], td->axismtx[0], 'x', DRAWLIGHT);
	}
	if (t->con.mode & CON_AXIS1) {
		drawLine(t, td->ob->obmat[3], td->axismtx[1], 'y', DRAWLIGHT);
	}
	if (t->con.mode & CON_AXIS2) {
		drawLine(t, td->ob->obmat[3], td->axismtx[2], 'z', DRAWLIGHT);
	}

	td++;

	for(i=1;i<t->total;i++,td++) {
		if (t->con.mode & CON_AXIS0) {
			drawLine(t, td->ob->obmat[3], td->axismtx[0], 'x', 0);
		}
		if (t->con.mode & CON_AXIS1) {
			drawLine(t, td->ob->obmat[3], td->axismtx[1], 'y', 0);
		}
		if (t->con.mode & CON_AXIS2) {
			drawLine(t, td->ob->obmat[3], td->axismtx[2], 'z', 0);
		}
	}
}

/*--------------------- START / STOP CONSTRAINTS ---------------------- */

void startConstraint(TransInfo *t) {
	t->con.mode |= CON_APPLY;
	*t->con.text = ' ';
	t->num.idx_max = MIN2(getConstraintSpaceDimension(t) - 1, t->idx_max);
}

void stopConstraint(TransInfo *t) {
	t->con.mode &= ~(CON_APPLY|CON_SELECT);
	*t->con.text = '\0';
	t->num.idx_max = t->idx_max;
}

void getConstraintMatrix(TransInfo *t)
{
	float mat[3][3];
	Mat3Inv(t->con.imtx, t->con.mtx);
	Mat3One(t->con.pmtx);

	if (!(t->con.mode & CON_AXIS0)) {
		t->con.pmtx[0][0]		=
			t->con.pmtx[0][1]	=
			t->con.pmtx[0][2]	= 0.0f;
	}

	if (!(t->con.mode & CON_AXIS1)) {
		t->con.pmtx[1][0]		=
			t->con.pmtx[1][1]	=
			t->con.pmtx[1][2]	= 0.0f;
	}

	if (!(t->con.mode & CON_AXIS2)) {
		t->con.pmtx[2][0]		=
			t->con.pmtx[2][1]	=
			t->con.pmtx[2][2]	= 0.0f;
	}

	Mat3MulMat3(mat, t->con.pmtx, t->con.imtx);
	Mat3MulMat3(t->con.pmtx, t->con.mtx, mat);
}

/*------------------------- MMB Select -------------------------------*/

void initSelectConstraint(TransInfo *t, float mtx[3][3])
{
	Mat3CpyMat3(t->con.mtx, mtx);
	t->con.mode |= CON_APPLY;
	t->con.mode |= CON_SELECT;

	setNearestAxis(t);
	t->con.drawExtra = NULL;
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
}

void selectConstraint(TransInfo *t) {
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
	t->redraw = 1;
}

static void setNearestAxis2d(TransInfo *t)
{
	/* no correction needed... just use whichever one is lower */
	if ( abs(t->mval[0]-t->con.imval[0]) < abs(t->mval[1]-t->con.imval[1]) ) {
		t->con.mode |= CON_AXIS1;
		sprintf(t->con.text, " along Y axis");
	}
	else {
		t->con.mode |= CON_AXIS0;
		sprintf(t->con.text, " along X axis");
	}
}

static void setNearestAxis3d(TransInfo *t)
{
	float zfac;
	float mvec[3], axis[3], proj[3];
	float len[3];
	int i, icoord[2];

	/* calculate mouse movement */
	mvec[0] = (float)(t->mval[0] - t->con.imval[0]);
	mvec[1] = (float)(t->mval[1] - t->con.imval[1]);
	mvec[2] = 0.0f;

	/* we need to correct axis length for the current zoomlevel of view,
	   this to prevent projected values to be clipped behind the camera
	   and to overflow the short integers.
	   The formula used is a bit stupid, just a simplification of the substraction
	   of two 2D points 30 pixels apart (that's the last factor in the formula) after
	   projecting them with window_to_3d_delta and then get the length of that vector.
	*/
	zfac= t->persmat[0][3]*t->center[0]+ t->persmat[1][3]*t->center[1]+ t->persmat[2][3]*t->center[2]+ t->persmat[3][3];
	zfac = VecLength(t->persinv[0]) * 2.0f/t->ar->winx * zfac * 30.0f;

	for (i = 0; i<3; i++) {
		VECCOPY(axis, t->con.mtx[i]);

		VecMulf(axis, zfac);
		/* now we can project to get window coordinate */
		VecAddf(axis, axis, t->con.center);
		projectIntView(t, axis, icoord);

		axis[0] = (float)(icoord[0] - t->center2d[0]);
		axis[1] = (float)(icoord[1] - t->center2d[1]);
		axis[2] = 0.0f;

 		if (Normalize(axis) != 0.0f) {
			Projf(proj, mvec, axis);
			VecSubf(axis, mvec, proj);
			len[i] = Normalize(axis);
		}
		else {
			len[i] = 10000000000.0f;
		}
	}

	if (len[0] <= len[1] && len[0] <= len[2]) {
		if (t->modifiers & MOD_CONSTRAINT_PLANE) {
			t->con.mode |= (CON_AXIS1|CON_AXIS2);
			sprintf(t->con.text, " locking %s X axis", t->spacename);
		}
		else {
			t->con.mode |= CON_AXIS0;
			sprintf(t->con.text, " along %s X axis", t->spacename);
		}
	}
	else if (len[1] <= len[0] && len[1] <= len[2]) {
		if (t->modifiers & MOD_CONSTRAINT_PLANE) {
			t->con.mode |= (CON_AXIS0|CON_AXIS2);
			sprintf(t->con.text, " locking %s Y axis", t->spacename);
		}
		else {
			t->con.mode |= CON_AXIS1;
			sprintf(t->con.text, " along %s Y axis", t->spacename);
		}
	}
	else if (len[2] <= len[1] && len[2] <= len[0]) {
		if (t->modifiers & MOD_CONSTRAINT_PLANE) {
			t->con.mode |= (CON_AXIS0|CON_AXIS1);
			sprintf(t->con.text, " locking %s Z axis", t->spacename);
		}
		else {
			t->con.mode |= CON_AXIS2;
			sprintf(t->con.text, " along %s Z axis", t->spacename);
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

char constraintModeToChar(TransInfo *t) {
	if ((t->con.mode & CON_APPLY)==0) {
		return '\0';
	}
	switch (t->con.mode & (CON_AXIS0|CON_AXIS1|CON_AXIS2)) {
	case (CON_AXIS0):
	case (CON_AXIS1|CON_AXIS2):
		return 'X';
	case (CON_AXIS1):
	case (CON_AXIS0|CON_AXIS2):
		return 'Y';
	case (CON_AXIS2):
	case (CON_AXIS0|CON_AXIS1):
		return 'Z';
	default:
		return '\0';
	}
}


int isLockConstraint(TransInfo *t) {
	int mode = t->con.mode;

	if ( (mode & (CON_AXIS0|CON_AXIS1)) == (CON_AXIS0|CON_AXIS1))
		return 1;

	if ( (mode & (CON_AXIS1|CON_AXIS2)) == (CON_AXIS1|CON_AXIS2))
		return 1;

	if ( (mode & (CON_AXIS0|CON_AXIS2)) == (CON_AXIS0|CON_AXIS2))
		return 1;

	return 0;
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
  Someone willing to do it criptically could do the following instead:

  return t->con & (CON_AXIS0|CON_AXIS1|CON_AXIS2);

  Based on the assumptions that the axis flags are one after the other and start at 1
*/
}

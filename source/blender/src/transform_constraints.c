/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
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
#include "DNA_ika_types.h"
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
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_userdef_types.h"
#include "DNA_property_types.h"
#include "DNA_vfont_types.h"
#include "DNA_constraint_types.h"

#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editview.h"
#include "BIF_resources.h"
#include "BIF_mywindow.h"
#include "BIF_gl.h"
#include "BIF_editlattice.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_lattice.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_displist.h"

#include "BSE_view.h"
#include "BSE_edit.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BDR_drawobject.h"

#include "blendef.h"

#include "mydevice.h"

#include "transform.h"
#include "transform_constraints.h"
#include "transform_generics.h"

extern ListBase editNurb;
extern ListBase editelems;

void recalcData();

/* ************************** CONSTRAINTS ************************* */
void getConstraintMatrix(TransInfo *t);

void constraintNumInput(TransInfo *t, float vec[3])
{
	int mode = t->con.mode;
	float nval = (t->flag & T_NULL_ONE)?1.0f:0.0f;

	if (getConstraintSpaceDimension(t) == 2) {
		if (mode & (CON_AXIS0|CON_AXIS1)) {
			vec[2] = nval;
		}
		else if (mode & (CON_AXIS1|CON_AXIS2)) {
			vec[2] = vec[1];
			vec[1] = vec[0];
			vec[0] = nval;
		}
		else if (mode & (CON_AXIS0|CON_AXIS2)) {
			vec[2] = vec[1];
			vec[1] = nval;
		}
	}
	else if (getConstraintSpaceDimension(t) == 1) {
		if (mode & CON_AXIS0) {
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
	float norm[3], n[3], vec[3], factor;

	VecAddf(vec, in, t->con.center);
	getViewVector(vec, norm);

	Normalise(axis);

	VECCOPY(n, axis);
	Mat4MulVecfl(t->viewmat, n);
	n[2] = t->viewmat[3][2];
	Mat4MulVecfl(t->viewinv, n);

	/* For when view is parallel to constraint... will cause NaNs otherwise
	   So we take vertical motion in 3D space and apply it to the
	   constraint axis. Nice for camera grab + MMB */
	if(n[0]*n[0] + n[1]*n[1] + n[2]*n[2] < 0.000001f) {
		Projf(vec, in, t->viewinv[1]);
		factor = Inpf(t->viewinv[1], vec) * 2.0f;
		/* since camera distance is quite relative, use quadratic relationship. holding shift can compensate */
		if(factor<0.0f) factor*= -factor;
		else factor*= factor;
		
		VECCOPY(out, axis);
		Normalise(out);
		VecMulf(out, -factor);	/* -factor makes move down going backwards */
	}
	else {
		// prevent division by zero, happens on constrainting without initial delta transform */
		if(in[0]!=0.0f || in[1]!=0.0f || in[2]!=0.0) {
			Projf(vec, in, n);
			factor = Normalise(vec);
			// prevent NaN for 0.0/0.0
			if(factor!=0.0f)
				factor /= Inpf(axis, vec);

			VecMulf(axis, factor);
			VECCOPY(out, axis);
		}
	}
}

static void planeProjection(TransInfo *t, float in[3], float out[3]) {
	float vec[3], factor, angle, norm[3];

	VecAddf(vec, in, t->con.center);
	getViewVector(vec, norm);

	VecSubf(vec, out, in);
	factor = Normalise(vec);
	angle = Inpf(vec, norm);

	if (angle * angle >= 0.000001f) {
		factor /= angle;

		VECCOPY(vec, norm);
		VecMulf(vec, factor);

		VecAddf(out, in, vec);
	}
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

static void applyAxisConstraintRot(TransInfo *t, TransData *td, float vec[3])
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
		if (!(mode & CON_NOFLIP)) {
			if (Inpf(vec, G.vd->viewinv[2]) > 0.0f) {
				VecMulf(vec, -1.0f);
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

static void applyObjectConstraintRot(TransInfo *t, TransData *td, float vec[3])
{
	if (td && t->con.mode & CON_APPLY) {
		int mode = t->con.mode & (CON_AXIS0|CON_AXIS1|CON_AXIS2);

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
		if (!(mode & CON_NOFLIP)) {
			if (Inpf(vec, G.vd->viewinv[2]) > 0.0f) {
				VecMulf(vec, -1.0f);
			}
		}
	}
}

static void drawObjectConstraint(TransInfo *t) {
	int i;
	TransData * td = t->data;

	if (t->con.mode & CON_AXIS0) {
		drawLine(td->ob->obmat[3], td->axismtx[0], 'x', DRAWLIGHT);
	}
	if (t->con.mode & CON_AXIS1) {
		drawLine(td->ob->obmat[3], td->axismtx[1], 'y', DRAWLIGHT);
	}
	if (t->con.mode & CON_AXIS2) {
		drawLine(td->ob->obmat[3], td->axismtx[2], 'z', DRAWLIGHT);
	}

	td++;
	for(i=1;i<t->total;i++,td++) {
		if (t->con.mode & CON_AXIS0) {
			drawLine(td->ob->obmat[3], td->axismtx[0], 'x', 0);
		}
		if (t->con.mode & CON_AXIS1) {
			drawLine(td->ob->obmat[3], td->axismtx[1], 'y', 0);
		}
		if (t->con.mode & CON_AXIS2) {
			drawLine(td->ob->obmat[3], td->axismtx[2], 'z', 0);
		}
	}
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

void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[]) {
	strncpy(t->con.text + 1, text, 48);
	Mat3CpyMat3(t->con.mtx, space);
	t->con.mode = mode;
	getConstraintMatrix(t);

	startConstraint(t);

	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = 1;
}

void setLocalConstraint(TransInfo *t, int mode, const char text[]) {
	if (t->flag & T_EDIT) {
		float obmat[3][3];
		Mat3CpyMat4(obmat, G.obedit->obmat);
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

/* text is optional, for header print */
void BIF_setSingleAxisConstraint(float vec[3], char *text) {
	TransInfo *t = BIF_GetTransInfo();
	float space[3][3], v[3];
	
	VECCOPY(space[0], vec);

	v[0] = vec[2];
	v[1] = vec[0];
	v[2] = vec[1];

	Crossf(space[1], vec, v);
	Crossf(space[2], vec, space[1]);
	Mat3Ortho(space);

	Mat3Ortho(space);

	Mat3CpyMat3(t->con.mtx, space);
	t->con.mode = (CON_AXIS0|CON_APPLY);
	getConstraintMatrix(t);

	/* start copying with an offset of 1, to reserve a spot for the SPACE char */
	if(text) strncpy(t->con.text+1, text, 48);	// 50 in struct

	
	t->con.drawExtra = NULL;
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = 1;
}

void BIF_setDualAxisConstraint(float vec1[3], float vec2[3]) {
	TransInfo *t = BIF_GetTransInfo();
	float space[3][3];
	
	VECCOPY(space[0], vec1);
	VECCOPY(space[1], vec2);
	Crossf(space[2], space[0], space[1]);
	Mat3Ortho(space);
	
	Mat3CpyMat3(t->con.mtx, space);
	t->con.mode = (CON_AXIS0|CON_AXIS1|CON_APPLY);
	getConstraintMatrix(t);
	
	t->con.drawExtra = NULL;
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applySize = applyAxisConstraintSize;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = 1;
}


void BIF_drawConstraint(void)
{
	TransInfo *t = BIF_GetTransInfo();
	TransCon *tc = &(t->con);

	if (!(tc->mode & CON_APPLY))
		return;
	if (t->flag & T_USES_MANIPULATOR)
		return;
	
	/* nasty exception for Z constraint in camera view */
	if( (t->flag & T_OBJECT) && G.vd->camera==OBACT && G.vd->persp>1) 
		return;

	if (tc->drawExtra) {
		tc->drawExtra(t);
	}
	else {
		if (tc->mode & CON_SELECT) {
			float vec[3];
			short mval[2];
			char col2[3] = {255,255,255};
			getmouseco_areawin(mval);
			window_to_3d(vec, (short)(mval[0] - t->con.imval[0]), (short)(mval[1] - t->con.imval[1]));
			VecAddf(vec, vec, tc->center);

//			drawLine(tc->center, tc->mtx[0], 'x', 0);
//			drawLine(tc->center, tc->mtx[1], 'y', 0);
//			drawLine(tc->center, tc->mtx[2], 'z', 0);

			draw_manipulator_ext(curarea, t->mode, 'c', 2, tc->center, tc->mtx);
			glColor3ubv(col2);
			
			glDisable(GL_DEPTH_TEST);
			setlinestyle(1);
			glBegin(GL_LINE_STRIP); 
				glVertex3fv(tc->center); 
				glVertex3fv(vec); 
			glEnd();
			setlinestyle(0);
			if(G.zbuf) glEnable(GL_DEPTH_TEST);	// warning for global!
		}

		if (tc->mode & CON_AXIS0) {
			draw_manipulator_ext(curarea, t->mode, 'x', 0, tc->center, tc->mtx);
			draw_manipulator_ext(curarea, t->mode, 'x', 2, tc->center, tc->mtx);
//			drawLine(tc->center, tc->mtx[0], 'x', DRAWLIGHT);
		}
		if (tc->mode & CON_AXIS1) {
			draw_manipulator_ext(curarea, t->mode, 'y', 0, tc->center, tc->mtx);
			draw_manipulator_ext(curarea, t->mode, 'y', 2, tc->center, tc->mtx);
//			drawLine(tc->center, tc->mtx[1], 'y', DRAWLIGHT);
		}
		if (tc->mode & CON_AXIS2) {
			draw_manipulator_ext(curarea, t->mode, 'z', 0, tc->center, tc->mtx);
			draw_manipulator_ext(curarea, t->mode, 'z', 2, tc->center, tc->mtx);
//			drawLine(tc->center, tc->mtx[2], 'z', DRAWLIGHT);
		}
	}
}

/* called from drawview.c, as an extra per-window draw option */
void BIF_drawPropCircle()
{
	TransInfo *t = BIF_GetTransInfo();

	if (G.f & G_PROPORTIONAL) {
		float tmat[4][4], imat[4][4];

		BIF_ThemeColor(TH_GRID);
		
		/* if editmode we need to go into object space */
		if(G.obedit) mymultmatrix(G.obedit->obmat);
		
		mygetmatrix(tmat);
		Mat4Invert(imat, tmat);
		
 		drawcircball(t->center, t->propsize, imat);
		
		/* if editmode we restore */
		if(G.obedit) myloadmatrix(G.vd->viewmat);
	}
}

void initConstraint(TransInfo *t) {
	if (t->con.mode & CON_APPLY) {
		startConstraint(t);
	}
}

void startConstraint(TransInfo *t) {
	t->con.mode |= CON_APPLY;
	*t->con.text = ' ';
	t->num.idx_max = MIN2(getConstraintSpaceDimension(t) - 1, t->idx_max);
}

void stopConstraint(TransInfo *t) {
	t->con.mode &= ~CON_APPLY;
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

void initSelectConstraint(TransInfo *t)
{
	Mat3One(t->con.mtx);
	Mat3One(t->con.pmtx);
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

void setNearestAxis(TransInfo *t)
{
	short coord[2];
	float mvec[3], axis[3], proj[3];
	float len[3];
	int i;

	t->con.mode &= ~CON_AXIS0;
	t->con.mode &= ~CON_AXIS1;
	t->con.mode &= ~CON_AXIS2;

	getmouseco_areawin(coord);
	mvec[0] = (float)(coord[0] - t->con.imval[0]);
	mvec[1] = (float)(coord[1] - t->con.imval[1]);
	mvec[2] = 0.0f;

	for (i = 0; i<3; i++) {
		VECCOPY(axis, t->con.mtx[i]);
		VecAddf(axis, axis, t->con.center);
		project_short_noclip(axis, coord);
		axis[0] = (float)(coord[0] - t->center2d[0]);
		axis[1] = (float)(coord[1] - t->center2d[1]);
		axis[2] = 0.0f;

		if (Normalise(axis) != 0.0f) {
			Projf(proj, mvec, axis);
			VecSubf(axis, mvec, proj);
			len[i] = Normalise(axis);
		}
		else {
			len[i] = 10000000000.0f;
		}
	}

	if (len[0] <= len[1] && len[0] <= len[2]) {
		if (G.qual & LR_SHIFTKEY) {
			t->con.mode |= (CON_AXIS1|CON_AXIS2);
			strcpy(t->con.text, " locking global X");
		}
		else {
			t->con.mode |= CON_AXIS0;
			strcpy(t->con.text, " along global X");
		}
	}
	else if (len[1] <= len[0] && len[1] <= len[2]) {
		if (G.qual & LR_SHIFTKEY) {
			t->con.mode |= (CON_AXIS0|CON_AXIS2);
			strcpy(t->con.text, " locking global Y");
		}
		else {
			t->con.mode |= CON_AXIS1;
			strcpy(t->con.text, " along global Y");
		}
	}
	else if (len[2] <= len[1] && len[2] <= len[0]) {
		if (G.qual & LR_SHIFTKEY) {
			t->con.mode |= (CON_AXIS0|CON_AXIS1);
			strcpy(t->con.text, " locking global Z");
		}
		else {
			t->con.mode |= CON_AXIS2;
			strcpy(t->con.text, " along global Z");
		}
	}
	getConstraintMatrix(t);
}

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
#include "BLI_winstuff.h"
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

extern TransInfo trans;

/* ************************** CONSTRAINTS ************************* */
void getConstraintMatrix(TransInfo *t);

void applyAxisConstraintVec(TransInfo *t, TransData *td, float in[3], float out[3])
{
	VECCOPY(out, in);
	if (!td && t->con.mode & APPLYCON) {
		Mat3MulVecfl(t->con.imtx, out);
		if (!(out[0] == out[1] == out[2] == 0.0f)) {
			if (getConstraintSpaceDimension(t) == 2) {
				float vec[3], factor, angle;

				VecSubf(vec, out, in);
				factor = Normalise(vec);
				angle = Inpf(vec, G.vd->viewinv[2]);

				if (angle * angle >= 0.000001f) {
					factor /= angle;

					VECCOPY(vec, G.vd->viewinv[2]);
					VecMulf(vec, factor);

					VecAddf(out, in, vec);
				}
			}
			else if (getConstraintSpaceDimension(t) == 1) {
				float c[3], n[3], vec[3], factor;

				if (t->con.mode & CONAXIS0) {
					VECCOPY(c, t->con.mtx[0]);
				}
				else if (t->con.mode & CONAXIS1) {
					VECCOPY(c, t->con.mtx[1]);
				}
				else if (t->con.mode & CONAXIS2) {
					VECCOPY(c, t->con.mtx[2]);
				}
				Normalise(c);

				VECCOPY(n, c);
				Mat4MulVecfl(G.vd->viewmat, n);
				n[2] = G.vd->viewmat[3][2];
				Mat4MulVecfl(G.vd->viewinv, n);

				if (Inpf(c, G.vd->viewinv[2]) != 1.0f) {
					Projf(vec, in, n);
					factor = Normalise(vec);
					factor /= Inpf(c, vec);

					VecMulf(c, factor);
					VECCOPY(out, c);

				}
				else {
					out[0] = out[1] = out[2] = 0.0f;
				}

			}
		}

		if (t->num.flags & NULLONE && !(t->con.mode & CONAXIS0))
			out[0] = 1.0f;

		if (t->num.flags & NULLONE && !(t->con.mode & CONAXIS1))
			out[1] = 1.0f;

		if (t->num.flags & NULLONE && !(t->con.mode & CONAXIS2))
			out[2] = 1.0f;
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
 * The vector is then modified to always point away from the screen (in global space)
 * This insures that the rotation is always logically following the mouse.
 * (ie: not doing counterclockwise rotations when the mouse moves clockwise).
 */
void applyAxisConstraintRot(TransInfo *t, TransData *td, float vec[3])
{
	if (!td && t->con.mode & APPLYCON) {
		int mode = t->con.mode & (CONAXIS0|CONAXIS1|CONAXIS2);

		switch(mode) {
		case CONAXIS0:
		case (CONAXIS1|CONAXIS2):
			VECCOPY(vec, t->con.mtx[0]);
			break;
		case CONAXIS1:
		case (CONAXIS0|CONAXIS2):
			VECCOPY(vec, t->con.mtx[1]);
			break;
		case CONAXIS2:
		case (CONAXIS0|CONAXIS1):
			VECCOPY(vec, t->con.mtx[2]);
			break;
		}
		if (Inpf(vec, G.vd->viewinv[2]) > 0.0f) {
			VecMulf(vec, -1.0f);
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

	if (t->con.mode & CONAXIS0)
		n++;

	if (t->con.mode & CONAXIS1)
		n++;

	if (t->con.mode & CONAXIS2)
		n++;

	return n;
}

void setConstraint(TransInfo *t, float space[3][3], int mode) {
	Mat3CpyMat3(t->con.mtx, space);
	t->con.mode = mode;
	getConstraintMatrix(t);

	VECCOPY(t->con.center, t->center);
	if (G.obedit) {
		Mat4MulVecfl(G.obedit->obmat, t->con.center);
	}

	t->con.applyVec = applyAxisConstraintVec;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = 1;
}

//void drawConstraint(TransCon *t) {
void drawConstraint() {
	int i = -1;
	TransCon *t = &(trans.con);

	if (t->mode == 0)
		return;

	if (!(t->mode & APPLYCON)) {
		i = nearestAxisIndex(&trans);
	}

	if (t->mode & CONAXIS0) {
		if (i == 0)
			drawLine(t->center, t->mtx[0], 255 - 'x');
		else
			drawLine(t->center, t->mtx[0], 'x');
	}
	if (t->mode & CONAXIS1) {
		if (i == 1)
			drawLine(t->center, t->mtx[1], 255 - 'y');
		else
			drawLine(t->center, t->mtx[1], 'y');
	}
	if (t->mode & CONAXIS2) {
		if (i == 2)
			drawLine(t->center, t->mtx[2], 255 - 'z');
		else
			drawLine(t->center, t->mtx[2], 'z');
	}

}

void drawPropCircle()
//void drawPropCircle(TransInfo *t)
{
	TransInfo *t = &trans;

	if (G.f & G_PROPORTIONAL) {
		float tmat[4][4], imat[4][4];

		BIF_ThemeColor(TH_GRID);

		mygetmatrix(tmat);
		Mat4Invert(imat, tmat);

 		drawcircball(t->center, t->propsize, imat);
	}
}

void getConstraintMatrix(TransInfo *t)
{
	Mat3Inv(t->con.imtx, t->con.mtx);

	if (!(t->con.mode & CONAXIS0)) {
		t->con.imtx[0][0]		=
			t->con.imtx[0][1]	=
			t->con.imtx[0][2]	= 0.0f;
	}

	if (!(t->con.mode & CONAXIS1)) {
		t->con.imtx[1][0]		=
			t->con.imtx[1][1]	=
			t->con.imtx[1][2]	= 0.0f;
	}

	if (!(t->con.mode & CONAXIS2)) {
		t->con.imtx[2][0]		=
			t->con.imtx[2][1]	=
			t->con.imtx[2][2]	= 0.0f;
	}
}

void selectConstraint(TransInfo *t)
{
	Mat3One(t->con.mtx);
	Mat3One(t->con.imtx);
	t->con.mode |= CONAXIS0;
	t->con.mode |= CONAXIS1;
	t->con.mode |= CONAXIS2;
	t->con.mode &= ~APPLYCON;
	VECCOPY(t->con.center, t->center);
	if (G.obedit) {
		Mat4MulVecfl(G.obedit->obmat, t->con.center);
	}
}

int nearestAxisIndex(TransInfo *t)
{
	short coord[2];
	float mvec[3], axis[3], center[3], proj[3];
	float len[3];
	int i;

	VECCOPY(center, t->center);
	if (G.obedit) {
		Mat4MulVecfl(G.obedit->obmat, center);
	}

	getmouseco_areawin(coord);
	mvec[0] = (float)(coord[0] - t->center2d[0]);
	mvec[1] = (float)(coord[1] - t->center2d[1]);
	mvec[2] = 0.0f;

	for (i = 0; i<3; i++) {
		VECCOPY(axis, t->con.mtx[i]);
		VecAddf(axis, axis, center);
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

	if (len[0] < len[1] && len[0] < len[2]) {
		return 0;
	}
	else if (len[1] < len[0] && len[1] < len[2]) {
		return 1;
	}
	else if (len[2] < len[1] && len[2] < len[0]) {
		return 2;
	}
	return -1;
}

void chooseConstraint(TransInfo *t)
{
	t->con.mode &= ~CONAXIS0;
	t->con.mode &= ~CONAXIS1;
	t->con.mode &= ~CONAXIS2;

	switch(nearestAxisIndex(t)) {
	case 0:
		t->con.mode |= CONAXIS0;
		break;
	case 1:
		t->con.mode |= CONAXIS1;
		break;
	case 2:
		t->con.mode |= CONAXIS2;
		break;
	}

	t->con.mode |= APPLYCON;
	VECCOPY(t->con.center, t->center);

	getConstraintMatrix(t);
	t->con.applyVec = applyAxisConstraintVec;
	t->con.applyRot = applyAxisConstraintRot;
	t->redraw = 1;
}

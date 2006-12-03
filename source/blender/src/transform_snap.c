/**
 * $Id: 
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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#include <stdlib.h>
#include <math.h>

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BLI_arithb.h"

#include "BDR_drawobject.h"

#include "BIF_editsima.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "transform.h"
#include "mydevice.h"		/* for KEY defines	*/

#define SNAP_ON	1

/********************* PROTOTYPES ***********************/

void setSnappingCallback(TransInfo *t);
void SnapTranslation(TransInfo *t, float vec[3]);
void SnapRotation(TransInfo *t, float *vec);

void CalcSnapGrid(TransInfo *t, float vec[3]);


/****************** IMPLEMENTATIONS *********************/

void drawSnapping(TransInfo *t)
{
	if (t->tsnap.status & SNAP_ON) {
		// Do something nice
	}
}

int  handleSnapping(TransInfo *t, int event)
{
	int status = 0;
	
	switch(event) {
	case ACCENTGRAVEKEY:
		t->tsnap.status ^= SNAP_ON;
		status = 1;
		break;
	}
	
	return status;
}

void applySnapping(TransInfo *t, float *vec)
{
	if ((t->tsnap.status & SNAP_ON) && t->tsnap.applySnap != NULL)
	{
		t->tsnap.applySnap(t, vec);
	}
}

void resetSnapping(TransInfo *t)
{
	t->tsnap.status = 0;
	t->tsnap.applySnap = NULL;
}

void initSnapping(TransInfo *t)
{
	setSnappingCallback(t);
	
	if ((t->spacetype==SPACE_VIEW3D) && (G.vd->flag2 & V3D_TRANSFORM_SNAP))
	{
		t->tsnap.status |= SNAP_ON;
	}
	else
	{
		t->tsnap.status &= ~SNAP_ON;
	}
}

void setSnappingCallback(TransInfo *t)
{
	switch (t->mode)
	{
	case TFM_TRANSLATION:
		t->tsnap.applySnap = SnapTranslation;
		t->tsnap.calcSnap = CalcSnapGrid;
		break;
	case TFM_ROTATION:
		t->tsnap.applySnap = NULL; //SnapRotation;
		//t->tsnap.calcSnap = CalcSnapGrid;
		break;
	case TFM_RESIZE:
		t->tsnap.applySnap = NULL;
		break;
	default:
		t->tsnap.applySnap = NULL;
		break;
	}
}

void SnapTranslation(TransInfo *t, float vec[3])
{
	float center[3];

	VECCOPY(center, t->center);	
	if(t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:t->poseobj;
		Mat4MulVecfl(ob->obmat, center);
	}
	
	VecAddf(t->tsnap.snapPoint, vec, center);

	t->tsnap.calcSnap(t, t->tsnap.snapPoint);
	
	VecSubf(vec, t->tsnap.snapPoint, center);
	
	if (t->con.mode & CON_APPLY)
	{
		Mat3MulVecfl(t->con.pmtx, vec);
	}
}

void SnapRotation(TransInfo *t, float *vec)
{
	TransData *td;
	
	td = t->data;
	
	VECCOPY(t->tsnap.snapPoint, td->loc);
	t->tsnap.calcSnap(t, t->tsnap.snapPoint);
	
	
}

void CalcSnapGrid(TransInfo *t, float vec[3])
{
	snapGridAction(t, vec, BIG_GEARS);
}

/*================================================================*/

static void applyGrid(TransInfo *t, float *val, int max_index, float fac[3], GearsType action);


void snapGridAction(TransInfo *t, float *val, GearsType action) {
	float fac[3];

	fac[NO_GEARS]    = t->snap[0];
	fac[BIG_GEARS]   = t->snap[1];
	fac[SMALL_GEARS] = t->snap[2];
	
	applyGrid(t, val, t->idx_max, fac, action);
}


void snapGrid(TransInfo *t, float *val) {
	int invert;
	GearsType action;

	if(t->mode==TFM_ROTATION || t->mode==TFM_WARP || t->mode==TFM_TILT || t->mode==TFM_TRACKBALL || t->mode==TFM_BONE_ROLL)
		invert = U.flag & USER_AUTOROTGRID;
	else if(t->mode==TFM_RESIZE || t->mode==TFM_SHEAR || t->mode==TFM_BONESIZE || t->mode==TFM_SHRINKFATTEN || t->mode==TFM_CURVE_SHRINKFATTEN)
		invert = U.flag & USER_AUTOSIZEGRID;
	else
		invert = U.flag & USER_AUTOGRABGRID;

	if(invert) {
		action = (G.qual & LR_CTRLKEY) ? NO_GEARS: BIG_GEARS;
	}
	else {
		action = (G.qual & LR_CTRLKEY) ? BIG_GEARS : NO_GEARS;
	}
	
	if (action == BIG_GEARS && (G.qual & LR_SHIFTKEY)) {
		action = SMALL_GEARS;
	}

	snapGridAction(t, val, action);
}


static void applyGrid(TransInfo *t, float *val, int max_index, float fac[3], GearsType action)
{
	int i;
	float asp[3] = {1.0f, 1.0f, 1.0f}; // TODO: Remove hard coded limit here (3)

	// Early bailing out if no need to snap
	if (fac[action] == 0.0)
		return;
	
	/* evil hack - snapping needs to be adapted for image aspect ratio */
	if((t->spacetype==SPACE_IMAGE) && (t->mode==TFM_TRANSLATION)) {
		transform_aspect_ratio_tface_uv(asp, asp+1);
	}

	for (i=0; i<=max_index; i++) {
		val[i]= fac[action]*asp[i]*(float)floor(val[i]/(fac[action]*asp[i]) +.5);
	}
}

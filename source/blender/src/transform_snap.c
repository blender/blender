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
#include <stdio.h>

#include "PIL_time.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BDR_drawobject.h"

#include "editmesh.h"
#include "BIF_editsima.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "transform.h"
#include "mydevice.h"		/* for KEY defines	*/
#include "blendef.h" /* for selection modes */

/********************* PROTOTYPES ***********************/

void setSnappingCallback(TransInfo *t);

void ApplySnapTranslation(TransInfo *t, float vec[3]);
void ApplySnapRotation(TransInfo *t, float *vec);

void CalcSnapGrid(TransInfo *t, float *vec);
void CalcSnapGeometry(TransInfo *t, float *vec);

void TargetSnapMedian(TransInfo *t);
void TargetSnapCenter(TransInfo *t);
void TargetSnapClosest(TransInfo *t);

/****************** IMPLEMENTATIONS *********************/

void drawSnapping(TransInfo *t)
{
	if ((t->tsnap.status & (SNAP_ON|POINT_INIT|TARGET_INIT)) == (SNAP_ON|POINT_INIT|TARGET_INIT)) {
		float unitmat[4][4];
		char col[4];
		
		BIF_GetThemeColor3ubv(TH_TRANSFORM, col);
		glColor4ub(col[0], col[1], col[2], 128);
		
		glPushMatrix();
		
		glTranslatef(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], t->tsnap.snapPoint[2]);
		
		/* sets view screen aligned */
		glRotatef( -360.0f*saacos(G.vd->viewquat[0])/(float)M_PI, G.vd->viewquat[1], G.vd->viewquat[2], G.vd->viewquat[3]);

		Mat4One(unitmat);
		drawcircball(GL_LINE_LOOP, unitmat[3], 0.1f, unitmat);
		glPopMatrix();
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
	if (	(t->tsnap.status & SNAP_ON) &&
			t->tsnap.applySnap != NULL &&
			(t->flag & T_PROP_EDIT) == 0)
	{
		double current = PIL_check_seconds_timer();
		
		// Time base quirky code to go around findnearest slowness
		if (current - t->tsnap.last  >= 0.25)
		{
			t->tsnap.calcSnap(t, vec);
			t->tsnap.targetSnap(t);
	
			t->tsnap.last = current;
		}
		if ((t->tsnap.status & (POINT_INIT|TARGET_INIT)) == (POINT_INIT|TARGET_INIT))
		{
			t->tsnap.applySnap(t, vec);
		}
	}
}

void resetSnapping(TransInfo *t)
{
	t->tsnap.status = 0;
	t->tsnap.last = 0;
	t->tsnap.applySnap = NULL;
}

void initSnapping(TransInfo *t)
{
	resetSnapping(t);
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
		t->tsnap.applySnap = ApplySnapTranslation;
		t->tsnap.calcSnap = CalcSnapGeometry;
		break;
	case TFM_ROTATION:
		t->tsnap.applySnap = NULL;
		break;
	case TFM_RESIZE:
		t->tsnap.applySnap = NULL;
		break;
	default:
		t->tsnap.applySnap = NULL;
		break;
	}
	
	switch(G.vd->flag2 & V3D_SNAP_TARGET)
	{
		case V3D_SNAP_TARGET_CLOSEST:
			t->tsnap.targetSnap = TargetSnapClosest;
			break;
		case V3D_SNAP_TARGET_CENTER:
			t->tsnap.targetSnap = TargetSnapCenter;
			break;
		case V3D_SNAP_TARGET_MEDIAN:
			t->tsnap.targetSnap = TargetSnapMedian;
			break;
	}

}

/********************** APPLY **************************/

void ApplySnapTranslation(TransInfo *t, float vec[3])
{
	VecSubf(vec, t->tsnap.snapPoint, t->tsnap.snapTarget);
/*	
	if (t->con.mode & CON_APPLY)
	{
		Mat3MulVecfl(t->con.pmtx, vec);
	}
*/
}

void ApplySnapRotation(TransInfo *t, float vec[3])
{
	// FOO
}

/********************** CALC **************************/

void CalcSnapGrid(TransInfo *t, float *vec)
{
	snapGridAction(t, t->tsnap.snapPoint, BIG_GEARS);
}

void CalcSnapGeometry(TransInfo *t, float *vec)
{
	if (G.obedit != NULL && G.obedit->type==OB_MESH)
	{
		/*if (G.scene->selectmode & B_SEL_VERT)*/
		{
			EditVert *nearest=NULL;
			int dist = 50; // Use a user defined value here
			
			// use findnearestverts in vert mode, others in other modes
			nearest = findnearestvert(&dist, 0);
			
			if (nearest != NULL)
			{
				VECCOPY(t->tsnap.snapPoint, nearest->co);
				
				Mat4MulVecfl(G.obedit->obmat, t->tsnap.snapPoint);
				
				t->tsnap.status |=  POINT_INIT;
			}
			else
			{
				t->tsnap.status &= ~POINT_INIT;
			}
		}
		/*
		if (G.scene->selectmode & B_SEL_EDGE)
		{
			EditEdge *nearest=NULL;
			int dist = 50; // Use a user defined value here
			
			// use findnearestverts in vert mode, others in other modes
			nearest = findnearestedge(&dist);
			
			if (nearest != NULL)
			{
				VecAddf(t->tsnap.snapPoint, nearest->v1->co, nearest->v2->co);
				
				VecMulf(t->tsnap.snapPoint, 0.5f); 
				
				Mat4MulVecfl(G.obedit->obmat, t->tsnap.snapPoint);
				
				t->tsnap.status |=  POINT_INIT;
			}
			else
			{
				t->tsnap.status &= ~POINT_INIT;
			}
		}
		*/
	}
}

/********************** TARGET **************************/

void TargetSnapCenter(TransInfo *t)
{
	// Only need to calculate once
	if ((t->tsnap.status & TARGET_INIT) == 0)
	{
		VECCOPY(t->tsnap.snapTarget, t->center);	
		if(t->flag & (T_EDIT|T_POSE)) {
			Object *ob= G.obedit?G.obedit:t->poseobj;
			Mat4MulVecfl(ob->obmat, t->tsnap.snapTarget);
		}
		
		t->tsnap.status |= TARGET_INIT;		
	}
}

void TargetSnapMedian(TransInfo *t)
{
	// Only need to calculate once
	if ((t->tsnap.status & TARGET_INIT) == 0)
	{
		TransData *td = NULL;

		t->tsnap.snapTarget[0] = 0;
		t->tsnap.snapTarget[1] = 0;
		t->tsnap.snapTarget[2] = 0;
		
		for (td = t->data; td != NULL && td->flag & TD_SELECTED ; td++)
		{
			VecAddf(t->tsnap.snapTarget, t->tsnap.snapTarget, td->iloc);
		}
		
		VecMulf(t->tsnap.snapTarget, 1.0 / t->total);
		
		if(t->flag & (T_EDIT|T_POSE)) {
			Object *ob= G.obedit?G.obedit:t->poseobj;
			Mat4MulVecfl(ob->obmat, t->tsnap.snapTarget);
		}
		
		t->tsnap.status |= TARGET_INIT;		
	}
}

void TargetSnapClosest(TransInfo *t)
{
	// Only valid if a snap point has been selected
	if (t->tsnap.status & POINT_INIT)
	{
		TransData *closest = NULL, *td = NULL;
		float closestDist = 0;
		
		// Base case, only one selected item
		if (t->total == 1)
		{
			closest = t->data;
		}
		// More than one selected item
		else
			{
			float point[3];
			
			VECCOPY(point, t->tsnap.snapPoint);
				
			if(t->flag & (T_EDIT|T_POSE)) {
				Object *ob= G.obedit?G.obedit:t->poseobj;
				float imat[4][4];
				Mat4Invert(imat, ob->obmat);
				Mat4MulVecfl(imat, point);
			}
				
			for (td = t->data; td != NULL && td->flag & TD_SELECTED ; td++)
			{
				float vdist[3];
				float dist;
				
				VecSubf(vdist, td->iloc, point);
				dist = Inpf(vdist, vdist);
				
				if (closest == NULL || dist < closestDist)
				{
					closest = td;
					closestDist = dist; 
				}
			}
		}
		
		VECCOPY(t->tsnap.snapTarget, closest->iloc);
		
		if(t->flag & (T_EDIT|T_POSE)) {
			Object *ob= G.obedit?G.obedit:t->poseobj;
			Mat4MulVecfl(ob->obmat, t->tsnap.snapTarget);
		}
		
		t->tsnap.status |= TARGET_INIT;
	}
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

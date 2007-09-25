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
#include "DNA_meshdata_types.h" // Temporary, for snapping to other unselected meshes
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
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
#include "BIF_screen.h"
#include "BIF_editsima.h"
#include "BIF_drawimage.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"

#include "BSE_view.h"

#include "MEM_guardedalloc.h"

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

float RotationBetween(TransInfo *t, float p1[3], float p2[3]);
float TranslationBetween(TransInfo *t, float p1[3], float p2[3]);

// Trickery
int findNearestVertFromObjects(int *dist, float *loc);

/****************** IMPLEMENTATIONS *********************/

void drawSnapping(TransInfo *t)
{
	if ((t->tsnap.status & (SNAP_ON|POINT_INIT|TARGET_INIT)) == (SNAP_ON|POINT_INIT|TARGET_INIT) &&
		(G.qual & LR_CTRLKEY)) {
		
		char col[4];
		BIF_GetThemeColor3ubv(TH_TRANSFORM, col);
		glColor4ub(col[0], col[1], col[2], 128);
		
		if (t->spacetype==SPACE_VIEW3D) {
			float unitmat[4][4];
			float size;
			
			glDisable(GL_DEPTH_TEST);
	
			size = get_drawsize(G.vd);
			
			size *= 0.5f * BIF_GetThemeValuef(TH_VERTEX_SIZE);
			
			glPushMatrix();
			
			glTranslatef(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], t->tsnap.snapPoint[2]);
			
			/* sets view screen aligned */
			glRotatef( -360.0f*saacos(G.vd->viewquat[0])/(float)M_PI, G.vd->viewquat[1], G.vd->viewquat[2], G.vd->viewquat[3]);
			
			Mat4One(unitmat);
			drawcircball(GL_LINE_LOOP, unitmat[3], size, unitmat);
			
			glPopMatrix();
			
			if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
		} else if (t->spacetype==SPACE_IMAGE) {
			/*This will not draw, and Im nor sure why - campbell */
			
			/*			
			float xuser_asp, yuser_asp;
			int wi, hi;
			float w, h;
			
			calc_image_view(G.sima, 'f');	// float
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glLoadIdentity();
			
			aspect_sima(G.sima, &xuser_asp, &yuser_asp);
			
			transform_width_height_tface_uv(&wi, &hi);
			w = (((float)wi)/256.0f)*G.sima->zoom * xuser_asp;
			h = (((float)hi)/256.0f)*G.sima->zoom * yuser_asp;
			
			cpack(0xFFFFFF);
			glTranslatef(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], 0.0f);
			
			//glRectf(0,0,1,1);
			
			setlinestyle(0);
			cpack(0x0);
			fdrawline(-0.020/w, 0, -0.1/w, 0);
			fdrawline(0.1/w, 0, .020/w, 0);
			fdrawline(0, -0.020/h, 0, -0.1/h);
			fdrawline(0, 0.1/h, 0, 0.020/h);
			
			glTranslatef(-t->tsnap.snapPoint[0], -t->tsnap.snapPoint[1], 0.0f);
			setlinestyle(0);
			*/
			
		}
	}
}

int  handleSnapping(TransInfo *t, int event)
{
	int status = 0;
	
	// Put keyhandling code here
	
	return status;
}

void applySnapping(TransInfo *t, float *vec)
{
	if ((t->tsnap.status & SNAP_ON) &&
		(G.qual & LR_CTRLKEY))
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
	t->tsnap.modePoint = 0;
	t->tsnap.modeTarget = 0;
	t->tsnap.last = 0;
	t->tsnap.applySnap = NULL;
}

void initSnapping(TransInfo *t)
{
	resetSnapping(t);
	
	if (t->spacetype == SPACE_VIEW3D || t->spacetype == SPACE_IMAGE) { // Only 3D view or UV
		setSnappingCallback(t);

		if (t->tsnap.applySnap != NULL && // A snapping function actually exist
			(G.obedit != NULL && G.obedit->type==OB_MESH) && // Temporary limited to edit mode meshes
			(G.scene->snap_flag & SCE_SNAP) && // Only if the snap flag is on
			(t->flag & T_PROP_EDIT) == 0) // No PET, obviously
		{
			t->tsnap.status |= SNAP_ON;
			t->tsnap.modePoint = SNAP_GEO;
		}
		else
		{	
			/* Grid if snap is not possible */
			t->tsnap.modePoint = SNAP_GRID;
		}
	}
	else
	{
		/* Always grid outside of 3D view */
		t->tsnap.modePoint = SNAP_GRID;
	}
}

void setSnappingCallback(TransInfo *t)
{
	t->tsnap.calcSnap = CalcSnapGeometry;

	switch(G.scene->snap_target)
	{
		case SCE_SNAP_TARGET_CLOSEST:
			t->tsnap.modeTarget = SNAP_CLOSEST;
			t->tsnap.targetSnap = TargetSnapClosest;
			break;
		case SCE_SNAP_TARGET_CENTER:
			t->tsnap.modeTarget = SNAP_CENTER;
			t->tsnap.targetSnap = TargetSnapCenter;
			break;
		case SCE_SNAP_TARGET_MEDIAN:
			t->tsnap.modeTarget = SNAP_MEDIAN;
			t->tsnap.targetSnap = TargetSnapMedian;
			break;
	}

	switch (t->mode)
	{
	case TFM_TRANSLATION:
		t->tsnap.applySnap = ApplySnapTranslation;
		t->tsnap.distance = TranslationBetween;
		break;
	case TFM_ROTATION:
		t->tsnap.applySnap = ApplySnapRotation;
		t->tsnap.distance = RotationBetween;
		
		// Can't do TARGET_CENTER with rotation, use TARGET_MEDIAN instead
		if (G.scene->snap_target == SCE_SNAP_TARGET_CENTER) {
			t->tsnap.modeTarget = SNAP_MEDIAN;
			t->tsnap.targetSnap = TargetSnapMedian;
		}
		break;
	default:
		t->tsnap.applySnap = NULL;
		break;
	}
}

/********************** APPLY **************************/

void ApplySnapTranslation(TransInfo *t, float vec[3])
{
	VecSubf(vec, t->tsnap.snapPoint, t->tsnap.snapTarget);
}

void ApplySnapRotation(TransInfo *t, float *vec)
{
	if (t->tsnap.modeTarget == SNAP_CLOSEST) {
		*vec = t->tsnap.dist;
	}
	else {
		*vec = RotationBetween(t, t->tsnap.snapTarget, t->tsnap.snapPoint);
	}
}


/********************** DISTANCE **************************/

float TranslationBetween(TransInfo *t, float p1[3], float p2[3])
{
	return VecLenf(p1, p2);
}

float RotationBetween(TransInfo *t, float p1[3], float p2[3])
{
	float angle, start[3], end[3], center[3];
	
	VECCOPY(center, t->center);	
	if(t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:t->poseobj;
		Mat4MulVecfl(ob->obmat, center);
	}

	VecSubf(start, p1, center);
	VecSubf(end, p2, center);	
		
	// Angle around a constraint axis (error prone, will need debug)
	if (t->con.applyRot != NULL && (t->con.mode & CON_APPLY)) {
		float axis[3], tmp[3];
		
		t->con.applyRot(t, NULL, axis);

		Projf(tmp, end, axis);
		VecSubf(end, end, tmp);
		
		Projf(tmp, start, axis);
		VecSubf(start, start, tmp);
		
		Normalize(end);
		Normalize(start);
		
		Crossf(tmp, start, end);
		
		if (Inpf(tmp, axis) < 0.0)
			angle = -acos(Inpf(start, end));
		else	
			angle = acos(Inpf(start, end));
	}
	else {
		float mtx[3][3];
		
		Mat3CpyMat4(mtx, t->viewmat);

		Mat3MulVecfl(mtx, end);
		Mat3MulVecfl(mtx, start);
		
		angle = atan2(start[1],start[0]) - atan2(end[1],end[0]);
	}
	
	if (angle > M_PI) {
		angle = angle - 2 * M_PI;
	}
	else if (angle < -(M_PI)) {
		angle = 2 * M_PI + angle;
	}
	
	return angle;
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
		
		if (t->spacetype == SPACE_VIEW3D)
		{
			EditVert *nearest=NULL;
			float vec[3];
			int found = 0;
			int dist = 40; // Use a user defined value here
			
			// use findnearestverts in vert mode, others in other modes
			nearest = findnearestvert(&dist, SELECT, 1);
			
			found = findNearestVertFromObjects(&dist, vec);
			if (found == 1)
			{
				VECCOPY(t->tsnap.snapPoint, vec);
				
				t->tsnap.status |=  POINT_INIT;
			}
			/* If there's no outside vertex nearer, but there's one in this mesh
			 */
			else if (nearest != NULL)
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
		else if (t->spacetype == SPACE_IMAGE)
		{
			
			MTFace *nearesttf=NULL;
			unsigned int face_corner;
			
			float vec[3];
			int found = 0;
			int dist = 40; // Use a user defined value here
			
			// use findnearestverts in vert mode, others in other modes
			//nearest = findnearestvert(&dist, SELECT, 1);
			find_nearest_uv(&nearesttf, NULL, NULL, &face_corner);
			
			if (nearesttf != NULL)
			{
				VECCOPY2D(t->tsnap.snapPoint, nearesttf->uv[face_corner]);
				//Mat4MulVecfl(G.obedit->obmat, t->tsnap.snapPoint);
				
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
		
		// Base case, only one selected item
		if (t->total == 1)
		{
			VECCOPY(t->tsnap.snapTarget, t->data[0].iloc);

			if(t->flag & (T_EDIT|T_POSE)) {
				Object *ob= G.obedit?G.obedit:t->poseobj;
				Mat4MulVecfl(ob->obmat, t->tsnap.snapTarget);
			}

			t->tsnap.dist = t->tsnap.distance(t, t->tsnap.snapTarget, t->tsnap.snapPoint);
		}
		// More than one selected item
		else
			{
			for (td = t->data; td != NULL && td->flag & TD_SELECTED ; td++)
			{
				float loc[3];
				float dist;
				
				VECCOPY(loc, td->iloc);
				
				if(t->flag & (T_EDIT|T_POSE)) {
					Object *ob= G.obedit?G.obedit:t->poseobj;
					Mat4MulVecfl(ob->obmat, loc);
				}
				
				dist = t->tsnap.distance(t, loc, t->tsnap.snapPoint);
				
				if (closest == NULL || fabs(dist) < fabs(t->tsnap.dist))
				{
					VECCOPY(t->tsnap.snapTarget, loc);
					closest = td;
					t->tsnap.dist = dist; 
				}
			}
		}
		
		t->tsnap.status |= TARGET_INIT;
	}
}
/*================================================================*/

int findNearestVertFromObjects(int *dist, float *loc) {
	Base *base;
	int retval = 0;
	short mval[2];
	
	getmouseco_areawin(mval);
	
	base= FIRSTBASE;
	for ( base = FIRSTBASE; base != NULL; base = base->next ) {
		if ( TESTBASE(base) && base != BASACT ) {
			Object *ob = base->object;
			
			if (ob->type == OB_MESH) {
				Mesh *me = ob->data;
				
				if (me->totvert > 0) {
					int test = 1;
					int i;
					
					/* If number of vert is more than an arbitrary limit,
					 * test against boundbox first
					 * */
					if (me->totvert > 16) {
						struct BoundBox *bb = object_get_boundbox(ob);
						
						int minx = 0, miny = 0, maxx = 0, maxy = 0;
						int i;
						
						for (i = 0; i < 8; i++) {
							float gloc[3];
							int sloc[2];
							
							VECCOPY(gloc, bb->vec[i]);
							Mat4MulVecfl(ob->obmat, gloc);
							project_int(gloc, sloc);
							
							if (i == 0) {
								minx = maxx = sloc[0];
								miny = maxy = sloc[1];
							}
							else {
								if (minx > sloc[0]) minx = sloc[0];
								else if (maxx < sloc[0]) maxx = sloc[0];
								
								if (miny > sloc[1]) miny = sloc[1];
								else if (maxy < sloc[1]) maxy = sloc[1];
							}
						}
						
						/* Pad with distance */
	
						minx -= *dist;
						miny -= *dist;
						maxx += *dist;
						maxy += *dist;
						
						if (mval[0] > maxx || mval[0] < minx ||
							mval[1] > maxy || mval[1] < miny) {
							
							test = 0;
						}
					}
					
					if (test == 1) {
						float *verts = mesh_get_mapped_verts_nors(ob);
						
						if (verts != NULL) {
							float *fp;
							
							fp = verts;
							for( i = 0; i < me->totvert; i++, fp += 6) {
								float gloc[3];
								int sloc[2];
								int curdist;
								
								VECCOPY(gloc, fp);
								Mat4MulVecfl(ob->obmat, gloc);
								project_int(gloc, sloc);
								
								sloc[0] -= mval[0];
								sloc[1] -= mval[1];
								
								curdist = abs(sloc[0]) + abs(sloc[1]);
								
								if (curdist < *dist) {
									*dist = curdist;
									retval = 1;
									VECCOPY(loc, gloc);
								}
							}
						}

						MEM_freeN(verts);
					}
				}
			}
		}
	}
	
	return retval;
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

	// Only do something if using Snap to Grid
	if (t->tsnap.modePoint != SNAP_GRID)
		return;

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

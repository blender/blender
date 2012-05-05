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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_snap.c
 *  \ingroup edtransform
 */

 
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include "PIL_time.h"

#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h" // Temporary, for snapping to other unselected meshes
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

//#include "BDR_drawobject.h"
//
//#include "editmesh.h"
//#include "BIF_editsima.h"
#include "BIF_gl.h"
//#include "BIF_mywindow.h"
//#include "BIF_screen.h"
//#include "BIF_editsima.h"
//#include "BIF_drawimage.h"
//#include "BIF_editmesh.h"

#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_anim.h" /* for duplis */
#include "BKE_context.h"
#include "BKE_tessmesh.h"
#include "BKE_mesh.h"

#include "ED_armature.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "MEM_guardedalloc.h"

#include "transform.h"

//#include "blendef.h" /* for selection modes */

#define USE_BVH_FACE_SNAP

/********************* PROTOTYPES ***********************/

static void setSnappingCallback(TransInfo *t);

static void ApplySnapTranslation(TransInfo *t, float vec[3]);
static void ApplySnapRotation(TransInfo *t, float *vec);
static void ApplySnapResize(TransInfo *t, float vec[2]);

/* static void CalcSnapGrid(TransInfo *t, float *vec); */
static void CalcSnapGeometry(TransInfo *t, float *vec);

static void TargetSnapMedian(TransInfo *t);
static void TargetSnapCenter(TransInfo *t);
static void TargetSnapClosest(TransInfo *t);
static void TargetSnapActive(TransInfo *t);

static float RotationBetween(TransInfo *t, float p1[3], float p2[3]);
static float TranslationBetween(TransInfo *t, float p1[3], float p2[3]);
static float ResizeBetween(TransInfo *t, float p1[3], float p2[3]);


/****************** IMPLEMENTATIONS *********************/

#if 0
int BIF_snappingSupported(Object *obedit)
{
	int status = 0;
	
	if (obedit == NULL || ELEM4(obedit->type, OB_MESH, OB_ARMATURE, OB_CURVE, OB_LATTICE)) /* only support object mesh, armature, curves */
	{
		status = 1;
	}
	
	return status;
}
#endif

int validSnap(TransInfo *t)
{
	return (t->tsnap.status & (POINT_INIT|TARGET_INIT)) == (POINT_INIT|TARGET_INIT) ||
			(t->tsnap.status & (MULTI_POINTS|TARGET_INIT)) == (MULTI_POINTS|TARGET_INIT);
}

int activeSnap(TransInfo *t)
{
	return (t->modifiers & (MOD_SNAP|MOD_SNAP_INVERT)) == MOD_SNAP || (t->modifiers & (MOD_SNAP|MOD_SNAP_INVERT)) == MOD_SNAP_INVERT;
}

void drawSnapping(const struct bContext *C, TransInfo *t)
{
	if (validSnap(t) && activeSnap(t)) {
		
		unsigned char col[4], selectedCol[4], activeCol[4];
		UI_GetThemeColor3ubv(TH_TRANSFORM, col);
		col[3]= 128;
		
		UI_GetThemeColor3ubv(TH_SELECT, selectedCol);
		selectedCol[3]= 128;

		UI_GetThemeColor3ubv(TH_ACTIVE, activeCol);
		activeCol[3]= 192;

		if (t->spacetype == SPACE_VIEW3D) {
			TransSnapPoint *p;
			View3D *v3d = CTX_wm_view3d(C);
			RegionView3D *rv3d = CTX_wm_region_view3d(C);
			float imat[4][4];
			float size;
			
			glDisable(GL_DEPTH_TEST);
	
			size = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);

			invert_m4_m4(imat, rv3d->viewmat);

			for (p = t->tsnap.points.first; p; p = p->next) {
				if (p == t->tsnap.selectedPoint) {
					glColor4ubv(selectedCol);
				}
				else {
					glColor4ubv(col);
				}

				drawcircball(GL_LINE_LOOP, p->co, ED_view3d_pixel_size(rv3d, p->co) * size * 0.75f, imat);
			}

			if (t->tsnap.status & POINT_INIT) {
				glColor4ubv(activeCol);

				drawcircball(GL_LINE_LOOP, t->tsnap.snapPoint, ED_view3d_pixel_size(rv3d, t->tsnap.snapPoint) * size, imat);
			}
			
			/* draw normal if needed */
			if (usingSnappingNormal(t) && validSnappingNormal(t)) {
				glColor4ubv(activeCol);

				glBegin(GL_LINES);
					glVertex3f(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], t->tsnap.snapPoint[2]);
					glVertex3f(t->tsnap.snapPoint[0] + t->tsnap.snapNormal[0],
					           t->tsnap.snapPoint[1] + t->tsnap.snapNormal[1],
					           t->tsnap.snapPoint[2] + t->tsnap.snapNormal[2]);
					glEnd();
			}
			
			if (v3d->zbuf)
				glEnable(GL_DEPTH_TEST);
		}
		else if (t->spacetype==SPACE_IMAGE) {
			/* This will not draw, and Im nor sure why - campbell */
#if 0
			float xuser_asp, yuser_asp;
			int wi, hi;
			float w, h;
			
			calc_image_view(G.sima, 'f');	// float
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glLoadIdentity();
			
			ED_space_image_aspect(t->sa->spacedata.first, &xuser_aspx, &yuser_asp);
			ED_space_image_width(t->sa->spacedata.first, &wi, &hi);
			w = (((float)wi)/256.0f)*G.sima->zoom * xuser_asp;
			h = (((float)hi)/256.0f)*G.sima->zoom * yuser_asp;
			
			cpack(0xFFFFFF);
			glTranslatef(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], 0.0f);
			
			//glRectf(0, 0, 1, 1);
			
			setlinestyle(0);
			cpack(0x0);
			fdrawline(-0.020/w, 0, -0.1/w, 0);
			fdrawline(0.1/w, 0, .020/w, 0);
			fdrawline(0, -0.020/h, 0, -0.1/h);
			fdrawline(0, 0.1/h, 0, 0.020/h);
			
			glTranslatef(-t->tsnap.snapPoint[0], -t->tsnap.snapPoint[1], 0.0f);
			setlinestyle(0);
#endif
			
		}
	}
}

int  handleSnapping(TransInfo *t, wmEvent *event)
{
	int status = 0;

#if 0 // XXX need a proper selector for all snap mode
	if (BIF_snappingSupported(t->obedit) && event->type == TABKEY && event->shift) {
		/* toggle snap and reinit */
		t->settings->snap_flag ^= SCE_SNAP;
		initSnapping(t, NULL);
		status = 1;
	}
#endif
	if (event->type == MOUSEMOVE) {
		status |= updateSelectedSnapPoint(t);
	}
	
	return status;
}

void applyProject(TransInfo *t)
{
	/* XXX FLICKER IN OBJECT MODE */
	if ((t->tsnap.project) && activeSnap(t) && (t->flag & T_NO_PROJECT) == 0) {
		TransData *td = t->data;
		float tvec[3];
		float imat[4][4];
		int i;
	
		if (t->flag & (T_EDIT|T_POSE)) {
			Object *ob = t->obedit?t->obedit:t->poseobj;
			invert_m4_m4(imat, ob->obmat);
		}

		for (i = 0 ; i < t->total; i++, td++) {
			float iloc[3], loc[3], no[3];
			float mval[2];
			int dist = 1000;
			
			if (td->flag & TD_NOACTION)
				break;
			
			if (td->flag & TD_SKIP)
				continue;
			
			copy_v3_v3(iloc, td->loc);
			if (t->flag & (T_EDIT|T_POSE)) {
				Object *ob = t->obedit?t->obedit:t->poseobj;
				mul_m4_v3(ob->obmat, iloc);
			}
			else if (t->flag & T_OBJECT) {
				td->ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
				object_handle_update(t->scene, td->ob);
				copy_v3_v3(iloc, td->ob->obmat[3]);
			}
			
			project_float(t->ar, iloc, mval);
			
			if (snapObjectsTransform(t, mval, &dist, loc, no, t->tsnap.modeSelect)) {
//				if (t->flag & (T_EDIT|T_POSE)) {
//					mul_m4_v3(imat, loc);
//				}
//				
				sub_v3_v3v3(tvec, loc, iloc);
				
				mul_m3_v3(td->smtx, tvec);
				
				add_v3_v3(td->loc, tvec);
			}
			
			//XXX constraintTransLim(t, td);
		}
	}
}

void applySnapping(TransInfo *t, float *vec)
{
	/* project is not applied this way */
	if (t->tsnap.project)
		return;
	
	if (t->tsnap.status & SNAP_FORCED) {
		t->tsnap.targetSnap(t);
	
		t->tsnap.applySnap(t, vec);
	}
	else if ((t->tsnap.mode != SCE_SNAP_MODE_INCREMENT) && activeSnap(t)) {
		double current = PIL_check_seconds_timer();
		
		// Time base quirky code to go around findnearest slowness
		/* !TODO! add exception for object mode, no need to slow it down then */
		if (current - t->tsnap.last  >= 0.01) {
			t->tsnap.calcSnap(t, vec);
			t->tsnap.targetSnap(t);
	
			t->tsnap.last = current;
		}
		if (validSnap(t)) {
			t->tsnap.applySnap(t, vec);
		}
	}
}

void resetSnapping(TransInfo *t)
{
	t->tsnap.status = 0;
	t->tsnap.align = 0;
	t->tsnap.project = 0;
	t->tsnap.mode = 0;
	t->tsnap.modeSelect = 0;
	t->tsnap.target = 0;
	t->tsnap.last = 0;
	t->tsnap.applySnap = NULL;

	t->tsnap.snapNormal[0] = 0;
	t->tsnap.snapNormal[1] = 0;
	t->tsnap.snapNormal[2] = 0;
}

int usingSnappingNormal(TransInfo *t)
{
	return t->tsnap.align;
}

int validSnappingNormal(TransInfo *t)
{
	if (validSnap(t)) {
		if (dot_v3v3(t->tsnap.snapNormal, t->tsnap.snapNormal) > 0) {
			return 1;
		}
	}
	
	return 0;
}

static void initSnappingMode(TransInfo *t)
{
	ToolSettings *ts = t->settings;
	Object *obedit = t->obedit;
	Scene *scene = t->scene;

	/* force project off when not supported */
	if (ts->snap_mode != SCE_SNAP_MODE_FACE) {
		t->tsnap.project = 0;
	}

	t->tsnap.mode = ts->snap_mode;

	if ((t->spacetype == SPACE_VIEW3D || t->spacetype == SPACE_IMAGE) && // Only 3D view or UV
			(t->flag & T_CAMERA) == 0) { // Not with camera selected in camera view
		setSnappingCallback(t);

		/* Edit mode */
		if (t->tsnap.applySnap != NULL && // A snapping function actually exist
			(obedit != NULL && ELEM4(obedit->type, OB_MESH, OB_ARMATURE, OB_CURVE, OB_LATTICE)) ) // Temporary limited to edit mode meshes, armature, curves
		{
			/* Exclude editmesh if using proportional edit */
			if ((obedit->type == OB_MESH) && (t->flag & T_PROP_EDIT)) {
				t->tsnap.modeSelect = SNAP_NOT_OBEDIT;
			}
			else {
				t->tsnap.modeSelect = t->tsnap.snap_self ? SNAP_ALL : SNAP_NOT_OBEDIT;
			}
		}
		/* Particles edit mode*/
		else if (t->tsnap.applySnap != NULL && // A snapping function actually exist
			(obedit == NULL && BASACT && BASACT->object && BASACT->object->mode & OB_MODE_PARTICLE_EDIT ))
		{
			t->tsnap.modeSelect = SNAP_ALL;
		}
		/* Object mode */
		else if (t->tsnap.applySnap != NULL && // A snapping function actually exist
			(obedit == NULL) ) // Object Mode
		{
			t->tsnap.modeSelect = SNAP_NOT_SELECTED;
		}
		else {
			/* Grid if snap is not possible */
			t->tsnap.mode = SCE_SNAP_MODE_INCREMENT;
		}
	}
	else {
		/* Always grid outside of 3D view */
		t->tsnap.mode = SCE_SNAP_MODE_INCREMENT;
	}
}

void initSnapping(TransInfo *t, wmOperator *op)
{
	ToolSettings *ts = t->settings;
	short snap_target = t->settings->snap_target;
	
	resetSnapping(t);
	
	/* if snap property exists */
	if (op && RNA_struct_find_property(op->ptr, "snap") && RNA_struct_property_is_set(op->ptr, "snap")) {
		if (RNA_boolean_get(op->ptr, "snap")) {
			t->modifiers |= MOD_SNAP;

			if (RNA_struct_property_is_set(op->ptr, "snap_target")) {
				snap_target = RNA_enum_get(op->ptr, "snap_target");
			}
			
			if (RNA_struct_property_is_set(op->ptr, "snap_point")) {
				RNA_float_get_array(op->ptr, "snap_point", t->tsnap.snapPoint);
				t->tsnap.status |= SNAP_FORCED|POINT_INIT;
			}
			
			/* snap align only defined in specific cases */
			if (RNA_struct_find_property(op->ptr, "snap_align")) {
				t->tsnap.align = RNA_boolean_get(op->ptr, "snap_align");
				RNA_float_get_array(op->ptr, "snap_normal", t->tsnap.snapNormal);
				normalize_v3(t->tsnap.snapNormal);
			}

			if (RNA_struct_find_property(op->ptr, "use_snap_project")) {
				t->tsnap.project = RNA_boolean_get(op->ptr, "use_snap_project");
			}

			if (RNA_struct_find_property(op->ptr, "use_snap_self")) {
				t->tsnap.snap_self = RNA_boolean_get(op->ptr, "use_snap_self");
			}
		}
	}
	/* use scene defaults only when transform is modal */
	else if (t->flag & T_MODAL) {
		if (ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE)) {
			if (ts->snap_flag & SCE_SNAP) {
				t->modifiers |= MOD_SNAP;
			}

			t->tsnap.align = ((t->settings->snap_flag & SCE_SNAP_ROTATE) == SCE_SNAP_ROTATE);
			t->tsnap.project = ((t->settings->snap_flag & SCE_SNAP_PROJECT) == SCE_SNAP_PROJECT);
			t->tsnap.snap_self = !((t->settings->snap_flag & SCE_SNAP_NO_SELF) == SCE_SNAP_NO_SELF);
			t->tsnap.peel = ((t->settings->snap_flag & SCE_SNAP_PROJECT) == SCE_SNAP_PROJECT);
		}
	}
	
	t->tsnap.target = snap_target;

	initSnappingMode(t);
}

static void setSnappingCallback(TransInfo *t)
{
	t->tsnap.calcSnap = CalcSnapGeometry;

	switch (t->tsnap.target) {
		case SCE_SNAP_TARGET_CLOSEST:
			t->tsnap.targetSnap = TargetSnapClosest;
			break;
		case SCE_SNAP_TARGET_CENTER:
			t->tsnap.targetSnap = TargetSnapCenter;
			break;
		case SCE_SNAP_TARGET_MEDIAN:
			t->tsnap.targetSnap = TargetSnapMedian;
			break;
		case SCE_SNAP_TARGET_ACTIVE:
			t->tsnap.targetSnap = TargetSnapActive;
			break;

	}

	switch (t->mode) {
	case TFM_TRANSLATION:
		t->tsnap.applySnap = ApplySnapTranslation;
		t->tsnap.distance = TranslationBetween;
		break;
	case TFM_ROTATION:
		t->tsnap.applySnap = ApplySnapRotation;
		t->tsnap.distance = RotationBetween;
		
		// Can't do TARGET_CENTER with rotation, use TARGET_MEDIAN instead
		if (t->tsnap.target == SCE_SNAP_TARGET_CENTER) {
			t->tsnap.target = SCE_SNAP_TARGET_MEDIAN;
			t->tsnap.targetSnap = TargetSnapMedian;
		}
		break;
	case TFM_RESIZE:
		t->tsnap.applySnap = ApplySnapResize;
		t->tsnap.distance = ResizeBetween;
		
		// Can't do TARGET_CENTER with resize, use TARGET_MEDIAN instead
		if (t->tsnap.target == SCE_SNAP_TARGET_CENTER) {
			t->tsnap.target = SCE_SNAP_TARGET_MEDIAN;
			t->tsnap.targetSnap = TargetSnapMedian;
		}
		break;
	default:
		t->tsnap.applySnap = NULL;
		break;
	}
}

void addSnapPoint(TransInfo *t)
{
	if (t->tsnap.status & POINT_INIT) {
		TransSnapPoint *p = MEM_callocN(sizeof(TransSnapPoint), "SnapPoint");

		t->tsnap.selectedPoint = p;

		copy_v3_v3(p->co, t->tsnap.snapPoint);

		BLI_addtail(&t->tsnap.points, p);

		t->tsnap.status |= MULTI_POINTS;
	}
}

int updateSelectedSnapPoint(TransInfo *t)
{
	int status = 0;
	if (t->tsnap.status & MULTI_POINTS) {
		TransSnapPoint *p, *closest_p = NULL;
		int closest_dist = 0;
		int screen_loc[2];

		for ( p = t->tsnap.points.first; p; p = p->next ) {
			int dx, dy;
			int dist;

			project_int(t->ar, p->co, screen_loc);

			dx = t->mval[0] - screen_loc[0];
			dy = t->mval[1] - screen_loc[1];

			dist = dx * dx + dy * dy;

			if (dist < 100 && (closest_p == NULL || closest_dist > dist)) {
				closest_p = p;
				closest_dist = dist;
			}
		}

		if (closest_p) {
			status = t->tsnap.selectedPoint == closest_p ? 0 : 1;
			t->tsnap.selectedPoint = closest_p;
		}
	}

	return status;
}

void removeSnapPoint(TransInfo *t)
{
	if (t->tsnap.status & MULTI_POINTS) {
		updateSelectedSnapPoint(t);

		if (t->tsnap.selectedPoint) {
			BLI_freelinkN(&t->tsnap.points, t->tsnap.selectedPoint);

			if (t->tsnap.points.first == NULL) {
				t->tsnap.status &= ~MULTI_POINTS;
			}

			t->tsnap.selectedPoint = NULL;
		}

	}
}

void getSnapPoint(TransInfo *t, float vec[3])
{
	if (t->tsnap.points.first) {
		TransSnapPoint *p;
		int total = 0;

		vec[0] = vec[1] = vec[2] = 0;

		for (p = t->tsnap.points.first; p; p = p->next, total++) {
			add_v3_v3(vec, p->co);
		}

		if (t->tsnap.status & POINT_INIT) {
			add_v3_v3(vec, t->tsnap.snapPoint);
			total++;
		}

		mul_v3_fl(vec, 1.0f / total);
	}
	else {
		copy_v3_v3(vec, t->tsnap.snapPoint);
	}
}

/********************** APPLY **************************/

static void ApplySnapTranslation(TransInfo *t, float vec[3])
{
	float point[3];
	getSnapPoint(t, point);
	sub_v3_v3v3(vec, point, t->tsnap.snapTarget);
}

static void ApplySnapRotation(TransInfo *t, float *value)
{
	if (t->tsnap.target == SCE_SNAP_TARGET_CLOSEST) {
		*value = t->tsnap.dist;
	}
	else {
		float point[3];
		getSnapPoint(t, point);
		*value = RotationBetween(t, t->tsnap.snapTarget, point);
	}
}

static void ApplySnapResize(TransInfo *t, float vec[3])
{
	if (t->tsnap.target == SCE_SNAP_TARGET_CLOSEST) {
		vec[0] = vec[1] = vec[2] = t->tsnap.dist;
	}
	else {
		float point[3];
		getSnapPoint(t, point);
		vec[0] = vec[1] = vec[2] = ResizeBetween(t, t->tsnap.snapTarget, point);
	}
}

/********************** DISTANCE **************************/

static float TranslationBetween(TransInfo *UNUSED(t), float p1[3], float p2[3])
{
	return len_v3v3(p1, p2);
}

static float RotationBetween(TransInfo *t, float p1[3], float p2[3])
{
	float angle, start[3], end[3], center[3];
	
	copy_v3_v3(center, t->center);	
	if (t->flag & (T_EDIT|T_POSE)) {
		Object *ob= t->obedit?t->obedit:t->poseobj;
		mul_m4_v3(ob->obmat, center);
	}

	sub_v3_v3v3(start, p1, center);
	sub_v3_v3v3(end, p2, center);	
		
	// Angle around a constraint axis (error prone, will need debug)
	if (t->con.applyRot != NULL && (t->con.mode & CON_APPLY)) {
		float axis[3], tmp[3];
		
		t->con.applyRot(t, NULL, axis, NULL);

		project_v3_v3v3(tmp, end, axis);
		sub_v3_v3v3(end, end, tmp);
		
		project_v3_v3v3(tmp, start, axis);
		sub_v3_v3v3(start, start, tmp);
		
		normalize_v3(end);
		normalize_v3(start);
		
		cross_v3_v3v3(tmp, start, end);
		
		if (dot_v3v3(tmp, axis) < 0.0f)
			angle = -acos(dot_v3v3(start, end));
		else	
			angle = acos(dot_v3v3(start, end));
	}
	else {
		float mtx[3][3];
		
		copy_m3_m4(mtx, t->viewmat);

		mul_m3_v3(mtx, end);
		mul_m3_v3(mtx, start);
		
		angle = atan2(start[1], start[0]) - atan2(end[1], end[0]);
	}
	
	if (angle > (float)M_PI) {
		angle = angle - 2 * (float)M_PI;
	}
	else if (angle < -((float)M_PI)) {
		angle = 2.0f * (float)M_PI + angle;
	}
	
	return angle;
}

static float ResizeBetween(TransInfo *t, float p1[3], float p2[3])
{
	float d1[3], d2[3], center[3], len_d1;
	
	copy_v3_v3(center, t->center);	
	if (t->flag & (T_EDIT|T_POSE)) {
		Object *ob= t->obedit?t->obedit:t->poseobj;
		mul_m4_v3(ob->obmat, center);
	}

	sub_v3_v3v3(d1, p1, center);
	sub_v3_v3v3(d2, p2, center);
	
	if (t->con.applyRot != NULL && (t->con.mode & CON_APPLY)) {
		mul_m3_v3(t->con.pmtx, d1);
		mul_m3_v3(t->con.pmtx, d2);
	}
	
	len_d1 = len_v3(d1);

	return len_d1 != 0.0f ? len_v3(d2) / len_d1 : 1;
}

/********************** CALC **************************/

static void UNUSED_FUNCTION(CalcSnapGrid)(TransInfo *t, float *UNUSED(vec))
{
	snapGridAction(t, t->tsnap.snapPoint, BIG_GEARS);
}

static void CalcSnapGeometry(TransInfo *t, float *UNUSED(vec))
{
	if (t->spacetype == SPACE_VIEW3D) {
		float loc[3];
		float no[3];
		float mval[2];
		int found = 0;
		int dist = SNAP_MIN_DISTANCE; // Use a user defined value here
		
		mval[0] = t->mval[0];
		mval[1] = t->mval[1];
		
		if (t->tsnap.mode == SCE_SNAP_MODE_VOLUME) {
			ListBase depth_peels;
			DepthPeel *p1, *p2;
			float *last_p = NULL;
			float max_dist = FLT_MAX;
			float p[3] = {0.0f, 0.0f, 0.0f};
			
			depth_peels.first = depth_peels.last = NULL;
			
			peelObjectsTransForm(t, &depth_peels, mval, t->tsnap.modeSelect);
			
//			if (LAST_SNAP_POINT_VALID)
//			{
//				last_p = LAST_SNAP_POINT;
//			}
//			else
//			{
				last_p = t->tsnap.snapPoint;
//			}
			
			
			for (p1 = depth_peels.first; p1; p1 = p1->next) {
				if (p1->flag == 0) {
					float vec[3];
					float new_dist;
					
					p2 = NULL;
					p1->flag = 1;
		
					/* if peeling objects, take the first and last from each object */			
					if (t->settings->snap_flag & SCE_SNAP_PEEL_OBJECT) {
						DepthPeel *peel;
						for (peel = p1->next; peel; peel = peel->next) {
							if (peel->ob == p1->ob) {
								peel->flag = 1;
								p2 = peel;
							}
						}
					}
					/* otherwise, pair first with second and so on */
					else {
						for (p2 = p1->next; p2 && p2->ob != p1->ob; p2 = p2->next) {
							/* nothing to do here */
						}
					}
					
					if (p2) {
						p2->flag = 1;
						
						add_v3_v3v3(vec, p1->p, p2->p);
						mul_v3_fl(vec, 0.5f);
					}
					else {
						copy_v3_v3(vec, p1->p);
					}

					if (last_p == NULL) {
						copy_v3_v3(p, vec);
						max_dist = 0;
						break;
					}
					
					new_dist = len_v3v3(last_p, vec);
					
					if (new_dist < max_dist) {
						copy_v3_v3(p, vec);
						max_dist = new_dist;
					}
				}
			}
			
			if (max_dist != FLT_MAX) {
				copy_v3_v3(loc, p);
				/* XXX, is there a correct normal in this case ???, for now just z up */
				no[0]= 0.0;
				no[1]= 0.0;
				no[2]= 1.0;
				found = 1;
			}
			
			BLI_freelistN(&depth_peels);
		}
		else {
			found = snapObjectsTransform(t, mval, &dist, loc, no, t->tsnap.modeSelect);
		}
		
		if (found == 1) {
			float tangent[3];
			
			sub_v3_v3v3(tangent, loc, t->tsnap.snapPoint);
			tangent[2] = 0; 
			
			if (dot_v3v3(tangent, tangent) > 0) {
				copy_v3_v3(t->tsnap.snapTangent, tangent);
			}
			
			copy_v3_v3(t->tsnap.snapPoint, loc);
			copy_v3_v3(t->tsnap.snapNormal, no);

			t->tsnap.status |=  POINT_INIT;
		}
		else {
			t->tsnap.status &= ~POINT_INIT;
		}
	}
	else if (t->spacetype == SPACE_IMAGE && t->obedit != NULL && t->obedit->type==OB_MESH) {
		/* same as above but for UV's */
		Image *ima= ED_space_image(t->sa->spacedata.first);
		float aspx, aspy, co[2];
		
		UI_view2d_region_to_view(&t->ar->v2d, t->mval[0], t->mval[1], co, co+1);

		if (ED_uvedit_nearest_uv(t->scene, t->obedit, ima, co, t->tsnap.snapPoint)) {
			ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
			t->tsnap.snapPoint[0] *= aspx;
			t->tsnap.snapPoint[1] *= aspy;

			t->tsnap.status |=  POINT_INIT;
		}
		else {
			t->tsnap.status &= ~POINT_INIT;
		}
	}
}

/********************** TARGET **************************/

static void TargetSnapCenter(TransInfo *t)
{
	/* Only need to calculate once */
	if ((t->tsnap.status & TARGET_INIT) == 0) {
		copy_v3_v3(t->tsnap.snapTarget, t->center);	
		if (t->flag & (T_EDIT|T_POSE)) {
			Object *ob= t->obedit?t->obedit:t->poseobj;
			mul_m4_v3(ob->obmat, t->tsnap.snapTarget);
		}
		
		t->tsnap.status |= TARGET_INIT;		
	}
}

static void TargetSnapActive(TransInfo *t)
{
	/* Only need to calculate once */
	if ((t->tsnap.status & TARGET_INIT) == 0) {
		TransData *td = NULL;
		TransData *active_td = NULL;
		int i;

		for (td = t->data, i = 0 ; i < t->total && td->flag & TD_SELECTED ; i++, td++) {
			if (td->flag & TD_ACTIVE) {
				active_td = td;
				break;
			}
		}

		if (active_td) {
			copy_v3_v3(t->tsnap.snapTarget, active_td->center);
				
			if (t->flag & (T_EDIT|T_POSE)) {
				Object *ob= t->obedit?t->obedit:t->poseobj;
				mul_m4_v3(ob->obmat, t->tsnap.snapTarget);
			}
			
			t->tsnap.status |= TARGET_INIT;
		}
		/* No active, default to median */
		else {
			t->tsnap.target = SCE_SNAP_TARGET_MEDIAN;
			t->tsnap.targetSnap = TargetSnapMedian;
			TargetSnapMedian(t);
		}		
	}
}

static void TargetSnapMedian(TransInfo *t)
{
	// Only need to calculate once
	if ((t->tsnap.status & TARGET_INIT) == 0) {
		TransData *td = NULL;
		int i;

		t->tsnap.snapTarget[0] = 0;
		t->tsnap.snapTarget[1] = 0;
		t->tsnap.snapTarget[2] = 0;
		
		for (td = t->data, i = 0 ; i < t->total && td->flag & TD_SELECTED ; i++, td++) {
			add_v3_v3(t->tsnap.snapTarget, td->center);
		}
		
		mul_v3_fl(t->tsnap.snapTarget, 1.0 / i);
		
		if (t->flag & (T_EDIT|T_POSE)) {
			Object *ob= t->obedit?t->obedit:t->poseobj;
			mul_m4_v3(ob->obmat, t->tsnap.snapTarget);
		}
		
		t->tsnap.status |= TARGET_INIT;		
	}
}

static void TargetSnapClosest(TransInfo *t)
{
	// Only valid if a snap point has been selected
	if (t->tsnap.status & POINT_INIT) {
		TransData *closest = NULL, *td = NULL;
		
		/* Object mode */
		if (t->flag & T_OBJECT) {
			int i;
			for (td = t->data, i = 0 ; i < t->total && td->flag & TD_SELECTED ; i++, td++) {
				struct BoundBox *bb = object_get_boundbox(td->ob);
				
				/* use boundbox if possible */
				if (bb) {
					int j;
					
					for (j = 0; j < 8; j++) {
						float loc[3];
						float dist;
						
						copy_v3_v3(loc, bb->vec[j]);
						mul_m4_v3(td->ext->obmat, loc);
						
						dist = t->tsnap.distance(t, loc, t->tsnap.snapPoint);
						
						if (closest == NULL || fabs(dist) < fabs(t->tsnap.dist)) {
							copy_v3_v3(t->tsnap.snapTarget, loc);
							closest = td;
							t->tsnap.dist = dist; 
						}
					}
				}
				/* use element center otherwise */
				else {
					float loc[3];
					float dist;
					
					copy_v3_v3(loc, td->center);
					
					dist = t->tsnap.distance(t, loc, t->tsnap.snapPoint);
					
					if (closest == NULL || fabs(dist) < fabs(t->tsnap.dist)) {
						copy_v3_v3(t->tsnap.snapTarget, loc);
						closest = td;
						t->tsnap.dist = dist; 
					}
				}
			}
		}
		else {
			int i;
			for (td = t->data, i = 0 ; i < t->total && td->flag & TD_SELECTED ; i++, td++) {
				float loc[3];
				float dist;
				
				copy_v3_v3(loc, td->center);
				
				if (t->flag & (T_EDIT|T_POSE)) {
					Object *ob= t->obedit?t->obedit:t->poseobj;
					mul_m4_v3(ob->obmat, loc);
				}
				
				dist = t->tsnap.distance(t, loc, t->tsnap.snapPoint);
				
				if (closest == NULL || fabs(dist) < fabs(t->tsnap.dist)) {
					copy_v3_v3(t->tsnap.snapTarget, loc);
					closest = td;
					t->tsnap.dist = dist; 
				}
			}
		}
		
		t->tsnap.status |= TARGET_INIT;
	}
}
/*================================================================*/
#ifndef USE_BVH_FACE_SNAP
static int snapFace(ARegion *ar, float v1co[3], float v2co[3], float v3co[3], float *v4co, float mval[2], float ray_start[3], float ray_start_local[3], float ray_normal_local[3], float obmat[][4], float timat[][3], float loc[3], float no[3], int *dist, float *depth)
{
	float lambda;
	int result;
	int retval = 0;
	
	result = isect_ray_tri_threshold_v3(ray_start_local, ray_normal_local, v1co, v2co, v3co, &lambda, NULL, 0.001);
	
	if (result) {
		float location[3], normal[3];
		float intersect[3];
		float new_depth;
		int screen_loc[2];
		int new_dist;
		
		copy_v3_v3(intersect, ray_normal_local);
		mul_v3_fl(intersect, lambda);
		add_v3_v3(intersect, ray_start_local);
		
		copy_v3_v3(location, intersect);
		
		if (v4co)
			normal_quad_v3(normal, v1co, v2co, v3co, v4co);
		else
			normal_tri_v3(normal, v1co, v2co, v3co);

		mul_m4_v3(obmat, location);
		
		new_depth = len_v3v3(location, ray_start);					
		
		project_int(ar, location, screen_loc);
		new_dist = abs(screen_loc[0] - (int)mval[0]) + abs(screen_loc[1] - (int)mval[1]);
		
		if (new_dist <= *dist && new_depth < *depth)  {
			*depth = new_depth;
			retval = 1;
			
			copy_v3_v3(loc, location);
			copy_v3_v3(no, normal);
			
			mul_m3_v3(timat, no);
			normalize_v3(no);

			*dist = new_dist;
		} 
	}
	
	return retval;
}
#endif

static int snapEdge(ARegion *ar, float v1co[3], short v1no[3], float v2co[3], short v2no[3], float obmat[][4], float timat[][3],
                    const float ray_start[3], const float ray_start_local[3], const float ray_normal_local[3], const float mval[2],
                    float r_loc[3], float r_no[3], int *r_dist, float *r_depth)
{
	float intersect[3] = {0, 0, 0}, ray_end[3], dvec[3];
	int result;
	int retval = 0;
	
	copy_v3_v3(ray_end, ray_normal_local);
	mul_v3_fl(ray_end, 2000);
	add_v3_v3v3(ray_end, ray_start_local, ray_end);
	
	result = isect_line_line_v3(v1co, v2co, ray_start_local, ray_end, intersect, dvec); /* dvec used but we don't care about result */
	
	if (result) {
		float edge_loc[3], vec[3];
		float mul;
	
		/* check for behind ray_start */
		sub_v3_v3v3(dvec, intersect, ray_start_local);
		
		sub_v3_v3v3(edge_loc, v1co, v2co);
		sub_v3_v3v3(vec, intersect, v2co);
		
		mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);
		
		if (mul > 1) {
			mul = 1;
			copy_v3_v3(intersect, v1co);
		}
		else if (mul < 0) {
			mul = 0;
			copy_v3_v3(intersect, v2co);
		}

		if (dot_v3v3(ray_normal_local, dvec) > 0) {
			float location[3];
			float new_depth;
			int screen_loc[2];
			int new_dist;
			
			copy_v3_v3(location, intersect);
			
			mul_m4_v3(obmat, location);
			
			new_depth = len_v3v3(location, ray_start);					
			
			project_int(ar, location, screen_loc);
			new_dist = abs(screen_loc[0] - (int)mval[0]) + abs(screen_loc[1] - (int)mval[1]);
			
			/* 10% threshold if edge is closer but a bit further
			 * this takes care of series of connected edges a bit slanted w.r.t the viewport
			 * otherwise, it would stick to the verts of the closest edge and not slide along merrily 
			 * */
			if (new_dist <= *r_dist && new_depth < *r_depth * 1.001f) {
				float n1[3], n2[3];
				
				*r_depth = new_depth;
				retval = 1;
				
				sub_v3_v3v3(edge_loc, v1co, v2co);
				sub_v3_v3v3(vec, intersect, v2co);
				
				mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);
				
				if (r_no) {
					normal_short_to_float_v3(n1, v1no);						
					normal_short_to_float_v3(n2, v2no);
					interp_v3_v3v3(r_no, n2, n1, mul);
					mul_m3_v3(timat, r_no);
					normalize_v3(r_no);
				}			

				copy_v3_v3(r_loc, location);
				
				*r_dist = new_dist;
			} 
		}
	}
	
	return retval;
}

static int snapVertex(ARegion *ar, float vco[3], short vno[3], float obmat[][4], float timat[][3],
                      const float ray_start[3], const float ray_start_local[3], const float ray_normal_local[3], const float mval[2],
                      float r_loc[3], float r_no[3], int *r_dist, float *r_depth)
{
	int retval = 0;
	float dvec[3];
	
	sub_v3_v3v3(dvec, vco, ray_start_local);
	
	if (dot_v3v3(ray_normal_local, dvec) > 0) {
		float location[3];
		float new_depth;
		int screen_loc[2];
		int new_dist;
		
		copy_v3_v3(location, vco);
		
		mul_m4_v3(obmat, location);
		
		new_depth = len_v3v3(location, ray_start);
		
		project_int(ar, location, screen_loc);
		new_dist = abs(screen_loc[0] - (int)mval[0]) + abs(screen_loc[1] - (int)mval[1]);
		
		if (new_dist <= *r_dist && new_depth < *r_depth) {
			*r_depth = new_depth;
			retval = 1;
			
			copy_v3_v3(r_loc, location);
			
			if (r_no) {
				normal_short_to_float_v3(r_no, vno);
				mul_m3_v3(timat, r_no);
				normalize_v3(r_no);
			}

			*r_dist = new_dist;
		} 
	}
	
	return retval;
}

static int snapArmature(short snap_mode, ARegion *ar, Object *ob, bArmature *arm, float obmat[][4],
                        const float ray_start[3], const float ray_normal[3], const float mval[2],
                        float r_loc[3], float *UNUSED(r_no), int *r_dist, float *r_depth)
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	int retval = 0;

	invert_m4_m4(imat, obmat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_normal);
	
	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	if (arm->edbo) {
		EditBone *eBone;

		for (eBone=arm->edbo->first; eBone; eBone=eBone->next) {
			if (eBone->layer & arm->layer) {
				/* skip hidden or moving (selected) bones */
				if ((eBone->flag & (BONE_HIDDEN_A|BONE_ROOTSEL|BONE_TIPSEL))==0) {
					switch (snap_mode) {
						case SCE_SNAP_MODE_VERTEX:
							retval |= snapVertex(ar, eBone->head, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist, r_depth);
							retval |= snapVertex(ar, eBone->tail, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist, r_depth);
							break;
						case SCE_SNAP_MODE_EDGE:
							retval |= snapEdge(ar, eBone->head, NULL, eBone->tail, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist, r_depth);
							break;
					}
				}
			}
		}
	}
	else if (ob->pose && ob->pose->chanbase.first) {
		bPoseChannel *pchan;
		Bone *bone;
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			bone= pchan->bone;
			/* skip hidden bones */
			if (bone && !(bone->flag & (BONE_HIDDEN_P|BONE_HIDDEN_PG))) {
				float *head_vec = pchan->pose_head;
				float *tail_vec = pchan->pose_tail;
				
				switch (snap_mode) {
					case SCE_SNAP_MODE_VERTEX:
						retval |= snapVertex(ar, head_vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist, r_depth);
						retval |= snapVertex(ar, tail_vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist, r_depth);
						break;
					case SCE_SNAP_MODE_EDGE:
						retval |= snapEdge(ar, head_vec, NULL, tail_vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist, r_depth);
						break;
				}
			}
		}
	}

	return retval;
}

static int snapDerivedMesh(short snap_mode, ARegion *ar, Object *ob, DerivedMesh *dm, BMEditMesh *em, float obmat[][4],
                           const float ray_start[3], const float ray_normal[3], const float mval[2],
                           float r_loc[3], float r_no[3], int *r_dist, float *r_depth)
{
	int retval = 0;
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumTessFaces(dm);

	if (totvert > 0) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		int test = 1;

		invert_m4_m4(imat, obmat);

		copy_m3_m4(timat, imat);
		transpose_m3(timat);
		
		copy_v3_v3(ray_start_local, ray_start);
		copy_v3_v3(ray_normal_local, ray_normal);
		
		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);
		
		
		/* If number of vert is more than an arbitrary limit, 
		 * test against boundbox first
		 * */
		if (totface > 16) {
			struct BoundBox *bb = object_get_boundbox(ob);
			test = ray_hit_boundbox(bb, ray_start_local, ray_normal_local);
		}
		
		if (test == 1) {
			
			switch (snap_mode) {
				case SCE_SNAP_MODE_FACE:
				{ 
#ifdef USE_BVH_FACE_SNAP				// Added for durian
					BVHTreeRayHit hit;
					BVHTreeFromMesh treeData;

					/* local scale in normal direction */
					float local_scale = len_v3(ray_normal_local);

					treeData.em_evil= em;
					bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6);

					hit.index = -1;
					hit.dist = *r_depth * (*r_depth == FLT_MAX ? 1.0f : local_scale);

					if (treeData.tree && BLI_bvhtree_ray_cast(treeData.tree, ray_start_local, ray_normal_local, 0.0f, &hit, treeData.raycast_callback, &treeData) != -1) {
						if (hit.dist/local_scale <= *r_depth) {
							*r_depth= hit.dist/local_scale;
							copy_v3_v3(r_loc, hit.co);
							copy_v3_v3(r_no, hit.no);

							/* back to worldspace */
							mul_m4_v3(obmat, r_loc);
							copy_v3_v3(r_no, hit.no);

							mul_m3_v3(timat, r_no);
							normalize_v3(r_no);

							retval |= 1;
						}
					}
					break;

#else
					MVert *verts = dm->getVertArray(dm);
					MFace *faces = dm->getTessFaceArray(dm);
					int *index_array = NULL;
					int index = 0;
					int i;
					
					if (em != NULL) {
						index_array = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
						EDBM_index_arrays_init(em, 0, 0, 1);
					}
					
					for ( i = 0; i < totface; i++) {
						BMFace *efa = NULL;
						MFace *f = faces + i;
						
						test = 1; /* reset for every face */
					
						if (em != NULL) {
							if (index_array) {
								index = index_array[i];
							}
							else {
								index = i;
							}
							
							if (index == ORIGINDEX_NONE) {
								test = 0;
							}
							else {
								efa = EDBM_face_at_index(em, index);
								
								if (efa && BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
									test = 0;
								}
								else if (efa) {
									BMIter iter;
									BMLoop *l;
									
									l = BM_iter_new(&iter, em->bm, BM_LOOPS_OF_FACE, efa);
									for ( ; l; l=BM_iter_step(&iter)) {
										if (BM_elem_flag_test(l->v, BM_ELEM_SELECT)) {
											test = 0;
											break;
										}
									}
								}
							}
						}
						
						
						if (test) {
							int result;
							float *v4co = NULL;
							
							if (f->v4) {
								v4co = verts[f->v4].co;
							}
							
							result = snapFace(ar, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, v4co, mval, ray_start, ray_start_local, ray_normal_local, obmat, timat, loc, no, dist, depth);
							retval |= result;

							if (f->v4 && result == 0) {
								retval |= snapFace(ar, verts[f->v3].co, verts[f->v4].co, verts[f->v1].co, verts[f->v2].co, mval, ray_start, ray_start_local, ray_normal_local, obmat, timat, loc, no, dist, depth);
							}
						}
					}
					
					if (em != NULL) {
						EDBM_index_arrays_free(em);
					}
#endif
					break;
				}
				case SCE_SNAP_MODE_VERTEX:
				{
					MVert *verts = dm->getVertArray(dm);
					int *index_array = NULL;
					int index = 0;
					int i;
					
					if (em != NULL) {
						index_array = dm->getVertDataArray(dm, CD_ORIGINDEX);
						EDBM_index_arrays_init(em, 1, 0, 0);
					}
					
					for ( i = 0; i < totvert; i++) {
						BMVert *eve = NULL;
						MVert *v = verts + i;
						
						test = 1; /* reset for every vert */
					
						if (em != NULL) {
							if (index_array) {
								index = index_array[i];
							}
							else {
								index = i;
							}
							
							if (index == ORIGINDEX_NONE) {
								test = 0;
							}
							else {
								eve = EDBM_vert_at_index(em, index);
								
								if (eve && (BM_elem_flag_test(eve, BM_ELEM_HIDDEN) || BM_elem_flag_test(eve, BM_ELEM_SELECT))) {
									test = 0;
								}
							}
						}
						
						
						if (test) {
							retval |= snapVertex(ar, v->co, v->no, obmat, timat, ray_start, ray_start_local, ray_normal_local, mval, r_loc, r_no, r_dist, r_depth);
						}
					}

					if (em != NULL) {
						EDBM_index_arrays_free(em);
					}
					break;
				}
				case SCE_SNAP_MODE_EDGE:
				{
					MVert *verts = dm->getVertArray(dm);
					MEdge *edges = dm->getEdgeArray(dm);
					int totedge = dm->getNumEdges(dm);
					int *index_array = NULL;
					int index = 0;
					int i;
					
					if (em != NULL) {
						index_array = dm->getEdgeDataArray(dm, CD_ORIGINDEX);
						EDBM_index_arrays_init(em, 0, 1, 0);
					}
					
					for ( i = 0; i < totedge; i++) {
						BMEdge *eed = NULL;
						MEdge *e = edges + i;
						
						test = 1; /* reset for every vert */
					
						if (em != NULL) {
							if (index_array) {
								index = index_array[i];
							}
							else {
								index = i;
							}
							
							if (index == ORIGINDEX_NONE) {
								test = 0;
							}
							else {
								eed = EDBM_edge_at_index(em, index);
								
								if (eed && (BM_elem_flag_test(eed, BM_ELEM_HIDDEN) ||
									BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) || 
									BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)))
								{
									test = 0;
								}
							}
						}

						if (test) {
							retval |= snapEdge(ar, verts[e->v1].co, verts[e->v1].no, verts[e->v2].co, verts[e->v2].no, obmat, timat, ray_start, ray_start_local, ray_normal_local, mval, r_loc, r_no, r_dist, r_depth);
						}
					}

					if (em != NULL) {
						EDBM_index_arrays_free(em);
					}
					break;
				}
			}
		}
	}

	return retval;
} 

static int snapObject(Scene *scene, ARegion *ar, Object *ob, int editobject, float obmat[][4],
                      const float ray_start[3], const float ray_normal[3], const float mval[2],
                      float r_loc[3], float r_no[3], int *r_dist, float *r_depth)
{
	ToolSettings *ts= scene->toolsettings;
	int retval = 0;
	
	if (ob->type == OB_MESH) {
		BMEditMesh *em;
		DerivedMesh *dm;
		
		if (editobject) {
			em = BMEdit_FromObject(ob);
			/* dm = editbmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH); */
			dm = editbmesh_get_derived_base(ob, em); /* limitation, em & dm MUST have the same number of faces */
		}
		else {
			em = NULL;
			dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		}
		
		retval = snapDerivedMesh(ts->snap_mode, ar, ob, dm, em, obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist, r_depth);

		dm->release(dm);
	}
	else if (ob->type == OB_ARMATURE) {
		retval = snapArmature(ts->snap_mode, ar, ob, ob->data, obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist, r_depth);
	}
	
	return retval;
}

static int snapObjects(Scene *scene, View3D *v3d, ARegion *ar, Object *obedit, const float mval[2],
                       int *r_dist, float r_loc[3], float r_no[3], SnapMode mode)
{
	Base *base;
	float depth = FLT_MAX;
	int retval = 0;
	float ray_start[3], ray_normal[3];
	
	ED_view3d_win_to_ray(ar, v3d, mval, ray_start, ray_normal);

	if (mode == SNAP_ALL && obedit) {
		Object *ob = obedit;

		retval |= snapObject(scene, ar, ob, 1, ob->obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist, &depth);
	}

	/* Need an exception for particle edit because the base is flagged with BA_HAS_RECALC_DATA
	 * which makes the loop skip it, even the derived mesh will never change
	 *
	 * To solve that problem, we do it first as an exception. 
	 * */
	base= BASACT;
	if (base && base->object && base->object->mode & OB_MODE_PARTICLE_EDIT) {
		Object *ob = base->object;
		retval |= snapObject(scene, ar, ob, 0, ob->obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist, &depth);
	}

	for ( base = FIRSTBASE; base != NULL; base = base->next ) {
		if ( (BASE_VISIBLE(v3d, base)) &&
		     (base->flag & (BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA)) == 0 &&

		     (  (mode == SNAP_NOT_SELECTED && (base->flag & (SELECT|BA_WAS_SEL)) == 0) ||
		        (ELEM(mode, SNAP_ALL, SNAP_NOT_OBEDIT) && base != BASACT))  )
		{
			Object *ob = base->object;
			
			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(scene, ob);
				
				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					Object *dob = dupli_ob->ob;
					
					retval |= snapObject(scene, ar, dob, 0, dupli_ob->mat, ray_start, ray_normal, mval, r_loc, r_no, r_dist, &depth);
				}
				
				free_object_duplilist(lb);
			}
			
			retval |= snapObject(scene, ar, ob, 0, ob->obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist, &depth);
		}
	}
	
	return retval;
}

int snapObjectsTransform(TransInfo *t, const float mval[2], int *r_dist, float r_loc[3], float r_no[3], SnapMode mode)
{
	return snapObjects(t->scene, t->view, t->ar, t->obedit, mval, r_dist, r_loc, r_no, mode);
}

int snapObjectsContext(bContext *C, const float mval[2], int *r_dist, float r_loc[3], float r_no[3], SnapMode mode)
{
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;

	return snapObjects(CTX_data_scene(C), v3d, CTX_wm_region(C), CTX_data_edit_object(C), mval, r_dist, r_loc, r_no, mode);
}

/******************** PEELING *********************************/


static int cmpPeel(void *arg1, void *arg2)
{
	DepthPeel *p1 = arg1;
	DepthPeel *p2 = arg2;
	int val = 0;
	
	if (p1->depth < p2->depth) {
		val = -1;
	}
	else if (p1->depth > p2->depth) {
		val = 1;
	}
	
	return val;
}

static void removeDoublesPeel(ListBase *depth_peels)
{
	DepthPeel *peel;
	
	for (peel = depth_peels->first; peel; peel = peel->next) {
		DepthPeel *next_peel = peel->next;

		if (next_peel && ABS(peel->depth - next_peel->depth) < 0.0015f) {
			peel->next = next_peel->next;
			
			if (next_peel->next) {
				next_peel->next->prev = peel;
			}
			
			MEM_freeN(next_peel);
		}
	}
}

static void addDepthPeel(ListBase *depth_peels, float depth, float p[3], float no[3], Object *ob)
{
	DepthPeel *peel = MEM_callocN(sizeof(DepthPeel), "DepthPeel");
	
	peel->depth = depth;
	peel->ob = ob;
	copy_v3_v3(peel->p, p);
	copy_v3_v3(peel->no, no);
	
	BLI_addtail(depth_peels, peel);
	
	peel->flag = 0;
}

static int peelDerivedMesh(Object *ob, DerivedMesh *dm, float obmat[][4],
                           const float ray_start[3], const float ray_normal[3], const float UNUSED(mval[2]),
                           ListBase *depth_peels)
{
	int retval = 0;
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumTessFaces(dm);
	
	if (totvert > 0) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		int test = 1;

		invert_m4_m4(imat, obmat);

		copy_m3_m4(timat, imat);
		transpose_m3(timat);
		
		copy_v3_v3(ray_start_local, ray_start);
		copy_v3_v3(ray_normal_local, ray_normal);
		
		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);
		
		
		/* If number of vert is more than an arbitrary limit, 
		 * test against boundbox first
		 * */
		if (totface > 16) {
			struct BoundBox *bb = object_get_boundbox(ob);
			test = ray_hit_boundbox(bb, ray_start_local, ray_normal_local);
		}
		
		if (test == 1) {
			MVert *verts = dm->getVertArray(dm);
			MFace *faces = dm->getTessFaceArray(dm);
			int i;
			
			for ( i = 0; i < totface; i++) {
				MFace *f = faces + i;
				float lambda;
				int result;
				
				
				result = isect_ray_tri_threshold_v3(ray_start_local, ray_normal_local, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, &lambda, NULL, 0.001);
				
				if (result) {
					float location[3], normal[3];
					float intersect[3];
					float new_depth;
					
					copy_v3_v3(intersect, ray_normal_local);
					mul_v3_fl(intersect, lambda);
					add_v3_v3(intersect, ray_start_local);
					
					copy_v3_v3(location, intersect);
					
					if (f->v4)
						normal_quad_v3(normal, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, verts[f->v4].co);
					else
						normal_tri_v3(normal, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co);

					mul_m4_v3(obmat, location);
					
					new_depth = len_v3v3(location, ray_start);					
					
					mul_m3_v3(timat, normal);
					normalize_v3(normal);

					addDepthPeel(depth_peels, new_depth, location, normal, ob);
				}
		
				if (f->v4 && result == 0) {
					result = isect_ray_tri_threshold_v3(ray_start_local, ray_normal_local, verts[f->v3].co, verts[f->v4].co, verts[f->v1].co, &lambda, NULL, 0.001);
					
					if (result) {
						float location[3], normal[3];
						float intersect[3];
						float new_depth;
						
						copy_v3_v3(intersect, ray_normal_local);
						mul_v3_fl(intersect, lambda);
						add_v3_v3(intersect, ray_start_local);
						
						copy_v3_v3(location, intersect);
						
						if (f->v4)
							normal_quad_v3(normal, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, verts[f->v4].co);
						else
							normal_tri_v3(normal, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co);

						mul_m4_v3(obmat, location);
						
						new_depth = len_v3v3(location, ray_start);
						
						mul_m3_v3(timat, normal);
						normalize_v3(normal);
	
						addDepthPeel(depth_peels, new_depth, location, normal, ob);
					} 
				}
			}
		}
	}

	return retval;
} 

static int peelObjects(Scene *scene, View3D *v3d, ARegion *ar, Object *obedit, ListBase *depth_peels, const float mval[2], SnapMode mode)
{
	Base *base;
	int retval = 0;
	float ray_start[3], ray_normal[3];
	
	ED_view3d_win_to_ray(ar, v3d, mval, ray_start, ray_normal);

	for (base = scene->base.first; base != NULL; base = base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			Object *ob = base->object;

			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(scene, ob);
				
				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					Object *dob = dupli_ob->ob;
					
					if (dob->type == OB_MESH) {
						BMEditMesh *em;
						DerivedMesh *dm = NULL;
						int val;

						if (dob != obedit) {
							dm = mesh_get_derived_final(scene, dob, CD_MASK_BAREMESH);
							
							val = peelDerivedMesh(dob, dm, dob->obmat, ray_start, ray_normal, mval, depth_peels);
						}
						else {
							em = BMEdit_FromObject(dob);
							dm = editbmesh_get_derived_cage(scene, obedit, em, CD_MASK_BAREMESH);
							
							val = peelDerivedMesh(dob, dm, dob->obmat, ray_start, ray_normal, mval, depth_peels);
						}

						retval = retval || val;
						
						dm->release(dm);
					}
				}
				
				free_object_duplilist(lb);
			}
			
			if (ob->type == OB_MESH) {
				int val = 0;

				if (ob != obedit && ((mode == SNAP_NOT_SELECTED && (base->flag & (SELECT|BA_WAS_SEL)) == 0) || ELEM(mode, SNAP_ALL, SNAP_NOT_OBEDIT))) {
					DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
					
					val = peelDerivedMesh(ob, dm, ob->obmat, ray_start, ray_normal, mval, depth_peels);
					dm->release(dm);
				}
				else if (ob == obedit && mode != SNAP_NOT_OBEDIT) {
					BMEditMesh *em = BMEdit_FromObject(ob);
					DerivedMesh *dm = editbmesh_get_derived_cage(scene, obedit, em, CD_MASK_BAREMESH);
					
					val = peelDerivedMesh(ob, dm, ob->obmat, ray_start, ray_normal, mval, depth_peels);
					dm->release(dm);
				}
					
				retval = retval || val;
				
			}
		}
	}
	
	BLI_sortlist(depth_peels, cmpPeel);
	removeDoublesPeel(depth_peels);
	
	return retval;
}

int peelObjectsTransForm(TransInfo *t, ListBase *depth_peels, const float mval[2], SnapMode mode)
{
	return peelObjects(t->scene, t->view, t->ar, t->obedit, depth_peels, mval, mode);
}

int peelObjectsContext(bContext *C, ListBase *depth_peels, const float mval[2], SnapMode mode)
{
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;

	return peelObjects(CTX_data_scene(C), v3d, CTX_wm_region(C), CTX_data_edit_object(C), depth_peels, mval, mode);
}

/*================================================================*/

static void applyGrid(TransInfo *t, float *val, int max_index, float fac[3], GearsType action);


void snapGridAction(TransInfo *t, float *val, GearsType action)
{
	float fac[3];

	fac[NO_GEARS]    = t->snap[0];
	fac[BIG_GEARS]   = t->snap[1];
	fac[SMALL_GEARS] = t->snap[2];
	
	applyGrid(t, val, t->idx_max, fac, action);
}


void snapGrid(TransInfo *t, float *val)
{
	GearsType action;

	// Only do something if using Snap to Grid
	if (t->tsnap.mode != SCE_SNAP_MODE_INCREMENT)
		return;

	action = activeSnap(t) ? BIG_GEARS : NO_GEARS;

	if (action == BIG_GEARS && (t->modifiers & MOD_PRECISION)) {
		action = SMALL_GEARS;
	}

	snapGridAction(t, val, action);
}


static void applyGrid(TransInfo *t, float *val, int max_index, float fac[3], GearsType action)
{
	int i;
	float asp[3] = {1.0f, 1.0f, 1.0f}; // TODO: Remove hard coded limit here (3)

	if (max_index > 2) {
		printf("applyGrid: invalid index %d, clamping\n", max_index);
		max_index= 2;
	}

	// Early bailing out if no need to snap
	if (fac[action] == 0.0f)
		return;
	
	/* evil hack - snapping needs to be adapted for image aspect ratio */
	if ((t->spacetype==SPACE_IMAGE) && (t->mode==TFM_TRANSLATION)) {
		ED_space_image_uv_aspect(t->sa->spacedata.first, asp, asp+1);
	}

	for (i=0; i<=max_index; i++) {
		val[i]= fac[action]*asp[i]*(float)floor(val[i]/(fac[action]*asp[i]) +0.5f);
	}
}

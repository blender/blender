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
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"  /* Temporary, for snapping to other unselected meshes */
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_anim.h"  /* for duplis */
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_main.h"
#include "BKE_tracking.h"

#include "RNA_access.h"

#include "WM_types.h"

#include "ED_armature.h"
#include "ED_image.h"
#include "ED_node.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "MEM_guardedalloc.h"

#include "transform.h"

/* this should be passed as an arg for use in snap functions */
#undef BASACT

#define TRANSFORM_DIST_MAX_PX 1000.0f
#define TRANSFORM_SNAP_MAX_PX 100.0f
/* use half of flt-max so we can scale up without an exception */

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

static float RotationBetween(TransInfo *t, const float p1[3], const float p2[3]);
static float TranslationBetween(TransInfo *t, const float p1[3], const float p2[3]);
static float ResizeBetween(TransInfo *t, const float p1[3], const float p2[3]);


/****************** IMPLEMENTATIONS *********************/

static bool snapNodeTest(View2D *v2d, bNode *node, SnapMode mode);
static NodeBorder snapNodeBorder(int snap_node_mode);

#if 0
int BIF_snappingSupported(Object *obedit)
{
	int status = 0;
	
	/* only support object mesh, armature, curves */
	if (obedit == NULL || ELEM(obedit->type, OB_MESH, OB_ARMATURE, OB_CURVE, OB_LATTICE, OB_MBALL)) {
		status = 1;
	}
	
	return status;
}
#endif

bool validSnap(TransInfo *t)
{
	return (t->tsnap.status & (POINT_INIT | TARGET_INIT)) == (POINT_INIT | TARGET_INIT) ||
	       (t->tsnap.status & (MULTI_POINTS | TARGET_INIT)) == (MULTI_POINTS | TARGET_INIT);
}

bool activeSnap(TransInfo *t)
{
	return ((t->modifiers & (MOD_SNAP | MOD_SNAP_INVERT)) == MOD_SNAP) ||
	       ((t->modifiers & (MOD_SNAP | MOD_SNAP_INVERT)) == MOD_SNAP_INVERT);
}

void drawSnapping(const struct bContext *C, TransInfo *t)
{
	unsigned char col[4], selectedCol[4], activeCol[4];
	
	if (!activeSnap(t))
		return;
	
	UI_GetThemeColor3ubv(TH_TRANSFORM, col);
	col[3] = 128;
	
	UI_GetThemeColor3ubv(TH_SELECT, selectedCol);
	selectedCol[3] = 128;
	
	UI_GetThemeColor3ubv(TH_ACTIVE, activeCol);
	activeCol[3] = 192;
	
	if (t->spacetype == SPACE_VIEW3D) {
		if (validSnap(t)) {
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
	}
	else if (t->spacetype == SPACE_IMAGE) {
		if (validSnap(t)) {
			/* This will not draw, and Im nor sure why - campbell */
#if 0
			float xuser_asp, yuser_asp;
			int wi, hi;
			float w, h;
			
			calc_image_view(G.sima, 'f');   // float
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glLoadIdentity();
			
			ED_space_image_get_aspect(t->sa->spacedata.first, &xuser_aspx, &yuser_asp);
			ED_space_image_width(t->sa->spacedata.first, &wi, &hi);
			w = (((float)wi) / IMG_SIZE_FALLBACK) * G.sima->zoom * xuser_asp;
			h = (((float)hi) / IMG_SIZE_FALLBACK) * G.sima->zoom * yuser_asp;
			
			cpack(0xFFFFFF);
			glTranslatef(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], 0.0f);
			
			//glRectf(0, 0, 1, 1);
			
			setlinestyle(0);
			cpack(0x0);
			fdrawline(-0.020 / w, 0, -0.1 / w, 0);
			fdrawline(0.1 / w, 0, 0.020 / w, 0);
			fdrawline(0, -0.020 / h, 0, -0.1 / h);
			fdrawline(0, 0.1 / h, 0, 0.020 / h);
			
			glTranslatef(-t->tsnap.snapPoint[0], -t->tsnap.snapPoint[1], 0.0f);
			setlinestyle(0);
#endif
		}
	}
	else if (t->spacetype == SPACE_NODE) {
		if (validSnap(t)) {
			ARegion *ar = CTX_wm_region(C);
			TransSnapPoint *p;
			float size;
			
			size = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);
			
			glEnable(GL_BLEND);
			
			for (p = t->tsnap.points.first; p; p = p->next) {
				if (p == t->tsnap.selectedPoint) {
					glColor4ubv(selectedCol);
				}
				else {
					glColor4ubv(col);
				}
				
				ED_node_draw_snap(&ar->v2d, p->co, size, 0);
			}
			
			if (t->tsnap.status & POINT_INIT) {
				glColor4ubv(activeCol);
				
				ED_node_draw_snap(&ar->v2d, t->tsnap.snapPoint, size, t->tsnap.snapNodeBorder);
			}
			
			glDisable(GL_BLEND);
		}
	}
}

eRedrawFlag handleSnapping(TransInfo *t, const wmEvent *event)
{
	eRedrawFlag status = TREDRAW_NOTHING;

#if 0 // XXX need a proper selector for all snap mode
	if (BIF_snappingSupported(t->obedit) && event->type == TABKEY && event->shift) {
		/* toggle snap and reinit */
		t->settings->snap_flag ^= SCE_SNAP;
		initSnapping(t, NULL);
		status = TREDRAW_HARD;
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
	
		if (t->flag & (T_EDIT | T_POSE)) {
			Object *ob = t->obedit ? t->obedit : t->poseobj;
			invert_m4_m4(imat, ob->obmat);
		}

		for (i = 0; i < t->total; i++, td++) {
			float iloc[3], loc[3], no[3];
			float mval_fl[2];
			float dist_px = TRANSFORM_DIST_MAX_PX;
			
			if (td->flag & TD_NOACTION)
				break;
			
			if (td->flag & TD_SKIP)
				continue;

			if ((t->flag & T_PROP_EDIT) && (td->factor == 0.0f))
				continue;
			
			copy_v3_v3(iloc, td->loc);
			if (t->flag & (T_EDIT | T_POSE)) {
				Object *ob = t->obedit ? t->obedit : t->poseobj;
				mul_m4_v3(ob->obmat, iloc);
			}
			else if (t->flag & T_OBJECT) {
				/* TODO(sergey): Ideally force update is not needed here. */
				td->ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
				BKE_object_handle_update(G.main->eval_ctx, t->scene, td->ob);
				copy_v3_v3(iloc, td->ob->obmat[3]);
			}
			
			if (ED_view3d_project_float_global(t->ar, iloc, mval_fl, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
				if (snapObjectsTransform(t, mval_fl, &dist_px, loc, no, t->tsnap.modeSelect)) {
//					if (t->flag & (T_EDIT|T_POSE)) {
//						mul_m4_v3(imat, loc);
//					}

					sub_v3_v3v3(tvec, loc, iloc);

					mul_m3_v3(td->smtx, tvec);

					add_v3_v3(td->loc, tvec);

					if (t->tsnap.align) {
						/* handle alignment as well */
						const float *original_normal;
						float axis[3];
						float mat[3][3];
						float angle;
						float totmat[3][3], smat[3][3];
						float eul[3], fmat[3][3], quat[4];
						float obmat[3][3];

						/* In pose mode, we want to align normals with Y axis of bones... */
						original_normal = td->axismtx[2];

						cross_v3_v3v3(axis, original_normal, no);
						angle = saacos(dot_v3v3(original_normal, no));

						axis_angle_to_quat(quat, axis, angle);

						quat_to_mat3(mat, quat);

						mul_m3_m3m3(totmat, mat, td->mtx);
						mul_m3_m3m3(smat, td->smtx, totmat);

						/* calculate the total rotatation in eulers */
						add_v3_v3v3(eul, td->ext->irot, td->ext->drot); /* we have to correct for delta rot */
						eulO_to_mat3(obmat, eul, td->ext->rotOrder);
						/* mat = transform, obmat = object rotation */
						mul_m3_m3m3(fmat, smat, obmat);

						mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

						/* correct back for delta rot */
						sub_v3_v3v3(eul, eul, td->ext->drot);

						/* and apply */
						copy_v3_v3(td->ext->rot, eul);

						/* TODO support constraints for rotation too? see ElementRotation */
					}
				}
			}
			
			//XXX constraintTransLim(t, td);
		}
	}
}

void applyGridAbsolute(TransInfo *t)
{
	float grid_size = 0.0f;
	GearsType grid_action;
	TransData *td;
	float imat[4][4];
	int i;
	
	if (!(activeSnap(t) && (t->tsnap.mode == SCE_SNAP_MODE_GRID)))
		return;
	
	grid_action = BIG_GEARS;
	if (t->modifiers & MOD_PRECISION)
		grid_action = SMALL_GEARS;
	
	switch (grid_action) {
		case NO_GEARS: grid_size = t->snap[0]; break;
		case BIG_GEARS: grid_size = t->snap[1]; break;
		case SMALL_GEARS: grid_size = t->snap[2]; break;
	}
	/* early exit on unusable grid size */
	if (grid_size == 0.0f)
		return;
	
	if (t->flag & (T_EDIT | T_POSE)) {
		Object *ob = t->obedit ? t->obedit : t->poseobj;
		invert_m4_m4(imat, ob->obmat);
	}
	
	for (i = 0, td = t->data; i < t->total; i++, td++) {
		float iloc[3], loc[3], tvec[3];
		
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if ((t->flag & T_PROP_EDIT) && (td->factor == 0.0f))
			continue;
		
		copy_v3_v3(iloc, td->loc);
		if (t->flag & (T_EDIT | T_POSE)) {
			Object *ob = t->obedit ? t->obedit : t->poseobj;
			mul_m4_v3(ob->obmat, iloc);
		}
		else if (t->flag & T_OBJECT) {
			td->ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
			BKE_object_handle_update(G.main->eval_ctx, t->scene, td->ob);
			copy_v3_v3(iloc, td->ob->obmat[3]);
		}
		
		mul_v3_v3fl(loc, iloc, 1.0f / grid_size);
		loc[0] = floorf(loc[0]);
		loc[1] = floorf(loc[1]);
		loc[2] = floorf(loc[2]);
		mul_v3_fl(loc, grid_size);
		
		sub_v3_v3v3(tvec, loc, iloc);
		mul_m3_v3(td->smtx, tvec);
		add_v3_v3(td->loc, tvec);
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
		if (current - t->tsnap.last >= 0.01) {
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
	t->tsnap.align = false;
	t->tsnap.project = 0;
	t->tsnap.mode = 0;
	t->tsnap.modeSelect = 0;
	t->tsnap.target = 0;
	t->tsnap.last = 0;
	t->tsnap.applySnap = NULL;

	t->tsnap.snapNormal[0] = 0;
	t->tsnap.snapNormal[1] = 0;
	t->tsnap.snapNormal[2] = 0;
	
	t->tsnap.snapNodeBorder = 0;
}

bool usingSnappingNormal(TransInfo *t)
{
	return t->tsnap.align;
}

bool validSnappingNormal(TransInfo *t)
{
	if (validSnap(t)) {
		if (!is_zero_v3(t->tsnap.snapNormal)) {
			return true;
		}
	}
	
	return false;
}

static void initSnappingMode(TransInfo *t)
{
	ToolSettings *ts = t->settings;
	Object *obedit = t->obedit;
	Scene *scene = t->scene;
	Base *base_act = scene->basact;

	if (t->spacetype == SPACE_NODE) {
		/* force project off when not supported */
		t->tsnap.project = 0;
		
		t->tsnap.mode = ts->snap_node_mode;
	}
	else if (t->spacetype == SPACE_IMAGE) {
		/* force project off when not supported */
		t->tsnap.project = 0;
		
		t->tsnap.mode = ts->snap_uv_mode;
	}
	else {
		/* force project off when not supported */
		if (ts->snap_mode != SCE_SNAP_MODE_FACE)
			t->tsnap.project = 0;
		
		t->tsnap.mode = ts->snap_mode;
	}

	if ((t->spacetype == SPACE_VIEW3D || t->spacetype == SPACE_IMAGE) &&  /* Only 3D view or UV */
	    (t->flag & T_CAMERA) == 0)  /* Not with camera selected in camera view */
	{
		setSnappingCallback(t);

		/* Edit mode */
		if (t->tsnap.applySnap != NULL && // A snapping function actually exist
		    (obedit != NULL && ELEM(obedit->type, OB_MESH, OB_ARMATURE, OB_CURVE, OB_LATTICE, OB_MBALL)) ) // Temporary limited to edit mode meshes, armature, curves, mballs
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
		         (obedit == NULL && base_act && base_act->object && base_act->object->mode & OB_MODE_PARTICLE_EDIT))
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
	else if (t->spacetype == SPACE_NODE) {
		setSnappingCallback(t);
		
		if (t->tsnap.applySnap != NULL) {
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
				t->tsnap.status |= SNAP_FORCED | POINT_INIT;
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
		if (ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE, SPACE_NODE)) {
			if (ts->snap_flag & SCE_SNAP) {
				t->modifiers |= MOD_SNAP;
			}

			t->tsnap.align = ((t->settings->snap_flag & SCE_SNAP_ROTATE) != 0);
			t->tsnap.project = ((t->settings->snap_flag & SCE_SNAP_PROJECT) != 0);
			t->tsnap.snap_self = !((t->settings->snap_flag & SCE_SNAP_NO_SELF) != 0);
			t->tsnap.peel = ((t->settings->snap_flag & SCE_SNAP_PROJECT) != 0);
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
	/* Currently only 3D viewport works for snapping points. */
	if (t->tsnap.status & POINT_INIT && t->spacetype == SPACE_VIEW3D) {
		TransSnapPoint *p = MEM_callocN(sizeof(TransSnapPoint), "SnapPoint");

		t->tsnap.selectedPoint = p;

		copy_v3_v3(p->co, t->tsnap.snapPoint);

		BLI_addtail(&t->tsnap.points, p);

		t->tsnap.status |= MULTI_POINTS;
	}
}

eRedrawFlag updateSelectedSnapPoint(TransInfo *t)
{
	eRedrawFlag status = TREDRAW_NOTHING;

	if (t->tsnap.status & MULTI_POINTS) {
		TransSnapPoint *p, *closest_p = NULL;
		float dist_min_sq = TRANSFORM_SNAP_MAX_PX;
		const float mval_fl[2] = {t->mval[0], t->mval[1]};
		float screen_loc[2];

		for (p = t->tsnap.points.first; p; p = p->next) {
			float dist_sq;

			if (ED_view3d_project_float_global(t->ar, p->co, screen_loc, V3D_PROJ_TEST_NOP) != V3D_PROJ_RET_OK) {
				continue;
			}

			dist_sq = len_squared_v2v2(mval_fl, screen_loc);

			if (dist_sq < dist_min_sq) {
				closest_p = p;
				dist_min_sq = dist_sq;
			}
		}

		if (closest_p) {
			if (t->tsnap.selectedPoint != closest_p) {
				status = TREDRAW_HARD;
			}

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

			if (BLI_listbase_is_empty(&t->tsnap.points)) {
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

	if (t->spacetype == SPACE_NODE) {
		char border = t->tsnap.snapNodeBorder;
		if (border & (NODE_LEFT | NODE_RIGHT))
			vec[0] = point[0] - t->tsnap.snapTarget[0];
		if (border & (NODE_BOTTOM | NODE_TOP))
			vec[1] = point[1] - t->tsnap.snapTarget[1];
	}
	else {
		sub_v3_v3v3(vec, point, t->tsnap.snapTarget);
	}
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

static float TranslationBetween(TransInfo *UNUSED(t), const float p1[3], const float p2[3])
{
	return len_v3v3(p1, p2);
}

static float RotationBetween(TransInfo *t, const float p1[3], const float p2[3])
{
	float angle, start[3], end[3], center[3];
	
	copy_v3_v3(center, t->center);
	if (t->flag & (T_EDIT | T_POSE)) {
		Object *ob = t->obedit ? t->obedit : t->poseobj;
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

static float ResizeBetween(TransInfo *t, const float p1[3], const float p2[3])
{
	float d1[3], d2[3], center[3], len_d1;
	
	copy_v3_v3(center, t->center);
	if (t->flag & (T_EDIT | T_POSE)) {
		Object *ob = t->obedit ? t->obedit : t->poseobj;
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

static void UNUSED_FUNCTION(CalcSnapGrid) (TransInfo *t, float *UNUSED(vec))
{
	snapGridIncrementAction(t, t->tsnap.snapPoint, BIG_GEARS);
}

static void CalcSnapGeometry(TransInfo *t, float *UNUSED(vec))
{
	if (t->spacetype == SPACE_VIEW3D) {
		float loc[3];
		float no[3];
		float mval[2];
		bool found = false;
		float dist_px = SNAP_MIN_DISTANCE; // Use a user defined value here
		
		mval[0] = t->mval[0];
		mval[1] = t->mval[1];
		
		if (t->tsnap.mode == SCE_SNAP_MODE_VOLUME) {
			ListBase depth_peels;
			DepthPeel *p1, *p2;
			const float *last_p = NULL;
			float max_dist = FLT_MAX;
			float p[3] = {0.0f, 0.0f, 0.0f};
			
			BLI_listbase_clear(&depth_peels);
			
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
				no[0] = 0.0;
				no[1] = 0.0;
				no[2] = 1.0;
				found = true;
			}
			
			BLI_freelistN(&depth_peels);
		}
		else {
			found = snapObjectsTransform(t, mval, &dist_px, loc, no, t->tsnap.modeSelect);
		}
		
		if (found == true) {
			float tangent[3];
			
			sub_v2_v2v2(tangent, loc, t->tsnap.snapPoint);
			tangent[2] = 0.0f;
			
			if (!is_zero_v3(tangent)) {
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
	else if (t->spacetype == SPACE_IMAGE && t->obedit != NULL && t->obedit->type == OB_MESH) {
		/* same as above but for UV's */
		Image *ima = ED_space_image(t->sa->spacedata.first);
		float aspx, aspy, co[2];
		
		UI_view2d_region_to_view(&t->ar->v2d, t->mval[0], t->mval[1], &co[0], &co[1]);

		if (ED_uvedit_nearest_uv(t->scene, t->obedit, ima, co, t->tsnap.snapPoint)) {
			ED_space_image_get_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
			t->tsnap.snapPoint[0] *= aspx;
			t->tsnap.snapPoint[1] *= aspy;

			t->tsnap.status |=  POINT_INIT;
		}
		else {
			t->tsnap.status &= ~POINT_INIT;
		}
	}
	else if (t->spacetype == SPACE_NODE) {
		float loc[2];
		float dist_px = SNAP_MIN_DISTANCE; // Use a user defined value here
		char node_border;
		
		if (snapNodesTransform(t, t->mval, &dist_px, loc, &node_border, t->tsnap.modeSelect)) {
			copy_v2_v2(t->tsnap.snapPoint, loc);
			t->tsnap.snapNodeBorder = node_border;
			
			t->tsnap.status |=  POINT_INIT;
		}
		else {
			t->tsnap.status &= ~POINT_INIT;
		}
	}
}

/********************** TARGET **************************/

static void TargetSnapOffset(TransInfo *t, TransData *td)
{
	if (t->spacetype == SPACE_NODE && td != NULL) {
		bNode *node = td->extra;
		char border = t->tsnap.snapNodeBorder;
		float width  = BLI_rctf_size_x(&node->totr);
		float height = BLI_rctf_size_y(&node->totr);
		
#ifdef USE_NODE_CENTER
		if (border & NODE_LEFT)
			t->tsnap.snapTarget[0] -= 0.5f * width;
		if (border & NODE_RIGHT)
			t->tsnap.snapTarget[0] += 0.5f * width;
		if (border & NODE_BOTTOM)
			t->tsnap.snapTarget[1] -= 0.5f * height;
		if (border & NODE_TOP)
			t->tsnap.snapTarget[1] += 0.5f * height;
#else
		if (border & NODE_LEFT)
			t->tsnap.snapTarget[0] -= 0.0f;
		if (border & NODE_RIGHT)
			t->tsnap.snapTarget[0] += width;
		if (border & NODE_BOTTOM)
			t->tsnap.snapTarget[1] -= height;
		if (border & NODE_TOP)
			t->tsnap.snapTarget[1] += 0.0f;
#endif
	}
}

static void TargetSnapCenter(TransInfo *t)
{
	/* Only need to calculate once */
	if ((t->tsnap.status & TARGET_INIT) == 0) {
		copy_v3_v3(t->tsnap.snapTarget, t->center);
		
		if (t->flag & (T_EDIT | T_POSE)) {
			Object *ob = t->obedit ? t->obedit : t->poseobj;
			mul_m4_v3(ob->obmat, t->tsnap.snapTarget);
		}
		
		TargetSnapOffset(t, NULL);
		
		t->tsnap.status |= TARGET_INIT;
	}
}

static void TargetSnapActive(TransInfo *t)
{
	/* Only need to calculate once */
	if ((t->tsnap.status & TARGET_INIT) == 0) {
		if (calculateCenterActive(t, true, t->tsnap.snapTarget)) {
			if (t->flag & (T_EDIT | T_POSE)) {
				Object *ob = t->obedit ? t->obedit : t->poseobj;
				mul_m4_v3(ob->obmat, t->tsnap.snapTarget);
			}

			TargetSnapOffset(t, NULL);

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
		
		for (td = t->data, i = 0; i < t->total && td->flag & TD_SELECTED; i++, td++) {
			add_v3_v3(t->tsnap.snapTarget, td->center);
		}
		
		mul_v3_fl(t->tsnap.snapTarget, 1.0 / i);
		
		if (t->flag & (T_EDIT | T_POSE)) {
			Object *ob = t->obedit ? t->obedit : t->poseobj;
			mul_m4_v3(ob->obmat, t->tsnap.snapTarget);
		}
		
		TargetSnapOffset(t, NULL);
		
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
			for (td = t->data, i = 0; i < t->total && td->flag & TD_SELECTED; i++, td++) {
				struct BoundBox *bb = BKE_object_boundbox_get(td->ob);
				
				/* use boundbox if possible */
				if (bb) {
					int j;
					
					for (j = 0; j < 8; j++) {
						float loc[3];
						float dist;
						
						copy_v3_v3(loc, bb->vec[j]);
						mul_m4_v3(td->ext->obmat, loc);
						
						dist = t->tsnap.distance(t, loc, t->tsnap.snapPoint);
						
						if (closest == NULL || fabsf(dist) < fabsf(t->tsnap.dist)) {
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
					
					if (closest == NULL || fabsf(dist) < fabsf(t->tsnap.dist)) {
						copy_v3_v3(t->tsnap.snapTarget, loc);
						closest = td;
						t->tsnap.dist = dist; 
					}
				}
			}
		}
		else {
			int i;
			for (td = t->data, i = 0; i < t->total && td->flag & TD_SELECTED; i++, td++) {
				float loc[3];
				float dist;
				
				copy_v3_v3(loc, td->center);
				
				if (t->flag & (T_EDIT | T_POSE)) {
					Object *ob = t->obedit ? t->obedit : t->poseobj;
					mul_m4_v3(ob->obmat, loc);
				}
				
				dist = t->tsnap.distance(t, loc, t->tsnap.snapPoint);
				
				if (closest == NULL || fabsf(dist) < fabsf(t->tsnap.dist)) {
					copy_v3_v3(t->tsnap.snapTarget, loc);
					closest = td;
					t->tsnap.dist = dist; 
				}
			}
		}
		
		TargetSnapOffset(t, closest);
		
		t->tsnap.status |= TARGET_INIT;
	}
}

static bool snapEdge(ARegion *ar, const float v1co[3], const short v1no[3], const float v2co[3], const short v2no[3], float obmat[4][4], float timat[3][3],
                     const float ray_start[3], const float ray_start_local[3], const float ray_normal_local[3], const float mval_fl[2],
                     float r_loc[3], float r_no[3], float *r_dist_px, float *r_depth)
{
	float intersect[3] = {0, 0, 0}, ray_end[3], dvec[3];
	int result;
	bool retval = false;
	
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
			float screen_loc[2];
			float new_dist;
			
			copy_v3_v3(location, intersect);
			
			mul_m4_v3(obmat, location);
			
			new_depth = len_v3v3(location, ray_start);
			
			if (ED_view3d_project_float_global(ar, location, screen_loc, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
				new_dist = len_manhattan_v2v2(mval_fl, screen_loc);
			}
			else {
				new_dist = TRANSFORM_DIST_MAX_PX;
			}
			
			/* 10% threshold if edge is closer but a bit further
			 * this takes care of series of connected edges a bit slanted w.r.t the viewport
			 * otherwise, it would stick to the verts of the closest edge and not slide along merrily 
			 * */
			if (new_dist <= *r_dist_px && new_depth < *r_depth * 1.001f) {
				float n1[3], n2[3];
				
				*r_depth = new_depth;
				retval = true;
				
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
				
				*r_dist_px = new_dist;
			}
		}
	}
	
	return retval;
}

static bool snapVertex(ARegion *ar, const float vco[3], const short vno[3], float obmat[4][4], float timat[3][3],
                       const float ray_start[3], const float ray_start_local[3], const float ray_normal_local[3], const float mval_fl[2],
                       float r_loc[3], float r_no[3], float *r_dist_px, float *r_depth)
{
	bool retval = false;
	float dvec[3];
	
	sub_v3_v3v3(dvec, vco, ray_start_local);
	
	if (dot_v3v3(ray_normal_local, dvec) > 0) {
		float location[3];
		float new_depth;
		float screen_loc[2];
		float new_dist;
		
		copy_v3_v3(location, vco);
		
		mul_m4_v3(obmat, location);
		
		new_depth = len_v3v3(location, ray_start);
		
		if (ED_view3d_project_float_global(ar, location, screen_loc, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			new_dist = len_manhattan_v2v2(mval_fl, screen_loc);
		}
		else {
			new_dist = TRANSFORM_DIST_MAX_PX;
		}

		
		if (new_dist <= *r_dist_px && new_depth < *r_depth) {
			*r_depth = new_depth;
			retval = true;
			
			copy_v3_v3(r_loc, location);
			
			if (r_no) {
				normal_short_to_float_v3(r_no, vno);
				mul_m3_v3(timat, r_no);
				normalize_v3(r_no);
			}

			*r_dist_px = new_dist;
		}
	}
	
	return retval;
}

static bool snapArmature(short snap_mode, ARegion *ar, Object *ob, bArmature *arm, float obmat[4][4],
                         const float ray_start[3], const float ray_normal[3], const float mval[2],
                         float r_loc[3], float *UNUSED(r_no), float *r_dist_px, float *r_depth)
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	bool retval = false;

	invert_m4_m4(imat, obmat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_normal);
	
	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	if (arm->edbo) {
		EditBone *eBone;

		for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			if (eBone->layer & arm->layer) {
				/* skip hidden or moving (selected) bones */
				if ((eBone->flag & (BONE_HIDDEN_A | BONE_ROOTSEL | BONE_TIPSEL)) == 0) {
					switch (snap_mode) {
						case SCE_SNAP_MODE_VERTEX:
							retval |= snapVertex(ar, eBone->head, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							retval |= snapVertex(ar, eBone->tail, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							break;
						case SCE_SNAP_MODE_EDGE:
							retval |= snapEdge(ar, eBone->head, NULL, eBone->tail, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							break;
					}
				}
			}
		}
	}
	else if (ob->pose && ob->pose->chanbase.first) {
		bPoseChannel *pchan;
		Bone *bone;
		
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			/* skip hidden bones */
			if (bone && !(bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
				const float *head_vec = pchan->pose_head;
				const float *tail_vec = pchan->pose_tail;
				
				switch (snap_mode) {
					case SCE_SNAP_MODE_VERTEX:
						retval |= snapVertex(ar, head_vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
						retval |= snapVertex(ar, tail_vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
						break;
					case SCE_SNAP_MODE_EDGE:
						retval |= snapEdge(ar, head_vec, NULL, tail_vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
						break;
				}
			}
		}
	}

	return retval;
}

static bool snapCurve(short snap_mode, ARegion *ar, Object *ob, Curve *cu, float obmat[4][4],
                      const float ray_start[3], const float ray_normal[3], const float mval[2],
                      float r_loc[3], float *UNUSED(r_no), float *r_dist_px, float *r_depth)
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	bool retval = false;
	int u;

	Nurb *nu;

	/* only vertex snapping mode (eg control points and handles) supported for now) */
	if (snap_mode != SCE_SNAP_MODE_VERTEX) {
		return retval;
	}

	invert_m4_m4(imat, obmat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_normal);

	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	for (nu = (ob->mode == OB_MODE_EDIT ? cu->editnurb->nurbs.first : cu->nurb.first); nu; nu = nu->next) {
		for (u = 0; u < nu->pntsu; u++) {
			switch (snap_mode) {
				case SCE_SNAP_MODE_VERTEX:
				{
					if (ob->mode == OB_MODE_EDIT) {
						if (nu->bezt) {
							/* don't snap to selected (moving) or hidden */
							if (nu->bezt[u].f2 & SELECT || nu->bezt[u].hide != 0) {
								break;
							}
							retval |= snapVertex(ar, nu->bezt[u].vec[1], NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							/* don't snap if handle is selected (moving), or if it is aligning to a moving handle */
							if (!(nu->bezt[u].f1 & SELECT) && !(nu->bezt[u].h1 & HD_ALIGN && nu->bezt[u].f3 & SELECT)) {
								retval |= snapVertex(ar, nu->bezt[u].vec[0], NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							}
							if (!(nu->bezt[u].f3 & SELECT) && !(nu->bezt[u].h2 & HD_ALIGN && nu->bezt[u].f1 & SELECT)) {
								retval |= snapVertex(ar, nu->bezt[u].vec[2], NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							}
						}
						else {
							/* don't snap to selected (moving) or hidden */
							if (nu->bp[u].f1 & SELECT || nu->bp[u].hide != 0) {
								break;
							}
							retval |= snapVertex(ar, nu->bp[u].vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
						}
					}
					else {
						/* curve is not visible outside editmode if nurb length less than two */
						if (nu->pntsu > 1) {
							if (nu->bezt) {
								retval |= snapVertex(ar, nu->bezt[u].vec[1], NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							}
							else {
								retval |= snapVertex(ar, nu->bp[u].vec, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
							}
						}
					}
					break;
				}
				default:
					break;
			}
		}
	}
	return retval;
}

static bool snapDerivedMesh(short snap_mode, ARegion *ar, Object *ob, DerivedMesh *dm, BMEditMesh *em, float obmat[4][4],
                            const float ray_start[3], const float ray_normal[3], const float ray_origin[3],
                            const float mval[2], float r_loc[3], float r_no[3], float *r_dist_px, float *r_depth, bool do_bb)
{
	bool retval = false;
	const bool do_ray_start_correction = (snap_mode == SCE_SNAP_MODE_FACE && ar &&
	                                      !((RegionView3D *)ar->regiondata)->is_persp);
	int totvert = dm->getNumVerts(dm);

	if (totvert > 0) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3], local_scale, len_diff = TRANSFORM_DIST_MAX_RAY;

		invert_m4_m4(imat, obmat);
		copy_m3_m4(timat, imat);
		transpose_m3(timat);

		copy_v3_v3(ray_start_local, ray_start);
		copy_v3_v3(ray_normal_local, ray_normal);

		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);

		/* local scale in normal direction */
		local_scale = normalize_v3(ray_normal_local);

		if (do_bb) {
			BoundBox *bb = BKE_object_boundbox_get(ob);
			if (!BKE_boundbox_ray_hit_check(bb, ray_start_local, ray_normal_local, &len_diff)) {
				return retval;
			}
		}
		else if (do_ray_start_correction) {
			/* We *need* a reasonably valid len_diff in this case.
			 * Use BHVTree to find the closest face from ray_start_local.
			 */
			BVHTreeFromMesh treeData;
			BVHTreeNearest nearest;
			len_diff = 0.0f;  /* In case BVHTree would fail for some reason... */

			treeData.em_evil = em;
			bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 2, 6);
			if (treeData.tree != NULL) {
				nearest.index = -1;
				nearest.dist_sq = FLT_MAX;
				/* Compute and store result. */
				BLI_bvhtree_find_nearest(treeData.tree, ray_start_local, &nearest,
				                         treeData.nearest_callback, &treeData);
				if (nearest.index != -1) {
					len_diff = sqrtf(nearest.dist_sq);
				}
			}
			free_bvhtree_from_mesh(&treeData);
		}

		switch (snap_mode) {
			case SCE_SNAP_MODE_FACE:
			{
				BVHTreeRayHit hit;
				BVHTreeFromMesh treeData;

				/* Only use closer ray_start in case of ortho view! In perspective one, ray_start may already
				 * been *inside* boundbox, leading to snap failures (see T38409).
				 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
				 */
				if (do_ray_start_correction) {
					float ray_org_local[3];

					copy_v3_v3(ray_org_local, ray_origin);
					mul_m4_v3(imat, ray_org_local);

					/* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with very far
					 * away ray_start values (as returned in case of ortho view3d), see T38358.
					 */
					len_diff -= local_scale;  /* make temp start point a bit away from bbox hit point. */
					madd_v3_v3v3fl(ray_start_local, ray_org_local, ray_normal_local,
					               len_diff - len_v3v3(ray_start_local, ray_org_local));
				}
				else {
					len_diff = 0.0f;
				}

				treeData.em_evil = em;
				bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6);

				hit.index = -1;
				hit.dist = *r_depth;
				if (hit.dist != TRANSFORM_DIST_MAX_RAY) {
					hit.dist *= local_scale;
					hit.dist -= len_diff;
				}

				if (treeData.tree &&
				    BLI_bvhtree_ray_cast(treeData.tree, ray_start_local, ray_normal_local, 0.0f,
				                         &hit, treeData.raycast_callback, &treeData) != -1)
				{
					hit.dist += len_diff;
					hit.dist /= local_scale;
					if (hit.dist <= *r_depth) {
						*r_depth = hit.dist;
						copy_v3_v3(r_loc, hit.co);
						copy_v3_v3(r_no, hit.no);

						/* back to worldspace */
						mul_m4_v3(obmat, r_loc);
						mul_m3_v3(timat, r_no);
						normalize_v3(r_no);

						retval = true;
					}
				}
				free_bvhtree_from_mesh(&treeData);
				break;
			}
			case SCE_SNAP_MODE_VERTEX:
			{
				MVert *verts = dm->getVertArray(dm);
				const int *index_array = NULL;
				int index = 0;
				int i;

				if (em != NULL) {
					index_array = dm->getVertDataArray(dm, CD_ORIGINDEX);
					BM_mesh_elem_table_ensure(em->bm, BM_VERT);
				}

				for (i = 0; i < totvert; i++) {
					BMVert *eve = NULL;
					MVert *v = verts + i;
					bool test = true;

					if (em != NULL) {
						if (index_array) {
							index = index_array[i];
						}
						else {
							index = i;
						}
						
						if (index == ORIGINDEX_NONE) {
							test = false;
						}
						else {
							eve = BM_vert_at_index(em->bm, index);
							
							if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN) ||
							    BM_elem_flag_test(eve, BM_ELEM_SELECT))
							{
								test = false;
							}
						}
					}

					if (test) {
						retval |= snapVertex(ar, v->co, v->no, obmat, timat, ray_start, ray_start_local,
						                     ray_normal_local, mval, r_loc, r_no, r_dist_px, r_depth);
					}
				}

				break;
			}
			case SCE_SNAP_MODE_EDGE:
			{
				MVert *verts = dm->getVertArray(dm);
				MEdge *edges = dm->getEdgeArray(dm);
				int totedge = dm->getNumEdges(dm);
				const int *index_array = NULL;
				int index = 0;
				int i;

				if (em != NULL) {
					index_array = dm->getEdgeDataArray(dm, CD_ORIGINDEX);
					BM_mesh_elem_table_ensure(em->bm, BM_EDGE);
				}

				for (i = 0; i < totedge; i++) {
					MEdge *e = edges + i;
					bool test = true;

					if (em != NULL) {
						if (index_array) {
							index = index_array[i];
						}
						else {
							index = i;
						}

						if (index == ORIGINDEX_NONE) {
							test = false;
						}
						else {
							BMEdge *eed = BM_edge_at_index(em->bm, index);

							if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN) ||
							    BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
							    BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))
							{
								test = false;
							}
						}
					}

					if (test) {
						retval |= snapEdge(ar, verts[e->v1].co, verts[e->v1].no, verts[e->v2].co, verts[e->v2].no,
						                   obmat, timat, ray_start, ray_start_local, ray_normal_local, mval,
						                   r_loc, r_no, r_dist_px, r_depth);
					}
				}

				break;
			}
		}
	}

	return retval;
} 

/* may extend later (for now just snaps to empty center) */
static bool snapEmpty(short snap_mode, ARegion *ar, Object *ob, float obmat[4][4],
                      const float ray_start[3], const float ray_normal[3], const float mval[2],
                      float r_loc[3], float *UNUSED(r_no), float *r_dist_px, float *r_depth)
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	bool retval = false;

	if (ob->transflag & OB_DUPLI) {
		return retval;
	}
	/* for now only vertex supported */
	if (snap_mode != SCE_SNAP_MODE_VERTEX) {
		return retval;
	}

	invert_m4_m4(imat, obmat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_normal);

	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	switch (snap_mode) {
		case SCE_SNAP_MODE_VERTEX:
		{
			const float zero_co[3] = {0.0f};
			retval |= snapVertex(ar, zero_co, NULL, obmat, NULL, ray_start, ray_start_local, ray_normal_local, mval, r_loc, NULL, r_dist_px, r_depth);
			break;
		}
		default:
			break;
	}

	return retval;
}

static bool snapCamera(short snap_mode, ARegion *ar, Scene *scene, Object *object, float obmat[4][4],
                       const float ray_start[3], const float ray_normal[3], const float mval[2],
                       float r_loc[3], float *UNUSED(r_no), float *r_dist_px, float *r_depth)
{
	float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
	bool retval = false;
	MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
	MovieTracking *tracking;
	float ray_start_local[3], ray_normal_local[3];

	if (clip == NULL) {
		return retval;
	}
	if (object->transflag & OB_DUPLI) {
		return retval;
	}

	tracking = &clip->tracking;

	BKE_tracking_get_camera_object_matrix(scene, object, orig_camera_mat);

	invert_m4_m4(orig_camera_imat, orig_camera_mat);
	invert_m4_m4(imat, obmat);

	switch (snap_mode) {
		case SCE_SNAP_MODE_VERTEX:
		{
			MovieTrackingObject *tracking_object;

			for (tracking_object = tracking->objects.first;
			     tracking_object;
			     tracking_object = tracking_object->next)
			{
				ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
				MovieTrackingTrack *track;
				float reconstructed_camera_mat[4][4],
				      reconstructed_camera_imat[4][4];
				float (*vertex_obmat)[4];

				copy_v3_v3(ray_start_local, ray_start);
				copy_v3_v3(ray_normal_local, ray_normal);

				if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
					BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object,
					                                                  CFRA, reconstructed_camera_mat);

					invert_m4_m4(reconstructed_camera_imat, reconstructed_camera_mat);
				}

				for (track = tracksbase->first; track; track = track->next) {
					float bundle_pos[3];

					if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
						continue;
					}

					copy_v3_v3(bundle_pos, track->bundle_pos);
					if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
						mul_m4_v3(orig_camera_imat, ray_start_local);
						mul_mat3_m4_v3(orig_camera_imat, ray_normal_local);
						vertex_obmat = orig_camera_mat;
					}
					else {
						mul_m4_v3(reconstructed_camera_imat, bundle_pos);
						mul_m4_v3(imat, ray_start_local);
						mul_mat3_m4_v3(imat, ray_normal_local);
						vertex_obmat = obmat;
					}

					retval |= snapVertex(ar, bundle_pos, NULL, vertex_obmat, NULL,
					                     ray_start, ray_start_local, ray_normal_local, mval,
					                     r_loc, NULL, r_dist_px, r_depth);
				}
			}

			break;
		}
		default:
			break;
	}

	return retval;
}

static bool snapObject(Scene *scene, short snap_mode, ARegion *ar, Object *ob, float obmat[4][4], bool use_obedit,
                       Object **r_ob, float r_obmat[4][4],
                       const float ray_start[3], const float ray_normal[3], const float ray_origin[3],
                       const float mval[2], float r_loc[3], float r_no[3], float *r_dist_px, float *r_depth)
{
	bool retval = false;
	
	if (ob->type == OB_MESH) {
		BMEditMesh *em;
		DerivedMesh *dm;
		bool do_bb = true;
		
		if (use_obedit) {
			em = BKE_editmesh_from_object(ob);
			dm = editbmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH);
			do_bb = false;
		}
		else {
			em = NULL;
			dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		}
		
		retval = snapDerivedMesh(snap_mode, ar, ob, dm, em, obmat, ray_start, ray_normal, ray_origin, mval, r_loc, r_no, r_dist_px, r_depth, do_bb);

		dm->release(dm);
	}
	else if (ob->type == OB_ARMATURE) {
		retval = snapArmature(snap_mode, ar, ob, ob->data, obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist_px, r_depth);
	}
	else if (ob->type == OB_CURVE) {
		retval = snapCurve(snap_mode, ar, ob, ob->data, obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist_px, r_depth);
	}
	else if (ob->type == OB_EMPTY) {
		retval = snapEmpty(snap_mode, ar, ob, obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist_px, r_depth);
	}
	else if (ob->type == OB_CAMERA) {
		retval = snapCamera(snap_mode, ar, scene, ob, obmat, ray_start, ray_normal, mval, r_loc, r_no, r_dist_px, r_depth);
	}
	
	if (retval) {
		if (r_ob) {
			*r_ob = ob;
			copy_m4_m4(r_obmat, obmat);
		}
	}

	return retval;
}

static bool snapObjectsRay(Scene *scene, short snap_mode, Base *base_act, View3D *v3d, ARegion *ar, Object *obedit,
                           Object **r_ob, float r_obmat[4][4],
                           const float ray_start[3], const float ray_normal[3], const float ray_origin[3],
                           float *r_ray_dist,
                           const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode)
{
	Base *base;
	bool retval = false;

	if (mode == SNAP_ALL && obedit) {
		Object *ob = obedit;

		retval |= snapObject(scene, snap_mode, ar, ob, ob->obmat, true,
		                     r_ob, r_obmat,
		                     ray_start, ray_normal, ray_origin, mval, r_loc, r_no, r_dist_px, r_ray_dist);
	}

	/* Need an exception for particle edit because the base is flagged with BA_HAS_RECALC_DATA
	 * which makes the loop skip it, even the derived mesh will never change
	 *
	 * To solve that problem, we do it first as an exception. 
	 * */
	base = base_act;
	if (base && base->object && base->object->mode & OB_MODE_PARTICLE_EDIT) {
		Object *ob = base->object;
		retval |= snapObject(scene, snap_mode, ar, ob, ob->obmat, false,
		                     r_ob, r_obmat,
		                     ray_start, ray_normal, ray_origin, mval, r_loc, r_no, r_dist_px, r_ray_dist);
	}

	for (base = FIRSTBASE; base != NULL; base = base->next) {
		if ((BASE_VISIBLE_BGMODE(v3d, scene, base)) &&
		    (base->flag & (BA_HAS_RECALC_OB | BA_HAS_RECALC_DATA)) == 0 &&

		    ((mode == SNAP_NOT_SELECTED && (base->flag & (SELECT | BA_WAS_SEL)) == 0) ||
		     (ELEM(mode, SNAP_ALL, SNAP_NOT_OBEDIT) && base != base_act)))
		{
			Object *ob = base->object;
			
			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(G.main->eval_ctx, scene, ob);
				
				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					retval |= snapObject(scene, snap_mode, ar, dupli_ob->ob, dupli_ob->mat, false,
					                     r_ob, r_obmat,
					                     ray_start, ray_normal, ray_origin, mval, r_loc, r_no, r_dist_px, r_ray_dist);
				}
				
				free_object_duplilist(lb);
			}
			
			retval |= snapObject(scene, snap_mode, ar, ob, ob->obmat, false,
			                     r_ob, r_obmat,
			                     ray_start, ray_normal, ray_origin, mval, r_loc, r_no, r_dist_px, r_ray_dist);
		}
	}
	
	return retval;
}
static bool snapObjects(Scene *scene, short snap_mode, Base *base_act, View3D *v3d, ARegion *ar, Object *obedit,
                        const float mval[2], float *r_dist_px,
                        float r_loc[3], float r_no[3], float *r_ray_dist, SnapMode mode)
{
	float ray_start[3], ray_normal[3], ray_orgigin[3];

	if (!ED_view3d_win_to_ray_ex(ar, v3d, mval, ray_orgigin, ray_normal, ray_start, true)) {
		return false;
	}

	return snapObjectsRay(scene, snap_mode, base_act, v3d, ar, obedit,
	                      NULL, NULL,
	                      ray_start, ray_normal, ray_orgigin, r_ray_dist,
	                      mval, r_dist_px, r_loc, r_no, mode);
}

bool snapObjectsTransform(TransInfo *t, const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode)
{
	float ray_dist = TRANSFORM_DIST_MAX_RAY;
	return snapObjects(t->scene, t->scene->toolsettings->snap_mode, t->scene->basact, t->view, t->ar, t->obedit,
	                   mval, r_dist_px, r_loc, r_no, &ray_dist, mode);
}

bool snapObjectsContext(bContext *C, const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode)
{
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	Object *obedit = CTX_data_edit_object(C);
	float ray_dist = TRANSFORM_DIST_MAX_RAY;

	return snapObjects(scene, scene->toolsettings->snap_mode, scene->basact, v3d, ar, obedit,
	                   mval, r_dist_px, r_loc, r_no, &ray_dist, mode);
}

bool snapObjectsEx(Scene *scene, Base *base_act, View3D *v3d, ARegion *ar, Object *obedit, short snap_mode,
                   const float mval[2], float *r_dist_px,
                   float r_loc[3], float r_no[3], float *r_ray_dist, SnapMode mode)
{
	return snapObjects(scene, snap_mode, base_act, v3d, ar, obedit,
	                   mval, r_dist_px,
	                   r_loc, r_no, r_ray_dist, mode);
}
bool snapObjectsRayEx(Scene *scene, Base *base_act, View3D *v3d, ARegion *ar, Object *obedit, short snap_mode,
                      Object **r_ob, float r_obmat[4][4],
                      const float ray_start[3], const float ray_normal[3], float *r_ray_dist,
                      const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode)
{
	return snapObjectsRay(scene, snap_mode, base_act, v3d, ar, obedit,
	                      r_ob, r_obmat,
	                      ray_start, ray_normal, ray_start, r_ray_dist,
	                      mval, r_dist_px, r_loc, r_no, mode);
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

		if (next_peel && fabsf(peel->depth - next_peel->depth) < 0.0015f) {
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

static bool peelDerivedMesh(Object *ob, DerivedMesh *dm, float obmat[4][4],
                            const float ray_start[3], const float ray_normal[3], const float UNUSED(mval[2]),
                            ListBase *depth_peels)
{
	bool retval = false;
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
			struct BoundBox *bb = BKE_object_boundbox_get(ob);
			test = BKE_boundbox_ray_hit_check(bb, ray_start_local, ray_normal_local, NULL);
		}
		
		if (test == 1) {
			MVert *verts = dm->getVertArray(dm);
			MFace *faces = dm->getTessFaceArray(dm);
			int i;
			
			for (i = 0; i < totface; i++) {
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

static bool peelObjects(Scene *scene, View3D *v3d, ARegion *ar, Object *obedit,
                        ListBase *depth_peels, const float mval[2], SnapMode mode)
{
	Base *base;
	bool retval = false;
	float ray_start[3], ray_normal[3];
	
	if (ED_view3d_win_to_ray(ar, v3d, mval, ray_start, ray_normal, true) == false) {
		return false;
	}

	for (base = scene->base.first; base != NULL; base = base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			Object *ob = base->object;

			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(G.main->eval_ctx, scene, ob);
				
				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					Object *dob = dupli_ob->ob;
					
					if (dob->type == OB_MESH) {
						BMEditMesh *em;
						DerivedMesh *dm = NULL;
						bool val;

						if (dob != obedit) {
							dm = mesh_get_derived_final(scene, dob, CD_MASK_BAREMESH);
							
							val = peelDerivedMesh(dob, dm, dob->obmat, ray_start, ray_normal, mval, depth_peels);
						}
						else {
							em = BKE_editmesh_from_object(dob);
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
				bool val = false;

				if (ob != obedit && ((mode == SNAP_NOT_SELECTED && (base->flag & (SELECT | BA_WAS_SEL)) == 0) || ELEM(mode, SNAP_ALL, SNAP_NOT_OBEDIT))) {
					DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
					
					val = peelDerivedMesh(ob, dm, ob->obmat, ray_start, ray_normal, mval, depth_peels);
					dm->release(dm);
				}
				else if (ob == obedit && mode != SNAP_NOT_OBEDIT) {
					BMEditMesh *em = BKE_editmesh_from_object(ob);
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

bool peelObjectsTransForm(TransInfo *t, ListBase *depth_peels, const float mval[2], SnapMode mode)
{
	return peelObjects(t->scene, t->view, t->ar, t->obedit, depth_peels, mval, mode);
}

bool peelObjectsContext(bContext *C, ListBase *depth_peels, const float mval[2], SnapMode mode)
{
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	ARegion *ar = CTX_wm_region(C);
	Object *obedit = CTX_data_edit_object(C);

	return peelObjects(scene, v3d, ar, obedit, depth_peels, mval, mode);
}

/******************** NODES ***********************************/

static bool snapNodeTest(View2D *v2d, bNode *node, SnapMode mode)
{
	/* node is use for snapping only if a) snap mode matches and b) node is inside the view */
	return ((mode == SNAP_NOT_SELECTED && !(node->flag & NODE_SELECT)) ||
	        (mode == SNAP_ALL && !(node->flag & NODE_ACTIVE))) &&
	        (node->totr.xmin < v2d->cur.xmax && node->totr.xmax > v2d->cur.xmin &&
	         node->totr.ymin < v2d->cur.ymax && node->totr.ymax > v2d->cur.ymin);
}

static NodeBorder snapNodeBorder(int snap_node_mode)
{
	switch (snap_node_mode) {
		case SCE_SNAP_MODE_NODE_X:
			return NODE_LEFT | NODE_RIGHT;
		case SCE_SNAP_MODE_NODE_Y:
			return NODE_TOP | NODE_BOTTOM;
		case SCE_SNAP_MODE_NODE_XY:
			return NODE_LEFT | NODE_RIGHT | NODE_TOP | NODE_BOTTOM;
	}
	return 0;
}

static bool snapNode(ToolSettings *ts, SpaceNode *UNUSED(snode), ARegion *ar, bNode *node, const int mval[2],
                     float r_loc[2], float *r_dist_px, char *r_node_border)
{
	View2D *v2d = &ar->v2d;
	NodeBorder border = snapNodeBorder(ts->snap_node_mode);
	bool retval = false;
	rcti totr;
	int new_dist;
	
	UI_view2d_view_to_region_rcti(v2d, &node->totr, &totr);
	
	if (border & NODE_LEFT) {
		new_dist = abs(totr.xmin - mval[0]);
		if (new_dist < *r_dist_px) {
			UI_view2d_region_to_view(v2d, totr.xmin, mval[1], &r_loc[0], &r_loc[1]);
			*r_dist_px = new_dist;
			*r_node_border = NODE_LEFT;
			retval = true;
		}
	}
	
	if (border & NODE_RIGHT) {
		new_dist = abs(totr.xmax - mval[0]);
		if (new_dist < *r_dist_px) {
			UI_view2d_region_to_view(v2d, totr.xmax, mval[1], &r_loc[0], &r_loc[1]);
			*r_dist_px = new_dist;
			*r_node_border = NODE_RIGHT;
			retval = true;
		}
	}
	
	if (border & NODE_BOTTOM) {
		new_dist = abs(totr.ymin - mval[1]);
		if (new_dist < *r_dist_px) {
			UI_view2d_region_to_view(v2d, mval[0], totr.ymin, &r_loc[0], &r_loc[1]);
			*r_dist_px = new_dist;
			*r_node_border = NODE_BOTTOM;
			retval = true;
		}
	}
	
	if (border & NODE_TOP) {
		new_dist = abs(totr.ymax - mval[1]);
		if (new_dist < *r_dist_px) {
			UI_view2d_region_to_view(v2d, mval[0], totr.ymax, &r_loc[0], &r_loc[1]);
			*r_dist_px = new_dist;
			*r_node_border = NODE_TOP;
			retval = true;
		}
	}
	
	return retval;
}

static bool snapNodes(ToolSettings *ts, SpaceNode *snode, ARegion *ar, const int mval[2],
                     float *r_dist_px, float r_loc[2], char *r_node_border, SnapMode mode)
{
	bNodeTree *ntree = snode->edittree;
	bNode *node;
	bool retval = false;
	
	*r_node_border = 0;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (snapNodeTest(&ar->v2d, node, mode))
			retval |= snapNode(ts, snode, ar, node, mval, r_loc, r_dist_px, r_node_border);
	}
	
	return retval;
}

bool snapNodesTransform(TransInfo *t, const int mval[2], float *r_dist_px, float r_loc[2], char *r_node_border, SnapMode mode)
{
	return snapNodes(t->settings, t->sa->spacedata.first, t->ar, mval, r_dist_px, r_loc, r_node_border, mode);
}

bool snapNodesContext(bContext *C, const int mval[2], float *r_dist_px, float r_loc[2], char *r_node_border, SnapMode mode)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	return snapNodes(scene->toolsettings, CTX_wm_space_node(C), ar, mval, r_dist_px, r_loc, r_node_border, mode);
}

/*================================================================*/

static void applyGridIncrement(TransInfo *t, float *val, int max_index, float fac[3], GearsType action);


void snapGridIncrementAction(TransInfo *t, float *val, GearsType action)
{
	float fac[3];

	fac[NO_GEARS]    = t->snap[0];
	fac[BIG_GEARS]   = t->snap[1];
	fac[SMALL_GEARS] = t->snap[2];
	
	applyGridIncrement(t, val, t->idx_max, fac, action);
}


void snapGridIncrement(TransInfo *t, float *val)
{
	GearsType action;

	// Only do something if using Snap to Grid
	if (t->tsnap.mode != SCE_SNAP_MODE_INCREMENT)
		return;

	action = activeSnap(t) ? BIG_GEARS : NO_GEARS;

	if (action == BIG_GEARS && (t->modifiers & MOD_PRECISION)) {
		action = SMALL_GEARS;
	}

	snapGridIncrementAction(t, val, action);
}


static void applyGridIncrement(TransInfo *t, float *val, int max_index, float fac[3], GearsType action)
{
	int i;
	float asp[3] = {1.0f, 1.0f, 1.0f}; // TODO: Remove hard coded limit here (3)

	if (max_index > 2) {
		printf("applyGridIncrement: invalid index %d, clamping\n", max_index);
		max_index = 2;
	}

	// Early bailing out if no need to snap
	if (fac[action] == 0.0f)
		return;
	
	/* evil hack - snapping needs to be adapted for image aspect ratio */
	if ((t->spacetype == SPACE_IMAGE) && (t->mode == TFM_TRANSLATION)) {
		if (t->options & CTX_MASK) {
			ED_space_image_get_aspect(t->sa->spacedata.first, asp, asp + 1);
		}
		else if (t->options & CTX_PAINT_CURVE) {
			asp[0] = asp[1] = 1.0;
		}
		else {
			ED_space_image_get_uv_aspect(t->sa->spacedata.first, asp, asp + 1);
		}
	}

	for (i = 0; i <= max_index; i++) {
		val[i] = fac[action] * asp[i] * (float)floor(val[i] / (fac[action] * asp[i]) + 0.5f);
	}
}

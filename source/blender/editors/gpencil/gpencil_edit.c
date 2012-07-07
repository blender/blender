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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_edit.c
 *  \ingroup edgpencil
 */

 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"


#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_tracking.h"


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"
#include "ED_clip.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Context Wrangling... */

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it */
bGPdata **gpencil_data_get_pointers(const bContext *C, PointerRNA *ptr)
{
	ID *screen_id = (ID *)CTX_wm_screen(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	
	/* if there's an active area, check if the particular editor may
	 * have defined any special Grease Pencil context for editing...
	 */
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_VIEW3D: /* 3D-View */
			{
				Object *ob = CTX_data_active_object(C);
				
				/* TODO: we can include other data-types such as bones later if need be... */

				/* just in case no active object */
				if (ob) {
					/* for now, as long as there's an object, default to using that in 3D-View */
					if (ptr) RNA_id_pointer_create(&ob->id, ptr);
					return &ob->gpd;
				}
			}
			break;
			
			case SPACE_NODE: /* Nodes Editor */
			{
				SpaceNode *snode = (SpaceNode *)CTX_wm_space_data(C);
				
				/* return the GP data for the active node block/node */
				if (snode && snode->nodetree) {
					/* for now, as long as there's an active node tree, default to using that in the Nodes Editor */
					if (ptr) RNA_id_pointer_create(&snode->nodetree->id, ptr);
					return &snode->nodetree->gpd;
				}
				else {
					/* even when there is no node-tree, don't allow this to flow to scene */
					return NULL;
				}
			}
			break;
				
			case SPACE_SEQ: /* Sequencer */
			{
				SpaceSeq *sseq = (SpaceSeq *)CTX_wm_space_data(C);
				
				/* for now, Grease Pencil data is associated with the space (actually preview region only) */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceSequenceEditor, sseq, ptr);
				return &sseq->gpd;
			}
			break;

			case SPACE_IMAGE: /* Image/UV Editor */
			{
				SpaceImage *sima = (SpaceImage *)CTX_wm_space_data(C);

				/* for now, Grease Pencil data is associated with the space... */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceImageEditor, sima, ptr);
				return &sima->gpd;
			}
			break;
				
			case SPACE_CLIP: /* Nodes Editor */
			{
				SpaceClip *sc = (SpaceClip *)CTX_wm_space_data(C);
				MovieClip *clip = ED_space_clip_get_clip(sc);
				
				if (clip) {
					if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
						MovieTrackingTrack *track = BKE_tracking_track_get_active(&clip->tracking);

						if (!track)
							return NULL;

						if (ptr)
							RNA_pointer_create(&clip->id, &RNA_MovieTrackingTrack, track, ptr);

						return &track->gpd;
					}
					else {
						if (ptr)
							RNA_id_pointer_create(&clip->id, ptr);

						return &clip->gpd;
					}
				}
			}
			break;
				
			default: /* unsupported space */
				return NULL;
		}
	}
	
	/* just fall back on the scene's GP data */
	if (ptr) RNA_id_pointer_create((ID *)scene, ptr);
	return (scene) ? &scene->gpd : NULL;
}

/* Get the active Grease Pencil datablock */
bGPdata *gpencil_data_get_active(const bContext *C)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* needed for offscreen rendering */
bGPdata *gpencil_data_get_active_v3d(Scene *scene)
{
	bGPdata *gpd = scene->basact ? scene->basact->object->gpd : NULL;
	return gpd ? gpd : scene->gpd;
}

/* ************************************************ */
/* Panel Operators */

/* poll callback for adding data/layers - special */
static int gp_add_poll(bContext *C)
{
	/* the base line we have is that we have somewhere to add Grease Pencil data */
	return gpencil_data_get_pointers(C, NULL) != NULL;
}

/* ******************* Add New Data ************************ */

/* add new datablock - wrapper around API */
static int gp_data_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for Grease Pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* decrement user count and add new datablock */
		bGPdata *gpd = (*gpd_ptr);
		
		id_us_min(&gpd->id);
		*gpd_ptr = gpencil_data_addnew("GPencil");
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN | ND_GPENCIL | NA_EDITED, NULL); // XXX need a nicer one that will work
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Add New";
	ot->idname = "GPENCIL_OT_data_add";
	ot->description = "Add new Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Unlink Data ************************ */

/* poll callback for adding data/layers - special */
static int gp_data_unlink_poll(bContext *C)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	/* if we have access to some active data, make sure there's a datablock before enabling this */
	return (gpd_ptr && *gpd_ptr);
}


/* unlink datablock - wrapper around API */
static int gp_data_unlink_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for Grease Pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* just unlink datablock now, decreasing its user count */
		bGPdata *gpd = (*gpd_ptr);
		
		id_us_min(&gpd->id);
		*gpd_ptr = NULL;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN | ND_GPENCIL | NA_EDITED, NULL); // XXX need a nicer one that will work
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Unlink";
	ot->idname = "GPENCIL_OT_data_unlink";
	ot->description = "Unlink active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_unlink_exec;
	ot->poll = gp_data_unlink_poll;
}

/* ******************* Add New Layer ************************ */

/* add new layer - wrapper around API */
static int gp_layer_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for Grease Pencil data to go");
		return OPERATOR_CANCELLED;
	}
	if (*gpd_ptr == NULL)
		*gpd_ptr = gpencil_data_addnew("GPencil");
		
	/* add new layer now */
	gpencil_layer_addnew(*gpd_ptr);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN | ND_GPENCIL | NA_EDITED, NULL); // XXX please work!
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Layer";
	ot->idname = "GPENCIL_OT_layer_add";
	ot->description = "Add new Grease Pencil layer for the active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_layer_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Delete Active Frame ************************ */

static int gp_actframe_delete_poll(bContext *C)
{
	bGPdata *gpd = gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	/* only if there's an active layer with an active frame */
	return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static int gp_actframe_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	bGPDframe *gpf = gpencil_layer_getframe(gpl, CFRA, 0);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	if (ELEM(NULL, gpl, gpf)) {
		BKE_report(op->reports, RPT_ERROR, "No active frame to delete");
		return OPERATOR_CANCELLED;
	}
	
	/* delete it... */
	gpencil_layer_delframe(gpl, gpf);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN | ND_GPENCIL | NA_EDITED, NULL); // XXX please work!
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_active_frame_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Active Frame";
	ot->idname = "GPENCIL_OT_active_frame_delete";
	ot->description = "Delete the active frame for the active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_actframe_delete_exec;
	ot->poll = gp_actframe_delete_poll;
}

/* ************************************************ */
/* Grease Pencil to Data Operator */

/* defines for possible modes */
enum {
	GP_STROKECONVERT_PATH = 1,
	GP_STROKECONVERT_CURVE,
};

/* RNA enum define */
static EnumPropertyItem prop_gpencil_convertmodes[] = {
	{GP_STROKECONVERT_PATH, "PATH", 0, "Path", ""},
	{GP_STROKECONVERT_CURVE, "CURVE", 0, "Bezier Curve", ""},
	{0, NULL, 0, NULL, NULL}
};

/* --- */

/* convert the coordinates from the given stroke point into 3d-coordinates 
 *	- assumes that the active space is the 3D-View
 */
static void gp_strokepoint_convertcoords(bContext *C, bGPDstroke *gps, bGPDspoint *pt, float p3d[3], rctf *subrect)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* directly use 3d-coordinates */
		copy_v3_v3(p3d, &pt->x);
	}
	else {
		float *fp = give_cursor(scene, v3d);
		float mvalf[2];
		
		/* get screen coordinate */
		if (gps->flag & GP_STROKE_2DSPACE) {
			int mvali[2];
			View2D *v2d = &ar->v2d;
			UI_view2d_view_to_region(v2d, pt->x, pt->y, mvali, mvali + 1);
			VECCOPY2D(mvalf, mvali);
		}
		else {
			if (subrect) {
				mvalf[0] = (((float)pt->x / 100.0f) * (subrect->xmax - subrect->xmin)) + subrect->xmin;
				mvalf[1] = (((float)pt->y / 100.0f) * (subrect->ymax - subrect->ymin)) + subrect->ymin;
			}
			else {
				mvalf[0] = (float)pt->x / 100.0f * ar->winx;
				mvalf[1] = (float)pt->y / 100.0f * ar->winy;
			}
		}

		/* convert screen coordinate to 3d coordinates 
		 *	- method taken from editview.c - mouse_cursor() 
		 */
		ED_view3d_win_to_3d(ar, fp, mvalf, p3d);
	}
}

/* --- */

/* convert stroke to 3d path */
static void gp_stroke_to_path(bContext *C, bGPDlayer *gpl, bGPDstroke *gps, Curve *cu, rctf *subrect)
{
	bGPDspoint *pt;
	Nurb *nu;
	BPoint *bp;
	int i;

	/* create new 'nurb' within the curve */
	nu = (Nurb *)MEM_callocN(sizeof(Nurb), "gpstroke_to_path(nurb)");
	
	nu->pntsu = gps->totpoints;
	nu->pntsv = 1;
	nu->orderu = gps->totpoints;
	nu->flagu = CU_NURB_ENDPOINT;
	nu->resolu = 32;
	
	nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * gps->totpoints, "bpoints");
	
	/* add points */
	for (i = 0, pt = gps->points, bp = nu->bp; i < gps->totpoints; i++, pt++, bp++) {
		float p3d[3];
		
		/* get coordinates to add at */
		gp_strokepoint_convertcoords(C, gps, pt, p3d, subrect);
		copy_v3_v3(bp->vec, p3d);
		
		/* set settings */
		bp->f1 = SELECT;
		bp->radius = bp->weight = pt->pressure * gpl->thickness;
	}
	
	/* add nurb to curve */
	BLI_addtail(&cu->nurb, nu);
}

static int gp_camera_view_subrect(bContext *C, rctf *subrect)
{
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);

	if (v3d) {
		RegionView3D *rv3d = ar->regiondata;
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			Scene *scene = CTX_data_scene(C);
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, subrect, TRUE); /* no shift */
			return 1;
		}
	}

	return 0;
}

/* convert stroke to 3d bezier */
static void gp_stroke_to_bezier(bContext *C, bGPDlayer *gpl, bGPDstroke *gps, Curve *cu, rctf *subrect)
{
	bGPDspoint *pt;
	Nurb *nu;
	BezTriple *bezt;
	int i, tot;
	float p3d_cur[3], p3d_prev[3], p3d_next[3];

	/* create new 'nurb' within the curve */
	nu = (Nurb *)MEM_callocN(sizeof(Nurb), "gpstroke_to_bezier(nurb)");

	nu->pntsu = gps->totpoints;
	nu->resolu = 12;
	nu->resolv = 12;
	nu->type = CU_BEZIER;
	nu->bezt = (BezTriple *)MEM_callocN(gps->totpoints * sizeof(BezTriple), "bezts");

	tot = gps->totpoints;

	/* get initial coordinates */
	pt = gps->points;
	if (tot) {
		gp_strokepoint_convertcoords(C, gps, pt, p3d_cur, subrect);
		if (tot > 1) {
			gp_strokepoint_convertcoords(C, gps, pt + 1, p3d_next, subrect);
		}
	}

	/* add points */
	for (i = 0, bezt = nu->bezt; i < tot; i++, pt++, bezt++) {
		float h1[3], h2[3];
		
		if (i) interp_v3_v3v3(h1, p3d_cur, p3d_prev, 0.3);
		else interp_v3_v3v3(h1, p3d_cur, p3d_next, -0.3);
		
		if (i < tot - 1) interp_v3_v3v3(h2, p3d_cur, p3d_next, 0.3);
		else interp_v3_v3v3(h2, p3d_cur, p3d_prev, -0.3);
		
		copy_v3_v3(bezt->vec[0], h1);
		copy_v3_v3(bezt->vec[1], p3d_cur);
		copy_v3_v3(bezt->vec[2], h2);
		
		/* set settings */
		bezt->h1 = bezt->h2 = HD_FREE;
		bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
		bezt->radius = bezt->weight = pt->pressure * gpl->thickness * 0.1f;
		
		/* shift coord vects */
		copy_v3_v3(p3d_prev, p3d_cur);
		copy_v3_v3(p3d_cur, p3d_next);
		
		if (i + 2 < tot) {
			gp_strokepoint_convertcoords(C, gps, pt + 2, p3d_next, subrect);
		}
	}

	/* must calculate handles or else we crash */
	BKE_nurb_handles_calc(nu);

	/* add nurb to curve */
	BLI_addtail(&cu->nurb, nu);
}

/* convert a given grease-pencil layer to a 3d-curve representation (using current view if appropriate) */
static void gp_layer_to_curve(bContext *C, bGPdata *gpd, bGPDlayer *gpl, short mode)
{
	Scene *scene = CTX_data_scene(C);
	bGPDframe *gpf = gpencil_layer_getframe(gpl, CFRA, 0);
	bGPDstroke *gps;
	Object *ob;
	Curve *cu;

	/* camera framing */
	rctf subrect, *subrect_ptr = NULL;

	/* error checking */
	if (ELEM3(NULL, gpd, gpl, gpf))
		return;
		
	/* only convert if there are any strokes on this layer's frame to convert */
	if (gpf->strokes.first == NULL)
		return;

	/* initialize camera framing */
	if (gp_camera_view_subrect(C, &subrect)) {
		subrect_ptr = &subrect;
	}

	/* init the curve object (remove rotation and get curve data from it)
	 *	- must clear transforms set on object, as those skew our results
	 */
	ob = BKE_object_add(scene, OB_CURVE);
	zero_v3(ob->loc);
	zero_v3(ob->rot);
	cu = ob->data;
	cu->flag |= CU_3D;
	
	/* rename object and curve to layer name */
	rename_id((ID *)ob, gpl->info);
	rename_id((ID *)cu, gpl->info);
	
	/* add points to curve */
	for (gps = gpf->strokes.first; gps; gps = gps->next) {
		switch (mode) {
			case GP_STROKECONVERT_PATH: 
				gp_stroke_to_path(C, gpl, gps, cu, subrect_ptr);
				break;
			case GP_STROKECONVERT_CURVE:
				gp_stroke_to_bezier(C, gpl, gps, cu, subrect_ptr);
				break;
			default:
				BLI_assert(!"invalid mode");
				break;
		}
	}
}

/* --- */

static int gp_convert_poll(bContext *C)
{
	bGPdata *gpd = gpencil_data_get_active(C);
	ScrArea *sa = CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);

	/* only if there's valid data, and the current view is 3D View */
	return ((sa && sa->spacetype == SPACE_VIEW3D) && gpencil_layer_getactive(gpd) && (scene->obedit == NULL));
}

static int gp_convert_layer_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	Scene *scene = CTX_data_scene(C);
	int mode = RNA_enum_get(op->ptr, "type");

	/* check if there's data to work with */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data to work on");
		return OPERATOR_CANCELLED;
	}

	gp_layer_to_curve(C, gpd, gpl, mode);

	/* notifiers */
	WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, NULL);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

	/* done */
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_convert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert Grease Pencil";
	ot->idname = "GPENCIL_OT_convert";
	ot->description = "Convert the active Grease Pencil layer to a new Object";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = gp_convert_layer_exec;
	ot->poll = gp_convert_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_gpencil_convertmodes, 0, "Type", "");
}

/* ************************************************ */

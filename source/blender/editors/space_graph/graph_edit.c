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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "UI_view2d.h"

#include "BIF_transform.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "graph_intern.h"

/* ************************************************************************** */
/* KEYFRAME-RANGE STUFF */

/* *************************** Calculate Range ************************** */

/* Get the min/max keyframes*/
static void get_graph_keyframe_extents (bAnimContext *ac, float *xmin, float *xmax, float *ymin, float *ymax)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get data to filter, from Dopesheet */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* set large values to try to override */
	if (xmin) *xmin= 999999999.0f;
	if (xmax) *xmax= -999999999.0f;
	if (ymin) *ymin= 999999999.0f;
	if (ymax) *ymax= -999999999.0f;
	
	/* check if any channels to set range with */
	if (anim_data.first) {
		/* go through channels, finding max extents */
		for (ale= anim_data.first; ale; ale= ale->next) {
			AnimData *adt= ANIM_nla_mapping_get(ac, ale);
			FCurve *fcu= (FCurve *)ale->key_data;
			float txmin, txmax, tymin, tymax;
			
			/* get range and apply necessary scaling before */
			calc_fcurve_bounds(fcu, &txmin, &txmax, &tymin, &tymax);
			
			if (adt) {
				txmin= BKE_nla_tweakedit_remap(adt, txmin, 1);
				txmax= BKE_nla_tweakedit_remap(adt, txmax, 1);
			}
			
			/* try to set cur using these values, if they're more extreme than previously set values */
			if ((xmin) && (txmin < *xmin)) 		*xmin= txmin;
			if ((xmax) && (txmax > *xmax)) 		*xmax= txmax;
			if ((ymin) && (tymin < *ymin)) 		*ymin= tymin;
			if ((ymax) && (tymax > *ymax)) 		*ymax= tymax;
		}
		
		/* free memory */
		BLI_freelistN(&anim_data);
	}
	else {
		/* set default range */
		if (ac->scene) {
			if (xmin) *xmin= (float)ac->scene->r.sfra;
			if (xmax) *xmax= (float)ac->scene->r.efra;
		}
		else {
			if (xmin) *xmin= -5;
			if (xmax) *xmax= 100;
		}
		
		if (ymin) *ymin= -5;
		if (ymax) *ymax= 5;
	}
}

/* ****************** Automatic Preview-Range Operator ****************** */

static int graphkeys_previewrange_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	Scene *scene;
	float min, max;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.scene == NULL)
		return OPERATOR_CANCELLED;
	else
		scene= ac.scene;
	
	/* set the range directly */
	get_graph_keyframe_extents(&ac, &min, &max, NULL, NULL);
	scene->r.psfra= (int)floor(min + 0.5f);
	scene->r.pefra= (int)floor(max + 0.5f);
	
	/* set notifier that things have changed */
	// XXX err... there's nothing for frame ranges yet, but this should do fine too
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, ac.scene); 
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_previewrange_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Auto-Set Preview Range";
	ot->idname= "GRAPH_OT_previewrange_set";
	
	/* api callbacks */
	ot->exec= graphkeys_previewrange_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** View-All Operator ****************** */

static int graphkeys_viewall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	View2D *v2d;
	float extra;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	v2d= &ac.ar->v2d;
	
	/* set the horizontal range, with an extra offset so that the extreme keys will be in view */
	get_graph_keyframe_extents(&ac, &v2d->cur.xmin, &v2d->cur.xmax, &v2d->cur.ymin, &v2d->cur.ymax);
	
	extra= 0.1f * (v2d->cur.xmax - v2d->cur.xmin);
	v2d->cur.xmin -= extra;
	v2d->cur.xmax += extra;
	
	extra= 0.1f * (v2d->cur.ymax - v2d->cur.ymin);
	v2d->cur.ymin -= extra;
	v2d->cur.ymax += extra;
	
	/* do View2D syncing */
	UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
	
	/* set notifier that things have changed */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_view_all (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View All";
	ot->idname= "GRAPH_OT_view_all";
	
	/* api callbacks */
	ot->exec= graphkeys_viewall_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Create Ghost-Curves Operator *********************** */
/* This operator samples the data of the selected F-Curves to F-Points, storing them
 * as 'ghost curves' in the active Graph Editor
 */

/* Bake each F-Curve into a set of samples, and store as a ghost curve */
static void create_ghost_curves (bAnimContext *ac, int start, int end)
{	
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* free existing ghost curves */
	free_fcurves(&sipo->ghostCurves);
	
	/* sanity check */
	if (start >= end) {
		printf("Error: Frame range for Ghost F-Curve creation is inappropriate \n");
		return;
	}
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_SEL | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		FCurve *gcu= MEM_callocN(sizeof(FCurve), "Ghost FCurve");
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		ChannelDriver *driver= fcu->driver;
		FPoint *fpt;
		int cfra;		
		
		/* disable driver so that it don't muck up the sampling process */
		fcu->driver= NULL;
		
		/* create samples, but store them in a new curve 
		 *	- we cannot use fcurve_store_samples() as that will only overwrite the original curve 
		 */
		gcu->fpt= fpt= MEM_callocN(sizeof(FPoint)*(end-start+1), "Ghost FPoint Samples");
		gcu->totvert= end - start + 1;
		
		/* use the sampling callback at 1-frame intervals from start to end frames */
		for (cfra= start; cfra <= end; cfra++, fpt++) {
			float cfrae= BKE_nla_tweakedit_remap(adt, cfra, 0);
			
			fpt->vec[0]= cfrae;
			fpt->vec[1]= fcurve_samplingcb_evalcurve(fcu, NULL, cfrae);
		}
		
		/* set color of ghost curve 
		 *	- make the color slightly darker
		 */
		gcu->color[0]= fcu->color[0] - 0.07f;
		gcu->color[1]= fcu->color[1] - 0.07f;
		gcu->color[2]= fcu->color[2] - 0.07f;
		
		/* store new ghost curve */
		BLI_addtail(&sipo->ghostCurves, gcu);
		
		/* restore driver */
		fcu->driver= driver;
	}
	
	/* admin and redraws */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_create_ghostcurves_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	View2D *v2d;
	int start, end;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* ghost curves are snapshots of the visible portions of the curves, so set range to be the visible range */
	v2d= &ac.ar->v2d;
	start= (int)v2d->cur.xmin;
	end= (int)v2d->cur.xmax;
	
	/* bake selected curves into a ghost curve */
	create_ghost_curves(&ac, start, end);
	
	/* update this editor only */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_ghost_curves_create (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create Ghost Curves";
	ot->idname= "GRAPH_OT_ghost_curves_create";
	ot->description= "Create snapshot (Ghosts) of selected F-Curves as background aid for active Graph Editor.";
	
	/* api callbacks */
	ot->exec= graphkeys_create_ghostcurves_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	// todo: add props for start/end frames
}

/* ******************** Clear Ghost-Curves Operator *********************** */
/* This operator clears the 'ghost curves' for the active Graph Editor */

static int graphkeys_clear_ghostcurves_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	SpaceIpo *sipo;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	sipo= (SpaceIpo *)ac.sa->spacedata.first;
		
	/* if no ghost curves, don't do anything */
	if (sipo->ghostCurves.first == NULL)
		return OPERATOR_CANCELLED;
	
	/* free ghost curves */
	free_fcurves(&sipo->ghostCurves);
	
	/* update this editor only */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_ghost_curves_clear (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create Ghost Curves";
	ot->idname= "GRAPH_OT_ghost_curves_clear";
	ot->description= "Clear F-Curve snapshots (Ghosts) for active Graph Editor.";
	
	/* api callbacks */
	ot->exec= graphkeys_clear_ghostcurves_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************************************************************** */
/* GENERAL STUFF */

// TODO: insertkey

/* ******************** Click-Insert Keyframes Operator ************************* */

static int graphkeys_click_insert_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	bAnimListElem *ale;
	AnimData *adt;
	float frame, val;
	
	/* get animation context */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get active F-Curve 'anim-list-element' */
	ale= get_active_fcurve_channel(&ac);
	if (ELEM(NULL, ale, ale->data)) {
		if (ale) MEM_freeN(ale);
		return OPERATOR_CANCELLED;
	}
		
	/* get frame and value from props */
	frame= RNA_float_get(op->ptr, "frame");
	val= RNA_float_get(op->ptr, "value");
	
	/* apply inverse NLA-mapping to frame to get correct time in un-scaled action */
	adt= ANIM_nla_mapping_get(&ac, ale);
	frame= BKE_nla_tweakedit_remap(adt, frame, 0);
	
	/* insert keyframe on the specified frame + value */
	insert_vert_fcurve((FCurve *)ale->data, frame, val, 0);
	
	/* free temp data */
	MEM_freeN(ale);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	/* done */
	return OPERATOR_FINISHED;
}

static int graphkeys_click_insert_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	bAnimContext ac;
	ARegion *ar;
	View2D *v2d;
	int mval[2];
	float x, y;
	
	/* get animation context */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* store mouse coordinates in View2D space, into the operator's properties */
	ar= ac.ar;
	v2d= &ar->v2d;
	
	mval[0]= (evt->x - ar->winrct.xmin);
	mval[1]= (evt->y - ar->winrct.ymin);
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	
	RNA_float_set(op->ptr, "frame", x);
	RNA_float_set(op->ptr, "value", y);
	
	/* run exec now */
	return graphkeys_click_insert_exec(C, op);
}

void GRAPH_OT_click_insert (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Click-Insert Keyframes";
	ot->idname= "GRAPH_OT_click_insert";
	
	/* api callbacks */
	ot->invoke= graphkeys_click_insert_invoke;
	ot->exec= graphkeys_click_insert_exec;
	ot->poll= ED_operator_areaactive; // XXX active + editable poll
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float(ot->srna, "frame", 1.0f, -FLT_MAX, FLT_MAX, "Frame Number", "Frame to insert keyframe on", 0, 100);
	RNA_def_float(ot->srna, "value", 1.0f, -FLT_MAX, FLT_MAX, "Value", "Value for keyframe on", 0, 100);
}

/* ******************** Copy/Paste Keyframes Operator ************************* */
/* NOTE: the backend code for this is shared with the dopesheet editor */

static short copy_graph_keys (bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok=0;
	
	/* clear buffer first */
	free_anim_copybuf();
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* copy keyframes */
	ok= copy_animedit_keys(ac, &anim_data);
	
	/* clean up */
	BLI_freelistN(&anim_data);

	return ok;
}

static short paste_graph_keys (bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok=0;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* paste keyframes */
	ok= paste_animedit_keys(ac, &anim_data);
	
	/* clean up */
	BLI_freelistN(&anim_data);

	return ok;
}

/* ------------------- */

static int graphkeys_copy_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* copy keyframes */
	if (copy_graph_keys(&ac)) {	
		BKE_report(op->reports, RPT_ERROR, "No keyframes copied to keyframes copy/paste buffer");
		return OPERATOR_CANCELLED;
	}
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_copy (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Keyframes";
	ot->idname= "GRAPH_OT_copy";
	
	/* api callbacks */
	ot->exec= graphkeys_copy_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



static int graphkeys_paste_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* paste keyframes */
	if (paste_graph_keys(&ac)) {
		BKE_report(op->reports, RPT_ERROR, "No keyframes to paste");
		return OPERATOR_CANCELLED;
	}
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_paste (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste Keyframes";
	ot->idname= "GRAPH_OT_paste";
	
	/* api callbacks */
	ot->exec= graphkeys_paste_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Duplicate Keyframes Operator ************************* */

static void duplicate_graph_keys (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale= anim_data.first; ale; ale= ale->next) {
		duplicate_fcurve_keys((FCurve *)ale->key_data);
	}
	
	/* free filtered list */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_duplicate_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* duplicate keyframes */
	duplicate_graph_keys(&ac);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}

static int graphkeys_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	graphkeys_duplicate_exec(C, op);
	
	RNA_int_set(op->ptr, "mode", TFM_TRANSLATION);
	WM_operator_name_call(C, "TFM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_duplicate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Keyframes";
	ot->idname= "GRAPH_OT_duplicate";
	
	/* api callbacks */
	ot->invoke= graphkeys_duplicate_invoke;
	ot->exec= graphkeys_duplicate_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/* ******************** Delete Keyframes Operator ************************* */

static void delete_graph_keys (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale= anim_data.first; ale; ale= ale->next) {
		delete_fcurve_keys((FCurve *)ale->key_data); // XXX... this doesn't delete empty curves anymore
	}
	
	/* free filtered list */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_delete_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* delete keyframes */
	delete_graph_keys(&ac);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Keyframes";
	ot->idname= "GRAPH_OT_delete";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= graphkeys_delete_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Clean Keyframes Operator ************************* */

static void clean_graph_keys (bAnimContext *ac, float thresh)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_SEL | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and clean curves */
	for (ale= anim_data.first; ale; ale= ale->next)
		clean_fcurve((FCurve *)ale->key_data, thresh);
	
	/* free temp data */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_clean_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	float thresh;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get cleaning threshold */
	thresh= RNA_float_get(op->ptr, "threshold");
	
	/* clean keyframes */
	clean_graph_keys(&ac, thresh);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_clean (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clean Keyframes";
	ot->idname= "GRAPH_OT_clean";
	
	/* api callbacks */
	//ot->invoke=  // XXX we need that number popup for this! 
	ot->exec= graphkeys_clean_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float(ot->srna, "threshold", 0.001f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 1000.0f);
}

/* ******************** Bake F-Curve Operator *********************** */
/* This operator bakes the data of the selected F-Curves to F-Points */

/* Bake each F-Curve into a set of samples */
static void bake_graph_curves (bAnimContext *ac, int start, int end)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		ChannelDriver *driver= fcu->driver;
		
		/* disable driver so that it don't muck up the sampling process */
		fcu->driver= NULL;
		
		/* create samples */
		fcurve_store_samples(fcu, NULL, start, end, fcurve_samplingcb_evalcurve);
		
		/* restore driver */
		fcu->driver= driver;
	}
	
	/* admin and redraws */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_bake_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	Scene *scene= NULL;
	int start, end;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* for now, init start/end from preview-range extents */
	// TODO: add properties for this 
	scene= ac.scene;
	start= PSFRA;
	end= PEFRA;
	
	/* bake keyframes */
	bake_graph_curves(&ac, start, end);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_bake (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Bake Curve";
	ot->idname= "GRAPH_OT_bake";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm; // FIXME...
	ot->exec= graphkeys_bake_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	// todo: add props for start/end frames
}

/* ******************** Sample Keyframes Operator *********************** */
/* This operator 'bakes' the values of the curve into new keyframes between pairs
 * of selected keyframes. It is useful for creating keyframes for tweaking overlap.
 */

// XXX some of the common parts (with DopeSheet) should be unified in animation module...

/* little cache for values... */
typedef struct tempFrameValCache {
	float frame, val;
} tempFrameValCache;

/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
static void sample_graph_keys (bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		BezTriple *bezt, *start=NULL, *end=NULL;
		tempFrameValCache *value_cache, *fp;
		int sfra, range;
		int i, n;
		
		/* find selected keyframes... once pair has been found, add keyframes  */
		for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
			/* check if selected, and which end this is */
			if (BEZSELECTED(bezt)) {
				if (start) {
					/* set end */
					end= bezt;
					
					/* cache values then add keyframes using these values, as adding
					 * keyframes while sampling will affect the outcome...
					 */
					range= (int)( ceil(end->vec[1][0] - start->vec[1][0]) );
					sfra= (int)( floor(start->vec[1][0]) );
					
					if (range) {
						value_cache= MEM_callocN(sizeof(tempFrameValCache)*range, "IcuFrameValCache");
						
						/* 	sample values 	*/
						for (n=0, fp=value_cache; n<range && fp; n++, fp++) {
							fp->frame= (float)(sfra + n);
							fp->val= evaluate_fcurve(fcu, fp->frame);
						}
						
						/* 	add keyframes with these 	*/
						for (n=0, fp=value_cache; n<range && fp; n++, fp++) {
							insert_vert_fcurve(fcu, fp->frame, fp->val, 1);
						}
						
						/* free temp cache */
						MEM_freeN(value_cache);
						
						/* as we added keyframes, we need to compensate so that bezt is at the right place */
						bezt = fcu->bezt + i + range - 1;
						i += (range - 1);
					}
					
					/* bezt was selected, so it now marks the start of a whole new chain to search */
					start= bezt;
					end= NULL;
				}
				else {
					/* just set start keyframe */
					start= bezt;
					end= NULL;
				}
			}
		}
		
		/* recalculate channel's handles? */
		calchandles_fcurve(fcu);
	}
	
	/* admin and redraws */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_sample_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* sample keyframes */
	sample_graph_keys(&ac);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_sample (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sample Keyframes";
	ot->idname= "GRAPH_OT_sample";
	
	/* api callbacks */
	ot->exec= graphkeys_sample_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


/* ************************************************************************** */
/* SETTINGS STUFF */

/* ******************** Set Extrapolation-Type Operator *********************** */

/* defines for set extrapolation-type for selected keyframes tool */
EnumPropertyItem prop_graphkeys_expo_types[] = {
	{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", 0, "Constant Extrapolation", ""},
	{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", 0, "Linear Extrapolation", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for setting extrapolation mode for keyframes */
static void setexpo_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting mode per F-Curve */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->data;
		fcu->extend= mode;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_expo_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setexpo_graph_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_extrapolation_type (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Extrapolation";
	ot->idname= "GRAPH_OT_extrapolation_type";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= graphkeys_expo_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_graphkeys_expo_types, 0, "Type", "");
}

/* ******************** Set Interpolation-Type Operator *********************** */

/* this function is responsible for setting interpolation mode for keyframes */
static void setipo_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditFunc set_cb= ANIM_editkeyframes_ipo(mode);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple interpolation
	 * Note: we do not supply BeztEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale= anim_data.first; ale; ale= ale->next)
		ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_ipo_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setipo_graph_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_interpolation_type (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Interpolation";
	ot->idname= "GRAPH_OT_interpolation_type";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= graphkeys_ipo_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", beztriple_interpolation_mode_items, 0, "Type", "");
}

/* ******************** Set Handle-Type Operator *********************** */

/* this function is responsible for setting handle-type of selected keyframes */
static void sethandles_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditFunc set_cb= ANIM_editkeyframes_handles(mode);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting flags for handles 
	 * Note: we do not supply BeztEditData to the looper yet. Currently that's not necessary here...
	 */
	// XXX we might need to supply BeztEditData to get it to only affect selected handles
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (mode == -1) {	
			BeztEditFunc toggle_cb;
			
			/* check which type of handle to set (free or aligned) 
			 *	- check here checks for handles with free alignment already
			 */
			if (ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, NULL))
				toggle_cb= ANIM_editkeyframes_handles(HD_FREE);
			else
				toggle_cb= ANIM_editkeyframes_handles(HD_ALIGN);
				
			/* set handle-type */
			ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, toggle_cb, calchandles_fcurve);
		}
		else {
			/* directly set handle-type */
			ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);
		}
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_handletype_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	sethandles_graph_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_handletype (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Handle Type";
	ot->idname= "GRAPH_OT_handletype";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= graphkeys_handletype_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", beztriple_handle_type_items, 0, "Type", "");
}

/* ************************************************************************** */
/* TRANSFORM STUFF */

/* ***************** 'Euler Filter' Operator **************************** */
/* Euler filter tools (as seen in Maya), are necessary for working with 'baked'
 * rotation curves (with Euler rotations). The main purpose of such tools is to
 * resolve any discontinuities that may arise in the curves due to the clamping
 * of values to -180 degrees to 180 degrees.
 */

#if 0 // XXX this is not ready for the primetime yet
 
/* set of three euler-rotation F-Curves */
typedef struct tEulerFilter {
	ID *id;							/* ID-block which owns the channels */
	FCurve *fcu1, *fcu2, *fcu3;		/* x,y,z rotation curves */
	int i1, i2, i3;					/* current index for each curve */
} tEulerFilter;
 
static int graphkeys_euler_filter_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	ListBase eulers = {NULL, NULL};
	tEulerFilter *euf= NULL;	
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* The process is done in two passes:
	 * 	 1) Sets of three related rotation curves are identified from the selected channels,
	 *		and are stored as a single 'operation unit' for the next step
	 *	 2) Each set of three F-Curves is processed for each keyframe, with the values being
	 * 		processed according to one of several ways.
	 */
	 
	/* step 1: extract only the rotation f-curves */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		/* check if this is an appropriate F-Curve 
		 *	- only rotation curves
		 *	- for pchan curves, make sure we're only using the euler curves
		 */
		if (ELEM(0, fcu->rna_path, strstr(fcu->rna_path, "rotation")))
			continue;
		if (strstr(fcu->rna_path, "pose.pose_channels")) {
			if (strstr(fcu->rna_path, "euler_rotation") == 0)
				continue;
		}
		
		/* check if current set of 3-curves is suitable to add this curve to 
		 *	- things like whether the current set of curves is 'full' should be checked later only
		 *	- first check if id-blocks are compatible
		 */
		if ((euf) && (ale->id != euf->id)) {
			
		}
	}
	
	// XXX for now
	return OPERATOR_CANCELLED;
}
 
void GRAPH_OT_euler_filter (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Euler Filter";
	ot->idname= "GRAPH_OT_euler_filter";
	
	/* api callbacks */
	ot->exec= graphkeys_euler_filter_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

#endif // XXX this is not ready for the primetime yet

/* ***************** Jump to Selected Frames Operator *********************** */

/* snap current-frame indicator to 'average time' of selected keyframe */
static int graphkeys_framejump_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditData bed;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* init edit data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* loop over action data, averaging values */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_calc_average, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1); 
		}
		else
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_calc_average, NULL);
		
	}
	
	BLI_freelistN(&anim_data);
	
	/* set the new current frame value, based on the average time */
	if (bed.i1) {
		Scene *scene= ac.scene;
		CFRA= (int)floor((bed.f1 / bed.i1) + 0.5f);
	}
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_frame_jump (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Jump to Frame";
	ot->idname= "GRAPH_OT_frame_jump";
	
	/* api callbacks */
	ot->exec= graphkeys_framejump_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Snap Keyframes Operator *********************** */

/* defines for snap keyframes tool */
EnumPropertyItem prop_graphkeys_snap_types[] = {
	{GRAPHKEYS_SNAP_CFRA, "CFRA", 0, "Current frame", ""},
	{GRAPHKEYS_SNAP_NEAREST_FRAME, "NEAREST_FRAME", 0, "Nearest Frame", ""}, // XXX as single entry?
	{GRAPHKEYS_SNAP_NEAREST_SECOND, "NEAREST_SECOND", 0, "Nearest Second", ""}, // XXX as single entry?
	{GRAPHKEYS_SNAP_NEAREST_MARKER, "NEAREST_MARKER", 0, "Nearest Marker", ""},
	{GRAPHKEYS_SNAP_HORIZONTAL, "HORIZONTAL", 0, "Flatten Handles", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void snap_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc edit_cb;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing callbacks */
	edit_cb= ANIM_editkeyframes_snap(mode);
	
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.scene= ac->scene;
	if (mode == GRAPHKEYS_SNAP_NEAREST_MARKER) {
		bed.list.first= (ac->markers) ? ac->markers->first : NULL;
		bed.list.last= (ac->markers) ? ac->markers->last : NULL;
	}
	
	/* snap keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
	}
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_snap_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get snapping mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* snap keyframes */
	snap_graph_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_snap (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Snap Keys";
	ot->idname= "GRAPH_OT_snap";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= graphkeys_snap_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_graphkeys_snap_types, 0, "Type", "");
}

/* ******************** Mirror Keyframes Operator *********************** */

/* defines for mirror keyframes tool */
EnumPropertyItem prop_graphkeys_mirror_types[] = {
	{GRAPHKEYS_MIRROR_CFRA, "CFRA", 0, "Current frame", ""},
	{GRAPHKEYS_MIRROR_YAXIS, "YAXIS", 0, "Vertical Axis", ""},
	{GRAPHKEYS_MIRROR_XAXIS, "XAXIS", 0, "Horizontal Axis", ""},
	{GRAPHKEYS_MIRROR_MARKER, "MARKER", 0, "First Selected Marker", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for mirroring keyframes */
static void mirror_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc edit_cb;
	
	/* get beztriple editing callbacks */
	edit_cb= ANIM_editkeyframes_mirror(mode);
	
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.scene= ac->scene;
	
	/* for 'first selected marker' mode, need to find first selected marker first! */
	// XXX should this be made into a helper func in the API?
	if (mode == GRAPHKEYS_MIRROR_MARKER) {
		TimeMarker *marker= NULL;
		
		/* find first selected marker */
		if (ac->markers) {
			for (marker= ac->markers->first; marker; marker=marker->next) {
				if (marker->flag & SELECT) {
					break;
				}
			}
		}
		
		/* store marker's time (if available) */
		if (marker)
			bed.f1= (float)marker->frame;
		else
			return;
	}
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* mirror keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
	}
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_mirror_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get mirroring mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* mirror keyframes */
	mirror_graph_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_mirror (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mirror Keys";
	ot->idname= "GRAPH_OT_mirror";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= graphkeys_mirror_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_graphkeys_mirror_types, 0, "Type", "");
}

/* ******************** Smooth Keyframes Operator *********************** */

static int graphkeys_smooth_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE| ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* smooth keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* For now, we can only smooth by flattening handles AND smoothing curve values.
		 * Perhaps the mode argument could be removed, as that functionality is offerred through 
		 * Snap->Flatten Handles anyway.
		 */
		smooth_fcurve(ale->key_data);
	}
	BLI_freelistN(&anim_data);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_KEYFRAMES_VALUES);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_smooth (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Smooth Keys";
	ot->idname= "GRAPH_OT_smooth";
	
	/* api callbacks */
	ot->exec= graphkeys_smooth_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************************************************************** */
/* F-CURVE MODIFIERS */

/* ******************** Add F-Curve Modifier Operator *********************** */

static int graph_fmodifier_add_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	bAnimListElem *ale;
	FCurve *fcu;
	FModifier *fcm;
	short type;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
		// xxx call the raw methods here instead?
	ale= get_active_fcurve_channel(&ac);
	if (ale == NULL) 
		return OPERATOR_CANCELLED;
	
	fcu= (FCurve *)ale->data;
	MEM_freeN(ale);
	if (fcu == NULL) 
		return OPERATOR_CANCELLED;
		
	/* get type of modifier to add */
	type= RNA_enum_get(op->ptr, "type");
	
	/* add F-Modifier of specified type to active F-Curve, and make it the active one */
	fcm= fcurve_add_modifier(fcu, type);
	if (fcm)
		fcurve_set_active_modifier(fcu, fcm);
	else {
		BKE_report(op->reports, RPT_ERROR, "Modifier couldn't be added. See console for details.");
		return OPERATOR_CANCELLED;
	}
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_fmodifier_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add F-Curve Modifier";
	ot->idname= "GRAPH_OT_fmodifier_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= graph_fmodifier_add_exec;
	ot->poll= ED_operator_areaactive; // XXX need active F-Curve
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", fmodifier_type_items, 0, "Type", "");
}

/* ************************************************************************** */

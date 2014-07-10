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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_graph/graph_edit.c
 *  \ingroup spgraph
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLF_translation.h"

#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_report.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_markers.h"

#include "WM_api.h"
#include "WM_types.h"

#include "graph_intern.h"

/* ************************************************************************** */
/* KEYFRAME-RANGE STUFF */

/* *************************** Calculate Range ************************** */

/* Get the min/max keyframes*/
/* note: it should return total boundbox, filter for selection only can be argument... */
void get_graph_keyframe_extents(bAnimContext *ac, float *xmin, float *xmax, float *ymin, float *ymax, 
                                const bool do_sel_only, const bool include_handles)
{
	Scene *scene = ac->scene;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get data to filter, from Dopesheet */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* set large values initial values that will be easy to override */
	if (xmin) *xmin = 999999999.0f;
	if (xmax) *xmax = -999999999.0f;
	if (ymin) *ymin = 999999999.0f;
	if (ymax) *ymax = -999999999.0f;
	
	/* check if any channels to set range with */
	if (anim_data.first) {
		bool foundBounds = false;
		
		/* go through channels, finding max extents */
		for (ale = anim_data.first; ale; ale = ale->next) {
			AnimData *adt = ANIM_nla_mapping_get(ac, ale);
			FCurve *fcu = (FCurve *)ale->key_data;
			float txmin, txmax, tymin, tymax;
			float unitFac;
			
			/* get range */
			if (calc_fcurve_bounds(fcu, &txmin, &txmax, &tymin, &tymax, do_sel_only, include_handles)) {
				short mapping_flag = ANIM_get_normalization_flags(ac);

				/* apply NLA scaling */
				if (adt) {
					txmin = BKE_nla_tweakedit_remap(adt, txmin, NLATIME_CONVERT_MAP);
					txmax = BKE_nla_tweakedit_remap(adt, txmax, NLATIME_CONVERT_MAP);
				}
				
				/* apply unit corrections */
				unitFac = ANIM_unit_mapping_get_factor(ac->scene, ale->id, fcu, mapping_flag);
				tymin *= unitFac;
				tymax *= unitFac;
				
				/* try to set cur using these values, if they're more extreme than previously set values */
				if ((xmin) && (txmin < *xmin)) *xmin = txmin;
				if ((xmax) && (txmax > *xmax)) *xmax = txmax;
				if ((ymin) && (tymin < *ymin)) *ymin = tymin;
				if ((ymax) && (tymax > *ymax)) *ymax = tymax;
				
				foundBounds = true;
			}
		}
		
		/* ensure that the extents are not too extreme that view implodes...*/
		if (foundBounds) {
			if ((xmin && xmax) && (fabsf(*xmax - *xmin) < 0.1f)) *xmax += 0.1f;
			if ((ymin && ymax) && (fabsf(*ymax - *ymin) < 0.1f)) *ymax += 0.1f;
		}
		else {
			if (xmin) *xmin = (float)PSFRA;
			if (xmax) *xmax = (float)PEFRA;
			if (ymin) *ymin = -5;
			if (ymax) *ymax = 5;
		}
		
		/* free memory */
		ANIM_animdata_freelist(&anim_data);
	}
	else {
		/* set default range */
		if (ac->scene) {
			if (xmin) *xmin = (float)PSFRA;
			if (xmax) *xmax = (float)PEFRA;
		}
		else {
			if (xmin) *xmin = -5;
			if (xmax) *xmax = 100;
		}
		
		if (ymin) *ymin = -5;
		if (ymax) *ymax = 5;
	}
}

/* ****************** Automatic Preview-Range Operator ****************** */

static int graphkeys_previewrange_exec(bContext *C, wmOperator *UNUSED(op))
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
		scene = ac.scene;
	
	/* set the range directly */
	get_graph_keyframe_extents(&ac, &min, &max, NULL, NULL, false, false);
	scene->r.flag |= SCER_PRV_RANGE;
	scene->r.psfra = iroundf(min);
	scene->r.pefra = iroundf(max);
	
	/* set notifier that things have changed */
	// XXX err... there's nothing for frame ranges yet, but this should do fine too
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_previewrange_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Auto-Set Preview Range";
	ot->idname = "GRAPH_OT_previewrange_set";
	ot->description = "Automatically set Preview Range based on range of keyframes";
	
	/* api callbacks */
	ot->exec = graphkeys_previewrange_exec;
	ot->poll = ED_operator_graphedit_active; // XXX: unchecked poll to get fsamples working too, but makes modifier damage trickier...
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** View-All Operator ****************** */

static int graphkeys_viewall(bContext *C, const bool do_sel_only, const bool include_handles,
                             const int smooth_viewtx)
{
	bAnimContext ac;
	rctf cur_new;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* set the horizontal range, with an extra offset so that the extreme keys will be in view */
	get_graph_keyframe_extents(&ac,
	                           &cur_new.xmin, &cur_new.xmax,
	                           &cur_new.ymin, &cur_new.ymax,
	                           do_sel_only, include_handles);

	BLI_rctf_scale(&cur_new, 1.1f);
	
	UI_view2d_smooth_view(C, ac.ar, &cur_new, smooth_viewtx);
	
	return OPERATOR_FINISHED;
}

/* ......... */

static int graphkeys_viewall_exec(bContext *C, wmOperator *op)
{
	const bool include_handles = RNA_boolean_get(op->ptr, "include_handles");
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
	
	/* whole range */
	return graphkeys_viewall(C, false, include_handles, smooth_viewtx);
}
 
static int graphkeys_view_selected_exec(bContext *C, wmOperator *op)
{
	const bool include_handles = RNA_boolean_get(op->ptr, "include_handles");
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
	
	/* only selected */
	return graphkeys_viewall(C, true, include_handles, smooth_viewtx);
}

void GRAPH_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "GRAPH_OT_view_all";
	ot->description = "Reset viewable area to show full keyframe range";
	
	/* api callbacks */
	ot->exec = graphkeys_viewall_exec;
	ot->poll = ED_operator_graphedit_active; /* XXX: unchecked poll to get fsamples working too, but makes modifier damage trickier... */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	ot->prop = RNA_def_boolean(ot->srna, "include_handles", true, "Include Handles", 
	                           "Include handles of keyframes when calculating extents");
}

void GRAPH_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Selected";
	ot->idname = "GRAPH_OT_view_selected";
	ot->description = "Reset viewable area to show selected keyframe range";

	/* api callbacks */
	ot->exec = graphkeys_view_selected_exec;
	ot->poll = ED_operator_graphedit_active; /* XXX: unchecked poll to get fsamples working too, but makes modifier damage trickier... */

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	ot->prop = RNA_def_boolean(ot->srna, "include_handles", true, "Include Handles", 
	                           "Include handles of keyframes when calculating extents");
}

/* ******************** Create Ghost-Curves Operator *********************** */
/* This operator samples the data of the selected F-Curves to F-Points, storing them
 * as 'ghost curves' in the active Graph Editor
 */

/* Bake each F-Curve into a set of samples, and store as a ghost curve */
static void create_ghost_curves(bAnimContext *ac, int start, int end)
{	
	SpaceIpo *sipo = (SpaceIpo *)ac->sl;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* free existing ghost curves */
	free_fcurves(&sipo->ghostCurves);
	
	/* sanity check */
	if (start >= end) {
		printf("Error: Frame range for Ghost F-Curve creation is inappropriate\n");
		return;
	}
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		FCurve *gcu = MEM_callocN(sizeof(FCurve), "Ghost FCurve");
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		ChannelDriver *driver = fcu->driver;
		FPoint *fpt;
		float unitFac;
		int cfra;
		SpaceIpo *sipo = (SpaceIpo *) ac->sl;
		short mapping_flag = ANIM_get_normalization_flags(ac);
		
		/* disable driver so that it don't muck up the sampling process */
		fcu->driver = NULL;
		
		/* calculate unit-mapping factor */
		unitFac = ANIM_unit_mapping_get_factor(ac->scene, ale->id, fcu, mapping_flag);
		
		/* create samples, but store them in a new curve 
		 *	- we cannot use fcurve_store_samples() as that will only overwrite the original curve 
		 */
		gcu->fpt = fpt = MEM_callocN(sizeof(FPoint) * (end - start + 1), "Ghost FPoint Samples");
		gcu->totvert = end - start + 1;
		
		/* use the sampling callback at 1-frame intervals from start to end frames */
		for (cfra = start; cfra <= end; cfra++, fpt++) {
			float cfrae = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
			
			fpt->vec[0] = cfrae;
			fpt->vec[1] = fcurve_samplingcb_evalcurve(fcu, NULL, cfrae) * unitFac;
		}
		
		/* set color of ghost curve 
		 *	- make the color slightly darker
		 */
		gcu->color[0] = fcu->color[0] - 0.07f;
		gcu->color[1] = fcu->color[1] - 0.07f;
		gcu->color[2] = fcu->color[2] - 0.07f;
		
		/* store new ghost curve */
		BLI_addtail(&sipo->ghostCurves, gcu);
		
		/* restore driver */
		fcu->driver = driver;
	}
	
	/* admin and redraws */
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_create_ghostcurves_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	View2D *v2d;
	int start, end;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* ghost curves are snapshots of the visible portions of the curves, so set range to be the visible range */
	v2d = &ac.ar->v2d;
	start = (int)v2d->cur.xmin;
	end = (int)v2d->cur.xmax;
	
	/* bake selected curves into a ghost curve */
	create_ghost_curves(&ac, start, end);
	
	/* update this editor only */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_ghost_curves_create(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Create Ghost Curves";
	ot->idname = "GRAPH_OT_ghost_curves_create";
	ot->description = "Create snapshot (Ghosts) of selected F-Curves as background aid for active Graph Editor";
	
	/* api callbacks */
	ot->exec = graphkeys_create_ghostcurves_exec;
	ot->poll = graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	// todo: add props for start/end frames
}

/* ******************** Clear Ghost-Curves Operator *********************** */
/* This operator clears the 'ghost curves' for the active Graph Editor */

static int graphkeys_clear_ghostcurves_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	SpaceIpo *sipo;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	sipo = (SpaceIpo *)ac.sl;
		
	/* if no ghost curves, don't do anything */
	if (BLI_listbase_is_empty(&sipo->ghostCurves))
		return OPERATOR_CANCELLED;
	
	/* free ghost curves */
	free_fcurves(&sipo->ghostCurves);
	
	/* update this editor only */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_ghost_curves_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Ghost Curves";
	ot->idname = "GRAPH_OT_ghost_curves_clear";
	ot->description = "Clear F-Curve snapshots (Ghosts) for active Graph Editor";
	
	/* api callbacks */
	ot->exec = graphkeys_clear_ghostcurves_exec;
	ot->poll = ED_operator_graphedit_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************** */
/* GENERAL STUFF */

/* ******************** Insert Keyframes Operator ************************* */

/* defines for insert keyframes tool */
static EnumPropertyItem prop_graphkeys_insertkey_types[] = {
	{1, "ALL", 0, "All Channels", ""},
	{2, "SEL", 0, "Only Selected Channels", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void insert_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	ReportList *reports = ac->reports;
	Scene *scene = ac->scene;
	short flag = 0;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	if (mode == 2) filter |= ANIMFILTER_SEL;
	
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* init keyframing flag */
	flag = ANIM_get_keyframing_flags(scene, 1);
	
	/* insert keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		FCurve *fcu = (FCurve *)ale->key_data;
		float cfra;
		
		/* adjust current frame for NLA-mapping */
		if (adt)
			cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
		else 
			cfra = (float)CFRA;
			
		/* if there's an id */
		if (ale->id)
			insert_keyframe(reports, ale->id, NULL, ((fcu->grp) ? (fcu->grp->name) : (NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
		else
			insert_vert_fcurve(fcu, cfra, fcu->curval, 0);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}
	
	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_insertkey_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* which channels to affect? */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* insert keyframes */
	insert_graph_keys(&ac, mode);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_keyframe_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert Keyframes";
	ot->idname = "GRAPH_OT_keyframe_insert";
	ot->description = "Insert keyframes for the specified channels";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_insertkey_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_insertkey_types, 0, "Type", "");
}

/* ******************** Click-Insert Keyframes Operator ************************* */

static int graphkeys_click_insert_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	bAnimListElem *ale;
	AnimData *adt;
	FCurve *fcu;
	float frame, val;
	
	/* get animation context */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get active F-Curve 'anim-list-element' */
	ale = get_active_fcurve_channel(&ac);
	if (ELEM(NULL, ale, ale->data)) {
		if (ale) MEM_freeN(ale);
		return OPERATOR_CANCELLED;
	}
	fcu = ale->data;
	
	/* when there are F-Modifiers on the curve, only allow adding
	 * keyframes if these will be visible after doing so...
	 */
	if (fcurve_is_keyframable(fcu)) {
		ListBase anim_data;

		short mapping_flag = ANIM_get_normalization_flags(&ac);

		/* get frame and value from props */
		frame = RNA_float_get(op->ptr, "frame");
		val = RNA_float_get(op->ptr, "value");
		
		/* apply inverse NLA-mapping to frame to get correct time in un-scaled action */
		adt = ANIM_nla_mapping_get(&ac, ale);
		frame = BKE_nla_tweakedit_remap(adt, frame, NLATIME_CONVERT_UNMAP);
		
		/* apply inverse unit-mapping to value to get correct value for F-Curves */
		val *= ANIM_unit_mapping_get_factor(ac.scene, ale->id, fcu, mapping_flag | ANIM_UNITCONV_RESTORE);
		
		/* insert keyframe on the specified frame + value */
		insert_vert_fcurve(fcu, frame, val, 0);

		ale->update |= ANIM_UPDATE_DEPS;

		BLI_listbase_clear(&anim_data);
		BLI_addtail(&anim_data, ale);

		ANIM_animdata_update(&ac, &anim_data);
	}
	else {
		/* warn about why this can't happen */
		if (fcu->fpt)
			BKE_report(op->reports, RPT_ERROR, "Keyframes cannot be added to sampled F-Curves");
		else if (fcu->flag & FCURVE_PROTECTED)
			BKE_report(op->reports, RPT_ERROR, "Active F-Curve is not editable");
		else
			BKE_report(op->reports, RPT_ERROR, "Remove F-Modifiers from F-Curve to add keyframes");
	}
	
	/* free temp data */
	MEM_freeN(ale);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

static int graphkeys_click_insert_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
	ar = ac.ar;
	v2d = &ar->v2d;
	
	mval[0] = (event->x - ar->winrct.xmin);
	mval[1] = (event->y - ar->winrct.ymin);
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	
	RNA_float_set(op->ptr, "frame", x);
	RNA_float_set(op->ptr, "value", y);
	
	/* run exec now */
	return graphkeys_click_insert_exec(C, op);
}

void GRAPH_OT_click_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Click-Insert Keyframes";
	ot->idname = "GRAPH_OT_click_insert";
	ot->description = "Insert new keyframe at the cursor position for the active F-Curve";
	
	/* api callbacks */
	ot->invoke = graphkeys_click_insert_invoke;
	ot->exec = graphkeys_click_insert_exec;
	ot->poll = graphop_active_fcurve_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float(ot->srna, "frame", 1.0f, -FLT_MAX, FLT_MAX, "Frame Number", "Frame to insert keyframe on", 0, 100);
	RNA_def_float(ot->srna, "value", 1.0f, -FLT_MAX, FLT_MAX, "Value", "Value for keyframe on", 0, 100);
}

/* ******************** Copy/Paste Keyframes Operator ************************* */
/* NOTE: the backend code for this is shared with the dopesheet editor */

static short copy_graph_keys(bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok = 0;
	
	/* clear buffer first */
	free_anim_copybuf();
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* copy keyframes */
	ok = copy_animedit_keys(ac, &anim_data);
	
	/* clean up */
	ANIM_animdata_freelist(&anim_data);

	return ok;
}

static short paste_graph_keys(bAnimContext *ac,
                              const eKeyPasteOffset offset_mode, const eKeyMergeMode merge_mode)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok = 0;
	
	/* filter data 
	 * - First time we try to filter more strictly, allowing only selected channels 
	 *   to allow copying animation between channels
	 * - Second time, we loosen things up if nothing was found the first time, allowing
	 *   users to just paste keyframes back into the original curve again [#31670]
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	
	if (ANIM_animdata_filter(ac, &anim_data, filter | ANIMFILTER_SEL, ac->data, ac->datatype) == 0)
		ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* paste keyframes */
	ok = paste_animedit_keys(ac, &anim_data, offset_mode, merge_mode);

	/* clean up */
	ANIM_animdata_freelist(&anim_data);

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
	
	/* just return - no operator needed here (no changes) */
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Keyframes";
	ot->idname = "GRAPH_OT_copy";
	ot->description = "Copy selected keyframes to the copy/paste buffer";
	
	/* api callbacks */
	ot->exec = graphkeys_copy_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}



static int graphkeys_paste_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;

	const eKeyPasteOffset offset_mode = RNA_enum_get(op->ptr, "offset");
	const eKeyMergeMode merge_mode = RNA_enum_get(op->ptr, "merge");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* ac.reports by default will be the global reports list, which won't show warnings */
	ac.reports = op->reports;

	/* paste keyframes - non-zero return means an error occurred while trying to paste */
	if (paste_graph_keys(&ac, offset_mode, merge_mode)) {
		return OPERATOR_CANCELLED;
	}
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Keyframes";
	ot->idname = "GRAPH_OT_paste";
	ot->description = "Paste keyframes from copy/paste buffer for the selected channels, starting on the current frame";
	
	/* api callbacks */
//	ot->invoke = WM_operator_props_popup; // better wait for graph redo panel
	ot->exec = graphkeys_paste_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "offset", keyframe_paste_offset_items, KEYFRAME_PASTE_OFFSET_CFRA_START, "Offset", "Paste time offset of keys");
	RNA_def_enum(ot->srna, "merge", keyframe_paste_merge_items, KEYFRAME_PASTE_MERGE_MIX, "Type", "Method of merging pasted keys and existing");
}

/* ******************** Duplicate Keyframes Operator ************************* */

static void duplicate_graph_keys(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale = anim_data.first; ale; ale = ale->next) {
		duplicate_fcurve_keys((FCurve *)ale->key_data);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* duplicate keyframes */
	duplicate_graph_keys(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}

static int graphkeys_duplicate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	graphkeys_duplicate_exec(C, op);

	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Keyframes";
	ot->idname = "GRAPH_OT_duplicate";
	ot->description = "Make a copy of all selected keyframes";
	
	/* api callbacks */
	ot->invoke = graphkeys_duplicate_invoke;
	ot->exec = graphkeys_duplicate_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_enum(ot->srna, "mode", transform_mode_types, TFM_TRANSLATION, "Mode", "");
}

/* ******************** Delete Keyframes Operator ************************* */

static bool delete_graph_keys(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	bool changed_final = false;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		AnimData *adt = ale->adt;
		bool changed;
		
		/* delete selected keyframes only */
		changed = delete_fcurve_keys(fcu);

		if (changed) {
			ale->update |= ANIM_UPDATE_DEFAULT;
			changed_final = true;
		}
		
		/* Only delete curve too if it won't be doing anything anymore */
		if ((fcu->totvert == 0) &&
		    (list_has_suitable_fmodifier(&fcu->modifiers, 0, FMI_TYPE_GENERATE_CURVE) == 0) &&
		    (fcu->driver == NULL))
		{
			ANIM_fcurve_delete_from_animdata(ac, adt, fcu);
			ale->key_data = NULL;
		}
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);

	return changed_final;
}

/* ------------------- */

static int graphkeys_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* delete keyframes */
	if (!delete_graph_keys(&ac))
		return OPERATOR_CANCELLED;
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Keyframes";
	ot->idname = "GRAPH_OT_delete";
	ot->description = "Remove all selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = graphkeys_delete_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Clean Keyframes Operator ************************* */

static void clean_graph_keys(bAnimContext *ac, float thresh)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and clean curves */
	for (ale = anim_data.first; ale; ale = ale->next) {
		clean_fcurve((FCurve *)ale->key_data, thresh);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	thresh = RNA_float_get(op->ptr, "threshold");
	
	/* clean keyframes */
	clean_graph_keys(&ac, thresh);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_clean(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clean Keyframes";
	ot->idname = "GRAPH_OT_clean";
	ot->description = "Simplify F-Curves by removing closely spaced keyframes";
	
	/* api callbacks */
	//ot->invoke =  // XXX we need that number popup for this! 
	ot->exec = graphkeys_clean_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_float(ot->srna, "threshold", 0.001f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 1000.0f);
}

/* ******************** Bake F-Curve Operator *********************** */
/* This operator bakes the data of the selected F-Curves to F-Points */

/* Bake each F-Curve into a set of samples */
static void bake_graph_curves(bAnimContext *ac, int start, int end)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		ChannelDriver *driver = fcu->driver;
		
		/* disable driver so that it don't muck up the sampling process */
		fcu->driver = NULL;
		
		/* create samples */
		fcurve_store_samples(fcu, NULL, start, end, fcurve_samplingcb_evalcurve);
		
		/* restore driver */
		fcu->driver = driver;

		ale->update |= ANIM_UPDATE_DEPS;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_bake_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	Scene *scene = NULL;
	int start, end;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* for now, init start/end from preview-range extents */
	// TODO: add properties for this 
	scene = ac.scene;
	start = PSFRA;
	end = PEFRA;
	
	/* bake keyframes */
	bake_graph_curves(&ac, start, end);
	
	/* set notifier that keyframes have changed */
	// NOTE: some distinction between order/number of keyframes and type should be made?
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake Curve";
	ot->idname = "GRAPH_OT_bake";
	ot->description = "Bake selected F-Curves to a set of sampled points defining a similar curve";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm; // FIXME...
	ot->exec = graphkeys_bake_exec;
	ot->poll = graphop_selected_fcurve_poll; 
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	// todo: add props for start/end frames
}

#ifdef WITH_AUDASPACE

/* ******************** Sound Bake F-Curve Operator *********************** */
/* This operator bakes the given sound to the selected F-Curves */

/* ------------------- */

/* Custom data storage passed to the F-Sample-ing function,
 * which provides the necessary info for baking the sound
 */
typedef struct tSoundBakeInfo {
	float *samples;
	int length;
	int cfra;
} tSoundBakeInfo;

/* ------------------- */

/* Sampling callback used to determine the value from the sound to
 * save in the F-Curve at the specified frame
 */
static float fcurve_samplingcb_sound(FCurve *UNUSED(fcu), void *data, float evaltime)
{
	tSoundBakeInfo *sbi = (tSoundBakeInfo *)data;

	int position = evaltime - sbi->cfra;
	if ((position < 0) || (position >= sbi->length))
		return 0.0f;

	return sbi->samples[position];
}

/* ------------------- */

static int graphkeys_sound_bake_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	tSoundBakeInfo sbi;
	Scene *scene = NULL;
	int start, end;

	char path[FILE_MAX];

	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	RNA_string_get(op->ptr, "filepath", path);

	scene = ac.scene;    /* current scene */

	/* store necessary data for the baking steps */
	sbi.samples = AUD_readSoundBuffer(path,
	                                  RNA_float_get(op->ptr, "low"),
	                                  RNA_float_get(op->ptr, "high"),
	                                  RNA_float_get(op->ptr, "attack"),
	                                  RNA_float_get(op->ptr, "release"),
	                                  RNA_float_get(op->ptr, "threshold"),
	                                  RNA_boolean_get(op->ptr, "use_accumulate"),
	                                  RNA_boolean_get(op->ptr, "use_additive"),
	                                  RNA_boolean_get(op->ptr, "use_square"),
	                                  RNA_float_get(op->ptr, "sthreshold"),
	                                  FPS, &sbi.length);

	if (sbi.samples == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	/* determine extents of the baking */
	sbi.cfra = start = CFRA;
	end = CFRA + sbi.length - 1;

	/* filter anim channels */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

	/* loop through all selected F-Curves, replacing its data with the sound samples */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		
		/* sample the sound */
		fcurve_store_samples(fcu, &sbi, start, end, fcurve_samplingcb_sound);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	/* free sample data */
	free(sbi.samples);

	/* validate keyframes after editing */
	ANIM_animdata_update(&ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);

	/* set notifier that 'keyframes' have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

#else //WITH_AUDASPACE

static int graphkeys_sound_bake_exec(bContext *UNUSED(C), wmOperator *op)
{
	BKE_report(op->reports, RPT_ERROR, "Compiled without sound support");

	return OPERATOR_CANCELLED;
}

#endif //WITH_AUDASPACE

static int graphkeys_sound_bake_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	bAnimContext ac;

	/* verify editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	return WM_operator_filesel(C, op, event);
}

void GRAPH_OT_sound_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake Sound to F-Curves";
	ot->idname = "GRAPH_OT_sound_bake";
	ot->description = "Bakes a sound wave to selected F-Curves";

	/* api callbacks */
	ot->invoke = graphkeys_sound_bake_invoke;
	ot->exec = graphkeys_sound_bake_exec;
	ot->poll = graphop_selected_fcurve_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE | SOUNDFILE | MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
	RNA_def_float(ot->srna, "low", 0.0f, 0.0, 100000.0, "Lowest frequency",
	              "Cutoff frequency of a high-pass filter that is applied to the audio data", 0.1, 1000.00);
	RNA_def_float(ot->srna, "high", 100000.0, 0.0, 100000.0, "Highest frequency",
	              "Cutoff frequency of a low-pass filter that is applied to the audio data", 0.1, 1000.00);
	RNA_def_float(ot->srna, "attack", 0.005, 0.0, 2.0, "Attack time",
	              "Value for the hull curve calculation that tells how fast the hull curve can rise "
	              "(the lower the value the steeper it can rise)", 0.01, 0.1);
	RNA_def_float(ot->srna, "release", 0.2, 0.0, 5.0, "Release time",
	              "Value for the hull curve calculation that tells how fast the hull curve can fall "
	              "(the lower the value the steeper it can fall)", 0.01, 0.2);
	RNA_def_float(ot->srna, "threshold", 0.0, 0.0, 1.0, "Threshold",
	              "Minimum amplitude value needed to influence the hull curve", 0.01, 0.1);
	RNA_def_boolean(ot->srna, "use_accumulate", 0, "Accumulate",
	                "Only the positive differences of the hull curve amplitudes are summarized to produce the output");
	RNA_def_boolean(ot->srna, "use_additive", 0, "Additive",
	                "The amplitudes of the hull curve are summarized (or, when Accumulate is enabled, "
	                "both positive and negative differences are accumulated)");
	RNA_def_boolean(ot->srna, "use_square", 0, "Square",
	                "The output is a square curve (negative values always result in -1, and positive ones in 1)");
	RNA_def_float(ot->srna, "sthreshold", 0.1, 0.0, 1.0, "Square Threshold",
	              "Square only: all values with an absolute amplitude lower than that result in 0", 0.01, 0.1);
}

/* ******************** Sample Keyframes Operator *********************** */
/* This operator 'bakes' the values of the curve into new keyframes between pairs
 * of selected keyframes. It is useful for creating keyframes for tweaking overlap.
 */

/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
static void sample_graph_keys(bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale = anim_data.first; ale; ale = ale->next) {
		sample_fcurve((FCurve *)ale->key_data);

		ale->update |= ANIM_UPDATE_DEPS;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_sample_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* sample keyframes */
	sample_graph_keys(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Keyframes";
	ot->idname = "GRAPH_OT_sample";
	ot->description = "Add keyframes on every frame between the selected keyframes";
	
	/* api callbacks */
	ot->exec = graphkeys_sample_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ************************************************************************** */
/* SETTINGS STUFF */

/* ******************** Set Extrapolation-Type Operator *********************** */

/* defines for make/clear cyclic extrapolation tools */
#define MAKE_CYCLIC_EXPO    -1
#define CLEAR_CYCLIC_EXPO   -2

/* defines for set extrapolation-type for selected keyframes tool */
static EnumPropertyItem prop_graphkeys_expo_types[] = {
	{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", 0, "Constant Extrapolation", "Values on endpoint keyframes are held"},
	{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", 0, "Linear Extrapolation", "Straight-line slope of end segments are extended past the endpoint keyframes"},
	
	{MAKE_CYCLIC_EXPO, "MAKE_CYCLIC", 0, "Make Cyclic (F-Modifier)", "Add Cycles F-Modifier if one doesn't exist already"},
	{CLEAR_CYCLIC_EXPO, "CLEAR_CYCLIC", 0, "Clear Cyclic (F-Modifier)", "Remove Cycles F-Modifier if not needed anymore"},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for setting extrapolation mode for keyframes */
static void setexpo_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting mode per F-Curve */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		if (mode >= 0) {
			/* just set mode setting */
			fcu->extend = mode;

			ale->update |= ANIM_UPDATE_HANDLES;
		}
		else {
			/* shortcuts for managing Cycles F-Modifiers to make it easier to toggle cyclic animation 
			 * without having to go through FModifier UI in Graph Editor to do so
			 */
			if (mode == MAKE_CYCLIC_EXPO) {
				/* only add if one doesn't exist */
				if (list_has_suitable_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, -1) == 0) {
					// TODO: add some more preset versions which set different extrapolation options?
					add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES);
				}
			}
			else if (mode == CLEAR_CYCLIC_EXPO) {
				/* remove all the modifiers fitting this description */
				FModifier *fcm, *fcn = NULL;
				
				for (fcm = fcu->modifiers.first; fcm; fcm = fcn) {
					fcn = fcm->next;
					
					if (fcm->type == FMODIFIER_TYPE_CYCLES)
						remove_fmodifier(&fcu->modifiers, fcm);
				}
			}
		}

		ale->update |= ANIM_UPDATE_DEPS;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setexpo_graph_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_extrapolation_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Extrapolation";
	ot->idname = "GRAPH_OT_extrapolation_type";
	ot->description = "Set extrapolation mode for selected F-Curves";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_expo_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_expo_types, 0, "Type", "");
}

/* ******************** Set Interpolation-Type Operator *********************** */

/* this function is responsible for setting interpolation mode for keyframes */
static void setipo_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	KeyframeEditFunc set_cb = ANIM_editkeyframes_ipo(mode);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple interpolation
	 * Note: we do not supply KeyframeEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale = anim_data.first; ale; ale = ale->next) {
		ANIM_fcurve_keyframes_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);

		ale->update |= ANIM_UPDATE_DEFAULT_NOHANDLES;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setipo_graph_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_interpolation_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Interpolation";
	ot->idname = "GRAPH_OT_interpolation_type";
	ot->description = "Set interpolation mode for the F-Curve segments starting from the selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_ipo_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", beztriple_interpolation_mode_items, 0, "Type", "");
}

/* ******************** Set Easing Operator *********************** */

static void seteasing_graph_keys(bAnimContext *ac, short mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	KeyframeEditFunc set_cb = ANIM_editkeyframes_easing(mode);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple easing
	 * Note: we do not supply KeyframeEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale = anim_data.first; ale; ale = ale->next) {
		ANIM_fcurve_keyframes_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);

		ale->update |= ANIM_UPDATE_DEFAULT_NOHANDLES;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

static int graphkeys_easing_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get handle setting mode */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	seteasing_graph_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_easing_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Easing Type";
	ot->idname = "GRAPH_OT_easing_type";
	ot->description = "Set easing type for the F-Curve segments starting from the selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_easing_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", beztriple_interpolation_easing_items, 0, "Type", "");
}

/* ******************** Set Handle-Type Operator *********************** */

/* this function is responsible for setting handle-type of selected keyframes */
static void sethandles_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc edit_cb = ANIM_editkeyframes_handles(mode);
	KeyframeEditFunc sel_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting flags for handles 
	 * Note: we do not supply KeyframeEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		
		/* any selected keyframes for editing? */
		if (ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, sel_cb, NULL)) {
			/* change type of selected handles */
			ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, edit_cb, calchandles_fcurve);

			ale->update |= ANIM_UPDATE_DEFAULT;
		}
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	sethandles_graph_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_handle_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Handle Type";
	ot->idname = "GRAPH_OT_handle_type";
	ot->description = "Set type of handle for selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_handletype_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", keyframe_handle_type_items, 0, "Type", "");
}

/* ************************************************************************** */
/* TRANSFORM STUFF */

/* ***************** 'Euler Filter' Operator **************************** */
/* Euler filter tools (as seen in Maya), are necessary for working with 'baked'
 * rotation curves (with Euler rotations). The main purpose of such tools is to
 * resolve any discontinuities that may arise in the curves due to the clamping
 * of values to -180 degrees to 180 degrees.
 */

/* set of three euler-rotation F-Curves */
typedef struct tEulerFilter {
	struct tEulerFilter *next, *prev;
	
	ID *id;                         /* ID-block which owns the channels */
	FCurve *(fcurves[3]);           /* 3 Pointers to F-Curves */
	const char *rna_path;           /* Pointer to one of the RNA Path's used by one of the F-Curves */
} tEulerFilter;
 
static int graphkeys_euler_filter_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	ListBase eulers = {NULL, NULL};
	tEulerFilter *euf = NULL;
	int groups = 0, failed = 0;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* The process is done in two passes:
	 *   1) Sets of three related rotation curves are identified from the selected channels,
	 *		and are stored as a single 'operation unit' for the next step
	 *	 2) Each set of three F-Curves is processed for each keyframe, with the values being
	 *      processed as necessary
	 */
	 
	/* step 1: extract only the rotation f-curves */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		/* check if this is an appropriate F-Curve 
		 *	- only rotation curves
		 *	- for pchan curves, make sure we're only using the euler curves
		 */
		if (strstr(fcu->rna_path, "rotation_euler") == NULL)
			continue;
		else if (ELEM3(fcu->array_index, 0, 1, 2) == 0) {
			BKE_reportf(op->reports, RPT_WARNING,
			            "Euler Rotation F-Curve has invalid index (ID='%s', Path='%s', Index=%d)",
			            (ale->id) ? ale->id->name : TIP_("<No ID>"), fcu->rna_path, fcu->array_index);
			continue;
		}
		
		/* optimization: assume that xyz curves will always be stored consecutively,
		 * so if the paths or the ID's don't match up, then a curve needs to be added 
		 * to a new group
		 */
		if ((euf) && (euf->id == ale->id) && (strcmp(euf->rna_path, fcu->rna_path) == 0)) {
			/* this should be fine to add to the existing group then */
			euf->fcurves[fcu->array_index] = fcu;
		}
		else {
			/* just add to a new block */
			euf = MEM_callocN(sizeof(tEulerFilter), "tEulerFilter");
			BLI_addtail(&eulers, euf);
			groups++;
			
			euf->id = ale->id;
			euf->rna_path = fcu->rna_path; /* this should be safe, since we're only using it for a short time */
			euf->fcurves[fcu->array_index] = fcu;
		}

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	if (groups == 0) {
		ANIM_animdata_freelist(&anim_data);
		BKE_report(op->reports, RPT_WARNING, "No Euler Rotation F-Curves to fix up");
		return OPERATOR_CANCELLED;
	}
	
	/* step 2: go through each set of curves, processing the values at each keyframe 
	 *	- it is assumed that there must be a full set of keyframes at each keyframe position
	 */
	for (euf = eulers.first; euf; euf = euf->next) {
		int f;
		
		/* sanity check: ensure that there are enough F-Curves to work on in this group */
		/* TODO: also enforce assumption that there be a full set of keyframes at each position by ensuring that totvert counts are same? */
		if (ELEM3(NULL, euf->fcurves[0], euf->fcurves[1], euf->fcurves[2])) {
			/* report which components are missing */
			BKE_reportf(op->reports, RPT_WARNING,
			            "Missing %s%s%s component(s) of euler rotation for ID='%s' and RNA-Path='%s'",
			            (euf->fcurves[0] == NULL) ? "X" : "",
			            (euf->fcurves[1] == NULL) ? "Y" : "",
			            (euf->fcurves[2] == NULL) ? "Z" : "",
			            euf->id->name, euf->rna_path);
				
			/* keep track of number of failed sets, and carry on to next group */
			failed++;
			continue;
		}

		/* simple method: just treat any difference between keys of greater than 180 degrees as being a flip */
		/* FIXME: there are more complicated methods that will be needed to fix more cases than just some */
		for (f = 0; f < 3; f++) {
			FCurve *fcu = euf->fcurves[f];
			BezTriple *bezt, *prev;
			unsigned int i;
			
			/* skip if not enough vets to do a decent analysis of... */
			if (fcu->totvert <= 2)
				continue;
			
			/* prev follows bezt, bezt = "current" point to be fixed */
			/* our method depends on determining a "difference" from the previous vert */
			for (i = 1, prev = fcu->bezt, bezt = fcu->bezt + 1; i < fcu->totvert; i++, prev = bezt++) {
				const float sign = (prev->vec[1][1] > bezt->vec[1][1]) ? 1.0f : -1.0f;
				
				/* > 180 degree flip? */
				if ((sign * (prev->vec[1][1] - bezt->vec[1][1])) >= (float)M_PI) {
					/* 360 degrees to add/subtract frame value until difference is acceptably small that there's no more flip */
					const float fac = sign * 2.0f * (float)M_PI;
					
					while ((sign * (prev->vec[1][1] - bezt->vec[1][1])) >= (float)M_PI) {
						bezt->vec[0][1] += fac;
						bezt->vec[1][1] += fac;
						bezt->vec[2][1] += fac;
					}
				}
			}
		}
	}
	BLI_freelistN(&eulers);
	
	ANIM_animdata_update(&ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);

	/* updates + finishing warnings */
	if (failed == groups) {
		BKE_report(op->reports, RPT_ERROR, 
		           "No Euler Rotations could be corrected, ensure each rotation has keys for all components, "
		           "and that F-Curves for these are in consecutive XYZ order and selected");
		return OPERATOR_CANCELLED;
	}
	else {
		if (failed) {
			BKE_report(op->reports, RPT_ERROR,
			           "Some Euler Rotations could not be corrected due to missing/unselected/out-of-order F-Curves, "
			           "ensure each rotation has keys for all components, and that F-Curves for these are in "
			           "consecutive XYZ order and selected");
		}
		
		/* set notifier that keyframes have changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
		
		/* done at last */
		return OPERATOR_FINISHED;
	}
}
 
void GRAPH_OT_euler_filter(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Euler Discontinuity Filter";
	ot->idname = "GRAPH_OT_euler_filter";
	ot->description = "Fix large jumps and flips in the selected "
	                  "Euler Rotation F-Curves arising from rotation "
	                  "values being clipped when baking physics";
	
	/* api callbacks */
	ot->exec = graphkeys_euler_filter_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Jump to Selected Frames Operator *********************** */

static int graphkeys_framejump_poll(bContext *C)
{
	/* prevent changes during render */
	if (G.is_rendering)
		return 0;

	return graphop_visible_keyframes_poll(C);
}

/* snap current-frame indicator to 'average time' of selected keyframe */
static int graphkeys_framejump_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	KeyframeEditData ked;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* init edit data */
	memset(&ked, 0, sizeof(KeyframeEditData));
	
	/* loop over action data, averaging values */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
		short mapping_flag = ANIM_get_normalization_flags(&ac);
		KeyframeEditData current_ked;
		float unit_scale = ANIM_unit_mapping_get_factor(ac.scene, ale->id, ale->key_data, mapping_flag | ANIM_UNITCONV_ONLYKEYS);

		memset(&current_ked, 0, sizeof(current_ked));

		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keyframes_loop(&current_ked, ale->key_data, NULL, bezt_calc_average, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1); 
		}
		else
			ANIM_fcurve_keyframes_loop(&current_ked, ale->key_data, NULL, bezt_calc_average, NULL);

		ked.f1 += current_ked.f1;
		ked.i1 += current_ked.i1;
		ked.f2 += current_ked.f2 / unit_scale;
		ked.i2 += current_ked.i2;
	}
	
	ANIM_animdata_freelist(&anim_data);
	
	/* set the new current frame and cursor values, based on the average time and value */
	if (ked.i1) {
		SpaceIpo *sipo = (SpaceIpo *)ac.sl;
		Scene *scene = ac.scene;
		
		/* take the average values, rounding to the nearest int for the current frame */
		CFRA = iroundf(ked.f1 / ked.i1);
		SUBFRA = 0.f;
		sipo->cursorVal = ked.f2 / (float)ked.i1;
	}
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_frame_jump(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Jump to Keyframes";
	ot->idname = "GRAPH_OT_frame_jump";
	ot->description = "Place the cursor on the midpoint of selected keyframes";
	
	/* api callbacks */
	ot->exec = graphkeys_framejump_exec;
	ot->poll = graphkeys_framejump_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Snap Keyframes Operator *********************** */

/* defines for snap keyframes tool */
static EnumPropertyItem prop_graphkeys_snap_types[] = {
	{GRAPHKEYS_SNAP_CFRA, "CFRA", 0, "Current Frame",
	 "Snap selected keyframes to the current frame"},
	{GRAPHKEYS_SNAP_VALUE, "VALUE", 0, "Cursor Value",
	 "Set values of selected keyframes to the cursor value (Y/Horizontal component)"},
	{GRAPHKEYS_SNAP_NEAREST_FRAME, "NEAREST_FRAME", 0, "Nearest Frame",
	 "Snap selected keyframes to the nearest (whole) frame (use to fix accidental sub-frame offsets)"},
	{GRAPHKEYS_SNAP_NEAREST_SECOND, "NEAREST_SECOND", 0, "Nearest Second",
	 "Snap selected keyframes to the nearest second"},
	{GRAPHKEYS_SNAP_NEAREST_MARKER, "NEAREST_MARKER", 0, "Nearest Marker",
	 "Snap selected keyframes to the nearest marker"},
	{GRAPHKEYS_SNAP_HORIZONTAL, "HORIZONTAL", 0, "Flatten Handles",
	 "Flatten handles for a smoother transition"},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void snap_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked;
	KeyframeEditFunc edit_cb;
	float cursor_value = 0.0f;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing callbacks */
	edit_cb = ANIM_editkeyframes_snap(mode);
	
	memset(&ked, 0, sizeof(KeyframeEditData)); 
	ked.scene = ac->scene;
	if (mode == GRAPHKEYS_SNAP_NEAREST_MARKER) {
		ked.list.first = (ac->markers) ? ac->markers->first : NULL;
		ked.list.last = (ac->markers) ? ac->markers->last : NULL;
	}
	else if (mode == GRAPHKEYS_SNAP_VALUE) {
		SpaceIpo *sipo = (SpaceIpo *)ac->sl;
		cursor_value = (sipo) ? sipo->cursorVal : 0.0f;
	}
	
	/* snap keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		/* normalise cursor value (for normalised F-Curves display) */
		if (mode == GRAPHKEYS_SNAP_VALUE) {
			short mapping_flag = ANIM_get_normalization_flags(ac);
			float unit_scale = ANIM_unit_mapping_get_factor(ac->scene, ale->id, ale->key_data, mapping_flag);
			
			ked.f1 = cursor_value / unit_scale;
		}
		
		/* perform snapping */
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	mode = RNA_enum_get(op->ptr, "type");
	
	/* snap keyframes */
	snap_graph_keys(&ac, mode);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_snap(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Keys";
	ot->idname = "GRAPH_OT_snap";
	ot->description = "Snap selected keyframes to the chosen times/values";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_snap_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_snap_types, 0, "Type", "");
}

/* ******************** Mirror Keyframes Operator *********************** */

/* defines for mirror keyframes tool */
static EnumPropertyItem prop_graphkeys_mirror_types[] = {
	{GRAPHKEYS_MIRROR_CFRA, "CFRA", 0, "By Times over Current Frame",
	 "Flip times of selected keyframes using the current frame as the mirror line"},
	{GRAPHKEYS_MIRROR_VALUE, "VALUE", 0, "By Values over Cursor Value",
	 "Flip values of selected keyframes using the cursor value (Y/Horizontal component) as the mirror line"},
	{GRAPHKEYS_MIRROR_YAXIS, "YAXIS", 0, "By Times over Time=0",
	 "Flip times of selected keyframes, effectively reversing the order they appear in"},
	{GRAPHKEYS_MIRROR_XAXIS, "XAXIS", 0, "By Values over Value=0",
	 "Flip values of selected keyframes (i.e. negative values become positive, and vice versa)"},
	{GRAPHKEYS_MIRROR_MARKER, "MARKER", 0, "By Times over First Selected Marker",
	 "Flip times of selected keyframes using the first selected marker as the reference point"},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for mirroring keyframes */
static void mirror_graph_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked;
	KeyframeEditFunc edit_cb;
	float cursor_value = 0.0f;

	/* get beztriple editing callbacks */
	edit_cb = ANIM_editkeyframes_mirror(mode);
	
	memset(&ked, 0, sizeof(KeyframeEditData)); 
	ked.scene = ac->scene;
	
	/* for 'first selected marker' mode, need to find first selected marker first! */
	// XXX should this be made into a helper func in the API?
	if (mode == GRAPHKEYS_MIRROR_MARKER) {
		TimeMarker *marker = NULL;
		
		/* find first selected marker */
		marker = ED_markers_get_first_selected(ac->markers);
		
		/* store marker's time (if available) */
		if (marker)
			ked.f1 = (float)marker->frame;
		else
			return;
	}
	else if (mode == GRAPHKEYS_MIRROR_VALUE) {
		SpaceIpo *sipo = (SpaceIpo *)ac->sl;
		cursor_value = (sipo) ? sipo->cursorVal : 0.0f;
	}
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* mirror keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		/* apply unit corrections */
		if (mode == GRAPHKEYS_MIRROR_VALUE) {
			short mapping_flag = ANIM_get_normalization_flags(ac);
			float unit_scale = ANIM_unit_mapping_get_factor(ac->scene, ale->id, ale->key_data, mapping_flag | ANIM_UNITCONV_ONLYKEYS);
			
			ked.f1 = cursor_value * unit_scale;
		}
		
		/* perform actual mirroring */
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	mode = RNA_enum_get(op->ptr, "type");
	
	/* mirror keyframes */
	mirror_graph_keys(&ac, mode);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mirror Keys";
	ot->idname = "GRAPH_OT_mirror";
	ot->description = "Flip selected keyframes over the selected mirror line";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graphkeys_mirror_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_mirror_types, 0, "Type", "");
}

/* ******************** Smooth Keyframes Operator *********************** */

static int graphkeys_smooth_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* smooth keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		/* For now, we can only smooth by flattening handles AND smoothing curve values.
		 * Perhaps the mode argument could be removed, as that functionality is offered through
		 * Snap->Flatten Handles anyway.
		 */
		smooth_fcurve(ale->key_data);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(&ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Keys";
	ot->idname = "GRAPH_OT_smooth";
	ot->description = "Apply weighted moving means to make selected F-Curves less bumpy";
	
	/* api callbacks */
	ot->exec = graphkeys_smooth_exec;
	ot->poll = graphop_editable_keyframes_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************** */
/* F-CURVE MODIFIERS */

/* ******************** Add F-Modifier Operator *********************** */

static EnumPropertyItem *graph_fmodifier_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	int totitem = 0;
	int i = 0;

	if (C == NULL) {
		return fmodifier_type_items;
	}

	/* start from 1 to skip the 'Invalid' modifier type */
	for (i = 1; i < FMODIFIER_NUM_TYPES; i++) {
		FModifierTypeInfo *fmi = get_fmodifier_typeinfo(i);
		int index;

		/* check if modifier is valid for this context */
		if (fmi == NULL)
			continue;

		index = RNA_enum_from_value(fmodifier_type_items, fmi->type);
		RNA_enum_item_add(&item, &totitem, &fmodifier_type_items[index]);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int graph_fmodifier_add_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	short type;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get type of modifier to add */
	type = RNA_enum_get(op->ptr, "type");
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	if (RNA_boolean_get(op->ptr, "only_active"))
		filter |= ANIMFILTER_ACTIVE;  // FIXME: enforce in this case only a single channel to get handled?
	else
		filter |= (ANIMFILTER_SEL | ANIMFILTER_CURVE_VISIBLE);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* add f-modifier to each curve */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		FModifier *fcm;
		
		/* add F-Modifier of specified type to active F-Curve, and make it the active one */
		fcm = add_fmodifier(&fcu->modifiers, type);
		if (fcm) {
			set_active_fmodifier(&fcu->modifiers, fcm);
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "Modifier could not be added (see console for details)");
			break;
		}

		ale->update |= ANIM_UPDATE_DEPS;
	}

	ANIM_animdata_update(&ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
	
	/* set notifier that things have changed */
	// FIXME: this really isn't the best description for it...
	WM_event_add_notifier(C, NC_ANIMATION, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_fmodifier_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add F-Curve Modifier";
	ot->idname = "GRAPH_OT_fmodifier_add";
	ot->description = "Add F-Modifiers to the selected F-Curves";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = graph_fmodifier_add_exec;
	ot->poll = graphop_selected_fcurve_poll; 
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	prop = RNA_def_enum(ot->srna, "type", fmodifier_type_items, 0, "Type", "");
	RNA_def_enum_funcs(prop, graph_fmodifier_itemf);
	ot->prop = prop;

	RNA_def_boolean(ot->srna, "only_active", 1, "Only Active", "Only add F-Modifier to active F-Curve");
}

/* ******************** Copy F-Modifiers Operator *********************** */

static int graph_fmodifier_copy_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	bAnimListElem *ale;
	bool ok = false;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* clear buffer first */
	free_fmodifiers_copybuf();
	
	/* get the active F-Curve */
	ale = get_active_fcurve_channel(&ac);
	
	/* if this exists, call the copy F-Modifiers API function */
	if (ale && ale->data) {
		FCurve *fcu = (FCurve *)ale->data;

		/* TODO: when 'active' vs 'all' boolean is added, change last param! */
		ok = ANIM_fmodifiers_copy_to_buf(&fcu->modifiers, 0);

		/* free temp data now */
		MEM_freeN(ale);
	}
	
	/* successful or not? */
	if (ok == 0) {
		BKE_report(op->reports, RPT_ERROR, "No F-Modifiers available to be copied");
		return OPERATOR_CANCELLED;
	}
	else
		return OPERATOR_FINISHED;
}
 
void GRAPH_OT_fmodifier_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy F-Modifiers";
	ot->idname = "GRAPH_OT_fmodifier_copy";
	ot->description = "Copy the F-Modifier(s) of the active F-Curve";
	
	/* api callbacks */
	ot->exec = graph_fmodifier_copy_exec;
	ot->poll = graphop_active_fcurve_poll; 
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	//ot->prop = RNA_def_boolean(ot->srna, "all", 1, "All F-Modifiers", "Copy all the F-Modifiers, instead of just the active one");
}

/* ******************** Paste F-Modifiers Operator *********************** */

static int graph_fmodifier_paste_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter, ok = 0;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* paste modifiers */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		int tot;

		/* TODO: do we want to replace existing modifiers? add user pref for that! */
		tot = ANIM_fmodifiers_paste_from_buf(&fcu->modifiers, 0);

		if (tot) {
			ale->update |= ANIM_UPDATE_DEPS;
		}

		ok += tot;
	}

	if (ok) {
		ANIM_animdata_update(&ac, &anim_data);
	}
	ANIM_animdata_freelist(&anim_data);
	
	/* successful or not? */
	if (ok) {

		/* set notifier that keyframes have changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
		
		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No F-Modifiers to paste");
		return OPERATOR_CANCELLED;
	}
}
 
void GRAPH_OT_fmodifier_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste F-Modifiers";
	ot->idname = "GRAPH_OT_fmodifier_paste";
	ot->description = "Add copied F-Modifiers to the selected F-Curves";
	
	/* api callbacks */
	ot->exec = graph_fmodifier_paste_exec;
	ot->poll = graphop_active_fcurve_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************** */

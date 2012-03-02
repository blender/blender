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
 * The Original Code is Copyright (C) 2008 Blender Foundation
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_graph/graph_select.c
 *  \ingroup spgraph
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_context.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"

#include "WM_api.h"
#include "WM_types.h"

#include "graph_intern.h"


/* ************************************************************************** */
/* KEYFRAMES STUFF */

/* ******************** Deselect All Operator ***************************** */
/* This operator works in one of three ways:
 *	1) (de)select all (AKEY) - test if select all or deselect all
 *	2) invert all (CTRL-IKEY) - invert selection of all keyframes
 *	3) (de)select all - no testing is done; only for use internal tools as normal function...
 */

/* Deselects keyframes in the Graph Editor
 *	- This is called by the deselect all operator, as well as other ones!
 *
 * 	- test: check if select or deselect all
 *	- sel: how to select keyframes 
 *		0 = deselect
 *		1 = select
 *		2 = invert
 *	- do_channels: whether to affect selection status of channels
 */
static void deselect_graph_keys (bAnimContext *ac, short test, short sel, short do_channels)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sl;
	KeyframeEditData ked= {{NULL}};
	KeyframeEditFunc test_cb, sel_cb;
	
	/* determine type-based settings */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	
	/* filter data */
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* init BezTriple looping data */
	test_cb= ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			if (ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, test_cb, NULL)) {
				sel= SELECT_SUBTRACT;
				break;
			}
		}
	}
	
	/* convert sel to selectmode, and use that to get editor */
	sel_cb= ANIM_editkeyframes_select(sel);
	
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* Keyframes First */
		ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, sel_cb, NULL);
		
		/* affect channel selection status? */
		if (do_channels) {
			/* only change selection of channel when the visibility of keyframes doesn't depend on this */
			if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0) {
				/* deactivate the F-Curve, and deselect if deselecting keyframes.
				 * otherwise select the F-Curve too since we've selected all the keyframes
				 */
				if (sel == SELECT_SUBTRACT) 
					fcu->flag &= ~FCURVE_SELECTED;
				else
					fcu->flag |= FCURVE_SELECTED;
			}
			
			/* always deactivate all F-Curves if we perform batch ops for selection */
			fcu->flag &= ~FCURVE_ACTIVE;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behavior - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		deselect_graph_keys(&ac, 0, SELECT_INVERT, TRUE);
	else
		deselect_graph_keys(&ac, 1, SELECT_ADD, TRUE);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_select_all_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "GRAPH_OT_select_all_toggle";
	ot->description= "Toggle selection of all keyframes";
	
	/* api callbacks */
	ot->exec= graphkeys_deselectall_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	ot->prop= RNA_def_boolean(ot->srna, "invert", 0, "Invert", "");
}

/* ******************** Border Select Operator **************************** */
/* This operator currently works in one of three ways:
 *	-> BKEY 	- 1) all keyframes within region are selected (validation with BEZT_OK_REGION)
 *	-> ALT-BKEY - depending on which axis of the region was larger...
 *		-> 2) x-axis, so select all frames within frame range (validation with BEZT_OK_FRAMERANGE)
 *		-> 3) y-axis, so select all frames within channels that region included (validation with BEZT_OK_VALUERANGE)
 */

/* Borderselect only selects keyframes now, as overshooting handles often get caught too,
 * which means that they may be inadvertantly moved as well. However, incl_handles overrides
 * this, and allow handles to be considered independently too.
 * Also, for convenience, handles should get same status as keyframe (if it was within bounds).
 */
static void borderselect_graphkeys (bAnimContext *ac, rcti rect, short mode, short selectmode, short incl_handles)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter, mapping_flag;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sl;
	KeyframeEditData ked;
	KeyframeEditFunc ok_cb, select_cb;
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	
	/* convert mouse coordinates to frame ranges and channel coordinates corrected for view pan/zoom */
	UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing/validation funcs  */
	select_cb= ANIM_editkeyframes_select(selectmode);
	ok_cb= ANIM_editkeyframes_ok(mode);
	
	/* init editing data */
	memset(&ked, 0, sizeof(KeyframeEditData));
	ked.data= &rectf;
	
	/* treat handles separately? */
	if (incl_handles) {
		ked.iterflags |= KEYFRAME_ITER_INCL_HANDLES;
		mapping_flag= 0;
	}
	else
		mapping_flag= ANIM_UNITCONV_ONLYKEYS;
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* apply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, mapping_flag);
		
		/* apply NLA mapping to all the keyframes, since it's easier than trying to
		 * guess when a callback might use something different
		 */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, incl_handles==0);
		
		/* set horizontal range (if applicable) 
		 * NOTE: these values are only used for x-range and y-range but not region 
		 * 		(which uses ked.data, i.e. rectf)
		 */
		if (mode != BEZT_OK_VALUERANGE) {
			ked.f1= rectf.xmin;
			ked.f2= rectf.xmax;
		}
		else {
			ked.f1= rectf.ymin;
			ked.f2= rectf.ymax;
		}
		
		/* firstly, check if any keyframes will be hit by this */
		if (ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, ok_cb, NULL)) {
			/* select keyframes that are in the appropriate places */
			ANIM_fcurve_keyframes_loop(&ked, fcu, ok_cb, select_cb, NULL);
			
			/* only change selection of channel when the visibility of keyframes doesn't depend on this */
			if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0) {
				/* select the curve too now that curve will be touched */
				if (selectmode == SELECT_ADD)
					fcu->flag |= FCURVE_SELECTED;
			}
		}
		
		/* un-apply NLA mapping from all the keyframes */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, incl_handles==0);
			
		/* unapply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, ANIM_UNITCONV_RESTORE|mapping_flag);
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short mode=0, selectmode=0;
	short incl_handles;
	int extend;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	/* clear all selection if not extending selection */
	extend= RNA_boolean_get(op->ptr, "extend");
	if (!extend)
		deselect_graph_keys(&ac, 1, SELECT_SUBTRACT, TRUE);

	/* get select mode 
	 *	- 'gesture_mode' from the operator specifies how to select
	 *	- 'include_handles' from the operator specifies whether to include handles in the selection
	 */
	if (RNA_int_get(op->ptr, "gesture_mode")==GESTURE_MODAL_SELECT)
		selectmode= SELECT_ADD;
	else
		selectmode= SELECT_SUBTRACT;
		
	incl_handles = RNA_boolean_get(op->ptr, "include_handles");
	
	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	/* selection 'mode' depends on whether borderselect region only matters on one axis */
	if (RNA_boolean_get(op->ptr, "axis_range")) {
		/* mode depends on which axis of the range is larger to determine which axis to use 
		 *	- checking this in region-space is fine, as it's fundamentally still going to be a different rect size
		 *	- the frame-range select option is favored over the channel one (x over y), as frame-range one is often
		 *	  used for tweaking timing when "blocking", while channels is not that useful...
		 */
		if ((rect.xmax - rect.xmin) >= (rect.ymax - rect.ymin))
			mode= BEZT_OK_FRAMERANGE;
		else
			mode= BEZT_OK_VALUERANGE;
	}
	else 
		mode= BEZT_OK_REGION;
	
	/* apply borderselect action */
	borderselect_graphkeys(&ac, rect, mode, selectmode, incl_handles);
	
	/* send notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
} 

void GRAPH_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "GRAPH_OT_select_border";
	ot->description= "Select all keyframes within the specified region";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= graphkeys_borderselect_exec;
	ot->modal= WM_border_select_modal;
	ot->cancel= WM_border_select_cancel;
	
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, TRUE);
	
	ot->prop= RNA_def_boolean(ot->srna, "axis_range", 0, "Axis Range", "");
	RNA_def_boolean(ot->srna, "include_handles", 0, "Include Handles", "Are handles tested individually against the selection criteria");
}

/* ******************** Column Select Operator **************************** */
/* This operator works in one of four ways:
 *	- 1) select all keyframes in the same frame as a selected one  (KKEY)
 *	- 2) select all keyframes in the same frame as the current frame marker (CTRL-KKEY)
 *	- 3) select all keyframes in the same frame as a selected markers (SHIFT-KKEY)
 *	- 4) select all keyframes that occur between selected markers (ALT-KKEY)
 */

/* defines for column-select mode */
static EnumPropertyItem prop_column_select_types[] = {
	{GRAPHKEYS_COLUMNSEL_KEYS, "KEYS", 0, "On Selected Keyframes", ""},
	{GRAPHKEYS_COLUMNSEL_CFRA, "CFRA", 0, "On Current Frame", ""},
	{GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN, "MARKERS_COLUMN", 0, "On Selected Markers", ""},
	{GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN, "MARKERS_BETWEEN", 0, "Between Min/Max Selected Markers", ""},
	{0, NULL, 0, NULL, NULL}
};

/* ------------------- */ 

/* Selects all visible keyframes between the specified markers */
/* TODO, this is almost an _exact_ duplicate of a function of the same name in action_select.c
 * should de-duplicate - campbell */
static void markers_selectkeys_between (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc ok_cb, select_cb;
	KeyframeEditData ked= {{NULL}};
	float min, max;
	
	/* get extreme markers */
	ED_markers_get_minmax(ac->markers, 1, &min, &max);
	min -= 0.5f;
	max += 0.5f;
	
	/* get editing funcs + data */
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb= ANIM_editkeyframes_select(SELECT_ADD);

	ked.f1= min;
	ked.f2= max;
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* select keys in-between */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);

		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else {
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}


/* Selects all visible keyframes in the same frames as the specified elements */
static void columnselect_graph_keys (bAnimContext *ac, short mode)
{
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene= ac->scene;
	CfraElem *ce;
	KeyframeEditFunc select_cb, ok_cb;
	KeyframeEditData ked;
	
	/* initialize keyframe editing data */
	memset(&ked, 0, sizeof(KeyframeEditData));
	
	/* build list of columns */
	switch (mode) {
		case GRAPHKEYS_COLUMNSEL_KEYS: /* list of selected keys */
			filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
			ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
			
			for (ale= anim_data.first; ale; ale= ale->next)
				ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, bezt_to_cfraelem, NULL);
			
			BLI_freelistN(&anim_data);
			break;
			
		case GRAPHKEYS_COLUMNSEL_CFRA: /* current frame */
			/* make a single CfraElem for storing this */
			ce= MEM_callocN(sizeof(CfraElem), "cfraElem");
			BLI_addtail(&ked.list, ce);
			
			ce->cfra= (float)CFRA;
			break;
			
		case GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN: /* list of selected markers */
			ED_markers_make_cfra_list(ac->markers, &ked.list, SELECT);
			break;
			
		default: /* invalid option */
			return;
	}
	
	/* set up BezTriple edit callbacks */
	select_cb= ANIM_editkeyframes_select(SELECT_ADD);
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* loop over cfraelems (stored in the KeyframeEditData->list)
		 *	- we need to do this here, as we can apply fewer NLA-mapping conversions
		 */
		for (ce= ked.list.first; ce; ce= ce->next) {
			/* set frame for validation callback to refer to */
			ked.f1= BKE_nla_tweakedit_remap(adt, ce->cfra, NLATIME_CONVERT_UNMAP);

			/* select elements with frame number matching cfraelem */
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
		}
	}
	
	/* free elements */
	BLI_freelistN(&ked.list);
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_columnselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* action to take depends on the mode */
	mode= RNA_enum_get(op->ptr, "mode");
	
	if (mode == GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN)
		markers_selectkeys_between(&ac);
	else
		columnselect_graph_keys(&ac, mode);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_select_column (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "GRAPH_OT_select_column";
	ot->description= "Select all keyframes on the specified frame(s)";
	
	/* api callbacks */
	ot->exec= graphkeys_columnselect_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	ot->prop= RNA_def_enum(ot->srna, "mode", prop_column_select_types, 0, "Mode", "");
}

/* ******************** Select Linked Operator *********************** */

static int graphkeys_select_linked_exec (bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc ok_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	KeyframeEditFunc sel_cb = ANIM_editkeyframes_select(SELECT_ADD);
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* loop through all of the keys and select additional keyframes based on these */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* check if anything selected? */
		if (ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, ok_cb, NULL)) {
			/* select every keyframe in this curve then */
			ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, sel_cb, NULL);
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_select_linked (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname= "GRAPH_OT_select_linked";
	ot->description = "Select keyframes occurring in the same F-Curves as selected ones";
	
	/* api callbacks */
	ot->exec= graphkeys_select_linked_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
}

/* ******************** Select More/Less Operators *********************** */

/* Common code to perform selection */
static void select_moreless_graph_keys (bAnimContext *ac, short mode)
{
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked;
	KeyframeEditFunc build_cb;
	
	
	/* init selmap building data */
	build_cb= ANIM_editkeyframes_buildselmap(mode);
	memset(&ked, 0, sizeof(KeyframeEditData)); 
	
	/* loop through all of the keys and select additional keyframes based on these */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* only continue if F-Curve has keyframes */
		if (fcu->bezt == NULL)
			continue;
		
		/* build up map of whether F-Curve's keyframes should be selected or not */
		ked.data= MEM_callocN(fcu->totvert, "selmap graphEdit");
		ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, build_cb, NULL);
		
		/* based on this map, adjust the selection status of the keyframes */
		ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, bezt_selmap_flush, NULL);
		
		/* free the selmap used here */
		MEM_freeN(ked.data);
		ked.data= NULL;
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ----------------- */

static int graphkeys_select_more_exec (bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* perform select changes */
	select_moreless_graph_keys(&ac, SELMAP_MORE);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_select_more (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname= "GRAPH_OT_select_more";
	ot->description = "Select keyframes beside already selected ones";
	
	/* api callbacks */
	ot->exec= graphkeys_select_more_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
}

/* ----------------- */

static int graphkeys_select_less_exec (bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* perform select changes */
	select_moreless_graph_keys(&ac, SELMAP_LESS);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void GRAPH_OT_select_less (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname= "GRAPH_OT_select_less";
	ot->description = "Deselect keyframes on ends of selection islands";
	
	/* api callbacks */
	ot->exec= graphkeys_select_less_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
}

/* ******************** Select Left/Right Operator ************************* */
/* Select keyframes left/right of the current frame indicator */

/* defines for left-right select tool */
static EnumPropertyItem prop_graphkeys_leftright_select_types[] = {
	{GRAPHKEYS_LRSEL_TEST, "CHECK", 0, "Check if Select Left or Right", ""},
	{GRAPHKEYS_LRSEL_LEFT, "LEFT", 0, "Before current frame", ""},
	{GRAPHKEYS_LRSEL_RIGHT, "RIGHT", 0, "After current frame", ""},
	{0, NULL, 0, NULL, NULL}
};

/* --------------------------------- */

static void graphkeys_select_leftright (bAnimContext *ac, short leftright, short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc ok_cb, select_cb;
	KeyframeEditData ked= {{NULL}};
	Scene *scene= ac->scene;
	
	/* if select mode is replace, deselect all keyframes (and channels) first */
	if (select_mode==SELECT_REPLACE) {
		select_mode= SELECT_ADD;
		
		/* - deselect all other keyframes, so that just the newly selected remain
		 * - channels aren't deselected, since we don't re-select any as a consequence
		 */
		deselect_graph_keys(ac, 0, SELECT_SUBTRACT, FALSE);
	}
	
	/* set callbacks and editing data */
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb= ANIM_editkeyframes_select(select_mode);
	
	if (leftright == GRAPHKEYS_LRSEL_LEFT) {
		ked.f1 = MINAFRAMEF;
		ked.f2 = (float)(CFRA + 0.1f);
	} 
	else {
		ked.f1 = (float)(CFRA - 0.1f);
		ked.f2 = MAXFRAMEF;
	}
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
		
	/* select keys */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
	}

	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ----------------- */

static int graphkeys_select_leftright_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short leftright = RNA_enum_get(op->ptr, "mode");
	short selectmode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
		
	/* if "test" mode is set, we don't have any info to set this with */
	if (leftright == GRAPHKEYS_LRSEL_TEST)
		return OPERATOR_CANCELLED;
	
	/* do the selecting now */
	graphkeys_select_leftright(&ac, leftright, selectmode);
	
	/* set notifier that keyframe selection (and channels too) have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|ND_ANIMCHAN|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

static int graphkeys_select_leftright_invoke (bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	short leftright = RNA_enum_get(op->ptr, "mode");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* handle mode-based testing */
	if (leftright == GRAPHKEYS_LRSEL_TEST) {
		Scene *scene= ac.scene;
		ARegion *ar= ac.ar;
		View2D *v2d= &ar->v2d;
		float x;

		/* determine which side of the current frame mouse is on */
		UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &x, NULL);
		if (x < CFRA)
			RNA_enum_set(op->ptr, "mode", GRAPHKEYS_LRSEL_LEFT);
		else 	
			RNA_enum_set(op->ptr, "mode", GRAPHKEYS_LRSEL_RIGHT);
	}
	
	/* perform selection */
	return graphkeys_select_leftright_exec(C, op);
}

void GRAPH_OT_select_leftright (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Left/Right";
	ot->idname= "GRAPH_OT_select_leftright";
	ot->description= "Select keyframes to the left or the right of the current frame";
	
	/* api callbacks  */
	ot->invoke=	graphkeys_select_leftright_invoke;
	ot->exec= graphkeys_select_leftright_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	ot->prop= RNA_def_enum(ot->srna, "mode", prop_graphkeys_leftright_select_types, GRAPHKEYS_LRSEL_TEST, "Mode", "");
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
}

/* ******************** Mouse-Click Select Operator *********************** */
/* This operator works in one of three ways:
 *	- 1) keyframe under mouse - no special modifiers
 *	- 2) all keyframes on the same side of current frame indicator as mouse - ALT modifier
 *	- 3) column select all keyframes in frame under mouse - CTRL modifier
 *
 * In addition to these basic options, the SHIFT modifier can be used to toggle the 
 * selection mode between replacing the selection (without) and inverting the selection (with).
 */

/* temp info for caching handle vertices close */
typedef struct tNearestVertInfo {
	struct tNearestVertInfo *next, *prev;
	
	FCurve *fcu;		/* F-Curve that keyframe comes from */
	
	BezTriple *bezt;	/* keyframe to consider */
	FPoint *fpt;		/* sample point to consider */
	
	short hpoint;		/* the handle index that we hit (eHandleIndex) */
	short sel;			/* whether the handle is selected or not */
	int dist;			/* distance from mouse to vert */
} tNearestVertInfo;

/* Tags for the type of graph vert that we have */
typedef enum eGraphVertIndex {
	NEAREST_HANDLE_LEFT	= -1,
	NEAREST_HANDLE_KEY,
	NEAREST_HANDLE_RIGHT
} eGraphVertIndex; 

/* Tolerance for absolute radius (in pixels) of the vert from the cursor to use */
// TODO: perhaps this should depend a bit on the size that the user set the vertices to be?
#define GVERTSEL_TOL	10

/* ....... */

/* check if its ok to select a handle */
// XXX also need to check for int-values only?
static int fcurve_handle_sel_check(SpaceIpo *sipo, BezTriple *bezt)
{
	if (sipo->flag & SIPO_NOHANDLES) return 0;
	if ((sipo->flag & SIPO_SELVHANDLESONLY) && BEZSELECTED(bezt)==0) return 0;
	return 1;
}

/* check if the given vertex is within bounds or not */
// TODO: should we return if we hit something?
static void nearest_fcurve_vert_store (ListBase *matches, View2D *v2d, FCurve *fcu, BezTriple *bezt, FPoint *fpt, short hpoint, const int mval[2])
{
	/* Keyframes or Samples? */
	if (bezt) {
		int screen_co[2], dist;
		
		/* convert from data-space to screen coordinates 
		 * NOTE: hpoint+1 gives us 0,1,2 respectively for each handle, 
		 * 	needed to access the relevant vertex coordinates in the 3x3 
		 * 	'vec' matrix
		 */
		UI_view2d_view_to_region(v2d, bezt->vec[hpoint+1][0], bezt->vec[hpoint+1][1], &screen_co[0], &screen_co[1]);
		
		/* check if distance from mouse cursor to vert in screen space is within tolerance */
			// XXX: inlined distance calculation, since we cannot do this on ints using the math lib...
		//dist = len_v2v2(mval, screen_co);
		dist = sqrt((mval[0] - screen_co[0])*(mval[0] - screen_co[0]) + 
					(mval[1] - screen_co[1])*(mval[1] - screen_co[1]));
		
		if (dist <= GVERTSEL_TOL) {
			tNearestVertInfo *nvi = (tNearestVertInfo *)matches->last;
			short replace = 0;
			
			/* if there is already a point for the F-Curve, check if this point is closer than that was */
			if ((nvi) && (nvi->fcu == fcu)) {
				/* replace if we are closer, or if equal and that one wasn't selected but we are... */
				if ( (nvi->dist > dist) || ((nvi->sel == 0) && BEZSELECTED(bezt)) )
					replace= 1;
			}
			/* add new if not replacing... */
			if (replace == 0)
				nvi = MEM_callocN(sizeof(tNearestVertInfo), "Nearest Graph Vert Info - Bezt");
			
			/* store values */
			nvi->fcu = fcu;
			nvi->bezt = bezt;
			nvi->hpoint = hpoint;
			nvi->dist = dist;
			
			nvi->sel= BEZSELECTED(bezt); // XXX... should this use the individual verts instead?
			
			/* add to list of matches if appropriate... */
			if (replace == 0)
				BLI_addtail(matches, nvi);
		}
	}
	else if (fpt) {
		// TODO...
	}
} 

/* helper for find_nearest_fcurve_vert() - build the list of nearest matches */
static void get_nearest_fcurve_verts_list (bAnimContext *ac, const int mval[2], ListBase *matches)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sl;
	View2D *v2d= &ac->ar->v2d;
	
	/* get curves to search through 
	 *	- if the option to only show keyframes that belong to selected F-Curves is enabled,
	 *	  include the 'only selected' flag...
	 */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	if (sipo->flag & SIPO_SELCUVERTSONLY) 	// FIXME: this should really be check for by the filtering code...
		filter |= ANIMFILTER_SEL;
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* apply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, 0);
		
		/* apply NLA mapping to all the keyframes */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 0);
		
		if (fcu->bezt) {
			BezTriple *bezt1=fcu->bezt, *prevbezt=NULL;
			int i;
			
			for (i=0; i < fcu->totvert; i++, prevbezt=bezt1, bezt1++) {
				/* keyframe */
				nearest_fcurve_vert_store(matches, v2d, fcu, bezt1, NULL, NEAREST_HANDLE_KEY, mval);
				
				/* handles - only do them if they're visible */
				if (fcurve_handle_sel_check(sipo, bezt1) && (fcu->totvert > 1)) {
					/* first handle only visible if previous segment had handles */
					if ( (!prevbezt && (bezt1->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) )
					{
						nearest_fcurve_vert_store(matches, v2d, fcu, bezt1, NULL, NEAREST_HANDLE_LEFT, mval);
					}
					
					/* second handle only visible if this segment is bezier */
					if (bezt1->ipo == BEZT_IPO_BEZ) 
					{
						nearest_fcurve_vert_store(matches, v2d, fcu, bezt1, NULL, NEAREST_HANDLE_RIGHT, mval);
					}
				}
			}
		}
		else if (fcu->fpt) {
			// TODO; do this for samples too
			
		}
		
		/* un-apply NLA mapping from all the keyframes */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0);
		
		/* unapply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, ANIM_UNITCONV_RESTORE);
	}
	
	/* free channels */
	BLI_freelistN(&anim_data);
}

/* helper for find_nearest_fcurve_vert() - get the best match to use */
static tNearestVertInfo *get_best_nearest_fcurve_vert (ListBase *matches)
{
	tNearestVertInfo *nvi = NULL;
	short found = 0;
	
	/* abort if list is empty */
	if (matches->first == NULL) 
		return NULL;
		
	/* if list only has 1 item, remove it from the list and return */
	if (matches->first == matches->last) {
		/* need to remove from the list, otherwise it gets freed and then we can't return it */
		nvi= matches->first;
		BLI_remlink(matches, nvi);
		
		return nvi;
	}
	
	/* try to find the first selected F-Curve vert, then take the one after it */
	for (nvi = matches->first; nvi; nvi = nvi->next) {
		/* which mode of search are we in: find first selected, or find vert? */
		if (found) {
			/* just take this vert now that we've found the selected one 
			 *	- we'll need to remove this from the list so that it can be returned to the original caller
			 */
			BLI_remlink(matches, nvi);
			return nvi;
		}
		else {
			/* if vert is selected, we've got what we want... */
			if (nvi->sel)
				found= 1;
		}
	}
	
	/* if we're still here, this means that we failed to find anything appropriate in the first pass,
	 * so just take the first item now...
	 */
	nvi = matches->first;
	BLI_remlink(matches, nvi);
	return nvi;
}

/* Find the nearest vertices (either a handle or the keyframe) that are nearest to the mouse cursor (in area coordinates) 
 * NOTE: the match info found must still be freed 
 */
static tNearestVertInfo *find_nearest_fcurve_vert (bAnimContext *ac, const int mval[2])
{
	ListBase matches = {NULL, NULL};
	tNearestVertInfo *nvi;
	
	/* step 1: get the nearest verts */
	get_nearest_fcurve_verts_list(ac, mval, &matches);
	
	/* step 2: find the best vert */
	nvi= get_best_nearest_fcurve_vert(&matches);
	
	BLI_freelistN(&matches);
	
	/* return the best vert found */
	return nvi;
}

/* ------------------- */

/* option 1) select keyframe directly under mouse */
static void mouse_graph_keys (bAnimContext *ac, const int mval[2], short select_mode, short curves_only)
{
	SpaceIpo *sipo= (SpaceIpo *)ac->sl;
	tNearestVertInfo *nvi;
	BezTriple *bezt= NULL;
	
	/* find the beztriple that we're selecting, and the handle that was clicked on */
	nvi = find_nearest_fcurve_vert(ac, mval);
	
	/* check if anything to select */
	if (nvi == NULL)	
		return;
	
	/* deselect all other curves? */
	if (select_mode == SELECT_REPLACE) {
		/* reset selection mode */
		select_mode= SELECT_ADD;
		
		/* deselect all other keyframes (+ F-Curves too) */
		deselect_graph_keys(ac, 0, SELECT_SUBTRACT, TRUE);
		
		/* deselect other channels too, but only only do this if 
		 * selection of channel when the visibility of keyframes 
		 * doesn't depend on this 
		 */
		if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0)
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
	}
	
	/* if points can be selected on this F-Curve */
	// TODO: what about those with no keyframes?
	if ((curves_only == 0) && ((nvi->fcu->flag & FCURVE_PROTECTED)==0)) {
		/* only if there's keyframe */
		if (nvi->bezt) {
			bezt= nvi->bezt; /* used to check bezt seletion is set */
			/* depends on selection mode */
			if (select_mode == SELECT_INVERT) {
				/* keyframe - invert select of all */
				if (nvi->hpoint == NEAREST_HANDLE_KEY) {
					if (BEZSELECTED(bezt)) {
						BEZ_DESEL(bezt);
					}
					else {
						BEZ_SEL(bezt);
					}
				}
				
				/* handles - toggle selection of relevant handle */
				else if (nvi->hpoint == NEAREST_HANDLE_LEFT) {
					/* toggle selection */
					bezt->f1 ^= SELECT;
				}
				else {
					/* toggle selection */
					bezt->f3 ^= SELECT;
				}
			}
			else {
				/* if the keyframe was clicked on, select all verts of given beztriple */
				if (nvi->hpoint == NEAREST_HANDLE_KEY) {
					BEZ_SEL(bezt);
				}
				/* otherwise, select the handle that applied */
				else if (nvi->hpoint == NEAREST_HANDLE_LEFT) 
					bezt->f1 |= SELECT;
				else 
					bezt->f3 |= SELECT;
			}
		}
		else if (nvi->fpt) {
			// TODO: need to handle sample points
		}
	}
	else {
		KeyframeEditFunc select_cb;
		KeyframeEditData ked;
		
		/* initialize keyframe editing data */
		memset(&ked, 0, sizeof(KeyframeEditData));
		
		/* set up BezTriple edit callbacks */
		select_cb= ANIM_editkeyframes_select(select_mode);
		
		/* select all keyframes */
		ANIM_fcurve_keyframes_loop(&ked, nvi->fcu, NULL, select_cb, NULL);
	}
	
	/* only change selection of channel when the visibility of keyframes doesn't depend on this */
	if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0) {
		/* select or deselect curve? */
		if (bezt) {
			/* take selection status from item that got hit, to prevent flip/flop on channel 
			 * selection status when shift-selecting (i.e. "SELECT_INVERT") points
			 */
			if (BEZSELECTED(bezt))
				nvi->fcu->flag |= FCURVE_SELECTED;
			else
				nvi->fcu->flag &= ~FCURVE_SELECTED;
		}
		else {
			/* didn't hit any channel, so just apply that selection mode to the curve's selection status */
			if (select_mode == SELECT_INVERT)
				nvi->fcu->flag ^= FCURVE_SELECTED;
			else if (select_mode == SELECT_ADD)
				nvi->fcu->flag |= FCURVE_SELECTED;
		}
	}

	/* set active F-Curve (NOTE: sync the filter flags with findnearest_fcurve_vert) */
	/* needs to be called with (sipo->flag & SIPO_SELCUVERTSONLY) otherwise the active flag won't be set [#26452] */
	if (nvi->fcu->flag & FCURVE_SELECTED) {
		int filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
		ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, nvi->fcu, ANIMTYPE_FCURVE);
	}

	/* free temp sample data for filtering */
	MEM_freeN(nvi);
}

/* Option 2) Selects all the keyframes on either side of the current frame (depends on which side the mouse is on) */
/* (see graphkeys_select_leftright) */

/* Option 3) Selects all visible keyframes in the same frame as the mouse click */
static void graphkeys_mselect_column (bAnimContext *ac, const int mval[2], short select_mode)
{
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc select_cb, ok_cb;
	KeyframeEditData ked;
	tNearestVertInfo *nvi;
	float selx = (float)ac->scene->r.cfra;
	
	/* find the beztriple that we're selecting, and the handle that was clicked on */
	nvi = find_nearest_fcurve_vert(ac, mval);
	
	/* check if anything to select */
	if (nvi == NULL)	
		return;
	
	/* get frame number on which elements should be selected */
	// TODO: should we restrict to integer frames only?
	if (nvi->bezt)
		selx= nvi->bezt->vec[1][0];
	else if (nvi->fpt)
		selx= nvi->fpt->vec[0];
	
	/* if select mode is replace, deselect all keyframes first */
	if (select_mode==SELECT_REPLACE) {
		/* reset selection mode to add to selection */
		select_mode= SELECT_ADD;
		
		/* - deselect all other keyframes, so that just the newly selected remain
		 * - channels aren't deselected, since we don't re-select any as a consequence
		 */
		deselect_graph_keys(ac, 0, SELECT_SUBTRACT, FALSE);
	}
	
	/* initialize keyframe editing data */
	memset(&ked, 0, sizeof(KeyframeEditData));
	
	/* set up BezTriple edit callbacks */
	select_cb= ANIM_editkeyframes_select(select_mode);
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* set frame for validation callback to refer to */
		if (adt)
			ked.f1= BKE_nla_tweakedit_remap(adt, selx, NLATIME_CONVERT_UNMAP);
		else
			ked.f1= selx;
		
		/* select elements with frame number matching cfra */
		ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
	}
	
	/* free elements */
	MEM_freeN(nvi);
	BLI_freelistN(&ked.list);
	BLI_freelistN(&anim_data);
}
 
/* ------------------- */

/* handle clicking */
static int graphkeys_clickselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	short selectmode;

	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
	
	/* figure out action to take */
	if (RNA_boolean_get(op->ptr, "column")) {
		/* select all keyframes in the same frame as the one that was under the mouse */
		graphkeys_mselect_column(&ac, event->mval, selectmode);
	}
	else if (RNA_boolean_get(op->ptr, "curves")) {
		/* select all keyframes in the same F-Curve as the one under the mouse */
		mouse_graph_keys(&ac, event->mval, selectmode, 1);
	}
	else {
		/* select keyframe under mouse */
		mouse_graph_keys(&ac, event->mval, selectmode, 0);
	}
	
	/* set notifier that keyframe selection (and also channel selection in some cases) has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|ND_ANIMCHAN|NA_SELECTED, NULL);
	
	/* for tweak grab to work */
	return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
}
 
void GRAPH_OT_clickselect (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Select Keys";
	ot->idname= "GRAPH_OT_clickselect";
	ot->description= "Select keyframes by clicking on them";
	
	/* api callbacks */
	ot->invoke= graphkeys_clickselect_invoke;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* id-props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
	RNA_def_boolean(ot->srna, "column", 0, "Column Select", "Select all keyframes that occur on the same frame as the one under the mouse"); // ALTKEY
	RNA_def_boolean(ot->srna, "curves", 0, "Only Curves", "Select all the keyframes in the curve"); // CTRLKEY + ALTKEY
}

/* ************************************************************************** */

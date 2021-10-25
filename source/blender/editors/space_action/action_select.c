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

/** \file blender/editors/space_action/action_select.c
 *  \ingroup spaction
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"
#include "BLI_lasso.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mask_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_context.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_mask.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "action_intern.h"


/* ************************************************************************** */
/* KEYFRAMES STUFF */

/* ******************** Deselect All Operator ***************************** */
/* This operator works in one of three ways:
 *	1) (de)select all (AKEY) - test if select all or deselect all
 *	2) invert all (CTRL-IKEY) - invert selection of all keyframes
 *	3) (de)select all - no testing is done; only for use internal tools as normal function...
 */

/* Deselects keyframes in the action editor
 *	- This is called by the deselect all operator, as well as other ones!
 *
 *  - test: check if select or deselect all
 *	- sel: how to select keyframes (SELECT_*)
 */
static void deselect_action_keys(bAnimContext *ac, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked = {{NULL}};
	KeyframeEditFunc test_cb, sel_cb;
	
	/* determine type-based settings */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	
	/* filter data */
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* init BezTriple looping data */
	test_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale = anim_data.first; ale; ale = ale->next) {
			if (ale->type == ANIMTYPE_GPLAYER) {
				if (ED_gplayer_frame_select_check(ale->data)) {
					sel = SELECT_SUBTRACT;
					break;
				}
			}
			else if (ale->type == ANIMTYPE_MASKLAYER) {
				if (ED_masklayer_frame_select_check(ale->data)) {
					sel = SELECT_SUBTRACT;
					break;
				}
			}
			else {
				if (ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, test_cb, NULL)) {
					sel = SELECT_SUBTRACT;
					break;
				}
			}
		}
	}
	
	/* convert sel to selectmode, and use that to get editor */
	sel_cb = ANIM_editkeyframes_select(sel);
	
	/* Now set the flags */
	for (ale = anim_data.first; ale; ale = ale->next) {
		if (ale->type == ANIMTYPE_GPLAYER)
			ED_gplayer_frame_select_set(ale->data, sel);
		else if (ale->type == ANIMTYPE_MASKLAYER)
			ED_masklayer_frame_select_set(ale->data, sel);
		else
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, sel_cb, NULL); 
	}
	
	/* Cleanup */
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behavior - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		deselect_action_keys(&ac, 0, SELECT_INVERT);
	else
		deselect_action_keys(&ac, 1, SELECT_ADD);
	
	/* set notifier that keyframe selection have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select All";
	ot->idname = "ACTION_OT_select_all_toggle";
	ot->description = "Toggle selection of all keyframes";
	
	/* api callbacks */
	ot->exec = actkeys_deselectall_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	ot->prop = RNA_def_boolean(ot->srna, "invert", 0, "Invert", "");
	RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/* ******************** Border Select Operator **************************** */
/* This operator currently works in one of three ways:
 *	-> BKEY     - 1) all keyframes within region are selected (ACTKEYS_BORDERSEL_ALLKEYS)
 *	-> ALT-BKEY - depending on which axis of the region was larger...
 *		-> 2) x-axis, so select all frames within frame range (ACTKEYS_BORDERSEL_FRAMERANGE)
 *		-> 3) y-axis, so select all frames within channels that region included (ACTKEYS_BORDERSEL_CHANNELS)
 */

/* defines for borderselect mode */
enum {
	ACTKEYS_BORDERSEL_ALLKEYS   = 0,
	ACTKEYS_BORDERSEL_FRAMERANGE,
	ACTKEYS_BORDERSEL_CHANNELS,
} /*eActKeys_BorderSelect_Mode*/;


static void borderselect_action(bAnimContext *ac, const rcti rect, short mode, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked;
	KeyframeEditFunc ok_cb, select_cb;
	View2D *v2d = &ac->ar->v2d;
	rctf rectf;
	float ymin = 0, ymax = (float)(-ACHANNEL_HEIGHT_HALF(ac));
	
	/* convert mouse coordinates to frame ranges and channel coordinates corrected for view pan/zoom */
	UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin + 2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax - 2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing/validation funcs  */
	select_cb = ANIM_editkeyframes_select(selectmode);
	
	if (ELEM(mode, ACTKEYS_BORDERSEL_FRAMERANGE, ACTKEYS_BORDERSEL_ALLKEYS))
		ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	else
		ok_cb = NULL;
		
	/* init editing data */
	memset(&ked, 0, sizeof(KeyframeEditData));
	
	/* loop over data, doing border select */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		/* get new vertical minimum extent of channel */
		ymin = ymax - ACHANNEL_STEP(ac);
		
		/* set horizontal range (if applicable) */
		if (ELEM(mode, ACTKEYS_BORDERSEL_FRAMERANGE, ACTKEYS_BORDERSEL_ALLKEYS)) {
			/* if channel is mapped in NLA, apply correction */
			if (adt) {
				ked.iterflags &= ~(KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP);
				ked.f1 = BKE_nla_tweakedit_remap(adt, rectf.xmin, NLATIME_CONVERT_UNMAP);
				ked.f2 = BKE_nla_tweakedit_remap(adt, rectf.xmax, NLATIME_CONVERT_UNMAP);
			}
			else {
				ked.iterflags |= (KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP); /* for summary tracks */
				ked.f1 = rectf.xmin;
				ked.f2 = rectf.xmax;
			}
		}
		
		/* perform vertical suitability check (if applicable) */
		if ((mode == ACTKEYS_BORDERSEL_FRAMERANGE) ||
		    !((ymax < rectf.ymin) || (ymin > rectf.ymax)))
		{
			/* loop over data selecting */
			switch (ale->type) {
#if 0 /* XXXX: Keyframes are not currently shown here */
				case ANIMTYPE_GPDATABLOCK:
				{
					bGPdata *gpd = ale->data;
					bGPDlayer *gpl;
					for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
						ED_gplayer_frames_select_border(gpl, rectf.xmin, rectf.xmax, selectmode);
					}
					break;
				}
#endif
				case ANIMTYPE_GPLAYER:
					ED_gplayer_frames_select_border(ale->data, rectf.xmin, rectf.xmax, selectmode);
					break;
				case ANIMTYPE_MASKDATABLOCK:
				{
					Mask *mask = ale->data;
					MaskLayer *masklay;
					for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
						ED_masklayer_frames_select_border(masklay, rectf.xmin, rectf.xmax, selectmode);
					}
					break;
				}
				case ANIMTYPE_MASKLAYER:
					ED_masklayer_frames_select_border(ale->data, rectf.xmin, rectf.xmax, selectmode);
					break;
				default:
					ANIM_animchannel_keyframes_loop(&ked, ac->ads, ale, ok_cb, select_cb, NULL);
					break;
			}
		}
		
		/* set minimum extent to be the maximum of the next channel */
		ymax = ymin;
	}
	
	/* cleanup */
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short mode = 0, selectmode = 0;
	int gesture_mode;
	bool extend;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	/* clear all selection if not extending selection */
	extend = RNA_boolean_get(op->ptr, "extend");
	if (!extend)
		deselect_action_keys(&ac, 1, SELECT_SUBTRACT);
	
	/* get settings from operator */
	WM_operator_properties_border_to_rcti(op, &rect);
		
	gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	if (gesture_mode == GESTURE_MODAL_SELECT)
		selectmode = SELECT_ADD;
	else
		selectmode = SELECT_SUBTRACT;
	
	/* selection 'mode' depends on whether borderselect region only matters on one axis */
	if (RNA_boolean_get(op->ptr, "axis_range")) {
		/* mode depends on which axis of the range is larger to determine which axis to use 
		 *	- checking this in region-space is fine, as it's fundamentally still going to be a different rect size
		 *	- the frame-range select option is favored over the channel one (x over y), as frame-range one is often
		 *	  used for tweaking timing when "blocking", while channels is not that useful...
		 */
		if (BLI_rcti_size_x(&rect) >= BLI_rcti_size_y(&rect))
			mode = ACTKEYS_BORDERSEL_FRAMERANGE;
		else
			mode = ACTKEYS_BORDERSEL_CHANNELS;
	}
	else 
		mode = ACTKEYS_BORDERSEL_ALLKEYS;
	
	/* apply borderselect action */
	borderselect_action(&ac, rect, mode, selectmode);
	
	/* set notifier that keyframe selection have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
} 

void ACTION_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->idname = "ACTION_OT_select_border";
	ot->description = "Select all keyframes within the specified region";
	
	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = actkeys_borderselect_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;
	
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, true);
	
	ot->prop = RNA_def_boolean(ot->srna, "axis_range", 0, "Axis Range", "");
}

/* ******************** Region Select Operators ***************************** */
/* "Region Select" operators include the Lasso and Circle Select operators.
 * These two ended up being lumped together, as it was easier in the 
 * original Graph Editor implementation of these to do it this way.
 */

static void region_select_action_keys(bAnimContext *ac, const rctf *rectf_view, short mode, short selectmode, void *data)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked;
	KeyframeEditFunc ok_cb, select_cb;
	View2D *v2d = &ac->ar->v2d;
	rctf rectf, scaled_rectf;
	float ymin = 0, ymax = (float)(-ACHANNEL_HEIGHT_HALF(ac));
	
	/* convert mouse coordinates to frame ranges and channel coordinates corrected for view pan/zoom */
	UI_view2d_region_to_view_rctf(v2d, rectf_view, &rectf);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing/validation funcs  */
	select_cb = ANIM_editkeyframes_select(selectmode);
	ok_cb = ANIM_editkeyframes_ok(mode);
	
	/* init editing data */
	memset(&ked, 0, sizeof(KeyframeEditData));
	if (mode == BEZT_OK_CHANNEL_LASSO) {
		KeyframeEdit_LassoData *data_lasso = data;
		data_lasso->rectf_scaled = &scaled_rectf;
		ked.data = data_lasso;
	}
	else if (mode == BEZT_OK_CHANNEL_CIRCLE) {
		KeyframeEdit_CircleData *data_circle = data;
		data_circle->rectf_scaled = &scaled_rectf;
		ked.data = data;
	}
	else {
		ked.data = &scaled_rectf;
	}
	
	/* loop over data, doing region select */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		/* get new vertical minimum extent of channel */
		ymin = ymax - ACHANNEL_STEP(ac);
		
		/* compute midpoint of channel (used for testing if the key is in the region or not) */
		ked.channel_y = ymin + ACHANNEL_HEIGHT_HALF(ac);
		
		/* if channel is mapped in NLA, apply correction
		 * - Apply to the bounds being checked, not all the keyframe points,
		 *   to avoid having scaling everything
		 * - Save result to the scaled_rect, which is all that these operators
		 *   will read from
		 */
		if (adt) {
			ked.iterflags &= ~(KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP);
			ked.f1 = BKE_nla_tweakedit_remap(adt, rectf.xmin, NLATIME_CONVERT_UNMAP);
			ked.f2 = BKE_nla_tweakedit_remap(adt, rectf.xmax, NLATIME_CONVERT_UNMAP);
		}
		else {
			ked.iterflags |= (KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP); /* for summary tracks */
			ked.f1 = rectf.xmin;
			ked.f2 = rectf.xmax;
		}
		
		/* Update values for scaled_rectf - which is used to compute the mapping in the callbacks
		 * NOTE: Since summary tracks need late-binding remapping, the callbacks may overwrite these 
		 *       with the properly remapped ked.f1/f2 values, when needed
		 */
		scaled_rectf.xmin = ked.f1;
		scaled_rectf.xmax = ked.f2;
		scaled_rectf.ymin = ymin;
		scaled_rectf.ymax = ymax;
		
		/* perform vertical suitability check (if applicable) */
		if ((mode == ACTKEYS_BORDERSEL_FRAMERANGE) ||
		    !((ymax < rectf.ymin) || (ymin > rectf.ymax)))
		{
			/* loop over data selecting */
			switch (ale->type) {
				case ANIMTYPE_GPDATABLOCK:
				{
					bGPdata *gpd = ale->data;
					bGPDlayer *gpl;
					for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
						ED_gplayer_frames_select_region(&ked, ale->data, mode, selectmode);
					}
					break;
				}
				case ANIMTYPE_GPLAYER:
				{
					ED_gplayer_frames_select_region(&ked, ale->data, mode, selectmode);
					break;
				}
				case ANIMTYPE_MASKDATABLOCK:
				{
					Mask *mask = ale->data;
					MaskLayer *masklay;
					for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
						ED_masklayer_frames_select_region(&ked, masklay, mode, selectmode);
					}
					break;
				}
				case ANIMTYPE_MASKLAYER:
				{
					ED_masklayer_frames_select_region(&ked, ale->data, mode, selectmode);
					break;
				}
				default:
					ANIM_animchannel_keyframes_loop(&ked, ac->ads, ale, ok_cb, select_cb, NULL);
					break;
			}
		}
		
		/* set minimum extent to be the maximum of the next channel */
		ymax = ymin;
	}
	
	/* cleanup */
	ANIM_animdata_freelist(&anim_data);
}
 
/* ----------------------------------- */
 
static int actkeys_lassoselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	KeyframeEdit_LassoData data_lasso;
	rcti rect;
	rctf rect_fl;
	
	short selectmode;
	bool extend;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	data_lasso.rectf_view = &rect_fl;
	data_lasso.mcords = WM_gesture_lasso_path_to_array(C, op, &data_lasso.mcords_tot);
	if (data_lasso.mcords == NULL)
		return OPERATOR_CANCELLED;
	
	/* clear all selection if not extending selection */
	extend = RNA_boolean_get(op->ptr, "extend");
	if (!extend)
		deselect_action_keys(&ac, 1, SELECT_SUBTRACT);
	
	if (!RNA_boolean_get(op->ptr, "deselect"))
		selectmode = SELECT_ADD;
	else
		selectmode = SELECT_SUBTRACT;
	
	/* get settings from operator */
	BLI_lasso_boundbox(&rect, data_lasso.mcords, data_lasso.mcords_tot);
	BLI_rctf_rcti_copy(&rect_fl, &rect);
	
	/* apply borderselect action */
	region_select_action_keys(&ac, &rect_fl, BEZT_OK_CHANNEL_LASSO, selectmode, &data_lasso);
	
	MEM_freeN((void *)data_lasso.mcords);
	
	/* send notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_select_lasso(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lasso Select";
	ot->description = "Select keyframe points using lasso selection";
	ot->idname = "ACTION_OT_select_lasso";
	
	/* api callbacks */
	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = actkeys_lassoselect_exec;
	ot->poll = ED_operator_action_active;
	ot->cancel = WM_gesture_lasso_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend selection instead of deselecting everything first");
}

/* ------------------- */

static int action_circle_select_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	const int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	const short selectmode = (gesture_mode == GESTURE_MODAL_SELECT) ? SELECT_ADD : SELECT_SUBTRACT;
	
	KeyframeEdit_CircleData data = {0};
	rctf rect_fl;
	
	float x = RNA_int_get(op->ptr, "x");
	float y = RNA_int_get(op->ptr, "y");
	float radius = RNA_int_get(op->ptr, "radius");

	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	data.mval[0] = x;
	data.mval[1] = y;
	data.radius_squared = radius * radius;
	data.rectf_view = &rect_fl;
	
	rect_fl.xmin = x - radius;
	rect_fl.xmax = x + radius;
	rect_fl.ymin = y - radius;
	rect_fl.ymax = y + radius;
	
	/* apply region select action */
	region_select_action_keys(&ac, &rect_fl, BEZT_OK_CHANNEL_CIRCLE, selectmode, &data);
	
	/* send notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_select_circle(wmOperatorType *ot)
{
	ot->name = "Circle Select";
	ot->description = "Select keyframe points using circle selection";
	ot->idname = "ACTION_OT_select_circle";
	
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = action_circle_select_exec;
	ot->poll = ED_operator_action_active;
	ot->cancel = WM_gesture_circle_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 1, 1, INT_MAX, "Radius", "", 1, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
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
	{ACTKEYS_COLUMNSEL_KEYS, "KEYS", 0, "On Selected Keyframes", ""},
	{ACTKEYS_COLUMNSEL_CFRA, "CFRA", 0, "On Current Frame", ""},
	{ACTKEYS_COLUMNSEL_MARKERS_COLUMN, "MARKERS_COLUMN", 0, "On Selected Markers", ""},
	{ACTKEYS_COLUMNSEL_MARKERS_BETWEEN, "MARKERS_BETWEEN", 0, "Between Min/Max Selected Markers", ""},
	{0, NULL, 0, NULL, NULL}
};

/* ------------------- */ 

/* Selects all visible keyframes between the specified markers */
/* TODO, this is almost an _exact_ duplicate of a function of the same name in graph_select.c
 * should de-duplicate - campbell */
static void markers_selectkeys_between(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc ok_cb, select_cb;
	KeyframeEditData ked = {{NULL}};
	float min, max;
	
	/* get extreme markers */
	ED_markers_get_minmax(ac->markers, 1, &min, &max);
	min -= 0.5f;
	max += 0.5f;
	
	/* get editing funcs + data */
	ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb = ANIM_editkeyframes_select(SELECT_ADD);

	ked.f1 = min;
	ked.f2 = max;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY */ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* select keys in-between */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else if (ale->type == ANIMTYPE_GPLAYER) {
			ED_gplayer_frames_select_border(ale->data, min, max, SELECT_ADD);
		}
		else if (ale->type == ANIMTYPE_MASKLAYER) {
			ED_masklayer_frames_select_border(ale->data, min, max, SELECT_ADD);
		}
		else {
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
		}
	}
	
	/* Cleanup */
	ANIM_animdata_freelist(&anim_data);
}


/* Selects all visible keyframes in the same frames as the specified elements */
static void columnselect_action_keys(bAnimContext *ac, short mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene = ac->scene;
	CfraElem *ce;
	KeyframeEditFunc select_cb, ok_cb;
	KeyframeEditData ked = {{NULL}};
	
	/* initialize keyframe editing data */
	
	/* build list of columns */
	switch (mode) {
		case ACTKEYS_COLUMNSEL_KEYS: /* list of selected keys */
			if (ac->datatype == ANIMCONT_GPENCIL) {
				filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE);
				ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
				
				for (ale = anim_data.first; ale; ale = ale->next)
					ED_gplayer_make_cfra_list(ale->data, &ked.list, 1);
			}
			else {
				filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/);
				ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
				
				for (ale = anim_data.first; ale; ale = ale->next)
					ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, bezt_to_cfraelem, NULL);
			}
			ANIM_animdata_freelist(&anim_data);
			break;
			
		case ACTKEYS_COLUMNSEL_CFRA: /* current frame */
			/* make a single CfraElem for storing this */
			ce = MEM_callocN(sizeof(CfraElem), "cfraElem");
			BLI_addtail(&ked.list, ce);
			
			ce->cfra = (float)CFRA;
			break;
			
		case ACTKEYS_COLUMNSEL_MARKERS_COLUMN: /* list of selected markers */
			ED_markers_make_cfra_list(ac->markers, &ked.list, SELECT);
			break;
			
		default: /* invalid option */
			return;
	}
	
	/* set up BezTriple edit callbacks */
	select_cb = ANIM_editkeyframes_select(SELECT_ADD);
	ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		/* loop over cfraelems (stored in the KeyframeEditData->list)
		 *	- we need to do this here, as we can apply fewer NLA-mapping conversions
		 */
		for (ce = ked.list.first; ce; ce = ce->next) {
			/* set frame for validation callback to refer to */
			if (adt)
				ked.f1 = BKE_nla_tweakedit_remap(adt, ce->cfra, NLATIME_CONVERT_UNMAP);
			else
				ked.f1 = ce->cfra;
			
			/* select elements with frame number matching cfraelem */
			if (ale->type == ANIMTYPE_GPLAYER)
				ED_gpencil_select_frame(ale->data, ce->cfra, SELECT_ADD);
			else if (ale->type == ANIMTYPE_MASKLAYER)
				ED_mask_select_frame(ale->data, ce->cfra, SELECT_ADD);
			else
				ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
		}
	}
	
	/* free elements */
	BLI_freelistN(&ked.list);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_columnselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* action to take depends on the mode */
	mode = RNA_enum_get(op->ptr, "mode");
	
	if (mode == ACTKEYS_COLUMNSEL_MARKERS_BETWEEN)
		markers_selectkeys_between(&ac);
	else
		columnselect_action_keys(&ac, mode);
	
	/* set notifier that keyframe selection have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_select_column(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select All";
	ot->idname = "ACTION_OT_select_column";
	ot->description = "Select all keyframes on the specified frame(s)";
	
	/* api callbacks */
	ot->exec = actkeys_columnselect_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	ot->prop = RNA_def_enum(ot->srna, "mode", prop_column_select_types, 0, "Mode", "");
}

/* ******************** Select Linked Operator *********************** */

static int actkeys_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc ok_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	KeyframeEditFunc sel_cb = ANIM_editkeyframes_select(SELECT_ADD);
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* loop through all of the keys and select additional keyframes based on these */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		
		/* check if anything selected? */
		if (ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, ok_cb, NULL)) {
			/* select every keyframe in this curve then */
			ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, sel_cb, NULL);
		}
	}
	
	/* Cleanup */
	ANIM_animdata_freelist(&anim_data);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "ACTION_OT_select_linked";
	ot->description = "Select keyframes occurring in the same F-Curves as selected ones";
	
	/* api callbacks */
	ot->exec = actkeys_select_linked_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Select More/Less Operators *********************** */

/* Common code to perform selection */
static void select_moreless_action_keys(bAnimContext *ac, short mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked = {{NULL}};
	KeyframeEditFunc build_cb;
	
	
	/* init selmap building data */
	build_cb = ANIM_editkeyframes_buildselmap(mode);
	
	/* loop through all of the keys and select additional keyframes based on these */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		
		/* only continue if F-Curve has keyframes */
		if (fcu->bezt == NULL)
			continue;
		
		/* build up map of whether F-Curve's keyframes should be selected or not */
		ked.data = MEM_callocN(fcu->totvert, "selmap actEdit more");
		ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, build_cb, NULL);
		
		/* based on this map, adjust the selection status of the keyframes */
		ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, bezt_selmap_flush, NULL);
		
		/* free the selmap used here */
		MEM_freeN(ked.data);
		ked.data = NULL;
	}
	
	/* Cleanup */
	ANIM_animdata_freelist(&anim_data);
}

/* ----------------- */

static int actkeys_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* perform select changes */
	select_moreless_action_keys(&ac, SELMAP_MORE);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "ACTION_OT_select_more";
	ot->description = "Select keyframes beside already selected ones";
	
	/* api callbacks */
	ot->exec = actkeys_select_more_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ----------------- */

static int actkeys_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* perform select changes */
	select_moreless_action_keys(&ac, SELMAP_LESS);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "ACTION_OT_select_less";
	ot->description = "Deselect keyframes on ends of selection islands";
	
	/* api callbacks */
	ot->exec = actkeys_select_less_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Select Left/Right Operator ************************* */
/* Select keyframes left/right of the current frame indicator */

/* defines for left-right select tool */
static EnumPropertyItem prop_actkeys_leftright_select_types[] = {
	{ACTKEYS_LRSEL_TEST, "CHECK", 0, "Check if Select Left or Right", ""},
	{ACTKEYS_LRSEL_LEFT, "LEFT", 0, "Before current frame", ""},
	{ACTKEYS_LRSEL_RIGHT, "RIGHT", 0, "After current frame", ""},
	{0, NULL, 0, NULL, NULL}
};

/* --------------------------------- */

static void actkeys_select_leftright(bAnimContext *ac, short leftright, short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc ok_cb, select_cb;
	KeyframeEditData ked = {{NULL}};
	Scene *scene = ac->scene;
	
	/* if select mode is replace, deselect all keyframes (and channels) first */
	if (select_mode == SELECT_REPLACE) {
		select_mode = SELECT_ADD;
		
		/* - deselect all other keyframes, so that just the newly selected remain
		 * - channels aren't deselected, since we don't re-select any as a consequence
		 */
		deselect_action_keys(ac, 0, SELECT_SUBTRACT);
	}
	
	/* set callbacks and editing data */
	ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb = ANIM_editkeyframes_select(select_mode);
	
	if (leftright == ACTKEYS_LRSEL_LEFT) {
		ked.f1 = MINAFRAMEF;
		ked.f2 = (float)(CFRA + 0.1f);
	}
	else {
		ked.f1 = (float)(CFRA - 0.1f);
		ked.f2 = MAXFRAMEF;
	}
	
	/* filter data */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
		
	/* select keys */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else if (ale->type == ANIMTYPE_GPLAYER)
			ED_gplayer_frames_select_border(ale->data, ked.f1, ked.f2, select_mode);
		else if (ale->type == ANIMTYPE_MASKLAYER)
			ED_masklayer_frames_select_border(ale->data, ked.f1, ked.f2, select_mode);
		else
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
	}
	
	/* Sync marker support */
	if (select_mode == SELECT_ADD) {
		SpaceAction *saction = (SpaceAction *)ac->sl;
		
		if ((saction) && (saction->flag & SACTION_MARKERS_MOVE)) {
			ListBase *markers = ED_animcontext_get_markers(ac);
			TimeMarker *marker;
			
			for (marker = markers->first; marker; marker = marker->next) {
				if (((leftright == ACTKEYS_LRSEL_LEFT)  && (marker->frame <  CFRA)) ||
				    ((leftright == ACTKEYS_LRSEL_RIGHT) && (marker->frame >= CFRA)))
				{
					marker->flag |= SELECT;
				}
				else {
					marker->flag &= ~SELECT;
				}
			}
		}
	}

	/* Cleanup */
	ANIM_animdata_freelist(&anim_data);
}

/* ----------------- */

static int actkeys_select_leftright_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short leftright = RNA_enum_get(op->ptr, "mode");
	short selectmode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode = SELECT_INVERT;
	else
		selectmode = SELECT_REPLACE;
		
	/* if "test" mode is set, we don't have any info to set this with */
	if (leftright == ACTKEYS_LRSEL_TEST)
		return OPERATOR_CANCELLED;
	
	/* do the selecting now */
	actkeys_select_leftright(&ac, leftright, selectmode);
	
	/* set notifier that keyframe selection (and channels too) have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | ND_ANIMCHAN | NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

static int actkeys_select_leftright_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	bAnimContext ac;
	short leftright = RNA_enum_get(op->ptr, "mode");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* handle mode-based testing */
	if (leftright == ACTKEYS_LRSEL_TEST) {
		Scene *scene = ac.scene;
		ARegion *ar = ac.ar;
		View2D *v2d = &ar->v2d;
		float x;

		/* determine which side of the current frame mouse is on */
		x = UI_view2d_region_to_view_x(v2d, event->mval[0]);
		if (x < CFRA)
			RNA_enum_set(op->ptr, "mode", ACTKEYS_LRSEL_LEFT);
		else
			RNA_enum_set(op->ptr, "mode", ACTKEYS_LRSEL_RIGHT);
	}
	
	/* perform selection */
	return actkeys_select_leftright_exec(C, op);
}

void ACTION_OT_select_leftright(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Select Left/Right";
	ot->idname = "ACTION_OT_select_leftright";
	ot->description = "Select keyframes to the left or the right of the current frame";
	
	/* api callbacks  */
	ot->invoke = actkeys_select_leftright_invoke;
	ot->exec = actkeys_select_leftright_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "mode", prop_actkeys_leftright_select_types, ACTKEYS_LRSEL_TEST, "Mode", "");
	RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
	
	prop = RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ******************** Mouse-Click Select Operator *********************** */
/* This operator works in one of three ways:
 *	- 1) keyframe under mouse - no special modifiers
 *	- 2) all keyframes on the same side of current frame indicator as mouse - ALT modifier
 *	- 3) column select all keyframes in frame under mouse - CTRL modifier
 *  - 4) all keyframes in channel under mouse - CTRL+ALT modifiers
 *
 * In addition to these basic options, the SHIFT modifier can be used to toggle the 
 * selection mode between replacing the selection (without) and inverting the selection (with).
 */

/* ------------------- */
 
/* option 1) select keyframe directly under mouse */
static void actkeys_mselect_single(bAnimContext *ac, bAnimListElem *ale, short select_mode, float selx)
{
	KeyframeEditData ked = {{NULL}};
	KeyframeEditFunc select_cb, ok_cb;
	
	/* get functions for selecting keyframes */
	select_cb = ANIM_editkeyframes_select(select_mode);
	ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	ked.f1 = selx;
	ked.iterflags |= KED_F1_NLA_UNMAP;
	
	/* select the nominated keyframe on the given frame */
	if (ale->type == ANIMTYPE_GPLAYER) {
		ED_gpencil_select_frame(ale->data, selx, select_mode);
	}
	else if (ale->type == ANIMTYPE_MASKLAYER) {
		ED_mask_select_frame(ale->data, selx, select_mode);
	}
	else {
		if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK) &&
		    (ale->type == ANIMTYPE_SUMMARY) && (ale->datatype == ALE_ALL))
		{
			ListBase anim_data = {NULL, NULL};
			int filter;
			
			filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY */ | ANIMFILTER_NODUPLIS);
			ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
			
			for (ale = anim_data.first; ale; ale = ale->next) {
				if (ale->type == ANIMTYPE_GPLAYER) {
					ED_gpencil_select_frame(ale->data, selx, select_mode);
				}
				else if (ale->type == ANIMTYPE_MASKLAYER) {
					ED_mask_select_frame(ale->data, selx, select_mode);
				}
			}
			
			ANIM_animdata_freelist(&anim_data);
		}
		else {
			ANIM_animchannel_keyframes_loop(&ked, ac->ads, ale, ok_cb, select_cb, NULL);
		}
	}
}

/* Option 2) Selects all the keyframes on either side of the current frame (depends on which side the mouse is on) */
/* (see actkeys_select_leftright) */

/* Option 3) Selects all visible keyframes in the same frame as the mouse click */
static void actkeys_mselect_column(bAnimContext *ac, short select_mode, float selx)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc select_cb, ok_cb;
	KeyframeEditData ked = {{NULL}};
	
	/* set up BezTriple edit callbacks */
	select_cb = ANIM_editkeyframes_select(select_mode);
	ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY */ | ANIMFILTER_NODUPLIS);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		/* set frame for validation callback to refer to */
		if (adt)
			ked.f1 = BKE_nla_tweakedit_remap(adt, selx, NLATIME_CONVERT_UNMAP);
		else
			ked.f1 = selx;
		
		/* select elements with frame number matching cfra */
		if (ale->type == ANIMTYPE_GPLAYER)
			ED_gpencil_select_frame(ale->key_data, selx, select_mode);
		else if (ale->type == ANIMTYPE_MASKLAYER)
			ED_mask_select_frame(ale->key_data, selx, select_mode);
		else
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, ok_cb, select_cb, NULL);
	}
	
	/* free elements */
	BLI_freelistN(&ked.list);
	ANIM_animdata_freelist(&anim_data);
}

/* option 4) select all keyframes in same channel */
static void actkeys_mselect_channel_only(bAnimContext *ac, bAnimListElem *ale, short select_mode)
{
	KeyframeEditFunc select_cb;
	
	/* get functions for selecting keyframes */
	select_cb = ANIM_editkeyframes_select(select_mode);
	
	/* select all keyframes in this channel */
	if (ale->type == ANIMTYPE_GPLAYER) {
		ED_gpencil_select_frames(ale->data, select_mode);
	}
	else if (ale->type == ANIMTYPE_MASKLAYER) {
		ED_mask_select_frames(ale->data, select_mode);
	}
	else {
		if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK) &&
		    (ale->type == ANIMTYPE_SUMMARY) && (ale->datatype == ALE_ALL))
		{
			ListBase anim_data = {NULL, NULL};
			int filter;
			
			filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY */ | ANIMFILTER_NODUPLIS);
			ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
			
			for (ale = anim_data.first; ale; ale = ale->next) {
				if (ale->type == ANIMTYPE_GPLAYER) {
					ED_gpencil_select_frames(ale->data, select_mode);
				}
				else if (ale->type == ANIMTYPE_MASKLAYER) {
					ED_mask_select_frames(ale->data, select_mode);
				}
			}
			
			ANIM_animdata_freelist(&anim_data);
		}
		else {
			ANIM_animchannel_keyframes_loop(NULL, ac->ads, ale, NULL, select_cb, NULL);
		}
	}
}
 
/* ------------------- */

static void mouse_action_keys(bAnimContext *ac, const int mval[2], short select_mode, bool column, bool same_channel)
{
	ListBase anim_data = {NULL, NULL};
	DLRBT_Tree anim_keys;
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d = &ac->ar->v2d;
	bDopeSheet *ads = NULL;
	int channel_index;
	bool found = false;
	float frame = 0.0f; /* frame of keyframe under mouse - NLA corrections not applied/included */
	float selx = 0.0f;  /* frame of keyframe under mouse */
	float key_hsize;
	float x, y;
	rctf rectf;
	
	/* get dopesheet info */
	if (ac->datatype == ANIMCONT_DOPESHEET)
		ads = ac->data;
	
	/* use View2D to determine the index of the channel (i.e a row in the list) where keyframe was */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, 0, ACHANNEL_STEP(ac), 0, (float)ACHANNEL_HEIGHT_HALF(ac), x, y, NULL, &channel_index);
	
	/* x-range to check is +/- 7px for standard keyframe under standard dpi/y-scale (in screen/region-space),
	 * on either side of mouse click (size of keyframe icon)
	 */
	key_hsize = ACHANNEL_HEIGHT(ac) * 0.8f;    /* standard channel height (to allow for some slop) */
	key_hsize = roundf(key_hsize / 2.0f);      /* half-size (for either side), but rounded up to nearest int (for easier targetting) */
	
	UI_view2d_region_to_view(v2d, mval[0] - (int)key_hsize, mval[1], &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, mval[0] + (int)key_hsize, mval[1], &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* try to get channel */
	ale = BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		printf("Error: animation channel (index = %d) not found in mouse_action_keys()\n", channel_index);
		ANIM_animdata_freelist(&anim_data);
		return;
	}
	else {
		/* found match - must return here... */
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		ActKeyColumn *ak, *akn = NULL;
		
		/* make list of keyframes */
		BLI_dlrbTree_init(&anim_keys);
		
		if (ale->key_data) {
			switch (ale->datatype) {
				case ALE_SCE:
				{
					Scene *scene = (Scene *)ale->key_data;
					scene_to_keylist(ads, scene, &anim_keys, NULL);
					break;
				}
				case ALE_OB:
				{
					Object *ob = (Object *)ale->key_data;
					ob_to_keylist(ads, ob, &anim_keys, NULL);
					break;
				}
				case ALE_ACT:
				{
					bAction *act = (bAction *)ale->key_data;
					action_to_keylist(adt, act, &anim_keys, NULL);
					break;
				}
				case ALE_FCURVE:
				{
					FCurve *fcu = (FCurve *)ale->key_data;
					fcurve_to_keylist(adt, fcu, &anim_keys, NULL);
					break;
				}
			}
		}
		else if (ale->type == ANIMTYPE_SUMMARY) {
			/* dopesheet summary covers everything */
			summary_to_keylist(ac, &anim_keys, NULL);
		}
		else if (ale->type == ANIMTYPE_GROUP) {
			// TODO: why don't we just give groups key_data too?
			bActionGroup *agrp = (bActionGroup *)ale->data;
			agroup_to_keylist(adt, agrp, &anim_keys, NULL);
		}
		else if (ale->type == ANIMTYPE_GPLAYER) {
			// TODO: why don't we just give gplayers key_data too?
			bGPDlayer *gpl = (bGPDlayer *)ale->data;
			gpl_to_keylist(ads, gpl, &anim_keys);
		}
		else if (ale->type == ANIMTYPE_MASKLAYER) {
			// TODO: why don't we just give masklayers key_data too?
			MaskLayer *masklay = (MaskLayer *)ale->data;
			mask_to_keylist(ads, masklay, &anim_keys);
		}

		/* start from keyframe at root of BST, traversing until we find one within the range that was clicked on */
		for (ak = anim_keys.root; ak; ak = akn) {
			if (IN_RANGE(ak->cfra, rectf.xmin, rectf.xmax)) {
				/* set the frame to use, and apply inverse-correction for NLA-mapping 
				 * so that the frame will get selected by the selection functions without
				 * requiring to map each frame once again...
				 */
				selx = BKE_nla_tweakedit_remap(adt, ak->cfra, NLATIME_CONVERT_UNMAP);
				frame = ak->cfra;
				found = true;
				break;
			}
			else if (ak->cfra < rectf.xmin)
				akn = ak->right;
			else
				akn = ak->left;
		}
		
		/* remove active channel from list of channels for separate treatment (since it's needed later on) */
		BLI_remlink(&anim_data, ale);
		
		/* cleanup temporary lists */
		BLI_dlrbTree_free(&anim_keys);
		
		/* free list of channels, since it's not used anymore */
		ANIM_animdata_freelist(&anim_data);
	}
	
	/* for replacing selection, firstly need to clear existing selection */
	if (select_mode == SELECT_REPLACE) {
		/* reset selection mode for next steps */
		select_mode = SELECT_ADD;
		
		/* deselect all keyframes */
		deselect_action_keys(ac, 0, SELECT_SUBTRACT);
		
		/* highlight channel clicked on */
		if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET)) {
			/* deselect all other channels first */
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
			
			/* Highlight Action-Group or F-Curve? */
			if (ale && ale->data) {
				if (ale->type == ANIMTYPE_GROUP) {
					bActionGroup *agrp = ale->data;
					
					agrp->flag |= AGRP_SELECTED;
					ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, agrp, ANIMTYPE_GROUP);
				}
				else if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
					FCurve *fcu = ale->data;
					
					fcu->flag |= FCURVE_SELECTED;
					ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, fcu, ale->type);
				}
			}
		}
		else if (ac->datatype == ANIMCONT_GPENCIL) {
			/* deselect all other channels first */
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
			
			/* Highlight GPencil Layer */
			if ((ale && ale->data) && (ale->type == ANIMTYPE_GPLAYER)) {
				bGPDlayer *gpl = ale->data;
				
				gpl->flag |= GP_LAYER_SELECT;
				//gpencil_layer_setactive(gpd, gpl);
			}
		}
		else if (ac->datatype == ANIMCONT_MASK) {
			/* deselect all other channels first */
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);

			/* Highlight GPencil Layer */
			if ((ale && ale->data) && (ale->type == ANIMTYPE_MASKLAYER)) {
				MaskLayer *masklay = ale->data;

				masklay->flag |= MASK_LAYERFLAG_SELECT;
				//gpencil_layer_setactive(gpd, gpl);
			}
		}
	}
	
	/* only select keyframes if we clicked on a valid channel and hit something */
	if (ale) {
		if (found) {
			/* apply selection to keyframes */
			if (column) {
				/* select all keyframes in the same frame as the one we hit on the active channel 
				 * [T41077]: "frame" not "selx" here (i.e. no NLA corrections yet) as the code here
				 *            does that itself again as it needs to work on multiple datablocks 
				 */
				actkeys_mselect_column(ac, select_mode, frame);
			}
			else if (same_channel) {
				/* select all keyframes in the active channel */
				actkeys_mselect_channel_only(ac, ale, select_mode);	
			}
			else {
				/* select the nominated keyframe on the given frame */
				actkeys_mselect_single(ac, ale, select_mode, selx);
			}
		}
		
		/* free this channel */
		MEM_freeN(ale);
	}
}

/* handle clicking */
static int actkeys_clickselect_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	bAnimContext ac;
	/* ARegion *ar; */ /* UNUSED */
	short selectmode;
	bool column, channel;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	/* ar = ac.ar; */ /* UNUSED */

	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode = SELECT_INVERT;
	else
		selectmode = SELECT_REPLACE;
		
	/* column selection */
	column = RNA_boolean_get(op->ptr, "column");
	channel = RNA_boolean_get(op->ptr, "channel");
	
	/* select keyframe(s) based upon mouse position*/
	mouse_action_keys(&ac, event->mval, selectmode, column, channel);
	
	/* set notifier that keyframe selection (and channels too) have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | ND_ANIMCHAN | NA_SELECTED, NULL);
	
	/* for tweak grab to work */
	return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}
 
void ACTION_OT_clickselect(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Mouse Select Keys";
	ot->idname = "ACTION_OT_clickselect";
	ot->description = "Select keyframes by clicking on them";
	
	/* callbacks */
	ot->invoke = actkeys_clickselect_invoke;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	prop = RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", 
	                       "Toggle keyframe selection instead of leaving newly selected keyframes only"); // SHIFTKEY
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	
	prop = RNA_def_boolean(ot->srna, "column", 0, "Column Select", 
	                       "Select all keyframes that occur on the same frame as the one under the mouse"); // ALTKEY
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	
	prop = RNA_def_boolean(ot->srna, "channel", 0, "Only Channel", 
	                       "Select all the keyframes in the channel under the mouse"); // CTRLKEY + ALTKEY
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************************************************************************** */

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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_nla/nla_select.c
 *  \ingroup spnla
 */


#include <string.h>
#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "nla_intern.h"	// own include

/* ******************** Utilities ***************************************** */

/* Convert SELECT_* flags to ACHANNEL_SETFLAG_* flags */
static short selmodes_to_flagmodes (short sel)
{
	/* convert selection modes to selection modes */
	switch (sel) {
		case SELECT_SUBTRACT:
			return ACHANNEL_SETFLAG_CLEAR;
			break;
			
		case SELECT_INVERT:
			return ACHANNEL_SETFLAG_INVERT;
			break;
			
		case SELECT_ADD:
		default:
			return ACHANNEL_SETFLAG_ADD;
			break;
	}
}


/* ******************** Deselect All Operator ***************************** */
/* This operator works in one of three ways:
 *	1) (de)select all (AKEY) - test if select all or deselect all
 *	2) invert all (CTRL-IKEY) - invert selection of all keyframes
 *	3) (de)select all - no testing is done; only for use internal tools as normal function...
 */

enum {
	DESELECT_STRIPS_NOTEST = 0,
	DESELECT_STRIPS_TEST,
	DESELECT_STRIPS_CLEARACTIVE,
} /*eDeselectNlaStrips*/;
 
/* Deselects strips in the NLA Editor
 *	- This is called by the deselect all operator, as well as other ones!
 *
 * 	- test: check if select or deselect all (1) or clear all active (2)
 *	- sel: how to select keyframes 
 *		0 = deselect
 *		1 = select
 *		2 = invert
 */
static void deselect_nla_strips (bAnimContext *ac, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	short smode;
	
	/* determine type-based settings */
	// FIXME: double check whether ANIMFILTER_LIST_VISIBLE is needed!
	filter= (ANIMFILTER_DATA_VISIBLE);
	
	/* filter data */
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* See if we should be selecting or deselecting */
	if (test == DESELECT_STRIPS_TEST) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			NlaTrack *nlt= (NlaTrack *)ale->data;
			NlaStrip *strip;
			
			/* if any strip is selected, break out, since we should now be deselecting */
			for (strip= nlt->strips.first; strip; strip= strip->next) {
				if (strip->flag & NLASTRIP_FLAG_SELECT) {
					sel= SELECT_SUBTRACT;
					break;
				}
			}
			
			if (sel == SELECT_SUBTRACT)
				break;
		}
	}
	
	/* convert selection modes to selection modes */
	smode= selmodes_to_flagmodes(sel);
	
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		
		/* apply same selection to all strips */
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			/* set selection */
			if (test != DESELECT_STRIPS_CLEARACTIVE)
				ACHANNEL_SET_FLAG(strip, smode, NLASTRIP_FLAG_SELECT);
			
			/* clear active flag */
			// TODO: for clear active, do we want to limit this to only doing this on a certain set of tracks though?
			strip->flag &= ~NLASTRIP_FLAG_ACTIVE;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int nlaedit_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behavior - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		deselect_nla_strips(&ac, DESELECT_STRIPS_NOTEST, SELECT_INVERT);
	else
		deselect_nla_strips(&ac, DESELECT_STRIPS_TEST, SELECT_ADD);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void NLA_OT_select_all_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->idname= "NLA_OT_select_all_toggle";
	ot->description= "(De)Select all NLA-Strips";
	
	/* api callbacks */
	ot->exec= nlaedit_deselectall_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	RNA_def_boolean(ot->srna, "invert", 0, "Invert", "");
}

/* ******************** Border Select Operator **************************** */
/* This operator currently works in one of three ways:
 *	-> BKEY 	- 1) all strips within region are selected (NLAEDIT_BORDERSEL_ALLSTRIPS)
 *	-> ALT-BKEY - depending on which axis of the region was larger...
 *		-> 2) x-axis, so select all frames within frame range (NLAEDIT_BORDERSEL_FRAMERANGE)
 *		-> 3) y-axis, so select all frames within channels that region included (NLAEDIT_BORDERSEL_CHANNELS)
 */

/* defines for borderselect mode */
enum {
	NLA_BORDERSEL_ALLSTRIPS	= 0,
	NLA_BORDERSEL_FRAMERANGE,
	NLA_BORDERSEL_CHANNELS,
} /* eNLAEDIT_BorderSelect_Mode */;


static void borderselect_nla_strips (bAnimContext *ac, rcti rect, short mode, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceNla *snla = (SpaceNla *)ac->sl;
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	float ymin /* =(float)(-NLACHANNEL_HEIGHT(snla)) */ /* UNUSED */, ymax=0;
	
	/* convert border-region to view coordinates */
	UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin+2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax-2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* convert selection modes to selection modes */
	selectmode= selmodes_to_flagmodes(selectmode);
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		ymin= ymax - NLACHANNEL_STEP(snla);
		
		/* perform vertical suitability check (if applicable) */
		if ( (mode == NLA_BORDERSEL_FRAMERANGE) ||
			!((ymax < rectf.ymin) || (ymin > rectf.ymax)) ) 
		{
			/* loop over data selecting (only if NLA-Track) */
			if (ale->type == ANIMTYPE_NLATRACK) {
				NlaTrack *nlt= (NlaTrack *)ale->data;
				NlaStrip *strip;
				
				/* only select strips if they fall within the required ranges (if applicable) */
				for (strip= nlt->strips.first; strip; strip= strip->next) {
					if ( (mode == NLA_BORDERSEL_CHANNELS) || 
						  BKE_nlastrip_within_bounds(strip, rectf.xmin, rectf.xmax) ) 
					{
						/* set selection */
						ACHANNEL_SET_FLAG(strip, selectmode, NLASTRIP_FLAG_SELECT);
						
						/* clear active flag */
						strip->flag &= ~NLASTRIP_FLAG_ACTIVE;
					}
				}
			}
		}
		
		/* set minimum extent to be the maximum of the next channel */
		ymax= ymin;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int nlaedit_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short mode=0, selectmode=0;
	int extend;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	/* clear all selection if not extending selection */
	extend= RNA_boolean_get(op->ptr, "extend");
	if (!extend)
		deselect_nla_strips(&ac, DESELECT_STRIPS_TEST, SELECT_SUBTRACT);

	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
		
	if (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_SELECT)
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
		if ((rect.xmax - rect.xmin) >= (rect.ymax - rect.ymin))
			mode= NLA_BORDERSEL_FRAMERANGE;
		else
			mode= NLA_BORDERSEL_CHANNELS;
	}
	else 
		mode= NLA_BORDERSEL_ALLSTRIPS;
	
	/* apply borderselect action */
	borderselect_nla_strips(&ac, rect, mode, selectmode);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
} 

void NLA_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "NLA_OT_select_border";
	ot->description= "Use box selection to grab NLA-Strips";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= nlaedit_borderselect_exec;
	ot->modal= WM_border_select_modal;
	ot->cancel= WM_border_select_cancel;
	
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, 1);
	
	RNA_def_boolean(ot->srna, "axis_range", 0, "Axis Range", "");
}

/* ******************** Select Left/Right Operator ************************* */
/* Select keyframes left/right of the current frame indicator */

/* defines for left-right select tool */
static EnumPropertyItem prop_nlaedit_leftright_select_types[] = {
	{NLAEDIT_LRSEL_TEST, "CHECK", 0, "Check if Select Left or Right", ""},
	{NLAEDIT_LRSEL_LEFT, "LEFT", 0, "Before current frame", ""},
	{NLAEDIT_LRSEL_RIGHT, "RIGHT", 0, "After current frame", ""},
	{0, NULL, 0, NULL, NULL}
};

/* ------------------- */

static void nlaedit_select_leftright (bContext *C, bAnimContext *ac, short leftright, short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene= ac->scene;
	float xmin, xmax;
	
	/* if currently in tweakmode, exit tweakmode first */
	if (scene->flag & SCE_NLA_EDIT_ON)
		WM_operator_name_call(C, "NLA_OT_tweakmode_exit", WM_OP_EXEC_DEFAULT, NULL);
	
	/* if select mode is replace, deselect all keyframes (and channels) first */
	if (select_mode==SELECT_REPLACE) {
		select_mode= SELECT_ADD;
		
		/* - deselect all other keyframes, so that just the newly selected remain
		 * - channels aren't deselected, since we don't re-select any as a consequence
		 */
		deselect_nla_strips(ac, 0, SELECT_SUBTRACT);
	}
	
	/* get range, and get the right flag-setting mode */
	if (leftright == NLAEDIT_LRSEL_LEFT) {
		xmin = MINAFRAMEF;
		xmax = (float)(CFRA + 0.1f);
	} 
	else {
		xmin = (float)(CFRA - 0.1f);
		xmax = MAXFRAMEF;
	}
	
	select_mode= selmodes_to_flagmodes(select_mode);
	
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* select strips on the side where most data occurs */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		
		/* check each strip to see if it is appropriate */
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			if (BKE_nlastrip_within_bounds(strip, xmin, xmax)) {
				ACHANNEL_SET_FLAG(strip, select_mode, NLASTRIP_FLAG_SELECT);
			}
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int nlaedit_select_leftright_exec (bContext *C, wmOperator *op)
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
	if (leftright == NLAEDIT_LRSEL_TEST)
		return OPERATOR_CANCELLED;
	
	/* do the selecting now */
	nlaedit_select_leftright(C, &ac, leftright, selectmode);
	
	/* set notifier that keyframe selection (and channels too) have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|ND_ANIMCHAN|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}

static int nlaedit_select_leftright_invoke (bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	short leftright = RNA_enum_get(op->ptr, "mode");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* handle mode-based testing */
	if (leftright == NLAEDIT_LRSEL_TEST) {
		Scene *scene= ac.scene;
		ARegion *ar= ac.ar;
		View2D *v2d= &ar->v2d;
		float x;
		
		/* determine which side of the current frame mouse is on */
		UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &x, NULL);
		if (x < CFRA)
			RNA_int_set(op->ptr, "mode", NLAEDIT_LRSEL_LEFT);
		else 	
			RNA_int_set(op->ptr, "mode", NLAEDIT_LRSEL_RIGHT);
	}
	
	/* perform selection */
	return nlaedit_select_leftright_exec(C, op);
}

void NLA_OT_select_leftright (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Left/Right";
	ot->idname= "NLA_OT_select_leftright";
	ot->description= "Select strips to the left or the right of the current frame";
	
	/* api callbacks  */
	ot->invoke= nlaedit_select_leftright_invoke;
	ot->exec= nlaedit_select_leftright_exec;
	ot->poll= ED_operator_nla_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	ot->prop= RNA_def_enum(ot->srna, "mode", prop_nlaedit_leftright_select_types, NLAEDIT_LRSEL_TEST, "Mode", "");
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
}


/* ******************** Mouse-Click Select Operator *********************** */

/* select strip directly under mouse */
static void mouse_nla_strips (bContext *C, bAnimContext *ac, const int mval[2], short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale = NULL;
	int filter;
	
	SpaceNla *snla = (SpaceNla *)ac->sl;
	View2D *v2d= &ac->ar->v2d;
	Scene *scene= ac->scene;
	NlaStrip *strip = NULL;
	int channel_index;
	float xmin, xmax, dummy;
	float x, y;
	
	
	/* use View2D to determine the index of the channel (i.e a row in the list) where keyframe was */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, 0, NLACHANNEL_STEP(snla), 0, (float)NLACHANNEL_HEIGHT_HALF(snla), x, y, NULL, &channel_index);
	
	/* x-range to check is +/- 7 (in screen/region-space) on either side of mouse click 
	 * (that is the size of keyframe icons, so user should be expecting similar tolerances) 
	 */
	UI_view2d_region_to_view(v2d, mval[0]-7, mval[1], &xmin, &dummy);
	UI_view2d_region_to_view(v2d, mval[0]+7, mval[1], &xmax, &dummy);
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* try to get channel */
	ale= BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		printf("Error: animation channel (index = %d) not found in mouse_nla_strips() \n", channel_index);
		BLI_freelistN(&anim_data);
		return;
	}
	else {
		/* found some channel - we only really should do somethign when its an Nla-Track */
		if (ale->type == ANIMTYPE_NLATRACK) {
			NlaTrack *nlt= (NlaTrack *)ale->data;
			
			/* loop over NLA-strips in this track, trying to find one which occurs in the necessary bounds */
			for (strip= nlt->strips.first; strip; strip= strip->next) {
				if (BKE_nlastrip_within_bounds(strip, xmin, xmax))
					break;
			}
		}
		
		/* remove active channel from list of channels for separate treatment (since it's needed later on) */
		BLI_remlink(&anim_data, ale);
		
		/* free list of channels, since it's not used anymore */
		BLI_freelistN(&anim_data);
	}
	
	/* if currently in tweakmode, exit tweakmode before changing selection states
	 * now that we've found our target...
	 */
	if (scene->flag & SCE_NLA_EDIT_ON)
		WM_operator_name_call(C, "NLA_OT_tweakmode_exit", WM_OP_EXEC_DEFAULT, NULL);
	
	/* for replacing selection, firstly need to clear existing selection */
	if (select_mode == SELECT_REPLACE) {
		/* reset selection mode for next steps */
		select_mode = SELECT_ADD;
		
		/* deselect all strips */
		deselect_nla_strips(ac, 0, SELECT_SUBTRACT);
		
		/* deselect all other channels first */
		ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
		
		/* Highlight NLA-Track */
		if (ale->type == ANIMTYPE_NLATRACK) {	
			NlaTrack *nlt= (NlaTrack *)ale->data;
			
			nlt->flag |= NLATRACK_SELECTED;
			ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, nlt, ANIMTYPE_NLATRACK);
		}
	}
	
	/* only select strip if we clicked on a valid channel and hit something */
	if (ale) {
		/* select the strip accordingly (if a matching one was found) */
		if (strip) {
			select_mode= selmodes_to_flagmodes(select_mode);
			ACHANNEL_SET_FLAG(strip, select_mode, NLASTRIP_FLAG_SELECT);
			
			/* if we selected it, we can make it active too
			 *	- we always need to clear the active strip flag though... 
			 */
			deselect_nla_strips(ac, DESELECT_STRIPS_CLEARACTIVE, 0);
			if (strip->flag & NLASTRIP_FLAG_SELECT)
				strip->flag |= NLASTRIP_FLAG_ACTIVE;
		}
		
		/* free this channel */
		MEM_freeN(ale);
	}
}

/* ------------------- */

/* handle clicking */
static int nlaedit_clickselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	/* Scene *scene; */ /* UNUSED */
	/* ARegion *ar; */ /* UNUSED */
	// View2D *v2d; /*UNUSED*/
	short selectmode;

	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	/* scene= ac.scene; */ /* UNUSED */
	/* ar= ac.ar; */ /* UNUSED */
	// v2d= &ar->v2d;

	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
		
	/* select strips based upon mouse position */
	mouse_nla_strips(C, &ac, event->mval, selectmode);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA|NA_SELECTED, NULL);
	
	/* for tweak grab to work */
	return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
}
 
void NLA_OT_click_select (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Select";
	ot->idname= "NLA_OT_click_select";
	ot->description= "Handle clicks to select NLA Strips";
	
	/* api callbacks - absolutely no exec() this yet... */
	ot->invoke= nlaedit_clickselect_invoke;
	ot->poll= ED_operator_nla_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
}

/* *********************************************** */

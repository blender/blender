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

/** \file blender/editors/space_nla/nla_channels.c
 *  \ingroup spnla
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_report.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "nla_intern.h" // own include

/* *********************************************** */
/* Operators for NLA channels-list which need to be different from the standard Animation Editor ones */

/* ******************** Mouse-Click Operator *********************** */
/* Depending on the channel that was clicked on, the mouse click will activate whichever
 * part of the channel is relevant.
 *
 * NOTE: eventually, this should probably be phased out when many of these things are replaced with buttons
 *	--> Most channels are now selection only...
 */

static int mouse_nla_channels(bContext *C, bAnimContext *ac, float x, int channel_index, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d = &ac->ar->v2d;
	int notifierFlags = 0;
	
	/* get the channel that was clicked on */
	/* filter channels */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get channel from index */
	ale = BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		if (G.debug & G_DEBUG)
			printf("Error: animation channel (index = %d) not found in mouse_anim_channels()\n", channel_index);
		
		ANIM_animdata_freelist(&anim_data);
		return 0;
	}
	
	/* action to take depends on what channel we've got */
	// WARNING: must keep this in sync with the equivalent function in anim_channels_edit.c
	switch (ale->type) {
		case ANIMTYPE_SCENE:
		{
			Scene *sce = (Scene *)ale->data;
			AnimData *adt = sce->adt;
			
			/* set selection status */
			if (selectmode == SELECT_INVERT) {
				/* swap select */
				sce->flag ^= SCE_DS_SELECTED;
				if (adt) adt->flag ^= ADT_UI_SELECTED;
			}
			else {
				sce->flag |= SCE_DS_SELECTED;
				if (adt) adt->flag |= ADT_UI_SELECTED;
			}
			
			notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
			break;
		}
		case ANIMTYPE_OBJECT:
		{
			bDopeSheet *ads = (bDopeSheet *)ac->data;
			Scene *sce = (Scene *)ads->source;
			Base *base = (Base *)ale->data;
			Object *ob = base->object;
			AnimData *adt = ob->adt;
			
			if (nlaedit_is_tweakmode_on(ac) == 0) {
				/* set selection status */
				if (selectmode == SELECT_INVERT) {
					/* swap select */
					base->flag ^= SELECT;
					ob->flag = base->flag;
					
					if (adt) adt->flag ^= ADT_UI_SELECTED;
				}
				else {
					Base *b;
					
					/* deselect all */
					/* TODO: should this deselect all other types of channels too? */
					for (b = sce->base.first; b; b = b->next) {
						b->flag &= ~SELECT;
						b->object->flag = b->flag;
						if (b->object->adt) b->object->adt->flag &= ~(ADT_UI_SELECTED | ADT_UI_ACTIVE);
					}
					
					/* select object now */
					base->flag |= SELECT;
					ob->flag |= SELECT;
					if (adt) adt->flag |= ADT_UI_SELECTED;
				}
				
				/* change active object - regardless of whether it is now selected [T37883] */
				ED_base_object_activate(C, base); /* adds notifier */
				
				if ((adt) && (adt->flag & ADT_UI_SELECTED))
					adt->flag |= ADT_UI_ACTIVE;
				
				/* notifiers - channel was selected */
				notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
			}
			break;
		}
		case ANIMTYPE_FILLACTD: /* Action Expander */
		case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
		case ANIMTYPE_DSLAM:
		case ANIMTYPE_DSCAM:
		case ANIMTYPE_DSCUR:
		case ANIMTYPE_DSSKEY:
		case ANIMTYPE_DSWOR:
		case ANIMTYPE_DSNTREE:
		case ANIMTYPE_DSPART:
		case ANIMTYPE_DSMBALL:
		case ANIMTYPE_DSARM:
		case ANIMTYPE_DSMESH:
		case ANIMTYPE_DSTEX:
		case ANIMTYPE_DSLAT:
		case ANIMTYPE_DSLINESTYLE:
		case ANIMTYPE_DSSPK:
		case ANIMTYPE_DSGPENCIL:
		{
			/* sanity checking... */
			if (ale->adt) {
				/* select/deselect */
				if (selectmode == SELECT_INVERT) {
					/* inverse selection status of this AnimData block only */
					ale->adt->flag ^= ADT_UI_SELECTED;
				}
				else {
					/* select AnimData block by itself */
					ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
					ale->adt->flag |= ADT_UI_SELECTED;
				}
				
				/* set active? */
				if ((ale->adt) && (ale->adt->flag & ADT_UI_SELECTED))
					ale->adt->flag |= ADT_UI_ACTIVE;
			}
			
			notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
			break;
		}
		case ANIMTYPE_NLATRACK:
		{
			NlaTrack *nlt = (NlaTrack *)ale->data;
			AnimData *adt = ale->adt;
			short offset;
			
			/* offset for start of channel (on LHS of channel-list) */
			if (ale->id) {
				/* special exception for materials and particles */
				if (ELEM(GS(ale->id->name), ID_MA, ID_PA))
					offset = 21 + NLACHANNEL_BUTTON_WIDTH;
				else
					offset = 14;
			}
			else
				offset = 0;
			
			if (x >= (v2d->cur.xmax - NLACHANNEL_BUTTON_WIDTH)) {
				/* toggle protection (only if there's a toggle there) */
				nlt->flag ^= NLATRACK_PROTECTED;
				
				/* notifier flags - channel was edited */
				notifierFlags |= (ND_ANIMCHAN | NA_EDITED);
			}
			else if (x >= (v2d->cur.xmax - 2 * NLACHANNEL_BUTTON_WIDTH)) {
				/* toggle mute */
				nlt->flag ^= NLATRACK_MUTED;
				
				/* notifier flags - channel was edited */
				notifierFlags |= (ND_ANIMCHAN | NA_EDITED);
			}
			else if (x <= ((NLACHANNEL_BUTTON_WIDTH * 2) + offset)) {
				/* toggle 'solo' */
				BKE_nlatrack_solo_toggle(adt, nlt);
				
				/* notifier flags - channel was edited */
				notifierFlags |= (ND_ANIMCHAN | NA_EDITED);
			}
			else if (nlaedit_is_tweakmode_on(ac) == 0) {
				/* set selection */
				if (selectmode == SELECT_INVERT) {
					/* inverse selection status of this F-Curve only */
					nlt->flag ^= NLATRACK_SELECTED;
				}
				else {
					/* select F-Curve by itself */
					ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
					nlt->flag |= NLATRACK_SELECTED;
				}
				
				/* if NLA-Track is selected now, make NLA-Track the 'active' one in the visible list */
				if (nlt->flag & NLATRACK_SELECTED)
					ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, nlt, ANIMTYPE_NLATRACK);
					
				/* notifier flags - channel was selected */
				notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
			}
			break;
		}
		case ANIMTYPE_NLAACTION:
		{
			AnimData *adt = BKE_animdata_from_id(ale->id);
			
			/* button area... */
			if (x >= (v2d->cur.xmax - NLACHANNEL_BUTTON_WIDTH)) {
				if (nlaedit_is_tweakmode_on(ac) == 0) {
					/* 'push-down' action - only usable when not in TweakMode */
					/* TODO: make this use the operator instead of calling the function directly
					 *  however, calling the operator requires that we supply the args, and that works with proper buttons only */
					BKE_nla_action_pushdown(adt);
				}
				else {
					/* when in tweakmode, this button becomes the toggle for mapped editing */
					adt->flag ^= ADT_NLA_EDIT_NOMAP;
				}
				
				/* changes to NLA-Action occurred */
				notifierFlags |= ND_NLA_ACTCHANGE;
			}
			/* OR rest of name... */
			else {
				/* NOTE: rest of NLA-Action name doubles for operating on the AnimData block 
				 * - this is useful when there's no clear divider, and makes more sense in
				 *   the case of users trying to use this to change actions
				 * - in tweakmode, clicking here gets us out of tweakmode, as changing selection
				 *   while in tweakmode is really evil!
				 */
				if (nlaedit_is_tweakmode_on(ac)) {
					/* exit tweakmode immediately */
					nlaedit_disable_tweakmode(ac);
					
					/* changes to NLA-Action occurred */
					notifierFlags |= ND_NLA_ACTCHANGE;
				}
				else {
					/* select/deselect */
					if (selectmode == SELECT_INVERT) {
						/* inverse selection status of this AnimData block only */
						adt->flag ^= ADT_UI_SELECTED;
					}
					else {
						/* select AnimData block by itself */
						ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
						adt->flag |= ADT_UI_SELECTED;
					}
					
					/* set active? */
					if (adt->flag & ADT_UI_SELECTED)
						adt->flag |= ADT_UI_ACTIVE;
					
					notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
				}
			}
			break;
		}
		default:
			if (G.debug & G_DEBUG)
				printf("Error: Invalid channel type in mouse_nla_channels()\n");
			break;
	}
	
	/* free channels */
	ANIM_animdata_freelist(&anim_data);
	
	/* return the notifier-flags set */
	return notifierFlags;
}

/* ------------------- */

/* handle clicking */
static int nlachannels_mouseclick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	bAnimContext ac;
	SpaceNla *snla;
	ARegion *ar;
	View2D *v2d;
	int channel_index;
	int notifierFlags = 0;
	short selectmode;
	float x, y;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	snla = (SpaceNla *)ac.sl;
	ar = ac.ar;
	v2d = &ar->v2d;
	
	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode = SELECT_INVERT;
	else
		selectmode = SELECT_REPLACE;
	
	/* figure out which channel user clicked in 
	 * Note: although channels technically start at y= NLACHANNEL_FIRST, we need to adjust by half a channel's height
	 *		so that the tops of channels get caught ok. Since NLACHANNEL_FIRST is really NLACHANNEL_HEIGHT, we simply use
	 *		NLACHANNEL_HEIGHT_HALF.
	 */
	UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, NLACHANNEL_NAMEWIDTH, NLACHANNEL_STEP(snla), 0, (float)NLACHANNEL_HEIGHT_HALF(snla), x, y, NULL, &channel_index);
	
	/* handle mouse-click in the relevant channel then */
	notifierFlags = mouse_nla_channels(C, &ac, x, channel_index, selectmode);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | notifierFlags, NULL);
	
	return OPERATOR_FINISHED;
}
 
void NLA_OT_channels_click(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Mouse Click on NLA Channels";
	ot->idname = "NLA_OT_channels_click";
	ot->description = "Handle clicks to select NLA channels";
	
	/* api callbacks */
	ot->invoke = nlachannels_mouseclick_invoke;
	ot->poll = ED_operator_nla_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* props */
	prop = RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* *********************************************** */
/* Special Operators */

/* ******************** Action Push Down ******************************** */

static int nlachannels_pushdown_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	AnimData *adt = NULL;
	int channel_index = RNA_int_get(op->ptr, "channel_index");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get anim-channel to use (or more specifically, the animdata block behind it) */
	if (channel_index == -1) {
		PointerRNA adt_ptr = {{NULL}};
		
		/* active animdata block */
		if (nla_panel_context(C, &adt_ptr, NULL, NULL) == 0 || (adt_ptr.data == NULL)) {
			BKE_report(op->reports, RPT_ERROR, "No active AnimData block to use "
			           "(select a datablock expander first or set the appropriate flags on an AnimData block)");
			return OPERATOR_CANCELLED;
		}
		else {
			adt = adt_ptr.data;
		}
	}
	else {
		/* indexed channel */
		ListBase anim_data = {NULL, NULL};
		bAnimListElem *ale;
		int filter;
		
		/* filter channels */
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
		ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		/* get channel from index */
		ale = BLI_findlink(&anim_data, channel_index);
		if (ale == NULL) {
			BKE_reportf(op->reports, RPT_ERROR, "No animation channel found at index %d", channel_index);
			ANIM_animdata_freelist(&anim_data);
			return OPERATOR_CANCELLED;
		}
		else if (ale->type != ANIMTYPE_NLAACTION) {
			BKE_reportf(op->reports, RPT_ERROR, "Animation channel at index %d is not a NLA 'Active Action' channel", channel_index);
			ANIM_animdata_freelist(&anim_data);
			return OPERATOR_CANCELLED;
		}
		
		/* grab AnimData from the channel */
		adt = ale->adt;
		
		/* we don't need anything here anymore, so free it all */
		ANIM_animdata_freelist(&anim_data);
	}
	
	/* double-check that we are free to push down here... */
	if (adt == NULL) {
		BKE_report(op->reports, RPT_WARNING, "Internal Error - AnimData block is not valid");
		return OPERATOR_CANCELLED;
	}
	else if (nlaedit_is_tweakmode_on(&ac)) {
		BKE_report(op->reports, RPT_WARNING,
		           "Cannot push down actions while tweaking a strip's action, exit tweak mode first");
		return OPERATOR_CANCELLED;
	}
	else if (adt->action == NULL) {
		BKE_report(op->reports, RPT_WARNING, "No active action to push down");
		return OPERATOR_CANCELLED;
	}
	else {
		/* 'push-down' action - only usable when not in TweakMode */
		BKE_nla_action_pushdown(adt);
	}
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
	return OPERATOR_FINISHED;
}

void NLA_OT_action_pushdown(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Push Down Action";
	ot->idname = "NLA_OT_action_pushdown";
	ot->description = "Push action down onto the top of the NLA stack as a new strip";
	
	/* callbacks */
	ot->exec = nlachannels_pushdown_exec;
	ot->poll = nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_int(ot->srna, "channel_index", -1, -1, INT_MAX, "Channel Index",
	                       "Index of NLA action channel to perform pushdown operation on",
	                       0, INT_MAX);
	RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/* ******************** Add Tracks Operator ***************************** */
/* Add NLA Tracks to the same AnimData block as a selected track, or above the selected tracks */

/* helper - add NLA Tracks alongside existing ones */
bool nlaedit_add_tracks_existing(bAnimContext *ac, bool above_sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	AnimData *lastAdt = NULL;
	bool added = false;
	
	/* get a list of the (selected) NLA Tracks being shown in the NLA */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* add tracks... */
	for (ale = anim_data.first; ale; ale = ale->next) {
		if (ale->type == ANIMTYPE_NLATRACK) {
			NlaTrack *nlt = (NlaTrack *)ale->data;
			AnimData *adt = ale->adt;
			
			/* check if just adding a new track above this one,
			 * or whether we're adding a new one to the top of the stack that this one belongs to
			 */
			if (above_sel) {
				/* just add a new one above this one */
				add_nlatrack(adt, nlt);
				added = true;
			}
			else if ((lastAdt == NULL) || (adt != lastAdt)) {
				/* add one track to the top of the owning AnimData's stack, then don't add anymore to this stack */
				add_nlatrack(adt, NULL);
				lastAdt = adt;
				added = true;
			}
		}
	}
	
	/* free temp data */
	ANIM_animdata_freelist(&anim_data);
	
	return added;
}

/* helper - add NLA Tracks to empty (and selected) AnimData blocks */
bool nlaedit_add_tracks_empty(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	bool added = false;
	
	/* get a list of the selected AnimData blocks in the NLA */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ANIMDATA | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* check if selected AnimData blocks are empty, and add tracks if so... */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ale->adt;
		
		/* sanity check */
		BLI_assert(adt->flag & ADT_UI_SELECTED);
		
		/* ensure it is empty */
		if (BLI_listbase_is_empty(&adt->nla_tracks)) {
			/* add new track to this AnimData block then */
			add_nlatrack(adt, NULL);
			added = true;
		}
	}
	
	/* cleanup */
	ANIM_animdata_freelist(&anim_data);
	
	return added;
}

/* ----- */

static int nlaedit_add_tracks_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	bool above_sel = RNA_boolean_get(op->ptr, "above_selected");
	bool op_done = false;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* perform adding in two passes - existing first so that we don't double up for empty */
	op_done |= nlaedit_add_tracks_existing(&ac, above_sel);
	op_done |= nlaedit_add_tracks_empty(&ac);
	
	/* done? */
	if (op_done) {
		/* set notifier that things have changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
		
		/* done */
		return OPERATOR_FINISHED;
	}
	else {
		/* failed to add any tracks */
		BKE_report(op->reports, RPT_WARNING,
		           "Select an existing NLA Track or an empty action line first");
		
		/* not done */
		return OPERATOR_CANCELLED;
	}
}

void NLA_OT_tracks_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Tracks";
	ot->idname = "NLA_OT_tracks_add";
	ot->description = "Add NLA-Tracks above/after the selected tracks";
	
	/* api callbacks */
	ot->exec = nlaedit_add_tracks_exec;
	ot->poll = nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "above_selected", 0, "Above Selected", "Add a new NLA Track above every existing selected one");
}

/* ******************** Delete Tracks Operator ***************************** */
/* Delete selected NLA Tracks */

static int nlaedit_delete_tracks_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the AnimData blocks being shown in the NLA */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* delete tracks */
	for (ale = anim_data.first; ale; ale = ale->next) {
		if (ale->type == ANIMTYPE_NLATRACK) {
			NlaTrack *nlt = (NlaTrack *)ale->data;
			AnimData *adt = ale->adt;
			
			/* if track is currently 'solo', then AnimData should have its
			 * 'has solo' flag disabled
			 */
			if (nlt->flag & NLATRACK_SOLO)
				adt->flag &= ~ADT_NLA_SOLO_TRACK;
			
			/* call delete on this track - deletes all strips too */
			free_nlatrack(&adt->nla_tracks, nlt);
		}
	}
	
	/* free temp data */
	ANIM_animdata_freelist(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_tracks_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Tracks";
	ot->idname = "NLA_OT_tracks_delete";
	ot->description = "Delete selected NLA-Tracks and the strips they contain";
	
	/* api callbacks */
	ot->exec = nlaedit_delete_tracks_exec;
	ot->poll = nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *********************************************** */
/* AnimData Related Operators */

/* ******************** Include Objects Operator ***************************** */
/* Include selected objects in NLA Editor, by giving them AnimData blocks 
 * NOTE: This doesn't help for non-object AnimData, where we do not have any effective
 *       selection mechanism in place. Unfortunately, this means that non-object AnimData
 *       once again becomes a second-class citizen here. However, at least for the most 
 *       common use case, we now have a nice shortcut again.
 */

static int nlaedit_objects_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	SpaceNla *snla;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* ensure that filters are set so that the effect will be immediately visible */
	snla = (SpaceNla *)ac.sl;
	if (snla && snla->ads) {
		snla->ads->filterflag &= ~ADS_FILTER_NLA_NOACT;
	}
	
	/* operate on selected objects... */	
	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		/* ensure that object has AnimData... that's all */
		BKE_id_add_animdata(&ob->id);
	}
	CTX_DATA_END;
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_selected_objects_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Include Selected Objects";
	ot->idname = "NLA_OT_selected_objects_add";
	ot->description = "Make selected objects appear in NLA Editor by adding Animation Data";
	
	/* api callbacks */
	ot->exec = nlaedit_objects_add_exec;
	ot->poll = nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *********************************************** */

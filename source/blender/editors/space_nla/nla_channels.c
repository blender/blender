/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "DNA_listBase.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include

/* *********************************************** */
/* Operators for NLA channels-list which need to be different from the standard Animation Editor ones */

/* ******************** Mouse-Click Operator *********************** */
/* Depending on the channel that was clicked on, the mouse click will activate whichever
 * part of the channel is relevant.
 *
 * NOTE: eventually, this should probably be phased out when many of these things are replaced with buttons
 *	--> Most channels are now selection only...
 */

static int mouse_nla_channels (bAnimContext *ac, float x, int channel_index, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	View2D *v2d= &ac->ar->v2d;
	int notifierFlags = 0;
	
	/* get the channel that was clicked on */
		/* filter channels */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	filter= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
		/* get channel from index */
	ale= BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		if (G.f & G_DEBUG)
			printf("Error: animation channel (index = %d) not found in mouse_anim_channels() \n", channel_index);
		
		BLI_freelistN(&anim_data);
		return 0;
	}
	
	/* action to take depends on what channel we've got */
	// WARNING: must keep this in sync with the equivalent function in anim_channels_edit.c
	switch (ale->type) {
		case ANIMTYPE_SCENE:
		{
			Scene *sce= (Scene *)ale->data;
			AnimData *adt= sce->adt;
			
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
			
			notifierFlags |= ND_ANIMCHAN_SELECT;
		}
			break;
		case ANIMTYPE_OBJECT:
		{
			bDopeSheet *ads= (bDopeSheet *)ac->data;
			Scene *sce= (Scene *)ads->source;
			Base *base= (Base *)ale->data;
			Object *ob= base->object;
			AnimData *adt= ob->adt;
			
			if (nlaedit_is_tweakmode_on(ac) == 0) {
				/* set selection status */
				if (selectmode == SELECT_INVERT) {
					/* swap select */
					base->flag ^= SELECT;
					ob->flag= base->flag;
					
					if (adt) adt->flag ^= ADT_UI_SELECTED;
				}
				else {
					Base *b;
					
					/* deselect all */
					// TODO: should this deselect all other types of channels too?
					for (b= sce->base.first; b; b= b->next) {
						b->flag &= ~SELECT;
						b->object->flag= b->flag;
						if (b->object->adt) b->object->adt->flag &= ~(ADT_UI_SELECTED|ADT_UI_ACTIVE);
					}
					
					/* select object now */
					base->flag |= SELECT;
					ob->flag |= SELECT;
					if (adt) adt->flag |= ADT_UI_SELECTED;
				}
				
				/* xxx should be ED_base_object_activate(), but we need context pointer for that... */
				//set_active_base(base);
				if ((adt) && (adt->flag & ADT_UI_SELECTED))
					adt->flag |= ADT_UI_ACTIVE;
				
				/* notifiers - channel was selected */
				notifierFlags |= ND_ANIMCHAN_SELECT;
			}
		}
			break;
			
		case ANIMTYPE_FILLACTD: /* Action Expander */
		case ANIMTYPE_DSMAT:	/* Datablock AnimData Expanders */
		case ANIMTYPE_DSLAM:
		case ANIMTYPE_DSCAM:
		case ANIMTYPE_DSCUR:
		case ANIMTYPE_DSSKEY:
		case ANIMTYPE_DSWOR:
		case ANIMTYPE_DSNTREE:
		case ANIMTYPE_DSPART:
		case ANIMTYPE_DSMBALL:
		case ANIMTYPE_DSARM:
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
			
			notifierFlags |= ND_ANIMCHAN_SELECT;
		}	
			break;
			
		case ANIMTYPE_NLATRACK:
		{
			NlaTrack *nlt= (NlaTrack *)ale->data;
			AnimData *adt= ale->adt;
			short offset;
			
			/* offset for start of channel (on LHS of channel-list) */
			if (ale->id) {
				/* special exception for materials and particles */
				if (ELEM(GS(ale->id->name),ID_MA,ID_PA))
					offset= 21 + NLACHANNEL_BUTTON_WIDTH;
				else
					offset= 14;
			}
			else
				offset= 0;
			
			if (x >= (v2d->cur.xmax-NLACHANNEL_BUTTON_WIDTH)) {
				/* toggle protection (only if there's a toggle there) */
				nlt->flag ^= NLATRACK_PROTECTED;
				
				/* notifier flags - channel was edited */
				notifierFlags |= ND_ANIMCHAN_EDIT;
			}
			else if (x >= (v2d->cur.xmax-2*NLACHANNEL_BUTTON_WIDTH)) {
				/* toggle mute */
				nlt->flag ^= NLATRACK_MUTED;
				
				/* notifier flags - channel was edited */
				notifierFlags |= ND_ANIMCHAN_EDIT;
			}
			else if (x <= ((NLACHANNEL_BUTTON_WIDTH*2)+offset)) {
				/* toggle 'solo' */
				BKE_nlatrack_solo_toggle(adt, nlt);
				
				/* notifier flags - channel was edited */
				notifierFlags |= ND_ANIMCHAN_EDIT;
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
				notifierFlags |= ND_ANIMCHAN_SELECT;
			}
		}
			break;
		case ANIMTYPE_NLAACTION:
		{
			AnimData *adt= BKE_animdata_from_id(ale->id);
			
			if (x >= (v2d->cur.xmax-NLACHANNEL_BUTTON_WIDTH)) {
				if (nlaedit_is_tweakmode_on(ac) == 0) {
					/* 'push-down' action - only usable when not in TweakMode */
					// TODO: make this use the operator instead of calling the function directly
					// 	however, calling the operator requires that we supply the args, and that works with proper buttons only
					BKE_nla_action_pushdown(adt);
				}
				else {
					/* when in tweakmode, this button becomes the toggle for mapped editing */
					adt->flag ^= ADT_NLA_EDIT_NOMAP;
				}
				
				/* changes to NLA-Action occurred */
				notifierFlags |= ND_NLA_ACTCHANGE;
			}
		}
			break;
			
		default:
			if (G.f & G_DEBUG)
				printf("Error: Invalid channel type in mouse_nla_channels() \n");
	}
	
	/* free channels */
	BLI_freelistN(&anim_data);
	
	/* return the notifier-flags set */
	return notifierFlags;
}

/* ------------------- */

/* handle clicking */
static int nlachannels_mouseclick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	Scene *scene;
	ARegion *ar;
	View2D *v2d;
	int mval[2], channel_index;
	int notifierFlags = 0;
	short selectmode;
	float x, y;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	scene= ac.scene;
	ar= ac.ar;
	v2d= &ar->v2d;
	
	/* get mouse coordinates (in region coordinates) */
	mval[0]= (event->x - ar->winrct.xmin);
	mval[1]= (event->y - ar->winrct.ymin);
	
	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
	
	/* figure out which channel user clicked in 
	 * Note: although channels technically start at y= NLACHANNEL_FIRST, we need to adjust by half a channel's height
	 *		so that the tops of channels get caught ok. Since NLACHANNEL_FIRST is really NLACHANNEL_HEIGHT, we simply use
	 *		NLACHANNEL_HEIGHT_HALF.
	 */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, NLACHANNEL_NAMEWIDTH, NLACHANNEL_STEP, 0, (float)NLACHANNEL_HEIGHT_HALF, x, y, NULL, &channel_index);
	
	/* handle mouse-click in the relevant channel then */
	notifierFlags= mouse_nla_channels(&ac, x, channel_index, selectmode);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|notifierFlags, NULL);
	
	return OPERATOR_FINISHED;
}
 
void NLA_OT_channels_click (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Click on Channels";
	ot->idname= "NLA_OT_channels_click";
	
	/* api callbacks */
	ot->invoke= nlachannels_mouseclick_invoke;
	ot->poll= ED_operator_nla_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
}

/* *********************************************** */
/* Special Operators */

/* ******************** Add Tracks Operator ***************************** */
/* Add NLA Tracks to the same AnimData block as a selected track, or above the selected tracks */

static int nlaedit_add_tracks_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	AnimData *lastAdt = NULL;
	short above_sel= RNA_boolean_get(op->ptr, "above_selected");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the AnimData blocks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_SEL);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* add tracks... */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		AnimData *adt= ale->adt;
		
		/* check if just adding a new track above this one,
		 * or whether we're adding a new one to the top of the stack that this one belongs to
		 */
		if (above_sel) {
			/* just add a new one above this one */
			add_nlatrack(adt, nlt);
		}
		else if ((lastAdt == NULL) || (adt != lastAdt)) {
			/* add one track to the top of the owning AnimData's stack, then don't add anymore to this stack */
			add_nlatrack(adt, NULL);
			lastAdt= adt;
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_tracks_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Track(s)";
	ot->idname= "NLA_OT_tracks_add";
	ot->description= "Add NLA-Tracks above/after the selected tracks.";
	
	/* api callbacks */
	ot->exec= nlaedit_add_tracks_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "above_selected", 0, "Above Selected", "Add a new NLA Track above every existing selected one.");
}

/* ******************** Delete Tracks Operator ***************************** */
/* Delete selected NLA Tracks */

static int nlaedit_delete_tracks_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the AnimData blocks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_SEL);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* delete tracks */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		AnimData *adt= ale->adt;
		
		/* call delete on this track - deletes all strips too */
		free_nlatrack(&adt->nla_tracks, nlt);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_delete_tracks (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Tracks";
	ot->idname= "NLA_OT_delete_tracks";
	ot->description= "Delete selected NLA-Tracks and the strips they contain.";
	
	/* api callbacks */
	ot->exec= nlaedit_delete_tracks_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *********************************************** */

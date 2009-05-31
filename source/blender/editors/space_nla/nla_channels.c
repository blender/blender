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
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_context.h"
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

/* ******************** Borderselect Operator *********************** */

static void borderselect_nla_channels (bAnimContext *ac, rcti *rect, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	float ymin=(float)(-NLACHANNEL_HEIGHT), ymax=0;
	
	/* convert border-region to view coordinates */
	UI_view2d_region_to_view(v2d, rect->xmin, rect->ymin+2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect->xmax, rect->ymax-2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		ymax= ymin + NLACHANNEL_STEP;
		
		/* if channel is within border-select region, alter it */
		if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
			/* only the following types can be selected */
			switch (ale->type) {
				case ANIMTYPE_OBJECT: /* object */
				{
					Base *base= (Base *)ale->data;
					Object *ob= base->object;
					
					ACHANNEL_SET_FLAG(base, selectmode, SELECT);
					ACHANNEL_SET_FLAG(ob, selectmode, SELECT);
				}
					break;
				case ANIMTYPE_NLATRACK: /* nla-track */
				{
					NlaTrack *nlt= (NlaTrack *)ale->data;
					
					ACHANNEL_SET_FLAG(nlt, selectmode, NLATRACK_SELECTED);
				}
					break;
			}
		}
		
		/* set maximum extent to be the minimum of the next channel */
		ymin= ymax;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int nlachannels_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short selectmode=0;
	int event;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	event= RNA_int_get(op->ptr, "event_type");
	if (event == LEFTMOUSE) // FIXME... hardcoded
		selectmode = ACHANNEL_SETFLAG_ADD;
	else
		selectmode = ACHANNEL_SETFLAG_CLEAR;
	
	/* apply borderselect animation channels */
	borderselect_nla_channels(&ac, &rect, selectmode);
	
	return OPERATOR_FINISHED;
} 

void NLA_OT_channels_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "NLA_OT_channels_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= nlachannels_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
}

/* ******************** Mouse-Click Operator *********************** */
/* Depending on the channel that was clicked on, the mouse click will activate whichever
 * part of the channel is relevant.
 *
 * NOTE: eventually, this should probably be phased out when many of these things are replaced with buttons
 */

static void mouse_nla_channels (bAnimContext *ac, float x, int channel_index, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get the channel that was clicked on */
		/* filter channels */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	filter= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
		/* get channel from index */
	ale= BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		printf("Error: animation channel (index = %d) not found in mouse_anim_channels() \n", channel_index);
		
		BLI_freelistN(&anim_data);
		return;
	}
	
	/* action to take depends on what channel we've got */
	switch (ale->type) {
		case ANIMTYPE_SCENE:
		{
			Scene *sce= (Scene *)ale->data;
			
			if (x < 16) {
				/* toggle expand */
				sce->flag ^= SCE_DS_COLLAPSED;
			}
			else {
				/* set selection status */
				if (selectmode == SELECT_INVERT) {
					/* swap select */
					sce->flag ^= SCE_DS_SELECTED;
				}
				else {
					sce->flag |= SCE_DS_SELECTED;
				}
			}
		}
			break;
		case ANIMTYPE_OBJECT:
		{
			bDopeSheet *ads= (bDopeSheet *)ac->data;
			Scene *sce= (Scene *)ads->source;
			Base *base= (Base *)ale->data;
			Object *ob= base->object;
			
			if (x < 16) {
				/* toggle expand */
				ob->nlaflag ^= OB_ADS_COLLAPSED; // XXX 
			}
			else {
				/* set selection status */
				if (selectmode == SELECT_INVERT) {
					/* swap select */
					base->flag ^= SELECT;
					ob->flag= base->flag;
				}
				else {
					Base *b;
					
					/* deleselect all */
					for (b= sce->base.first; b; b= b->next) {
						b->flag &= ~SELECT;
						b->object->flag= b->flag;
					}
					
					/* select object now */
					base->flag |= SELECT;
					ob->flag |= SELECT;
				}
				
				/* xxx should be ED_base_object_activate(), but we need context pointer for that... */
				//set_active_base(base);
			}
		}
			break;
		case ANIMTYPE_FILLMATD:
		{
			Object *ob= (Object *)ale->data;
			ob->nlaflag ^= OB_ADS_SHOWMATS;	// XXX 
		}
			break;
				
		case ANIMTYPE_DSMAT:
		{
			Material *ma= (Material *)ale->data;
			ma->flag ^= MA_DS_EXPAND;
		}
			break;
		case ANIMTYPE_DSLAM:
		{
			Lamp *la= (Lamp *)ale->data;
			la->flag ^= LA_DS_EXPAND;
		}
			break;
		case ANIMTYPE_DSCAM:
		{
			Camera *ca= (Camera *)ale->data;
			ca->flag ^= CAM_DS_EXPAND;
		}
			break;
		case ANIMTYPE_DSCUR:
		{
			Curve *cu= (Curve *)ale->data;
			cu->flag ^= CU_DS_EXPAND;
		}
			break;
		case ANIMTYPE_DSSKEY:
		{
			Key *key= (Key *)ale->data;
			key->flag ^= KEYBLOCK_DS_EXPAND;
		}
			break;
		case ANIMTYPE_DSWOR:
		{
			World *wo= (World *)ale->data;
			wo->flag ^= WO_DS_EXPAND;
		}
			break;
			
		case ANIMTYPE_NLATRACK:
		{
			NlaTrack *nlt= (NlaTrack *)ale->data;
			
			if (x >= (NLACHANNEL_NAMEWIDTH-NLACHANNEL_BUTTON_WIDTH)) {
				/* toggle protection (only if there's a toggle there) */
				nlt->flag ^= NLATRACK_PROTECTED;
			}
			else if (x >= (NLACHANNEL_NAMEWIDTH-2*NLACHANNEL_BUTTON_WIDTH)) {
				/* toggle mute */
				nlt->flag ^= NLATRACK_MUTED;
			}
			else {
				/* set selection */
				if (selectmode == SELECT_INVERT) {
					/* inverse selection status of this F-Curve only */
					nlt->flag ^= NLATRACK_SELECTED;
				}
				else {
					/* select F-Curve by itself */
					ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
					nlt->flag |= NLATRACK_SELECTED;
				}
				
				/* if NLA-Track is selected now, make NLA-Track the 'active' one in the visible list */
				if (nlt->flag & NLATRACK_SELECTED)
					ANIM_set_active_channel(ac->data, ac->datatype, filter, nlt, ANIMTYPE_NLATRACK);
			}
		}
			break;
		case ANIMTYPE_NLAACTION:
		{
			AnimData *adt= BKE_animdata_from_id(ale->owner); /* this won't crash, right? */
			
			/* for now, only do something if user clicks on the 'push-down' button */
			if (x >= (NLACHANNEL_NAMEWIDTH-NLACHANNEL_BUTTON_WIDTH)) {
				/* activate push-down operator */
				// TODO: make this use the operator instead of calling the function directly
				// 	however, calling the operator requires that we supply the args, and that works with proper buttons only
				BKE_nla_action_pushdown(adt);
			}
		}
			break;
			
		default:
			printf("Error: Invalid channel type in mouse_nla_channels() \n");
	}
	
	/* free channels */
	BLI_freelistN(&anim_data);
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
	 * Note: although channels technically start at y= ACHANNEL_FIRST, we need to adjust by half a channel's height
	 *		so that the tops of channels get caught ok. Since NLACHANNEL_FIRST is really NLACHANNEL_HEIGHT, we simply use
	 *		NLACHANNEL_HEIGHT_HALF.
	 */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, NLACHANNEL_NAMEWIDTH, NLACHANNEL_STEP, 0, (float)NLACHANNEL_HEIGHT_HALF, x, y, NULL, &channel_index);
	
	/* handle mouse-click in the relevant channel then */
	mouse_nla_channels(&ac, x, channel_index, selectmode);
	
	/* set notifier tha things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_CHANNELS);
	
	return OPERATOR_FINISHED;
}
 
void NLA_OT_channels_click (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Click on Channels";
	ot->idname= "NLA_OT_channels_click";
	
	/* api callbacks */
	ot->invoke= nlachannels_mouseclick_invoke;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
}

/* *********************************************** */

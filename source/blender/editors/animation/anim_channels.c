/**
 * $Id: editaction.c 17746 2008-12-08 11:19:44Z aligorith $
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

#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
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

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h" // XXX move the select modes out of there!
#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************************************************************** */
/* CHANNELS API */

/* -------------------------- Internal Macros ------------------------------- */

/* set/clear/toggle macro 
 *	- channel - channel with a 'flag' member that we're setting
 *	- smode - 0=clear, 1=set, 2=toggle
 *	- sflag - bitflag to set
 */
#define ACHANNEL_SET_FLAG(channel, smode, sflag) \
	{ \
		if (smode == ACHANNEL_SETFLAG_TOGGLE) 	(channel)->flag ^= (sflag); \
		else if (smode == ACHANNEL_SETFLAG_ADD) (channel)->flag |= (sflag); \
		else 									(channel)->flag &= ~(sflag); \
	}

/* -------------------------- Internal Tools -------------------------------- */

/* -------------------------- Exposed API ----------------------------------- */



/* Deselect all animation channels 
 *	- data: pointer to datatype, as contained in bAnimContext
 *	- datatype: the type of data that 'data' represents (eAnim_ChannelType)
 *	- test: check if deselecting instead of selecting
 *	- sel: eAnimChannels_SetFlag;
 */
void ANIM_deselect_anim_channels (void *data, short datatype, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= ANIMFILTER_VISIBLE;
	ANIM_animdata_filter(&anim_data, filter, data, datatype);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			if (sel == 0) 
				break;
			
			switch (ale->type) {
				case ANIMTYPE_OBJECT:
					if (ale->flag & SELECT)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_FILLACTD:
					if (ale->flag & ACTC_SELECTED)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_GROUP:
					if (ale->flag & AGRP_SELECTED)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_ACHAN:
					if (ale->flag & ACHAN_SELECTED) 
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_CONCHAN:
					if (ale->flag & CONSTRAINT_CHANNEL_SELECT) 
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_ICU:
					if (ale->flag & IPO_SELECT)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
			}
		}
	}
		
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ANIMTYPE_OBJECT:
			{
				Base *base= (Base *)ale->data;
				Object *ob= base->object;
				
				ACHANNEL_SET_FLAG(base, sel, SELECT);
				ACHANNEL_SET_FLAG(ob, sel, SELECT);
			}
				break;
			case ANIMTYPE_FILLACTD:
			{
				bAction *act= (bAction *)ale->data;
				
				ACHANNEL_SET_FLAG(act, sel, ACTC_SELECTED);
			}
				break;
			case ANIMTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				
				ACHANNEL_SET_FLAG(agrp, sel, AGRP_SELECTED);
				agrp->flag &= ~AGRP_ACTIVE;
			}
				break;
			case ANIMTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				
				ACHANNEL_SET_FLAG(achan, sel, ACHAN_SELECTED);
				
				//select_poseelement_by_name(achan->name, sel); // XXX
				achan->flag &= ~ACHAN_HILIGHTED;
			}
				break;
			case ANIMTYPE_CONCHAN:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)ale->data;
				
				ACHANNEL_SET_FLAG(conchan, sel, CONSTRAINT_CHANNEL_SELECT);
			}
				break;
			case ANIMTYPE_ICU:
			{
				IpoCurve *icu= (IpoCurve *)ale->data;
				
				ACHANNEL_SET_FLAG(icu, sel, IPO_SELECT);
				icu->flag &= ~IPO_ACTIVE;
			}
				break;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ************************************************************************** */
/* OPERATORS */

/* ********************** Select All Operator *********************** */

static int animchannels_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behaviour - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		ANIM_deselect_anim_channels(ac.data, ac.datatype, 0, ACHANNEL_SETFLAG_TOGGLE);
	else
		ANIM_deselect_anim_channels(ac.data, ac.datatype, 1, ACHANNEL_SETFLAG_ADD);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_channels_deselectall (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "ANIM_OT_channels_deselectall";
	
	/* api callbacks */
	ot->exec= animchannels_deselectall_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	RNA_def_property(ot->srna, "invert", PROP_BOOLEAN, PROP_NONE);
}

/* ******************** Mouse-Click Operator *********************** */
/* Depending on the channel that was clicked on, the mouse click will activate whichever
 * part of the channel is relevant.
 *
 * NOTE: eventually, this should probably be phased out when many of these things are replaced with buttons
 */

static void mouse_anim_channels (bAnimContext *ac, float x, int channel_index, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get the channel that was clicked on */
		/* filter channels */
	filter= (ANIMFILTER_FORDRAWING | ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	filter= ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
		/* get channel from index */
	ale= BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		printf("Error: animation channel not found in mouse_anim_channels() \n");
			// XXX remove me..
		printf("\t channel index = %d, channels = %d\n", channel_index, filter);
		
		BLI_freelistN(&anim_data);
		return;
	}
	
	/* action to take depends on what channel we've got */
	switch (ale->type) {
		case ANIMTYPE_OBJECT:
			{
				bDopeSheet *ads= (bDopeSheet *)ac->data;
				Scene *sce= (Scene *)ads->source;
				Base *base= (Base *)ale->data;
				Object *ob= base->object;
				
				if (x < 16) {
					/* toggle expand */
					ob->nlaflag ^= OB_ADS_COLLAPSED;
				}
				else {
					/* set selection status */
					// FIXME: this needs to use the new stuff...
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
					
					//set_active_base(base);	/* editview.c */
				}
			}
				break;
		case ANIMTYPE_FILLIPOD:
			{
				Object *ob= (Object *)ale->data;
				ob->nlaflag ^= OB_ADS_SHOWIPO;
			}
				break;
		case ANIMTYPE_FILLACTD:
			{
				bAction *act= (bAction *)ale->data;
				act->flag ^= ACTC_EXPANDED;
			}
				break;
		case ANIMTYPE_FILLCOND:
			{
				Object *ob= (Object *)ale->data;
				ob->nlaflag ^= OB_ADS_SHOWCONS;
			}
				break;
		case ANIMTYPE_FILLMATD:
			{
				Object *ob= (Object *)ale->data;
				ob->nlaflag ^= OB_ADS_SHOWMATS;
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
			
		case ANIMTYPE_GROUP: 
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				short offset= (ac->datatype == ANIMCONT_DOPESHEET)? 21 : 0;
				
				if ((x < (offset+17)) && (agrp->channels.first)) {
					/* toggle expand */
					agrp->flag ^= AGRP_EXPANDED;
				}
				else if (x >= (ACHANNEL_NAMEWIDTH-ACHANNEL_BUTTON_WIDTH)) {
					/* toggle protection/locking */
					agrp->flag ^= AGRP_PROTECTED;
 				}
 				else {
					/* select/deselect group */
					if (selectmode == SELECT_INVERT) {
						/* inverse selection status of group */
						//select_action_group(act, agrp, SELECT_INVERT);
					}
					else if (/*G.qual == (LR_CTRLKEY|LR_SHIFTKEY)*/selectmode == -1) {
						// FIXME: need a special case for this!
						/* select all in group (and deselect everthing else) */	
						//select_action_group_channels(act, agrp);
						//select_action_group(act, agrp, SELECT_ADD);
					}
					else {
						/* select group by itself */
						ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
						//select_action_group(act, agrp, SELECT_ADD);
					}
					
					// XXX
					agrp->flag ^= AGRP_SELECTED;
 				}
			}
			break;
		case ANIMTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				short offset= (ac->datatype == ANIMCONT_DOPESHEET)? 21 : 0;
				
				if (x >= (ACHANNEL_NAMEWIDTH-ACHANNEL_BUTTON_WIDTH)) {
					/* toggle protect */
					achan->flag ^= ACHAN_PROTECTED;
				}
				else if ((x >= (ACHANNEL_NAMEWIDTH-2*ACHANNEL_BUTTON_WIDTH)) && (achan->ipo)) {
					/* toggle mute */
					achan->ipo->muteipo = (achan->ipo->muteipo)? 0: 1;
				}
				else if (x <= (offset+17)) {
					/* toggle expand */
					achan->flag ^= ACHAN_EXPANDED;
				}				
				else {
					/* select/deselect achan */		
					if (selectmode == SELECT_INVERT) {
						//select_channel(act, achan, SELECT_INVERT);
					}
					else {
						ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
						//select_channel(act, achan, SELECT_ADD);
					}
					
					/* messy... set active bone */
					//select_poseelement_by_name(achan->name, 2);
					
					// XXX for now only
					achan->flag ^= ACHAN_SELECTED;
				}
			}
				break;
		case ANIMTYPE_FILLIPO:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				
				achan->flag ^= ACHAN_SHOWIPO;
				
				if ((x > 24) && (achan->flag & ACHAN_SHOWIPO)) {
					/* select+make active achan */		
					ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
					//select_channel(act, achan, SELECT_ADD);
					
					/* messy... set active bone */
					//select_poseelement_by_name(achan->name, 2);
					
					// XXX for now only
					achan->flag ^= ACHAN_SELECTED;
				}	
			}
			break;
		case ANIMTYPE_FILLCON:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				
				achan->flag ^= ACHAN_SHOWCONS;
				
				if ((x > 24) && (achan->flag & ACHAN_SHOWCONS)) {
					/* select+make active achan */	
					ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
					//select_channel(act, achan, SELECT_ADD);
					
					/* messy... set active bone */
					//select_poseelement_by_name(achan->name, 2);
					
					// XXX for now only
					achan->flag ^= ACHAN_SELECTED;
				}	
			}
			break;
		case ANIMTYPE_ICU: 
			{
				IpoCurve *icu= (IpoCurve *)ale->data;
				
				if (x >= (ACHANNEL_NAMEWIDTH-ACHANNEL_BUTTON_WIDTH)) {
					/* toggle protection */
					icu->flag ^= IPO_PROTECT;
				}
				else if (x >= (ACHANNEL_NAMEWIDTH-2*ACHANNEL_BUTTON_WIDTH)) {
					/* toggle mute */
					icu->flag ^= IPO_MUTE;
				}
				else {
					/* select/deselect */
					//select_icu_channel(act, icu, SELECT_INVERT);
					
					// XXX for now only
					icu->flag ^= IPO_SELECT;
				}
			}
			break;
		case ANIMTYPE_CONCHAN:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)ale->data;
				
				if (x >= (ACHANNEL_NAMEWIDTH-16)) {
					/* toggle protection */
					conchan->flag ^= CONSTRAINT_CHANNEL_PROTECTED;
				}
				else if ((x >= (ACHANNEL_NAMEWIDTH-32)) && (conchan->ipo)) {
					/* toggle mute */
					conchan->ipo->muteipo = (conchan->ipo->muteipo)? 0: 1;
				}
				else {
					/* select/deselect */
					//select_constraint_channel(act, conchan, SELECT_INVERT);
					
					// XXX for now only
					conchan->flag ^= CONSTRAINT_CHANNEL_SELECT;
				}
			}
				break;
		case ANIMTYPE_GPDATABLOCK:
			{
				bGPdata *gpd= (bGPdata *)ale->data;
				
				/* toggle expand */
				gpd->flag ^= GP_DATA_EXPAND;
			}
				break;
		case ANIMTYPE_GPLAYER:
			{
#if 0 // XXX future of this is unclear
				bGPdata *gpd= (bGPdata *)ale->owner;
				bGPDlayer *gpl= (bGPDlayer *)ale->data;
				
				if (x >= (ACHANNEL_NAMEWIDTH-16)) {
					/* toggle lock */
					gpl->flag ^= GP_LAYER_LOCKED;
				}
				else if (x >= (ACHANNEL_NAMEWIDTH-32)) {
					/* toggle hide */
					gpl->flag ^= GP_LAYER_HIDE;
				}
				else {
					/* select/deselect */
					//if (G.qual & LR_SHIFTKEY) {
						//select_gplayer_channel(gpd, gpl, SELECT_INVERT);
					//}
					//else {
						//deselect_gpencil_layers(data, 0);
						//select_gplayer_channel(gpd, gpl, SELECT_INVERT);
					//}
				}
#endif // XXX future of this is unclear
			}
				break;
		case ANIMTYPE_SHAPEKEY:
			/* TODO: shapekey channels cannot be selected atm... */
			break;
		default:
			printf("Error: Invalid channel type in mouse_anim_channels() \n");
	}
	
	/* free channels */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

/* handle clicking */
static int animchannels_mouseclick_invoke(bContext *C, wmOperator *op, wmEvent *event)
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
	if (RNA_boolean_get(op->ptr, "extend_select"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
	
	/* figure out which channel user clicked in 
	 * Note: although channels technically start at y= ACHANNEL_FIRST, we need to adjust by half a channel's height
	 *		so that the tops of channels get caught ok. Since ACHANNEL_FIRST is really ACHANNEL_HEIGHT, we simply use
	 *		ACHANNEL_HEIGHT_HALF.
	 */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, ACHANNEL_NAMEWIDTH, ACHANNEL_STEP, 0, (float)ACHANNEL_HEIGHT_HALF, x, y, NULL, &channel_index);
	
	/* handle mouse-click in the relevant channel then */
	mouse_anim_channels(&ac, x, channel_index, selectmode);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_channels_mouseclick (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Click on Channels";
	ot->idname= "ANIM_OT_channels_mouseclick";
	
	/* api callbacks */
	ot->invoke= animchannels_mouseclick_invoke;
	ot->poll= ED_operator_areaactive;
	
	/* id-props */
	RNA_def_property(ot->srna, "extend_select", PROP_BOOLEAN, PROP_NONE); // SHIFTKEY
}

/* ************************************************************************** */
/* Operator Registration */

void ED_operatortypes_animchannels(void)
{
	WM_operatortype_append(ANIM_OT_channels_deselectall);
	WM_operatortype_append(ANIM_OT_channels_mouseclick);
}

void ED_keymap_animchannels(wmWindowManager *wm)
{
	ListBase *keymap = WM_keymap_listbase(wm, "Animation_Channels", 0, 0);
	
	/* click-select */
		// XXX for now, only leftmouse.... 
	WM_keymap_add_item(keymap, "ANIM_OT_channels_mouseclick", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_mouseclick", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend_select", 1);
	
	/* deselect all */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_deselectall", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_deselectall", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", 1);
}

/* ************************************************************************** */

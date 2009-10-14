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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
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
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
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

/* -------------------------- Exposed API ----------------------------------- */

/* Set the given animation-channel as the active one for the active context */
// TODO: extend for animdata types...
void ANIM_set_active_channel (bAnimContext *ac, void *data, short datatype, int filter, void *channel_data, short channel_type)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	
	/* try to build list of filtered items */
	ANIM_animdata_filter(ac, &anim_data, filter, data, datatype);
	if (anim_data.first == NULL)
		return;
		
	/* only clear the 'active' flag for the channels of the same type */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* skip if types don't match */
		if (channel_type != ale->type)
			continue;
		
		/* flag to set depends on type */
		switch (ale->type) {
			case ANIMTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				
				ACHANNEL_SET_FLAG(agrp, ACHANNEL_SETFLAG_CLEAR, AGRP_ACTIVE);
			}
				break;
			case ANIMTYPE_FCURVE:
			{
				FCurve *fcu= (FCurve *)ale->data;
				
				ACHANNEL_SET_FLAG(fcu, ACHANNEL_SETFLAG_CLEAR, FCURVE_ACTIVE);
			}
				break;
			case ANIMTYPE_NLATRACK:
			{
				NlaTrack *nlt= (NlaTrack *)ale->data;
				
				ACHANNEL_SET_FLAG(nlt, ACHANNEL_SETFLAG_CLEAR, NLATRACK_ACTIVE);
			}
				break;
			
			case ANIMTYPE_FILLACTD: /* Action Expander */
			case ANIMTYPE_DSMAT:	/* Datablock AnimData Expanders */
			case ANIMTYPE_DSLAM:
			case ANIMTYPE_DSCAM:
			case ANIMTYPE_DSCUR:
			case ANIMTYPE_DSSKEY:
			case ANIMTYPE_DSWOR:
			case ANIMTYPE_DSPART:
			case ANIMTYPE_DSMBALL:
			case ANIMTYPE_DSARM:
			{
				/* need to verify that this data is valid for now */
				if (ale->adt) {
					ACHANNEL_SET_FLAG(ale->adt, ACHANNEL_SETFLAG_CLEAR, ADT_UI_ACTIVE);
				}
			}
				break;
		}
	}
	
	/* set active flag */
	if (channel_data) {
		switch (channel_type) {
			case ANIMTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)channel_data;
				agrp->flag |= AGRP_ACTIVE;
			}
				break;
			case ANIMTYPE_FCURVE:
			{
				FCurve *fcu= (FCurve *)channel_data;
				fcu->flag |= FCURVE_ACTIVE;
			}
				break;
			case ANIMTYPE_NLATRACK:
			{
				NlaTrack *nlt= (NlaTrack *)channel_data;
				nlt->flag |= NLATRACK_ACTIVE;
			}
				break;
				
			case ANIMTYPE_FILLACTD: /* Action Expander */
			case ANIMTYPE_DSMAT:	/* Datablock AnimData Expanders */
			case ANIMTYPE_DSLAM:
			case ANIMTYPE_DSCAM:
			case ANIMTYPE_DSCUR:
			case ANIMTYPE_DSSKEY:
			case ANIMTYPE_DSWOR:
			case ANIMTYPE_DSPART:
			case ANIMTYPE_DSMBALL:
			case ANIMTYPE_DSARM:
			{
				/* need to verify that this data is valid for now */
				if (ale->adt)
					ale->adt->flag |= ADT_UI_ACTIVE;
			}
				break;
		}
	}
	
	/* clean up */
	BLI_freelistN(&anim_data);
}

/* Deselect all animation channels 
 *	- data: pointer to datatype, as contained in bAnimContext
 *	- datatype: the type of data that 'data' represents (eAnimCont_Types)
 *	- test: check if deselecting instead of selecting
 *	- sel: eAnimChannels_SetFlag;
 */
void ANIM_deselect_anim_channels (void *data, short datatype, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS;
	ANIM_animdata_filter(NULL, &anim_data, filter, data, datatype);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			if (sel == 0) 
				break;
			
			switch (ale->type) {
				case ANIMTYPE_SCENE:
					if (ale->flag & SCE_DS_SELECTED)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_OBJECT:
				#if 0	/* for now, do not take object selection into account, since it gets too annoying */
					if (ale->flag & SELECT)
						sel= ACHANNEL_SETFLAG_CLEAR;
				#endif
					break;
				case ANIMTYPE_GROUP:
					if (ale->flag & AGRP_SELECTED)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_FCURVE:
					if (ale->flag & FCURVE_SELECTED)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
				case ANIMTYPE_NLATRACK:
					if (ale->flag & NLATRACK_SELECTED)
						sel= ACHANNEL_SETFLAG_CLEAR;
					break;
					
				case ANIMTYPE_FILLACTD: /* Action Expander */
				case ANIMTYPE_DSMAT:	/* Datablock AnimData Expanders */
				case ANIMTYPE_DSLAM:
				case ANIMTYPE_DSCAM:
				case ANIMTYPE_DSCUR:
				case ANIMTYPE_DSSKEY:
				case ANIMTYPE_DSWOR:
				case ANIMTYPE_DSPART:
				case ANIMTYPE_DSMBALL:
				case ANIMTYPE_DSARM:
				{
					if ((ale->adt) && (ale->adt->flag & ADT_UI_SELECTED))
						sel= ACHANNEL_SETFLAG_CLEAR;
				}
					break;
			}
		}
	}
		
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ANIMTYPE_SCENE:
			{
				Scene *scene= (Scene *)ale->data;
				
				ACHANNEL_SET_FLAG(scene, sel, SCE_DS_SELECTED);
				
				if (scene->adt) {
					ACHANNEL_SET_FLAG(scene, sel, ADT_UI_SELECTED);
				}
			}
				break;
			case ANIMTYPE_OBJECT:
			#if 0	/* for now, do not take object selection into account, since it gets too annoying */
			{
				Base *base= (Base *)ale->data;
				Object *ob= base->object;
				
				ACHANNEL_SET_FLAG(base, sel, SELECT);
				ACHANNEL_SET_FLAG(ob, sel, SELECT);
				
				if (ob->adt) {
					ACHANNEL_SET_FLAG(ob, sel, ADT_UI_SELECTED);
				}
			}
			#endif
				break;
			case ANIMTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				
				ACHANNEL_SET_FLAG(agrp, sel, AGRP_SELECTED);
				agrp->flag &= ~AGRP_ACTIVE;
			}
				break;
			case ANIMTYPE_FCURVE:
			{
				FCurve *fcu= (FCurve *)ale->data;
				
				ACHANNEL_SET_FLAG(fcu, sel, FCURVE_SELECTED);
				fcu->flag &= ~FCURVE_ACTIVE;
			}
				break;
			case ANIMTYPE_NLATRACK:
			{
				NlaTrack *nlt= (NlaTrack *)ale->data;
				
				ACHANNEL_SET_FLAG(nlt, sel, NLATRACK_SELECTED);
				nlt->flag &= ~NLATRACK_ACTIVE;
			}
				break;
				
			case ANIMTYPE_FILLACTD: /* Action Expander */
			case ANIMTYPE_DSMAT:	/* Datablock AnimData Expanders */
			case ANIMTYPE_DSLAM:
			case ANIMTYPE_DSCAM:
			case ANIMTYPE_DSCUR:
			case ANIMTYPE_DSSKEY:
			case ANIMTYPE_DSWOR:
			case ANIMTYPE_DSPART:
			case ANIMTYPE_DSMBALL:
			case ANIMTYPE_DSARM:
			{
				/* need to verify that this data is valid for now */
				if (ale->adt) {
					ACHANNEL_SET_FLAG(ale->adt, sel, ADT_UI_SELECTED);
					ale->adt->flag &= ~ADT_UI_ACTIVE;
				}
			}
				break;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ************************************************************************** */
/* OPERATORS */

/* ****************** Operator Utilities ********************************** */

/* poll callback for being in an Animation Editor channels list region */
int animedit_poll_channels_active (bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* channels region test */
	// TODO: could enhance with actually testing if channels region?
	if (ELEM(NULL, sa, CTX_wm_region(C)))
		return 0;
	/* animation editor test */
	if (ELEM3(sa->spacetype, SPACE_ACTION, SPACE_IPO, SPACE_NLA) == 0)
		return 0;
		
	return 1;
}

/* poll callback for Animation Editor channels list region + not in NLA-tweakmode for NLA */
int animedit_poll_channels_nla_tweakmode_off (bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);
	
	/* channels region test */
	// TODO: could enhance with actually testing if channels region?
	if (ELEM(NULL, sa, CTX_wm_region(C)))
		return 0;
	/* animation editor test */
	if (ELEM3(sa->spacetype, SPACE_ACTION, SPACE_IPO, SPACE_NLA) == 0)
		return 0;
	
	/* NLA TweakMode test */	
	if (sa->spacetype == SPACE_NLA) {
		if ((scene == NULL) || (scene->flag & SCE_NLA_EDIT_ON))
			return 0;
	}
		
	return 1;
}

/* ****************** Rearrange Channels Operator ******************* */
/* This operator only works for Action Editor mode for now, as having it elsewhere makes things difficult */

#if 0 // XXX old animation system - needs to be updated for new system...

/* constants for channel rearranging */
/* WARNING: don't change exising ones without modifying rearrange func accordingly */
enum {
	REARRANGE_ACTCHAN_TOP= -2,
	REARRANGE_ACTCHAN_UP= -1,
	REARRANGE_ACTCHAN_DOWN= 1,
	REARRANGE_ACTCHAN_BOTTOM= 2
};

/* make sure all action-channels belong to a group (and clear action's list) */
static void split_groups_action_temp (bAction *act, bActionGroup *tgrp)
{
	bActionChannel *achan;
	bActionGroup *agrp;
	
	/* Separate action-channels into lists per group */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		if (agrp->channels.first) {
			achan= agrp->channels.last;
			act->chanbase.first= achan->next;
			
			achan= agrp->channels.first;
			achan->prev= NULL;
			
			achan= agrp->channels.last;
			achan->next= NULL;
		}
	}
	
	/* Initialise memory for temp-group */
	memset(tgrp, 0, sizeof(bActionGroup));
	tgrp->flag |= (AGRP_EXPANDED|AGRP_TEMP);
	strcpy(tgrp->name, "#TempGroup");
		
	/* Move any action-channels not already moved, to the temp group */
	if (act->chanbase.first) {
		/* start of list */
		achan= act->chanbase.first;
		achan->prev= NULL;
		tgrp->channels.first= achan;
		act->chanbase.first= NULL;
		
		/* end of list */
		achan= act->chanbase.last;
		achan->next= NULL;
		tgrp->channels.last= achan;
		act->chanbase.last= NULL;
	}
	
	/* Add temp-group to list */
	BLI_addtail(&act->groups, tgrp);
}

/* link lists of channels that groups have */
static void join_groups_action_temp (bAction *act)
{
	bActionGroup *agrp;
	bActionChannel *achan;
	
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		ListBase tempGroup;
		
		/* add list of channels to action's channels */
		tempGroup= agrp->channels;
		addlisttolist(&act->chanbase, &agrp->channels);
		agrp->channels= tempGroup;
		
		/* clear moved flag */
		agrp->flag &= ~AGRP_MOVED;
		
		/* if temp-group... remove from list (but don't free as it's on the stack!) */
		if (agrp->flag & AGRP_TEMP) {
			BLI_remlink(&act->groups, agrp);
			break;
		}
	}
	
	/* clear "moved" flag from all achans */
	for (achan= act->chanbase.first; achan; achan= achan->next) 
		achan->flag &= ~ACHAN_MOVED;
}


static short rearrange_actchannel_is_ok (Link *channel, short type)
{
	if (type == ANIMTYPE_GROUP) {
		bActionGroup *agrp= (bActionGroup *)channel;
		
		if (SEL_AGRP(agrp) && !(agrp->flag & AGRP_MOVED))
			return 1;
	}
	else if (type == ANIMTYPE_ACHAN) {
		bActionChannel *achan= (bActionChannel *)channel;
		
		if (VISIBLE_ACHAN(achan) && SEL_ACHAN(achan) && !(achan->flag & ACHAN_MOVED))
			return 1;
	}
	
	return 0;
}

static short rearrange_actchannel_after_ok (Link *channel, short type)
{
	if (type == ANIMTYPE_GROUP) {
		bActionGroup *agrp= (bActionGroup *)channel;
		
		if (agrp->flag & AGRP_TEMP)
			return 0;
	}
	
	return 1;
}


static short rearrange_actchannel_top (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		/* take it out off the chain keep data */
		BLI_remlink(list, channel);
		
		/* make it first element */
		BLI_insertlinkbefore(list, list->first, channel);
		
		return 1;
	}
	
	return 0;
}

static short rearrange_actchannel_up (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		Link *prev= channel->prev;
		
		if (prev) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* push it up */
			BLI_insertlinkbefore(list, prev, channel);
			
			return 1;
		}
	}
	
	return 0;
}

static short rearrange_actchannel_down (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		Link *next = (channel->next) ? channel->next->next : NULL;
		
		if (next) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* move it down */
			BLI_insertlinkbefore(list, next, channel);
			
			return 1;
		}
		else if (rearrange_actchannel_after_ok(list->last, type)) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* add at end */
			BLI_addtail(list, channel);
			
			return 1;
		}
		else {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* add just before end */
			BLI_insertlinkbefore(list, list->last, channel);
			
			return 1;
		}
	}
	
	return 0;
}

static short rearrange_actchannel_bottom (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		if (rearrange_actchannel_after_ok(list->last, type)) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* add at end */
			BLI_addtail(list, channel);
			
			return 1;
		}
	}
	
	return 0;
}


/* Change the order of action-channels 
 *	mode: REARRANGE_ACTCHAN_*  
 */
static void rearrange_action_channels (bAnimContext *ac, short mode)
{
	bAction *act;
	bActionChannel *achan, *chan;
	bActionGroup *agrp, *grp;
	bActionGroup tgrp;
	
	short (*rearrange_func)(ListBase *, Link *, short);
	short do_channels = 1;
	
	/* Get the active action, exit if none are selected */
	act= (bAction *)ac->data;
	
	/* exit if invalid mode */
	switch (mode) {
		case REARRANGE_ACTCHAN_TOP:
			rearrange_func= rearrange_actchannel_top;
			break;
		case REARRANGE_ACTCHAN_UP:
			rearrange_func= rearrange_actchannel_up;
			break;
		case REARRANGE_ACTCHAN_DOWN:
			rearrange_func= rearrange_actchannel_down;
			break;
		case REARRANGE_ACTCHAN_BOTTOM:
			rearrange_func= rearrange_actchannel_bottom;
			break;
		default:
			return;
	}
	
	/* make sure we're only operating with groups */
	split_groups_action_temp(act, &tgrp);
	
	/* rearrange groups first (and then, only consider channels if the groups weren't moved) */
	#define GET_FIRST(list) ((mode > 0) ? (list.first) : (list.last))
	#define GET_NEXT(item) ((mode > 0) ? (item->next) : (item->prev))
	
	for (agrp= GET_FIRST(act->groups); agrp; agrp= grp) {
		/* Get next group to consider */
		grp= GET_NEXT(agrp);
		
		/* try to do group first */
		if (rearrange_func(&act->groups, (Link *)agrp, ANIMTYPE_GROUP)) {
			do_channels= 0;
			agrp->flag |= AGRP_MOVED;
		}
	}
	
	if (do_channels) {
		for (agrp= GET_FIRST(act->groups); agrp; agrp= grp) {
			/* Get next group to consider */
			grp= GET_NEXT(agrp);
			
			/* only consider action-channels if they're visible (group expanded) */
			if (EXPANDED_AGRP(agrp)) {
				for (achan= GET_FIRST(agrp->channels); achan; achan= chan) {
					/* Get next channel to consider */
					chan= GET_NEXT(achan);
					
					/* Try to do channel */
					if (rearrange_func(&agrp->channels, (Link *)achan, ANIMTYPE_ACHAN))
						achan->flag |= ACHAN_MOVED;
				}
			}
		}
	}
	#undef GET_FIRST
	#undef GET_NEXT
	
	/* assemble lists into one list (and clear moved tags) */
	join_groups_action_temp(act);
}

/* ------------------- */

static int animchannels_rearrange_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data - only for Action Editor (for now) */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype != ANIMCONT_ACTION)
		return OPERATOR_PASS_THROUGH;
		
	/* get mode, then rearrange channels */
	mode= RNA_enum_get(op->ptr, "direction");
	rearrange_action_channels(&ac, mode);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 

void ANIM_OT_channels_move_up (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Channel(s) Up";
	ot->idname= "ANIM_OT_channels_move_up";
	
	/* api callbacks */
	ot->exec= animchannels_rearrange_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "direction", NULL /* XXX add enum for this */, REARRANGE_ACTCHAN_UP, "Direction", "");
}

void ANIM_OT_channels_move_down (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Channel(s) Down";
	ot->idname= "ANIM_OT_channels_move_down";
	
	/* api callbacks */
	ot->exec= animchannels_rearrange_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "direction", NULL /* XXX add enum for this */, REARRANGE_ACTCHAN_DOWN, "Direction", "");
}

void ANIM_OT_channels_move_top (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Channel(s) to Top";
	ot->idname= "ANIM_OT_channels_move_to_top";
	
	/* api callbacks */
	ot->exec= animchannels_rearrange_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "direction", NULL /* XXX add enum for this */, REARRANGE_ACTCHAN_TOP, "Direction", "");
}

void ANIM_OT_channels_move_bottom (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Channel(s) to Bottom";
	ot->idname= "ANIM_OT_channels_move_to_bottom";
	
	/* api callbacks */
	ot->exec= animchannels_rearrange_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "direction", NULL /* XXX add enum for this */, REARRANGE_ACTCHAN_BOTTOM, "Direction", "");
}

#endif // XXX old animation system - needs to be updated for new system...

/* ******************** Delete Channel Operator *********************** */

static int animchannels_delete_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* cannot delete in shapekey */
	if (ac.datatype == ANIMCONT_SHAPEKEY) 
		return OPERATOR_CANCELLED;
		
		
	/* do groups only first (unless in Drivers mode, where there are none) */
	if (ac.datatype != ANIMCONT_DRIVERS) {
		/* filter data */
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_CHANNELS | ANIMFILTER_FOREDIT);
		ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		/* delete selected groups and their associated channels */
		for (ale= anim_data.first; ale; ale= ale->next) {
			/* only groups - don't check other types yet, since they may no-longer exist */
			if (ale->type == ANIMTYPE_GROUP) {
				bActionGroup *agrp= (bActionGroup *)ale->data;
				AnimData *adt= ale->adt;
				FCurve *fcu, *fcn;
				
				/* skip this group if no AnimData available, as we can't safely remove the F-Curves */
				if (adt == NULL)
					continue;
				
				/* delete all of the Group's F-Curves, but no others */
				for (fcu= agrp->channels.first; fcu && fcu->grp==agrp; fcu= fcn) {
					fcn= fcu->next;
					
					/* remove from group and action, then free */
					action_groups_remove_channel(adt->action, fcu);
					free_fcurve(fcu);
				}
				
				/* free the group itself */
				if (adt->action)
					BLI_freelinkN(&adt->action->groups, agrp);
				else
					MEM_freeN(agrp);
			}
		}
		
		/* cleanup */
		BLI_freelistN(&anim_data);
	}
	
	/* now do F-Curves */
	if (ac.datatype != ANIMCONT_GPENCIL) {
		/* filter data */
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT);
		ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		/* delete selected F-Curves */
		for (ale= anim_data.first; ale; ale= ale->next) {
			/* only F-Curves, and only if we can identify its parent */
			if (ale->type == ANIMTYPE_FCURVE) {
				AnimData *adt= ale->adt;
				FCurve *fcu= (FCurve *)ale->data;
				
				/* if no AnimData, we've got nowhere to remove the F-Curve from */
				if (adt == NULL)
					continue;
					
				/* remove from whatever list it came from
				 *	- Action Group
				 *	- Action
				 *	- Drivers
				 *	- TODO... some others?
				 */
				if (fcu->grp)
					action_groups_remove_channel(adt->action, fcu);
				else if (adt->action)
					BLI_remlink(&adt->action->curves, fcu);
				else if (ac.datatype == ANIMCONT_DRIVERS)
					BLI_remlink(&adt->drivers, fcu);
					
				/* free the F-Curve itself */
				free_fcurve(fcu);
			}
		}
		
		/* cleanup */
		BLI_freelistN(&anim_data);
	}
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_channels_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Channels";
	ot->idname= "ANIM_OT_channels_delete";
	ot->description= "Delete all selected animation channels.";
	
	/* api callbacks */
	ot->exec= animchannels_delete_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Toggle Channel Visibility Operator *********************** */

static int animchannels_visibility_toggle_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	short vis= ACHANNEL_SETFLAG_ADD;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_SEL);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* See if we should be making showing all selected or hiding */
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (vis == ACHANNEL_SETFLAG_CLEAR) 
			break;
		
		if ((ale->type == ANIMTYPE_FCURVE) && (ale->flag & FCURVE_VISIBLE))
			vis= ACHANNEL_SETFLAG_CLEAR;
		else if ((ale->type == ANIMTYPE_GROUP) && !(ale->flag & AGRP_NOTVISIBLE))
			vis= ACHANNEL_SETFLAG_CLEAR;
	}
		
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ANIMTYPE_FCURVE: /* F-Curve */
			{
				FCurve *fcu= (FCurve *)ale->data;
				ACHANNEL_SET_FLAG(fcu, vis, FCURVE_VISIBLE);
			}
				break;
			case ANIMTYPE_GROUP: /* Group */
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				ACHANNEL_SET_FLAG_NEG(agrp, vis, AGRP_NOTVISIBLE);
			}
				break;
		}
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_channels_visibility_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Visibility";
	ot->idname= "ANIM_OT_channels_visibility_toggle";
	ot->description= "Toggle visibility in Graph Editor of all selected animation channels.";
	
	/* api callbacks */
	ot->exec= animchannels_visibility_toggle_exec;
	ot->poll= ED_operator_ipo_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************** Set Flags Operator *********************** */

/* defines for setting animation-channel flags */
EnumPropertyItem prop_animchannel_setflag_types[] = {
	{ACHANNEL_SETFLAG_CLEAR, "DISABLE", 0, "Disable", ""},
	{ACHANNEL_SETFLAG_ADD, "ENABLE", 0, "Enable", ""},
	{ACHANNEL_SETFLAG_TOGGLE, "TOGGLE", 0, "Toggle", ""},
	{0, NULL, 0, NULL, NULL}
};

/* defines for set animation-channel settings */
// TODO: could add some more types, but those are really quite dependent on the mode...
EnumPropertyItem prop_animchannel_settings_types[] = {
	{ACHANNEL_SETTING_PROTECT, "PROTECT", 0, "Protect", ""},
	{ACHANNEL_SETTING_MUTE, "MUTE", 0, "Mute", ""},
	{0, NULL, 0, NULL, NULL}
};


/* ------------------- */

/* macro to be used in setflag_anim_channels */
#define ASUBCHANNEL_SEL_OK(ale) ( (onlysel == 0) || \
		((ale->id) && (GS(ale->id->name)==ID_OB) && (((Object *)ale->id)->flag & SELECT)) ) 

/* Set/clear a particular flag (setting) for all selected + visible channels 
 *	setting: the setting to modify
 *	mode: eAnimChannels_SetFlag
 *	onlysel: only selected channels get the flag set
 */
static void setflag_anim_channels (bAnimContext *ac, short setting, short mode, short onlysel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	if (onlysel) filter |= ANIMFILTER_SEL;
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* affect selected channels */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* set the setting in the appropriate way (if available) */
		ANIM_channel_setting_set(ac, ale, setting, mode);
	}
	
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int animchannels_setflag_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode, setting;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* mode (eAnimChannels_SetFlag), setting (eAnimChannel_Settings) */
	mode= RNA_enum_get(op->ptr, "mode");
	setting= RNA_enum_get(op->ptr, "type");
	
	/* modify setting */
	setflag_anim_channels(&ac, setting, mode, 1);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}


void ANIM_OT_channels_setting_enable (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Enable Channel Setting";
	ot->idname= "ANIM_OT_channels_setting_enable";
	ot->description= "Enable specified setting on all selected animation channels.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= animchannels_setflag_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
		/* flag-setting mode */
	RNA_def_enum(ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_ADD, "Mode", "");
		/* setting to set */
	RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

void ANIM_OT_channels_setting_disable (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Disable Channel Setting";
	ot->idname= "ANIM_OT_channels_setting_disable";
	ot->description= "Disable specified setting on all selected animation channels.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= animchannels_setflag_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
		/* flag-setting mode */
	RNA_def_enum(ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_CLEAR, "Mode", "");
		/* setting to set */
	RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

void ANIM_OT_channels_setting_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Channel Setting";
	ot->idname= "ANIM_OT_channels_setting_toggle";
	ot->description= "Toggle specified setting on all selected animation channels.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= animchannels_setflag_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
		/* flag-setting mode */
	RNA_def_enum(ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_TOGGLE, "Mode", "");
		/* setting to set */
	RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

// XXX currently, this is a separate operator, but perhaps we could in future specify in keymaps whether to call invoke or exec?
void ANIM_OT_channels_editable_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Channel Editability";
	ot->idname= "ANIM_OT_channels_editable_toggle";
	ot->description= "Toggle editability of selected channels.";
	
	/* api callbacks */
	ot->exec= animchannels_setflag_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
		/* flag-setting mode */
	RNA_def_enum(ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_TOGGLE, "Mode", "");
		/* setting to set */
	RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, ACHANNEL_SETTING_PROTECT, "Type", "");
}

/* ********************** Expand Channels Operator *********************** */

static int animchannels_expand_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short onlysel= 1;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* only affect selected channels? */
	if (RNA_boolean_get(op->ptr, "all"))
		onlysel= 0;
	
	/* modify setting */
	setflag_anim_channels(&ac, ACHANNEL_SETTING_EXPAND, ACHANNEL_SETFLAG_ADD, onlysel);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_channels_expand (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Expand Channels";
	ot->idname= "ANIM_OT_channels_expand";
	ot->description= "Expand (i.e. open) all selected expandable animation channels.";
	
	/* api callbacks */
	ot->exec= animchannels_expand_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Expand all channels (not just selected ones)");
}

/* ********************** Collapse Channels Operator *********************** */

static int animchannels_collapse_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short onlysel= 1;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* only affect selected channels? */
	if (RNA_boolean_get(op->ptr, "all"))
		onlysel= 0;
	
	/* modify setting */
	setflag_anim_channels(&ac, ACHANNEL_SETTING_EXPAND, ACHANNEL_SETFLAG_CLEAR, onlysel);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_channels_collapse (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Collapse Channels";
	ot->idname= "ANIM_OT_channels_collapse";
	ot->description= "Collapse (i.e. close) all selected expandable animation channels.";
	
	/* api callbacks */
	ot->exec= animchannels_collapse_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Collapse all channels (not just selected ones)");
}

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
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_channels_select_all_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "ANIM_OT_channels_select_all_toggle";
	ot->description= "Toggle selection of all animation channels.";
	
	/* api callbacks */
	ot->exec= animchannels_deselectall_exec;
	ot->poll= animedit_poll_channels_nla_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "invert", 0, "Invert", "");
}

/* ******************** Borderselect Operator *********************** */

static void borderselect_anim_channels (bAnimContext *ac, rcti *rect, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	float ymin=0, ymax=(float)(-ACHANNEL_HEIGHT);
	
	/* convert border-region to view coordinates */
	UI_view2d_region_to_view(v2d, rect->xmin, rect->ymin+2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect->xmax, rect->ymax-2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		ymin= ymax - ACHANNEL_STEP;
		
		/* if channel is within border-select region, alter it */
		if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
			/* set selection flags only */
			ANIM_channel_setting_set(ac, ale, ACHANNEL_SETTING_SELECT, selectmode);
			
			/* type specific actions */
			switch (ale->type) {
				case ANIMTYPE_GROUP:
				{
					bActionGroup *agrp= (bActionGroup *)ale->data;
					
					/* always clear active flag after doing this */
					agrp->flag &= ~AGRP_ACTIVE;
				}
					break;
			}
		}
		
		/* set minimum extent to be the maximum of the next channel */
		ymax= ymin;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int animchannels_borderselect_exec(bContext *C, wmOperator *op)
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
	borderselect_anim_channels(&ac, &rect, selectmode);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_SELECT, NULL);
	
	return OPERATOR_FINISHED;
} 

void ANIM_OT_channels_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "ANIM_OT_channels_select_border";
	ot->description= "Select all animation channels within the specified region.";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= animchannels_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= animedit_poll_channels_nla_tweakmode_off;
	
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
/* Handle selection changes due to clicking on channels. Settings will get caught by UI code... */

static int mouse_anim_channels (bAnimContext *ac, float x, int channel_index, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	int notifierFlags = 0;
	
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
		return 0;
	}
	
	/* selectmode -1 is a special case for ActionGroups only, which selects all of the channels underneath it only... */
	// TODO: should this feature be extended to work with other channel types too?
	if ((selectmode == -1) && (ale->type != ANIMTYPE_GROUP)) {
		/* normal channels should not behave normally in this case */
		BLI_freelistN(&anim_data);
		return 0;
	}
	
	/* action to take depends on what channel we've got */
	// WARNING: must keep this in sync with the equivalent function in nla_channels.c
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
			
			notifierFlags |= ND_ANIMCHAN_SELECT;
		}
			break;
		
		case ANIMTYPE_FILLACTD: /* Action Expander */
		case ANIMTYPE_DSMAT:	/* Datablock AnimData Expanders */
		case ANIMTYPE_DSLAM:
		case ANIMTYPE_DSCAM:
		case ANIMTYPE_DSCUR:
		case ANIMTYPE_DSSKEY:
		case ANIMTYPE_DSWOR:
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
					ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
					ale->adt->flag |= ADT_UI_SELECTED;
				}
				
				/* set active? */
				if ((ale->adt) && (ale->adt->flag & ADT_UI_SELECTED))
					ale->adt->flag |= ADT_UI_ACTIVE;
			}
			
			notifierFlags |= ND_ANIMCHAN_SELECT;
		}	
			break;
		
		case ANIMTYPE_GROUP: 
		{
			bActionGroup *agrp= (bActionGroup *)ale->data;
			
			/* select/deselect group */
			if (selectmode == SELECT_INVERT) {
				/* inverse selection status of this group only */
				agrp->flag ^= AGRP_SELECTED;
			}
			else if (selectmode == -1) {
				/* select all in group (and deselect everthing else) */	
				FCurve *fcu;
				
				/* deselect all other channels */
				ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				
				/* only select channels in group and group itself */
				for (fcu= agrp->channels.first; fcu && fcu->grp==agrp; fcu= fcu->next)
					fcu->flag |= FCURVE_SELECTED;
				agrp->flag |= AGRP_SELECTED;					
			}
			else {
				/* select group by itself */
				ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				agrp->flag |= AGRP_SELECTED;
			}
			
			/* if group is selected now, make group the 'active' one in the visible list */
			if (agrp->flag & AGRP_SELECTED)
				ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, agrp, ANIMTYPE_GROUP);
				
			notifierFlags |= ND_ANIMCHAN_SELECT;
		}
			break;
		case ANIMTYPE_FCURVE: 
		{
			FCurve *fcu= (FCurve *)ale->data;
			
			/* select/deselect */
			if (selectmode == SELECT_INVERT) {
				/* inverse selection status of this F-Curve only */
				fcu->flag ^= FCURVE_SELECTED;
			}
			else {
				/* select F-Curve by itself */
				ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				fcu->flag |= FCURVE_SELECTED;
			}
			
			/* if F-Curve is selected now, make F-Curve the 'active' one in the visible list */
			if (fcu->flag & FCURVE_SELECTED)
				ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, fcu, ANIMTYPE_FCURVE);
				
			notifierFlags |= ND_ANIMCHAN_SELECT;
		}
			break;
		case ANIMTYPE_GPDATABLOCK:
		{
			bGPdata *gpd= (bGPdata *)ale->data;
			
			/* toggle expand */
			gpd->flag ^= GP_DATA_EXPAND;
			
			notifierFlags |= ND_ANIMCHAN_EDIT;
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
	
	/* return notifier flags */
	return notifierFlags;
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
	else if (RNA_boolean_get(op->ptr, "children_only"))
		selectmode= -1; /* this is a bit of a special case for ActionGroups only... should it be removed or extended to all instead? */
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
	notifierFlags= mouse_anim_channels(&ac, x, channel_index, selectmode);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|notifierFlags, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_channels_click (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Click on Channels";
	ot->idname= "ANIM_OT_channels_click";
	ot->description= "Handle mouse-clicks over animation channels.";
	
	/* api callbacks */
	ot->invoke= animchannels_mouseclick_invoke;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
	RNA_def_boolean(ot->srna, "children_only", 0, "Select Children Only", ""); // CTRLKEY|SHIFTKEY
}

/* ************************************************************************** */
/* Operator Registration */

void ED_operatortypes_animchannels(void)
{
	WM_operatortype_append(ANIM_OT_channels_select_all_toggle);
	WM_operatortype_append(ANIM_OT_channels_select_border);
	WM_operatortype_append(ANIM_OT_channels_click);
	
	WM_operatortype_append(ANIM_OT_channels_setting_enable);
	WM_operatortype_append(ANIM_OT_channels_setting_disable);
	WM_operatortype_append(ANIM_OT_channels_setting_toggle);
	
	WM_operatortype_append(ANIM_OT_channels_delete);
	
		// XXX does this need to be a separate operator?
	WM_operatortype_append(ANIM_OT_channels_editable_toggle);
	
		// XXX these need to be updated for new system... todo...
	//WM_operatortype_append(ANIM_OT_channels_move_up);
	//WM_operatortype_append(ANIM_OT_channels_move_down);
	//WM_operatortype_append(ANIM_OT_channels_move_top);
	//WM_operatortype_append(ANIM_OT_channels_move_bottom);
	
	WM_operatortype_append(ANIM_OT_channels_expand);
	WM_operatortype_append(ANIM_OT_channels_collapse);
	
	WM_operatortype_append(ANIM_OT_channels_visibility_toggle);
}

void ED_keymap_animchannels(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Animation_Channels", 0, 0);
	
	/* selection */
		/* click-select */
		// XXX for now, only leftmouse.... 
	WM_keymap_add_item(keymap, "ANIM_OT_channels_click", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_click", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_click", LEFTMOUSE, KM_PRESS, KM_CTRL|KM_SHIFT, 0)->ptr, "children_only", 1);
	
		/* deselect all */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", 1);
	
		/* borderselect */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_border", BKEY, KM_PRESS, 0, 0);
	
	/* delete */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_delete", DELKEY, KM_PRESS, 0, 0);
	
	/* settings */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_toggle", WKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_enable", WKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_disable", WKEY, KM_PRESS, KM_ALT, 0);
	
	/* settings - specialised hotkeys */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_editable_toggle", TABKEY, KM_PRESS, 0, 0);
	
	/* expand/collapse */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_expand", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_collapse", PADMINUS, KM_PRESS, 0, 0);
	
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_expand", PADPLUSKEY, KM_PRESS, KM_CTRL, 0)->ptr, "all", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_collapse", PADMINUS, KM_PRESS, KM_CTRL, 0)->ptr, "all", 1);
	
	/* rearranging - actions only */
	//WM_keymap_add_item(keymap, "ANIM_OT_channels_move_up", PAGEUPKEY, KM_PRESS, KM_SHIFT, 0);
	//WM_keymap_add_item(keymap, "ANIM_OT_channels_move_down", PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0);
	//WM_keymap_add_item(keymap, "ANIM_OT_channels_move_to_top", PAGEUPKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	//WM_keymap_add_item(keymap, "ANIM_OT_channels_move_to_bottom", PAGEDOWNKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	
	/* Graph Editor only */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_visibility_toggle", VKEY, KM_PRESS, 0, 0);
}

/* ************************************************************************** */

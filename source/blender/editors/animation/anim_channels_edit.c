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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_channels_edit.c
 *  \ingroup edanimation
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BKE_library.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_key_types.h"
#include "DNA_gpencil_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_context.h"
#include "BKE_global.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h" // XXX move the select modes out of there!
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************************************************************** */
/* CHANNELS API - Exposed API */

/* -------------------------- Selection ------------------------------------- */

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
			case ANIMTYPE_DSMESH:
			case ANIMTYPE_DSTEX:
			case ANIMTYPE_DSLAT:
			case ANIMTYPE_DSSPK:
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
			case ANIMTYPE_DSMESH:
			case ANIMTYPE_DSLAT:
			case ANIMTYPE_DSSPK:
			{
				/* need to verify that this data is valid for now */
				if (ale && ale->adt) {
					ale->adt->flag |= ADT_UI_ACTIVE;
				}
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
void ANIM_deselect_anim_channels (bAnimContext *ac, void *data, short datatype, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	/* NOTE: no list visible, otherwise, we get dangling */
	filter= ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS;
	ANIM_animdata_filter(ac, &anim_data, filter, data, datatype);
	
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
				case ANIMTYPE_SHAPEKEY:
					if (ale->flag & KEYBLOCK_SEL)
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
				case ANIMTYPE_DSMESH:
				case ANIMTYPE_DSNTREE:
				case ANIMTYPE_DSTEX:
				case ANIMTYPE_DSLAT:
				case ANIMTYPE_DSSPK:
				{
					if ((ale->adt) && (ale->adt->flag & ADT_UI_SELECTED))
						sel= ACHANNEL_SETFLAG_CLEAR;
				}
					break;
					
				case ANIMTYPE_GPLAYER:
					if (ale->flag & GP_LAYER_SELECT)
						sel= ACHANNEL_SETFLAG_CLEAR;
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
			case ANIMTYPE_SHAPEKEY:
			{
				KeyBlock *kb= (KeyBlock *)ale->data;
				
				ACHANNEL_SET_FLAG(kb, sel, KEYBLOCK_SEL);
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
			case ANIMTYPE_DSMESH:
			case ANIMTYPE_DSNTREE:
			case ANIMTYPE_DSTEX:
			case ANIMTYPE_DSLAT:
			case ANIMTYPE_DSSPK:
			{
				/* need to verify that this data is valid for now */
				if (ale->adt) {
					ACHANNEL_SET_FLAG(ale->adt, sel, ADT_UI_SELECTED);
					ale->adt->flag &= ~ADT_UI_ACTIVE;
				}
			}
				break;
				
			case ANIMTYPE_GPLAYER:
			{
				bGPDlayer *gpl = (bGPDlayer *)ale->data;
				
				ACHANNEL_SET_FLAG(gpl, sel, GP_LAYER_SELECT);
			}
				break;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ---------------------------- Graph Editor ------------------------------------- */

/* Flush visibility (for Graph Editor) changes up/down hierarchy for changes in the given setting 
 *	- anim_data: list of the all the anim channels that can be chosen
 *		-> filtered using ANIMFILTER_CHANNELS only, since if we took VISIBLE too,
 *		  then the channels under closed expanders get ignored...
 *	- ale_setting: the anim channel (not in the anim_data list directly, though occurring there)
 *		with the new state of the setting that we want flushed up/down the hierarchy 
 *	- setting: type of setting to set
 *	- on: whether the visibility setting has been enabled or disabled 
 */
void ANIM_flush_setting_anim_channels (bAnimContext *ac, ListBase *anim_data, bAnimListElem *ale_setting, int setting, short on)
{
	bAnimListElem *ale, *match=NULL;
	int prevLevel=0, matchLevel=0;
	
	/* sanity check */
	if (ELEM(NULL, anim_data, anim_data->first))
		return;
	
	/* find the channel that got changed */
	for (ale= anim_data->first; ale; ale= ale->next) {
		/* compare data, and type as main way of identifying the channel */
		if ((ale->data == ale_setting->data) && (ale->type == ale_setting->type)) {
			/* we also have to check the ID, this is assigned to, since a block may have multiple users */
			// TODO: is the owner-data more revealing?
			if (ale->id == ale_setting->id) {
				match= ale;
				break;
			}
		}
	}
	if (match == NULL) {
		printf("ERROR: no channel matching the one changed was found \n");
		return;
	}
	else {
		bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale_setting);
		
		if (acf == NULL) {
			printf("ERROR: no channel info for the changed channel \n");
			return;
		}
		
		/* get the level of the channel that was affected
		 * 	 - we define the level as simply being the offset for the start of the channel
		 */
		matchLevel= (acf->get_offset)? acf->get_offset(ac, ale_setting) : 0;
		prevLevel= matchLevel;
	}
	
	/* flush up? 
	 *
	 * For Visibility:
	 *	- only flush up if the current state is now enabled (positive 'on' state is default) 
	 *	  (otherwise, it's too much work to force the parents to be inactive too)
	 *
	 * For everything else:
	 *	- only flush up if the current state is now disabled (negative 'off' state is default)
	 *	  (otherwise, it's too much work to force the parents to be active too)
	 */
	if ( ((setting == ACHANNEL_SETTING_VISIBLE) && on) ||
		 ((setting != ACHANNEL_SETTING_VISIBLE) && on==0) )
	{
		/* go backwards in the list, until the highest-ranking element (by indention has been covered) */
		for (ale= match->prev; ale; ale= ale->prev) {
			bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
			int level;
			
			/* if no channel info was found, skip, since this type might not have any useful info */
			if (acf == NULL)
				continue;
			
			/* get the level of the current channel traversed 
			 * 	 - we define the level as simply being the offset for the start of the channel
			 */
			level= (acf->get_offset)? acf->get_offset(ac, ale) : 0;
			
			/* if the level is 'less than' (i.e. more important) the level we're matching
			 * but also 'less than' the level just tried (i.e. only the 1st group above grouped F-Curves, 
			 * when toggling visibility of F-Curves, gets flushed, which should happen if we don't let prevLevel
			 * get updated below once the first 1st group is found)...
			 */
			if (level < prevLevel) {
				/* flush the new status... */
				ANIM_channel_setting_set(ac, ale, setting, on);
				
				/* store this level as the 'old' level now */
				prevLevel= level;
			}	
			/* if the level is 'greater than' (i.e. less important) than the previous level... */
			else if (level > prevLevel) {
				/* if previous level was a base-level (i.e. 0 offset / root of one hierarchy),
				 * stop here
				 */
				if (prevLevel == 0)
					break;
				/* otherwise, this level weaves into another sibling hierarchy to the previous one just
				 * finished, so skip until we get to the parent of this level 
				 */
				else
					continue;
			}
		}
	}
	
	/* flush down (always) */
	{
		/* go forwards in the list, until the lowest-ranking element (by indention has been covered) */
		for (ale= match->next; ale; ale= ale->next) {
			bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
			int level;
			
			/* if no channel info was found, skip, since this type might not have any useful info */
			if (acf == NULL)
				continue;
			
			/* get the level of the current channel traversed 
			 * 	 - we define the level as simply being the offset for the start of the channel
			 */
			level= (acf->get_offset)? acf->get_offset(ac, ale) : 0;
			
			/* if the level is 'greater than' (i.e. less important) the channel that was changed, 
			 * flush the new status...
			 */
			if (level > matchLevel)
				ANIM_channel_setting_set(ac, ale, setting, on);
			/* however, if the level is 'less than or equal to' the channel that was changed,
			 * (i.e. the current channel is as important if not more important than the changed channel)
			 * then we should stop, since we've found the last one of the children we should flush
			 */
			else
				break;
			
			/* store this level as the 'old' level now */
			// prevLevel= level; // XXX: prevLevel is unused
		}
	}
}

/* -------------------------- F-Curves ------------------------------------- */

/* Delete the given F-Curve from its AnimData block */
void ANIM_fcurve_delete_from_animdata (bAnimContext *ac, AnimData *adt, FCurve *fcu)
{
	/* - if no AnimData, we've got nowhere to remove the F-Curve from 
	 *	(this doesn't guarantee that the F-Curve is in there, but at least we tried
	 * - if no F-Curve, there is nothing to remove
	 */
	if (ELEM(NULL, adt, fcu))
		return;
		
	/* remove from whatever list it came from
	 *	- Action Group
	 *	- Action
	 *	- Drivers
	 *	- TODO... some others?
	 */
	if ((ac) && (ac->datatype == ANIMCONT_DRIVERS)) {
		/* driver F-Curve */
		BLI_remlink(&adt->drivers, fcu);
	}
	else if (adt->action) {
		/* remove from group or action, whichever one "owns" the F-Curve */
		if (fcu->grp)
			action_groups_remove_channel(adt->action, fcu);
		else
			BLI_remlink(&adt->action->curves, fcu);
			
		/* if action has no more F-Curves as a result of this, unlink it from
		 * AnimData if it did not come from a NLA Strip being tweaked.
		 *
		 * This is done so that we don't have dangling Object+Action entries in
		 * channel list that are empty, and linger around long after the data they
		 * are for has disappeared (and probably won't come back).
		 */
			// XXX: does everybody always want this?
			/* XXX: there's a problem where many actions could build up in the file if multiple
			 * full add/delete cycles are performed on the same objects, but assume that this is rare
			 */
		if ((adt->action->curves.first == NULL) && (adt->flag & ADT_NLA_EDIT_ON)==0) {
			id_us_min(&adt->action->id);
			adt->action = NULL;
		}
	}
		
	/* free the F-Curve itself */
	free_fcurve(fcu);
}

/* ************************************************************************** */
/* OPERATORS */

/* ****************** Operator Utilities ********************************** */

/* poll callback for being in an Animation Editor channels list region */
static int animedit_poll_channels_active (bContext *C)
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
static int animedit_poll_channels_nla_tweakmode_off (bContext *C)
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

/* constants for channel rearranging */
/* WARNING: don't change exising ones without modifying rearrange func accordingly */
enum {
	REARRANGE_ANIMCHAN_TOP= -2,
	REARRANGE_ANIMCHAN_UP= -1,
	REARRANGE_ANIMCHAN_DOWN= 1,
	REARRANGE_ANIMCHAN_BOTTOM= 2
};

/* defines for rearranging channels */
static EnumPropertyItem prop_animchannel_rearrange_types[] = {
	{REARRANGE_ANIMCHAN_TOP, "TOP", 0, "To Top", ""},
	{REARRANGE_ANIMCHAN_UP, "UP", 0, "Up", ""},
	{REARRANGE_ANIMCHAN_DOWN, "DOWN", 0, "Down", ""},
	{REARRANGE_ANIMCHAN_BOTTOM, "BOTTOM", 0, "To Bottom", ""},
	{0, NULL, 0, NULL, NULL}
};

/* Reordering "Islands" Defines ----------------------------------- */

/* Island definition - just a listbase container */
typedef struct tReorderChannelIsland {
	struct tReorderChannelIsland *next, *prev;
	
	ListBase channels; 	/* channels within this region with the same state */
	int flag;			/* eReorderIslandFlag */
} tReorderChannelIsland;

/* flags for channel reordering islands */
typedef enum eReorderIslandFlag {
	REORDER_ISLAND_SELECTED 		= (1<<0),	/* island is selected */
	REORDER_ISLAND_UNTOUCHABLE 		= (1<<1),	/* island should be ignored */
	REORDER_ISLAND_MOVED			= (1<<2)	/* island has already been moved */
} eReorderIslandFlag;


/* Rearrange Methods --------------------------------------------- */

static short rearrange_island_ok (tReorderChannelIsland *island)
{
	/* island must not be untouchable */
	if (island->flag & REORDER_ISLAND_UNTOUCHABLE)
		return 0;
	
	/* island should be selected to be moved */
	return (island->flag & REORDER_ISLAND_SELECTED) && !(island->flag & REORDER_ISLAND_MOVED);
}

/* ............................. */

static short rearrange_island_top (ListBase *list, tReorderChannelIsland *island)
{
	if (rearrange_island_ok(island)) {
		/* remove from current position */
		BLI_remlink(list, island);
		
		/* make it first element */
		BLI_insertlinkbefore(list, list->first, island);
		
		return 1;
	}
	
	return 0;
}

static short rearrange_island_up (ListBase *list, tReorderChannelIsland *island)
{
	if (rearrange_island_ok(island)) {
		/* moving up = moving before the previous island, otherwise we're in the same place */
		tReorderChannelIsland *prev= island->prev;
		
		if (prev) {
			/* remove from current position */
			BLI_remlink(list, island);
			
			/* push it up */
			BLI_insertlinkbefore(list, prev, island);
			
			return 1;
		}
	}
	
	return 0;
}

static short rearrange_island_down (ListBase *list, tReorderChannelIsland *island)
{
	if (rearrange_island_ok(island)) {
		/* moving down = moving after the next island, otherwise we're in the same place */
		tReorderChannelIsland *next = island->next;
		
		if (next) {
			/* can only move past if next is not untouchable (i.e. nothing can go after it) */
			if ((next->flag & REORDER_ISLAND_UNTOUCHABLE)==0) {
				/* remove from current position */
				BLI_remlink(list, island);
				
				/* push it down */
				BLI_insertlinkafter(list, next, island);
				
				return 1;
			}
		}
		/* else: no next channel, so we're at the bottom already, so can't move */
	}
	
	return 0;
}

static short rearrange_island_bottom (ListBase *list, tReorderChannelIsland *island)
{
	if (rearrange_island_ok(island)) {
		tReorderChannelIsland *last = list->last;
		
		/* remove island from current position */
		BLI_remlink(list, island);
		
		/* add before or after the last channel? */
		if ((last->flag & REORDER_ISLAND_UNTOUCHABLE)==0) {
			/* can add after it */
			BLI_addtail(list, island);
		}
		else {
			/* can at most go just before it, since last cannot be moved */
			BLI_insertlinkbefore(list, last, island);
			
		}
		
		return 1;
	}
	
	return 0;
}

/* ............................. */

/* typedef for channel rearranging function 
 * < list: list that channels belong to
 * < island: island to be moved
 * > return[0]: whether operation was a success
 */
typedef short (*AnimChanRearrangeFp)(ListBase *list, tReorderChannelIsland *island);

/* get rearranging function, given 'rearrange' mode */
static AnimChanRearrangeFp rearrange_get_mode_func (short mode)
{
	switch (mode) {
		case REARRANGE_ANIMCHAN_TOP:
			return rearrange_island_top;
		case REARRANGE_ANIMCHAN_UP:
			return rearrange_island_up;
		case REARRANGE_ANIMCHAN_DOWN:
			return rearrange_island_down;
		case REARRANGE_ANIMCHAN_BOTTOM:
			return rearrange_island_bottom;
		default:
			return NULL;
	}
}

/* Rearrange Islands Generics ------------------------------------- */

/* add channel into list of islands */
static void rearrange_animchannel_add_to_islands (ListBase *islands, ListBase *srcList, Link *channel, short type)
{
	tReorderChannelIsland *island = islands->last; 	/* always try to add to last island if possible */
	short is_sel=0, is_untouchable=0;
	
	/* get flags - selected and untouchable from the channel */
	switch (type) {
		case ANIMTYPE_GROUP:
		{
			bActionGroup *agrp= (bActionGroup *)channel;
			
			is_sel= SEL_AGRP(agrp);
			is_untouchable= (agrp->flag & AGRP_TEMP) != 0;
		}
			break;
		case ANIMTYPE_FCURVE:
		{
			FCurve *fcu= (FCurve *)channel;
			
			is_sel= SEL_FCU(fcu);
		}	
			break;
		case ANIMTYPE_NLATRACK:
		{
			NlaTrack *nlt= (NlaTrack *)channel;
			
			is_sel= SEL_NLT(nlt);
		}
			break;
			
		default:
			printf("rearrange_animchannel_add_to_islands(): don't know how to handle channels of type %d\n", type);
			return;
	}
	
	/* do we need to add to a new island? */
	if ((island == NULL) ||                                 /* 1) no islands yet */
		((island->flag & REORDER_ISLAND_SELECTED) == 0) ||  /* 2) unselected islands have single channels only - to allow up/down movement */
		(is_sel == 0))                                      /* 3) if channel is unselected, stop existing island (it was either wrong sel status, or full already) */
	{
		/* create a new island now */
		island = MEM_callocN(sizeof(tReorderChannelIsland), "tReorderChannelIsland");
		BLI_addtail(islands, island);
		
		if (is_sel)
			island->flag |= REORDER_ISLAND_SELECTED;
		if (is_untouchable)
			island->flag |= REORDER_ISLAND_UNTOUCHABLE;
	}

	/* add channel to island - need to remove it from its existing list first though */
	BLI_remlink(srcList, channel);
	BLI_addtail(&island->channels, channel);
}

/* flatten islands out into a single list again */
static void rearrange_animchannel_flatten_islands (ListBase *islands, ListBase *srcList)
{
	tReorderChannelIsland *island, *isn=NULL;
	
	/* make sure srcList is empty now */
	BLI_assert(srcList->first == NULL);
	
	/* go through merging islands */
	for (island = islands->first; island; island = isn) {
		isn = island->next;
		
		/* merge island channels back to main list, then delete the island */
		BLI_movelisttolist(srcList, &island->channels);
		BLI_freelinkN(islands, island);
	}
}

/* ............................. */

/* performing rearranging of channels using islands */
static short rearrange_animchannel_islands (ListBase *list, AnimChanRearrangeFp rearrange_func, short mode, short type)
{
	ListBase islands = {NULL, NULL};
	Link *channel, *chanNext=NULL;
	short done = 0;
	
	/* don't waste effort on an empty list */
	if (list->first == NULL)
		return 0;
	
	/* group channels into islands */
	for (channel = list->first; channel; channel = chanNext) {
		chanNext = channel->next;
		rearrange_animchannel_add_to_islands(&islands, list, channel, type);
	}
	
	/* perform moving of selected islands now, but only if there is more than one of 'em so that something will happen 
	 *	- scanning of the list is performed in the opposite direction to the direction we're moving things, so that we 
	 *	  shouldn't need to encounter items we've moved already
	 */
	if (islands.first != islands.last) {
		tReorderChannelIsland *first = (mode > 0) ? islands.last : islands.first;
		tReorderChannelIsland *island, *isn=NULL;
		
		for (island = first; island; island = isn) {
			isn = (mode > 0) ? island->prev : island->next;
			
			/* perform rearranging */
			if (rearrange_func(&islands, island)) {
				island->flag |= REORDER_ISLAND_MOVED;
				done = 1;
			}
		}
	}
	
	/* ungroup islands */
	rearrange_animchannel_flatten_islands(&islands, list);
	
	/* did we do anything? */
	return done;
}

/* NLA Specific Stuff ----------------------------------------------------- */

/* Change the order NLA Tracks within NLA Stack
 * ! NLA tracks are displayed in opposite order, so directions need care
 *	mode: REARRANGE_ANIMCHAN_*  
 */
static void rearrange_nla_channels (bAnimContext *UNUSED(ac), AnimData *adt, short mode)
{
	AnimChanRearrangeFp rearrange_func;
	
	/* hack: invert mode so that functions will work in right order */
	mode *= -1;
	
	/* get rearranging function */
	rearrange_func = rearrange_get_mode_func(mode);
	if (rearrange_func == NULL)
		return;
	
	/* only consider NLA data if it's accessible */	
	//if (EXPANDED_DRVD(adt) == 0)
	//	return;
	
	/* perform rearranging on tracks list */
	rearrange_animchannel_islands(&adt->nla_tracks, rearrange_func, mode, ANIMTYPE_NLATRACK);
}

/* Drivers Specific Stuff ------------------------------------------------- */

/* Change the order drivers within AnimData block
 *	mode: REARRANGE_ANIMCHAN_*  
 */
static void rearrange_driver_channels (bAnimContext *UNUSED(ac), AnimData *adt, short mode)
{
	/* get rearranging function */
	AnimChanRearrangeFp rearrange_func = rearrange_get_mode_func(mode);
	
	if (rearrange_func == NULL)
		return;
	
	/* only consider drivers if they're accessible */	
	if (EXPANDED_DRVD(adt) == 0)
		return;
	
	/* perform rearranging on drivers list (drivers are really just F-Curves) */
	rearrange_animchannel_islands(&adt->drivers, rearrange_func, mode, ANIMTYPE_FCURVE);
}

/* Action Specific Stuff ------------------------------------------------- */

/* make sure all action-channels belong to a group (and clear action's list) */
static void split_groups_action_temp (bAction *act, bActionGroup *tgrp)
{
	bActionGroup *agrp;
	FCurve *fcu;
	
	if (act == NULL)
		return;
	
	/* Separate F-Curves into lists per group */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		if (agrp->channels.first) {
			fcu= agrp->channels.last;
			act->curves.first= fcu->next;
			
			fcu= agrp->channels.first;
			fcu->prev= NULL;
			
			fcu= agrp->channels.last;
			fcu->next= NULL;
		}
	}
	
	/* Initialize memory for temp-group */
	memset(tgrp, 0, sizeof(bActionGroup));
	tgrp->flag |= (AGRP_EXPANDED|AGRP_TEMP);
	BLI_strncpy(tgrp->name, "#TempGroup", sizeof(tgrp->name));
	
	/* Move any action-channels not already moved, to the temp group */
	if (act->curves.first) {
		/* start of list */
		fcu= act->curves.first;
		fcu->prev= NULL;
		tgrp->channels.first= fcu;
		act->curves.first= NULL;
		
		/* end of list */
		fcu= act->curves.last;
		fcu->next= NULL;
		tgrp->channels.last= fcu;
		act->curves.last= NULL;
	}
	
	/* Add temp-group to list */
	BLI_addtail(&act->groups, tgrp);
}

/* link lists of channels that groups have */
static void join_groups_action_temp (bAction *act)
{
	bActionGroup *agrp;
	
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		ListBase tempGroup;
		
		/* add list of channels to action's channels */
		tempGroup= agrp->channels;
		BLI_movelisttolist(&act->curves, &agrp->channels);
		agrp->channels= tempGroup;
		
		/* clear moved flag */
		agrp->flag &= ~AGRP_MOVED;
		
		/* if temp-group... remove from list (but don't free as it's on the stack!) */
		if (agrp->flag & AGRP_TEMP) {
			BLI_remlink(&act->groups, agrp);
			break;
		}
	}
}

/* Change the order of anim-channels within action 
 *	mode: REARRANGE_ANIMCHAN_*  
 */
static void rearrange_action_channels (bAnimContext *ac, bAction *act, short mode)
{
	bActionGroup tgrp;
	short do_channels;
	
	/* get rearranging function */
	AnimChanRearrangeFp rearrange_func = rearrange_get_mode_func(mode);
	
	if (rearrange_func == NULL)
		return;
	
	/* make sure we're only operating with groups (vs a mixture of groups+curves) */
	split_groups_action_temp(act, &tgrp);
	
	/* rearrange groups first 
	 *	- the group's channels will only get considered if nothing happened when rearranging the groups
	 *	  i.e. the rearrange function returned 0
	 */
	do_channels = rearrange_animchannel_islands(&act->groups, rearrange_func, mode, ANIMTYPE_GROUP) == 0;
	
	if (do_channels) {
		bActionGroup *agrp;
		
		for (agrp= act->groups.first; agrp; agrp= agrp->next) {
			/* only consider F-Curves if they're visible (group expanded) */
			if (EXPANDED_AGRP(ac, agrp)) {
				rearrange_animchannel_islands(&agrp->channels, rearrange_func, mode, ANIMTYPE_FCURVE);
			}
		}
	}
	
	/* assemble lists into one list (and clear moved tags) */
	join_groups_action_temp(act);
}

/* ------------------- */

static int animchannels_rearrange_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get mode */
	mode= RNA_enum_get(op->ptr, "direction");
	
	/* method to move channels depends on the editor */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		/* Grease Pencil channels */
		printf("Grease Pencil not supported for moving yet\n");
	}
	else if (ac.datatype == ANIMCONT_ACTION) {
		/* Directly rearrange action's channels */
		rearrange_action_channels(&ac, ac.data, mode);
	}
	else {
		ListBase anim_data = {NULL, NULL};
		bAnimListElem *ale;
		int filter;
		
		/* get animdata blocks */
		filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ANIMDATA);
		ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		for (ale = anim_data.first; ale; ale = ale->next) {
			AnimData *adt= ale->data;
			
			switch (ac.datatype) {
				case ANIMCONT_NLA: /* NLA-tracks only */
					rearrange_nla_channels(&ac, adt, mode);
					break;
				
				case ANIMCONT_DRIVERS: /* Drivers list only */
					rearrange_driver_channels(&ac, adt, mode);
					break;
				
				case ANIMCONT_SHAPEKEY: // DOUBLE CHECK ME...
					
				default: /* some collection of actions */
					if (adt->action)
						rearrange_action_channels(&ac, adt->action, mode);
					else if (G.f & G_DEBUG)
						printf("Animdata has no action\n");
					break;
			}
		}
		
		/* free temp data */
		BLI_freelistN(&anim_data);
	}
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_move (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Channels";
	ot->idname= "ANIM_OT_channels_move";
	ot->description = "Rearrange selected animation channels";
	
	/* api callbacks */
	ot->exec= animchannels_rearrange_exec;
	ot->poll= animedit_poll_channels_nla_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	ot->prop= RNA_def_enum(ot->srna, "direction", prop_animchannel_rearrange_types, REARRANGE_ANIMCHAN_DOWN, "Direction", "");
}

/* ******************** Delete Channel Operator *********************** */

static int animchannels_delete_exec(bContext *C, wmOperator *UNUSED(op))
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
		filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
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
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* delete selected data channels */
	for (ale= anim_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ANIMTYPE_FCURVE: 
			{
				/* F-Curves if we can identify its parent */
				AnimData *adt= ale->adt;
				FCurve *fcu= (FCurve *)ale->data;
				
				/* try to free F-Curve */
				ANIM_fcurve_delete_from_animdata(&ac, adt, fcu);
			}
				break;
				
			case ANIMTYPE_GPLAYER:
			{
				/* Grease Pencil layer */
				bGPdata *gpd= (bGPdata *)ale->id;
				bGPDlayer *gpl= (bGPDlayer *)ale->data;
				
				/* try to delete the layer's data and the layer itself */
				free_gpencil_frames(gpl);
				BLI_freelinkN(&gpd->layers, gpl);
			}
				break;
		}
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
static void ANIM_OT_channels_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Channels";
	ot->idname= "ANIM_OT_channels_delete";
	ot->description= "Delete all selected animation channels";
	
	/* api callbacks */
	ot->exec= animchannels_delete_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Set Channel Visibility Operator *********************** */
/* NOTE: this operator is only valid in the Graph Editor channels region */

static int animchannels_visibility_set_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	ListBase all_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get list of all channels that selection may need to be flushed to 
	 * - hierarchy mustn't affect what we have access to here...
	 */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &all_data, filter, ac.data, ac.datatype);
		
	/* hide all channels not selected
	 * - hierarchy matters if we're doing this from the channels region
	 *   since we only want to apply this to channels we can "see", 
	 *   and have these affect their relatives
	 * - but for Graph Editor, this gets used also from main region
	 *   where hierarchy doesn't apply, as for [#21276]
	 */
	if ((ac.spacetype == SPACE_IPO) && (ac.regiontype != RGN_TYPE_CHANNELS)) {
		/* graph editor (case 2) */
		filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_UNSEL | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	}
	else {
		/* standard case */
		filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_UNSEL | ANIMFILTER_NODUPLIS);
	}
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* clear setting first */
		ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_CLEAR);
		
		/* now also flush selection status as appropriate 
		 * NOTE: in some cases, this may result in repeat flushing being performed
		 */
		ANIM_flush_setting_anim_channels(&ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, 0);
	}
	
	BLI_freelistN(&anim_data);
	
	/* make all the selected channels visible */
	filter= (ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* hack: skip object channels for now, since flushing those will always flush everything, but they are always included */
		// TODO: find out why this is the case, and fix that
		if (ale->type == ANIMTYPE_OBJECT)
			continue;
		
		/* enable the setting */
		ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);
		
		/* now, also flush selection status up/down as appropriate */
		ANIM_flush_setting_anim_channels(&ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, 1);
	}
	
	BLI_freelistN(&anim_data);
	BLI_freelistN(&all_data);
	
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_visibility_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Visibility";
	ot->idname= "ANIM_OT_channels_visibility_set";
	ot->description= "Make only the selected animation channels visible in the Graph Editor";
	
	/* api callbacks */
	ot->exec= animchannels_visibility_set_exec;
	ot->poll= ED_operator_graphedit_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


/* ******************** Toggle Channel Visibility Operator *********************** */
/* NOTE: this operator is only valid in the Graph Editor channels region */

static int animchannels_visibility_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	ListBase all_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	short vis= ACHANNEL_SETFLAG_ADD;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get list of all channels that selection may need to be flushed to 
	 * - hierarchy mustn't affect what we have access to here...
	 */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &all_data, filter, ac.data, ac.datatype);
		
	/* filter data
	 * - restrict this to only applying on settings we can get to in the list
	 */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* See if we should be making showing all selected or hiding */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* set the setting in the appropriate way (if available) */
		if (ANIM_channel_setting_get(&ac, ale, ACHANNEL_SETTING_VISIBLE)) {
			vis= ACHANNEL_SETFLAG_CLEAR;
			break;
		}
	}

	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* hack: skip object channels for now, since flushing those will always flush everything, but they are always included */
		// TODO: find out why this is the case, and fix that
		if (ale->type == ANIMTYPE_OBJECT)
			continue;
		
		/* change the setting */
		ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, vis);
		
		/* now, also flush selection status up/down as appropriate */
		ANIM_flush_setting_anim_channels(&ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, (vis == ACHANNEL_SETFLAG_ADD));
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
	BLI_freelistN(&all_data);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_visibility_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Visibility";
	ot->idname= "ANIM_OT_channels_visibility_toggle";
	ot->description= "Toggle visibility in Graph Editor of all selected animation channels";
	
	/* api callbacks */
	ot->exec= animchannels_visibility_toggle_exec;
	ot->poll= ED_operator_graphedit_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************** Set Flags Operator *********************** */

/* defines for setting animation-channel flags */
static EnumPropertyItem prop_animchannel_setflag_types[] = {
	{ACHANNEL_SETFLAG_TOGGLE, "TOGGLE", 0, "Toggle", ""},
	{ACHANNEL_SETFLAG_CLEAR, "DISABLE", 0, "Disable", ""},
	{ACHANNEL_SETFLAG_ADD, "ENABLE", 0, "Enable", ""},
	{ACHANNEL_SETFLAG_INVERT, "INVERT", 0, "Invert", ""},
	{0, NULL, 0, NULL, NULL}
};

/* defines for set animation-channel settings */
// TODO: could add some more types, but those are really quite dependent on the mode...
static EnumPropertyItem prop_animchannel_settings_types[] = {
	{ACHANNEL_SETTING_PROTECT, "PROTECT", 0, "Protect", ""},
	{ACHANNEL_SETTING_MUTE, "MUTE", 0, "Mute", ""},
	{0, NULL, 0, NULL, NULL}
};


/* ------------------- */

/* Set/clear a particular flag (setting) for all selected + visible channels 
 *	setting: the setting to modify
 *	mode: eAnimChannels_SetFlag
 *	onlysel: only selected channels get the flag set
 */
// TODO: enable a setting which turns flushing on/off?
static void setflag_anim_channels (bAnimContext *ac, short setting, short mode, short onlysel, short flush)
{
	ListBase anim_data = {NULL, NULL};
	ListBase all_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data that we need if flush is on */
	if (flush) {
		/* get list of all channels that selection may need to be flushed to 
		 * - hierarchy visibility needs to be ignored so that settings can get flushed
		 *   "down" inside closed containers
		 */
		filter= ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS;
		ANIM_animdata_filter(ac, &all_data, filter, ac->data, ac->datatype);
	}
	
	/* filter data that we're working on 
	 * - hierarchy matters if we're doing this from the channels region
	 *   since we only want to apply this to channels we can "see", 
	 *   and have these affect their relatives
	 * - but for Graph Editor, this gets used also from main region
	 *   where hierarchy doesn't apply [#21276]
	 */
	if ((ac->spacetype == SPACE_IPO) && (ac->regiontype != RGN_TYPE_CHANNELS)) {
		/* graph editor (case 2) */
		filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	}
	else {
		/* standard case */
		filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	}
	if (onlysel) filter |= ANIMFILTER_SEL;
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* if toggling, check if disable or enable */
	if (mode == ACHANNEL_SETFLAG_TOGGLE) {
		/* default to turn all on, unless we encounter one that's on... */
		mode= ACHANNEL_SETFLAG_ADD;
		
		/* see if we should turn off instead... */
		for (ale= anim_data.first; ale; ale= ale->next) {
			/* set the setting in the appropriate way (if available) */
			if (ANIM_channel_setting_get(ac, ale, setting) > 0) {
				mode= ACHANNEL_SETFLAG_CLEAR;
				break;
			}
		}
	}
	
	/* apply the setting */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* skip channel if setting is not available */
		if (ANIM_channel_setting_get(ac, ale, setting) == -1)
			continue;
		
		/* set the setting in the appropriate way */
		ANIM_channel_setting_set(ac, ale, setting, mode);
		
		/* if flush status... */
		if (flush)
			ANIM_flush_setting_anim_channels(ac, &all_data, ale, setting, mode);
	}
	
	BLI_freelistN(&anim_data);
	BLI_freelistN(&all_data);
}

/* ------------------- */

static int animchannels_setflag_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode, setting;
	short flush=1;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* mode (eAnimChannels_SetFlag), setting (eAnimChannel_Settings) */
	mode= RNA_enum_get(op->ptr, "mode");
	setting= RNA_enum_get(op->ptr, "type");
	
	/* check if setting is flushable */
	if (setting == ACHANNEL_SETTING_EXPAND)
		flush= 0;
	
	/* modify setting 
	 *	- only selected channels are affected
	 */
	setflag_anim_channels(&ac, setting, mode, 1, flush);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

/* duplicate of 'ANIM_OT_channels_setting_toggle' for menu title only, weak! */
static void ANIM_OT_channels_setting_enable (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Enable Channel Setting";
	ot->idname= "ANIM_OT_channels_setting_enable";
	ot->description= "Enable specified setting on all selected animation channels";
	
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
	ot->prop= RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}
/* duplicate of 'ANIM_OT_channels_setting_toggle' for menu title only, weak! */
static void ANIM_OT_channels_setting_disable (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Disable Channel Setting";
	ot->idname= "ANIM_OT_channels_setting_disable";
	ot->description= "Disable specified setting on all selected animation channels";
	
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
	ot->prop= RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

static void ANIM_OT_channels_setting_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Channel Setting";
	ot->idname= "ANIM_OT_channels_setting_toggle";
	ot->description= "Toggle specified setting on all selected animation channels";
	
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
	ot->prop= RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

static void ANIM_OT_channels_editable_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Channel Editability";
	ot->idname= "ANIM_OT_channels_editable_toggle";
	ot->description= "Toggle editability of selected channels";
	
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
	setflag_anim_channels(&ac, ACHANNEL_SETTING_EXPAND, ACHANNEL_SETFLAG_ADD, onlysel, 0);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_expand (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Expand Channels";
	ot->idname= "ANIM_OT_channels_expand";
	ot->description= "Expand (i.e. open) all selected expandable animation channels";
	
	/* api callbacks */
	ot->exec= animchannels_expand_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	ot->prop= RNA_def_boolean(ot->srna, "all", 1, "All", "Expand all channels (not just selected ones)");
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
	setflag_anim_channels(&ac, ACHANNEL_SETTING_EXPAND, ACHANNEL_SETFLAG_CLEAR, onlysel, 0);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_collapse (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Collapse Channels";
	ot->idname= "ANIM_OT_channels_collapse";
	ot->description= "Collapse (i.e. close) all selected expandable animation channels";
	
	/* api callbacks */
	ot->exec= animchannels_collapse_exec;
	ot->poll= animedit_poll_channels_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	ot->prop= RNA_def_boolean(ot->srna, "all", 1, "All", "Collapse all channels (not just selected ones)");
}

/* ******************* Reenable Disabled Operator ******************* */

static int animchannels_enable_poll (bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* channels region test */
	// TODO: could enhance with actually testing if channels region?
	if (ELEM(NULL, sa, CTX_wm_region(C)))
		return 0;
		
	/* animation editor test - Action/Dopesheet/etc. and Graph only */
	if (ELEM(sa->spacetype, SPACE_ACTION, SPACE_IPO) == 0)
		return 0;
		
	return 1;
}

static int animchannels_enable_exec (bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* loop through filtered data and clean curves */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		/* remove disabled flags from F-Curves */
		fcu->flag &= ~FCURVE_DISABLED;
		
		/* for drivers, let's do the same too */
		if (fcu->driver)
			fcu->driver->flag &= ~DRIVER_FLAG_INVALID;
			
		/* tag everything for updates - in particular, this is needed to get drivers working again */
		ANIM_list_elem_update(ac.scene, ale);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
		
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_fcurves_enable (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Revive Disabled F-Curves";
	ot->idname= "ANIM_OT_channels_fcurves_enable";
	ot->description= "Clears 'disabled' tag from all F-Curves to get broken F-Curves working again";
	
	/* api callbacks */
	ot->exec= animchannels_enable_exec;
	ot->poll= animchannels_enable_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************** Select All Operator *********************** */

static int animchannels_deselectall_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behavior - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		ANIM_deselect_anim_channels(&ac, ac.data, ac.datatype, 0, ACHANNEL_SETFLAG_TOGGLE);
	else
		ANIM_deselect_anim_channels(&ac, ac.data, ac.datatype, 1, ACHANNEL_SETFLAG_ADD);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
}
 
static void ANIM_OT_channels_select_all_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "ANIM_OT_channels_select_all_toggle";
	ot->description= "Toggle selection of all animation channels";
	
	/* api callbacks */
	ot->exec= animchannels_deselectall_exec;
	ot->poll= animedit_poll_channels_nla_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	ot->prop= RNA_def_boolean(ot->srna, "invert", 0, "Invert", "");
}

/* ******************** Borderselect Operator *********************** */

static void borderselect_anim_channels (bAnimContext *ac, rcti *rect, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceNla *snla = (SpaceNla *)ac->sl;
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	float ymin, ymax;
	
	/* set initial y extents */
	if (ac->datatype == ANIMCONT_NLA) {
		ymin = (float)(-NLACHANNEL_HEIGHT(snla));
		ymax = 0.0f;
	}
	else {
		ymin = 0.0f;
		ymax = (float)(-ACHANNEL_HEIGHT);
	}
	
	/* convert border-region to view coordinates */
	UI_view2d_region_to_view(v2d, rect->xmin, rect->ymin+2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect->xmax, rect->ymax-2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (ac->datatype == ANIMCONT_NLA)
			ymin= ymax - NLACHANNEL_STEP(snla);
		else
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
				case ANIMTYPE_NLATRACK:
				{
					NlaTrack *nlt= (NlaTrack *)ale->data;
					
					/* for now, it's easier just to do this here manually, as defining a new type 
					 * currently adds complications when doing other stuff 
					 */
					ACHANNEL_SET_FLAG(nlt, selectmode, NLATRACK_SELECTED);
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
	int gesture_mode, extend;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	gesture_mode= RNA_int_get(op->ptr, "gesture_mode");
	extend= RNA_boolean_get(op->ptr, "extend");

	if(!extend)
		ANIM_deselect_anim_channels(&ac, ac.data, ac.datatype, 1, ACHANNEL_SETFLAG_CLEAR);

	if (gesture_mode == GESTURE_MODAL_SELECT)
		selectmode = ACHANNEL_SETFLAG_ADD;
	else
		selectmode = ACHANNEL_SETFLAG_CLEAR;
	
	/* apply borderselect animation channels */
	borderselect_anim_channels(&ac, &rect, selectmode);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_SELECTED, NULL);
	
	return OPERATOR_FINISHED;
} 

static void ANIM_OT_channels_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "ANIM_OT_channels_select_border";
	ot->description= "Select all animation channels within the specified region";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= animchannels_borderselect_exec;
	ot->modal= WM_border_select_modal;
	ot->cancel= WM_border_select_cancel;
	
	ot->poll= animedit_poll_channels_nla_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, TRUE);
}

/* ******************* Rename Operator ***************************** */
/* Allow renaming some channels by clicking on them */

static void rename_anim_channels (bAnimContext *ac, int channel_index)
{
	ListBase anim_data = {NULL, NULL};
	bAnimChannelType *acf;
	bAnimListElem *ale;
	int filter;
	
	/* get the channel that was clicked on */
		/* filter channels */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
		/* get channel from index */
	ale= BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		if (G.f & G_DEBUG)
			printf("Error: animation channel (index = %d) not found in rename_anim_channels() \n", channel_index);
		
		BLI_freelistN(&anim_data);
		return;
	}
	
	/* check that channel can be renamed */
	acf = ANIM_channel_get_typeinfo(ale);
	if (acf && acf->name_prop) {
		PointerRNA ptr;
		PropertyRNA *prop;
		
		/* ok if we can get name property to edit from this channel */
		if (acf->name_prop(ale, &ptr, &prop)) {
			/* actually showing the rename textfield is done on redraw,
			 * so here we just store the index of this channel in the 
			 * dopesheet data, which will get utilized when drawing the
			 * channel...
			 *
			 * +1 factor is for backwards compat issues
			 */
			if (ac->ads) {
				ac->ads->renameIndex = channel_index + 1;
			}
		}
	}
	
	/* free temp data and tag for refresh */
	BLI_freelistN(&anim_data);
	ED_region_tag_redraw(ac->ar);
}

static int animchannels_rename_invoke (bContext *C, wmOperator *UNUSED(op), wmEvent *evt)
{
	bAnimContext ac;
	ARegion *ar;
	View2D *v2d;
	int channel_index;
	float x, y;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	ar= ac.ar;
	v2d= &ar->v2d;
	
	/* figure out which channel user clicked in 
	 * Note: although channels technically start at y= ACHANNEL_FIRST, we need to adjust by half a channel's height
	 *		so that the tops of channels get caught ok. Since ACHANNEL_FIRST is really ACHANNEL_HEIGHT, we simply use
	 *		ACHANNEL_HEIGHT_HALF.
	 */
	UI_view2d_region_to_view(v2d, evt->mval[0], evt->mval[1], &x, &y);
	
	if (ac.datatype == ANIMCONT_NLA) {
		SpaceNla *snla = (SpaceNla *)ac.sl;
		UI_view2d_listview_view_to_cell(v2d, NLACHANNEL_NAMEWIDTH, NLACHANNEL_STEP(snla), 0, (float)NLACHANNEL_HEIGHT_HALF(snla), x, y, NULL, &channel_index);
	}
	else {
		UI_view2d_listview_view_to_cell(v2d, ACHANNEL_NAMEWIDTH, ACHANNEL_STEP, 0, (float)ACHANNEL_HEIGHT_HALF, x, y, NULL, &channel_index);
	}
	
	/* handle click */
	rename_anim_channels(&ac, channel_index);
	
	return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_rename (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Rename Channels";
	ot->idname= "ANIM_OT_channels_rename";
	ot->description= "Rename animation channel under mouse";
	
	/* api callbacks */
	ot->invoke= animchannels_rename_invoke;
	ot->poll= animedit_poll_channels_active;
}

/* ******************** Mouse-Click Operator *********************** */
/* Handle selection changes due to clicking on channels. Settings will get caught by UI code... */

static int mouse_anim_channels (bAnimContext *ac, float UNUSED(x), int channel_index, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	int notifierFlags = 0;
	
	/* get the channel that was clicked on */
		/* filter channels */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
		/* get channel from index */
	ale= BLI_findlink(&anim_data, channel_index);
	if (ale == NULL) {
		/* channel not found */
		if (G.f & G_DEBUG)
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
			
			notifierFlags |= (ND_ANIMCHAN|NA_SELECTED);
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
			
			if ((adt) && (adt->flag & ADT_UI_SELECTED))
				adt->flag |= ADT_UI_ACTIVE;
			
			notifierFlags |= (ND_ANIMCHAN|NA_SELECTED);
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
		case ANIMTYPE_DSMESH:
		case ANIMTYPE_DSNTREE:
		case ANIMTYPE_DSTEX:
		case ANIMTYPE_DSLAT:
		case ANIMTYPE_DSSPK:
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
			
			notifierFlags |= (ND_ANIMCHAN|NA_SELECTED);
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
				ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				
				/* only select channels in group and group itself */
				for (fcu= agrp->channels.first; fcu && fcu->grp==agrp; fcu= fcu->next)
					fcu->flag |= FCURVE_SELECTED;
				agrp->flag |= AGRP_SELECTED;					
			}
			else {
				/* select group by itself */
				ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				agrp->flag |= AGRP_SELECTED;
			}
			
			/* if group is selected now, make group the 'active' one in the visible list */
			if (agrp->flag & AGRP_SELECTED)
				ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, agrp, ANIMTYPE_GROUP);
				
			notifierFlags |= (ND_ANIMCHAN|NA_SELECTED);
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
				ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				fcu->flag |= FCURVE_SELECTED;
			}
			
			/* if F-Curve is selected now, make F-Curve the 'active' one in the visible list */
			if (fcu->flag & FCURVE_SELECTED)
				ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, fcu, ANIMTYPE_FCURVE);
				
			notifierFlags |= (ND_ANIMCHAN|NA_SELECTED);
		}
			break;
		case ANIMTYPE_SHAPEKEY: 
		{
			KeyBlock *kb= (KeyBlock *)ale->data;
			
			/* select/deselect */
			if (selectmode == SELECT_INVERT) {
				/* inverse selection status of this ShapeKey only */
				kb->flag ^= KEYBLOCK_SEL;
			}
			else {
				/* select ShapeKey by itself */
				ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				kb->flag |= KEYBLOCK_SEL;
			}
				
			notifierFlags |= (ND_ANIMCHAN|NA_SELECTED);
		}
			break;
		case ANIMTYPE_GPDATABLOCK:
		{
			bGPdata *gpd= (bGPdata *)ale->data;
			
			/* toggle expand 
			 *	- although the triangle widget already allows this, the whole channel can also be used for this purpose
			 */
			gpd->flag ^= GP_DATA_EXPAND;
			
			notifierFlags |= (ND_ANIMCHAN|NA_EDITED);
		}
			break;
		case ANIMTYPE_GPLAYER:
		{
			bGPDlayer *gpl= (bGPDlayer *)ale->data;
			
			/* select/deselect */
			if (selectmode == SELECT_INVERT) {
				/* invert selection status of this layer only */
				gpl->flag ^= GP_LAYER_SELECT;
			}
			else {	
				/* select layer by itself */
				ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
				gpl->flag |= GP_LAYER_SELECT;
			}
			
			notifierFlags |= (ND_ANIMCHAN|NA_EDITED);
		}
			break;
		default:
			if (G.f & G_DEBUG)
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
	ar= ac.ar;
	v2d= &ar->v2d;
	
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
	UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &x, &y);
	UI_view2d_listview_view_to_cell(v2d, ACHANNEL_NAMEWIDTH, ACHANNEL_STEP, 0, (float)ACHANNEL_HEIGHT_HALF, x, y, NULL, &channel_index);
	
	/* handle mouse-click in the relevant channel then */
	notifierFlags= mouse_anim_channels(&ac, x, channel_index, selectmode);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|notifierFlags, NULL);
	
	return OPERATOR_FINISHED;
}
 
static void ANIM_OT_channels_click (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Click on Channels";
	ot->idname= "ANIM_OT_channels_click";
	ot->description= "Handle mouse-clicks over animation channels";
	
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
	WM_operatortype_append(ANIM_OT_channels_rename);
	
	WM_operatortype_append(ANIM_OT_channels_setting_enable);
	WM_operatortype_append(ANIM_OT_channels_setting_disable);
	WM_operatortype_append(ANIM_OT_channels_setting_toggle);
	
	WM_operatortype_append(ANIM_OT_channels_delete);
	
		// XXX does this need to be a separate operator?
	WM_operatortype_append(ANIM_OT_channels_editable_toggle);
	
	WM_operatortype_append(ANIM_OT_channels_move);
	
	WM_operatortype_append(ANIM_OT_channels_expand);
	WM_operatortype_append(ANIM_OT_channels_collapse);
	
	WM_operatortype_append(ANIM_OT_channels_visibility_toggle);
	WM_operatortype_append(ANIM_OT_channels_visibility_set);
	
	WM_operatortype_append(ANIM_OT_channels_fcurves_enable);
}

// TODO: check on a poll callback for this, to get hotkeys into menus
void ED_keymap_animchannels(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Animation Channels", 0, 0);
	wmKeyMapItem *kmi;

	/* selection */
		/* click-select */
		// XXX for now, only leftmouse.... 
	WM_keymap_add_item(keymap, "ANIM_OT_channels_click", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_click", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", TRUE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_click", LEFTMOUSE, KM_PRESS, KM_CTRL|KM_SHIFT, 0)->ptr, "children_only", TRUE);
	
		/* rename */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_rename", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	
		/* deselect all */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", TRUE);
	
		/* borderselect */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_border", EVT_TWEAK_L, KM_ANY, 0, 0);
	
	/* delete */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_delete", DELKEY, KM_PRESS, 0, 0);
	
	/* settings */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_toggle", WKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_enable", WKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_disable", WKEY, KM_PRESS, KM_ALT, 0);
	
	/* settings - specialized hotkeys */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_editable_toggle", TABKEY, KM_PRESS, 0, 0);
	
	/* expand/collapse */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_expand", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_collapse", PADMINUS, KM_PRESS, 0, 0);
	
	kmi = WM_keymap_add_item(keymap, "ANIM_OT_channels_expand", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "all", FALSE);
	kmi = WM_keymap_add_item(keymap, "ANIM_OT_channels_collapse", PADMINUS, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "all", FALSE);

	/* rearranging */
	RNA_enum_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_move", PAGEUPKEY, KM_PRESS, 0, 0)->ptr, "direction", REARRANGE_ANIMCHAN_UP);
	RNA_enum_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_move", PAGEDOWNKEY, KM_PRESS, 0, 0)->ptr, "direction", REARRANGE_ANIMCHAN_DOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_move", PAGEUPKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "direction", REARRANGE_ANIMCHAN_TOP);
	RNA_enum_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_move", PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "direction", REARRANGE_ANIMCHAN_BOTTOM);
	
	/* Graph Editor only */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_visibility_set", VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_visibility_toggle", VKEY, KM_PRESS, KM_SHIFT, 0);
}

/* ************************************************************************** */

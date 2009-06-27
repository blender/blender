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
#include <math.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include
#include "nla_private.h" // FIXME... maybe this shouldn't be included?

/* *********************************************** */
/* 'Special' Editing */

/* ******************** Tweak-Mode Operators ***************************** */
/* 'Tweak mode' allows the action referenced by the active NLA-strip to be edited 
 * as if it were the normal Active-Action of its AnimData block. 
 */

static int nlaedit_enable_tweakmode_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	int ok=0;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the AnimData blocks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ANIMDATA);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* if no blocks, popup error? */
	if (anim_data.first == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No AnimData blocks to enter tweakmode for");
		return OPERATOR_CANCELLED;
	}	
	
	/* for each AnimData block with NLA-data, try setting it in tweak-mode */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ale->data;
		
		/* try entering tweakmode if valid */
		ok += BKE_nla_tweakmode_enter(adt);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* if we managed to enter tweakmode on at least one AnimData block, 
	 * set the flag for this in the active scene and send notifiers
	 */
	if (ac.scene && ok) {
		/* set editing flag */
		ac.scene->flag |= SCE_NLA_EDIT_ON;
		
		/* set notifier that things have changed */
		ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
		WM_event_add_notifier(C, NC_SCENE, NULL);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No active strip(s) to enter tweakmode on.");
		return OPERATOR_CANCELLED;
	}
	
	/* done */
	return OPERATOR_FINISHED;
}
 
void NLAEDIT_OT_tweakmode_enter (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Enter Tweak Mode";
	ot->idname= "NLAEDIT_OT_tweakmode_enter";
	ot->description= "Enter tweaking mode for the action referenced by the active strip.";
	
	/* api callbacks */
	ot->exec= nlaedit_enable_tweakmode_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ------------- */

static int nlaedit_disable_tweakmode_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the AnimData blocks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ANIMDATA);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* if no blocks, popup error? */
	if (anim_data.first == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No AnimData blocks to enter tweakmode for");
		return OPERATOR_CANCELLED;
	}	
	
	/* for each AnimData block with NLA-data, try exitting tweak-mode */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ale->data;
		
		/* try entering tweakmode if valid */
		BKE_nla_tweakmode_exit(adt);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* if we managed to enter tweakmode on at least one AnimData block, 
	 * set the flag for this in the active scene and send notifiers
	 */
	if (ac.scene) {
		/* clear editing flag */
		ac.scene->flag &= ~SCE_NLA_EDIT_ON;
		
		/* set notifier that things have changed */
		ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
		WM_event_add_notifier(C, NC_SCENE, NULL);
	}
	
	/* done */
	return OPERATOR_FINISHED;
}
 
void NLAEDIT_OT_tweakmode_exit (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Exit Tweak Mode";
	ot->idname= "NLAEDIT_OT_tweakmode_exit";
	ot->description= "Exit tweaking mode for the action referenced by the active strip.";
	
	/* api callbacks */
	ot->exec= nlaedit_disable_tweakmode_exec;
	ot->poll= nlaop_poll_tweakmode_on;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *********************************************** */
/* NLA Editing Operations (Constructive/Destructive) */

/* ******************** Add Action-Clip Operator ***************************** */
/* Add a new Action-Clip strip to the active track (or the active block if no space in the track) */

/* pop up menu allowing user to choose the action to use */
static int nlaedit_add_actionclip_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Main *m= CTX_data_main(C);
	bAction *act;
	uiPopupMenu *pup;
	uiLayout *layout;
	
	pup= uiPupMenuBegin(C, "Add Action Clip", 0);
	layout= uiPupMenuLayout(pup);
	
	/* loop through Actions in Main database, adding as items in the menu */
	for (act= m->action.first; act; act= act->id.next)
		uiItemStringO(layout, act->id.name+2, 0, "NLAEDIT_OT_add_actionclip", "action", act->id.name);
	uiItemS(layout);
	
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

/* add the specified action as new strip */
static int nlaedit_add_actionclip_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	Scene *scene;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter, items;
	
	bAction *act = NULL;
	char actname[22];
	float cfra;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	scene= ac.scene;
	cfra= (float)CFRA;
		
	/* get action to use */
	RNA_string_get(op->ptr, "action", actname);
	act= (bAction *)find_id("AC", actname+2);
	
	if (act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No valid Action to add.");
		//printf("Add strip - actname = '%s' \n", actname);
		return OPERATOR_CANCELLED;
	}
	
	/* get a list of the editable tracks being shown in the NLA
	 *	- this is limited to active ones for now, but could be expanded to 
	 */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ACTIVE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	items= ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	if (items == 0) {
		BKE_report(op->reports, RPT_ERROR, "No active track(s) to add strip to.");
		return OPERATOR_CANCELLED;
	}
	
	/* for every active track, try to add strip to free space in track or to the top of the stack if no space */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		AnimData *adt= BKE_animdata_from_id(ale->id);
		NlaStrip *strip= NULL;
		
		/* create a new strip, and offset it to start on the current frame */
		strip= add_nlastrip(act);
		
		strip->end 		+= (cfra - strip->start);
		strip->start	 = cfra;
		
		/* firstly try adding strip to our current track, but if that fails, add to a new track */
		if (BKE_nlatrack_add_strip(nlt, strip) == 0) {
			/* trying to add to the current failed (no space), 
			 * so add a new track to the stack, and add to that...
			 */
			nlt= add_nlatrack(adt, NULL);
			BKE_nlatrack_add_strip(nlt, strip);
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_add_actionclip (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Action Strip";
	ot->idname= "NLAEDIT_OT_add_actionclip";
	ot->description= "Add an Action-Clip strip (i.e. an NLA Strip referencing an Action) to the active track.";
	
	/* api callbacks */
	ot->invoke= nlaedit_add_actionclip_invoke;
	ot->exec= nlaedit_add_actionclip_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
		// TODO: this would be nicer as an ID-pointer...
	RNA_def_string(ot->srna, "action", "", 21, "Action", "Name of Action to add as a new Action-Clip Strip.");
}

/* ******************** Add Transition Operator ***************************** */
/* Add a new transition strip between selected strips */

/* add the specified action as new strip */
static int nlaedit_add_transition_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	int done = 0;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each track, find pairs of strips to add transitions to */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *s1, *s2;
		
		/* get initial pair of strips */
		if ELEM(nlt->strips.first, NULL, nlt->strips.last)
			continue;
		s1= nlt->strips.first;
		s2= s1->next;
		
		/* loop over strips */
		for (; s1 && s2; s1=s2, s2=s2->next) {
			NlaStrip *strip;
			
			/* check if both are selected */
			if ELEM(0, (s1->flag & NLASTRIP_FLAG_SELECT), (s2->flag & NLASTRIP_FLAG_SELECT))
				continue;
			/* check if there's space between the two */
			if (IS_EQ(s1->end, s2->start))
				continue;
				
			/* allocate new strip */
			strip= MEM_callocN(sizeof(NlaStrip), "NlaStrip");
			BLI_insertlinkafter(&nlt->strips, s1, strip);
			
			/* set the type */
			strip->type= NLASTRIP_TYPE_TRANSITION;
			
			/* generic settings 
			 *	- selected flag to highlight this to the user
			 *	- auto-blends to ensure that blend in/out values are automatically 
			 *	  determined by overlaps of strips
			 */
			strip->flag = NLASTRIP_FLAG_SELECT|NLASTRIP_FLAG_AUTO_BLENDS;
			
			/* range is simply defined as the endpoints of the adjacent strips */
			strip->start 	= s1->end;
			strip->end 		= s2->start;
			
			/* scale and repeat aren't of any use, but shouldn't ever be 0 */
			strip->scale= 1.0f;
			strip->repeat = 1.0f;
			
			/* make note of this */
			done++;
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* was anything added? */
	if (done) {
		/* set notifier that things have changed */
		ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
		WM_event_add_notifier(C, NC_SCENE, NULL);
		
		/* done */
		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "Needs at least a pair of adjacent selected strips.");
		return OPERATOR_CANCELLED;
	}
}

void NLAEDIT_OT_add_transition (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Transition";
	ot->idname= "NLAEDIT_OT_add_transition";
	ot->description= "Add a transition strip between two adjacent selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_add_transition_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Duplicate Strips Operator ************************** */
/* Duplicates the selected NLA-Strips, putting them on new tracks above the one
 * the originals were housed in.
 */
 
static int nlaedit_duplicate_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	short done = 0;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* duplicate strips in tracks starting from the last one so that we're 
	 * less likely to duplicate strips we just duplicated...
	 */
	for (ale= anim_data.last; ale; ale= ale->prev) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		AnimData *adt= BKE_animdata_from_id(ale->id);
		NlaStrip *strip, *nstrip, *next;
		NlaTrack *track;
		
		for (strip= nlt->strips.first; strip; strip= next) {
			next= strip->next;
			
			/* if selected, split the strip at its midpoint */
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				/* make a copy (assume that this is possible) */
				nstrip= copy_nlastrip(strip);
				
				/* in case there's no space in the track above, or we haven't got a reference to it yet, try adding */
				if (BKE_nlatrack_add_strip(nlt->next, nstrip) == 0) {
					/* need to add a new track above the one above the current one
					 *	- if the current one is the last one, nlt->next will be NULL, which defaults to adding 
					 *	  at the top of the stack anyway...
					 */
					track= add_nlatrack(adt, nlt->next);
					BKE_nlatrack_add_strip(track, nstrip);
				}
				
				/* deselect the original and the active flag */
				strip->flag &= ~(NLASTRIP_FLAG_SELECT|NLASTRIP_FLAG_ACTIVE);
				
				done++;
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	if (done) {
		/* set notifier that things have changed */
		ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
		WM_event_add_notifier(C, NC_SCENE, NULL);
		
		/* done */
		return OPERATOR_FINISHED;
	}
	else
		return OPERATOR_CANCELLED;
}

static int nlaedit_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	nlaedit_duplicate_exec(C, op);
	
	RNA_int_set(op->ptr, "mode", TFM_TIME_TRANSLATE); // XXX
	WM_operator_name_call(C, "TFM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_duplicate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Strips";
	ot->idname= "NLAEDIT_OT_duplicate";
	ot->description= "Duplicate selected NLA-Strips, adding the new strips in new tracks above the originals.";
	
	/* api callbacks */
	ot->invoke= nlaedit_duplicate_invoke;
	ot->exec= nlaedit_duplicate_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/* ******************** Delete Strips Operator ***************************** */
/* Deletes the selected NLA-Strips */

static int nlaedit_delete_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each NLA-Track, delete all selected strips */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip, *nstrip;
		
		for (strip= nlt->strips.first; strip; strip= nstrip) {
			nstrip= strip->next;
			
			/* if selected, delete */
			if (strip->flag & NLASTRIP_FLAG_SELECT)
				free_nlastrip(&nlt->strips, strip);
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Strips";
	ot->idname= "NLAEDIT_OT_delete";
	ot->description= "Delete selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_delete_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Split Strips Operator ***************************** */
/* Splits the selected NLA-Strips into two strips at the midpoint of the strip */
// TODO's? 
// 	- multiple splits
//	- variable-length splits?

static int nlaedit_split_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each NLA-Track, split all selected strips into two strips */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip, *nstrip, *next;
		
		for (strip= nlt->strips.first; strip; strip= next) {
			next= strip->next;
			
			/* if selected, split the strip at its midpoint */
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				float midframe, midaframe, len;
				
				/* calculate the frames to do the splitting at */
					/* strip extents */
				len= strip->end - strip->start;
				if (IS_EQ(len, 0.0f)) 
					continue;
				else
					midframe= strip->start + (len / 2.0f);
					
					/* action range */
				len= strip->actend - strip->actstart;
				if (IS_EQ(len, 0.0f))
					midaframe= strip->actend;
				else
					midaframe= strip->actstart + (len / 2.0f);
				
				/* make a copy (assume that this is possible) and append
				 * it immediately after the current strip
				 */
				nstrip= copy_nlastrip(strip);
				BLI_insertlinkafter(&nlt->strips, strip, nstrip);
				
				/* set the endpoint of the first strip and the start of the new strip 
				 * to the midframe values calculated above
				 */
				strip->end= midframe;
				nstrip->start= midframe;
				
				strip->actend= midaframe;
				nstrip->actstart= midaframe;
				
				/* clear the active flag from the copy */
				nstrip->flag &= ~NLASTRIP_FLAG_ACTIVE;
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_split (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Split Strips";
	ot->idname= "NLAEDIT_OT_split";
	ot->description= "Split selected strips at their midpoints.";
	
	/* api callbacks */
	ot->exec= nlaedit_split_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *********************************************** */
/* NLA Editing Operations (Modifying) */

/* ******************** Move Strips Up Operator ************************** */
/* Tries to move the selected strips into the track above if possible. */

static int nlaedit_move_up_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* init the editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* since we're potentially moving strips from lower tracks to higher tracks, we should
	 * loop over the tracks in reverse order to avoid moving earlier strips up multiple tracks
	 */
	for (ale= anim_data.last; ale; ale= ale->prev) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaTrack *nltn= nlt->next;
		NlaStrip *strip, *stripn;
		
		/* if this track has no tracks after it, skip for now... */
		if (nltn == NULL)
			continue;
		
		/* for every selected strip, try to move */
		for (strip= nlt->strips.first; strip; strip= stripn) {
			stripn= strip->next;
			
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				/* check if the track above has room for this strip */
				if (BKE_nlatrack_has_space(nltn, strip->start, strip->end)) {
					/* remove from its current track, and add to the one above (it 'should' work, so no need to worry) */
					BLI_remlink(&nlt->strips, strip);
					BKE_nlatrack_add_strip(nltn, strip);
				}
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_move_up (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Strips Up";
	ot->idname= "NLAEDIT_OT_move_up";
	ot->description= "Move selected strips up a track if there's room.";
	
	/* api callbacks */
	ot->exec= nlaedit_move_up_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Move Strips Down Operator ************************** */
/* Tries to move the selected strips into the track above if possible. */

static int nlaedit_move_down_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* init the editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* loop through the tracks in normal order, since we're pushing strips down,
	 * strips won't get operated on twice
	 */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaTrack *nltp= nlt->prev;
		NlaStrip *strip, *stripn;
		
		/* if this track has no tracks before it, skip for now... */
		if (nltp == NULL)
			continue;
		
		/* for every selected strip, try to move */
		for (strip= nlt->strips.first; strip; strip= stripn) {
			stripn= strip->next;
			
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				/* check if the track below has room for this strip */
				if (BKE_nlatrack_has_space(nltp, strip->start, strip->end)) {
					/* remove from its current track, and add to the one above (it 'should' work, so no need to worry) */
					BLI_remlink(&nlt->strips, strip);
					BKE_nlatrack_add_strip(nltp, strip);
				}
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_move_down (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Strips Down";
	ot->idname= "NLAEDIT_OT_move_down";
	ot->description= "Move selected strips down a track if there's room.";
	
	/* api callbacks */
	ot->exec= nlaedit_move_down_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Apply Scale Operator ***************************** */
/* Reset the scaling of the selected strips to 1.0f */

/* apply scaling to keyframe */
static short bezt_apply_nlamapping (BeztEditData *bed, BezTriple *bezt)
{
	/* NLA-strip which has this scaling is stored in bed->data */
	NlaStrip *strip= (NlaStrip *)bed->data;
	
	/* adjust all the times */
	bezt->vec[0][0]= nlastrip_get_frame(strip, bezt->vec[0][0], 1);
	bezt->vec[1][0]= nlastrip_get_frame(strip, bezt->vec[1][0], 1);
	bezt->vec[2][0]= nlastrip_get_frame(strip, bezt->vec[2][0], 1);
	
	/* nothing to return or else we exit */
	return 0;
}

static int nlaedit_apply_scale_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* init the editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* for each NLA-Track, apply scale of all selected strips */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			/* strip must be selected, and must be action-clip only (transitions don't have scale) */
			if ((strip->flag & NLASTRIP_FLAG_SELECT) && (strip->type == NLASTRIP_TYPE_CLIP)) {
				/* if the referenced action is used by other strips, make this strip use its own copy */
				if (strip->act == NULL) 
					continue;
				if (strip->act->id.us > 1) {
					/* make a copy of the Action to work on */
					bAction *act= copy_action(strip->act);
					
					/* set this as the new referenced action, decrementing the users of the old one */
					strip->act->id.us--;
					strip->act= act;
				}
				
				/* setup iterator, and iterate over all the keyframes in the action, applying this scaling */
				bed.data= strip;
				ANIM_animchanneldata_keys_bezier_loop(&bed, strip->act, ALE_ACT, NULL, bezt_apply_nlamapping, calchandles_fcurve, 0);
				
				/* clear scale of strip now that it has been applied,
				 * and recalculate the extents of the action now that it has been scaled
				 * but leave everything else alone 
				 */
				strip->scale= 1.0f;
				calc_action_range(strip->act, &strip->actstart, &strip->actend, 1);
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_apply_scale (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Scale";
	ot->idname= "NLAEDIT_OT_apply_scale";
	ot->description= "Apply scaling of selected strips to their referenced Actions.";
	
	/* api callbacks */
	ot->exec= nlaedit_apply_scale_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Clear Scale Operator ***************************** */
/* Reset the scaling of the selected strips to 1.0f */

static int nlaedit_clear_scale_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each NLA-Track, reset scale of all selected strips */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			/* strip must be selected, and must be action-clip only (transitions don't have scale) */
			if ((strip->flag & NLASTRIP_FLAG_SELECT) && (strip->type == NLASTRIP_TYPE_CLIP)) {
				PointerRNA strip_ptr;
				
				RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);
				RNA_float_set(&strip_ptr, "scale", 1.0f);
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	ANIM_animdata_send_notifiers(C, &ac, ANIM_CHANGED_BOTH);
	WM_event_add_notifier(C, NC_SCENE, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_clear_scale (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Scale";
	ot->idname= "NLAEDIT_OT_clear_scale";
	ot->description= "Reset scaling of selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_clear_scale_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *********************************************** */

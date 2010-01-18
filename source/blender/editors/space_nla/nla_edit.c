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
#include "BLI_math.h"
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
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include
#include "nla_private.h" // FIXME... maybe this shouldn't be included?

/* *********************************************** */
/* Utilities exported to other places... */

/* Perform validation for blending/extend settings */
void ED_nla_postop_refresh (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	short filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ANIMDATA | ANIMFILTER_FOREDIT);
	
	/* get blocks to work on */
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* performing auto-blending, extend-mode validation, etc. */
		BKE_nla_validate_state(ale->data);
	}
	
	/* free temp memory */
	BLI_freelistN(&anim_data);
}

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
		WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_ACTCHANGE, NULL);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No active strip(s) to enter tweakmode on.");
		return OPERATOR_CANCELLED;
	}
	
	/* done */
	return OPERATOR_FINISHED;
}
 
void NLA_OT_tweakmode_enter (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Enter Tweak Mode";
	ot->idname= "NLA_OT_tweakmode_enter";
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
		WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_ACTCHANGE, NULL);
	}
	
	/* done */
	return OPERATOR_FINISHED;
}
 
void NLA_OT_tweakmode_exit (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Exit Tweak Mode";
	ot->idname= "NLA_OT_tweakmode_exit";
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
		uiItemStringO(layout, act->id.name+2, 0, "NLA_OT_actionclip_add", "action", act->id.name);
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
		AnimData *adt= ale->adt;
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
		
		/* auto-name it */
		BKE_nlastrip_validate_name(adt, strip);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_actionclip_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Action Strip";
	ot->idname= "NLA_OT_actionclip_add";
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
		AnimData *adt= ale->adt;
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
			/* make neither one is a transition 
			 *	- although this is impossible to create with the standard tools, 
			 * 	  the user may have altered the settings
			 */
			if (ELEM(NLASTRIP_TYPE_TRANSITION, s1->type, s2->type))
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
			
			/* auto-name it */
			BKE_nlastrip_validate_name(adt, strip);
			
			/* make note of this */
			done++;
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* was anything added? */
	if (done) {
		/* refresh auto strip properties */
		ED_nla_postop_refresh(&ac);
		
		/* set notifier that things have changed */
		WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
		
		/* done */
		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "Needs at least a pair of adjacent selected strips with a gap between them.");
		return OPERATOR_CANCELLED;
	}
}

void NLA_OT_transition_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Transition";
	ot->idname= "NLA_OT_transition_add";
	ot->description= "Add a transition strip between two adjacent selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_add_transition_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Add Meta-Strip Operator ***************************** */
/* Add new meta-strips incorporating the selected strips */

/* add the specified action as new strip */
static int nlaedit_add_meta_exec (bContext *C, wmOperator *op)
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
	
	/* for each track, find pairs of strips to add transitions to */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		AnimData *adt= ale->adt;
		NlaStrip *strip;
		
		/* create meta-strips from the continuous chains of selected strips */
		BKE_nlastrips_make_metas(&nlt->strips, 0);
		
		/* name the metas */
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			/* auto-name this strip if selected (that means it is a meta) */
			if (strip->flag & NLASTRIP_FLAG_SELECT)
				BKE_nlastrip_validate_name(adt, strip);
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_meta_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Meta-Strips";
	ot->idname= "NLA_OT_meta_add";
	ot->description= "Add new meta-strips incorporating the selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_add_meta_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Remove Meta-Strip Operator ***************************** */
/* Separate out the strips held by the selected meta-strips */

static int nlaedit_remove_meta_exec (bContext *C, wmOperator *op)
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
	
	/* for each track, find pairs of strips to add transitions to */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		
		/* clear all selected meta-strips, regardless of whether they are temporary or not */
		BKE_nlastrips_clear_metas(&nlt->strips, 1, 0);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_meta_remove (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Meta-Strips";
	ot->idname= "NLA_OT_meta_remove";
	ot->description= "Separate out the strips held by the selected meta-strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_remove_meta_exec;
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
		AnimData *adt= ale->adt;
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
				
				/* auto-name it */
				BKE_nlastrip_validate_name(adt, strip);
				
				done++;
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	if (done) {
		/* refresh auto strip properties */
		ED_nla_postop_refresh(&ac);
		
		/* set notifier that things have changed */
		WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
		
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
	WM_operator_name_call(C, "TRANSFORM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void NLA_OT_duplicate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Strips";
	ot->idname= "NLA_OT_duplicate";
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
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				/* if a strip either side of this was a transition, delete those too */
				if ((strip->prev) && (strip->prev->type == NLASTRIP_TYPE_TRANSITION)) 
					free_nlastrip(&nlt->strips, strip->prev);
				if ((nstrip) && (nstrip->type == NLASTRIP_TYPE_TRANSITION)) {
					nstrip= nstrip->next;
					free_nlastrip(&nlt->strips, strip->next);
				}
				
				/* finally, delete this strip */
				free_nlastrip(&nlt->strips, strip);
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Strips";
	ot->idname= "NLA_OT_delete";
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

/* split a given Action-Clip strip */
static void nlaedit_split_strip_actclip (AnimData *adt, NlaTrack *nlt, NlaStrip *strip, float cfra)
{
	NlaStrip *nstrip;
	float splitframe, splitaframe;
	
	/* calculate the frames to do the splitting at 
	 *	- use current frame if within extents of strip 
	 */
	if ((cfra > strip->start) && (cfra < strip->end)) {
		/* use the current frame */
		splitframe= cfra;
		splitaframe= nlastrip_get_frame(strip, cfra, NLATIME_CONVERT_UNMAP);
	}
	else {
		/* split in the middle */
		float len;
			
			/* strip extents */
		len= strip->end - strip->start;
		if (IS_EQ(len, 0.0f)) 
			return;
		else
			splitframe= strip->start + (len / 2.0f);
			
			/* action range */
		len= strip->actend - strip->actstart;
		if (IS_EQ(len, 0.0f))
			splitaframe= strip->actend;
		else
			splitaframe= strip->actstart + (len / 2.0f);
	}
	
	/* make a copy (assume that this is possible) and append
	 * it immediately after the current strip
	 */
	nstrip= copy_nlastrip(strip);
	BLI_insertlinkafter(&nlt->strips, strip, nstrip);
	
	/* set the endpoint of the first strip and the start of the new strip 
	 * to the splitframe values calculated above
	 */
	strip->end= splitframe;
	nstrip->start= splitframe;
	
	if ((splitaframe > strip->actstart) && (splitaframe < strip->actend)) {
		/* only do this if we're splitting down the middle...  */
		strip->actend= splitaframe;
		nstrip->actstart= splitaframe;
	}
	
	/* clear the active flag from the copy */
	nstrip->flag &= ~NLASTRIP_FLAG_ACTIVE;
	
	/* auto-name the new strip */
	BKE_nlastrip_validate_name(adt, nstrip);
}

/* split a given Meta strip */
static void nlaedit_split_strip_meta (AnimData *adt, NlaTrack *nlt, NlaStrip *strip)
{
	/* simply ungroup it for now...  */
	BKE_nlastrips_clear_metastrip(&nlt->strips, strip);
}

/* ----- */

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
		AnimData *adt= ale->adt;
		NlaStrip *strip, *next;
		
		for (strip= nlt->strips.first; strip; strip= next) {
			next= strip->next;
			
			/* if selected, split the strip at its midpoint */
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				/* splitting method depends on the type of strip */
				switch (strip->type) {
					case NLASTRIP_TYPE_CLIP: /* action-clip */
						nlaedit_split_strip_actclip(adt, nlt, strip, (float)ac.scene->r.cfra);
						break;
						
					case NLASTRIP_TYPE_META: /* meta-strips need special handling */
						nlaedit_split_strip_meta(adt, nlt, strip);
						break;
					
					default: /* for things like Transitions, do not split! */
						break;
				}
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_split (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Split Strips";
	ot->idname= "NLA_OT_split";
	ot->description= "Split selected strips at their midpoints.";
	
	/* api callbacks */
	ot->exec= nlaedit_split_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Bake Strips Operator ***************************** */
/* Bakes the NLA Strips for the active AnimData blocks */

static int nlaedit_bake_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ANIMDATA | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each AnimData block, bake strips to animdata... */
	for (ale= anim_data.first; ale; ale= ale->next) {
		// FIXME
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_bake (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Bake Strips";
	ot->idname= "NLA_OT_bake";
	ot->description= "Bake all strips of selected AnimData blocks.";
	
	/* api callbacks */
	ot->exec= nlaedit_bake_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *********************************************** */
/* NLA Editing Operations (Modifying) */

/* ******************** Toggle Muting Operator ************************** */
/* Toggles whether strips are muted or not */

static int nlaedit_toggle_mute_exec (bContext *C, wmOperator *op)
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
	
	/* go over all selected strips */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		
		/* for every selected strip, toggle muting  */
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			if (strip->flag & NLASTRIP_FLAG_SELECT) {
				/* just flip the mute flag for now */
				// TODO: have a pre-pass to check if mute all or unmute all?
				strip->flag ^= NLASTRIP_FLAG_MUTED;
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_mute_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Muting";
	ot->idname= "NLA_OT_mute_toggle";
	ot->description= "Mute or un-muted selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_toggle_mute_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Move Strips Up Operator ************************** */
/* Tries to move the selected strips into the track above if possible. */

static int nlaedit_move_up_exec (bContext *C, wmOperator *op)
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
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_move_up (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Strips Up";
	ot->idname= "NLA_OT_move_up";
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
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
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
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_move_down (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Strips Down";
	ot->idname= "NLA_OT_move_down";
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
	bezt->vec[0][0]= nlastrip_get_frame(strip, bezt->vec[0][0], NLATIME_CONVERT_MAP);
	bezt->vec[1][0]= nlastrip_get_frame(strip, bezt->vec[1][0], NLATIME_CONVERT_MAP);
	bezt->vec[2][0]= nlastrip_get_frame(strip, bezt->vec[2][0], NLATIME_CONVERT_MAP);
	
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
				calc_action_range(strip->act, &strip->actstart, &strip->actend, 0);
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_apply_scale (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Scale";
	ot->idname= "NLA_OT_apply_scale";
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
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_clear_scale (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Scale";
	ot->idname= "NLA_OT_clear_scale";
	ot->description= "Reset scaling of selected strips.";
	
	/* api callbacks */
	ot->exec= nlaedit_clear_scale_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Snap Strips Operator ************************** */
/* Moves the start-point of the selected strips to the specified places */

/* defines for snap keyframes tool */
EnumPropertyItem prop_nlaedit_snap_types[] = {
	{NLAEDIT_SNAP_CFRA, "CFRA", 0, "Current frame", ""},
	{NLAEDIT_SNAP_NEAREST_FRAME, "NEAREST_FRAME", 0, "Nearest Frame", ""}, // XXX as single entry?
	{NLAEDIT_SNAP_NEAREST_SECOND, "NEAREST_SECOND", 0, "Nearest Second", ""}, // XXX as single entry?
	{NLAEDIT_SNAP_NEAREST_MARKER, "NEAREST_MARKER", 0, "Nearest Marker", ""},
	{0, NULL, 0, NULL, NULL}
};

static int nlaedit_snap_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene;
	int mode = RNA_enum_get(op->ptr, "type");
	float secf;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* get some necessary vars */
	scene= ac.scene;
	secf= (float)FPS;
	
	/* since we may add tracks, perform this in reverse order */
	for (ale= anim_data.last; ale; ale= ale->prev) {
		ListBase tmp_strips = {NULL, NULL};
		AnimData *adt= ale->adt;
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip, *stripn;
		NlaTrack *track;
		
		/* create meta-strips from the continuous chains of selected strips */
		BKE_nlastrips_make_metas(&nlt->strips, 1);
		
		/* apply the snapping to all the temp meta-strips, then put them in a separate list to be added
		 * back to the original only if they still fit
		 */
		for (strip= nlt->strips.first; strip; strip= stripn) {
			stripn= strip->next;
			
			if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
				float start, end;
				
				/* get the existing end-points */
				start= strip->start;
				end= strip->end;
				
				/* calculate new start position based on snapping mode */
				switch (mode) {
					case NLAEDIT_SNAP_CFRA: /* to current frame */
						strip->start= (float)CFRA;
						break;
					case NLAEDIT_SNAP_NEAREST_FRAME: /* to nearest frame */
						strip->start= (float)(floor(start+0.5));
						break;
					case NLAEDIT_SNAP_NEAREST_SECOND: /* to nearest second */
						strip->start= ((float)floor(start/secf + 0.5f) * secf);
						break;
					case NLAEDIT_SNAP_NEAREST_MARKER: /* to nearest marker */
						strip->start= (float)ED_markers_find_nearest_marker_time(ac.markers, start);
						break;
					default: /* just in case... no snapping */
						strip->start= start;
						break;
				}
				
				/* get new endpoint based on start-point (and old length) */
				strip->end= strip->start + (end - start);
				
				/* apply transforms to meta-strip to its children */
				BKE_nlameta_flush_transforms(strip);
				
				/* remove strip from track, and add to the temp buffer */
				BLI_remlink(&nlt->strips, strip);
				BLI_addtail(&tmp_strips, strip);
			}
		}
		
		/* try adding each meta-strip back to the track one at a time, to make sure they'll fit */
		for (strip= tmp_strips.first; strip; strip= stripn) {
			stripn= strip->next;
			
			/* remove from temp-strips list */
			BLI_remlink(&tmp_strips, strip);
			
			/* in case there's no space in the current track, try adding */
			if (BKE_nlatrack_add_strip(nlt, strip) == 0) {
				/* need to add a new track above the current one */
				track= add_nlatrack(adt, nlt);
				BKE_nlatrack_add_strip(track, strip);
				
				/* clear temp meta-strips on this new track, as we may not be able to get back to it */
				BKE_nlastrips_clear_metas(&track->strips, 0, 1);
			}
		}
		
		/* remove the meta-strips now that we're done */
		BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* refresh auto strip properties */
	ED_nla_postop_refresh(&ac);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

void NLA_OT_snap (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Snap Strips";
	ot->idname= "NLA_OT_snap";
	ot->description= "Move start of strips to specified time.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= nlaedit_snap_exec;
	ot->poll= nlaop_poll_tweakmode_off;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", prop_nlaedit_snap_types, 0, "Type", "");
}

/* *********************************************** */
/* NLA Modifiers */

/* ******************** Add F-Modifier Operator *********************** */

/* present a special customised popup menu for this, with some filtering */
static int nla_fmodifier_add_invoke (bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	int i;
	
	pup= uiPupMenuBegin(C, "Add F-Modifier", 0);
	layout= uiPupMenuLayout(pup);
	
	/* start from 1 to skip the 'Invalid' modifier type */
	for (i = 1; i < FMODIFIER_NUM_TYPES; i++) {
		FModifierTypeInfo *fmi= get_fmodifier_typeinfo(i);
		
		/* check if modifier is valid for this context */
		if (fmi == NULL)
			continue;
		if (i == FMODIFIER_TYPE_CYCLES) /* we already have repeat... */
			continue;
		
		/* add entry to add this type of modifier */
		uiItemEnumO(layout, fmi->name, 0, "NLA_OT_fmodifier_add", "type", i);
	}
	uiItemS(layout);
	
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static int nla_fmodifier_add_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	FModifier *fcm;
	int type= RNA_enum_get(op->ptr, "type");
	short onlyActive = RNA_boolean_get(op->ptr, "only_active");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get a list of the editable tracks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each NLA-Track, add the specified modifier to all selected strips */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		int i = 1;
		
		for (strip= nlt->strips.first; strip; strip=strip->next, i++) {
			/* only add F-Modifier if on active strip? */
			if ((onlyActive) && (strip->flag & NLASTRIP_FLAG_ACTIVE)==0)
				continue;
			
			/* add F-Modifier of specified type to selected, and make it the active one */
			fcm= add_fmodifier(&strip->modifiers, type);
			
			if (fcm)
				set_active_fmodifier(&strip->modifiers, fcm);
			else {
				char errormsg[128];
				sprintf(errormsg, "Modifier couldn't be added to (%s : %d). See console for details.", nlt->name, i);
				
				BKE_report(op->reports, RPT_ERROR, errormsg);
			}
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	/* set notifier that things have changed */
	// FIXME: this doesn't really do it justice...
	WM_event_add_notifier(C, NC_ANIMATION, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}
 
void NLA_OT_fmodifier_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add F-Modifier";
	ot->idname= "NLA_OT_fmodifier_add";
	
	/* api callbacks */
	ot->invoke= nla_fmodifier_add_invoke;
	ot->exec= nla_fmodifier_add_exec;
	ot->poll= nlaop_poll_tweakmode_off; 
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", fmodifier_type_items, 0, "Type", "");
	RNA_def_boolean(ot->srna, "only_active", 0, "Only Active", "Only add F-Modifier of the specified type to the active strip.");
}

/* *********************************************** */

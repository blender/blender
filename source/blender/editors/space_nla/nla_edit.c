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
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include

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
/* NLA Editing Operations */

/* ******************** Delete Operator ***************************** */
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
		
	/* get a list of the AnimData blocks being shown in the NLA */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* for each NLA-Track, delete all selected strips */
	// FIXME: need to double-check that we've got tracks
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

/* *********************************************** */
